#ifndef REQUIREMENTS_H
#define REQUIREMENTS_H

#include <stdbool.h>
#include <jansson.h>

typedef struct char_data CHAR_DATA;

typedef struct requirement_context {
    CHAR_DATA *actor;
} REQUIREMENT_CONTEXT;

bool requirements_evaluate_json(const json_t *spec,
                                const REQUIREMENT_CONTEXT *context,
                                bool default_if_empty);
bool requirements_evaluate_text(const char *spec_json,
                                const REQUIREMENT_CONTEXT *context,
                                bool default_if_empty);

#endif