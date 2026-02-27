#ifndef __UTILS_TABLEFMT_H__
#define __UTILS_TABLEFMT_H__

#include "../merc.h"

bool tablefmt_append_repeat(char *dst, size_t dst_size, const char *token, int count);
bool tablefmt_pad_visible(char *dst, size_t dst_size, int width);
bool tablefmt_build_border(char *dst, size_t dst_size, const char *prefix, const char *unit, int repeat_count, const char *suffix);

typedef enum {
	TABLEFMT_ALIGN_LEFT = 0,
	TABLEFMT_ALIGN_RIGHT,
	TABLEFMT_ALIGN_CENTER
} tablefmt_align_t;

typedef struct {
	int width;
	bool wrap;
	tablefmt_align_t align;
} tablefmt_column_t;

typedef struct {
	const char *cell_left;
	const char *cell_sep;
	const char *cell_right;
	const char *hr_left;
	const char *hr_sep;
	const char *hr_right;
	const char *hr_fill;
	const char *line_end;
	int padding;
} tablefmt_style_t;

tablefmt_style_t tablefmt_style_default(void);
bool tablefmt_add_hr(BUFFER *out, const tablefmt_column_t *cols, int col_count, const tablefmt_style_t *style);
bool tablefmt_add_row(BUFFER *out, const tablefmt_column_t *cols, int col_count, const char *const *cells, const tablefmt_style_t *style);

#endif
