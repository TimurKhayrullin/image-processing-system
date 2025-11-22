CXX := g++

INCLUDES := \
        -I../lib/include \
        -Iinclude \
        -I/usr/local/include/opencv4

CXXFLAGS := -std=c++17 -Wall -Wextra $(INCLUDES) -MMD -MP

LDFLAGS := \
        -L../build/lib \
        -lshared \
        -L/usr/lib \
        -L/usr/lib/x86_64-linux-gnu \
        -lpqxx \
        -lpq \
        -lyaml-cpp \
        -lzmq \
        -lopencv_core \
        -lopencv_imgcodecs \
        -lopencv_highgui \
        -lopencv_imgproc \
        -lopencv_features2d \

SRC_DIR := src
OBJ_DIR := ../build/data_logger
BIN_DIR := ../build/data_logger
EXEC_NAME := data_logger
TARGET := $(BIN_DIR)/$(EXEC_NAME)

SRCS := $(SRC_DIR)/data_logger.cpp \
        $(SRC_DIR)/database.cpp \
        $(SRC_DIR)/postgres_database.cpp \
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
