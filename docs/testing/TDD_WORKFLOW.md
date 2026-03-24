# TDD Workflow for MUD Development

## Overview

This guide explains how to adopt Test-Driven Development (TDD) in Sentience MUD development. This is a workflow guide focused on the process and philosophy of writing tests first, complementing the mechanical instructions in WRITING_TESTS.md.

## 1. Why TDD for MUD Development

MUD systems are highly interconnected ecosystems where changes in one area can have far-reaching effects. A new spell might interact unexpectedly with existing combat mechanics. A command enhancement might break scripting systems. A data structure change might corrupt player saves.

TDD provides three critical benefits for MUD development:

**Regression Safety:** Every change is protected by tests that verify existing behavior continues to work. This is essential when modifying core systems like combat, magic, or player data.

**API Design Force:** Writing tests first forces you to think about how your code will be used before implementing it. This leads to cleaner, more intuitive interfaces for commands, spells, and game systems.

**Confident Refactoring:** With comprehensive tests, you can safely restructure code, optimize performance, or modernize legacy systems without fear of breaking player experience.

**JSON-Driven Advantage:** Our framework's JSON test definitions make it particularly easy to define expected behavior before writing C code. The JSON format forces you to think through inputs, outputs, and edge cases systematically.

## 2. The Red-Green-Refactor Cycle

The classic TDD cycle adapted to our JSON-driven testing framework:

### Red Phase: Make It Fail

1. **Write JSON Test Definition:** Create a test JSON file with `input` and `expected_output` that describes the behavior you want to implement.

2. **Write Test Handler (if needed):** If this is a new test type, write the C handler function that interprets your JSON and executes the test logic.

3. **Build Test Suite:** Run `./build tests` to compile the test framework with your new test.

4. **Run the Test:** Execute `./sent -test:your_pattern` from `/sentience/`. The test should fail (red) because you haven't implemented the feature yet.

### Green Phase: Make It Pass

1. **Write Minimal Implementation:** Write just enough C code to make the test pass. Don't worry about optimization or elegance yet.

2. **Build and Test:** Run `./build tests` again, then `./sent -test:your_pattern`. The test should now pass (green).

3. **Verify Completeness:** Run the full test suite to ensure you didn't break anything else.

### Refactor Phase: Make It Clean

1. **Improve the Implementation:** Now that you have a working solution, refactor it for clarity, performance, and maintainability.

2. **Run Tests Again:** After each refactoring step, re-run your test to ensure it still passes (stays green).

3. **Run Full Suite:** Verify that your refactored code doesn't break any existing functionality.

## 3. TDD Patterns for Common MUD Tasks

### New Command Implementation

**Design First:** Write a JSON integration test that defines the complete command behavior:
```json
{
  "test_id": "test_whisper_basic",
  "input": [
    "whisper testplayer Hello there"
  ],
  "expected_output": [
    "You whisper to testplayer: Hello there",
    "@testplayer_output:You hear testplayer whisper: Hello there"
  ]
}
```

**Implementation Steps:**
1. Write integration test handler that creates fake players, executes the command, and checks output
2. Implement the command function in appropriate `act_*.c` file
3. Add the command entry to `cmd_table[]` in `interp.c`
4. Handle edge cases (player not found, no arguments, etc.)

### New Spell Implementation

**Design First:** Define spell parameters and behavior in JSON:
```json
{
  "test_id": "test_heal_light_spell",
  "input": [
    "cast heal light testplayer"
  ],
  "setup": {
    "caster_level": 10,
    "target_hp": 50,
    "target_max_hp": 100
  },
  "expected_output": [
    "You cast heal light on testplayer.",
    "@testplayer_hp_range:[60,75]"
  ]
}
```

**Implementation Steps:**
1. Write test that validates spell function pointer in spell tables
2. Test damage/healing calculation formulas
3. Test affect application and duration
4. Implement in appropriate `magic_*.c` file

### New Game System

**Design First:** Break complex systems into data model and behavior tests:
```json
{
  "test_id": "test_reputation_load_data",
  "input": ["load_reputation_data"],
  "expected_output": ["reputation_data_loaded:true"]
}
```

