# ==============================================================================
# Cross-Platform Makefile for DNS Resolver & Caching Server
# Supported Environments:
#   - Linux / VMware Workstation Pro Virtual Machines (make)
#   - Windows with MinGW (mingw32-make)
# ==============================================================================

CXX ?= g++
CXXFLAGS = -std=c++14 -Wall -Wextra -Iinclude -O2

ifeq ($(OS),Windows_NT)
    TARGET_EXT = .exe
    LDFLAGS = -lws2_32
    RM = del /Q
    MKDIR = if not exist bin mkdir bin
    SEP = \\
else
    TARGET_EXT =
    LDFLAGS = -lpthread
    RM = rm -f
    MKDIR = mkdir -p bin
    SEP = /
endif

BIN_DIR = bin
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests

# Engine Object Files
OBJS = $(BIN_DIR)/protocol_engine.o \
       $(BIN_DIR)/storage_engine.o \
       $(BIN_DIR)/upstream_engine.o \
       $(BIN_DIR)/telemetry_engine.o \
       $(BIN_DIR)/vm_engine.o

# Default Target
all: $(BIN_DIR)/dns_server$(TARGET_EXT)

# Compile Object Files
$(BIN_DIR)/protocol_engine.o: $(SRC_DIR)/protocol_engine.cpp $(INC_DIR)/protocol_engine.hpp $(INC_DIR)/common.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR)/storage_engine.o: $(SRC_DIR)/storage_engine.cpp $(INC_DIR)/storage_engine.hpp $(INC_DIR)/common.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR)/upstream_engine.o: $(SRC_DIR)/upstream_engine.cpp $(INC_DIR)/upstream_engine.hpp $(INC_DIR)/common.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR)/telemetry_engine.o: $(SRC_DIR)/telemetry_engine.cpp $(INC_DIR)/telemetry_engine.hpp $(INC_DIR)/common.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR)/vm_engine.o: $(SRC_DIR)/vm_engine.cpp $(INC_DIR)/vm_engine.hpp $(INC_DIR)/common.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Main Server Executable
$(BIN_DIR)/dns_server$(TARGET_EXT): $(SRC_DIR)/main.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $(SRC_DIR)/main.cpp $(OBJS) $(LDFLAGS) -o $@

# Test Executables
$(BIN_DIR)/test_protocol$(TARGET_EXT): $(TEST_DIR)/test_protocol.cpp $(BIN_DIR)/protocol_engine.o
	$(CXX) $(CXXFLAGS) $< $(BIN_DIR)/protocol_engine.o $(LDFLAGS) -o $@

$(BIN_DIR)/test_cache$(TARGET_EXT): $(TEST_DIR)/test_cache.cpp $(BIN_DIR)/storage_engine.o $(BIN_DIR)/protocol_engine.o
	$(CXX) $(CXXFLAGS) $< $(BIN_DIR)/storage_engine.o $(BIN_DIR)/protocol_engine.o $(LDFLAGS) -o $@

$(BIN_DIR)/test_upstream$(TARGET_EXT): $(TEST_DIR)/test_upstream.cpp $(BIN_DIR)/upstream_engine.o $(BIN_DIR)/protocol_engine.o
	$(CXX) $(CXXFLAGS) $< $(BIN_DIR)/upstream_engine.o $(BIN_DIR)/protocol_engine.o $(LDFLAGS) -o $@

$(BIN_DIR)/test_vm$(TARGET_EXT): $(TEST_DIR)/test_vm_integration.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $< $(OBJS) $(LDFLAGS) -o $@

# Test Runner Targets
test: test_protocol test_cache test_upstream test_vm

test_protocol: $(BIN_DIR)/test_protocol$(TARGET_EXT)
	@$(BIN_DIR)$(SEP)test_protocol$(TARGET_EXT)

test_cache: $(BIN_DIR)/test_cache$(TARGET_EXT)
	@$(BIN_DIR)$(SEP)test_cache$(TARGET_EXT)

test_upstream: $(BIN_DIR)/test_upstream$(TARGET_EXT)
	@$(BIN_DIR)$(SEP)test_upstream$(TARGET_EXT)

test_vm: $(BIN_DIR)/test_vm$(TARGET_EXT)
	@$(BIN_DIR)$(SEP)test_vm$(TARGET_EXT)

.PHONY: all test clean test_protocol test_cache test_upstream test_vm

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(BIN_DIR)\*.o del /Q $(BIN_DIR)\*.o
	@if exist $(BIN_DIR)\*.exe del /Q $(BIN_DIR)\*.exe
else
	@rm -f $(BIN_DIR)/*.o $(BIN_DIR)/dns_server $(BIN_DIR)/test_*
endif
