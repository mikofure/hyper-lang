/**
 * Hyper Programming Language - Runtime System
 * 
 * Core runtime implementation for executing Hyper programs.
 * Handles value management, function calls, garbage collection, and execution.
 */

#include "../../include/hyp_runtime.h"
#include "../../include/hyp_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Built-in function implementations */
static hyp_value_t builtin_print(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count) {
    for (size_t i = 0; i < arg_count; i++) {
        if (i > 0) printf(" ");
        
        switch (args[i].type) {
            case HYP_VALUE_NULL:
                printf("null");
                break;
            case HYP_VALUE_BOOLEAN:
                printf("%s", args[i].as.boolean ? "true" : "false");
                break;
            case HYP_VALUE_NUMBER:
                printf("%.17g", args[i].as.number);
                break;
            case HYP_VALUE_STRING:
                printf("%s", args[i].as.string->chars);
                break;
            case HYP_VALUE_ARRAY:
                printf("[Array]");
                break;
            case HYP_VALUE_OBJECT:
                printf("[Object]");
                break;
            case HYP_VALUE_FUNCTION:
                printf("[Function]");
                break;
            case HYP_VALUE_NATIVE_FUNCTION:
                printf("[Native Function]");
                break;
        }
    }
    printf("\n");
    
    return hyp_value_null();
}

static hyp_value_t builtin_typeof(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count) {
    if (arg_count != 1) {
        runtime->error = "typeof expects exactly 1 argument";
        return hyp_value_null();
    }
    
    const char* type_name;
    switch (args[0].type) {
        case HYP_VALUE_NULL: type_name = "null"; break;
        case HYP_VALUE_BOOLEAN: type_name = "boolean"; break;
        case HYP_VALUE_NUMBER: type_name = "number"; break;
        case HYP_VALUE_STRING: type_name = "string"; break;
        case HYP_VALUE_ARRAY: type_name = "array"; break;
        case HYP_VALUE_OBJECT: type_name = "object"; break;
        case HYP_VALUE_FUNCTION: type_name = "function"; break;
        case HYP_VALUE_NATIVE_FUNCTION: type_name = "function"; break;
        default: type_name = "unknown"; break;
    }
    
    return hyp_value_string(hyp_string_create(type_name));
}

static hyp_value_t builtin_len(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count) {
    if (arg_count != 1) {
        runtime->error = "len expects exactly 1 argument";
        return hyp_value_null();
    }
    
    switch (args[0].type) {
        case HYP_VALUE_STRING:
            return hyp_value_number((double)args[0].as.string->length);
        case HYP_VALUE_ARRAY:
            return hyp_value_number((double)args[0].as.array->count);
        case HYP_VALUE_OBJECT:
            return hyp_value_number((double)args[0].as.object->count);
        default:
            runtime->error = "len can only be called on strings, arrays, or objects";
            return hyp_value_null();
    }
}

/* Value creation functions */
hyp_value_t hyp_value_null(void) {
    hyp_value_t value;
    value.type = HYP_VALUE_NULL;
    return value;
}

hyp_value_t hyp_value_boolean(bool boolean) {
    hyp_value_t value;
    value.type = HYP_VALUE_BOOLEAN;
    value.as.boolean = boolean;
    return value;
}

hyp_value_t hyp_value_number(double number) {
    hyp_value_t value;
    value.type = HYP_VALUE_NUMBER;
    value.as.number = number;
    return value;
}

hyp_value_t hyp_value_string(hyp_string_t* string) {
    hyp_value_t value;
    value.type = HYP_VALUE_STRING;
    value.as.string = string;
    return value;
}

hyp_value_t hyp_value_array(hyp_array_t* array) {
    hyp_value_t value;
    value.type = HYP_VALUE_ARRAY;
    value.as.array = array;
    return value;
}

hyp_value_t hyp_value_object(hyp_object_t* object) {
    hyp_value_t value;
    value.type = HYP_VALUE_OBJECT;
    value.as.object = object;
    return value;
}

hyp_value_t hyp_value_function(hyp_function_t* function) {
    hyp_value_t value;
    value.type = HYP_VALUE_FUNCTION;
    value.as.function = function;
    return value;
}

