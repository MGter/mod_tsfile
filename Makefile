# Makefile for mod_tsfile project

CXX = g++
CXXFLAGS = -std=c++11 -g -Wall -Wextra
LDFLAGS = -lpthread
INCLUDES = -I./include

# Source files
SRCS = src/main.cpp \
       src/mod_ts_file.cpp \
       src/parse_json.cpp \
       src/common/Log.cpp \
       src/common/Tool.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Target
TARGET = mod_tsfile

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $(TARGET)

# Compile
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS) $(TARGET)

# Run with config
run: $(TARGET)
	./$(TARGET) $(CONF)

# Debug with gdb
debug: $(TARGET)
	gdb ./$(TARGET) $(CONF)

# Phony targets
.PHONY: all clean run debug