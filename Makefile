CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -fPIC -pthread

BUILD_DIR = build
LIB_NAME = liborchestrator.so

.PHONY: all clean test

all: $(BUILD_DIR)/$(LIB_NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(LIB_NAME): core/orchestrator.cpp core/orchestrator.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -shared core/orchestrator.cpp -o $(BUILD_DIR)/$(LIB_NAME)

$(BUILD_DIR)/test_scheduler: tests/test_scheduler.cpp core/orchestrator.cpp core/orchestrator.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) tests/test_scheduler.cpp core/orchestrator.cpp -o $(BUILD_DIR)/test_scheduler

test: $(BUILD_DIR)/test_scheduler
	./$(BUILD_DIR)/test_scheduler

clean:
	rm -rf $(BUILD_DIR)
