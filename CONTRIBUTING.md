# Contributing to Foldcessing

Thank you for your interest in contributing to Foldcessing! This document provides guidelines and information for contributors.

## How to Contribute

### Reporting Bugs

When reporting bugs, please include:

1. **Foldcessing version** (check source code header or binary size)
2. **Operating system** (Windows version)
3. **Processing version** (e.g., Processing 3.4)
4. **Steps to reproduce** the issue
5. **Expected behavior** vs **actual behavior**
6. **Error messages** (full output if possible)
7. **Example project structure** (if relevant)

Create an issue on GitHub with the "bug" label.

### Suggesting Features

Feature requests are welcome! Please:

1. Check existing issues to avoid duplicates
2. Describe the feature and **why it would be useful**
3. Provide example use cases
4. Consider backward compatibility

Create an issue with the "enhancement" label.

### Submitting Code

#### Before You Start

1. **Open an issue** to discuss major changes before implementing
2. **Check existing pull requests** to avoid duplicate work
3. **Ensure you can build** the project (see BUILD.md)

#### Development Setup

1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR-USERNAME/foldcessing.git
   cd foldcessing
   ```
3. Build the project:
   ```bash
   tcc -o foldcessing.exe foldcessing.c -luser32
   ```
4. Test with the example sketch:
   ```bash
   cd example_sketch
   ../foldcessing.exe --profile oni --run
   ```

#### Code Style

- **C99 standard**: Keep code compatible with C99
- **Windows API**: Use Win32 API for Windows-specific functionality
- **Indentation**: 4 spaces (no tabs)
- **Line length**: Prefer <100 characters, max 120
- **Comments**: Use `//` for single-line, `/* */` for multi-line
- **Naming**:
  - Functions: `snake_case` (e.g., `translate_line`)
  - Structs: `PascalCase` (e.g., `FileEntry`)
  - Constants: `UPPER_CASE` (e.g., `MAX_FILES`)
  - Variables: `snake_case` (e.g., `file_count`)

#### Testing Checklist

Before submitting, test:

- [ ] **Compilation**: Builds with TCC, GCC, and MSVC (if available)
- [ ] **Basic folding**: Concatenates files correctly
- [ ] **Line translation**: Errors map to correct source files
- [ ] **Console mode**: Works when run from terminal
- [ ] **GUI mode**: Shows MessageBox errors when double-clicked
- [ ] **Config parsing**: .foldcessing file loads correctly
- [ ] **Profiles**: Profile switching works
- [ ] **Ignore patterns**: Wildcard patterns exclude files
- [ ] **Large projects**: Test with >65K lines (if changing line translation)
- [ ] **Edge cases**: Empty files, missing directories, invalid paths

#### Pull Request Process

1. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes**:
   - Write clear, focused commits
   - Add comments for complex logic
   - Update documentation if needed

3. **Test thoroughly** (see checklist above)

4. **Update documentation**:
   - README.md (if user-facing changes)
   - BUILD.md (if build process changes)
   - Add inline comments for complex code

5. **Commit with clear messages**:
   ```bash
   git commit -m "Add feature: brief description

   Longer explanation of what changed and why.
   Fixes #123"
   ```

6. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

7. **Open a Pull Request**:
   - Describe the changes clearly
   - Reference related issues
   - Explain testing performed

## Code Architecture

### Key Components

1. **Config Parsing** (`parse_config`):
   - Reads `.foldcessing` INI-style config
   - Handles profiles and defaults
   - Parses ignore patterns

2. **File Collection** (`collect_files`):
   - Recursively scans directories
   - Applies ignore patterns
   - Sorts files alphabetically (depth-first)

3. **File Concatenation**:
   - Writes header comments (`//>/>/>/filename`)
   - Builds line mapping table
   - Tracks total line count

4. **Line Translation** (`translate_line`):
   - Handles Java's 16-bit line number limitation
   - Maps output.pde lines to source files
   - Reports ambiguous matches

5. **Process Spawning**:
   - Launches processing-java as child process
   - Captures stdout/stderr in real-time
   - Translates error messages on-the-fly

### Important Globals

- `files[]`: Array of discovered .pde files
- `line_map[]`: Mapping of output.pde lines to source files
- `file_count`: Number of discovered files
- `total_lines`: Total lines in concatenated output
- `config`: Parsed configuration

### Console Detection

Uses `FreeConsole()` + `AttachConsole(ATTACH_PARENT_PROCESS)` to detect if running from console vs double-clicked. Functions are loaded dynamically for TCC compatibility.

## Areas for Contribution

### High Priority

- **POSIX/Linux support**: Port to Linux/macOS
- **Better error messages**: More helpful diagnostics
- **Performance**: Optimize for very large projects (>10K files)

### Medium Priority

- **More IDE integrations**: IntelliJ, Atom, etc.

### Documentation

- **Video tutorial**: Screen recording of setup and usage
- **Troubleshooting guide**: Common issues and solutions

## Questions?

Feel free to open an issue with the "question" label or join the discussions section.

## License

By contributing, you agree that your contributions will be licensed under the GNU General Public License v3.0.

---

Thank you for helping make Foldcessing better!
