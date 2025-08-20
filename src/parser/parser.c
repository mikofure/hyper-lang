/**
 * Hyper Programming Language - Parser Implementation
 * 
 * Builds Abstract Syntax Tree (AST) from tokens produced by the lexer.
 * Implements recursive descent parsing for the Hyper language grammar.
 */

#include "../../include/parser.h"
#include "../../include/lexer.h"
#include "../../include/hyp_common.h"
#include <string.h>

/* Parser state */
struct hyp_parser {
    hyp_lexer_t* lexer;
    hyp_token_t current;
    hyp_token_t previous;
    hyp_arena_t* arena;
    bool had_error;
    bool panic_mode;
};

/* Forward declarations */
static hyp_ast_node_t* parse_expression(hyp_parser_t* parser);
static hyp_ast_node_t* parse_statement(hyp_parser_t* parser);
static hyp_ast_node_t* parse_declaration(hyp_parser_t* parser);

/* Error handling */
static void error_at(hyp_parser_t* parser, hyp_token_t* token, const char* message) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    
    fprintf(stderr, "[%s:%zu:%zu] Error", token->filename, token->line, token->column);
    
    if (token->type == HYP_TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == HYP_TOKEN_ERROR) {
        /* Nothing */
    } else {
        fprintf(stderr, " at '%.*s'", (int)token->length, token->start);
    }
    
    fprintf(stderr, ": %s\n", message);
    parser->had_error = true;
}

static void error(hyp_parser_t* parser, const char* message) {
    error_at(parser, &parser->previous, message);
}

static void error_at_current(hyp_parser_t* parser, const char* message) {
    error_at(parser, &parser->current, message);
}

/* Token management */
static void advance(hyp_parser_t* parser) {
    parser->previous = parser->current;
    
    for (;;) {
        parser->current = hyp_lexer_scan_token(parser->lexer);
        if (parser->current.type != HYP_TOKEN_ERROR) break;
        
        error_at_current(parser, parser->current.start);
    }
}

static bool check(hyp_parser_t* parser, hyp_token_type_t type) {
    return parser->current.type == type;
}

