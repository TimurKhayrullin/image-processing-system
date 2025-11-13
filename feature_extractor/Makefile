CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../lib -Iinclude \
            -I/opt/homebrew/include -MMD -MP
LDFLAGS := -L/opt/homebrew/lib -lzmq

SRC_DIR := src
OBJ_DIR := ../build/feature_extractor
BIN_DIR := ../build/feature_extractor
EXEC_NAME := feature_extractor
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
