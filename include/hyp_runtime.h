/**
 * Hyper Programming Language - Runtime System
 * 
 * The runtime system provides the core execution environment for Hyper
 * programs, including state management, event handling, and I/O operations.
 */

#ifndef HYP_RUNTIME_H
#define HYP_RUNTIME_H

#include "hyp_common.h"
#include "transpiler.h"

/* Forward declarations */
typedef struct hyp_runtime hyp_runtime_t;
typedef struct hyp_value hyp_value_t;
typedef struct hyp_object hyp_object_t;
typedef struct hyp_function hyp_function_t;

/* Value types in the runtime */
typedef enum {
    HYP_VAL_NULL,
    HYP_VAL_BOOLEAN,
    HYP_VAL_NUMBER,
    HYP_VAL_STRING,
    HYP_VAL_ARRAY,
    HYP_VAL_OBJECT,
    HYP_VAL_FUNCTION,
    HYP_VAL_NATIVE_FUNCTION
} hyp_value_type_t;

/* Runtime value structure */
struct hyp_value {
    hyp_value_type_t type;
    union {
        bool boolean;
        double number;
        char* string;
        struct {
            hyp_value_t* elements;
            size_t count;
            size_t capacity;
        } array;
        hyp_object_t* object;
        hyp_function_t* function;
        struct {
            const char* name;
            hyp_value_t (*native_fn)(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count);
        } native_function;
    };
};

/* Object property */
typedef struct {
    char* key;
    hyp_value_t value;
} hyp_property_t;

/* Runtime object */
struct hyp_object {
    hyp_property_t* properties;
    size_t count;
    size_t capacity;
    struct hyp_object* prototype;
};

/* Runtime function */
struct hyp_function {
    char* name;
    hyp_parameter_array_t parameters;
    hyp_ast_node_t* body;
    struct hyp_environment* closure;
};

/* Environment for variable scoping */
typedef struct hyp_environment {
    struct {
        char** names;
        hyp_value_t* values;
        size_t count;
        size_t capacity;
    } variables;
    struct hyp_environment* parent;
} hyp_environment_t;

/* Call frame for function calls */
typedef struct {
    hyp_function_t* function;
    hyp_environment_t* environment;
    hyp_ast_node_t* return_address;
    size_t stack_base;
} hyp_call_frame_t;

/* Runtime stack */
typedef HYP_ARRAY(hyp_value_t) hyp_stack_t;
typedef HYP_ARRAY(hyp_call_frame_t) hyp_call_stack_t;

/* Runtime execution modes */
typedef enum {
    HYP_MODE_INTERPRET,  /* Direct AST interpretation */
    HYP_MODE_BYTECODE,   /* Bytecode execution */
    HYP_MODE_COMPILED    /* Execute compiled code */
} hyp_runtime_mode_t;

/* Runtime configuration */
typedef struct {
    hyp_runtime_mode_t mode;
    size_t stack_size;
    size_t heap_size;
    bool debug_mode;
    bool gc_enabled;
    const char* module_paths[16];
    size_t module_path_count;
} hyp_runtime_config_t;

/* Main runtime structure */
struct hyp_runtime {
    hyp_runtime_config_t config;
    hyp_runtime_mode_t mode;
    
    /* Execution state */
    hyp_stack_t stack;
    hyp_call_stack_t call_stack;
    hyp_environment_t* global_env;
    hyp_environment_t* current_env;
    
    /* Memory management */
    hyp_arena_t* arena;
    struct {
        hyp_value_t* objects;
        size_t count;
        size_t capacity;
        bool* marked;
    } gc;
    
    /* Module system */
    struct {
        char** names;
        hyp_value_t* exports;
        size_t count;
        size_t capacity;
    } modules;
    
    /* Built-in functions */
    struct {
        const char** names;
        hyp_value_t (**functions)(hyp_runtime_t*, hyp_value_t*, size_t);
        size_t count;
    } builtins;
    
    /* Error handling */
    bool has_error;
    char error_message[256];
    hyp_ast_node_t* error_location;
    
    /* Bytecode execution (if in bytecode mode) */
    hyp_bytecode_t* bytecode;
    size_t pc;  /* Program counter */
    
    /* Event system */
    struct {
        void** handlers;
        char** event_names;
        size_t count;
        size_t capacity;
    } events;
};

/* Function declarations */

/**
 * Create a new runtime instance
 * @return New runtime instance, or NULL on failure
 */
hyp_runtime_t* hyp_runtime_create(void);

/**
 * Initialize runtime with configuration
 * @param runtime The runtime to initialize
 * @param config Runtime configuration
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hyp_runtime_init(hyp_runtime_t* runtime, const hyp_runtime_config_t* config);

/**
 * Execute an AST program
 * @param runtime The runtime instance
 * @param ast The program AST to execute
 * @return Execution result value
 */
hyp_value_t hyp_runtime_execute(hyp_runtime_t* runtime, hyp_ast_node_t* ast);

/**
 * Execute bytecode
 * @param runtime The runtime instance
 * @param bytecode The bytecode to execute
 * @return Execution result value
 */
hyp_value_t hyp_runtime_execute_bytecode(hyp_runtime_t* runtime, hyp_bytecode_t* bytecode);

/**
 * Load and execute a module
 * @param runtime The runtime instance
 * @param module_name Name of the module to load
 * @return Module exports object
 */
hyp_value_t hyp_runtime_load_module(hyp_runtime_t* runtime, const char* module_name);