hyp_value_t hyp_value_native_function(hyp_native_function_t* native_function) {
    hyp_value_t value;
    value.type = HYP_VALUE_NATIVE_FUNCTION;
    value.as.native_function = native_function;
    return value;
}

/* String functions */
hyp_string_t* hyp_string_create(const char* chars) {
    if (!chars) return NULL;
    
    size_t length = strlen(chars);
    hyp_string_t* string = HYP_MALLOC(sizeof(hyp_string_t));
    if (!string) return NULL;
    
    string->chars = HYP_MALLOC(length + 1);
    if (!string->chars) {
        HYP_FREE(string);
        return NULL;
    }
    
    strcpy(string->chars, chars);
    string->length = length;
    string->hash = 0; /* TODO: Implement hash function */
    
    return string;
}

void hyp_string_destroy(hyp_string_t* string) {
    if (!string) return;
    
    HYP_FREE(string->chars);
    HYP_FREE(string);
}

/* Array functions */
hyp_array_t* hyp_array_create(void) {
    hyp_array_t* array = HYP_MALLOC(sizeof(hyp_array_t));
    if (!array) return NULL;
    
    array->values = NULL;
    array->count = 0;
    array->capacity = 0;
    
    return array;
}

void hyp_array_destroy(hyp_array_t* array) {
    if (!array) return;
    
    HYP_FREE(array->values);
    HYP_FREE(array);
}

hyp_error_t hyp_array_push(hyp_array_t* array, hyp_value_t value) {
    if (!array) return HYP_ERROR_NULL_POINTER;
    
    if (array->count >= array->capacity) {
        size_t new_capacity = array->capacity == 0 ? 8 : array->capacity * 2;
        hyp_value_t* new_values = HYP_REALLOC(array->values, new_capacity * sizeof(hyp_value_t));
        if (!new_values) return HYP_ERROR_OUT_OF_MEMORY;
        
        array->values = new_values;
        array->capacity = new_capacity;
    }
    
    array->values[array->count++] = value;
    return HYP_OK;
}

hyp_value_t hyp_array_get(hyp_array_t* array, size_t index) {
    if (!array || index >= array->count) {
        return hyp_value_null();
    }
    
    return array->values[index];
}

hyp_error_t hyp_array_set(hyp_array_t* array, size_t index, hyp_value_t value) {
    if (!array || index >= array->count) {
        return HYP_ERROR_INDEX_OUT_OF_BOUNDS;
    }
    
    array->values[index] = value;
    return HYP_OK;
}

/* Object functions */
hyp_object_t* hyp_object_create(void) {
    hyp_object_t* object = HYP_MALLOC(sizeof(hyp_object_t));
    if (!object) return NULL;
    
    object->properties = NULL;
    object->count = 0;
    object->capacity = 0;
    
    return object;
}

void hyp_object_destroy(hyp_object_t* object) {
    if (!object) return;
    
    for (size_t i = 0; i < object->count; i++) {
        hyp_string_destroy(object->properties[i].key);
    }
    
    HYP_FREE(object->properties);
    HYP_FREE(object);
}

hyp_error_t hyp_object_set(hyp_object_t* object, const char* key, hyp_value_t value) {
    if (!object || !key) return HYP_ERROR_NULL_POINTER;
    
    /* Check if key already exists */
    for (size_t i = 0; i < object->count; i++) {
        if (strcmp(object->properties[i].key->chars, key) == 0) {
            object->properties[i].value = value;
            return HYP_OK;
        }
    }
    
    /* Add new property */
    if (object->count >= object->capacity) {
        size_t new_capacity = object->capacity == 0 ? 8 : object->capacity * 2;
        hyp_object_property_t* new_properties = HYP_REALLOC(object->properties, 
                                                            new_capacity * sizeof(hyp_object_property_t));
        if (!new_properties) return HYP_ERROR_OUT_OF_MEMORY;
        
        object->properties = new_properties;
        object->capacity = new_capacity;
    }
    
    hyp_string_t* key_string = hyp_string_create(key);
    if (!key_string) return HYP_ERROR_OUT_OF_MEMORY;
    
    object->properties[object->count].key = key_string;
    object->properties[object->count].value = value;
    object->count++;
    
    return HYP_OK;
}

