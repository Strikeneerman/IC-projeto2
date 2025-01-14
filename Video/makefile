# Makefile for video_inter Project
SHELL := /bin/bash

# Directories
SRC_DIR := .
BIN_DIR := bin
VIDEO_DIR := videos
ENCODED_DIR := encoded
OUTPUT_DIR := output
LOG_DIR := logs

# Log files
ENCODE_LOG := $(LOG_DIR)/video_inter_encode.log
DECODE_LOG := $(LOG_DIR)/video_inter_decode.log

# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -O3 -Wall -fopenmp $(shell pkg-config --cflags opencv4)
LDFLAGS := $(shell pkg-config --libs opencv4)

# Source files and binary
SOURCES := ./src/video_inter.cpp ./src/encoding.cpp ./src/decoding.cpp ./src/utils.cpp
BINARY := $(BIN_DIR)/video_inter

# Optional flags for encoding
SEARCH_SIZE ?= 16
BLOCK_SIZE ?= 8
FRAMES ?= -1
LOSSY_RATIO ?= 1.0

# Videos
VIDEOS := $(wildcard $(VIDEO_DIR)/*.y4m)

# Targets
.PHONY: all clean encode decode directories

all: directories $(BINARY) encode decode

directories:
	@mkdir -p $(BIN_DIR) $(ENCODED_DIR) $(OUTPUT_DIR) $(LOG_DIR)
	@touch $(ENCODE_LOG) $(DECODE_LOG)

$(BINARY): $(SOURCES)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

# Encode target
encode: directories $(BINARY)
	@for video in $(VIDEOS); do \
		filename=$$(basename $$video .y4m); \
		output=$(ENCODED_DIR)/$${filename}_inter.enc; \
		echo "Encoding $$video with $(BINARY) -> $$output"; \
		$(BINARY) -encode $$video $$output -s $(SEARCH_SIZE) -b $(BLOCK_SIZE) -f $(FRAMES) -l $(LOSSY_RATIO) >> $(ENCODE_LOG) 2>&1; \
	done

# Decode target
decode: directories $(BINARY)
	@for encoded in $(wildcard $(ENCODED_DIR)/*.enc); do \
		filename=$$(basename $$encoded .enc); \
		output=$(OUTPUT_DIR)/$${filename}_decoded.y4m; \
		echo "Decoding $$encoded with $(BINARY) -> $$output"; \
		$(BINARY) -decode $$encoded $$output >> $(DECODE_LOG) 2>&1; \
	done

clean:
	rm -rf $(BIN_DIR) $(ENCODED_DIR) $(OUTPUT_DIR) $(LOG_DIR)

help:
	@echo "Usage:"
	@echo "  make all                     - Compile the video_inter binary encode and decode all videos"
	@echo "  make encode                  - Encode all videos in $(VIDEO_DIR)"
	@echo "      Optional flags for encode:"
	@echo "        SEARCH_SIZE=<value>    - Set search size (default: 16)"
	@echo "        BLOCK_SIZE=<value>     - Set block size (default: 8)"
	@echo "        FRAMES=<value>         - Set frame period (default: 0)"
	@echo "        LOSSY_RATIO=<value>    - Set lossy ratio (default: 1.0)"
	@echo "  make decode                  - Decode all encoded videos in $(ENCODED_DIR)"
	@echo "  make clean                   - Remove all generated files"
