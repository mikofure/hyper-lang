/**
 * Hyper Programming Language - Transpiler Implementation
 * 
 * Generates target code (C, JavaScript, bytecode, etc.) from AST nodes.
 * Supports multiple output targets with optimizations and symbol management.
 */

#include "../../include/transpiler.h"
#include "../../include/parser.h"
#include "../../include/hyp_common.h"
#include <string.h>
#include <stdarg.h>

/* Symbol table implementation */
static hyp_symbol_t* symbol_table_lookup(hyp_codegen_t* codegen, const char* name) {
    for (size_t i = 0; i < codegen->symbol_table.count; i++) {
        hyp_symbol_t* symbol = codegen->symbol_table.data[i];
        if (strcmp(symbol->name, name) == 0) {
            return symbol;
        }
    }
    return NULL;
}

static void symbol_table_add(hyp_codegen_t* codegen, const char* name, hyp_symbol_type_t type) {
    hyp_symbol_t* symbol = hyp_arena_alloc(codegen->arena, sizeof(hyp_symbol_t));
    if (!symbol) return;
    
    symbol->name = name;
    symbol->type = type;
    symbol->scope_depth = codegen->scope_depth;
    
    HYP_ARRAY_PUSH(codegen->symbol_table, symbol);
}

/* Code emission helpers */
static void emit(hyp_codegen_t* codegen, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    
    hyp_string_append(&codegen->output, buffer);
    
    va_end(args);
}

static void emit_line(hyp_codegen_t* codegen, const char* format, ...) {
    /* Add indentation */
    for (int i = 0; i < codegen->indent_level; i++) {
        hyp_string_append(&codegen->output, "    ");
    }
    
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    
    hyp_string_append(&codegen->output, buffer);
    hyp_string_append(&codegen->output, "\n");
    
    va_end(args);
}

static void emit_indent(hyp_codegen_t* codegen) {
    codegen->indent_level++;
}

static void emit_dedent(hyp_codegen_t* codegen) {
    if (codegen->indent_level > 0) {
        codegen->indent_level--;
    }
}

/* C code generation */
static void generate_c_literal(hyp_codegen_t* codegen, hyp_literal_t* literal) {
    switch (literal->type) {
        case HYP_LITERAL_NULL:
            emit(codegen, "NULL");
            break;
        case HYP_LITERAL_BOOLEAN:
            emit(codegen, literal->as.boolean ? "true" : "false");
            break;
        case HYP_LITERAL_NUMBER:
            emit(codegen, "%.17g", literal->as.number);
            break;
        case HYP_LITERAL_STRING:
            emit(codegen, "\"%s\"", literal->as.string ? literal->as.string : "");
            break;
    }
}

static void generate_c_binary(hyp_codegen_t* codegen, hyp_binary_t* binary) {
    emit(codegen, "(");
    hyp_codegen_generate_node(codegen, binary->left);
    
    switch (binary->operator) {
        case HYP_BINARY_ADD: emit(codegen, " + "); break;
        case HYP_BINARY_SUBTRACT: emit(codegen, " - "); break;
        case HYP_BINARY_MULTIPLY: emit(codegen, " * "); break;
        case HYP_BINARY_DIVIDE: emit(codegen, " / "); break;
        case HYP_BINARY_MODULO: emit(codegen, " %% "); break;
        case HYP_BINARY_EQUAL: emit(codegen, " == "); break;
        case HYP_BINARY_NOT_EQUAL: emit(codegen, " != "); break;
        case HYP_BINARY_LESS: emit(codegen, " < "); break;
        case HYP_BINARY_LESS_EQUAL: emit(codegen, " <= "); break;
        case HYP_BINARY_GREATER: emit(codegen, " > "); break;
        case HYP_BINARY_GREATER_EQUAL: emit(codegen, " >= "); break;
        case HYP_BINARY_AND: emit(codegen, " && "); break;
        case HYP_BINARY_OR: emit(codegen, " || "); break;
    }
    
    hyp_codegen_generate_node(codegen, binary->right);
    emit(codegen, ")");
}

static void generate_c_unary(hyp_codegen_t* codegen, hyp_unary_t* unary) {
    switch (unary->operator) {
        case HYP_UNARY_NOT:
            emit(codegen, "!");
            break;
        case HYP_UNARY_NEGATE:
            emit(codegen, "-");
            break;
    }
    
    emit(codegen, "(");
    hyp_codegen_generate_node(codegen, unary->operand);
    emit(codegen, ")");
}

static void generate_c_identifier(hyp_codegen_t* codegen, hyp_identifier_t* identifier) {
    emit(codegen, "%s", identifier->name ? identifier->name : "unknown");
}

