# Formatting .c/.cpp/.h

```bash
find . -name '*.c' -o -name '*.cpp' -o -name '*.h' | xargs clang-format -i
```
