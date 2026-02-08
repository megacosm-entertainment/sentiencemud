# Testing Strategy and Roadmap

## Overview

This document outlines the strategic testing approach for Sentience MUD, building on the existing JSON-driven integration test framework to expand into comprehensive unit and system testing.

## Current State

### Implemented: JSON-Driven Integration Framework ✅

- **Framework**: JSON-configured integration tests in `/sentience/src/tests/data/`
- **Coverage**: Area loading, WNUM parsing, database integrity
- **Memory Safety**: Basic framework with conditional compilation
- **Documentation**: Comprehensive developer guides in `/sentience/src/docs/testing/`

### Current Test Results
- 6 total tests implemented
- 5 tests passing consistently
- 1 test with intermittent issues
- Full MUD environment integration working

## Strategic Goals

### 1. Prevent Regressions
Catch when working code breaks due to refactoring or changes.

**Example Case:** The `do_look` command had a bug in inventory iteration that went unnoticed for months. The manual iteration logic failed while the helper function `get_obj_carry_number()` worked correctly. This could have been caught with automated tests.

### 2. Enable Confident Refactoring
Provide developers with immediate feedback when changes break functionality.

### 3. Document Behavior
Tests serve as executable specifications of expected behavior.

### 4. Speed Development
Faster feedback cycle than manual in-game testing.

### 5. Code Quality
Force better separation of concerns and modularity through testability requirements.

## Testing Roadmap

### Phase 1: Foundation Hardening (Current → 2 weeks)

**Status**: ✅ COMPLETE
- [x] JSON-driven test framework operational
- [x] Conditional compilation with #ifdef BUILD_TESTS
- [x] Build system integration (CMake + Make)
- [x] Console output and logging infrastructure
- [x] Basic integration tests for core systems

**Next Steps (Priority Implementation)**:
- [ ] Memory testing integration (AddressSanitizer, Valgrind)
- [ ] Unit test framework extension for pure functions
- [ ] JSON schema expansion for unit test patterns

### Phase 2: Unit Testing Framework Extension (2 weeks → 2 months)

**Goal**: Extend JSON framework to support pure function unit testing with memory safety validation.

**Framework Extensions**:

1. **Pure Function Test Types** (HIGH PRIORITY)
   - Extend JSON test type handlers for unit testing patterns
   - Support functions with simple inputs/outputs (no MUD environment)
   - Memory allocation testing for string and object functions
   - Framework support for `requires_mud_environment: false`

2. **Unit Test JSON Schema** (HIGH PRIORITY)
   ```json
   {
     "test_suite": "string_utilities_unit_tests",
     "description": "Unit tests for string utility functions",
     "requires_mud_environment": false,
     "test_level": "unit",
     "tests": [
       {
         "name": "is_name_exact_match",
         "test_type": "pure_function_test",
         "target_function": "is_name",
         "input": {
           "arguments": ["TestPlayer", "testplayer test"]
         },
         "expected_output": {
           "return_value": true
         }
       }
     ]
   }
   ```

3. **Memory Testing Integration** (CRITICAL)
   - AddressSanitizer and Valgrind build integration
   - Memory leak detection for all unit tests
   - Extended build scripts: `./build tests-memory`, `./build tests-valgrind`
   - Memory testing result types in framework

**Priority Unit Test Areas**:

1. **String Management Functions** (HIGHEST PRIORITY)
   - `is_name()`, `is_prefix()`, `one_argument()`, `smash_tilde()`
   - `str_dup()`, `free_string()`, `alloc_string()` - with memory testing
   - Critical for all game functionality, memory safety essential

2. **Iterator System** (HIGH PRIORITY)
   - `iterator_start()`, `iterator_nextdata()`, `iterator_stop()`
   - LLIST operations with memory leak verification
   - Recently migrated, needs comprehensive testing

3. **Object Getter Functions** (HIGH PRIORITY)
   - `get_obj_carry_number()`, `get_obj_here()`, `get_obj_inv_only()`
   - `get_obj_wear_number()`, `get_obj_list_number()`
   - Used by all commands, need reliable unit tests

4. **Lookup and Validation Functions** (MEDIUM PRIORITY)
   - `skill_lookup()`, `flag_value()`, `item_lookup()`
   - `is_valid_name()`, `parse_gen_name()`, `is_number()`
   - Number parsing: `number_argument()`, `mult_argument()`

