# Compiler
CXX := g++

# ------------------------------------------------------------
# Include paths for header files inside lib/
# ------------------------------------------------------------
INCLUDES := \
	-Iinclude

# ------------------------------------------------------------
# Compiler flags
# ------------------------------------------------------------
CXXFLAGS := -std=c++17 -Wall -Wextra $(INCLUDES) -MMD -MP

# ------------------------------------------------------------
# Directories
# ------------------------------------------------------------
SRC_DIR := src
BUILD_DIR := ../build/lib
LIB_NAME := libshared.a
TARGET := $(BUILD_DIR)/$(LIB_NAME)

# ------------------------------------------------------------
# Library sources
# ------------------------------------------------------------
SRCS := \
    $(SRC_DIR)/shutdown_handler.cpp

OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# ------------------------------------------------------------
# Build rules
# ------------------------------------------------------------
all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	ar rcs $(TARGET) $(OBJS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
