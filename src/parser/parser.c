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

/* Parser implementation */

/* Forward declarations */
static hyp_ast_node_t* parse_expression(hyp_parser_t* parser);
static hyp_ast_node_t* parse_statement(hyp_parser_t* parser);
static hyp_ast_node_t* parse_declaration(hyp_parser_t* parser);

/* Error handling */
static void error_at(hyp_parser_t* parser, hyp_token_t* token, const char* message) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    
    fprintf(stderr, "[line %zu:%zu] Error", token->line, token->column);
    
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        /* Nothing */
    } else {
        fprintf(stderr, " at '%.*s'", (int)token->lexeme.length, token->lexeme.data);
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
        parser->current = hyp_lexer_next_token(parser->lexer);
        if (parser->current.type != TOKEN_ERROR) break;
        
        error_at_current(parser, parser->current.lexeme.data);
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
    
    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) return;
        
        switch (parser->current.type) {
            case TOKEN_FUNC:
            case TOKEN_LET:
            case TOKEN_CONST:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_RETURN:
                return;
            default:
                break;
        }
        
        advance(parser);
    }
}

/* Expression parsing */
static hyp_ast_node_t* parse_primary(hyp_parser_t* parser) {
    if (match(parser, TOKEN_BOOLEAN)) {
        hyp_ast_node_t* node = create_node(parser, AST_BOOLEAN);
        if (node) {
            node->boolean.value = parser->previous.value.boolean;
        }
        return node;
    }
    
    if (match(parser, TOKEN_NUMBER)) {
        hyp_ast_node_t* node = create_node(parser, AST_NUMBER);
        if (node) {
            node->number.value = parser->previous.value.number;
        }
        return node;
    }
    
    if (match(parser, TOKEN_STRING)) {
        hyp_ast_node_t* node = create_node(parser, AST_STRING);
        if (node) {
            node->string.value = copy_string(parser, parser->previous.lexeme.data, parser->previous.lexeme.length);
        }
        return node;
    }
    
    if (match(parser, TOKEN_IDENTIFIER)) {
        hyp_ast_node_t* node = create_node(parser, AST_IDENTIFIER);
        if (node) {
            node->identifier.name = copy_string(parser, 
                parser->previous.lexeme.data, parser->previous.lexeme.length);
        }
        return node;
    }
    
    if (match(parser, TOKEN_LEFT_PAREN)) {
        hyp_ast_node_t* expr = parse_expression(parser);
        consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
        return expr;
    }
    
    if (match(parser, TOKEN_LEFT_BRACKET)) {
        hyp_ast_node_t* node = create_node(parser, AST_ARRAY_LITERAL);
        if (!node) return NULL;
        
        HYP_ARRAY_INIT(&node->array_literal.elements);
        
        if (!check(parser, TOKEN_RIGHT_BRACKET)) {
            do {
                hyp_ast_node_t* element = parse_expression(parser);
                if (element) {
                    HYP_ARRAY_PUSH(&node->array_literal.elements, element);
                }
            } while (match(parser, TOKEN_COMMA));
        }
        
        consume(parser, TOKEN_RIGHT_BRACKET, "Expected ']' after array elements");
        return node;
    }
    
    if (match(parser, TOKEN_LEFT_BRACE)) {
        hyp_ast_node_t* node = create_node(parser, AST_OBJECT_LITERAL);
        if (!node) return NULL;
        
        HYP_ARRAY_INIT(&node->object_literal.properties);
        
        if (!check(parser, TOKEN_RIGHT_BRACE)) {
            do {
                hyp_object_property_t prop = {0};
                
                if (match(parser, TOKEN_IDENTIFIER)) {
                    prop.key = copy_string(parser, parser->previous.lexeme.data, parser->previous.lexeme.length);
                } else if (match(parser, TOKEN_STRING)) {
                    prop.key = copy_string(parser, parser->previous.lexeme.data + 1, parser->previous.lexeme.length - 2);
                } else {
                    error(parser, "Expected property name");
                    break;
                }
                
                consume(parser, TOKEN_COLON, "Expected ':' after property name");
                prop.value = parse_expression(parser);
                
                HYP_ARRAY_PUSH(&node->object_literal.properties, prop);
            } while (match(parser, TOKEN_COMMA));
        }
        
        consume(parser, TOKEN_RIGHT_BRACE, "Expected '}' after object properties");
        return node;
    }
    
    error(parser, "Expected expression");
    return NULL;
}