/* Value operations */
hyp_value_t hyp_value_null(void);
hyp_value_t hyp_value_boolean(bool value);
hyp_value_t hyp_value_number(double value);
hyp_value_t hyp_value_string(const char* value);
hyp_value_t hyp_value_array(size_t capacity);
hyp_value_t hyp_value_object(void);
hyp_value_t hyp_value_function(hyp_function_t* function);
hyp_value_t hyp_value_native_function(const char* name, hyp_value_t (*fn)(hyp_runtime_t*, hyp_value_t*, size_t));

/* Value utilities */
bool hyp_value_is_truthy(hyp_value_t value);
bool hyp_value_equals(hyp_value_t a, hyp_value_t b);
char* hyp_value_to_string(hyp_value_t value);
void hyp_value_print(hyp_value_t value);
void hyp_value_free(hyp_value_t value);

/* Array operations */
void hyp_array_push(hyp_value_t* array, hyp_value_t value);
hyp_value_t hyp_array_get(hyp_value_t* array, size_t index);
void hyp_array_set(hyp_value_t* array, size_t index, hyp_value_t value);
size_t hyp_array_length(hyp_value_t* array);

/* Object operations */
void hyp_object_set(hyp_object_t* object, const char* key, hyp_value_t value);
hyp_value_t hyp_object_get(hyp_object_t* object, const char* key);
bool hyp_object_has(hyp_object_t* object, const char* key);
void hyp_object_delete(hyp_object_t* object, const char* key);

/* Environment operations */
hyp_environment_t* hyp_environment_create(hyp_environment_t* parent);
void hyp_environment_define(hyp_environment_t* env, const char* name, hyp_value_t value);
hyp_value_t hyp_environment_get(hyp_environment_t* env, const char* name);
void hyp_environment_set(hyp_environment_t* env, const char* name, hyp_value_t value);
hyp_error_t hyp_environment_assign(hyp_environment_t* env, const char* name, hyp_value_t value);
void hyp_environment_destroy(hyp_environment_t* env);

/* Stack operations */
void hyp_stack_push(hyp_runtime_t* runtime, hyp_value_t value);
hyp_value_t hyp_stack_pop(hyp_runtime_t* runtime);
hyp_value_t hyp_stack_peek(hyp_runtime_t* runtime, size_t offset);

/* Function call operations */
hyp_value_t hyp_runtime_call_function(hyp_runtime_t* runtime, hyp_function_t* function, hyp_value_t* args, size_t arg_count);
hyp_value_t hyp_runtime_call_native(hyp_runtime_t* runtime, hyp_value_t (*fn)(hyp_runtime_t*, hyp_value_t*, size_t), hyp_value_t* args, size_t arg_count);

/* AST evaluation */
hyp_value_t hyp_runtime_eval_statement(hyp_runtime_t* runtime, hyp_ast_node_t* stmt);
hyp_value_t hyp_runtime_eval_expression(hyp_runtime_t* runtime, hyp_ast_node_t* expr);

/* Built-in functions */
hyp_value_t hyp_builtin_print(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count);
hyp_value_t hyp_builtin_println(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count);
hyp_value_t hyp_builtin_input(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count);
hyp_value_t hyp_builtin_typeof(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count);
hyp_value_t hyp_builtin_length(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count);
hyp_value_t hyp_builtin_parse_number(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count);
hyp_value_t hyp_builtin_to_string(hyp_runtime_t* runtime, hyp_value_t* args, size_t arg_count);

/* Register built-in functions */
void hyp_runtime_register_builtins(hyp_runtime_t* runtime);
void hyp_runtime_register_builtin(hyp_runtime_t* runtime, const char* name, hyp_value_t (*fn)(hyp_runtime_t*, hyp_value_t*, size_t));

/* Garbage collection */
void hyp_gc_mark_value(hyp_runtime_t* runtime, hyp_value_t value);
void hyp_gc_mark_object(hyp_runtime_t* runtime, hyp_object_t* object);
void hyp_gc_sweep(hyp_runtime_t* runtime);
void hyp_gc_collect(hyp_runtime_t* runtime);

/* AST execution */
hyp_error_t hyp_runtime_execute_ast(hyp_runtime_t* runtime, hyp_ast_node_t* ast);

/* Error handling */
void hyp_runtime_error(hyp_runtime_t* runtime, const char* format, ...);
const char* hyp_runtime_get_error(hyp_runtime_t* runtime);
void hyp_runtime_clear_error(hyp_runtime_t* runtime);

/* Event system */
void hyp_runtime_emit_event(hyp_runtime_t* runtime, const char* event_name, hyp_value_t data);
void hyp_runtime_on_event(hyp_runtime_t* runtime, const char* event_name, void (*handler)(hyp_value_t));

/* Module system */
hyp_error_t hyp_runtime_register_module(hyp_runtime_t* runtime, const char* name, hyp_value_t exports);
hyp_value_t hyp_runtime_get_module(hyp_runtime_t* runtime, const char* name);

/* Bytecode execution helpers */
hyp_value_t hyp_runtime_execute_instruction(hyp_runtime_t* runtime, hyp_instruction_t instruction);
void hyp_runtime_jump(hyp_runtime_t* runtime, size_t address);
void hyp_runtime_jump_if_false(hyp_runtime_t* runtime, size_t address, hyp_value_t condition);

/* Debug utilities */
void hyp_runtime_print_stack(hyp_runtime_t* runtime);
void hyp_runtime_print_environment(hyp_environment_t* env);
void hyp_runtime_trace_execution(hyp_runtime_t* runtime, bool enable);

/* Cleanup */
void hyp_runtime_destroy(hyp_runtime_t* runtime);

/* Default runtime configuration */
hyp_runtime_config_t hyp_runtime_default_config(void);

#endif /* HYP_RUNTIME_H */