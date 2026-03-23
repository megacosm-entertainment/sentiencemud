# Sentience MUD Testing Framework

## Quick Start

Build the MUD with testing enabled:
```bash
cd /sentience/src
./build tests
```

Run all tests:
```bash
cd /sentience
./sent -test
```

Run development profile (fast tests):
```bash
cd /sentience
./sent -test:profile:development
```

Run tests by pattern:
```bash
cd /sentience
./sent -test:wnum              # All wnum-related tests
./sent -test:area              # All area tests
./sent -test:string            # All string tests
```

## Framework Overview

Sentience uses a JSON-driven testing framework where test definitions are stored as JSON files and dispatched to C test handlers. The framework has zero production overhead (protected by `#ifdef BUILD_TESTS`) and supports both unit and integration testing.

**Current Test Coverage:**
- **50 test files** (26 unit, 24 integration)  
- **31 integration suites** testing game systems
- **19 unit test suites** testing core utilities
- **305 total tests** across all suites

## Test Profiles

| Profile | Description | Command |
|---------|------------|---------|
| `development` | Fast tests for development workflow | `./sent -test:profile:development` |
| `ci` | Complete test suite for CI/CD | `./sent -test:profile:ci` |
| `bootstrap_ci` | Bootstrap-compatible suite for CI fixture data | `./sent -test:profile:bootstrap_ci` |
| `bootstrap_ci_redis` | Bootstrap CI plus Redis cache validation | `./sent -test:profile:bootstrap_ci_redis` |
| `nonbootstrap_full` | Comprehensive non-bootstrap regression suite | `./sent -test:profile:nonbootstrap_full` |
| `serialization_cycle` | Persistence and round-trip focused tests | `./sent -test:profile:serialization_cycle` |
| `regression` | Core regression tests | `./sent -test:profile:regression` |
| `combat_bench` | Combat telemetry baseline benchmarks | `./sent -test:profile:combat_bench` |

## Documentation Index

- **[TESTING_FRAMEWORK.md](TESTING_FRAMEWORK.md)** — Complete architecture and reference
- **[WRITING_TESTS.md](WRITING_TESTS.md)** — How to write tests (TDD-oriented)  
- **[TDD_WORKFLOW.md](TDD_WORKFLOW.md)** — The red-green-refactor process for MUD development
- **[COVERAGE_STATUS.md](COVERAGE_STATUS.md)** — What's tested and what's not

## Key Commands

| Command | Description |
|---------|-------------|
| `./sent -test` | Run all default tests |
| `./sent -test:profile:<name>` | Run specific test profile |
| `./sent -test:<pattern>` | Run tests matching pattern |
| `./sent -test:summary` | Show test suite counts only |
| `./sent -test:config` | Show configuration and available profiles |
| `./sent -test:registry` | Show all available test suites |

**Build from:** `/sentience/src` with `./build tests`  
**Run from:** `/sentience` (not `src/`) because the binary loads data from relative paths