static void generate_c_call(hyp_codegen_t* codegen, hyp_call_t* call) {
    hyp_codegen_generate_node(codegen, call->callee);
    emit(codegen, "(");
    
    for (size_t i = 0; i < call->arguments.count; i++) {
        if (i > 0) emit(codegen, ", ");
        hyp_codegen_generate_node(codegen, call->arguments.data[i]);
    }
    
    emit(codegen, ")");
}

static void generate_c_assignment(hyp_codegen_t* codegen, hyp_assignment_t* assignment) {
    hyp_codegen_generate_node(codegen, assignment->target);
    
    switch (assignment->operator) {
        case HYP_ASSIGN_SIMPLE: emit(codegen, " = "); break;
        case HYP_ASSIGN_ADD: emit(codegen, " += "); break;
        case HYP_ASSIGN_SUBTRACT: emit(codegen, " -= "); break;
        case HYP_ASSIGN_MULTIPLY: emit(codegen, " *= "); break;
        case HYP_ASSIGN_DIVIDE: emit(codegen, " /= "); break;
    }
    
    hyp_codegen_generate_node(codegen, assignment->value);
}

static void generate_c_var_decl(hyp_codegen_t* codegen, hyp_var_decl_t* var_decl) {
    /* Add to symbol table */
    symbol_table_add(codegen, var_decl->name, HYP_SYMBOL_VARIABLE);
    
    if (var_decl->is_const) {
        emit_line(codegen, "const hyp_value_t %s", var_decl->name);
    } else {
        emit_line(codegen, "hyp_value_t %s", var_decl->name);
    }
    
    if (var_decl->initializer) {
        emit(codegen, " = ");
        hyp_codegen_generate_node(codegen, var_decl->initializer);
    } else {
        emit(codegen, " = hyp_value_null()");
    }
    
    emit(codegen, ";");
}

static void generate_c_function(hyp_codegen_t* codegen, hyp_function_t* function) {
    /* Add to symbol table */
    symbol_table_add(codegen, function->name, HYP_SYMBOL_FUNCTION);
    
    emit_line(codegen, "hyp_value_t %s(", function->name);
    
    /* Parameters */
    for (size_t i = 0; i < function->parameters.count; i++) {
        if (i > 0) emit(codegen, ", ");
        hyp_parameter_t* param = function->parameters.data[i];
        emit(codegen, "hyp_value_t %s", param->name);
    }
    
    emit(codegen, ") {");
    emit_indent(codegen);
    
    /* Function body */
    codegen->scope_depth++;
    hyp_codegen_generate_node(codegen, function->body);
    codegen->scope_depth--;
    
    emit_dedent(codegen);
    emit_line(codegen, "}");
}

static void generate_c_if(hyp_codegen_t* codegen, hyp_if_t* if_stmt) {
    emit_line(codegen, "if (hyp_value_is_truthy(");
    hyp_codegen_generate_node(codegen, if_stmt->condition);
    emit(codegen, ")) {");
    
    emit_indent(codegen);
    hyp_codegen_generate_node(codegen, if_stmt->then_branch);
    emit_dedent(codegen);
    
    if (if_stmt->else_branch) {
        emit_line(codegen, "} else {");
        emit_indent(codegen);
        hyp_codegen_generate_node(codegen, if_stmt->else_branch);
        emit_dedent(codegen);
    }
    
    emit_line(codegen, "}");
}

static void generate_c_while(hyp_codegen_t* codegen, hyp_while_t* while_stmt) {
    emit_line(codegen, "while (hyp_value_is_truthy(");
    hyp_codegen_generate_node(codegen, while_stmt->condition);
    emit(codegen, ")) {");
    
    emit_indent(codegen);
    hyp_codegen_generate_node(codegen, while_stmt->body);
    emit_dedent(codegen);
    
    emit_line(codegen, "}");
}

static void generate_c_return(hyp_codegen_t* codegen, hyp_return_t* return_stmt) {
    emit_line(codegen, "return ");
    
    if (return_stmt->value) {
        hyp_codegen_generate_node(codegen, return_stmt->value);
    } else {
        emit(codegen, "hyp_value_null()");
    }
    
    emit(codegen, ";");
}

static void generate_c_block(hyp_codegen_t* codegen, hyp_block_t* block) {
    codegen->scope_depth++;
    
    for (size_t i = 0; i < block->statements.count; i++) {
        hyp_codegen_generate_node(codegen, block->statements.data[i]);
    }
    
    codegen->scope_depth--;
}

