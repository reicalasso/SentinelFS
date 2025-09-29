# 🤝 Contributing to SentinelFS

Thank you for your interest in contributing to SentinelFS! This document outlines how to get involved, the development process, and coding standards.

## 📋 Table of Contents

1. [Getting Started](#getting-started)
2. [Development Workflow](#development-workflow)
3. [Code Standards](#code-standards)
4. [Testing Guidelines](#testing-guidelines)
5. [Documentation Standards](#documentation-standards)
6. [Security Considerations](#security-considerations)
7. [Community Guidelines](#community-guidelines)

## 🚀 Getting Started

### Prerequisites

Before contributing, ensure you have:

- **Rust** (1.70+) for core development
- **Python** (3.8+) for AI components
- **Docker** and **Docker Compose** for containerization
- **Git** for version control
- **Cargo** (Rust package manager)
- **Python virtual environment** tools (venv, conda, etc.)

### Setting Up Development Environment

```bash
# Clone the repository
git clone https://github.com/reicalasso/SentinelFS.git
cd SentinelFS

# Create a feature branch
git checkout -b feature/your-feature-name

# Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source ~/.cargo/env

# Install Python dependencies (if working on AI components)
pip install -r requirements.txt

# Build the project
cargo build --workspace
```

### Project Structure

```
SentinelFS/
├── sentinel-fuse/          # FUSE interface (Rust)
├── sentinel-security/      # Security engine (Rust + YARA)
├── sentinel-ai/           # AI components (Python + PyTorch)
├── sentinel-net/          # Network manager (Rust + Tokio)
├── sentinel-db/          # Database layer (Rust + PostgreSQL)
├── sentinel-api/          # REST API (Rust + Axum)
├── sentinel-common/       # Shared utilities (Rust)
├── scripts/               # Utility scripts
├── infra/                 # Infrastructure configurations
├── docs/                  # Documentation
├── tests/                 # Test suites
├── .github/workflows/     # CI/CD configurations
└── README.md
```

## 🔄 Development Workflow

### Branch Strategy

- `main`: Production-ready code
- `develop`: Integration branch for features
- `feature/*`: Feature development branches
- `release/*`: Release preparation branches
- `hotfix/*`: Critical bug fixes

### Git Workflow

1. **Fork** the repository (if external contributor)
2. **Create** a feature branch: `git checkout -b feature/your-feature`
3. **Implement** your changes
4. **Test** your changes thoroughly
5. **Commit** with clear, descriptive messages
6. **Push** to your fork: `git push origin feature/your-feature`
7. **Submit** a pull request

### Commit Message Guidelines

Follow the conventional commits format:

```
<type>(<scope>): <short summary>
<BLANK LINE>
<body>
<BLANK LINE>
<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, missing semicolons, etc.)
- `refactor`: Code changes that neither fix a bug nor add a feature
- `perf`: Performance improvements
- `test`: Adding or modifying tests
- `chore`: Other changes that don't modify src or test files

**Example:**
```
feat(security): add YARA rule validation during file upload

- Implement YARA rule syntax validation
- Add unit tests for validation logic
- Update documentation with validation requirements
```

## 📐 Code Standards

### Rust Code Standards

#### Formatting

- Use `rustfmt` for consistent formatting
- Line length: 100 characters max
- Use 4 spaces for indentation
- Always use explicit types for public functions

```rust
// Good
pub fn validate_file_security(path: &str, content: &[u8]) -> Result<bool, SecurityError> {
    // implementation
}

// Avoid
fn validate_file(path: &str, content: &[u8]) -> Result<bool> {
    // implementation
}
```

#### Naming

- Use `snake_case` for functions and variables
- Use `PascalCase` for types and traits
- Use `SCREAMING_SNAKE_CASE` for constants
- Prefix private items with `_` if unused

#### Error Handling

```rust
// Use proper error types
#[derive(Debug)]
pub enum SecurityError {
    IoError(std::io::Error),
    YaraError(String),
    ThreatDetected(String),
}

impl From<std::io::Error> for SecurityError {
    fn from(error: std::io::Error) -> Self {
        SecurityError::IoError(error)
    }
}
```

#### Documentation

```rust
/// Validates security of a file by running YARA scan and entropy analysis
/// 
/// # Arguments
/// * `path` - Path to the file to validate
/// * `content` - File content as bytes
/// 
/// # Returns
/// * `Ok(true)` if file is clean, `Ok(false)` if threats detected
/// * `Err(SecurityError)` if validation fails
/// 
/// # Examples
/// ```
/// let result = validate_file_security("/tmp/test.txt", b"test content");
/// assert!(result.is_ok());
/// ```
pub fn validate_file_security(path: &str, content: &[u8]) -> Result<bool, SecurityError> {
    // implementation
}
```

### Python Code Standards

#### Formatting

- Use `black` for consistent formatting
- Line length: 88 characters max
- Use 4 spaces for indentation
- Follow PEP 8 guidelines

#### Type Hints

```python
from typing import List, Dict, Optional, Tuple
import numpy as np


def analyze_access_pattern(
    user_id: str,
    file_path: str,
    access_times: List[float]
) -> Dict[str, float]:
    """Analyze access patterns for anomaly detection."""
    # implementation
    pass
```

### Security Code Standards

#### Input Validation

- Always validate inputs before processing
- Use parameterized queries to prevent injection
- Implement proper sanitization

```rust
// Good - input validation
pub fn validate_file_path(path: &str) -> Result<(), ValidationError> {
    if path.contains("../") {
        return Err(ValidationError::InvalidPath("Path traversal detected".to_string()));
    }
    
    if path.len() > MAX_PATH_LENGTH {
        return Err(ValidationError::PathTooLong);
    }
    
    Ok(())
}
```

#### Secure Coding Practices

- Never log sensitive data
- Use constant-time comparison for cryptographic operations
- Implement proper access controls
- Validate file types and sizes

## 🧪 Testing Guidelines

### Test Structure

Organize tests following Rust conventions:

```rust
// File: src/security/scanner.rs
#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempdir::TempDir;

    #[test]
    fn test_clean_file_scan() {
        // Given
        let temp_dir = TempDir::new("test_scan").unwrap();
        let test_file = temp_dir.path().join("clean.txt");
        fs::write(&test_file, "clean content").unwrap();
        
        // When
        let scanner = SecurityScanner::new();
        let result = scanner.scan_file(&test_file).unwrap();
        
        // Then
        assert_eq!(result.threats.len(), 0);
        assert_eq!(result.status, ScanStatus::Clean);
    }

    #[test]
    fn test_yara_detection() {
        // Given
        let malicious_content = b"X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*\n";
        
        // When
        let scanner = SecurityScanner::new();
        let result = scanner.scan_content(malicious_content).unwrap();
        
        // Then
        assert!(!result.threats.is_empty());
        assert_eq!(result.status, ScanStatus::ThreatDetected);
    }
}
```

### Test Coverage

- Aim for 85%+ code coverage
- Critical security functions should have 95%+ coverage
- Write tests for edge cases and potential vulnerabilities

### Running Tests

```bash
# Run all tests with coverage
cargo test

# Run tests with specific package
cargo test -p sentinel-security

# Run tests with debugging
cargo test -- --nocapture

# Run benchmarks
cargo bench

# Run Python tests
python -m pytest tests/python/

# Run integration tests
cargo test --test integration
```

### Test Categories

- **Unit Tests**: Test individual functions/components
- **Integration Tests**: Test module interactions
- **Security Tests**: Test security controls and vulnerabilities
- **Performance Tests**: Test performance under load
- **Chaos Tests**: Test system resilience

## 📚 Documentation Standards

### Code Documentation

- Document all public functions, structs, and enums
- Use examples in documentation
- Keep documentation up-to-date with code changes
- Document security implications and potential vulnerabilities

### Architecture Documentation

- Update architecture diagrams when making significant changes
- Document new modules and their responsibilities
- Explain complex algorithms and security mechanisms

### User Documentation

- Update user guides for new features
- Add API documentation for new endpoints
- Include security best practices

## 🔐 Security Considerations

### Security Review Process

All contributions undergo security review:

1. **Automated scanning**: Code is scanned for known vulnerabilities
2. **Manual review**: Security team reviews changes
3. **Penetration testing**: New features are tested for vulnerabilities
4. **Threat modeling**: Security implications are analyzed

### Security Testing

When contributing security-related code:

- Include corresponding security tests
- Consider potential attack vectors
- Validate input sanitization
- Test edge cases and error conditions

### Responsible Disclosure

If you discover security vulnerabilities:

- Do not create public issues
- Contact security@sentielfs.io directly
- Follow responsible disclosure guidelines
- Allow time for fixes before public disclosure

## 🤝 Community Guidelines

### Code of Conduct

- Be respectful and inclusive
- Provide constructive feedback
- Respect different experience levels
- Focus on technical merit, not personal opinions
- Help others learn and grow

### Pull Request Process

1. **Create PR**: Submit your changes with clear description
2. **Automated Checks**: CI/CD runs all tests and checks
3. **Code Review**: Maintainers review code quality and functionality
4. **Security Review**: Security team reviews security aspects
5. **Merge**: After approvals, PR is merged to target branch

### Pull Request Requirements

- **Clear Description**: Explain what and why changes are needed
- **Tests Included**: Include appropriate tests for changes
- **Documentation Updated**: Update docs for user-facing changes
- **Passing Checks**: All CI/CD checks must pass
- **Security Considerations**: Address security implications

### Getting Help

- **Issues**: Use GitHub issues for bug reports and feature requests
- **Discussions**: Use GitHub discussions for questions and proposals
- **Direct Contact**: Reach out to maintainers for complex issues

## 📝 Issue Guidelines

### Creating Issues

When creating issues, please include:

- **Detailed Description**: What happened vs. expected behavior
- **Reproduction Steps**: Clear steps to reproduce the issue
- **Environment Info**: OS, version, configuration
- **Error Messages**: Complete error messages and logs
- **Security Issues**: Report security issues directly to security@sentinelfs.io

### Good First Issues

Looking for a place to start? Check out issues labeled `good-first-issue`:

- These are well-defined, manageable tasks
- Great for getting familiar with the codebase
- Maintainers are available to provide guidance

### Feature Requests

For feature requests, please provide:

- **Use Case**: Real-world scenario requiring the feature
- **Alternatives**: Other solutions considered
- **Benefits**: How the feature adds value
- **Security Implications**: How security is maintained

## 🏁 Final Checklist

Before submitting contributions, ensure you have:

- [ ] Written and run appropriate tests
- [ ] Updated documentation as needed
- [ ] Followed code style guidelines
- [ ] Run `cargo fmt` and `cargo clippy`
- [ ] Tested locally with realistic workloads
- [ ] Considered security implications
- [ ] Added appropriate logging and monitoring
- [ ] Followed Git commit conventions
- [ ] Verified CI/CD pipeline passes

Thank you for contributing to SentinelFS and helping make distributed storage more secure!