**Implementation Steps:**
1. Write data model tests (loading, lookup, integrity)
2. Write behavior tests (state transitions, interactions)
3. Implement the system module step by step
4. Integration test with existing systems

### New Data Type

**Design First:** Define JSON serialization and lookup behavior:
```json
{
  "test_id": "test_race_roundtrip",
  "input": ["save_and_reload_race:human"],
  "expected_output": ["race_data_intact:human"]
}
```

**Implementation Steps:**
1. Write JSON serialization roundtrip test
2. Test loading and lookup by various keys
3. Test integrity constraints and validation
4. Implement data type handlers

## 4. When to Write Unit vs Integration Tests

### Unit Tests
Use for **pure functions** that:
- Take only primitive types or const pointers
- Don't access global MUD state
- Don't require loaded areas or game environment
- Have deterministic outputs

Examples: string utilities, math calculations, data parsing functions.

### Integration Tests
Use for **MUD-aware functions** that:
- Require loaded areas or game state
- Interact with players, rooms, or objects
- Use the command interpreter
- Modify global state

Examples: commands, spells, social interactions, area loading.

**Rule of Thumb:** If your function needs `world_list`, `player_list`, or mock zones to test properly, it's an integration test.

## 5. Working with JSON Test Definitions

### JSON as Specification

The JSON test files **ARE** the specification. Write them first as a design exercise. The `input`/`expected_output` structure forces you to think about:

- What parameters does this function need?
- What should the output look like?
- How should errors be handled?
- What edge cases exist?

### Design Benefits

Writing JSON first provides:

**Clear API Definition:** The test shows exactly how the feature will be used.

**Edge Case Discovery:** Writing expected outputs forces you to think through error conditions.

**Documentation:** The tests serve as executable examples of how the feature works.

**Regression Protection:** Once written, the tests protect against future breakage.

### Best Practices

- **Be Specific:** Use exact expected outputs rather than vague patterns when possible
- **Test Edge Cases:** Include tests for invalid inputs, missing data, boundary conditions
- **Use Descriptive IDs:** `test_whisper_player_not_found` is better than `test_whisper_2`
- **Group Related Tests:** Organize tests by feature or system for easier maintenance

## 6. Integration with the Build System

### Development Cycle

The TDD workflow integrates seamlessly with our build system:

1. **Write Test:** Create or modify JSON test definitions
2. **Build:** Run `./build tests` to compile test framework
3. **Test:** Run `./sent -test:your_pattern` to execute specific tests
4. **Implement:** Write C code to make tests pass
5. **Verify:** Run tests again to confirm green status
6. **Regression Check:** Run full test suite with `./sent -test:all`

### Rapid Feedback Loop

Use the development profile for fast iteration:
- Incremental builds are fast for small changes
- Targeted test execution with patterns like `-test:whisper` for focused testing
- Full suite runs for confidence before commits

### Integration Points

- **Commands:** Test via command interpreter integration
- **Data Loading:** Test area and configuration loading
- **Scripting:** Test script execution and state changes
- **Persistence:** Test save/load cycles and data integrity

### Debugging Failed Tests

When tests fail:

1. **Read the Failure Message:** Our framework provides detailed output about what was expected vs. actual
2. **Run Single Test:** Use specific patterns to isolate the failing test
3. **Add Debug Output:** Temporarily add logging to understand what's happening
4. **Check Test Logic:** Verify the test itself is correct
5. **Verify Environment:** Ensure test setup creates the expected initial state

## Best Practices Summary

1. **Start with JSON:** Write test definitions before any C code
2. **Think Small:** Write the simplest test that could possibly fail, then implement the simplest code to make it pass
3. **Red-Green-Refactor:** Always follow the three-phase cycle
4. **Test Edge Cases:** Don't just test the happy path
5. **Maintain Tests:** Update tests when requirements change
6. **Run Full Suite:** Verify no regressions before committing
7. **Use Descriptive Names:** Tests should clearly communicate what they verify

Remember: TDD is not about testing—it's about design. The tests are a byproduct of good design thinking. By writing tests first, you're forced to think about how your code will be used, leading to better APIs and more maintainable systems.