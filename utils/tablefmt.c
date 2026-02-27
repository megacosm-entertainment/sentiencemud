#include "tablefmt.h"
#include "buffer.h"

extern bool add_buf(BUFFER *buffer, const char *string);

#define TABLEFMT_MAX_COLS 16

static size_t tablefmt_colour_token_len(const char *s)
{
    size_t i;

    if (s == NULL || s[0] == '\0' || s[0] != COLOUR_CHAR || s[1] == '\0')
    return 0;

    if (s[1] == '[')
    {
    for (i = 0; i < 5 && s[i] != '\0'; i++)
        ;
    return i;
    }

    return 2;
}

static bool tablefmt_append_n_spaces(char *dst, size_t dst_size, int count)
{
    int i;
    for (i = 0; i < count; i++)
    {
    if (strlcat(dst, " ", dst_size) >= dst_size)
        return false;
    }
    return true;
}

static bool tablefmt_wrap_next_chunk(const char *src, size_t start, int width,
    char *out, size_t out_size, size_t *next_start)
{
    size_t i;
    size_t out_len;
    size_t last_space_src;
    size_t last_space_out;
    int visible;
    bool saw_content;

    if (src == NULL || out == NULL || out_size == 0 || next_start == NULL || width <= 0)
    return false;

    out[0] = '\0';
    out_len = 0;
    visible = 0;
    i = start;
    last_space_src = (size_t)-1;
    last_space_out = (size_t)-1;
    saw_content = false;

    while (src[i] != '\0')
    {
    size_t colour_len;

    if (src[i] == '\n' || src[i] == '\r')
    {
        i++;
        if (src[i] == '\n' || src[i] == '\r')
        i++;
        break;
    }

    colour_len = tablefmt_colour_token_len(&src[i]);
    if (colour_len > 0)
    {
        size_t j;

        if (out_len + colour_len >= out_size)
        return false;

        for (j = 0; j < colour_len; j++)
        out[out_len++] = src[i++];
        out[out_len] = '\0';
        continue;
    }

    if (visible >= width)
    {
        if (last_space_src != (size_t)-1)
        {
        out[last_space_out] = '\0';
        i = last_space_src + 1;
        while (src[i] == ' ')
            i++;
        }
        break;
    }

    if (out_len + 1 >= out_size)
        return false;

    out[out_len++] = src[i];
    out[out_len] = '\0';

    if (src[i] == ' ')
    {
        last_space_src = i;
        last_space_out = out_len - 1;
    }

    visible++;
    saw_content = true;
    i++;
    }

    if (!saw_content)
    out[0] = '\0';

    *next_start = i;
    return true;
}

static bool tablefmt_append_aligned(char *dst, size_t dst_size, const char *text, int width, tablefmt_align_t align)
{
    int visible;
    int pad_left;
    int pad_right;

    if (dst == NULL || dst_size == 0 || text == NULL || width < 0)
    return false;

    visible = strlen_no_colours(text);
    if (visible > width)
        visible = width;

    pad_left = 0;
    pad_right = width - visible;

    if (align == TABLEFMT_ALIGN_RIGHT)
    {
    pad_left = width - visible;
    pad_right = 0;
    }
    else if (align == TABLEFMT_ALIGN_CENTER)
    {
    pad_left = (width - visible) / 2;
    pad_right = width - visible - pad_left;
    }

    if (!tablefmt_append_n_spaces(dst, dst_size, pad_left))
    return false;
    if (strlcat(dst, text, dst_size) >= dst_size)
    return false;
    if (!tablefmt_append_n_spaces(dst, dst_size, pad_right))
    return false;

    return true;
}

bool tablefmt_append_repeat(char *dst, size_t dst_size, const char *token, int count)
{
    int i;

    if (dst == NULL || dst_size == 0 || token == NULL || count < 0)
    return false;

    for (i = 0; i < count; i++)
    {
    if (strlcat(dst, token, dst_size) >= dst_size)
        return false;
    }

    return true;
}

bool tablefmt_pad_visible(char *dst, size_t dst_size, int width)
{
    int visible;

    if (dst == NULL || dst_size == 0)
    return false;

    if (width <= 0)
    return true;

    visible = strlen_no_colours(dst);
    while (visible < width)
    {
    if (strlcat(dst, " ", dst_size) >= dst_size)
        return false;
    visible++;
    }

    return true;
}

