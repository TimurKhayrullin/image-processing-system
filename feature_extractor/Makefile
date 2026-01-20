CXX := g++

INCLUDES := \
    -I../lib/include \
    -Iinclude \
	-I/opt/homebrew/include \
    -I/opt/homebrew/include/opencv4
#     -I/usr/local/include/opencv4 # LINUX

CXXFLAGS := -std=c++17 -Wall -Wextra $(INCLUDES) -MMD -MP

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
    -lopencv_features2d

# 	-L/usr/lib \ # LINUX
# 	-L/usr/lib/x86_64-linux-gnu \

SRC_DIR := src
OBJ_DIR := ../build/feature_extractor
BIN_DIR := ../build/feature_extractor
EXEC_NAME := feature_extractor
TARGET := $(BIN_DIR)/$(EXEC_NAME)

SRCS := \
	$(SRC_DIR)/message_handling.cpp \
	$(SRC_DIR)/extractor.cpp \
	main.cpp

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
