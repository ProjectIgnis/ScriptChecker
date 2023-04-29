#!/bin/sh
CXX=${CXX:-c++}
DL="-ldl"
if [ -n "${NODL+1}" ]; then
	DL=""
fi
$CXX checker.cpp -std=c++17 -fexceptions $DL -lstdc++fs -Wpedantic -Wextra -Werror -O3 -static-libgcc -static-libstdc++ -o script_syntax_check
