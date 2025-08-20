/**
 * Hyper Programming Language - Transpiler Implementation
 * 
 * Generates target code (C, JavaScript, bytecode, etc.) from AST nodes.
 * Supports multiple output targets with optimizations and symbol management.
 */

#include "../../include/transpiler.h"
#include "../../include/parser.h"
#include "../../include/hyp_common.h"
#include "../../include/hyp_runtime.h"
#include <string.h>
#include <stdarg.h>

/* Forward declaration */
void hyp_codegen_generate_node(hyp_codegen_t* codegen, hyp_ast_node_t* node);

/* Symbol table implementation */
static const char* symbol_table_lookup(hyp_codegen_t* codegen, const char* name) {
    for (size_t i = 0; i < codegen->symbols.count; i++) {
        if (strcmp(codegen->symbols.names[i], name) == 0) {
            return codegen->symbols.names[i];
        }
    }
    return NULL;
}

static void symbol_table_add(hyp_codegen_t* codegen, const char* name, hyp_type_t* type) {
    if (codegen->symbols.count >= codegen->symbols.capacity) {
        size_t new_capacity = codegen->symbols.capacity ? codegen->symbols.capacity * 2 : 8;
        codegen->symbols.names = realloc(codegen->symbols.names, new_capacity * sizeof(char*));
        codegen->symbols.types = realloc(codegen->symbols.types, new_capacity * sizeof(hyp_type_t*));
        codegen->symbols.capacity = new_capacity;
    }
    
    codegen->symbols.names[codegen->symbols.count] = (char*)name;
    codegen->symbols.types[codegen->symbols.count] = type;
    codegen->symbols.count++;
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
static void generate_c_number(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit(codegen, "%.17g", node->number.value);
}

static void generate_c_string(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit(codegen, "\"%s\"", node->string.value ? node->string.value : "");
}

static void generate_c_boolean(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit(codegen, node->boolean.value ? "true" : "false");
}

static void generate_c_identifier(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit(codegen, "%s", node->identifier.name ? node->identifier.name : "unknown");
}

static void generate_c_binary(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit(codegen, "(");
    hyp_codegen_generate_node(codegen, node->binary_op.left);
    
    switch (node->binary_op.op) {
        case BINOP_ADD: emit(codegen, " + "); break;
        case BINOP_SUB: emit(codegen, " - "); break;
        case BINOP_MUL: emit(codegen, " * "); break;
        case BINOP_DIV: emit(codegen, " / "); break;
        case BINOP_MOD: emit(codegen, " %% "); break;
        case BINOP_EQ: emit(codegen, " == "); break;
        case BINOP_NE: emit(codegen, " != "); break;
        case BINOP_LT: emit(codegen, " < "); break;
        case BINOP_LE: emit(codegen, " <= "); break;
        case BINOP_GT: emit(codegen, " > "); break;
        case BINOP_GE: emit(codegen, " >= "); break;
        case BINOP_AND: emit(codegen, " && "); break;
        case BINOP_OR: emit(codegen, " || "); break;
        default: emit(codegen, " ? "); break;
    }
    
    hyp_codegen_generate_node(codegen, node->binary_op.right);
    emit(codegen, ")");
}

static void generate_c_unary(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    switch (node->unary_op.op) {
        case UNOP_NOT:
            emit(codegen, "!");
            break;
        case UNOP_MINUS:
            emit(codegen, "-");
            break;
        case UNOP_PLUS:
            emit(codegen, "+");
            break;
        default:
            break;
    }
    
    emit(codegen, "(");
    hyp_codegen_generate_node(codegen, node->unary_op.operand);
    emit(codegen, ")");
}



static void generate_c_call(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    hyp_codegen_generate_node(codegen, node->call.callee);
    emit(codegen, "(");
    
    for (size_t i = 0; i < node->call.arguments.count; i++) {
        if (i > 0) emit(codegen, ", ");
        hyp_codegen_generate_node(codegen, node->call.arguments.data[i]);
    }
    
    emit(codegen, ")");
}

static void generate_c_assignment(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    hyp_codegen_generate_node(codegen, node->assignment.target);
    
    switch (node->assignment.op) {
        case ASSIGN_SIMPLE: emit(codegen, " = "); break;
        case ASSIGN_ADD: emit(codegen, " += "); break;
        case ASSIGN_SUB: emit(codegen, " -= "); break;
        case ASSIGN_MUL: emit(codegen, " *= "); break;
        case ASSIGN_DIV: emit(codegen, " /= "); break;
    }
    
    hyp_codegen_generate_node(codegen, node->assignment.value);
}

static void generate_c_var_decl(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    /* Add to symbol table */
    symbol_table_add(codegen, node->variable_decl.name, NULL);
    
    if (node->variable_decl.is_const) {
        emit_line(codegen, "const hyp_value_t %s", node->variable_decl.name);
    } else {
        emit_line(codegen, "hyp_value_t %s", node->variable_decl.name);
    }
    
    if (node->variable_decl.initializer) {
        emit(codegen, " = ");
        hyp_codegen_generate_node(codegen, node->variable_decl.initializer);
    } else {
        emit(codegen, " = hyp_value_null()");
    }
    
    emit(codegen, ";");
}

static void generate_c_function(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    /* Add to symbol table */
    symbol_table_add(codegen, node->function_decl.name, NULL);
    
    emit_line(codegen, "hyp_value_t %s(", node->function_decl.name);
    
    /* Parameters */
    for (size_t i = 0; i < node->function_decl.parameters.count; i++) {
        if (i > 0) emit(codegen, ", ");
        hyp_parameter_t* param = &node->function_decl.parameters.data[i];
        emit(codegen, "hyp_value_t %s", param->name);
    }
    
    emit(codegen, ") {");
    emit_indent(codegen);
    
    /* Function body */
    codegen->function_ctx.loop_depth++;
    hyp_codegen_generate_node(codegen, node->function_decl.body);
    codegen->function_ctx.loop_depth--;
    
    emit_dedent(codegen);
    emit_line(codegen, "}");
}

static void generate_c_if(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit_line(codegen, "if (hyp_value_is_truthy(");
    hyp_codegen_generate_node(codegen, node->if_stmt.condition);
    emit(codegen, ")) {");
    
    emit_indent(codegen);
    hyp_codegen_generate_node(codegen, node->if_stmt.then_stmt);
    emit_dedent(codegen);
    
    if (node->if_stmt.else_stmt) {
        emit_line(codegen, "} else {");
        emit_indent(codegen);
        hyp_codegen_generate_node(codegen, node->if_stmt.else_stmt);
        emit_dedent(codegen);
    }
    
    emit_line(codegen, "}");
}

static void generate_c_while(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit_line(codegen, "while (hyp_value_is_truthy(");
    hyp_codegen_generate_node(codegen, node->while_stmt.condition);
    emit(codegen, ")) {");
    
    emit_indent(codegen);
    hyp_codegen_generate_node(codegen, node->while_stmt.body);
    emit_dedent(codegen);
    
    emit_line(codegen, "}");
}

static void generate_c_return(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit_line(codegen, "return ");
    
    if (node->return_stmt.value) {
        hyp_codegen_generate_node(codegen, node->return_stmt.value);
    } else {
        emit(codegen, "hyp_value_null()");
    }
    
    emit(codegen, ";");
}

static void generate_c_block(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    codegen->function_ctx.loop_depth++;
    
    for (size_t i = 0; i < node->block_stmt.statements.count; i++) {
        hyp_codegen_generate_node(codegen, node->block_stmt.statements.data[i]);
    }
    
    codegen->function_ctx.loop_depth--;
}

static void generate_c_expression_stmt(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit_line(codegen, "");
    hyp_codegen_generate_node(codegen, node->expression_stmt.expression);
    emit(codegen, ";");
}

static void generate_c_program(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    /* Generate includes */
    emit_line(codegen, "#include <stdio.h>");
    emit_line(codegen, "#include <stdlib.h>");
    emit_line(codegen, "#include <stdbool.h>");
    emit_line(codegen, "#include <string.h>");
    emit_line(codegen, "#include \"hyp_runtime.h\"");
    emit_line(codegen, "");
    
    /* Generate statements */
    for (size_t i = 0; i < node->program.statements.count; i++) {
        hyp_codegen_generate_node(codegen, node->program.statements.data[i]);
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
static void generate_js_number(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit(codegen, "%.17g", node->number.value);
}

static void generate_js_string(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit(codegen, "\"%s\"", node->string.value ? node->string.value : "");
}

static void generate_js_boolean(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit(codegen, node->boolean.value ? "true" : "false");
}

static void generate_js_identifier(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit(codegen, "%s", node->identifier.name ? node->identifier.name : "unknown");
}

static void generate_js_binary(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit(codegen, "(");
    hyp_codegen_generate_node(codegen, node->binary_op.left);
    
    switch (node->binary_op.op) {
        case BINOP_ADD: emit(codegen, " + "); break;
        case BINOP_SUB: emit(codegen, " - "); break;
        case BINOP_MUL: emit(codegen, " * "); break;
        case BINOP_DIV: emit(codegen, " / "); break;
        case BINOP_MOD: emit(codegen, " % "); break;
        case BINOP_EQ: emit(codegen, " === "); break;
        case BINOP_NE: emit(codegen, " !== "); break;
        case BINOP_LT: emit(codegen, " < "); break;
        case BINOP_LE: emit(codegen, " <= "); break;
        case BINOP_GT: emit(codegen, " > "); break;
        case BINOP_GE: emit(codegen, " >= "); break;
        case BINOP_AND: emit(codegen, " && "); break;
        case BINOP_OR: emit(codegen, " || "); break;
        default: emit(codegen, " ? "); break;
    }
    
    hyp_codegen_generate_node(codegen, node->binary_op.right);
    emit(codegen, ")");
}

static void generate_js_function(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    emit_line(codegen, "function %s(", node->function_decl.name);
    
    /* Parameters */
    for (size_t i = 0; i < node->function_decl.parameters.count; i++) {
        if (i > 0) emit(codegen, ", ");
        hyp_parameter_t* param = &node->function_decl.parameters.data[i];
        emit(codegen, "%s", param->name);
    }
    
    emit(codegen, ") {");
    emit_indent(codegen);
    
    /* Function body */
    hyp_codegen_generate_node(codegen, node->function_decl.body);
    
    emit_dedent(codegen);
    emit_line(codegen, "}");
}

/* Main code generation dispatch */
void hyp_codegen_generate_node(hyp_codegen_t* codegen, hyp_ast_node_t* node) {
    if (!codegen || !node) return;
    
    switch (codegen->target) {
        case TARGET_C:
            switch (node->type) {
                case AST_NUMBER:
                    emit(codegen, "%.17g", node->number.value);
                    break;
                case AST_BINARY_OP:
                    hyp_codegen_generate_node(codegen, node->binary_op.left);
                    emit(codegen, " %s ", hyp_binary_op_to_c(node->binary_op.op));
                    hyp_codegen_generate_node(codegen, node->binary_op.right);
                    break;
                case AST_UNARY_OP:
                    emit(codegen, "%s", hyp_unary_op_to_c(node->unary_op.op));
                    hyp_codegen_generate_node(codegen, node->unary_op.operand);
                    break;
                case AST_IDENTIFIER:
                    emit(codegen, "%s", node->identifier.name);
                    break;
                case AST_STRING:
                    emit(codegen, "\"%s\"", node->string.value);
                    break;
                case AST_BOOLEAN:
                    emit(codegen, "%s", node->boolean.value ? "true" : "false");
                    break;
                case AST_CALL:
                case AST_ASSIGNMENT:
                case AST_VARIABLE_DECL:
                case AST_FUNCTION_DECL:
                case AST_IF_STMT:
                case AST_WHILE_STMT:
                case AST_RETURN_STMT:
                case AST_BLOCK_STMT:
                case AST_EXPRESSION_STMT:
                case AST_PROGRAM:
                    /* TODO: Implement these cases */
                    emit(codegen, "/* TODO: %s */", hyp_ast_node_type_name(node->type));
                    break;
                default:
                    break;
            }
            break;
            
        case TARGET_JAVASCRIPT:
            switch (node->type) {
                case AST_NUMBER:
                    emit(codegen, "%.17g", node->number.value);
                    break;
                case AST_BINARY_OP:
                    hyp_codegen_generate_node(codegen, node->binary_op.left);
                    emit(codegen, " %s ", hyp_binary_op_to_js(node->binary_op.op));
                    hyp_codegen_generate_node(codegen, node->binary_op.right);
                    break;
                case AST_IDENTIFIER:
                    emit(codegen, "%s", node->identifier.name);
                    break;
                case AST_STRING:
                    emit(codegen, "\"%s\"", node->string.value);
                    break;
                case AST_BOOLEAN:
                    emit(codegen, "%s", node->boolean.value ? "true" : "false");
                    break;
                /* Add more JavaScript cases as needed */
                default:
                    emit(codegen, "/* TODO: %s */", hyp_ast_node_type_name(node->type));
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
    if (options) {
        codegen->optimize = options->optimize;
        codegen->debug_info = options->debug_info;
    } else {
        codegen->optimize = false;
        codegen->debug_info = false;
    }
    codegen->output = hyp_string_create("");
    codegen->indent_level = 0;
    codegen->function_ctx.loop_depth = 0;
    codegen->function_ctx.current_function = NULL;
    codegen->function_ctx.return_type = NULL;
    codegen->function_ctx.in_loop = false;
    
    codegen->symbols.names = NULL;
    codegen->symbols.types = NULL;
    codegen->symbols.count = 0;
    codegen->symbols.capacity = 0;
    
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
    if (!codegen || !ast) return HYP_ERROR_INVALID_ARG;
    
    /* Reset output */
    hyp_string_destroy(&codegen->output);
    codegen->output = hyp_string_create("");
    codegen->indent_level = 0;
    codegen->function_ctx.loop_depth = 0;
    
    /* Clear symbol table */
    codegen->symbols.count = 0;
    
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
        case TARGET_C: return "C";
        case TARGET_JAVASCRIPT: return "JavaScript";
        case TARGET_BYTECODE: return "Bytecode";
        case TARGET_ASSEMBLY: return "Assembly";
        case TARGET_LLVM_IR: return "LLVM IR";
        default: return "Unknown";
    }
}

hyp_error_t hyp_codegen_write_to_file(hyp_codegen_t* codegen, const char* filename) {
    if (!codegen || !filename) return HYP_ERROR_INVALID_ARG;
    
    return hyp_write_file(filename, codegen->output.data, codegen->output.length);
}

/* Initialize code generator */
hyp_error_t hyp_codegen_init(hyp_codegen_t* codegen, hyp_target_t target) {
    if (!codegen) return HYP_ERROR_INVALID_ARG;
    
    memset(codegen, 0, sizeof(hyp_codegen_t));
    codegen->target = target;
    codegen->indent_level = 0;
    codegen->symbols.capacity = 0;
    codegen->symbols.count = 0;
    codegen->symbols.names = NULL;
    codegen->symbols.types = NULL;
    
    codegen->output = hyp_string_create("");
    
    return HYP_OK;
}

/* Get AST node type name */
const char* hyp_ast_node_type_name(hyp_ast_node_type_t type) {
    switch (type) {
        case AST_NUMBER: return "Number";
        case AST_STRING: return "String";
        case AST_BOOLEAN: return "Boolean";
        case AST_NULL: return "Null";
        case AST_IDENTIFIER: return "Identifier";
        case AST_BINARY_OP: return "BinaryOp";
        case AST_UNARY_OP: return "UnaryOp";
        case AST_ASSIGNMENT: return "Assignment";
        case AST_CALL: return "Call";
        case AST_MEMBER_ACCESS: return "MemberAccess";
        case AST_INDEX_ACCESS: return "IndexAccess";
        case AST_CONDITIONAL: return "Conditional";
        case AST_ARRAY_LITERAL: return "ArrayLiteral";
        case AST_OBJECT_LITERAL: return "ObjectLiteral";
        case AST_LAMBDA: return "Lambda";
        case AST_EXPRESSION_STMT: return "ExpressionStmt";
        case AST_VARIABLE_DECL: return "VariableDecl";
        case AST_FUNCTION_DECL: return "FunctionDecl";
        case AST_IF_STMT: return "IfStmt";
        case AST_WHILE_STMT: return "WhileStmt";
        case AST_FOR_STMT: return "ForStmt";
        case AST_RETURN_STMT: return "ReturnStmt";
        case AST_BLOCK_STMT: return "BlockStmt";
        case AST_IMPORT_STMT: return "ImportStmt";
        case AST_EXPORT_STMT: return "ExportStmt";
        case AST_MATCH_STMT: return "MatchStmt";
        case AST_TRY_STMT: return "TryStmt";
        case AST_PROGRAM: return "Program";
        default: return "Unknown";
    }
}

/* Convert binary operator to C */
const char* hyp_binary_op_to_c(hyp_binary_op_t op) {
    switch (op) {
        case BINOP_ADD: return "+";
        case BINOP_SUB: return "-";
        case BINOP_MUL: return "*";
        case BINOP_DIV: return "/";
        case BINOP_MOD: return "%";
        case BINOP_EQ: return "==";
        case BINOP_NE: return "!=";
        case BINOP_LT: return "<";
        case BINOP_LE: return "<=";
        case BINOP_GT: return ">";
        case BINOP_GE: return ">=";
        case BINOP_AND: return "&&";
        case BINOP_OR: return "||";
        case BINOP_BITWISE_AND: return "&";
        case BINOP_BITWISE_OR: return "|";
        case BINOP_BITWISE_XOR: return "^";
        case BINOP_LEFT_SHIFT: return "<<";
        case BINOP_RIGHT_SHIFT: return ">>";
        default: return "?";
    }
}

/* Convert binary operator to JavaScript */
const char* hyp_binary_op_to_js(hyp_binary_op_t op) {
    switch (op) {
        case BINOP_ADD: return "+";
        case BINOP_SUB: return "-";
        case BINOP_MUL: return "*";
        case BINOP_DIV: return "/";
        case BINOP_MOD: return "%";
        case BINOP_EQ: return "===";
        case BINOP_NE: return "!==";
        case BINOP_LT: return "<";
        case BINOP_LE: return "<=";
        case BINOP_GT: return ">";
        case BINOP_GE: return ">=";
        case BINOP_AND: return "&&";
        case BINOP_OR: return "||";
        case BINOP_BITWISE_AND: return "&";
        case BINOP_BITWISE_OR: return "|";
        case BINOP_BITWISE_XOR: return "^";
        case BINOP_LEFT_SHIFT: return "<<";
        case BINOP_RIGHT_SHIFT: return ">>";
        default: return "?";
    }
}

/* Convert unary operator to C */
const char* hyp_unary_op_to_c(hyp_unary_op_t op) {
    switch (op) {
        case UNOP_PLUS: return "+";
        case UNOP_MINUS: return "-";
        case UNOP_NOT: return "!";
        case UNOP_BITWISE_NOT: return "~";
        case UNOP_INCREMENT: return "++";
        case UNOP_DECREMENT: return "--";
        default: return "?";
    }
}