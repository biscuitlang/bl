find . -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" \) -exec clang-format -i {} +
