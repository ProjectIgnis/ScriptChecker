#!/bin/sh
CXX=${CXX:-c++}
DL="-ldl"
if [ -n "${NODL+1}" ]; then
	DL=""
fi
GCC_LIBS=" -lstdc++fs -static-libgcc -static-libstdc++"
if [ -n "${MACOS+1}" ]; then
	GCC_LIBS=""
fi
$CXX checker.cpp -std=c++17 -fexceptions $DL -Wpedantic -Wextra -Werror -O3 $GCC_LIBS -o script_syntax_check