5. **Character Predicates** (MEDIUM PRIORITY)
   - `can_see_obj()`, `can_see()`, `can_see_room()`
   - `IS_NPC()`, `IS_IMMORTAL()`, `IS_AFFECTED()`
   - Guard conditions used throughout codebase

**Implementation Approach**:
- Extend existing JSON framework rather than introducing Criterion
- Add pure function test type handlers to current framework
- Implement minimal state setup for unit tests (no full MUD environment)
- Integrate memory testing at framework level for all unit tests
- Create unit test fixtures and helper functions for common patterns

**Success Metrics**:
- 100+ unit tests covering core infrastructure functions
- Zero memory leaks in all unit test scenarios  
- Unit tests execute in under 5 seconds for entire suite
- Framework supports both unit and integration testing seamlessly
- Tests catch function-level regressions before integration testing

### Phase 3: Command and Integration Testing (2-4 months)

**Goal**: Test user-facing commands and complex system interactions with realistic game state.

**Approach**:
- Extend JSON framework to support command simulation and multi-step scenarios
- Create comprehensive test fixtures for realistic player/room/object scenarios
- Build on unit test foundation to test command implementations
- Focus on integration of tested functions within command context

**Priority Areas**:
1. **Command Functions Built on Tested Units**:
   - `do_look`, `do_inventory`, `do_score` (using tested `get_obj_*` functions)
   - `do_get`, `do_drop`, `do_give`, `do_put` (using tested object manipulation)
   - Commands that exercise unit-tested string and predicate functions

2. **Object Manipulation Integration**:
   - Complete workflows: pickup → inventory → drop → room
   - Container operations: put objects in containers, get from containers
   - Wear/remove equipment cycles
   - Object movement between characters and locations

3. **Information Display Commands**:
   - Look command integration with unit-tested object getters
   - Inventory display with tested iteration and visibility functions
   - Character information display using tested predicates

**Framework Extensions**:
- Command test type handlers for simulating player input
- Game state fixtures with realistic object/character/room setups
- Multi-step test scenarios (command sequences)
- Output capture and validation for user-visible results
- Integration test dependencies on unit test success

### Phase 4: System Integration Testing (6+ months)

**Goal**: Test complete workflows and system interactions.

**Areas**:
- Player authentication and character loading
- Area persistence and dynamic loading
- Network protocol handling (Telnet, TLS, WebSocket)
- Combat system integration
- Scripting and event system
- Multi-character interactions

**Framework Extensions**:
- Multi-actor test scenarios
- Network simulation capabilities
- Time-based and event-driven test patterns
- Performance and load testing integration

## Testing Architecture Evolution

### Current: JSON-Driven Integration Framework ✅

```
/sentience/
├── src/tests/framework/           # Core test framework
├── src/tests/integration/         # Integration test handlers  
├── src/tests/data/                    # JSON test configurations
└── docs/testing/                  # Framework documentation
```

### Phase 2: Extended JSON Framework with Unit Testing

```
/sentience/
├── src/tests/
│   ├── framework/                 # Core framework (extended for unit tests)
│   │   ├── test_framework.h       # Extended with unit test support
│   │   ├── test_framework.c       # Pure function testing capabilities
│   │   ├── test_loader.c          # JSON loading for unit and integration
│   │   └── memory_testing.c       # AddressSanitizer/Valgrind integration
│   ├── unit/                      # Pure function test handlers
│   │   ├── string_function_tests.c    # String utility unit tests
│   │   ├── iterator_tests.c            # Iterator system unit tests  
│   │   ├── object_getter_tests.c       # Object getter unit tests
│   │   └── lookup_tests.c              # Lookup function unit tests
│   ├── integration/               # System integration handlers
│   │   └── wnum_tests.c           # Existing integration tests
│   └── fixtures/                  # Test data and setup utilities
│       ├── minimal_state.c        # Minimal MUD state for unit tests
│       ├── test_objects.c         # Test object creation/cleanup
│       └── memory_helpers.c       # Memory testing utilities
├── src/tests/data/
│   ├── unit/                      # Unit test JSON configurations
│   │   ├── string_utilities.json  # String function unit tests
│   │   ├── iterator_system.json   # Iterator unit tests
│   │   ├── object_getters.json    # Object getter unit tests
│   │   └── lookup_functions.json  # Lookup function unit tests
│   ├── integration/               # Integration test configurations
│   │   ├── area_loading_tests.json     # Existing integration tests
│   │   ├── wnum_parsing_tests.json     # Existing WNUM tests
│   │   └── database_integrity_tests.json # Existing DB tests
│   └── fixtures/                  # Shared test data
│       ├── test_strings.json      # Common string test cases
│       └── test_objects.json      # Standard test objects
└── docs/testing/                  # Expanded documentation
    ├── TESTING_FRAMEWORK.md       # Updated with unit testing
    ├── TESTING_QUICK_REFERENCE.md # Unit test examples
    ├── TESTING_ROADMAP.md         # This roadmap
    └── README.md                  # Documentation index
```