static void generate_c_expression_stmt(hyp_codegen_t* codegen, hyp_expression_stmt_t* expr_stmt) {
    emit_line(codegen, "");
    hyp_codegen_generate_node(codegen, expr_stmt->expression);
    emit(codegen, ";");
}

static void generate_c_program(hyp_codegen_t* codegen, hyp_program_t* program) {
    /* Generate includes */
    emit_line(codegen, "#include <stdio.h>");
    emit_line(codegen, "#include <stdlib.h>");
    emit_line(codegen, "#include <stdbool.h>");
    emit_line(codegen, "#include <string.h>");
    emit_line(codegen, "#include \"hyp_runtime.h\"");
    emit_line(codegen, "");
    
    /* Generate declarations */
    for (size_t i = 0; i < program->declarations.count; i++) {
        hyp_codegen_generate_node(codegen, program->declarations.data[i]);
        emit_line(codegen, "");
    }
    
    /* Generate main function if not present */
    if (!symbol_table_lookup(codegen, "main")) {
        emit_line(codegen, "int main(int argc, char* argv[]) {");
        emit_indent(codegen);
        emit_line(codegen, "hyp_runtime_t* runtime = hyp_runtime_create();");
        emit_line(codegen, "if (!runtime) return 1;");
        emit_line(codegen, "");
        emit_line(codegen, "/* Call user code here */");
        emit_line(codegen, "");
        emit_line(codegen, "hyp_runtime_destroy(runtime);");
        emit_line(codegen, "return 0;");
        emit_dedent(codegen);
        emit_line(codegen, "}");
    }
}

/* JavaScript code generation */
static void generate_js_literal(hyp_codegen_t* codegen, hyp_literal_t* literal) {
    switch (literal->type) {
        case HYP_LITERAL_NULL:
            emit(codegen, "null");
            break;
        case HYP_LITERAL_BOOLEAN:
            emit(codegen, literal->as.boolean ? "true" : "false");
            break;
        case HYP_LITERAL_NUMBER:
            emit(codegen, "%.17g", literal->as.number);
            break;
        case HYP_LITERAL_STRING:
            emit(codegen, "\"%s\"", literal->as.string ? literal->as.string : "");
            break;
    }
}

static void generate_js_binary(hyp_codegen_t* codegen, hyp_binary_t* binary) {
    emit(codegen, "(");
    hyp_codegen_generate_node(codegen, binary->left);
    
    switch (binary->operator) {
        case HYP_BINARY_ADD: emit(codegen, " + "); break;
        case HYP_BINARY_SUBTRACT: emit(codegen, " - "); break;
        case HYP_BINARY_MULTIPLY: emit(codegen, " * "); break;
        case HYP_BINARY_DIVIDE: emit(codegen, " / "); break;
        case HYP_BINARY_MODULO: emit(codegen, " %% "); break;
        case HYP_BINARY_EQUAL: emit(codegen, " === "); break;
        case HYP_BINARY_NOT_EQUAL: emit(codegen, " !== "); break;
        case HYP_BINARY_LESS: emit(codegen, " < "); break;
        case HYP_BINARY_LESS_EQUAL: emit(codegen, " <= "); break;
        case HYP_BINARY_GREATER: emit(codegen, " > "); break;
        case HYP_BINARY_GREATER_EQUAL: emit(codegen, " >= "); break;
        case HYP_BINARY_AND: emit(codegen, " && "); break;
        case HYP_BINARY_OR: emit(codegen, " || "); break;
    }
    
    hyp_codegen_generate_node(codegen, binary->right);
    emit(codegen, ")");
}

static void generate_js_function(hyp_codegen_t* codegen, hyp_function_t* function) {
    emit_line(codegen, "function %s(", function->name);
    
    /* Parameters */
    for (size_t i = 0; i < function->parameters.count; i++) {
        if (i > 0) emit(codegen, ", ");
        hyp_parameter_t* param = function->parameters.data[i];
        emit(codegen, "%s", param->name);
    }
    
    emit(codegen, ") {");
    emit_indent(codegen);
    
    /* Function body */
    hyp_codegen_generate_node(codegen, function->body);
    
    emit_dedent(codegen);
    emit_line(codegen, "}");
}

