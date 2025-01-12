
#include "VideoCodec.h"

void encodeFrameIntra(const Mat& frame, BitStream& stream) {
    vector<int> residuals;
    residuals.reserve(frame.rows * frame.cols);

    // First pass: collect residuals and calculate optimal m
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            int predicted = predictPixel(frame, x, y);
            int residual = frame.at<uchar>(y, x) - predicted;
            residuals.push_back(abs(residual));
        }
    }

    // Calculate average absolute residual
    double sum = std::accumulate(residuals.begin(), residuals.end(), 0.0);
    double average = sum / residuals.size();

    // Calculate optimal m
    int m = static_cast<int>(std::ceil(-1 / std::log2(1 - (1 / (average + 1)))));
    m = max(2, min(m, 16));

    // Write m value
    stream.writeBits(m, 8);

    Golomb golomb(m, false);

    // Second pass: encode residuals
    int i = 0;
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            int predicted = predictPixel(frame, x, y);
            int residual = frame.at<uchar>(y, x) - predicted;
            golomb.encode(stream, residual);
        }
    }
}

void encodeFrameInter(const Mat& currentFrame, const Mat& referenceFrame,
                     BitStream& stream, const BlockMatchingParams& params = BlockMatchingParams()) {
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
            Mat predictedBlock = getPredictedBlock(referenceFrame, x, y,
                                                 currentBlockWidth, currentBlockHeight,
                                                 mv);

            // Calculate residuals and optimal m for this block
            vector<int> blockResiduals;
            blockResiduals.reserve(currentBlockHeight * currentBlockWidth);

            for (int by = 0; by < currentBlockHeight; ++by) {
                for (int bx = 0; bx < currentBlockWidth; ++bx) {
                    int residual = currentBlock.at<uchar>(by, bx) -
                                 predictedBlock.at<uchar>(by, bx);
                    blockResiduals.push_back(abs(residual));
                }
            }

            // Calculate optimal m for this block
            double blockSum = std::accumulate(blockResiduals.begin(), blockResiduals.end(), 0.0);
            double blockAverage = blockSum / blockResiduals.size();
            int blockM = static_cast<int>(std::ceil(-1 / std::log2(1 - (1 / (blockAverage + 1)))));
            blockM = max(2, min(blockM, 16));

            // Write block header (m value and motion vectors)
            stream.writeBits(blockM, 8);
            Golomb golomb(blockM, false);
            golomb.encode(stream, mv.dx);
            golomb.encode(stream, mv.dy);

            // Encode residuals for the block
            int i = 0;
            for (int by = 0; by < currentBlockHeight; ++by) {
                for (int bx = 0; bx < currentBlockWidth; ++bx) {
                    int residual = currentBlock.at<uchar>(by, bx) -
                                 predictedBlock.at<uchar>(by, bx);
                    golomb.encode(stream, residual);
                }
            }
        }
    }
}

void encodeRawVideo(const std::string& inputFile,
                   const std::string& outputFile,
                   const BlockMatchingParams& params) {
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

    int width, height, frame_count = 0, frameCount = 0;
    parseY4MHeader(input, width, height, frame_count);

    // Write header information
    stream.writeBits(width, 16);
    stream.writeBits(height, 16);
    stream.writeBits(frame_count, 16);
    stream.writeBits(params.blockSize, 8);
    stream.writeBits(params.searchRange, 8);


    int yFrameSize = width * height;
    int uvWidth = width / 2;
    int uvHeight = height / 2;
    int uvFrameSize = uvWidth * uvHeight;

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

            // Determine frame type
            bool isIntra = (frameCount % 8 == 0);  // Every 8th frame is intra
            stream.writeBits(isIntra ? 0 : 1, 1);

            if (isIntra) {
                encodeFrameIntra(currentFrameY, stream);
                encodeFrameIntra(currentFrameU, stream);
                encodeFrameIntra(currentFrameV, stream);
                currentFrameY.copyTo(referenceFrameY);
                currentFrameU.copyTo(referenceFrameU);
                currentFrameV.copyTo(referenceFrameV);
            } else {
                encodeFrameInter(currentFrameY, referenceFrameY, stream, params);
                encodeFrameInter(currentFrameU, referenceFrameU, stream, params);
                encodeFrameInter(currentFrameV, referenceFrameV, stream, params);
                currentFrameY.copyTo(referenceFrameY);
                currentFrameU.copyTo(referenceFrameU);
                currentFrameV.copyTo(referenceFrameV);
            }

            frameCount++;
        }
    }

    cout << "Video encoded successfully."<<endl;
}