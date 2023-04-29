@echo off
cl.exe checker.cpp -std:c++17 -EHsc -W4 -Ox -MT -Fe:script_syntax_check.exe
