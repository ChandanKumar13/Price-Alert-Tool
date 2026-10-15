CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
LIB_SRC  := src/json.cpp src/prices.cpp src/rules.cpp src/engine.cpp
HEADERS  := $(wildcard src/*.hpp tests/*.hpp)

alerts: $(LIB_SRC) src/main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -Isrc $(LIB_SRC) src/main.cpp -o $@

run_tests: $(LIB_SRC) tests/test_alerts.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -Isrc -Itests $(LIB_SRC) tests/test_alerts.cpp -o $@

test: run_tests
	./run_tests

clean:
	rm -f alerts run_tests

.PHONY: test clean
