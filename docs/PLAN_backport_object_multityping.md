# Backport Analysis: Object Multityping

This document provides an analysis of the Object Multityping system from the `src_20_dev` branch and outlines a plan for backporting it into the main `src` codebase.

---

## 2. Object Multityping

### 2.1. System Overview (`src_20_dev`)

The object multityping system is a major architectural refactor that moves object-specific data out of the overloaded `value[5]` integer array and into dedicated, type-specific structures.

- **Core Concept:**
    -   The generic `long value[MAX_OBJVALUES]` array in `OBJ_INDEX_DATA` and `OBJ_DATA` is deprecated.
    -   Instead, these structs contain a series of pointers to new, specific data structs (e.g., `ARMOR_DATA *_armor;`, `WEAPON_DATA *_weapon;`, `CONTAINER_DATA *_container;`).
    -   This makes the code more readable, self-documenting, and significantly easier to extend with new object types and properties.

- **Example (`WEAPON_DATA`):**
    -   Instead of `obj->value[1] = 10;` and `obj->value[2] = 4;` (for 10d4), the code would use `obj->_weapon->dice_num = 10;` and `obj->_weapon->dice_size = 4;`.

### 2.2. Current State (`src`)

The `src` codebase still uses the legacy `long value[8]` array on both `OBJ_INDEX_DATA` and `OBJ_DATA`. All object-specific logic is implemented by accessing these integer indices, which is error-prone and difficult to maintain.

### 2.3. Backporting Plan: Object Multityping

This is a more invasive change than the reputation system and must be handled carefully.

1.  **Data Structures:**
    -   Define and copy all the type-specific object data structs (`ARMOR_DATA`, `WEAPON_DATA`, etc.) from `src_20_dev/merc.h` into `src/merc.h`.
    -   Add the corresponding `*_armor`, `*_weapon`, etc. pointers to `struct obj_index_data` and `struct obj_data`.

2.  **Memory Management:**
    -   Implement `new_*()` and `free_*()` functions for each new object data type in `src/mem.c`.
    -   Update `new_obj_index()` and `new_obj()` to allocate memory for these new structs based on the object's `item_type`.
    -   Update `free_obj_index()` and `free_obj()` to free the associated memory.

3.  **Data Migration & Logic Refactor (The Hard Part):**
    -   This is the most critical step. A comprehensive, codebase-wide search-and-replace must be performed.
    -   Every instance of `obj->value[X]` must be identified and replaced with the correct access to the new struct pointer (e.g., `obj->value[1]` for a weapon becomes `obj->_weapon->dice_num`).
    -   This requires careful analysis for each `item_type` to ensure the correct mapping.
    -   Automated scripting for this refactoring should be considered, but manual review will be essential.

4.  **OLC Refactor:**
    -   The `oedit` functions in `src/editors/objects/oedit.c` must be completely overhauled.
    -   Instead of commands like `value0`, `value1`, etc., new commands must be created to modify the fields of the specific data structs (e.g., `oedit damage <dice>`, `oedit charges <#>`, `oedit capacity <#>`, etc.).

5.  **Persistence:**
    -   The `io/json/json_objects.c` file must be updated.
    -   The `json_write_obj_values` function will be replaced. Instead of writing a simple `values` array, it will need to serialize the appropriate type-specific struct to a JSON object.
    -   The `json_read_obj_values` function will need to be updated to read this new JSON object and populate the corresponding struct.

This backport will be a significant undertaking but will result in a much cleaner, more robust, and more maintainable object system for the future.
