# KiCad-Auditor - Core Build System
# Locked to MSYS64 MinGW64 clang++ compiler with C++20 standard

CXX = D:/software/msys64/mingw64/bin/clang++.exe
CXXFLAGS = -std=c++20 -O3 -Wall -Wextra -Werror

# Target output executable
TARGET = kicad-auditor.exe

# Sources
SRCS = src/main.cpp src/common/sexpr.cpp src/common/json.cpp src/common/report.cpp src/test_sexpr.cpp \
       src/sch/sch_analyzer.cpp src/sch/rules/isolation_rule.cpp src/sch/rules/fb_resistor_rule.cpp \
       src/pcb/pcb_analyzer.cpp src/pcb/rules/emi_clearance_rule.cpp src/pcb/rules/sensitive_shield_rule.cpp

# Default rule
all: $(TARGET)

$(TARGET): $(SRCS) src/common/types.hpp src/common/sexpr.hpp src/common/json.hpp src/common/rule.hpp src/common/report.hpp src/common/registry.hpp src/sch/sch_analyzer.hpp src/pcb/pcb_analyzer.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	@if exist $(TARGET) del /F /Q $(TARGET)

.PHONY: all clean
