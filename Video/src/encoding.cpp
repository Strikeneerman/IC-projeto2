
#include "VideoCodec.h"

void encodeFrameIntra(Mat& frame, BitStream& stream, int shiftBits) {
    vector<int> residuals;
    residuals.reserve(frame.rows * frame.cols);

    // First pass: collect residuals and calculate optimal m
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            int predicted = predictPixel(frame, x, y);
            int residual = frame.at<uchar>(y, x) - predicted;
            residual >>= shiftBits;
            residuals.push_back(residual);
            frame.at<uchar>(y, x) = predicted + (residual << shiftBits);
        }
    }

    // Calculate average absolute residual
    double sum = std::accumulate(residuals.begin(), residuals.end(), 0, [](int acc, int val) { return acc + std::abs(val); });
    double average = sum / residuals.size();

    // Calculate optimal m
    int m = static_cast<int>(std::ceil(-1 / std::log2(1 - (1 / (average + 1)))));
    m = max(2, min(m, 64));

    // Write m value
    stream.writeBits(m, 8);

    Golomb golomb(m, false);

    // Second pass: encode residuals

    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            golomb.encode(stream, residuals[y * frame.cols + x]);
        }
    }
}

void encodeFrameInter(Mat& currentFrame, const Mat& referenceFrame,
                     BitStream& stream,
                     unsigned long long& counter1, unsigned long long& counter2,
                     int shiftBits,
                     const BlockMatchingParams& params = BlockMatchingParams()) {
    const int rows = currentFrame.rows;
    const int cols = currentFrame.cols;

    // Process each block
    for (int y = 0; y < rows; y += params.blockSize) {
        for (int x = 0; x < cols; x += params.blockSize) {
            // Actual block size might be smaller at borders
            int currentBlockHeight = min(params.blockSize, rows - y);
            int currentBlockWidth = min(params.blockSize, cols - x);

            // Get the current block
            Mat currentBlock = currentFrame(
                Range(y, y + currentBlockHeight),
                Range(x, x + currentBlockWidth)
            );

            // Find best motion vector for this block
            MotionVector mv = findBestMotionVector(currentBlock, referenceFrame,
                                                 x, y, params);

            // Get predicted block from reference frame using motion vector
            Mat predictedBlockInter = getPredictedBlock(referenceFrame, x, y,
                                                 currentBlockWidth, currentBlockHeight,
                                                 mv);

            // Calculate residuals for intra and inter prediction
            vector<int> residualsInter;
            vector<int> residualsIntra;
            residualsInter.reserve(currentBlockHeight * currentBlockWidth);
            residualsIntra.reserve(currentBlockHeight * currentBlockWidth);

            Mat currentBlockInter(currentBlockHeight, currentBlockWidth, currentBlock.type());
            Mat currentBlockIntra = currentFrame(
                Range(y, y + currentBlockHeight),
                Range(x, x + currentBlockWidth)
            );

            for (int by = 0; by < currentBlockHeight; ++by) {
                for (int bx = 0; bx < currentBlockWidth; ++bx) {
                    int residualInter = currentBlock.at<uchar>(by, bx) - predictedBlockInter.at<uchar>(by, bx);
                    int residualIntra = currentBlock.at<uchar>(by, bx) - predictPixel(currentBlockIntra, bx, by);

                    residualInter >>= shiftBits;
                    residualIntra >>= shiftBits;

                    residualsInter.push_back(residualInter);
                    residualsIntra.push_back(residualIntra);

                    currentBlockInter.at<uchar>(by, bx) = predictedBlockInter.at<uchar>(by, bx) + (residualInter << shiftBits);
                    currentBlockIntra.at<uchar>(by, bx) = predictPixel(currentBlockIntra, bx, by) + (residualIntra << shiftBits);
                }
            }

            // Get average residuals for both options
            double sumInter = std::accumulate(residualsInter.begin(), residualsInter.end(), 0, [](int acc, int val) { return acc + std::abs(val); });
            double sumIntra = std::accumulate(residualsIntra.begin(), residualsIntra.end(), 0, [](int acc, int val) { return acc + std::abs(val); });
            double averageInter = sumInter / residualsInter.size();
            double averageIntra = sumIntra / residualsIntra.size();

            // Choose the method that resulted in better prediction
            vector<int> blockResiduals;
            int blockM;
            bool useInter;
            if(averageInter < averageIntra){
                counter1++;
                useInter = true;
                stream.writeBit(1);

                blockResiduals = residualsInter;
                blockM = static_cast<int>(std::ceil(-1 / std::log2(1 - (1 / (averageInter + 1)))));
                currentFrame(Range(y, y + currentBlockInter.rows), Range(x, x + currentBlockInter.cols)) = currentBlockInter;
            } else {
                counter2++;
                useInter = false;
                stream.writeBit(0);

                blockResiduals = residualsIntra;
                blockM = static_cast<int>(std::ceil(-1 / std::log2(1 - (1 / (averageIntra + 1)))));
                currentFrame(Range(y, y + currentBlockIntra.rows), Range(x, x + currentBlockIntra.cols)) = currentBlockIntra;
            }
            blockM = max(2, min(blockM, 64));

            // Write block header (m value and motion vectors)
            stream.writeBits(blockM, 8);
            Golomb golomb(blockM, false);
            if(useInter){
                golomb.encode(stream, mv.dx);
                golomb.encode(stream, mv.dy);
            }

            // Encode residuals for the block
            for (int by = 0; by < currentBlockHeight; ++by) {
                for (int bx = 0; bx < currentBlockWidth; ++bx) {
                    golomb.encode(stream, blockResiduals[by * currentBlockWidth + bx]);
                }
            }
        }
    }
}

