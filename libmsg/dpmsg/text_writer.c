/*
 * Copyright (C) 2022 askmeaboutloom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#include "text_writer.h"
#include "blend_mode.h"
#include "dpcommon/output.h"
#include "message.h"
#include <dpcommon/base64.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <ctype.h>
#include <inttypes.h>
#include <parson.h>

#define BASE64_LINE_WIDTH 70


struct DP_TextWriter {
    DP_Output *output;
    DP_Output *multiline_output;
    void **multiline_buffer;
    size_t *multiline_size;
};

DP_TextWriter *DP_text_writer_new(DP_Output *output)
{
    DP_ASSERT(output);
    DP_TextWriter *writer = DP_malloc(sizeof(*writer));
    *writer = (DP_TextWriter){output, NULL, NULL, NULL};
    return writer;
}

void DP_text_writer_free(DP_TextWriter *writer)
{
    if (writer) {
        DP_output_free(writer->multiline_output);
        DP_output_free(writer->output);
        DP_free(writer);
    }
}


static bool print_header_field(DP_Output *output, const char *key,
                               bool want_format, const char *fmt, ...)
{
    if (!(DP_output_write(output, "!", 1) && DP_output_print(output, key)
          && DP_output_write(output, "=", 1))) {
        return false;
    }

    bool result;
    if (want_format) {
        va_list ap;
        va_start(ap, fmt);
        result = DP_output_vformat(output, fmt, ap);
        va_end(ap);
    }
    else {
        result = DP_output_print(output, fmt);
    }

    return result && DP_output_write(output, "\n", 1);
}

static bool write_header_field(DP_Output *output, const char *key,
                               JSON_Value *value)
{
    switch (json_type(value)) {
    case JSONNull:
        return print_header_field(output, key, false, "null");
    case JSONString:
        return print_header_field(output, key, false, json_string(value));
    case JSONNumber:
        return print_header_field(output, key, true, "%f", json_number(value));
    case JSONBoolean:
        return print_header_field(output, key, false,
                                  json_boolean(value) ? "true" : "false");
    default:
        DP_error_set("Header field '%s' cannot be represented as text", key);
        return false;
    }
}

bool DP_text_writer_write_header(DP_TextWriter *writer, JSON_Object *header)
{
    DP_ASSERT(writer);
    DP_ASSERT(header);

    DP_Output *output = writer->output;
    size_t count = json_object_get_count(header);
    for (size_t i = 0; i < count; ++i) {
        const char *key = json_object_get_name(header, i);
        JSON_Value *value = json_object_get_value_at(header, i);
        if (!write_header_field(output, key, value)) {
            return false;
        }
    }

    return DP_output_write(output, "\n", 1);
}


bool DP_text_writer_start_message(DP_TextWriter *writer, DP_Message *msg)
{
    DP_ASSERT(writer);
    DP_ASSERT(msg);
    return DP_output_format(writer->output, "%d %s", DP_message_context_id(msg),
                            DP_message_name(msg));
}

bool DP_text_writer_finish_message(DP_TextWriter *writer, DP_Message *msg)
{
    DP_ASSERT(writer);
    DP_Output *output = writer->output;
    size_t *multiline_size = writer->multiline_size;
    bool add_newline = DP_message_type(msg) == DP_MSG_UNDO_POINT;
    if (multiline_size && *multiline_size > 0) {
        return DP_output_write(output, " {", 2)
            && DP_output_write(output, *writer->multiline_buffer,
                               *multiline_size)
            && DP_output_write(output, "\n}\n\n", add_newline ? 4 : 3)
            && DP_output_clear(writer->multiline_output);
    }
    else {
        return DP_output_write(output, "\n\n", add_newline ? 2 : 1);
    }
}


static bool format_argument(DP_TextWriter *writer, const char *fmt, ...)
    DP_FORMAT(2, 3);

static bool format_argument(DP_TextWriter *writer, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool ok = DP_output_vformat(writer->output, fmt, ap);
    va_end(ap);
    return ok;
}


bool DP_text_writer_write_bool(DP_TextWriter *writer, const char *key,
                               bool value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return format_argument(writer, " %s=%s", key, value ? "true" : "false");
}

bool DP_text_writer_write_int(DP_TextWriter *writer, const char *key, int value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return format_argument(writer, " %s=%d", key, value);
}

bool DP_text_writer_write_uint(DP_TextWriter *writer, const char *key,
                               unsigned int value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return format_argument(writer, " %s=%u", key, value);
}

bool DP_text_writer_write_decimal(DP_TextWriter *writer, const char *key,
                                  unsigned int value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return format_argument(writer, " %s=%.2f", key, value / 255.0 * 100.0);
}

static bool contains_whitespace(const char *value)
{
    for (const char *c = value; *c; ++c) {
        if (isspace(*c)) {
            return true;
        }
    }
    return false;
}

static bool buffer_line(DP_TextWriter *writer, const char *key,
                        const char *value, size_t length)
{
    return DP_output_format(writer->multiline_output, "\n\t%s=", key)
        && DP_output_write(writer->multiline_output, value, length);
}

static bool buffer_multiline_argument(DP_TextWriter *writer, const char *key,
                                      const char *value)
{
    if (!writer->multiline_output) {
        writer->multiline_output = DP_mem_output_new(
            0, true, &writer->multiline_buffer, &writer->multiline_size);
        if (!writer->multiline_output) {
            return false;
        }
    }

    for (size_t start = 0, end = 0;; ++end) {
        if (!value[end] || value[end] == '\n') {
            if (buffer_line(writer, key, value + start, end - start)) {
                if (value[end]) {
                    start = end + 1;
                }
                else {
                    break;
                }
            }
            else {
                return false;
            }
        }
    }

    return true;
}

bool DP_text_writer_write_string(DP_TextWriter *writer, const char *key,
                                 const char *value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    DP_ASSERT(value);
    if (contains_whitespace(value)) {
        return buffer_multiline_argument(writer, key, value);
    }
    else {
        return format_argument(writer, " %s=%s", key, value);
    }
}

#define ALPHA_MASK 0xff000000
#define RGB_MASK   0x00ffffff

bool DP_text_writer_write_argb_color(DP_TextWriter *writer, const char *key,
                                     uint32_t bgra)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    if ((bgra & ALPHA_MASK) == ALPHA_MASK) {
        return format_argument(writer, " %s=#%06" PRIx32, key, bgra & RGB_MASK);
    }
    else {
        return format_argument(writer, " %s=#%08" PRIx32, key, bgra);
    }
}

bool DP_text_writer_write_blend_mode(DP_TextWriter *writer, const char *key,
                                     int blend_mode)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return DP_text_writer_write_string(writer, key,
                                       DP_blend_mode_svg_name(blend_mode));
}

static bool buffer_wrapped_argument(DP_TextWriter *writer, const char *key,
                                    const char *value, size_t value_length,
                                    size_t line_width)
{
    if (!writer->multiline_output) {
        writer->multiline_output = DP_mem_output_new(
            0, true, &writer->multiline_buffer, &writer->multiline_size);
        if (!writer->multiline_output) {
            return false;
        }
    }

    for (size_t start = 0, end = 0;; ++end) {
        if (end == value_length || end - start == line_width) {
            if (buffer_line(writer, key, value + start, end - start)) {
                if (end != value_length) {
                    start = end;
                }
                else {
                    break;
                }
            }
            else {
                return false;
            }
        }
    }

    return true;
}

bool DP_text_writer_write_base64(DP_TextWriter *writer, const char *key,
                                 const unsigned char *value, int length)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);

    if (length <= 0) {
        return format_argument(writer, " %s=", key);
    }
    else {
        DP_ASSERT(value);
        size_t base64_length;
        char *base64 =
            DP_base64_encode(value, DP_int_to_size(length), &base64_length);
        bool ok = base64_length <= BASE64_LINE_WIDTH
                    ? format_argument(writer, " %s=%s", key, base64)
                    : buffer_wrapped_argument(writer, key, base64,
                                              base64_length, BASE64_LINE_WIDTH);
        DP_free(base64);
        return ok;
    }
}

bool DP_text_writer_write_flags(DP_TextWriter *writer, const char *key,
                                unsigned int value, ...)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);

    va_list ap;
    va_start(ap, value);
    bool first = true;
    bool ok = true;
    const char *name;

    while (ok && (name = va_arg(ap, const char *))) {
        unsigned int mask = va_arg(ap, unsigned int);
        if (value & mask) {
            if (first) {
                ok = format_argument(writer, " %s=%s", key, name);
                first = false;
            }
            else {
                ok = format_argument(writer, ",%s", name);
            }
        }
    }

    va_end(ap);
    return ok;
}

bool DP_text_writer_write_id(DP_TextWriter *writer, const char *key, int value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return format_argument(writer, " %s=0x%04x", key, value);
}


#define WRITE_LIST(WRITER, KEY, VALUE, COUNT, TYPE, FMT)                   \
    do {                                                                   \
        DP_ASSERT(WRITER);                                                 \
        DP_ASSERT(KEY);                                                    \
        if (COUNT == 0) {                                                  \
            DP_RETURN_UNLESS(format_argument(WRITER, " %s=", KEY));        \
        }                                                                  \
        else {                                                             \
            DP_RETURN_UNLESS(                                              \
                format_argument(WRITER, " %s=" FMT, KEY, (TYPE)VALUE[0])); \
            for (int _i = 1; _i < COUNT; ++_i) {                           \
                DP_RETURN_UNLESS(                                          \
                    format_argument(writer, "," FMT, (TYPE)VALUE[_i]));    \
            }                                                              \
        }                                                                  \
        return true;                                                       \
    } while (0)

bool DP_text_writer_write_id_list(DP_TextWriter *writer, const char *key,
                                  const int *value, int count)
{
    WRITE_LIST(writer, key, value, count, int, "0x%04x");
}

bool DP_text_writer_write_uint_list(DP_TextWriter *writer, const char *key,
                                    const unsigned int *value, int count)
{
    WRITE_LIST(writer, key, value, count, unsigned int, "%u");
}

bool DP_text_writer_write_uint8_list(DP_TextWriter *writer, const char *key,
                                     const uint8_t *value, int count)
{
    WRITE_LIST(writer, key, value, count, unsigned int, "%u");
}

bool DP_text_writer_write_uint16_list(DP_TextWriter *writer, const char *key,
                                      const uint16_t *value, int count)
{
    WRITE_LIST(writer, key, value, count, unsigned int, "%u");
}


bool DP_text_writer_raw_write(DP_TextWriter *writer, const char *buffer,
                              size_t size)
{
    DP_ASSERT(writer);
    return DP_output_write(writer->output, (const unsigned char *)buffer, size);
}

bool DP_text_writer_raw_print(DP_TextWriter *writer, const char *str)
{
    return str ? DP_text_writer_raw_write(writer, str, strlen(str)) : true;
}

bool DP_text_writer_raw_format(DP_TextWriter *writer, const char *fmt, ...)
{
    DP_ASSERT(writer);
    va_list ap;
    va_start(ap, fmt);
    bool ok = DP_output_vformat(writer->output, fmt, ap);
    va_end(ap);
    return ok;
}
