/*
 * Copyright (c) 2013 PADL Software Pty Ltd
 * Portions Copyright (c) 2009-2011 Petri Lehtinen <petri@digip.org>
 *
 * cfjson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _CFJSON_H_
#define _CFJSON_H_ 1

#ifdef JANSSON_H
#error <jansson.h> cannot be included at the same time as <cfjson.h>
#endif

#define JANSSON_H 1

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_INTEGER,
    JSON_REAL,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL
} json_type;

typedef const void json_t;

#if __LP64__
typedef long long json_int_t;
#define JSON_INTEGER_TYPE kCFNumberLongLongType
#define JSON_INTEGER_FORMAT "lld"
#else
typedef long json_int_t;
#define JSON_INTEGER_TYPE kCFNumberLongType
#define JSON_INTEGER_FORMAT "ld"
#endif /* __LP64__ */

json_t *json_object(void) CF_RETURNS_RETAINED;
json_t *json_array(void) CF_RETURNS_RETAINED;
json_t *json_string(const char *value) CF_RETURNS_RETAINED;
json_t *json_string_nocheck(const char *value) CF_RETURNS_RETAINED;
json_t *json_integer(json_int_t value) CF_RETURNS_RETAINED;
json_t *json_real(double value) CF_RETURNS_RETAINED;
json_t *json_true(void) CF_RETURNS_RETAINED;
json_t *json_false(void) CF_RETURNS_RETAINED;
json_t *json_null(void) CF_RETURNS_RETAINED;

static inline json_type
json_typeof(json_t *json)
{
    CFTypeID cfTypeID = CFGetTypeID(json);
    json_type jsonType = JSON_OBJECT;

    if (cfTypeID == CFDictionaryGetTypeID()) {
        jsonType = JSON_OBJECT;
    } else if (cfTypeID == CFArrayGetTypeID()) {
        jsonType = JSON_ARRAY;
    } else if (cfTypeID == CFStringGetTypeID()) {
        jsonType = JSON_STRING;
    } else if (cfTypeID == CFNumberGetTypeID()) {
        if (CFNumberGetType((CFNumberRef)json) == kCFNumberDoubleType)
            jsonType = JSON_REAL;
        else
            jsonType = JSON_INTEGER;
    } else if (cfTypeID == CFBooleanGetTypeID()) {
        jsonType = CFBooleanGetValue((CFBooleanRef)json) ? JSON_TRUE : JSON_FALSE;
    } else if (cfTypeID == CFNullGetTypeID()) {
        jsonType = JSON_NULL;
    }

    return jsonType;
}

#define json_is_object(json)   (json && json_typeof(json) == JSON_OBJECT)
#define json_is_array(json)    (json && json_typeof(json) == JSON_ARRAY)
#define json_is_string(json)   (json && json_typeof(json) == JSON_STRING)
#define json_is_integer(json)  (json && json_typeof(json) == JSON_INTEGER)
#define json_is_real(json)     (json && json_typeof(json) == JSON_REAL)
#define json_is_number(json)   (json_is_integer(json) || json_is_real(json))
#define json_is_true(json)     (json && json_typeof(json) == JSON_TRUE)
#define json_is_false(json)    (json && json_typeof(json) == JSON_FALSE)
#define json_is_boolean(json)  (json_is_true(json) || json_is_false(json))
#define json_is_null(json)     (json && json_typeof(json) == JSON_NULL)

CF_RETURNS_RETAINED static inline
json_t *json_incref(json_t *json)
{
    if (json)
        return (json_t *)CFRetain(json);
    return NULL;
}

static inline
void json_decref(json_t *json CF_CONSUMED)
{
    if (json)
        CFRelease(json);
}

#define JSON_ERROR_TEXT_LENGTH    160
#define JSON_ERROR_SOURCE_LENGTH   80

typedef struct {
    int line;
    int column;
    int position;
    char source[JSON_ERROR_SOURCE_LENGTH];
    char text[JSON_ERROR_TEXT_LENGTH];
} json_error_t;

