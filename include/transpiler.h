/**
 * Hyper Programming Language - Transpiler
 * 
 * The transpiler converts the Abstract Syntax Tree (AST) into target
 * language code (C, JavaScript, etc.) or bytecode. It performs code
 * generation with optimizations for performance and readability.
 */

#ifndef HYP_TRANSPILER_H
#define HYP_TRANSPILER_H

#include "hyp_common.h"
#include "parser.h"

/* Target languages/formats */
typedef enum {
    TARGET_C,
    TARGET_JAVASCRIPT,
    TARGET_BYTECODE,
    TARGET_ASSEMBLY,
    TARGET_LLVM_IR
} hyp_target_t;

/* Code generation context */
typedef struct {
    hyp_target_t target;
    hyp_string_t output;
    int indent_level;
    bool optimize;
    bool debug_info;
    hyp_arena_t* arena;
    
    /* Symbol table for variable tracking */
    struct {
        char** names;
        hyp_type_t** types;
        size_t count;
        size_t capacity;
    } symbols;
    
    /* Function context */
    struct {
        char* current_function;
        hyp_type_t* return_type;
        bool in_loop;
        int loop_depth;
    } function_ctx;
    
    /* Error handling */
    bool has_error;
    char error_message[256];
} hyp_codegen_t;

/* Code generation options */
typedef struct {
    hyp_target_t target;
    bool optimize;
    bool debug_info;
    bool minify;
    const char* output_file;
    const char* include_paths[16];
    size_t include_count;
} hyp_codegen_options_t;

/* Function declarations */

/**
 * Initialize code generator
 * @param codegen The code generator to initialize
 * @param options Code generation options
 * @param arena Arena allocator for memory management
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hyp_codegen_init(hyp_codegen_t* codegen, const hyp_codegen_options_t* options, hyp_arena_t* arena);

/**
 * Generate code from AST
 * @param codegen The code generator instance
 * @param ast The root AST node
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hyp_codegen_generate(hyp_codegen_t* codegen, hyp_ast_node_t* ast);

/**
 * Get the generated code
 * @param codegen The code generator instance
 * @return Pointer to generated code string
 */
const char* hyp_codegen_get_output(hyp_codegen_t* codegen);

/* Code generation for different AST nodes */
hyp_error_t hyp_codegen_program(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_statement(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_expression(hyp_codegen_t* codegen, hyp_ast_node_t* node);

/* Statement code generation */
hyp_error_t hyp_codegen_variable_decl(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_function_decl(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_if_stmt(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_while_stmt(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_for_stmt(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_return_stmt(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_block_stmt(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_import_stmt(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_export_stmt(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_match_stmt(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_try_stmt(hyp_codegen_t* codegen, hyp_ast_node_t* node);

/* Expression code generation */
hyp_error_t hyp_codegen_binary_op(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_unary_op(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_assignment(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_call(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_member_access(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_index_access(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_conditional(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_array_literal(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_object_literal(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_lambda(hyp_codegen_t* codegen, hyp_ast_node_t* node);

/* Literal code generation */
hyp_error_t hyp_codegen_number(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_string(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_boolean(hyp_codegen_t* codegen, hyp_ast_node_t* node);
hyp_error_t hyp_codegen_identifier(hyp_codegen_t* codegen, hyp_ast_node_t* node);

/* Target-specific code generation */
hyp_error_t hyp_codegen_c_header(hyp_codegen_t* codegen);
hyp_error_t hyp_codegen_c_footer(hyp_codegen_t* codegen);
hyp_error_t hyp_codegen_js_header(hyp_codegen_t* codegen);
hyp_error_t hyp_codegen_js_footer(hyp_codegen_t* codegen);

/* Utility functions */
void hyp_codegen_emit(hyp_codegen_t* codegen, const char* format, ...);
void hyp_codegen_emit_line(hyp_codegen_t* codegen, const char* format, ...);
void hyp_codegen_emit_indent(hyp_codegen_t* codegen);
void hyp_codegen_increase_indent(hyp_codegen_t* codegen);
void hyp_codegen_decrease_indent(hyp_codegen_t* codegen);

/* Symbol table management */
hyp_error_t hyp_codegen_add_symbol(hyp_codegen_t* codegen, const char* name, hyp_type_t* type);
hyp_type_t* hyp_codegen_lookup_symbol(hyp_codegen_t* codegen, const char* name);
void hyp_codegen_clear_symbols(hyp_codegen_t* codegen);

/* Type conversion utilities */
const char* hyp_type_to_c(hyp_type_t* type);
const char* hyp_type_to_js(hyp_type_t* type);
const char* hyp_binary_op_to_c(hyp_binary_op_t op);
const char* hyp_binary_op_to_js(hyp_binary_op_t op);
const char* hyp_unary_op_to_c(hyp_unary_op_t op);
const char* hyp_unary_op_to_js(hyp_unary_op_t op);

/* String escaping */
char* hyp_escape_string_c(const char* str);
char* hyp_escape_string_js(const char* str);

/* Name mangling for C output */
char* hyp_mangle_name(const char* name);
char* hyp_mangle_function_name(const char* name, hyp_parameter_array_t* params);

/* Optimization passes */
hyp_error_t hyp_optimize_ast(hyp_ast_node_t* ast);
hyp_error_t hyp_optimize_constant_folding(hyp_ast_node_t* ast);
hyp_error_t hyp_optimize_dead_code_elimination(hyp_ast_node_t* ast);
hyp_error_t hyp_optimize_inline_functions(hyp_ast_node_t* ast);

/* Error handling */
void hyp_codegen_error(hyp_codegen_t* codegen, const char* format, ...);

/* Cleanup */
void hyp_codegen_destroy(hyp_codegen_t* codegen);

/* Template system for code generation */
typedef struct {
    const char* name;
    const char* template_str;
} hyp_template_t;

/* Built-in templates */
extern const hyp_template_t hyp_c_templates[];
extern const hyp_template_t hyp_js_templates[];
extern const size_t hyp_c_template_count;
extern const size_t hyp_js_template_count;

/* Template processing */
char* hyp_process_template(const char* template_str, const char** replacements, size_t replacement_count);

/* Bytecode generation (for interpreter) */
typedef enum {
    OP_LOAD_CONST,
    OP_LOAD_VAR,
    OP_STORE_VAR,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_CALL,
    OP_RETURN,
    OP_HALT
} hyp_opcode_t;

typedef struct {
    hyp_opcode_t opcode;
    uint32_t operand;
} hyp_instruction_t;

typedef HYP_ARRAY(hyp_instruction_t) hyp_bytecode_t;

/* Bytecode generation */
hyp_error_t hyp_generate_bytecode(hyp_ast_node_t* ast, hyp_bytecode_t* bytecode);
void hyp_bytecode_emit(hyp_bytecode_t* bytecode, hyp_opcode_t opcode, uint32_t operand);
void hyp_bytecode_print(const hyp_bytecode_t* bytecode);
void hyp_bytecode_free(hyp_bytecode_t* bytecode);

#endif /* HYP_TRANSPILER_H */