### Phase 3+: Comprehensive Test Ecosystem

```
/sentience/
├── src/tests/
│   ├── framework/                 # Core framework
│   ├── unit/                      # Unit tests
│   ├── integration/               # Integration tests
│   ├── system/                    # End-to-end system tests
│   ├── performance/               # Performance benchmarks
│   ├── fixtures/                  # Test data management
│   ├── mocks/                     # Mock and simulation layer
│   └── utilities/                 # Test helper functions
├── src/tests/data/
│   ├── unit/                      # Unit test configurations
│   ├── integration/               # Integration configurations
│   ├── system/                    # System test scenarios
│   ├── performance/               # Performance test definitions
│   └── fixtures/                  # Shared test data and scenarios
└── scripts/
    ├── test-runner.sh            # Automated test execution
    ├── memory-check.sh           # Memory testing automation
    ├── performance-baseline.sh   # Performance regression detection
    └── ci-integration.sh         # CI/CD pipeline integration
```

## Framework Technology Decisions

### JSON Framework Extension vs External Frameworks

**Decision**: Extend our existing JSON-driven framework for unit testing rather than adopting Criterion or other external frameworks.

**Rationale**:
- **Unified Developer Experience**: Single testing approach for all test types
- **Custom MUD Integration**: Easier integration with MUD-specific systems
- **Existing Investment**: Build on proven JSON configuration approach
- **Flexibility**: Can evolve framework to meet specific MUD testing needs
- **Consistency**: Same execution model, reporting, and documentation patterns

**Framework Capabilities After Extension**:
- Pure function unit testing through JSON configuration
- Integration testing with full MUD environment (existing)
- Memory testing integration (AddressSanitizer, Valgrind)
- Unified test execution and reporting
- Dependency management across unit and integration tests
- Flexible test fixtures and state management

### Memory Testing Integration Strategy

**AddressSanitizer for Development**:
```bash
# Fast memory testing during development
cd /sentience/src
./build tests-memory                    # Build with AddressSanitizer
cd /sentience
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 ./sent -test
```

**Valgrind for Comprehensive Analysis**:
```bash
# Thorough memory analysis for CI and releases
valgrind --tool=memcheck --leak-check=full ./sent -test
```

**Framework Integration**:
- Memory testing result types (MEMORY_LEAK, MEMORY_ERROR, etc.)
- Automatic memory analysis for all unit tests
- Memory testing configuration in JSON test files
- Build system integration for memory testing modes
- **Coverage analysis integration with gcov/lcov**
- **Automated coverage reporting and HTML generation**
- **Coverage regression detection in CI pipeline**

## CI/CD Integration Strategy

### Phase 1: Basic Automation
- Automated test execution on commit/PR
- Basic memory leak detection with AddressSanitizer
- Test result reporting in CI dashboard

### Phase 2: Comprehensive Testing
- Multi-platform testing (different compilers, OS versions)
- Valgrind integration for thorough memory analysis
- Performance regression detection
- Test coverage tracking and reporting

### Phase 3: Advanced Pipeline
- Automated performance benchmarking
- Load testing and stress testing
- Security testing integration
- Automated documentation generation from tests

## Implementation Priorities

### Immediate (Next 1-2 weeks)
1. **Memory Testing Setup**: Integrate AddressSanitizer and Valgrind into build system
2. **Coverage Analysis Integration**: Add gcov/lcov support to CMake and build scripts
3. **Unit Test Framework Design**: JSON schema and framework extensions for pure function testing
4. **Baseline Coverage Report**: Generate initial coverage metrics for current codebase
5. **String Function Unit Tests**: Start with `is_name()`, `str_dup()`, `one_argument()` as proof of concept