hyp_value_t hyp_object_get(hyp_object_t* object, const char* key) {
    if (!object || !key) return hyp_value_null();
    
    for (size_t i = 0; i < object->count; i++) {
        if (strcmp(object->properties[i].key->chars, key) == 0) {
            return object->properties[i].value;
        }
    }
    
    return hyp_value_null();
}

/* Environment functions */
hyp_environment_t* hyp_environment_create(hyp_environment_t* parent) {
    hyp_environment_t* env = HYP_MALLOC(sizeof(hyp_environment_t));
    if (!env) return NULL;
    
    env->parent = parent;
    env->variables = hyp_object_create();
    if (!env->variables) {
        HYP_FREE(env);
        return NULL;
    }
    
    return env;
}

void hyp_environment_destroy(hyp_environment_t* env) {
    if (!env) return;
    
    hyp_object_destroy(env->variables);
    HYP_FREE(env);
}

hyp_error_t hyp_environment_define(hyp_environment_t* env, const char* name, hyp_value_t value) {
    if (!env || !name) return HYP_ERROR_NULL_POINTER;
    
    return hyp_object_set(env->variables, name, value);
}

hyp_value_t hyp_environment_get(hyp_environment_t* env, const char* name) {
    if (!env || !name) return hyp_value_null();
    
    hyp_value_t value = hyp_object_get(env->variables, name);
    if (value.type != HYP_VALUE_NULL || !env->parent) {
        return value;
    }
    
    return hyp_environment_get(env->parent, name);
}

hyp_error_t hyp_environment_assign(hyp_environment_t* env, const char* name, hyp_value_t value) {
    if (!env || !name) return HYP_ERROR_NULL_POINTER;
    
    /* Check if variable exists in current environment */
    hyp_value_t existing = hyp_object_get(env->variables, name);
    if (existing.type != HYP_VALUE_NULL) {
        return hyp_object_set(env->variables, name, value);
    }
    
    /* Check parent environments */
    if (env->parent) {
        return hyp_environment_assign(env->parent, name, value);
    }
    
    /* Variable not found, define it in current environment */
    return hyp_environment_define(env, name, value);
}

/* Runtime functions */
hyp_runtime_t* hyp_runtime_create(void) {
    hyp_runtime_t* runtime = HYP_MALLOC(sizeof(hyp_runtime_t));
    if (!runtime) return NULL;
    
    runtime->global_env = hyp_environment_create(NULL);
    if (!runtime->global_env) {
        HYP_FREE(runtime);
        return NULL;
    }
    
    runtime->current_env = runtime->global_env;
    runtime->call_stack = NULL;
    runtime->stack_size = 0;
    runtime->stack_capacity = 0;
    runtime->error = NULL;
    runtime->modules = hyp_object_create();
    
    if (!runtime->modules) {
        hyp_environment_destroy(runtime->global_env);
        HYP_FREE(runtime);
        return NULL;
    }
    
    /* Define built-in functions */
    hyp_native_function_t* print_fn = HYP_MALLOC(sizeof(hyp_native_function_t));
    if (print_fn) {
        print_fn->name = "print";
        print_fn->function = builtin_print;
        hyp_environment_define(runtime->global_env, "print", hyp_value_native_function(print_fn));
    }
    
    hyp_native_function_t* typeof_fn = HYP_MALLOC(sizeof(hyp_native_function_t));
    if (typeof_fn) {
        typeof_fn->name = "typeof";
        typeof_fn->function = builtin_typeof;
        hyp_environment_define(runtime->global_env, "typeof", hyp_value_native_function(typeof_fn));
    }
    
    hyp_native_function_t* len_fn = HYP_MALLOC(sizeof(hyp_native_function_t));
    if (len_fn) {
        len_fn->name = "len";
        len_fn->function = builtin_len;
        hyp_environment_define(runtime->global_env, "len", hyp_value_native_function(len_fn));
    }
    
    return runtime;
}