static hyp_ast_node_t* parse_call(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_primary(parser);
    
    while (true) {
        if (match(parser, TOKEN_LEFT_PAREN)) {
            hyp_ast_node_t* call = create_node(parser, AST_CALL);
            if (!call) break;
            
            call->call.callee = expr;
            HYP_ARRAY_INIT(&call->call.arguments);
            
            if (!check(parser, TOKEN_RIGHT_PAREN)) {
                do {
                    hyp_ast_node_t* arg = parse_expression(parser);
                    if (arg) {
                        HYP_ARRAY_PUSH(&call->call.arguments, arg);
                    }
                } while (match(parser, TOKEN_COMMA));
            }
            
            consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
            expr = call;
        } else if (match(parser, TOKEN_DOT)) {
            hyp_ast_node_t* member = create_node(parser, AST_MEMBER_ACCESS);
            if (!member) break;
            
            member->member_access.object = expr;
            consume(parser, TOKEN_IDENTIFIER, "Expected property name after '.'");
            member->member_access.member = copy_string(parser, 
                parser->previous.lexeme.data, parser->previous.lexeme.length);
            
            expr = member;
        } else if (match(parser, TOKEN_LEFT_BRACKET)) {
            hyp_ast_node_t* index = create_node(parser, AST_INDEX_ACCESS);
            if (!index) break;
            
            index->index_access.object = expr;
            index->index_access.index = parse_expression(parser);
            consume(parser, TOKEN_RIGHT_BRACKET, "Expected ']' after index");
            
            expr = index;
        } else {
            break;
        }
    }
    
    return expr;
}

static hyp_ast_node_t* parse_unary(hyp_parser_t* parser) {
    if (match(parser, TOKEN_NOT) || match(parser, TOKEN_MINUS)) {
        hyp_ast_node_t* node = create_node(parser, AST_UNARY_OP);
        if (!node) return NULL;
        
        hyp_token_type_t op_type = parser->previous.type;
         switch (op_type) {
              case TOKEN_NOT: node->unary_op.op = UNOP_NOT; break;
              case TOKEN_MINUS: node->unary_op.op = UNOP_MINUS; break;
             default: break;
         }
         
         node->unary_op.operand = parse_unary(parser);
        return node;
    }
    
    return parse_call(parser);
}

