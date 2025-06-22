# Git Workflow & Code Review Guidelines

## 🌟 Branch Strategy

### Branch Types
```
main
├── feature/TODO-XXX-feature-name
├── bugfix/issue-description
├── hotfix/critical-fix
└── release/vX.Y.Z
```

### Branch Naming Convention
```bash
# Feature branches (for TODO items)
feature/TODO-001-module-heartbeat
feature/TODO-006-web-ui-framework

# Bug fixes
bugfix/sensor-reading-timeout
bugfix/memory-leak-in-eventbus

# Hot fixes for production
hotfix/critical-safety-issue

# Release branches
release/v1.0.0
release/v1.1.0-beta
```

## 📝 Commit Message Standards

### Conventional Commits Format
```
<type>[scope]: <description>

[optional body]

[optional footer(s)]
```

### Commit Types
- `feat`: New feature implementation
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring without functionality change
- `perf`: Performance improvements
- `test`: Adding or modifying tests
- `build`: Build system or dependency changes
- `ci`: CI/CD configuration changes
- `chore`: Maintenance tasks

### Commit Message Examples

#### Feature Implementation
```
feat(core): implement ModuleHeartbeat system

- Add ModuleHeartbeat class with watchdog functionality
- Integrate with ModuleManager for automatic monitoring
- Add configurable timeout and recovery actions
- Include unit tests with 95% coverage

Resolves: TODO-001
Performance: Adds <1% CPU overhead
Memory: Uses 256B static RAM
```

#### Bug Fix
```
fix(sensors): resolve DS18B20 timeout during bulk read

The bulk read operation was timing out when reading from
multiple sensors simultaneously. Fixed by:
- Increasing OneWire timeout to 750ms
- Adding retry logic with exponential backoff
- Improving error handling and logging

Fixes: #45
Tested: 8-sensor configuration for 24 hours
```

#### Documentation Update
```
docs(api): update SensorModule API documentation

- Add missing parameter descriptions
- Include usage examples for all public methods
- Fix incorrect return type documentation
- Add thread safety notes

No functional changes.
```

#### Performance Improvement
```
perf(memory): optimize EventBus memory allocation

- Replace dynamic allocation with memory pool
- Reduce heap fragmentation by 60%
- Improve event processing speed by 25%

Benchmark results:
- Before: 450μs average event processing
- After: 340μs average event processing
- Memory savings: 2.3KB heap freed
```
### Multi-commit Guidelines

For TODO items that require multiple commits:

```bash
# First commit - basic implementation
git commit -m "feat(core): TODO-001 basic ModuleHeartbeat implementation

- Add ModuleHeartbeat class skeleton
- Implement basic registration and monitoring
- Add initial unit tests"

# Second commit - integration
git commit -m "feat(core): TODO-001 integrate heartbeat with ModuleManager

- Connect heartbeat system to existing modules
- Add configuration options via Kconfig
- Update module lifecycle management"

# Final commit - completion
git commit -m "feat(core): TODO-001 complete ModuleHeartbeat system

- Add recovery actions and escalation
- Complete test coverage (95%)
- Add performance monitoring
- Update documentation

Closes: TODO-001"
```

## 🔄 Workflow Process

### 1. Starting New Work
```bash
# Start from latest main
git checkout main
git pull origin main

# Create feature branch
git checkout -b feature/TODO-XXX-description

# Make initial commit
git commit -m "feat(module): TODO-XXX initial implementation"
```

### 2. Development Cycle
```bash
# Regular commits during development
git add .
git commit -m "feat(module): add component X functionality"

# Push to remote regularly
git push origin feature/TODO-XXX-description
```

### 3. AI-Assisted Development Commits
```bash
# When working with AI assistant
git commit -m "feat(core): TODO-001 AI-generated heartbeat class

Co-authored-by: AI Assistant <ai@assistant.com>

- Generated ModuleHeartbeat class structure
- Implemented watchdog timer logic
- Added error recovery mechanisms

Human validation: Hardware testing completed
AI contribution: Algorithm design and implementation"
```

### 4. Completion and Merge
```bash
# Final cleanup
git rebase -i main  # Clean up commit history if needed

# Create pull request with template
gh pr create --template pull_request_template.md

# After review and approval
git checkout main
git merge feature/TODO-XXX-description --no-ff
git tag -a TODO-XXX-completed -m "Completed TODO-XXX: Feature description"
git push origin main --tags
```

## ✅ Code Review Checklist

### Pre-Review Checklist (Author)
- [ ] **Build**: Code compiles without warnings
- [ ] **Tests**: All unit tests pass
- [ ] **Memory**: No memory leaks detected
- [ ] **Performance**: Meets timing requirements
- [ ] **Documentation**: API docs updated
- [ ] **TODO Progress**: Updates TODO status

### Code Quality Review

