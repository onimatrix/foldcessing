# Building Foldcessing

Foldcessing is a single C99 source file with minimal dependencies.
It only requires a C compiler and the Windows SDK (for Win32 API functions).

## Requirements

- **C Compiler**: TCC, GCC (MinGW), MSVC, or Clang
- **Platform**: Windows (uses Win32 API)
- **Libraries**: `user32.lib` (for MessageBox)

## Quick Build

### Using TCC (Tiny C Compiler)

TCC is the fastest option for development:

```bash
tcc -o foldcessing.exe foldcessing.c -luser32
```

Download TCC: https://bellard.org/tcc/

### Using GCC (MinGW)

For optimized release builds:

```bash
gcc -o foldcessing.exe foldcessing.c -luser32 -O2 -s
```

Flags:
- `-O2`: Optimization level 2
- `-s`: Strip debug symbols (smaller executable)

### Using MSVC

From Visual Studio Developer Command Prompt:

```bash
cl /Fe:foldcessing.exe foldcessing.c /link user32.lib
```

For optimized build:
```bash
cl /O2 /Fe:foldcessing.exe foldcessing.c /link user32.lib
```

### Using Clang

```bash
clang -o foldcessing.exe foldcessing.c -luser32 -O2
```

## Build Output

- **Executable**: `foldcessing.exe`
- **Dependencies**: None (statically linked)

## Compiler-Specific Notes

### TCC

- **Pros**: Extremely fast compilation (~0.1 seconds)
- **Cons**: Larger executable size, less optimization
- **Best for**: Development and testing
- Uses dynamic function loading for `AttachConsole`/`FreeConsole` for compatibility

### GCC/MinGW

- **Pros**: Good optimization, portable
- **Cons**: Slightly slower compilation
- **Best for**: Release builds
- Add `-static` flag for fully static linking if needed

### MSVC

- **Pros**: Best Windows integration, excellent debugging
- **Cons**: Requires Visual Studio installation
- **Best for**: Professional development

## Development Build vs Release Build

### Development Build (Fast compilation)
```bash
tcc -o foldcessing.exe foldcessing.c -luser32
```

### Release Build (Optimized)
```bash
gcc -o foldcessing.exe foldcessing.c -luser32 -O2 -s -DNDEBUG
```

Flags:
- `-O2` or `-O3`: Enable optimizations
- `-s`: Strip symbols (reduce size)
- `-DNDEBUG`: Disable assertions (if any)

## Troubleshooting

### "undefined reference to MessageBox"

Add `-luser32` to link against the User32 library:
```bash
gcc -o foldcessing.exe foldcessing.c -luser32
```

### "undefined reference to AttachConsole"

This function is dynamically loaded at runtime for TCC compatibility. Ensure you're using the latest version of the code.

### Large executable size with TCC

TCC produces larger executables (~420KB) compared to GCC with optimization (~200KB). This is normal and doesn't affect functionality.

### Compilation errors with older compilers

Foldcessing uses C99 features. Ensure your compiler supports C99:
- GCC: Use `-std=c99` or later
- MSVC: Use `/std:c11` or `/std:c17`

## Testing the Build

After compilation, test basic functionality:

```bash
# Test help output
foldcessing.exe

# Test with example sketch (requires processing-java)
cd example_sketch
foldcessing.exe --profile oni --run
```

## Cross-Compilation

Currently, Foldcessing is Windows-only due to Win32 API dependencies. Future versions may support POSIX systems.

## Performance Notes

- Compilation time: <1 second with any compiler
- Runtime performance: Negligible overhead (file I/O bound)
- Memory usage: ~1-2MB for typical projects

## Contributing

When submitting code changes:
1. Ensure it compiles with TCC, GCC, and MSVC
2. Test with both console and GUI (double-click) execution
3. Verify line translation works correctly
4. Update this document if build process changes
