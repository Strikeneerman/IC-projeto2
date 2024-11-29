# Paths
INCLUDE_DIR = ./SFML-2.6.2/include
LIB_DIR = ./SFML-2.6.2/lib
SRC = audio.cpp
OUT = audio

# Compiler and flags
CXX = g++
CXXFLAGS = -I$(INCLUDE_DIR) -std=c++17
LDFLAGS = -L$(LIB_DIR)
LIBS = -lsfml-audio -lsfml-system

# Build target
$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC) $(LDFLAGS) $(LIBS)

# Run target
run:
	./$(OUT)

# Clean target
clean:
ifeq ($(OS),Windows_NT)
	del $(OUT).exe
else
	rm -f $(OUT)
endif
