CXX := g++
CXXFLAGS ?= -O2 -Wall -Wextra -Iinclude
LDFLAGS :=
LDLIBS := -lSDL2

SRC := $(wildcard src/*.cpp)
OBJ := $(patsubst src/%.cpp, build/%.o, $(SRC))
DEPS := $(OBJ:.o=.d)
TARGET := build/main

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJ) $(LDFLAGS) $(LDLIBS) -o $@

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build *.o *.d main

.PHONY: all clean run
