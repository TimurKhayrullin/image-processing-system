CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra \
            -I../include -Iinclude \
            -I/opt/homebrew/include \
            -I/opt/homebrew/opt/libpq/include \
            -MMD -MP

LDFLAGS := -L/opt/homebrew/lib \
           -L/opt/homebrew/opt/libpq/lib \
           -lpqxx -lpq -lyaml-cpp

SRC_DIR := src
OBJ_DIR := ../../build/utility/psql_export_csv
BIN_DIR := ../../build/utility/psql_export_csv
EXEC_NAME := psql_export_csv
TARGET := $(BIN_DIR)/$(EXEC_NAME)

SRCS := $(SRC_DIR)/$(EXEC_NAME).cpp
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