/* Main code generation dispatch */
void hyp_codegen_generate_node(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    if (!codegen || !node) return;
    
    switch (codegen->target) {
        case HYP_TARGET_C:
            switch (node->type) {
                case HYP_AST_LITERAL:
                    generate_c_literal(codegen, &node->as.literal);
                    break;
                case HYP_AST_BINARY:
                    generate_c_binary(codegen, &node->as.binary);
                    break;
                case HYP_AST_UNARY:
                    generate_c_unary(codegen, &node->as.unary);
                    break;
                case HYP_AST_IDENTIFIER:
                    generate_c_identifier(codegen, &node->as.identifier);
                    break;
                case HYP_AST_CALL:
                    generate_c_call(codegen, &node->as.call);
                    break;
                case HYP_AST_ASSIGNMENT:
                    generate_c_assignment(codegen, &node->as.assignment);
                    break;
                case HYP_AST_VAR_DECL:
                    generate_c_var_decl(codegen, &node->as.var_decl);
                    break;
                case HYP_AST_FUNCTION:
                    generate_c_function(codegen, &node->as.function);
                    break;
                case HYP_AST_IF:
                    generate_c_if(codegen, &node->as.if_stmt);
                    break;
                case HYP_AST_WHILE:
                    generate_c_while(codegen, &node->as.while_stmt);
                    break;
                case HYP_AST_RETURN:
                    generate_c_return(codegen, &node->as.return_stmt);
                    break;
                case HYP_AST_BLOCK:
                    generate_c_block(codegen, &node->as.block);
                    break;
                case HYP_AST_EXPRESSION_STMT:
                    generate_c_expression_stmt(codegen, &node->as.expression_stmt);
                    break;
                case HYP_AST_PROGRAM:
                    generate_c_program(codegen, &node->as.program);
                    break;
                default:
                    break;
            }
            break;
            
        case HYP_TARGET_JAVASCRIPT:
            switch (node->type) {
                case HYP_AST_LITERAL:
                    generate_js_literal(codegen, &node->as.literal);
                    break;
                case HYP_AST_BINARY:
                    generate_js_binary(codegen, &node->as.binary);
                    break;
                case HYP_AST_FUNCTION:
                    generate_js_function(codegen, &node->as.function);
                    break;
                /* Add more JavaScript cases as needed */
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}

/* Public API */
hyp_codegen_t* hyp_codegen_create(hyp_target_t target, hyp_codegen_options_t* options) {
    hyp_codegen_t* codegen = HYP_MALLOC(sizeof(hyp_codegen_t));
    if (!codegen) return NULL;
    
    codegen->target = target;
    codegen->options = options ? *options : (hyp_codegen_options_t){0};
    codegen->output = hyp_string_create("");
    codegen->indent_level = 0;
    codegen->scope_depth = 0;
    
    HYP_ARRAY_INIT(codegen->symbol_table);
    
    codegen->arena = hyp_arena_create(8192);
    if (!codegen->arena) {
        hyp_string_destroy(&codegen->output);
        HYP_FREE(codegen);
        return NULL;
    }
    
    return codegen;
}

void hyp_codegen_destroy(hyp_codegen_t* codegen) {
    if (!codegen) return;
    
    hyp_string_destroy(&codegen->output);
    
    if (codegen->arena) {
        hyp_arena_destroy(codegen->arena);
    }
    
    HYP_FREE(codegen);
}

hyp_error_t hyp_codegen_generate(hyp_codegen_t* codegen, hyp_ast_node_t* ast) {
    if (!codegen || !ast) return HYP_ERROR_INVALID_ARGUMENT;
    
    /* Reset output */
    hyp_string_destroy(&codegen->output);
    codegen->output = hyp_string_create("");
    codegen->indent_level = 0;
    codegen->scope_depth = 0;
    
    /* Clear symbol table */
    codegen->symbol_table.count = 0;
    
    /* Generate code */
    hyp_codegen_generate_node(codegen, ast);
    
    return HYP_OK;
}

const char* hyp_codegen_get_output(hyp_codegen_t* codegen) {
    return codegen ? codegen->output.data : NULL;
}

size_t hyp_codegen_get_output_length(hyp_codegen_t* codegen) {
    return codegen ? codegen->output.length : 0;
}

/* Utility functions */
const char* hyp_target_name(hyp_target_t target) {
    switch (target) {
        case HYP_TARGET_C: return "C";
        case HYP_TARGET_JAVASCRIPT: return "JavaScript";
        case HYP_TARGET_BYTECODE: return "Bytecode";
        case HYP_TARGET_ASSEMBLY: return "Assembly";
        case HYP_TARGET_LLVM_IR: return "LLVM IR";
        default: return "Unknown";
    }
}

hyp_error_t hyp_codegen_write_to_file(hyp_codegen_t* codegen, const char* filename) {
    if (!codegen || !filename) return HYP_ERROR_INVALID_ARGUMENT;
    
    return hyp_write_file(filename, codegen->output.data, codegen->output.length);
}