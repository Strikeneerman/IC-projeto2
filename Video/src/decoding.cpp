
#include "VideoCodec.h"

void decodeFrameIntra(Mat& frame, BitStream& stream, int shiftBits) {
    // Read m value for the frame
    int m = stream.readBits(8);

    Golomb golomb(m, false);

    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            try {
                int residual = golomb.decode(stream);
                residual <<= shiftBits;
                int predicted = predictPixel(frame, x, y);
                frame.at<uchar>(y, x) = saturate_cast<uchar>(residual + predicted);
            } catch (const std::runtime_error& e) {
                cerr << "Error during decoding: " << e.what() << endl;
                return;  // Exit the decoding loop if EOF or any error occurs
            }
        }
    }
}

void decodeFrameInter(Mat& frame, const Mat& referenceFrame,
                      BitStream& stream,
                      int shiftBits,
                      const BlockMatchingParams& params = BlockMatchingParams()) {
    const int rows = frame.rows;
    const int cols = frame.cols;

    // Process each block
    for (int y = 0; y < rows; y += params.blockSize) {
        for (int x = 0; x < cols; x += params.blockSize) {
            int currentBlockHeight = min(params.blockSize, rows - y);
            int currentBlockWidth = min(params.blockSize, cols - x);

            bool useInter = (stream.readBit() == 1);

            // Read m value for this block
            int blockM = stream.readBits(8);
            Golomb golomb(blockM, false);

            if(useInter){
                // Decode motion vector
                int dx = golomb.decode(stream);
                int dy = golomb.decode(stream);
                MotionVector mv(dx, dy);

                // Get predicted block from reference frame
                Mat predictedBlock = getPredictedBlock(referenceFrame, x, y,
                                                    currentBlockWidth, currentBlockHeight,
                                                    mv);

                // Decode residuals and reconstruct the block
                for (int by = 0; by < currentBlockHeight; ++by) {
                    for (int bx = 0; bx < currentBlockWidth; ++bx) {
                        int residual = golomb.decode(stream);
                        residual <<= shiftBits;
                        frame.at<uchar>(y + by, x + bx) = saturate_cast<uchar>(
                            predictedBlock.at<uchar>(by, bx) + residual
                        );
                    }
                }
            } else {
                Mat currentBlockIntra(currentBlockHeight, currentBlockWidth, frame.type());

                for (int by = 0; by < currentBlockHeight; ++by) {
                    for (int bx = 0; bx < currentBlockWidth; ++bx) {
                        int residual = golomb.decode(stream);
                        residual <<= shiftBits;
                        int predicted = predictPixel(currentBlockIntra, bx, by);
                        currentBlockIntra.at<uchar>(by, bx ) = saturate_cast<uchar>(residual + predicted);
                        frame.at<uchar>(y + by,  x + bx ) = saturate_cast<uchar>(residual + predicted);
                    }
                }
            }
        }
    }
}

void decodeRawVideo(const string& inputFile, const string& outputFile) {
    BitStream stream(inputFile, false);
    int linesize = stream.readBits(32);
    string header = "";
    for (int i = 0; i < linesize / 8; i++) {
        char c = static_cast<char>(stream.readBits(8));
        header += c;
    }
    int width = stream.readBits(16);
    int height = stream.readBits(16);
    int uvWidth = stream.readBits(16);
    int uvHeight = stream.readBits(16);
    int shiftBits = stream.readBits(16);
    int frame_count = stream.readBits(32);
    int blockSize = stream.readBits(8);
    int searchRange = stream.readBits(8);

    BlockMatchingParams params(blockSize, searchRange);

    ofstream output(outputFile, ios::binary);
    output << header << endl;

    int yFrameSize = width * height;
    int uvFrameSize = uvWidth * uvHeight;

    // Matrices for the current frame and reference frame
    Mat currentFrameY(height, width, CV_8UC1, Scalar(0));
    Mat currentFrameU(uvHeight, uvWidth, CV_8UC1, Scalar(0));
    Mat currentFrameV(uvHeight, uvWidth, CV_8UC1, Scalar(0));

    Mat referenceFrameY(height, width, CV_8UC1, Scalar(0));
    Mat referenceFrameU(uvHeight, uvWidth, CV_8UC1, Scalar(0));
    Mat referenceFrameV(uvHeight, uvWidth, CV_8UC1, Scalar(0));

    while (frame_count!=0) {
        try {
            // Read frame type
            bool isIntra = (stream.readBits(1) == 0);

            if (isIntra) {
                decodeFrameIntra(currentFrameY, stream, shiftBits);
                decodeFrameIntra(currentFrameU, stream, shiftBits);
                decodeFrameIntra(currentFrameV, stream, shiftBits);
                currentFrameY.copyTo(referenceFrameY);
                currentFrameU.copyTo(referenceFrameU);
                currentFrameV.copyTo(referenceFrameV);
            } else {
                decodeFrameInter(currentFrameY, referenceFrameY, stream, shiftBits, params);
                decodeFrameInter(currentFrameU, referenceFrameU, stream, shiftBits, params);
                decodeFrameInter(currentFrameV, referenceFrameV, stream, shiftBits, params);
                currentFrameY.copyTo(referenceFrameY);
                currentFrameU.copyTo(referenceFrameU);
                currentFrameV.copyTo(referenceFrameV);
            }

            // Write the YUV frame to the output file
            output.write("FRAME\n", 6);  // Write frame delimiter
            output.write(reinterpret_cast<const char*>(currentFrameY.data), yFrameSize);
            output.write(reinterpret_cast<const char*>(currentFrameU.data), uvFrameSize);
            output.write(reinterpret_cast<const char*>(currentFrameV.data), uvFrameSize);

            frame_count--;

        } catch (const std::runtime_error& e) {
            cerr << "Decoding finished or encountered error: " << e.what() << endl;
            break;
        }
    }
}