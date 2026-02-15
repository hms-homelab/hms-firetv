# Contributing to HMS FireTV

Thank you for your interest in contributing to HMS FireTV! This document provides guidelines for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Reporting Bugs](#reporting-bugs)
- [Feature Requests](#feature-requests)

## Code of Conduct

This project follows a simple code of conduct:

- Be respectful and considerate
- Focus on constructive feedback
- Help create a welcoming environment for all contributors

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/hms-firetv.git
   cd hms-firetv
   ```
3. **Add upstream remote**:
   ```bash
   git remote add upstream https://github.com/hms-homelab/hms-firetv.git
   ```

## Development Setup

### Prerequisites

- C++17 compiler (GCC 7+ or Clang 6+)
- CMake 3.16+
- PostgreSQL (for testing)
- MQTT broker (Mosquitto recommended for testing)

### Install Dependencies

**Debian/Ubuntu:**
```bash
sudo apt-get install -y build-essential cmake g++ git \
    libpqxx-dev libcurl4-openssl-dev libjsoncpp-dev \
    libpaho-mqtt-dev libpaho-mqttpp-dev
```

**Install Drogon:**
```bash
git clone https://github.com/drogonframework/drogon
cd drogon && git submodule update --init
mkdir build && cd build
cmake .. && make -j$(nproc) && sudo make install
```

### Build the Project

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### Run Tests

```bash
cd build
ctest --output-on-failure
```

## Making Changes

### Create a Branch

Always create a new branch for your changes:

```bash
git checkout -b feature/your-feature-name
# or
git checkout -b fix/bug-description
```

Branch naming conventions:
- `feature/` - New features
- `fix/` - Bug fixes
- `docs/` - Documentation updates
- `refactor/` - Code refactoring
- `test/` - Test improvements

### Keep Your Fork Updated

Before starting work, sync with upstream:

```bash
git fetch upstream
git checkout main
git merge upstream/main
git push origin main
```

## Coding Standards

### C++ Style Guide

- **Standard**: C++17
- **Formatting**: Follow existing code style (4 spaces for indentation)
- **Naming**:
  - Classes: `PascalCase` (e.g., `MQTTClient`)
  - Functions: `camelCase` (e.g., `publishState`)
  - Variables: `snake_case` (e.g., `device_id`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_RETRIES`)

### Code Quality

- Write clear, self-documenting code
- Add comments for complex logic
- Keep functions focused and small
- Avoid unnecessary complexity
- Handle errors appropriately

### Example

```cpp
// Good: Clear function with proper error handling
bool MQTTClient::connect(const std::string& broker_address,
                         const std::string& username,
                         const std::string& password) {
    try {
        // Connection logic here
        return true;
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTTClient] Connection failed: " << e.what() << std::endl;
        return false;
    }
}
```

## Testing

### Manual Testing

1. Set up test environment:
   ```bash
   cp .env.example .env
   # Edit .env with test credentials
   ```

2. Run the service:
   ```bash
   ./build/hms_firetv
   ```

3. Test health endpoint:
   ```bash
   curl http://localhost:8888/health
   ```

4. Test MQTT commands:
   ```bash
   mosquitto_pub -h localhost -t "maestro_hub/colada/test_device/volume_up" -m "PRESS"
   ```

### Integration Tests

If adding new features, ensure they work with:
- PostgreSQL database
- MQTT broker (Mosquitto/EMQX)
- Home Assistant MQTT Discovery

## Submitting Changes

### Commit Messages

Write clear, descriptive commit messages:

```
Short summary (50 chars or less)

More detailed explanation if needed. Wrap at 72 characters.
Explain what changed and why, not how.

- Bullet points are okay
- Use present tense ("Add feature" not "Added feature")
```

Examples:
```
Add volume control support for Fire TV devices

Fix MQTT reconnection issue on network interruption

Update README with Docker deployment instructions
```

### Push Your Changes

```bash
git push origin feature/your-feature-name
```

### Create a Pull Request

1. Go to your fork on GitHub
2. Click "New Pull Request"
3. Select your branch
4. Fill in the PR template:
   - **Description**: What does this PR do?
   - **Motivation**: Why is this change needed?
   - **Testing**: How was it tested?
   - **Screenshots**: If applicable

### PR Review Process

- Maintainers will review your PR
- Address any requested changes
- Once approved, your PR will be merged

## Reporting Bugs

### Before Submitting

1. Check existing issues
2. Verify the bug on the latest version
3. Collect relevant information:
   - HMS FireTV version
   - Operating system
   - Error messages/logs
   - Steps to reproduce

### Submit a Bug Report

Create an issue with:

**Title**: Clear, specific description

**Body**:
```
**Describe the bug**
A clear description of what the bug is.

**To Reproduce**
Steps to reproduce the behavior:
1. Configure '...'
2. Send MQTT message '...'
3. See error

**Expected behavior**
What should have happened.

**Logs**
```
Paste relevant logs here
```

**Environment**
- HMS FireTV version: [e.g., 1.0.0]
- OS: [e.g., Ubuntu 22.04]
- PostgreSQL version: [e.g., 15]
- MQTT broker: [e.g., Mosquitto 2.0]

**Additional context**
Any other information.
```

## Feature Requests

We welcome feature ideas! Submit an issue with:

**Title**: Feature: [Your feature name]

**Body**:
```
**Is your feature request related to a problem?**
A clear description of the problem.

**Describe the solution you'd like**
What you want to happen.

**Describe alternatives you've considered**
Other solutions or features you've considered.

**Additional context**
Mock-ups, examples, or other information.
```

## Questions?

- **GitHub Issues**: For bugs and feature requests
- **GitHub Discussions**: For questions and community support
- **Reddit**: [Original post](https://www.reddit.com/r/firetvstick/comments/1r534bl/local_script_to_control_multiple_fire_tv_devices/)

## Recognition

Contributors will be:
- Listed in release notes
- Mentioned in the README (for significant contributions)
- Appreciated by the community!

Thank you for contributing to HMS FireTV! 🎉
