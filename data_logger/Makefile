CXX := g++

INCLUDES := \
        -I../lib/include \
        -Iinclude \
        -I/opt/homebrew/include \
        -I/opt/homebrew/include/opencv4 \
        -I/opt/homebrew/opt/libpq/include
#     -I/usr/local/include/opencv4 # LINUX

CXXFLAGS := -std=c++17 -Wall -Wextra $(INCLUDES) -MMD -MP

LDFLAGS := \
        -L../build/lib \
        -lshared \
        -L/opt/homebrew/lib \
        -L/opt/homebrew/opt/libpq/lib \
        -lpqxx \
        -lpq \
        -lyaml-cpp \
        -lzmq \
        -lopencv_core \
        -lopencv_imgcodecs \
        -lopencv_highgui \
        -lopencv_imgproc \
        -lopencv_features2d \

#         -L/usr/lib \ # LINUX
#         -L/usr/lib/x86_64-linux-gnu \

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