void hyp_runtime_destroy(hyp_runtime_t* runtime) {
    if (!runtime) return;
    
    hyp_environment_destroy(runtime->global_env);
    hyp_object_destroy(runtime->modules);
    HYP_FREE(runtime->call_stack);
    HYP_FREE(runtime);
}

/* Value comparison */
bool hyp_value_equals(hyp_value_t a, hyp_value_t b) {
    if (a.type != b.type) return false;
    
    switch (a.type) {
        case HYP_VALUE_NULL:
            return true;
        case HYP_VALUE_BOOLEAN:
            return a.as.boolean == b.as.boolean;
        case HYP_VALUE_NUMBER:
            return a.as.number == b.as.number;
        case HYP_VALUE_STRING:
            return a.as.string == b.as.string || 
                   (a.as.string && b.as.string && 
                    strcmp(a.as.string->chars, b.as.string->chars) == 0);
        default:
            return a.as.object == b.as.object; /* Pointer comparison for objects */
    }
}

/* Value truthiness */
bool hyp_value_is_truthy(hyp_value_t value) {
    switch (value.type) {
        case HYP_VALUE_NULL:
            return false;
        case HYP_VALUE_BOOLEAN:
            return value.as.boolean;
        case HYP_VALUE_NUMBER:
            return value.as.number != 0.0 && !isnan(value.as.number);
        case HYP_VALUE_STRING:
            return value.as.string && value.as.string->length > 0;
        default:
            return true; /* Objects, arrays, functions are truthy */
    }
}

/* AST evaluation */
static hyp_value_t evaluate_literal(hyp_runtime_t* runtime, hyp_ast_node_t* node) {
    switch (node->as.literal.type) {
        case HYP_LITERAL_NULL:
            return hyp_value_null();
        case HYP_LITERAL_BOOLEAN:
            return hyp_value_boolean(node->as.literal.as.boolean);
        case HYP_LITERAL_NUMBER:
            return hyp_value_number(node->as.literal.as.number);
        case HYP_LITERAL_STRING:
            return hyp_value_string(hyp_string_create(node->as.literal.as.string));
        default:
            runtime->error = "Unknown literal type";
            return hyp_value_null();
    }
}

static hyp_value_t evaluate_identifier(hyp_runtime_t* runtime, hyp_ast_node_t* node) {
    return hyp_environment_get(runtime->current_env, node->as.identifier.name);
}

static hyp_value_t evaluate_binary(hyp_runtime_t* runtime, hyp_ast_node_t* node) {
    hyp_value_t left = hyp_runtime_evaluate_ast(runtime, node->as.binary.left);
    if (runtime->error) return hyp_value_null();
    
    hyp_value_t right = hyp_runtime_evaluate_ast(runtime, node->as.binary.right);
    if (runtime->error) return hyp_value_null();
    
    switch (node->as.binary.operator) {
        case HYP_BINARY_ADD:
            if (left.type == HYP_VALUE_NUMBER && right.type == HYP_VALUE_NUMBER) {
                return hyp_value_number(left.as.number + right.as.number);
            }
            runtime->error = "Invalid operands for addition";
            return hyp_value_null();
            
        case HYP_BINARY_SUBTRACT:
            if (left.type == HYP_VALUE_NUMBER && right.type == HYP_VALUE_NUMBER) {
                return hyp_value_number(left.as.number - right.as.number);
            }
            runtime->error = "Invalid operands for subtraction";
            return hyp_value_null();
            
        case HYP_BINARY_MULTIPLY:
            if (left.type == HYP_VALUE_NUMBER && right.type == HYP_VALUE_NUMBER) {
                return hyp_value_number(left.as.number * right.as.number);
            }
            runtime->error = "Invalid operands for multiplication";
            return hyp_value_null();
            
        case HYP_BINARY_DIVIDE:
            if (left.type == HYP_VALUE_NUMBER && right.type == HYP_VALUE_NUMBER) {
                if (right.as.number == 0.0) {
                    runtime->error = "Division by zero";
                    return hyp_value_null();
                }
                return hyp_value_number(left.as.number / right.as.number);
            }
            runtime->error = "Invalid operands for division";
            return hyp_value_null();
            
        case HYP_BINARY_EQUAL:
            return hyp_value_boolean(hyp_value_equals(left, right));
            
        case HYP_BINARY_NOT_EQUAL:
            return hyp_value_boolean(!hyp_value_equals(left, right));
            
        case HYP_BINARY_LESS:
            if (left.type == HYP_VALUE_NUMBER && right.type == HYP_VALUE_NUMBER) {
                return hyp_value_boolean(left.as.number < right.as.number);
            }
            runtime->error = "Invalid operands for comparison";
            return hyp_value_null();
            
        case HYP_BINARY_LESS_EQUAL:
            if (left.type == HYP_VALUE_NUMBER && right.type == HYP_VALUE_NUMBER) {
                return hyp_value_boolean(left.as.number <= right.as.number);
            }
            runtime->error = "Invalid operands for comparison";
            return hyp_value_null();
            
        case HYP_BINARY_GREATER:
            if (left.type == HYP_VALUE_NUMBER && right.type == HYP_VALUE_NUMBER) {
                return hyp_value_boolean(left.as.number > right.as.number);
            }
            runtime->error = "Invalid operands for comparison";
            return hyp_value_null();
            
        case HYP_BINARY_GREATER_EQUAL:
            if (left.type == HYP_VALUE_NUMBER && right.type == HYP_VALUE_NUMBER) {
                return hyp_value_boolean(left.as.number >= right.as.number);
            }
            runtime->error = "Invalid operands for comparison";
            return hyp_value_null();
            
        case HYP_BINARY_LOGICAL_AND:
            if (!hyp_value_is_truthy(left)) {
                return left;
            }
            return right;
            
        case HYP_BINARY_LOGICAL_OR:
            if (hyp_value_is_truthy(left)) {
                return left;
            }
            return right;
            
        default:
            runtime->error = "Unknown binary operator";
            return hyp_value_null();
    }
}