static bool match(hyp_parser_t* parser, hyp_token_type_t type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static void consume(hyp_parser_t* parser, hyp_token_type_t type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    
    error_at_current(parser, message);
}

/* AST node creation helpers */
static hyp_ast_node_t* create_node(hyp_parser_t* parser, hyp_ast_node_type_t type) {
    hyp_ast_node_t* node = hyp_arena_alloc(parser->arena, sizeof(hyp_ast_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(hyp_ast_node_t));
    node->type = type;
    node->line = parser->previous.line;
    node->column = parser->previous.column;
    
    return node;
}

static char* copy_string(hyp_parser_t* parser, const char* start, size_t length) {
    char* str = hyp_arena_alloc(parser->arena, length + 1);
    if (!str) return NULL;
    
    memcpy(str, start, length);
    str[length] = '\0';
    return str;
}

/* Synchronization for error recovery */
static void synchronize(hyp_parser_t* parser) {
    parser->panic_mode = false;
    
    while (parser->current.type != HYP_TOKEN_EOF) {
        if (parser->previous.type == HYP_TOKEN_SEMICOLON) return;
        
        switch (parser->current.type) {
            case HYP_TOKEN_FN:
            case HYP_TOKEN_LET:
            case HYP_TOKEN_CONST:
            case HYP_TOKEN_IF:
            case HYP_TOKEN_WHILE:
            case HYP_TOKEN_FOR:
            case HYP_TOKEN_RETURN:
                return;
            default:
                break;
        }
        
        advance(parser);
    }
}

/* Expression parsing */
static hyp_ast_node_t* parse_primary(hyp_parser_t* parser) {
    if (match(parser, HYP_TOKEN_TRUE)) {
        hyp_ast_node_t* node = create_node(parser, HYP_AST_LITERAL);
        if (node) {
            node->as.literal.type = HYP_LITERAL_BOOLEAN;
            node->as.literal.as.boolean = true;
        }
        return node;
    }
    
    if (match(parser, HYP_TOKEN_FALSE)) {
        hyp_ast_node_t* node = create_node(parser, HYP_AST_LITERAL);
        if (node) {
            node->as.literal.type = HYP_LITERAL_BOOLEAN;
            node->as.literal.as.boolean = false;
        }
        return node;
    }
    
    if (match(parser, HYP_TOKEN_NULL)) {
        hyp_ast_node_t* node = create_node(parser, HYP_AST_LITERAL);
        if (node) {
            node->as.literal.type = HYP_LITERAL_NULL;
        }
        return node;
    }
    
    if (match(parser, HYP_TOKEN_NUMBER)) {
        hyp_ast_node_t* node = create_node(parser, HYP_AST_LITERAL);
        if (node) {
            node->as.literal.type = HYP_LITERAL_NUMBER;
            char* str = copy_string(parser, parser->previous.start, parser->previous.length);
            if (str) {
                node->as.literal.as.number = strtod(str, NULL);
            }
        }
        return node;
    }
    
    if (match(parser, HYP_TOKEN_STRING)) {
        hyp_ast_node_t* node = create_node(parser, HYP_AST_LITERAL);
        if (node) {
            node->as.literal.type = HYP_LITERAL_STRING;
            /* Skip quotes */
            node->as.literal.as.string = copy_string(parser, 
                parser->previous.start + 1, parser->previous.length - 2);
        }
        return node;
    }
    
    if (match(parser, HYP_TOKEN_IDENTIFIER)) {
        hyp_ast_node_t* node = create_node(parser, HYP_AST_IDENTIFIER);
        if (node) {
            node->as.identifier.name = copy_string(parser, 
                parser->previous.start, parser->previous.length);
        }
        return node;
    }
    
    if (match(parser, HYP_TOKEN_LEFT_PAREN)) {
        hyp_ast_node_t* expr = parse_expression(parser);
        consume(parser, HYP_TOKEN_RIGHT_PAREN, "Expected ')' after expression");
        return expr;
    }
    
    if (match(parser, HYP_TOKEN_LEFT_BRACKET)) {
        hyp_ast_node_t* node = create_node(parser, HYP_AST_ARRAY_LITERAL);
        if (!node) return NULL;
        
        HYP_ARRAY_INIT(node->as.array_literal.elements);
        
        if (!check(parser, HYP_TOKEN_RIGHT_BRACKET)) {
            do {
                hyp_ast_node_t* element = parse_expression(parser);
                if (element) {
                    HYP_ARRAY_PUSH(node->as.array_literal.elements, element);
                }
            } while (match(parser, HYP_TOKEN_COMMA));
        }
        
        consume(parser, HYP_TOKEN_RIGHT_BRACKET, "Expected ']' after array elements");
        return node;
    }
    
    if (match(parser, HYP_TOKEN_LEFT_BRACE)) {
        hyp_ast_node_t* node = create_node(parser, HYP_AST_OBJECT_LITERAL);
        if (!node) return NULL;
        
        HYP_ARRAY_INIT(node->as.object_literal.properties);
        
        if (!check(parser, HYP_TOKEN_RIGHT_BRACE)) {
            do {
                hyp_object_property_t* prop = hyp_arena_alloc(parser->arena, sizeof(hyp_object_property_t));
                if (!prop) break;
                
                if (match(parser, HYP_TOKEN_IDENTIFIER)) {
                    prop->key = copy_string(parser, parser->previous.start, parser->previous.length);
                } else if (match(parser, HYP_TOKEN_STRING)) {
                    prop->key = copy_string(parser, parser->previous.start + 1, parser->previous.length - 2);
                } else {
                    error(parser, "Expected property name");
                    break;
                }
                
                consume(parser, HYP_TOKEN_COLON, "Expected ':' after property name");
                prop->value = parse_expression(parser);
                
                HYP_ARRAY_PUSH(node->as.object_literal.properties, prop);
            } while (match(parser, HYP_TOKEN_COMMA));
        }
        
        consume(parser, HYP_TOKEN_RIGHT_BRACE, "Expected '}' after object properties");
        return node;
    }
    
    error(parser, "Expected expression");
    return NULL;
}

static hyp_ast_node_t* parse_call(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_primary(parser);
    
    while (true) {
        if (match(parser, HYP_TOKEN_LEFT_PAREN)) {
            hyp_ast_node_t* call = create_node(parser, HYP_AST_CALL);
            if (!call) break;
            
            call->as.call.callee = expr;
            HYP_ARRAY_INIT(call->as.call.arguments);
            
            if (!check(parser, HYP_TOKEN_RIGHT_PAREN)) {
                do {
                    hyp_ast_node_t* arg = parse_expression(parser);
                    if (arg) {
                        HYP_ARRAY_PUSH(call->as.call.arguments, arg);
                    }
                } while (match(parser, HYP_TOKEN_COMMA));
            }
            
            consume(parser, HYP_TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
            expr = call;
        } else if (match(parser, HYP_TOKEN_DOT)) {
            hyp_ast_node_t* member = create_node(parser, HYP_AST_MEMBER_ACCESS);
            if (!member) break;
            
            member->as.member_access.object = expr;
            consume(parser, HYP_TOKEN_IDENTIFIER, "Expected property name after '.'");
            member->as.member_access.property = copy_string(parser, 
                parser->previous.start, parser->previous.length);
            
            expr = member;
        } else if (match(parser, HYP_TOKEN_LEFT_BRACKET)) {
            hyp_ast_node_t* index = create_node(parser, HYP_AST_INDEX_ACCESS);
            if (!index) break;
            
            index->as.index_access.object = expr;
            index->as.index_access.index = parse_expression(parser);
            consume(parser, HYP_TOKEN_RIGHT_BRACKET, "Expected ']' after index");
            
            expr = index;
        } else {
            break;
        }
    }
    
    return expr;
}

static hyp_ast_node_t* parse_unary(hyp_parser_t* parser) {
    if (match(parser, HYP_TOKEN_BANG) || match(parser, HYP_TOKEN_MINUS) || match(parser, HYP_TOKEN_NOT)) {
        hyp_ast_node_t* node = create_node(parser, HYP_AST_UNARY);
        if (!node) return NULL;
        
        hyp_token_type_t op_type = parser->previous.type;
        switch (op_type) {
            case HYP_TOKEN_BANG: node->as.unary.operator = HYP_UNARY_NOT; break;
            case HYP_TOKEN_MINUS: node->as.unary.operator = HYP_UNARY_NEGATE; break;
            case HYP_TOKEN_NOT: node->as.unary.operator = HYP_UNARY_NOT; break;
            default: break;
        }
        
        node->as.unary.operand = parse_unary(parser);
        return node;
    }
    
    return parse_call(parser);
}

static hyp_ast_node_t* parse_factor(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_unary(parser);
    
    while (match(parser, HYP_TOKEN_SLASH) || match(parser, HYP_TOKEN_STAR) || match(parser, HYP_TOKEN_PERCENT)) {
        hyp_ast_node_t* binary = create_node(parser, HYP_AST_BINARY);
        if (!binary) break;
        
        hyp_token_type_t op_type = parser->previous.type;
        switch (op_type) {
            case HYP_TOKEN_SLASH: binary->as.binary.operator = HYP_BINARY_DIVIDE; break;
            case HYP_TOKEN_STAR: binary->as.binary.operator = HYP_BINARY_MULTIPLY; break;
            case HYP_TOKEN_PERCENT: binary->as.binary.operator = HYP_BINARY_MODULO; break;
            default: break;
        }
        
        binary->as.binary.left = expr;
        binary->as.binary.right = parse_unary(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_term(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_factor(parser);
    
    while (match(parser, HYP_TOKEN_MINUS) || match(parser, HYP_TOKEN_PLUS)) {
        hyp_ast_node_t* binary = create_node(parser, HYP_AST_BINARY);
        if (!binary) break;
        
        hyp_token_type_t op_type = parser->previous.type;
        switch (op_type) {
            case HYP_TOKEN_MINUS: binary->as.binary.operator = HYP_BINARY_SUBTRACT; break;
            case HYP_TOKEN_PLUS: binary->as.binary.operator = HYP_BINARY_ADD; break;
            default: break;
        }
        
        binary->as.binary.left = expr;
        binary->as.binary.right = parse_factor(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_comparison(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_term(parser);
    
    while (match(parser, HYP_TOKEN_GREATER) || match(parser, HYP_TOKEN_GREATER_EQUAL) ||
           match(parser, HYP_TOKEN_LESS) || match(parser, HYP_TOKEN_LESS_EQUAL)) {
        hyp_ast_node_t* binary = create_node(parser, HYP_AST_BINARY);
        if (!binary) break;
        
        hyp_token_type_t op_type = parser->previous.type;
        switch (op_type) {
            case HYP_TOKEN_GREATER: binary->as.binary.operator = HYP_BINARY_GREATER; break;
            case HYP_TOKEN_GREATER_EQUAL: binary->as.binary.operator = HYP_BINARY_GREATER_EQUAL; break;
            case HYP_TOKEN_LESS: binary->as.binary.operator = HYP_BINARY_LESS; break;
            case HYP_TOKEN_LESS_EQUAL: binary->as.binary.operator = HYP_BINARY_LESS_EQUAL; break;
            default: break;
        }
        
        binary->as.binary.left = expr;
        binary->as.binary.right = parse_term(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_equality(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_comparison(parser);
    
    while (match(parser, HYP_TOKEN_BANG_EQUAL) || match(parser, HYP_TOKEN_EQUAL_EQUAL)) {
        hyp_ast_node_t* binary = create_node(parser, HYP_AST_BINARY);
        if (!binary) break;
        
        hyp_token_type_t op_type = parser->previous.type;
        switch (op_type) {
            case HYP_TOKEN_BANG_EQUAL: binary->as.binary.operator = HYP_BINARY_NOT_EQUAL; break;
            case HYP_TOKEN_EQUAL_EQUAL: binary->as.binary.operator = HYP_BINARY_EQUAL; break;
            default: break;
        }
        
        binary->as.binary.left = expr;
        binary->as.binary.right = parse_comparison(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_logical_and(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_equality(parser);
    
    while (match(parser, HYP_TOKEN_AND) || match(parser, HYP_TOKEN_AND_AND)) {
        hyp_ast_node_t* binary = create_node(parser, HYP_AST_BINARY);
        if (!binary) break;
        
        binary->as.binary.operator = HYP_BINARY_AND;
        binary->as.binary.left = expr;
        binary->as.binary.right = parse_equality(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_logical_or(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_logical_and(parser);
    
    while (match(parser, HYP_TOKEN_OR) || match(parser, HYP_TOKEN_OR_OR)) {
        hyp_ast_node_t* binary = create_node(parser, HYP_AST_BINARY);
        if (!binary) break;
        
        binary->as.binary.operator = HYP_BINARY_OR;
        binary->as.binary.left = expr;
        binary->as.binary.right = parse_logical_and(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_assignment(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_logical_or(parser);
    
    if (match(parser, HYP_TOKEN_EQUAL)) {
        hyp_ast_node_t* assignment = create_node(parser, HYP_AST_ASSIGNMENT);
        if (!assignment) return expr;
        
        assignment->as.assignment.operator = HYP_ASSIGN_SIMPLE;
        assignment->as.assignment.target = expr;
        assignment->as.assignment.value = parse_assignment(parser);
        return assignment;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_expression(hyp_parser_t* parser) {
    return parse_assignment(parser);
}

/* Statement parsing */
static hyp_ast_node_t* parse_expression_statement(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, HYP_AST_EXPRESSION_STMT);
    if (!node) return NULL;
    
    node->as.expression_stmt.expression = parse_expression(parser);
    consume(parser, HYP_TOKEN_SEMICOLON, "Expected ';' after expression");
    
    return node;
}

static hyp_ast_node_t* parse_block_statement(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, HYP_AST_BLOCK);
    if (!node) return NULL;
    
    HYP_ARRAY_INIT(node->as.block.statements);
    
    while (!check(parser, HYP_TOKEN_RIGHT_BRACE) && !check(parser, HYP_TOKEN_EOF)) {
        hyp_ast_node_t* stmt = parse_declaration(parser);
        if (stmt) {
            HYP_ARRAY_PUSH(node->as.block.statements, stmt);
        }
    }
    
    consume(parser, HYP_TOKEN_RIGHT_BRACE, "Expected '}' after block");
    return node;
}

static hyp_ast_node_t* parse_if_statement(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, HYP_AST_IF);
    if (!node) return NULL;
    
    consume(parser, HYP_TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    node->as.if_stmt.condition = parse_expression(parser);
    consume(parser, HYP_TOKEN_RIGHT_PAREN, "Expected ')' after if condition");
    
    node->as.if_stmt.then_branch = parse_statement(parser);
    
    if (match(parser, HYP_TOKEN_ELSE)) {
        node->as.if_stmt.else_branch = parse_statement(parser);
    }
    
    return node;
}

static hyp_ast_node_t* parse_while_statement(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, HYP_AST_WHILE);
    if (!node) return NULL;
    
    consume(parser, HYP_TOKEN_LEFT_PAREN, "Expected '(' after 'while'");
    node->as.while_stmt.condition = parse_expression(parser);
    consume(parser, HYP_TOKEN_RIGHT_PAREN, "Expected ')' after while condition");
    
    node->as.while_stmt.body = parse_statement(parser);
    
    return node;
}

static hyp_ast_node_t* parse_return_statement(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, HYP_AST_RETURN);
    if (!node) return NULL;
    
    if (!check(parser, HYP_TOKEN_SEMICOLON)) {
        node->as.return_stmt.value = parse_expression(parser);
    }
    
    consume(parser, HYP_TOKEN_SEMICOLON, "Expected ';' after return value");
    return node;
}

static hyp_ast_node_t* parse_statement(hyp_parser_t* parser) {
    if (match(parser, HYP_TOKEN_IF)) {
        return parse_if_statement(parser);
    }
    
    if (match(parser, HYP_TOKEN_WHILE)) {
        return parse_while_statement(parser);
    }
    
    if (match(parser, HYP_TOKEN_RETURN)) {
        return parse_return_statement(parser);
    }
    
    if (match(parser, HYP_TOKEN_LEFT_BRACE)) {
        return parse_block_statement(parser);
    }
    
    return parse_expression_statement(parser);
}

/* Declaration parsing */
static hyp_ast_node_t* parse_variable_declaration(hyp_parser_t* parser, bool is_const) {
    hyp_ast_node_t* node = create_node(parser, HYP_AST_VAR_DECL);
    if (!node) return NULL;
    
    node->as.var_decl.is_const = is_const;
    
    consume(parser, HYP_TOKEN_IDENTIFIER, "Expected variable name");
    node->as.var_decl.name = copy_string(parser, parser->previous.start, parser->previous.length);
    
    if (match(parser, HYP_TOKEN_EQUAL)) {
        node->as.var_decl.initializer = parse_expression(parser);
    }
    
    consume(parser, HYP_TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    return node;
}

static hyp_ast_node_t* parse_function_declaration(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, HYP_AST_FUNCTION);
    if (!node) return NULL;
    
    consume(parser, HYP_TOKEN_IDENTIFIER, "Expected function name");
    node->as.function.name = copy_string(parser, parser->previous.start, parser->previous.length);
    
    consume(parser, HYP_TOKEN_LEFT_PAREN, "Expected '(' after function name");
    
    HYP_ARRAY_INIT(node->as.function.parameters);
    
    if (!check(parser, HYP_TOKEN_RIGHT_PAREN)) {
        do {
            hyp_parameter_t* param = hyp_arena_alloc(parser->arena, sizeof(hyp_parameter_t));
            if (!param) break;
            
            consume(parser, HYP_TOKEN_IDENTIFIER, "Expected parameter name");
            param->name = copy_string(parser, parser->previous.start, parser->previous.length);
            param->type = NULL; /* Type inference for now */
            
            HYP_ARRAY_PUSH(node->as.function.parameters, param);
        } while (match(parser, HYP_TOKEN_COMMA));
    }
    
    consume(parser, HYP_TOKEN_RIGHT_PAREN, "Expected ')' after parameters");
    consume(parser, HYP_TOKEN_LEFT_BRACE, "Expected '{' before function body");
    
    node->as.function.body = parse_block_statement(parser);
    
    return node;
}

static hyp_ast_node_t* parse_declaration(hyp_parser_t* parser) {
    if (match(parser, HYP_TOKEN_LET)) {
        return parse_variable_declaration(parser, false);
    }
    
    if (match(parser, HYP_TOKEN_CONST)) {
        return parse_variable_declaration(parser, true);
    }
    
    if (match(parser, HYP_TOKEN_FN)) {
        return parse_function_declaration(parser);
    }
    
    return parse_statement(parser);
}

/* Public API */
hyp_parser_t* hyp_parser_create(hyp_lexer_t* lexer) {
    if (!lexer) return NULL;
    
    hyp_parser_t* parser = HYP_MALLOC(sizeof(hyp_parser_t));
    if (!parser) return NULL;
    
    parser->lexer = lexer;
    parser->had_error = false;
    parser->panic_mode = false;
    parser->arena = hyp_arena_create(16384); /* 16KB arena for AST nodes */
    
    if (!parser->arena) {
        HYP_FREE(parser);
        return NULL;
    }
    
    /* Prime the parser with the first token */
    advance(parser);
    
    return parser;
}

void hyp_parser_destroy(hyp_parser_t* parser) {
    if (!parser) return;
    
    if (parser->arena) {
        hyp_arena_destroy(parser->arena);
    }
    
    HYP_FREE(parser);
}

hyp_ast_node_t* hyp_parser_parse(hyp_parser_t* parser) {
    if (!parser) return NULL;
    
    hyp_ast_node_t* program = create_node(parser, HYP_AST_PROGRAM);
    if (!program) return NULL;
    
    HYP_ARRAY_INIT(program->as.program.declarations);
    
    while (!match(parser, HYP_TOKEN_EOF)) {
        hyp_ast_node_t* decl = parse_declaration(parser);
        if (decl) {
            HYP_ARRAY_PUSH(program->as.program.declarations, decl);
        }
        
        if (parser->panic_mode) {
            synchronize(parser);
        }
    }
    
    return parser->had_error ? NULL : program;
}

bool hyp_parser_had_error(hyp_parser_t* parser) {
    return parser ? parser->had_error : true;
}