CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
SOURCES = main.cpp lexer.cpp parser.cpp ast.cpp semantic.cpp symbol_table.cpp interpreter.cpp codegen.cpp assembly.cpp
all: logix_cpp
logix_cpp: $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o logix_cpp
clean:
	rm -f logix_cpp logix_cpp.exe
