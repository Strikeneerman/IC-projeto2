# Automatically get the workspace directory
INCLUDE_DIR = ./SFML-2.6.1/include
LIB_DIR = ./SFML-2.6.1/lib
SRC = audio.cpp
OUT = audio

# Compiler and flags
CXX = g++
CXXFLAGS = -I$(INCLUDE_DIR) -L$(LIB_DIR) -lsfml-audio -lsfml-system

# Build target
$(OUT): $(SRC)
	$(CXX) $(SRC) -o $(OUT) $(CXXFLAGS)

run:
	./$(OUT)

# Clean target
clean:
	rm -f $(OUT)
