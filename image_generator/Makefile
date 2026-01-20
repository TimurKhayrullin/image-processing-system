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
#     -I/usr/local/include/opencv4 # LINUX

# ------------------------------------------------------------
# Compiler and linker flags
# ------------------------------------------------------------
CXXFLAGS := -std=c++17 -Wall -Wextra $(INCLUDES) -MMD -MP  # -g -fno-omit-frame-pointer -O0

# Link ZeroMQ + OpenCV + yamlcpp
LDFLAGS := \
    -L../build/lib \
    -lshared \
    -L/opt/homebrew/lib \
    -lyaml-cpp \
    -lzmq \
    -lopencv_core \
    -lopencv_imgcodecs \
    -lopencv_highgui \
    -lopencv_imgproc \
    -lopencv_features2d \

#     -L/usr/lib \ # LINUX
#     -L/usr/lib/x86_64-linux-gnu \

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
