#!/usr/bin/env python3
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

EXCLUDE_DIRS = {
    ".build",
    ".deps",
    "old_stuff",
    "src_20_dev",
}

ALLOWLIST = {
    "do_mailshow",
    "do_chat_show",
}

FUNC_RE = re.compile(r"^\s*void\s+(do_[A-Za-z0-9_]*show)\s*\(")


def iter_c_files(root: pathlib.Path):
    for path in root.rglob("*.c"):
        rel_parts = path.relative_to(root).parts
        if any(part in EXCLUDE_DIRS for part in rel_parts):
            continue
        yield path


def collect_show_functions(path: pathlib.Path):
    lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    i = 0
    while i < len(lines):
        match = FUNC_RE.match(lines[i])
        if not match:
            i += 1
            continue

        name = match.group(1)
        start_line = i + 1

        body_lines = []
        depth = 0
        seen_open = False

        while i < len(lines):
            line = lines[i]
            body_lines.append(line)

            opens = line.count("{")
            closes = line.count("}")

            if opens > 0:
                seen_open = True
            depth += opens - closes

            i += 1

            if seen_open and depth <= 0:
                break

        yield name, start_line, "\n".join(body_lines)


def main() -> int:
    failures = []

    for cfile in iter_c_files(ROOT):
        for name, start_line, body in collect_show_functions(cfile):
            if name in ALLOWLIST:
                continue

            if re.search(r"\bold_edit\b", body):
                failures.append((cfile, start_line, name, "uses old_edit swap pattern"))

            if re.search(r"ch->desc->pEdit\s*=", body):
                failures.append((cfile, start_line, name, "assigns ch->desc->pEdit directly"))

            calls_edit_show = re.search(r"\b[a-z]+edit_show\s*\(\s*ch\s*,", body) is not None
            uses_helper = "olc_show_item(" in body
            if calls_edit_show and not uses_helper:
                failures.append((cfile, start_line, name, "calls *edit_show directly instead of olc_show_item"))

    if failures:
        print("OLC show guard failed:")
        for path, line, name, reason in failures:
            rel = path.relative_to(ROOT)
            print(f"  - {rel}:{line} {name}: {reason}")
        return 1

    print("OLC show guard passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