bool tablefmt_build_border(char *dst, size_t dst_size, const char *prefix, const char *unit, int repeat_count, const char *suffix)
{
    if (dst == NULL || dst_size == 0 || prefix == NULL || unit == NULL || suffix == NULL || repeat_count < 0)
    return false;

    dst[0] = '\0';

    if (strlcat(dst, prefix, dst_size) >= dst_size)
    return false;

    if (!tablefmt_append_repeat(dst, dst_size, unit, repeat_count))
    return false;

    if (strlcat(dst, suffix, dst_size) >= dst_size)
    return false;

    return true;
}

tablefmt_style_t tablefmt_style_default(void)
{
    tablefmt_style_t style;

    style.cell_left = "|";
    style.cell_sep = "|";
    style.cell_right = "|";
    style.hr_left = "+";
    style.hr_sep = "+";
    style.hr_right = "+";
    style.hr_fill = "-";
    style.line_end = "\n\r";
    style.padding = 1;

    return style;
}

bool tablefmt_add_hr(BUFFER *out, const tablefmt_column_t *cols, int col_count, const tablefmt_style_t *style)
{
    char line[MAX_STRING_LENGTH];
    int i;
    int fill_width;

    if (out == NULL || cols == NULL || style == NULL || col_count <= 0 || col_count > TABLEFMT_MAX_COLS)
    return false;

    line[0] = '\0';
    if (strlcat(line, style->hr_left, sizeof(line)) >= sizeof(line))
    return false;

    for (i = 0; i < col_count; i++)
    {
    fill_width = cols[i].width + (style->padding * 2);
    if (!tablefmt_append_repeat(line, sizeof(line), style->hr_fill, fill_width))
        return false;

    if (i < col_count - 1)
    {
        if (strlcat(line, style->hr_sep, sizeof(line)) >= sizeof(line))
        return false;
    }
    }

    if (strlcat(line, style->hr_right, sizeof(line)) >= sizeof(line))
    return false;
    if (strlcat(line, style->line_end, sizeof(line)) >= sizeof(line))
    return false;

    return add_buf(out, line);
}

bool tablefmt_add_row(BUFFER *out, const tablefmt_column_t *cols, int col_count, const char *const *cells, const tablefmt_style_t *style)
{
    size_t offsets[TABLEFMT_MAX_COLS];
    bool done[TABLEFMT_MAX_COLS];
    char chunks[TABLEFMT_MAX_COLS][MAX_INPUT_LENGTH];
    char line[MAX_STRING_LENGTH];
    int i;
    bool any_open;

    if (out == NULL || cols == NULL || cells == NULL || style == NULL || col_count <= 0 || col_count > TABLEFMT_MAX_COLS)
    return false;

    for (i = 0; i < col_count; i++)
    {
    offsets[i] = 0;
    done[i] = false;
    chunks[i][0] = '\0';
    }

    do {
    any_open = false;
    line[0] = '\0';

    if (strlcat(line, style->cell_left, sizeof(line)) >= sizeof(line))
        return false;

    for (i = 0; i < col_count; i++)
    {
        const char *cell = cells[i] ? cells[i] : "";

        if (!done[i])
        {
        if (!tablefmt_wrap_next_chunk(cell, offsets[i], cols[i].width, chunks[i], sizeof(chunks[i]), &offsets[i]))
            return false;

        if (!cols[i].wrap)
        done[i] = true;
        else if (cell[offsets[i]] == '\0')
        done[i] = true;
        }
        else
        {
        chunks[i][0] = '\0';
        }

        if (style->padding > 0 && !tablefmt_append_n_spaces(line, sizeof(line), style->padding))
        return false;

        if (!tablefmt_append_aligned(line, sizeof(line), chunks[i], cols[i].width, cols[i].align))
        return false;

        if (style->padding > 0 && !tablefmt_append_n_spaces(line, sizeof(line), style->padding))
        return false;

        if (i < col_count - 1)
        {
        if (strlcat(line, style->cell_sep, sizeof(line)) >= sizeof(line))
            return false;
        }

        if (!done[i])
        any_open = true;
    }

    if (strlcat(line, style->cell_right, sizeof(line)) >= sizeof(line))
        return false;
    if (strlcat(line, style->line_end, sizeof(line)) >= sizeof(line))
        return false;

    if (!add_buf(out, line))
        return false;
    } while (any_open);

    return true;
}
