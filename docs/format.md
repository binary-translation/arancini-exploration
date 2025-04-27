# Formatting .c/.cpp/.h

```bash
find . -name '*.c' -o -name '*.cpp' -o -name '*.h' | xargs clang-format -i
```

# Formatting CMake

## Installing clang format

### Using `uv`

```bash
uv tool install cmakelang
```

### Other methods

See: [https://github.com/cheshirekow/cmake_format](https://github.com/cheshirekow/cmake_format)

## Running

```bash
find . -name '*.cmake' -o -name 'CMakeLists.txt' | xargs cmake-format -i
```

# Formatting flake.nix

## Installing nixfmt

```bash
nix-shell -p nixfmt-rfc-style
```

## Running

```bash
nixfmt flake.nix
```
