/**
 * Hyper Programming Language - Common Utilities Implementation
 * 
 * Implementation of common utilities, memory management, and helper functions
 * used across all modules of the Hyper language system.
 */

#include "../../include/hyp_common.h"
#include <stdarg.h>

/* Arena allocator implementation */
hyp_arena_t* hyp_arena_create(size_t size) {
    hyp_arena_t* arena = HYP_MALLOC(sizeof(hyp_arena_t));
    if (!arena) return NULL;
    
    arena->memory = HYP_MALLOC(size);
    if (!arena->memory) {
        HYP_FREE(arena);
        return NULL;
    }
    
    arena->size = size;
    arena->used = 0;
    arena->next = NULL;
    
    return arena;
}

void hyp_arena_destroy(hyp_arena_t* arena) {
    while (arena) {
        hyp_arena_t* next = arena->next;
        HYP_FREE(arena->memory);
        HYP_FREE(arena);
        arena = next;
    }
}

void* hyp_arena_alloc(hyp_arena_t* arena, size_t size) {
    if (!arena) return NULL;
    
    /* Align to 8-byte boundary */
    size = (size + 7) & ~7;
    
    /* Check if current arena has enough space */
    if (arena->used + size > arena->size) {
        /* Create new arena if needed */
        size_t new_size = arena->size * 2;
        if (new_size < size) new_size = size * 2;
        
        hyp_arena_t* new_arena = hyp_arena_create(new_size);
        if (!new_arena) return NULL;
        
        new_arena->next = arena->next;
        arena->next = new_arena;
        arena = new_arena;
    }
    
    void* ptr = arena->memory + arena->used;
    arena->used += size;
    return ptr;
}

void hyp_arena_reset(hyp_arena_t* arena) {
    while (arena) {
        arena->used = 0;
        arena = arena->next;
    }
}

/* String utilities implementation */
hyp_string_t hyp_string_create(const char* str) {
    hyp_string_t string;
    if (!str) {
        string.data = NULL;
        string.length = 0;
        string.capacity = 0;
        return string;
    }
    
    string.length = strlen(str);
    string.capacity = string.length + 1;
    string.data = HYP_MALLOC(string.capacity);
    
    if (string.data) {
#ifdef _MSC_VER
        strcpy_s(string.data, string.capacity, str);
#else
        strncpy(string.data, str, string.capacity - 1);
        string.data[string.capacity - 1] = '\0';
#endif
    } else {
        string.length = 0;
        string.capacity = 0;
    }
    
    return string;
}

void hyp_string_destroy(hyp_string_t* str) {
    if (str && str->data) {
        HYP_FREE(str->data);
        str->length = 0;
        str->capacity = 0;
    }
}

void hyp_string_append(hyp_string_t* str, const char* append) {
    if (!str || !append) return;
    
    size_t append_len = strlen(append);
    size_t new_length = str->length + append_len;
    
    if (new_length + 1 > str->capacity) {
        size_t new_capacity = (new_length + 1) * 2;
        char* new_data = HYP_REALLOC(str->data, new_capacity);
        if (!new_data) return;
        
        str->data = new_data;
        str->capacity = new_capacity;
    }
    
#ifdef _MSC_VER
    strcpy_s(str->data + str->length, str->capacity - str->length, append);
#else
    strncpy(str->data + str->length, append, str->capacity - str->length - 1);
    str->data[new_length] = '\0';
#endif
    str->length = new_length;
}

int hyp_string_compare(const hyp_string_t* a, const hyp_string_t* b) {
    if (!a || !b) return -1;
    if (!a->data || !b->data) return -1;
    
    return strcmp(a->data, b->data);
}

/* File utilities implementation */
char* hyp_read_file(const char* filename, size_t* size) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;
    
    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }
    
    /* Allocate buffer */
    char* buffer = HYP_MALLOC(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    /* Read file */
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        HYP_FREE(buffer);
        return NULL;
    }
    
    buffer[file_size] = '\0';
    if (size) *size = file_size;
    
    return buffer;
}

hyp_error_t hyp_write_file(const char* filename, const char* content, size_t size) {
    FILE* file = fopen(filename, "wb");
    if (!file) return HYP_ERROR_IO;
    
    size_t bytes_written = fwrite(content, 1, size, file);
    fclose(file);
    
    return (bytes_written == size) ? HYP_OK : HYP_ERROR_IO;
}

bool hyp_file_exists(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}