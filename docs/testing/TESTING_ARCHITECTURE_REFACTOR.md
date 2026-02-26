# Testing Architecture Refactoring Plan

## Current Issue

The test framework currently dispatches all test types through `wnum_tests.c`, which has become a monolithic dispatcher handling:

- WNUM parsing tests
- Area loading tests  
- Reserved entity tests
- System area tests
- JSON serialization tests
- Redis caching tests
- Reset tests
- Shop stock tests
- **Church tests** (newly added)
- **Instance/Blueprint tests** (newly added)
- **Chat room tests** (newly added)

This violates separation of concerns and makes the codebase harder to maintain.

## Proposed Architecture

### 1. Create Central Test Dispatcher

Create `src/tests/framework/test_dispatcher.c` to handle test routing:

```c
test_result_t dispatch_test(test_case_t *test) {
    if (!test || !test->test_type) {
        return TEST_ERROR;
    }
    
    // Pattern-based routing to specialized handlers
    if (strstr(test->test_type, "church_") != NULL) {
        return run_church_test_case(test);
    }
    else if (strstr(test->test_type, "instance_") != NULL ||
             strstr(test->test_type, "blueprint_") != NULL) {
        return run_instance_test_case(test);
    }
    // ... etc
}
```

### 2. Rename wnum_tests.c

Rename `wnum_tests.c` to `core_tests.c` or `area_tests.c` to better reflect its actual scope:
- WNUM parsing
- Area loading and integrity
- UID uniqueness
- Cross-area references

### 3. Modularize Test Handlers

Each test domain gets its own file:
- `src/tests/integration/area_tests.c` - Area loading, WNUM parsing
- `src/tests/integration/church_tests.c` ✓ (already done)
- `src/tests/integration/instance_tests.c` ✓ (already done)
- `src/tests/integration/chat_rooms_tests.c` ✓ (already done)
- `src/tests/integration/reset_tests.c` ✓ (already exists)
- `src/tests/integration/shop_stock_tests.c` ✓ (already exists)
- `src/tests/integration/redis_tests.c` (split from area_tests)
- `src/tests/integration/json_tests.c` (JSON serialization)
- `src/tests/integration/reserved_tests.c` (split from area_tests)

### 4. Update test_framework.h

Add dispatcher registration:

```c
// Test dispatcher registration
typedef test_result_t (*test_dispatcher_func)(test_case_t *test);

void register_test_dispatcher(const char *pattern, test_dispatcher_func func);
test_result_t dispatch_test_case(test_case_t *test);
```

### 5. Benefits

- **Maintainability**: Each test domain is self-contained
- **Scalability**: Easy to add new test types without modifying core dispatcher
- **Clarity**: Clear ownership of test implementations
- **Testability**: Test handlers can be unit tested independently

## Migration Path

1. Create `test_dispatcher.c` with initial routing logic
2. Move existing handlers out of `wnum_tests.c` into specialized files
3. Update `run_test_case()` to call `dispatch_test_case()`
4. Gradually refactor and clean up legacy code
5. Update documentation and examples

## Timeline

**Priority**: Medium  
**Effort**: 4-6 hours  
**Dependencies**: None (can be done incrementally)

## Current Status

As of February 2, 2026:
- ✓ Church, instance, and chat room test handlers implemented
- ✓ All handlers currently dispatched through `wnum_tests.c`
- ✗ Central dispatcher not yet created
- ✗ Test routing still monolithic

## Notes

This refactoring should be done **after** Phase 4 (widevnum migration) is complete to avoid disrupting the migration work. The current architecture works but needs improvement for long-term maintainability.
