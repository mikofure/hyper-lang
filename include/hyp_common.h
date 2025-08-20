/**
 * Hyper Programming Language - Common Definitions
 * 
 * This header contains common types, constants, and utility functions
 * used across all modules of the Hyper language system.
 */

#ifndef HYP_COMMON_H
#define HYP_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

/* Platform detection */
#ifdef _WIN32
    #define HYP_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define HYP_PLATFORM_LINUX
#elif defined(__APPLE__)
    #define HYP_PLATFORM_MACOS
#endif

/* Compiler attributes */
#ifdef __GNUC__
    #define HYP_INLINE __inline__
    #define HYP_FORCE_INLINE __attribute__((always_inline))
    #define HYP_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
    #define HYP_INLINE __inline
    #define HYP_FORCE_INLINE __forceinline
    #define HYP_NORETURN __declspec(noreturn)
#else
    #define HYP_INLINE inline
    #define HYP_FORCE_INLINE inline
    #define HYP_NORETURN
#endif

/* Memory management macros */
#define HYP_MALLOC(size) malloc(size)
#define HYP_CALLOC(count, size) calloc(count, size)
#define HYP_REALLOC(ptr, size) realloc(ptr, size)
#define HYP_FREE(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while(0)

/* Error handling */
typedef enum {
    HYP_OK = 0,
    HYP_ERROR_MEMORY,
    HYP_ERROR_IO,
    HYP_ERROR_SYNTAX,
    HYP_ERROR_SEMANTIC,
    HYP_ERROR_RUNTIME,
    HYP_ERROR_INVALID_ARG,
    HYP_ERROR_NOT_FOUND,
    HYP_ERROR_PERMISSION
} hyp_error_t;

/* String utilities */
typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} hyp_string_t;

/* Dynamic array template */
#define HYP_ARRAY(type) struct { \
    type* data; \
    size_t count; \
    size_t capacity; \
}

/* Array operations */
#define HYP_ARRAY_INIT(arr) do { \
    (arr)->data = NULL; \
    (arr)->count = 0; \
    (arr)->capacity = 0; \
} while(0)

#define HYP_ARRAY_PUSH(arr, item) do { \
    if ((arr)->count >= (arr)->capacity) { \
        size_t new_cap = (arr)->capacity == 0 ? 8 : (arr)->capacity * 2; \
        (arr)->data = HYP_REALLOC((arr)->data, new_cap * sizeof(*(arr)->data)); \
        (arr)->capacity = new_cap; \
    } \
    (arr)->data[(arr)->count++] = (item); \
} while(0)

#define HYP_ARRAY_FREE(arr) do { \
    HYP_FREE((arr)->data); \
    (arr)->count = 0; \
    (arr)->capacity = 0; \
} while(0)

/* Arena allocator for fast memory management */
typedef struct hyp_arena {
    char* memory;
    size_t size;
    size_t used;
    struct hyp_arena* next;
} hyp_arena_t;

/* Function declarations */
hyp_arena_t* hyp_arena_create(size_t size);
void hyp_arena_destroy(hyp_arena_t* arena);
void* hyp_arena_alloc(hyp_arena_t* arena, size_t size);
void hyp_arena_reset(hyp_arena_t* arena);

/* String functions */
hyp_string_t hyp_string_create(const char* str);
void hyp_string_destroy(hyp_string_t* str);
void hyp_string_append(hyp_string_t* str, const char* append);
int hyp_string_compare(const hyp_string_t* a, const hyp_string_t* b);

/* File utilities */
char* hyp_read_file(const char* filename, size_t* size);
hyp_error_t hyp_write_file(const char* filename, const char* content, size_t size);
bool hyp_file_exists(const char* filename);

/* Debug and logging */
#ifdef DEBUG
    #define HYP_DEBUG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
    #define HYP_DEBUG(fmt, ...)
#endif

#define HYP_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define HYP_WARNING(fmt, ...) fprintf(stderr, "[WARNING] " fmt "\n", ##__VA_ARGS__)
#define HYP_INFO(fmt, ...) fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)

/* Version information */
#define HYP_VERSION_MAJOR 0
#define HYP_VERSION_MINOR 1
#define HYP_VERSION_PATCH 0
#define HYP_VERSION_STRING "0.1.0"

#endif /* HYP_COMMON_H */