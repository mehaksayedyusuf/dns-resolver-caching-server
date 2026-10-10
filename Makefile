# Makefile for DNS Resolver & Caching Server (C++)
# Environment: MinGW GCC on Windows

CXX ?= g++
CXXFLAGS = -std=c++14 -Wall -Wextra -Iinclude -O2
LDFLAGS = -lws2_32

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
all: $(BIN_DIR)/dns_server.exe

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
$(BIN_DIR)/dns_server.exe: $(SRC_DIR)/main.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $(SRC_DIR)/main.cpp $(OBJS) $(LDFLAGS) -o $@

# Test Executables
$(BIN_DIR)/test_protocol.exe: $(TEST_DIR)/test_protocol.cpp $(BIN_DIR)/protocol_engine.o
	$(CXX) $(CXXFLAGS) $< $(BIN_DIR)/protocol_engine.o $(LDFLAGS) -o $@

$(BIN_DIR)/test_cache.exe: $(TEST_DIR)/test_cache.cpp $(BIN_DIR)/storage_engine.o
	$(CXX) $(CXXFLAGS) $< $(BIN_DIR)/storage_engine.o $(LDFLAGS) -o $@

$(BIN_DIR)/test_upstream.exe: $(TEST_DIR)/test_upstream.cpp $(BIN_DIR)/upstream_engine.o $(BIN_DIR)/protocol_engine.o
	$(CXX) $(CXXFLAGS) $< $(BIN_DIR)/upstream_engine.o $(BIN_DIR)/protocol_engine.o $(LDFLAGS) -o $@

$(BIN_DIR)/test_vm.exe: $(TEST_DIR)/test_vm_integration.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) $< $(OBJS) $(LDFLAGS) -o $@

.PHONY: all test clean

clean:
	@if exist $(BIN_DIR)\*.o del /Q $(BIN_DIR)\*.o
	@if exist $(BIN_DIR)\*.exe del /Q $(BIN_DIR)\*.exe