void encodeRawVideo( array<unsigned long long, 8>& stats,
                   const std::string& inputFile,
                   const std::string& outputFile,
                   const BlockMatchingParams& params,
                   int frame_period,
                   int shiftBits) {
    ifstream input(inputFile, ios::binary);
    BitStream stream(outputFile, true);
    if (!input) {
        cerr << "Error: Could not open input file." << endl;
        return;
    }

    string headerLine;
    getline(input, headerLine);

    // Calculate line length in bits (8 bits per char)
    int lineSize = headerLine.length() * 8;

    // Write line size followed by line content
    stream.writeBits(lineSize, 32); // Using 32 bits to store size
    for(char c : headerLine) {
        stream.writeBits(static_cast<int>(c), 8);
    }


    int width, height, uvWidth, uvHeight, uvFrameSize, yFrameSize ,frame_count = 0, frameCount = 0;

    parseY4MHeader(input, width, height, frame_count, uvWidth, uvHeight, uvFrameSize, yFrameSize);

    // Write header information
    stream.writeBits(width, 16);
    stream.writeBits(height, 16);
    stream.writeBits(uvWidth, 16);
    stream.writeBits(uvHeight, 16);
    stream.writeBits(shiftBits, 16);
    stream.writeBits(frame_count, 32);
    stream.writeBits(params.blockSize, 8);
    stream.writeBits(params.searchRange, 8);



    vector<unsigned char> yPlane(yFrameSize);
    vector<unsigned char> uPlane(uvFrameSize);
    vector<unsigned char> vPlane(uvFrameSize);

    Mat currentFrameY(height, width, CV_8UC1, yPlane.data());
    Mat currentFrameU(uvHeight, uvWidth, CV_8UC1, uPlane.data());
    Mat currentFrameV(uvHeight, uvWidth, CV_8UC1, vPlane.data());

    Mat referenceFrameY(height, width, CV_8UC1, Scalar(0));
    Mat referenceFrameU(uvHeight, uvWidth, CV_8UC1, Scalar(0));
    Mat referenceFrameV(uvHeight, uvWidth, CV_8UC1, Scalar(0));



    string line;
    // Read until we find the "YUV4MPEG2" header, then start processing frames
    while (getline(input, line)) {
        if (line.find("FRAME") != string::npos) {
            input.read(reinterpret_cast<char*>(yPlane.data()), yFrameSize);
            if (input.gcount() != yFrameSize) {
                cerr << "Error: Incomplete Y plane read." << endl;
                break;
            }

            // Read U plane
            input.read(reinterpret_cast<char*>(uPlane.data()), uvFrameSize);
            if (input.gcount() != uvFrameSize) {
                cerr << "Error: Incomplete U plane read." << endl;
                break;
            }

            // Read V plane
            input.read(reinterpret_cast<char*>(vPlane.data()), uvFrameSize);
            if (input.gcount() != uvFrameSize) {
                cerr << "Error: Incomplete V plane read." << endl;
                break;
            }

            bool doIntra = false;
            if(frameCount == 0) doIntra = true;
            else if(frame_period == -1) doIntra = false;
            else if(frame_period == 0) doIntra = true;
            else if(frameCount % frame_period == 0) doIntra = true;

            stream.writeBits(doIntra ? 0 : 1, 1);

            if (doIntra) {
                stats[0]++;
                encodeFrameIntra(currentFrameY, stream, shiftBits);
                encodeFrameIntra(currentFrameU, stream, shiftBits);
                encodeFrameIntra(currentFrameV, stream, shiftBits);
                currentFrameY.copyTo(referenceFrameY);
                currentFrameU.copyTo(referenceFrameU);
                currentFrameV.copyTo(referenceFrameV);
            } else {
                stats[1]++;
                encodeFrameInter(currentFrameY, referenceFrameY, stream, stats[2], stats[3], shiftBits, params);
                encodeFrameInter(currentFrameU, referenceFrameU, stream, stats[4], stats[5], shiftBits, params);
                encodeFrameInter(currentFrameV, referenceFrameV, stream, stats[6], stats[7], shiftBits, params);
                currentFrameY.copyTo(referenceFrameY);
                currentFrameU.copyTo(referenceFrameU);
                currentFrameV.copyTo(referenceFrameV);
            }

            frameCount++;
        }
    }

    cout << "Video encoded successfully."<<endl;
}