#### Architecture & Design
- [ ] **Modularity**: Changes follow modular architecture
- [ ] **SOLID Principles**: Code follows SOLID design principles
- [ ] **Interfaces**: Uses existing HAL interfaces correctly
- [ ] **Dependencies**: Minimal coupling between modules
- [ ] **Error Handling**: Proper error handling and recovery

#### Implementation Quality
- [ ] **Naming**: Clear, descriptive names for variables and functions
- [ ] **Comments**: Complex logic is well documented
- [ ] **Code Style**: Follows project coding standards
- [ ] **Resource Management**: Proper RAII and resource cleanup
- [ ] **Thread Safety**: Correct synchronization where needed

#### ESP32 Specific
- [ ] **Memory Usage**: Static allocation preferred
- [ ] **Task Priority**: Appropriate task priorities used
- [ ] **Core Assignment**: Tasks assigned to correct cores
- [ ] **Stack Size**: Adequate stack allocation
- [ ] **Watchdog**: Critical sections don't block watchdog

#### Testing & Validation
- [ ] **Unit Tests**: Comprehensive test coverage
- [ ] **Integration**: Works with existing modules
- [ ] **Edge Cases**: Error conditions properly tested
- [ ] **Hardware**: Tested on actual ESP32 hardware
- [ ] **Performance**: Performance requirements met

#### Security & Safety
- [ ] **Input Validation**: All inputs properly validated
- [ ] **Buffer Overflow**: No buffer overflow risks
- [ ] **Safety Critical**: Safety requirements addressed
- [ ] **Secrets**: No hardcoded secrets or credentials

### AI-Generated Code Review

#### Additional Checks for AI Code
- [ ] **Algorithm Correctness**: Verify mathematical correctness
- [ ] **Domain Knowledge**: Check refrigeration domain accuracy
- [ ] **ESP32 Constraints**: Ensure ESP32 limitations considered
- [ ] **Real-world Validation**: Test with actual hardware scenarios
- [ ] **Edge Cases**: Verify AI covered all edge cases

#### AI Collaboration Documentation
- [ ] **AI Contribution**: Clearly marked AI-generated sections
- [ ] **Human Validation**: Document human verification steps
- [ ] **Modifications**: Note any changes made to AI code
- [ ] **Learning**: Document lessons learned for future AI sessions

## 🚨 Merge Requirements

### Automated Checks (CI/CD)
- ✅ Build successful (ESP32 target)
- ✅ Unit tests pass (>90% coverage)
- ✅ Static analysis clean
- ✅ Memory usage within limits
- ✅ No lint warnings

### Manual Review Requirements
- ✅ Code review by human developer
- ✅ Architecture review (for major changes)
- ✅ Hardware testing completed
- ✅ Documentation updated
- ✅ TODO item marked complete

### Special Requirements

#### For Safety-Critical Code
- ✅ Safety review by domain expert
- ✅ Failure mode analysis completed
- ✅ Emergency stop testing
- ✅ Compliance documentation

#### For AI-Generated Code
- ✅ Human validation of algorithms
- ✅ Real-world testing completed
- ✅ Performance benchmarking
- ✅ Edge case verification

## 📊 Review Templates

### Pull Request Template
```markdown
## TODO Item
Closes: TODO-XXX

## Summary
Brief description of changes implemented.

## AI Collaboration
- [ ] AI-assisted development used
- [ ] Human validation completed
- [ ] Algorithm correctness verified

## Type of Change
- [ ] New feature
- [ ] Bug fix
- [ ] Performance improvement
- [ ] Documentation update

## Testing
- [ ] Unit tests added/updated
- [ ] Integration tests pass
- [ ] Hardware testing completed
- [ ] Performance benchmarks run

## Review Focus Areas
Please pay special attention to:
- Algorithm correctness in [specific area]
- Memory usage in [specific component]
- Thread safety in [specific function]

## Performance Impact
- Memory usage: +/- X KB
- CPU usage: +/- X%
- Response time: +/- X ms

## Screenshots/Logs
[Include relevant screenshots or log outputs]
```

### Code Review Response Template
```markdown
## Review Summary
Overall assessment: ✅ Approve / ⚠️ Needs Changes / ❌ Request Changes

## Strengths
- Well-structured implementation
- Good test coverage
- Clear documentation

## Areas for Improvement
1. **Performance**: Consider optimizing loop in function X
2. **Memory**: Potential memory leak in resource cleanup
3. **Testing**: Add edge case test for condition Y

## Detailed Comments
[Line-by-line comments provided in GitHub]

## Action Items
- [ ] Fix memory leak in cleanup function
- [ ] Add unit test for edge case
- [ ] Update API documentation

## Questions
1. Why was approach X chosen over approach Y?
2. Have you considered the impact on power consumption?
```

This workflow ensures high-quality, traceable development with clear AI-human collaboration documentation.