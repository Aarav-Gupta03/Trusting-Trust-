CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

SRCS = main.cpp lexer.cpp parser.cpp codegen.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = tlc

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) *_generated.cpp test_runner

test: all
	@echo "== Compiling auth.tl =="
	./$(TARGET) tests/auth.tl -o tests/auth_generated.cpp
	@echo "== Compiling variables.tl =="
	./$(TARGET) tests/variables.tl -o tests/variables_generated.cpp
	@echo "== Building and running test harness =="
	$(CXX) $(CXXFLAGS) -o test_runner tests/test_main.cpp
	./test_runner
