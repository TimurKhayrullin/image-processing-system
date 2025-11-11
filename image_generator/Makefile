CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../include -Iinclude \
            -I/opt/homebrew/include -MMD -MP
LDFLAGS := -L/opt/homebrew/lib -lzmq

SRC_DIR := src
OBJ_DIR := ../build/image_generator
BIN_DIR := ../build/image_generator
EXEC_NAME := image_generator
TARGET := $(BIN_DIR)/$(EXEC_NAME)

SRCS := $(SRC_DIR)/$(EXEC_NAME).cpp main.cpp
OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(OBJ_DIR)

.PHONY: all clean
