#include "command_handler.h"
#include "sim_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * @file command_handler.c
 * @brief JSON command parser implementation
 *
 * Simple hand-rolled JSON parser for command protocol.
 * Only handles the specific structures we need.
 */

// Command type names
static const char *cmd_type_names[] = {
    "unknown", "button", "cv_manual", "cv_lfo", "cv_envelope",
    "cv_gate", "cv_trigger", "cv_wavetable", "reset", "quit"
};

const char* command_type_str(CommandType type) {
    if (type >= sizeof(cmd_type_names) / sizeof(cmd_type_names[0])) {
        return "invalid";
    }
    return cmd_type_names[type];
}

// =============================================================================
// Simple JSON Parser
// =============================================================================

// Skip whitespace
static const char* skip_ws(const char *p) {
    while (*p && isspace(*p)) p++;
    return p;
}

// Parse string value (returns pointer past closing quote, or NULL on error)
// Writes string to buf (up to buf_size-1 chars)
static const char* parse_string(const char *p, char *buf, size_t buf_size) {
    p = skip_ws(p);
    if (*p != '"') return NULL;
    p++;

    size_t i = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            p++;  // Skip escape char, take next literally
        }
        if (i < buf_size - 1) {
            buf[i++] = *p;
        }
        p++;
    }
    buf[i] = '\0';

    if (*p != '"') return NULL;
    return p + 1;
}

// Parse number (int or float)
static const char* parse_number(const char *p, double *value) {
    p = skip_ws(p);
    char *end;
    *value = strtod(p, &end);
    if (end == p) return NULL;
    return end;
}

// Parse boolean
static const char* parse_bool(const char *p, bool *value) {
    p = skip_ws(p);
    if (strncmp(p, "true", 4) == 0) {
        *value = true;
        return p + 4;
    }
    if (strncmp(p, "false", 5) == 0) {
        *value = false;
        return p + 5;
    }
    return NULL;
}

// Find key in JSON object and return pointer to value
// Assumes p points to start of object (after '{')
static const char* find_key(const char *json, const char *key) {
    const char *p = strchr(json, '{');
    if (!p) return NULL;
    p++;

    char found_key[64];
    while (*p) {
        p = skip_ws(p);
        if (*p == '}') return NULL;  // End of object
        if (*p == ',') { p++; continue; }

        // Parse key
        const char *next = parse_string(p, found_key, sizeof(found_key));
        if (!next) return NULL;
        p = skip_ws(next);

        // Expect colon
        if (*p != ':') return NULL;
        p++;
        p = skip_ws(p);

        // Check if this is our key
        if (strcmp(found_key, key) == 0) {
            return p;  // Return pointer to value
        }

        // Skip value (simplified: handles strings, numbers, bools, nested objects)
        if (*p == '"') {
            char dummy[256];
            p = parse_string(p, dummy, sizeof(dummy));
        } else if (*p == '{') {
            // Skip nested object (count braces)
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                else if (*p == '"') {
                    char dummy[256];
                    p = parse_string(p, dummy, sizeof(dummy));
                    if (!p) return NULL;
                    continue;
                }
                p++;
            }
        } else if (*p == '[') {
            // Skip array (count brackets)
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '[') depth++;
                else if (*p == ']') depth--;
                p++;
            }
        } else {
            // Number or bool - skip to next delimiter
            while (*p && *p != ',' && *p != '}') p++;
        }

        if (!p) return NULL;
    }
    return NULL;
}

// Get string value for key
static bool get_string(const char *json, const char *key, char *buf, size_t buf_size) {
    const char *val = find_key(json, key);
    if (!val) return false;
    return parse_string(val, buf, buf_size) != NULL;
}

// Get number value for key
static bool get_number(const char *json, const char *key, double *value) {
    const char *val = find_key(json, key);
    if (!val) return false;
    return parse_number(val, value) != NULL;
}

// Get bool value for key
static bool get_bool(const char *json, const char *key, bool *value) {
    const char *val = find_key(json, key);
    if (!val) return false;
    return parse_bool(val, value) != NULL;
}

// =============================================================================
// Command Handlers
// =============================================================================

static CommandResult handle_button(const char *json) {
    CommandResult result = { CMD_BUTTON, false, false, "" };

    char id[16];
    bool state;

    if (!get_string(json, "id", id, sizeof(id))) {
        snprintf(result.error, sizeof(result.error), "missing 'id' field");
        return result;
    }
    if (!get_bool(json, "state", &state)) {
        snprintf(result.error, sizeof(result.error), "missing 'state' field");
        return result;
    }

    if (strcmp(id, "a") == 0) {
        sim_set_button_a(state);
        result.success = true;
    } else if (strcmp(id, "b") == 0) {
        sim_set_button_b(state);
        result.success = true;
    } else {
        snprintf(result.error, sizeof(result.error), "invalid button id: %s", id);
    }

    return result;
}

static CommandResult handle_cv_manual(const char *json, CVSource *cv_source) {
    CommandResult result = { CMD_CV_MANUAL, false, false, "" };

    double value;
    if (!get_number(json, "value", &value)) {
        snprintf(result.error, sizeof(result.error), "missing 'value' field");
        return result;
    }

    if (value < 0) value = 0;
    if (value > 255) value = 255;

    cv_source_set_manual(cv_source, (uint8_t)value);
    result.success = true;
    return result;
}

