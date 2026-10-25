CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
CXXFLAGS += -Ithird_party $(shell pkg-config --cflags sdl2)
LDLIBS   += $(shell pkg-config --libs sdl2) -lm

SRC = src/main.cpp src/gfx.cpp src/audio.cpp src/level.cpp
OBJ = $(SRC:.cpp=.o)
BIN = la-ultima-luciernaga

.PHONY: all run clean debug capture autoplay

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDLIBS)

src/%.o: src/%.cpp src/*.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

capture: $(BIN)
	mkdir -p captures
	SDL_VIDEODRIVER=dummy ./$(BIN) --capture captures

autoplay: $(BIN)
	SDL_VIDEODRIVER=dummy ./$(BIN) --autoplay

debug: CXXFLAGS += -g -O0 -fsanitize=address,undefined
debug: LDLIBS += -fsanitize=address,undefined
debug: clean all

clean:
	rm -f $(OBJ) $(BIN)
