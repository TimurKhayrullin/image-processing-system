CXX := g++

# ------------------------------------------------------------
# Include paths
# Homebrew installs OpenCV under /opt/homebrew/include/opencv4
# ------------------------------------------------------------
INCLUDES := \
    -I../lib/include \
    -Iinclude \
    -I/opt/homebrew/include \
    -I/opt/homebrew/include/opencv4

# ------------------------------------------------------------
# Compiler and linker flags
# ------------------------------------------------------------
CXXFLAGS := -std=c++17 -Wall -Wextra $(INCLUDES) -MMD -MP

# Link ZeroMQ + OpenCV
LDFLAGS := \
    -L../build/lib \
    -lshared \
    -L/opt/homebrew/lib \
    -lzmq \
    -lopencv_core \
    -lopencv_imgcodecs \
    -lopencv_highgui \
    -lopencv_imgproc \
    -lopencv_features2d

# ------------------------------------------------------------
# Directory setup
# ------------------------------------------------------------
SRC_DIR := src
OBJ_DIR := ../build/image_generator
BIN_DIR := ../build/image_generator
EXEC_NAME := image_generator
TARGET := $(BIN_DIR)/$(EXEC_NAME)

# ------------------------------------------------------------
# Source files
# ------------------------------------------------------------
SRCS := \
    $(SRC_DIR)/image_generator.cpp \
    $(SRC_DIR)/image_handlers.cpp \
    main.cpp

OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# ------------------------------------------------------------
# Build rules
# ------------------------------------------------------------
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
