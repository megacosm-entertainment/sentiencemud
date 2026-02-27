# Table Formatting Guide (`utils/tablefmt`)

This guide covers how to render aligned, wrapped, color-aware tables using the shared helpers in `src/utils/tablefmt.h`.

## When to use `tablefmt`

Use `tablefmt` when output needs:

- multiple aligned columns,
- optional border/separator styles,
- wrapped multi-line cells,
- visible-width alignment with in-band color codes.

For one-off single lines, plain `snprintf` + `send_to_char` is still fine.

## API overview

Header: `src/utils/tablefmt.h`

### Low-level helpers

- `tablefmt_append_repeat(dst, size, token, count)`
  - Repeats a token into a string buffer safely.
- `tablefmt_pad_visible(dst, size, width)`
  - Pads with spaces to a *visible* width (ignores color token width).
- `tablefmt_build_border(dst, size, prefix, unit, repeat_count, suffix)`
  - Builds simple borders/dividers into a local string buffer.

### Table renderer

- `tablefmt_style_default()`
  - Returns baseline ASCII style (`|`, `+`, `-`, `\n\r`, padding=1).
- `tablefmt_add_hr(out, cols, col_count, style)`
  - Emits one horizontal rule line.
- `tablefmt_add_row(out, cols, col_count, cells, style)`
  - Emits one row, wrapping cell text by column width if enabled.

## Minimal usage pattern

1. Create a `BUFFER *out`.
2. Define `tablefmt_style_t style = tablefmt_style_default();`
3. Define `tablefmt_column_t cols[]` with width/wrap/alignment.
4. Call `tablefmt_add_hr()` / `tablefmt_add_row()` as needed.
5. Send `buf_string(out)` and `free_buf(out)`.

## Example skeleton

```c
BUFFER *out = new_buf();
tablefmt_style_t style = tablefmt_style_default();
tablefmt_column_t cols[3] = {
    { .width = 20, .wrap = true,  .align = TABLEFMT_ALIGN_LEFT },
    { .width = 10, .wrap = false, .align = TABLEFMT_ALIGN_RIGHT },
    { .width = 16, .wrap = true,  .align = TABLEFMT_ALIGN_LEFT }
};
const char *header[3] = { "Name", "Level", "Area" };

if (!tablefmt_add_hr(out, cols, 3, &style)
||  !tablefmt_add_row(out, cols, 3, header, &style)
||  !tablefmt_add_hr(out, cols, 3, &style)) {
    free_buf(out);
    send_to_char("Formatting error.\n\r", ch);
    return;
}
```

## Style customization

`tablefmt_style_t` allows command-specific formatting without duplicating rendering logic:

- `cell_left`, `cell_sep`, `cell_right` control row framing.
- `hr_left`, `hr_sep`, `hr_right`, `hr_fill` control horizontal lines.
- `padding` sets spaces inside each cell.
- `line_end` should normally remain `"\n\r"` for MUD output.

For sparse output (e.g. `who`-style), use empty separators (`""`) and include any bracket decoration in cell text.

## Color and width behavior

`tablefmt` computes visible width using `strlen_no_colours()`.

Implications:

- Color tokens do not count toward alignment width.
- Wrapping/padding uses display width, not byte count.
- Keep color tokens well-formed in cell strings.

Current wrapping supports standard two-byte color tokens and extended `{[...]` style tokens used in the codebase.

## Error handling pattern

Always treat helper failures as fatal for the current output block:

```c
if (!tablefmt_add_row(out, cols, 3, cells, &style)) {
    free_buf(out);
    send_to_char("Formatting error.\n\r", ch);
    return;
}
```

This keeps formatting paths fail-closed and avoids partial/corrupt output.

## Existing examples

- `do_areas`: `src/act_info.c`
- `do_who_new`: `src/act_info.c`
- Border/padding helpers in score output paths: `src/act_info.c`

Use these as reference patterns before adding new table outputs.