static CommandResult handle_cv_lfo(const char *json, CVSource *cv_source) {
    CommandResult result = { CMD_CV_LFO, false, false, "" };

    double freq_hz = 1.0;
    double min_val = 0;
    double max_val = 255;
    char shape_str[16] = "sine";

    get_number(json, "freq_hz", &freq_hz);
    get_number(json, "min", &min_val);
    get_number(json, "max", &max_val);
    get_string(json, "shape", shape_str, sizeof(shape_str));

    // Parse shape
    LFOShape shape = LFO_SINE;
    if (strcmp(shape_str, "sine") == 0) shape = LFO_SINE;
    else if (strcmp(shape_str, "tri") == 0 || strcmp(shape_str, "triangle") == 0) shape = LFO_TRI;
    else if (strcmp(shape_str, "saw") == 0 || strcmp(shape_str, "sawtooth") == 0) shape = LFO_SAW;
    else if (strcmp(shape_str, "square") == 0) shape = LFO_SQUARE;
    else if (strcmp(shape_str, "random") == 0 || strcmp(shape_str, "sh") == 0) shape = LFO_RANDOM;

    // Clamp values
    if (freq_hz < 0.01) freq_hz = 0.01;
    if (freq_hz > 100) freq_hz = 100;
    if (min_val < 0) min_val = 0;
    if (min_val > 255) min_val = 255;
    if (max_val < 0) max_val = 0;
    if (max_val > 255) max_val = 255;

    cv_source_set_lfo(cv_source, (float)freq_hz, shape,
                      (uint8_t)min_val, (uint8_t)max_val);
    result.success = true;
    return result;
}

static CommandResult handle_cv_envelope(const char *json, CVSource *cv_source) {
    CommandResult result = { CMD_CV_ENVELOPE, false, false, "" };

    double attack = 10;
    double decay = 100;
    double sustain = 200;
    double release = 200;

    get_number(json, "attack_ms", &attack);
    get_number(json, "decay_ms", &decay);
    get_number(json, "sustain", &sustain);
    get_number(json, "release_ms", &release);

    // Clamp values
    if (attack < 0) attack = 0;
    if (decay < 0) decay = 0;
    if (sustain < 0) sustain = 0;
    if (sustain > 255) sustain = 255;
    if (release < 0) release = 0;

    cv_source_set_envelope(cv_source,
                           (uint16_t)attack,
                           (uint16_t)decay,
                           (uint8_t)sustain,
                           (uint16_t)release);
    result.success = true;
    return result;
}

static CommandResult handle_cv_gate(const char *json, CVSource *cv_source) {
    CommandResult result = { CMD_CV_GATE, false, false, "" };

    bool state;
    if (!get_bool(json, "state", &state)) {
        snprintf(result.error, sizeof(result.error), "missing 'state' field");
        return result;
    }

    if (state) {
        cv_source_gate_on(cv_source);
    } else {
        cv_source_gate_off(cv_source);
    }
    result.success = true;
    return result;
}

static CommandResult handle_cv_trigger(CVSource *cv_source) {
    CommandResult result = { CMD_CV_TRIGGER, true, false, "" };
    cv_source_trigger(cv_source);
    return result;
}

// =============================================================================
// Main Entry Point
// =============================================================================

CommandResult command_handler_execute(const char *json, CVSource *cv_source) {
    CommandResult result = { CMD_UNKNOWN, false, false, "" };

    if (!json || !cv_source) {
        snprintf(result.error, sizeof(result.error), "null argument");
        return result;
    }

    // Get command type
    char cmd[32];
    if (!get_string(json, "cmd", cmd, sizeof(cmd))) {
        snprintf(result.error, sizeof(result.error), "missing 'cmd' field");
        return result;
    }

    // Dispatch to handler
    if (strcmp(cmd, "button") == 0) {
        return handle_button(json);
    }
    if (strcmp(cmd, "cv_manual") == 0) {
        return handle_cv_manual(json, cv_source);
    }
    if (strcmp(cmd, "cv_lfo") == 0) {
        return handle_cv_lfo(json, cv_source);
    }
    if (strcmp(cmd, "cv_envelope") == 0) {
        return handle_cv_envelope(json, cv_source);
    }
    if (strcmp(cmd, "cv_gate") == 0) {
        return handle_cv_gate(json, cv_source);
    }
    if (strcmp(cmd, "cv_trigger") == 0) {
        return handle_cv_trigger(cv_source);
    }
    if (strcmp(cmd, "reset") == 0) {
        sim_reset_time();
        cv_source_init(cv_source);
        result.type = CMD_RESET;
        result.success = true;
        return result;
    }
    if (strcmp(cmd, "quit") == 0) {
        result.type = CMD_QUIT;
        result.success = true;
        result.should_quit = true;
        return result;
    }

    snprintf(result.error, sizeof(result.error), "unknown command: %s", cmd);
    return result;
}
