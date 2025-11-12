CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../include -Iinclude \
            -I/opt/homebrew/include -I/opt/homebrew/opt/libpq/include \
            -I/opt/homebrew/opt/yaml-cpp/include -I/opt/homebrew/opt/zeromq/include -MMD -MP

LDFLAGS := -L/opt/homebrew/lib \
           -L/opt/homebrew/opt/libpq/lib \
           -L/opt/homebrew/opt/yaml-cpp/lib \
           -L/opt/homebrew/opt/zeromq/lib \
           -lpqxx -lpq -lyaml-cpp -lzmq

SRC_DIR := src
OBJ_DIR := ../build/data_logger
BIN_DIR := ../build/data_logger
EXEC_NAME := data_logger
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
