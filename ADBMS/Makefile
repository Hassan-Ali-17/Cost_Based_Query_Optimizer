CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Werror -O2
LDFLAGS :=

SRC := src/main.cpp src/parser.cpp src/catalog.cpp src/plan.cpp src/rewriter.cpp src/cost.cpp src/joinorder.cpp src/executor.cpp
OBJ := $(SRC:.cpp=.o)

all: qopt datagen bench

qopt: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LDFLAGS)

CB-SQLopt: qopt
	cp qopt CB-SQLopt

datagen: datagen/datagen.c
	$(CXX) $(CXXFLAGS) -o $@ datagen/datagen.cpp

bench: benchmark/bench.c
	$(CXX) $(CXXFLAGS) -o $@ benchmark/bench.cpp

test: qopt tests/test_parser tests/test_cost tests/test_rewriter tests/test_joinorder
	./tests/test_parser
	./tests/test_cost
	./tests/test_rewriter
	./tests/test_joinorder

clean:
	rm -f qopt CB-SQLopt datagen bench $(OBJ) tests/test_parser tests/test_cost tests/test_rewriter tests/test_joinorder

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

tests/test_parser: tests/test_parser.cpp src/parser.cpp src/plan.cpp
	$(CXX) $(CXXFLAGS) -I./src -o $@ tests/test_parser.cpp src/parser.cpp src/plan.cpp

tests/test_cost: tests/test_cost.cpp src/cost.cpp src/catalog.cpp src/plan.cpp
	$(CXX) $(CXXFLAGS) -I./src -o $@ tests/test_cost.cpp src/cost.cpp src/catalog.cpp src/plan.cpp

tests/test_rewriter: tests/test_rewriter.cpp src/rewriter.cpp src/plan.cpp
	$(CXX) $(CXXFLAGS) -I./src -o $@ tests/test_rewriter.cpp src/rewriter.cpp src/plan.cpp

tests/test_joinorder: tests/test_joinorder.cpp src/joinorder.cpp src/plan.cpp src/catalog.cpp src/cost.cpp
	$(CXX) $(CXXFLAGS) -I./src -o $@ tests/test_joinorder.cpp src/joinorder.cpp src/plan.cpp src/catalog.cpp src/cost.cpp

.PHONY: all clean test
