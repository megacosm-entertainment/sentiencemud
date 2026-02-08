# Analysis of Data-Driven Class/Progression Systems in `src_20_dev`

This document outlines the findings from an analysis of the class and progression systems in the `src_20_dev` directory, comparing it to the current `src` directory.

## Summary

The `src_20_dev` directory introduces a completely refactored, data-driven class system, which is a significant improvement over the hardcoded system found in `src`. The new system is more flexible, extensible, and easier to maintain.

## Old System (`src`)

In the original `src` directory, the class system is largely hardcoded, making it rigid and difficult to modify.

*   **Hardcoded Definitions:** Class and subclass definitions are stored in static arrays, `class_table` and `sub_class_table`, located in `src/const.c`.
*   **Limited Extensibility:** Adding new classes or changing existing ones requires modifying the source code and recompiling the server. This is a significant barrier to content development and iteration.
*   **Rigid Structure:** Player class information is stored in a series of integer fields within the `pcdata` struct, limiting a player to a small, fixed set of classes.

## New System (`src_20_dev`)

The `src_20_dev` directory implements a modern, data-driven approach to classes and progression.

*   **External Data File:** Class definitions are loaded at boot time from an external file: `system/classes.dat`. The `load_classes` function in `src_20_dev/skills.c` is responsible for parsing this file.
*   **New Data Structures:**
    *   `CLASS_DATA`: Defined in `src_20_dev/merc.h`, this struct holds the properties for a single class (e.g., name, stat modifiers, etc.).
    *   `CLASS_LEVEL`: Also in `src_20_dev/merc.h`, this struct tracks a player's level and experience within a specific class. Players have a list of these, enabling multi-classing.
*   **Migration Path:** The system is designed for a smooth transition. If `classes.dat` is not found, the `load_classes` function will bootstrap the data from the old hardcoded tables and create the new file.
*   **Extensibility:** The new system is highly extensible. New classes, including non-combat types such as `CLASS_CRAFTING` and `CLASS_GATHERING` (defined in `src_20_dev/tables.c`), can be added simply by editing the `classes.dat` file.

## Benefits of the New System

The new data-driven system offers several key advantages:

*   **Flexibility:** Allows for easy addition, removal, and modification of classes without needing to recompile the codebase.
*   **Multi-classing:** The architecture naturally supports players having levels in multiple classes simultaneously.
*   **Maintainability:** Separating class data from the source code makes the system cleaner and easier to manage.

## Key Files and Symbols

*   **`src_20_dev/skills.c`**: Contains the core logic for loading and saving class data (`load_classes`, `save_classes`).
*   **`src_20_dev/merc.h`**: Defines the new data structures (`CLASS_DATA`, `CLASS_LEVEL`) and the location of the data file (`CLASSES_FILE`).
*   **`src_2c_dev/tables.c`**: Defines enumerations for class types and flags.
*   **`src/const.c`**: (In the old system) Contains the hardcoded `class_table` and `sub_class_table`.