hyp_value_t hyp_runtime_evaluate_ast(hyp_runtime_t* runtime, hyp_ast_node_t* node) {
    if (!runtime || !node) {
        if (runtime) runtime->error = "Null AST node";
        return hyp_value_null();
    }
    
    switch (node->type) {
        case HYP_AST_LITERAL:
            return evaluate_literal(runtime, node);
        case HYP_AST_IDENTIFIER:
            return evaluate_identifier(runtime, node);
        case HYP_AST_BINARY:
            return evaluate_binary(runtime, node);
        default:
            runtime->error = "Unsupported AST node type for evaluation";
            return hyp_value_null();
    }
}

hyp_error_t hyp_runtime_execute_ast(hyp_runtime_t* runtime, hyp_ast_node_t* ast) {
    if (!runtime || !ast) return HYP_ERROR_NULL_POINTER;
    
    runtime->error = NULL;
    hyp_value_t result = hyp_runtime_evaluate_ast(runtime, ast);
    
    if (runtime->error) {
        return HYP_ERROR_RUNTIME;
    }
    
    return HYP_OK;
}

/* Placeholder implementations for remaining functions */
hyp_error_t hyp_runtime_execute_bytecode(hyp_runtime_t* runtime, hyp_bytecode_t* bytecode) {
    /* TODO: Implement bytecode execution */
    return HYP_ERROR_NOT_IMPLEMENTED;
}

hyp_error_t hyp_runtime_load_module(hyp_runtime_t* runtime, const char* name, const char* path) {
    /* TODO: Implement module loading */
    return HYP_ERROR_NOT_IMPLEMENTED;
}

hyp_error_t hyp_runtime_call_function(hyp_runtime_t* runtime, hyp_value_t function, hyp_value_t* args, size_t arg_count, hyp_value_t* result) {
    /* TODO: Implement function calling */
    return HYP_ERROR_NOT_IMPLEMENTED;
}

void hyp_runtime_collect_garbage(hyp_runtime_t* runtime) {
    /* TODO: Implement garbage collection */
}

const char* hyp_runtime_get_error(hyp_runtime_t* runtime) {
    return runtime ? runtime->error : "Runtime is null";
}

void hyp_runtime_clear_error(hyp_runtime_t* runtime) {
    if (runtime) {
        runtime->error = NULL;
    }
}