size_t json_object_size(const json_t *object);
json_t *json_object_get(const json_t *object, const char *key) CF_RETURNS_NOT_RETAINED;
int json_object_set_new(json_t *object, const char *key, json_t *value CF_CONSUMED);
int json_object_set_new_nocheck(json_t *object, const char *key, json_t *value CF_CONSUMED);
int json_object_del(json_t *object, const char *key);
int json_object_clear(json_t *object);
int json_object_update(json_t *object, json_t *other);
void *json_object_iter(json_t *object);
void *json_object_iter_at(json_t *object, const char *key);
void *json_object_iter_next(json_t *object, void *iter);
const char *json_object_iter_key(void *iter);
json_t *json_object_iter_value(void *iter) CF_RETURNS_NOT_RETAINED;
int json_object_iter_set_new(json_t *object, void *iter, json_t *value CF_CONSUMED);

int json_object_set(json_t *object, const char *key, json_t *value);
int json_object_set_nocheck(json_t *object, const char *key, json_t *value);
int json_object_iter_set(json_t *object, void *iter, json_t *value);

size_t json_array_size(const json_t *array);
json_t *json_array_get(const json_t *array, size_t index) CF_RETURNS_NOT_RETAINED;
int json_array_set_new(json_t *array, size_t index, json_t *value CF_CONSUMED);
int json_array_append_new(json_t *array, json_t *value CF_CONSUMED);
int json_array_insert_new(json_t *array, size_t index, json_t *value CF_CONSUMED);
int json_array_remove(json_t *array, size_t index);
int json_array_clear(json_t *array);
int json_array_extend(json_t *array, json_t *other);

int json_array_set(json_t *array, size_t index, json_t *value);
int json_array_append(json_t *array, json_t *value);
int json_array_insert(json_t *array, size_t index, json_t *value);

const char *json_string_value(const json_t *string);
json_int_t json_integer_value(const json_t *integer);
double json_real_value(const json_t *real);
double json_number_value(const json_t *json);

int json_string_set(json_t *string, const char *value);
int json_string_set_nocheck(json_t *string, const char *value);
int json_integer_set(json_t *integer, json_int_t value);
int json_real_set(json_t *real, double value);

#if 0
/* pack, unpack */

json_t *json_pack(const char *fmt, ...);
json_t *json_pack_ex(json_error_t *error, size_t flags, const char *fmt, ...);
json_t *json_vpack_ex(json_error_t *error, size_t flags, const char *fmt, va_list ap);

#define JSON_VALIDATE_ONLY  0x1
#define JSON_STRICT         0x2

int json_unpack(json_t *root, const char *fmt, ...);
int json_unpack_ex(json_t *root, json_error_t *error, size_t flags, const char *fmt, ...);
int json_vunpack_ex(json_t *root, json_error_t *error, size_t flags, const char *fmt, va_list ap);
#endif

/* equality */

int json_equal(json_t *value1, json_t *value2);

/* copying */

json_t *json_copy(json_t *value) CF_RETURNS_RETAINED;
#if 0
json_t *json_deep_copy(json_t *value) CF_RETURNS_RETAINED;
#endif

/* loading, printing */

json_t *json_loads(const char *input, size_t flags, json_error_t *error) CF_RETURNS_RETAINED;
json_t *json_loadcf(CFTypeRef input, size_t flags, json_error_t *error) CF_RETURNS_RETAINED;
json_t *json_loadf(FILE *input, size_t flags, json_error_t *error) CF_RETURNS_RETAINED;
json_t *json_load_file(const char *path, size_t flags, json_error_t *error) CF_RETURNS_RETAINED;

#define JSON_INDENT(n)      (n & 0x1F)
#define JSON_COMPACT        0x20
#define JSON_ENSURE_ASCII   0x40
#define JSON_SORT_KEYS      0x80
#define JSON_PRESERVE_ORDER 0x100

char *json_dumps(const json_t *json, size_t flags);
int json_dumpf(const json_t *json, FILE *output, size_t flags);
int json_dump_file(const json_t *json, const char *path, size_t flags);

/* custom memory allocation */

typedef void *(*json_malloc_t)(size_t);
typedef void (*json_free_t)(void *);

void json_set_alloc_funcs(json_malloc_t malloc_fn, json_free_t free_fn);

char *json_string_copy(json_t *string);

#ifdef __cplusplus
}
#endif

#endif /* _CFJSON_H_ */