### Short Term (1 month)
1. **Core Unit Test Implementation**: String utilities, lookup functions, basic predicates
2. **Memory Safety Validation**: Comprehensive memory leak detection for all unit tests
3. **Coverage-Driven Development**: Use coverage analysis to guide unit test priorities
4. **Automated Coverage Reporting**: Integrate coverage generation into test execution
5. **Framework Documentation**: Update guides with unit testing patterns and coverage analysis

### Medium Term (2-3 months)  
1. **Iterator and Object Systems**: Unit tests for recently migrated iterator system and object getters
2. **Command Integration Testing**: Build on unit test foundation for command testing
3. **CI Integration**: Automated unit and integration testing with memory analysis
4. **Coverage Regression Detection**: Automated coverage tracking and regression alerts
5. **Coverage-Guided Test Prioritization**: Focus testing efforts on uncovered critical paths

### Long Term (6+ months)
1. **Complete Testing Pyramid**: Unit → Integration → System testing all through JSON framework
2. **Performance and Load Testing**: Framework extensions for performance regression detection
3. **Advanced Analytics**: Test effectiveness metrics, coverage analysis, and optimization

## Success Metrics

### Technical Metrics
- **Unit Test Coverage**: Target 90%+ line coverage for core infrastructure functions (string utilities, iterators, object getters)
- **Function Coverage**: Target 95%+ function coverage for critical systems (memory management, lookup functions, predicates)
- **Integration Test Coverage**: Target 70%+ line coverage for command and system interactions
- **Memory Safety**: Zero memory leaks in all unit and integration test scenarios
- **Performance**: Unit test execution under 5 seconds, full test suite under 30 seconds
- **Coverage Regression**: No coverage decrease below 85% for core functions
- **Reliability**: 99%+ test consistency across runs and environments

### Development Impact Metrics
- **Bug Detection**: Tests catch regressions before production
- **Development Speed**: Faster feedback cycle than manual testing
- **Confidence**: Developers comfortable with refactoring critical code
- **Documentation**: Tests serve as behavior specification

### Maintenance Metrics
- **Test Maintenance**: Time to update tests after code changes
- **False Positives**: Minimize tests that fail due to environmental issues
- **Adoption**: Percentage of new features developed with accompanying tests

## Resource Requirements

### Development Time
- **Phase 1**: 1-2 weeks (memory testing integration)
- **Phase 2**: 2 months (unit test framework extension and core function coverage)  
- **Phase 3**: 2-4 months (command and integration testing expansion)
- **Ongoing**: 15-25% additional development time for unit test creation and maintenance

### Infrastructure
- **CI/CD Resources**: Automated testing pipeline with coverage reporting
- **Development Tools**: AddressSanitizer, Valgrind, gcov/lcov for coverage analysis
- **Coverage Tools**: lcov, genhtml for HTML coverage reports, coverage badges for documentation
- **Documentation**: Expanded testing guides with coverage analysis examples

### Training and Adoption
- **Developer Education**: Testing patterns and best practices
- **Code Review**: Include test quality in review process
- **Cultural Shift**: Make testing part of development workflow

## Risk Mitigation

### Technical Risks
- **Performance Impact**: Monitor test execution time, optimize as needed
- **Maintenance Burden**: Focus on high-value tests, avoid over-testing
- **False Positives**: Invest in reliable test infrastructure and environment

### Organizational Risks
- **Developer Resistance**: Start small, demonstrate value, provide good tools
- **Resource Allocation**: Balance testing investment with feature development
- **Skill Gap**: Provide training and clear documentation

## Next Steps

1. **Review and Approve Strategy**: Validate JSON framework extension approach with development team
2. **Implement Memory Testing**: AddressSanitizer integration and build system updates this week
3. **Design Unit Test JSON Schema**: Define JSON structure for pure function testing
4. **Create Proof-of-Concept**: Implement 5-10 unit tests for string utilities as framework validation
5. **Plan Phase 2 Implementation**: Detailed planning for comprehensive unit test coverage

This roadmap evolves our successful JSON-driven integration test framework into a comprehensive testing ecosystem. By extending our existing framework rather than introducing new tools, we maintain consistency while adding powerful unit testing capabilities. The focus on memory testing ensures the reliability critical for C-based MUD development, while the testing pyramid approach provides comprehensive coverage from pure functions to complete system integration.