static hyp_ast_node_t* parse_factor(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_unary(parser);
    
    while (match(parser, TOKEN_DIVIDE) || match(parser, TOKEN_MULTIPLY) || match(parser, TOKEN_MODULO)) {
        hyp_ast_node_t* binary = create_node(parser, AST_BINARY_OP);
        if (!binary) break;
        
        hyp_token_type_t op_type = parser->previous.type;
        switch (op_type) {
            case TOKEN_DIVIDE: binary->binary_op.op = BINOP_DIV; break;
            case TOKEN_MULTIPLY: binary->binary_op.op = BINOP_MUL; break;
            case TOKEN_MODULO: binary->binary_op.op = BINOP_MOD; break;
            default: break;
        }
         
        binary->binary_op.left = expr;
        binary->binary_op.right = parse_unary(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_term(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_factor(parser);
    
    while (match(parser, TOKEN_MINUS) || match(parser, TOKEN_PLUS)) {
        hyp_ast_node_t* binary = create_node(parser, AST_BINARY_OP);
        if (!binary) break;
        
        hyp_token_type_t op_type = parser->previous.type;
        switch (op_type) {
            case TOKEN_MINUS: binary->binary_op.op = BINOP_SUB; break;
            case TOKEN_PLUS: binary->binary_op.op = BINOP_ADD; break;
            default: break;
        }
         
        binary->binary_op.left = expr;
        binary->binary_op.right = parse_factor(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_comparison(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_term(parser);
    
    while (match(parser, TOKEN_GREATER) || match(parser, TOKEN_GREATER_EQUAL) ||
           match(parser, TOKEN_LESS) || match(parser, TOKEN_LESS_EQUAL)) {
        hyp_ast_node_t* binary = create_node(parser, AST_BINARY_OP);
        if (!binary) break;
        
        hyp_token_type_t op_type = parser->previous.type;
        switch (op_type) {
            case TOKEN_GREATER: binary->binary_op.op = BINOP_GT; break;
            case TOKEN_GREATER_EQUAL: binary->binary_op.op = BINOP_GE; break;
            case TOKEN_LESS: binary->binary_op.op = BINOP_LT; break;
            case TOKEN_LESS_EQUAL: binary->binary_op.op = BINOP_LE; break;
            default: break;
        }
         
        binary->binary_op.left = expr;
        binary->binary_op.right = parse_term(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_equality(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_comparison(parser);
    
    while (match(parser, TOKEN_NOT_EQUAL) || match(parser, TOKEN_EQUAL)) {
        hyp_ast_node_t* binary = create_node(parser, AST_BINARY_OP);
        if (!binary) break;
        
        hyp_token_type_t op_type = parser->previous.type;
        switch (op_type) {
            case TOKEN_NOT_EQUAL: binary->binary_op.op = BINOP_NE; break;
            case TOKEN_EQUAL: binary->binary_op.op = BINOP_EQ; break;
            default: break;
        }
         
        binary->binary_op.left = expr;
        binary->binary_op.right = parse_comparison(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_logical_and(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_equality(parser);
    
    while (match(parser, TOKEN_AND)) {
         hyp_ast_node_t* binary = create_node(parser, AST_BINARY_OP);
        if (!binary) break;
        
        binary->binary_op.op = BINOP_AND;
        binary->binary_op.left = expr;
        binary->binary_op.right = parse_equality(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_logical_or(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_logical_and(parser);
    
    while (match(parser, TOKEN_OR)) {
         hyp_ast_node_t* binary = create_node(parser, AST_BINARY_OP);
        if (!binary) break;
        
        binary->binary_op.op = BINOP_OR;
        binary->binary_op.left = expr;
        binary->binary_op.right = parse_logical_and(parser);
        expr = binary;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_assignment(hyp_parser_t* parser) {
    hyp_ast_node_t* expr = parse_logical_or(parser);
    
    if (match(parser, TOKEN_EQUAL)) {
        hyp_ast_node_t* assignment = create_node(parser, AST_ASSIGNMENT);
        if (!assignment) return expr;
        
        assignment->assignment.op = ASSIGN_SIMPLE;
        assignment->assignment.target = expr;
        assignment->assignment.value = parse_assignment(parser);
        return assignment;
    }
    
    return expr;
}

static hyp_ast_node_t* parse_expression(hyp_parser_t* parser) {
    return parse_assignment(parser);
}

/* Statement parsing */
static hyp_ast_node_t* parse_expression_statement(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, AST_EXPRESSION_STMT);
    if (!node) return NULL;
    
    node->expression_stmt.expression = parse_expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression");
    
    return node;
}

static hyp_ast_node_t* parse_block_statement(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, AST_BLOCK_STMT);
    if (!node) return NULL;
    
    HYP_ARRAY_INIT(&node->block_stmt.statements);
    
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        hyp_ast_node_t* stmt = parse_declaration(parser);
        if (stmt) {
            HYP_ARRAY_PUSH(&node->block_stmt.statements, stmt);
        }
    }
    
    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}' after block");
    return node;
}

static hyp_ast_node_t* parse_if_statement(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, AST_IF_STMT);
    if (!node) return NULL;
    
    consume(parser, TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    node->if_stmt.condition = parse_expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after if condition");
    
    node->if_stmt.then_stmt = parse_statement(parser);
    
    if (match(parser, TOKEN_ELSE)) {
        node->if_stmt.else_stmt = parse_statement(parser);
    }
    
    return node;
}

static hyp_ast_node_t* parse_while_statement(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, AST_WHILE_STMT);
    if (!node) return NULL;
    
    consume(parser, TOKEN_LEFT_PAREN, "Expected '(' after 'while'");
    node->while_stmt.condition = parse_expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after while condition");
    
    node->while_stmt.body = parse_statement(parser);
    
    return node;
}

static hyp_ast_node_t* parse_return_statement(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, AST_RETURN_STMT);
    if (!node) return NULL;
    
    if (!check(parser, TOKEN_SEMICOLON)) {
        node->return_stmt.value = parse_expression(parser);
    }
    
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after return value");
    return node;
}

static hyp_ast_node_t* parse_statement(hyp_parser_t* parser) {
    if (match(parser, TOKEN_IF)) {
        return parse_if_statement(parser);
    }
    
    if (match(parser, TOKEN_WHILE)) {
        return parse_while_statement(parser);
    }
    
    if (match(parser, TOKEN_RETURN)) {
        return parse_return_statement(parser);
    }
    
    if (match(parser, TOKEN_LEFT_BRACE)) {
        return parse_block_statement(parser);
    }
    
    return parse_expression_statement(parser);
}

/* Declaration parsing */
static hyp_ast_node_t* parse_variable_declaration(hyp_parser_t* parser, bool is_const) {
    hyp_ast_node_t* node = create_node(parser, AST_VARIABLE_DECL);
    if (!node) return NULL;
    
    node->variable_decl.is_const = is_const;
    
    consume(parser, TOKEN_IDENTIFIER, "Expected variable name");
    node->variable_decl.name = copy_string(parser, parser->previous.lexeme.data, parser->previous.lexeme.length);
    
    if (match(parser, TOKEN_EQUAL)) {
        node->variable_decl.initializer = parse_expression(parser);
    }
    
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    return node;
}

static hyp_ast_node_t* parse_function_declaration(hyp_parser_t* parser) {
    hyp_ast_node_t* node = create_node(parser, AST_FUNCTION_DECL);
    if (!node) return NULL;
    
    consume(parser, TOKEN_IDENTIFIER, "Expected function name");
    node->function_decl.name = copy_string(parser, parser->previous.lexeme.data, parser->previous.lexeme.length);
    
    consume(parser, TOKEN_LEFT_PAREN, "Expected '(' after function name");
    
    HYP_ARRAY_INIT(&node->function_decl.parameters);
    
    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            hyp_parameter_t param = {0};
            
            consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");
            param.name = copy_string(parser, parser->previous.lexeme.data, parser->previous.lexeme.length);
            param.type = NULL; /* Type inference for now */
            
            HYP_ARRAY_PUSH(&node->function_decl.parameters, param);
        } while (match(parser, TOKEN_COMMA));
    }
    
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after parameters");
    consume(parser, TOKEN_LEFT_BRACE, "Expected '{' before function body");
    
    node->function_decl.body = parse_block_statement(parser);
    
    return node;
}

static hyp_ast_node_t* parse_declaration(hyp_parser_t* parser) {
    if (match(parser, TOKEN_LET)) {
        return parse_variable_declaration(parser, false);
    }
    
    if (match(parser, TOKEN_CONST)) {
        return parse_variable_declaration(parser, true);
    }
    
    if (match(parser, TOKEN_FUNC)) {
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
    
    hyp_ast_node_t* program = create_node(parser, AST_PROGRAM);
    if (!program) return NULL;
    
    HYP_ARRAY_INIT(&program->program.statements);
    
    while (!match(parser, TOKEN_EOF)) {
        hyp_ast_node_t* decl = parse_declaration(parser);
        if (decl) {
            HYP_ARRAY_PUSH(&program->program.statements, decl);
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