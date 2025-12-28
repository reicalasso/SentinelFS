# Contributing to SentinelFS

Thank you for your interest in contributing to SentinelFS! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [How to Contribute](#how-to-contribute)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Pull Request Process](#pull-request-process)
- [Issue Reporting](#issue-reporting)

---

## Code of Conduct

We are committed to providing a welcoming and inclusive environment. All contributors are expected to:

- Be respectful and considerate
- Accept constructive criticism gracefully
- Focus on what is best for the community
- Show empathy towards other community members

## Getting Started

### Prerequisites

Before contributing, ensure you have:

- Git installed and configured
- C++ compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.15 or higher
- Node.js 16+ and npm
- Required libraries: Boost, OpenSSL, SQLite3, ONNX Runtime

### Fork and Clone

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/SentinelFS.git
   cd SentinelFS
   ```
3. Add upstream remote:
   ```bash
   git remote add upstream https://github.com/reicalasso/SentinelFS.git
   ```

## Development Setup

### Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git \
    libboost-all-dev libssl-dev libsqlite3-dev \
    onnxruntime-dev \
    nodejs npm
```

**Fedora/RHEL:**
```bash
sudo dnf install -y \
    gcc-c++ cmake git \
    boost-devel openssl-devel sqlite-devel \
    onnxruntime-devel \
    nodejs npm
```

**macOS:**
```bash
brew install cmake boost openssl sqlite3 onnxruntime node
```

### Build the Project

```bash
cd SentinelFS
./scripts/install_deps.sh
./scripts/start_safe.sh --rebuild
```

### Run Tests

```bash
cd build
ctest --output-on-failure
```

## How to Contribute

### Types of Contributions

We welcome various types of contributions:

1. **Bug Fixes**: Fix issues reported in GitHub Issues
2. **New Features**: Implement new functionality
3. **Documentation**: Improve or add documentation
4. **Tests**: Add or improve test coverage
5. **Performance**: Optimize existing code
6. **Refactoring**: Improve code quality

### Contribution Workflow

1. **Check existing issues** to avoid duplicate work
2. **Create an issue** if one doesn't exist for your contribution
3. **Discuss your approach** in the issue before starting major work
4. **Create a feature branch** from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```
5. **Make your changes** following our coding standards
6. **Write tests** for new functionality
7. **Commit your changes** with clear messages
8. **Push to your fork** and create a Pull Request

## Coding Standards

### C++ Guidelines

- Follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- Use C++17/C++20 features appropriately
- Prefer smart pointers over raw pointers
- Use RAII for resource management
- Keep functions small and focused
- Add comments for complex logic

**Example:**
```cpp
// Good
std::unique_ptr<Connection> createConnection(const std::string& address) {
    auto conn = std::make_unique<Connection>();
    if (!conn->connect(address)) {
        throw ConnectionError("Failed to connect to " + address);
    }
    return conn;
}

// Bad
Connection* createConnection(const std::string& address) {
    Connection* conn = new Connection();
    conn->connect(address);
    return conn;
}
```

### TypeScript Guidelines

- Use TypeScript strict mode
- Follow ESLint configuration
- Use functional components with hooks in React
- Prefer `const` over `let`, avoid `var`
- Use meaningful variable names

**Example:**
```typescript
// Good
const fetchPeerStatus = async (peerId: string): Promise<PeerStatus> => {
    const response = await api.get(`/peers/${peerId}`);
    return response.data;
};

// Bad
function fetchPeerStatus(peerId) {
    return api.get('/peers/' + peerId).then(r => r.data);
}
```

### Code Formatting

- **C++**: Use `clang-format` with provided `.clang-format` file
  ```bash
  clang-format -i path/to/file.cpp
  ```
- **TypeScript**: Use `prettier` with provided configuration
  ```bash
  npx prettier --write path/to/file.ts
  ```

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Classes | PascalCase | `DeltaEngine`, `NetworkManager` |
| Functions | camelCase | `calculateChecksum()`, `sendData()` |
| Variables | camelCase | `peerCount`, `syncEnabled` |
| Constants | UPPER_SNAKE_CASE | `MAX_BUFFER_SIZE`, `DEFAULT_PORT` |
| Files | snake_case | `delta_engine.cpp`, `network_manager.h` |

## Testing Guidelines

### Writing Tests

- Write unit tests for all new functionality
- Ensure tests are deterministic and isolated
- Use descriptive test names
- Test edge cases and error conditions

**Example C++ Test:**
```cpp
TEST(DeltaEngineTest, CalculatesChecksumCorrectly) {
    DeltaEngine engine;
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    
    uint32_t checksum = engine.calculateChecksum(data);
    
    EXPECT_NE(checksum, 0);
    EXPECT_EQ(checksum, engine.calculateChecksum(data)); // Deterministic
}
```

### Running Tests

```bash
# Run all tests
cd build
ctest --output-on-failure

# Run specific test
./tests/test_delta_engine

# Run with verbose output
ctest -V
```

### Test Coverage

- Aim for at least 80% code coverage for new code
- Critical paths should have 100% coverage
- Use coverage tools to identify gaps:
  ```bash
  cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
  make coverage
  ```

## Pull Request Process

### Before Submitting

1. **Update your branch** with latest upstream changes:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Run all tests** and ensure they pass:
   ```bash
   cd build && ctest
   ```

3. **Format your code**:
   ```bash
   # C++
   find . -name "*.cpp" -o -name "*.h" | xargs clang-format -i
   
   # TypeScript
   cd gui && npx prettier --write "src/**/*.{ts,tsx}"
   ```

4. **Update documentation** if needed

### Commit Messages

Write clear, descriptive commit messages following this format:

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Maintenance tasks

**Example:**
```
feat(network): add QUIC transport support

Implement QUIC protocol support in NetFalcon plugin for improved
performance over high-latency networks. Includes automatic fallback
to TCP when QUIC is unavailable.

Closes #123
```

### Pull Request Template

When creating a PR, include:

- **Description**: What does this PR do?
- **Motivation**: Why is this change needed?
- **Testing**: How was this tested?
- **Screenshots**: If UI changes, include before/after screenshots
- **Checklist**:
  - [ ] Tests pass locally
  - [ ] Code follows style guidelines
  - [ ] Documentation updated
  - [ ] No breaking changes (or documented)

### Review Process

1. Maintainers will review your PR within 1-2 weeks
2. Address feedback by pushing new commits
3. Once approved, a maintainer will merge your PR
4. Your contribution will be included in the next release

## Issue Reporting

### Bug Reports

When reporting bugs, include:

- **Description**: Clear description of the issue
- **Steps to Reproduce**: Detailed steps to reproduce the bug
- **Expected Behavior**: What should happen
- **Actual Behavior**: What actually happens
- **Environment**:
  - OS and version
  - SentinelFS version
  - Relevant logs
- **Screenshots**: If applicable

**Example:**
```markdown
## Bug Description
Daemon crashes when connecting to peer with invalid session code

## Steps to Reproduce
1. Start daemon with session code "TEST-1234"
2. Attempt to connect to peer with session code "TEST-5678"
3. Daemon crashes with segmentation fault

## Expected Behavior
Daemon should reject connection with error message

## Actual Behavior
Daemon crashes with SIGSEGV

## Environment
- OS: Ubuntu 22.04
- SentinelFS: v1.0.0
- Logs: [attach daemon.log]
```

### Feature Requests

When requesting features, include:

- **Use Case**: Why is this feature needed?
- **Proposed Solution**: How should it work?
- **Alternatives**: Other approaches considered
- **Additional Context**: Any other relevant information

## Questions?

If you have questions about contributing:

- Check existing [documentation](docs/)
- Search [GitHub Issues](https://github.com/reicalasso/SentinelFS/issues)
- Ask in [GitHub Discussions](https://github.com/reicalasso/SentinelFS/discussions)

---

Thank you for contributing to SentinelFS! 🛡️
