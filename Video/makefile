# Makefile for Video Project

# Directories
SRC_DIR := src
BIN_DIR := bin
VIDEO_DIR := videos
ENCODED_DIR := encoded
OUTPUT_DIR := output
LOG_DIR := logs

# Log files
BLOCK_LOG_ENCODE := $(LOG_DIR)/video_block_encode.txt
FRAME_LOG_ENCODE := $(LOG_DIR)/video_frame_encode.
BLOCK_LOG_DECODE := $(LOG_DIR)/video_block_decode.txt
FRAME_LOG_DECODE := $(LOG_DIR)/video_frame_decode.txt

# Source files and binaries
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
BINARIES := $(patsubst $(SRC_DIR)/%.cpp, $(BIN_DIR)/%, $(SRC_FILES))
VIDEOS := $(wildcard $(VIDEO_DIR)/*.y4m)

# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -fopenmp $(shell pkg-config --cflags opencv4)
LDFLAGS := $(shell pkg-config --libs opencv4)

# Default m value for video_frame
M ?= 3

# Targets
.PHONY: all clean encode encode_block encode_frame decode directories

directories:
	@mkdir -p $(BIN_DIR) $(ENCODED_DIR) $(OUTPUT_DIR) $(LOG_DIR)
	@touch $(BLOCK_LOG_DECODE) $(BLOCK_LOG_DECODE) $(FRAME_LOG_DECODE) $(FRAME_LOG_ENCODE)

all: compile encode decode

compile: directories $(BINARIES)

$(BIN_DIR)/%: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

# Split encode into two separate targets
encode: encode_block encode_frame

# Encode using video_block
encode_block: directories $(BIN_DIR)/video_block
	@for video in $(VIDEOS); do \
		filename=$$(basename $$video .y4m); \
		output=$(ENCODED_DIR)/$${filename}_video_block.enc; \
		echo "Encoding $$video with bin/video_block -> $$output"; \
		$(BIN_DIR)/video_block -encode $$video $$output -v >> $(BLOCK_LOG_ENCODE) 2>&1; \
	done

# Encode using video_frame
encode_frame: directories $(BIN_DIR)/video_frame
	@for video in $(VIDEOS); do \
		filename=$$(basename $$video .y4m); \
		output=$(ENCODED_DIR)/$${filename}_video_frame_m$(M).enc; \
		echo "Encoding $$video with bin/video_frame (m=$(M)) -> $$output"; \
		$(BIN_DIR)/video_frame -encode $$video $$output $(M) >> $(FRAME_LOG_ENCODE) 2>&1; \
	done

# Decode all encoded videos
decode: directories $(BINARIES)
	@for encoded in $(wildcard $(ENCODED_DIR)/*.enc); do \
		filename=$$(basename $$encoded | sed 's/_m[0-9]\+\.enc$$//'); \
		if [[ $$encoded == *"video_frame"* ]]; then \
			output=$(OUTPUT_DIR)/$${filename}_decoded.y4m; \
			echo "Decoding $$encoded with bin/video_frame -> $$output"; \
			$(BIN_DIR)/video_frame -decode $$encoded $$output >> $(FRAME_LOG_DECODE) 2>&1; \
		else \
			output=$(OUTPUT_DIR)/$${filename}_decoded.y4m; \
			echo "Decoding $$encoded with bin/video_block -> $$output"; \
			$(BIN_DIR)/video_block -decode $$encoded $$output -v >> $(BLOCK_LOG_DECODE) 2>&1; \
		fi; \
	done

clean:
	rm -rf $(BIN_DIR) $(ENCODED_DIR) $(OUTPUT_DIR) $(LOG_DIR)

help:
	@echo "Usage:"
	@echo "  make all              - Compile all binaries"
	@echo "  make encode          - Run both encoders"
	@echo "  make encode_block    - Run only video_block encoder"
	@echo "  make encode_frame M=<value>  - Run only video_frame encoder (default m=3)"
	@echo "  make decode          - Decode all encoded videos"
	@echo "  make clean           - Remove all generated files"