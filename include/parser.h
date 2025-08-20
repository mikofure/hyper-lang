/**
 * Hyper Programming Language - Parser
 * 
 * The parser builds an Abstract Syntax Tree (AST) from the token stream
 * produced by the lexer. It implements a recursive descent parser for
 * the Hyper language grammar.
 */

#ifndef HYP_PARSER_H
#define HYP_PARSER_H

#include "hyp_common.h"
#include "lexer.h"

/* Forward declarations */
typedef struct hyp_ast_node hyp_ast_node_t;
typedef struct hyp_parser hyp_parser_t;

/* AST node types */
typedef enum {
    /* Literals */
    AST_NUMBER,
    AST_STRING,
    AST_BOOLEAN,
    AST_IDENTIFIER,
    AST_NULL,
    
    /* Expressions */
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_ASSIGNMENT,
    AST_CALL,
    AST_MEMBER_ACCESS,
    AST_INDEX_ACCESS,
    AST_CONDITIONAL,
    AST_ARRAY_LITERAL,
    AST_OBJECT_LITERAL,
    AST_LAMBDA,
    
    /* Statements */
    AST_EXPRESSION_STMT,
    AST_VARIABLE_DECL,
    AST_FUNCTION_DECL,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_RETURN_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_BLOCK_STMT,
    AST_IMPORT_STMT,
    AST_EXPORT_STMT,
    AST_TYPE_DECL,
    AST_STRUCT_DECL,
    AST_ENUM_DECL,
    AST_MATCH_STMT,
    AST_TRY_STMT,
    
    /* Program */
    AST_PROGRAM
} hyp_ast_node_type_t;

/* Binary operators */
typedef enum {
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV,
    BINOP_MOD,
    BINOP_POW,
    BINOP_EQ,
    BINOP_NE,
    BINOP_LT,
    BINOP_LE,
    BINOP_GT,
    BINOP_GE,
    BINOP_AND,
    BINOP_OR,
    BINOP_BITWISE_AND,
    BINOP_BITWISE_OR,
    BINOP_BITWISE_XOR,
    BINOP_LEFT_SHIFT,
    BINOP_RIGHT_SHIFT,
    BINOP_PIPE
} hyp_binary_op_t;

/* Unary operators */
typedef enum {
    UNOP_PLUS,
    UNOP_MINUS,
    UNOP_NOT,
    UNOP_BITWISE_NOT,
    UNOP_INCREMENT,
    UNOP_DECREMENT
} hyp_unary_op_t;

/* Assignment operators */
typedef enum {
    ASSIGN_SIMPLE,
    ASSIGN_ADD,
    ASSIGN_SUB,
    ASSIGN_MUL,
    ASSIGN_DIV
} hyp_assign_op_t;

/* Type information */
typedef struct {
    char* name;
    bool is_array;
    bool is_optional;
    struct hyp_type* element_type;  /* For arrays */
} hyp_type_t;

/* Parameter for function declarations */
typedef struct {
    char* name;
    hyp_type_t* type;
    hyp_ast_node_t* default_value;
} hyp_parameter_t;

/* Array of parameters */
typedef HYP_ARRAY(hyp_parameter_t) hyp_parameter_array_t;

/* Array of AST nodes */
typedef HYP_ARRAY(hyp_ast_node_t*) hyp_ast_node_array_t;

/* Object literal property */
typedef struct {
    char* key;
    hyp_ast_node_t* value;
} hyp_object_property_t;

/* Array of object properties */
typedef HYP_ARRAY(hyp_object_property_t) hyp_object_property_array_t;

/* Match case */
typedef struct {
    hyp_ast_node_t* pattern;
    hyp_ast_node_t* guard;  /* Optional guard condition */
    hyp_ast_node_t* body;
} hyp_match_case_t;

/* Array of match cases */
typedef HYP_ARRAY(hyp_match_case_t) hyp_match_case_array_t;

/* AST node structure */
struct hyp_ast_node {
    hyp_ast_node_type_t type;
    size_t line;
    size_t column;
    
    union {
        /* Literals */
        struct {
            double value;
        } number;
        
        struct {
            char* value;
        } string;
        
        struct {
            bool value;
        } boolean;
        
        struct {
            char* name;
        } identifier;
        
        /* Binary operation */
        struct {
            hyp_binary_op_t op;
            hyp_ast_node_t* left;
            hyp_ast_node_t* right;
        } binary_op;
        
        /* Unary operation */
        struct {
            hyp_unary_op_t op;
            hyp_ast_node_t* operand;
            bool is_postfix;
        } unary_op;
        
        /* Assignment */
        struct {
            hyp_assign_op_t op;
            hyp_ast_node_t* target;
            hyp_ast_node_t* value;
        } assignment;
        
        /* Function call */
        struct {
            hyp_ast_node_t* callee;
            hyp_ast_node_array_t arguments;
        } call;
        
        /* Member access (obj.member) */
        struct {
            hyp_ast_node_t* object;
            char* member;
        } member_access;
        
        /* Index access (obj[index]) */
        struct {
            hyp_ast_node_t* object;
            hyp_ast_node_t* index;
        } index_access;
        
        /* Conditional (ternary) operator */
        struct {
            hyp_ast_node_t* condition;
            hyp_ast_node_t* then_expr;
            hyp_ast_node_t* else_expr;
        } conditional;
        
        /* Array literal */
        struct {
            hyp_ast_node_array_t elements;
        } array_literal;
        
        /* Object literal */
        struct {
            hyp_object_property_array_t properties;
        } object_literal;
        
        /* Lambda function */
        struct {
            hyp_parameter_array_t parameters;
            hyp_ast_node_t* body;
            hyp_type_t* return_type;
        } lambda;
        
        /* Expression statement */
        struct {
            hyp_ast_node_t* expression;
        } expression_stmt;
        
        /* Variable declaration */
        struct {
            char* name;
            hyp_type_t* type;
            hyp_ast_node_t* initializer;
            bool is_const;
        } variable_decl;
        
        /* Function declaration */
        struct {
            char* name;
            hyp_parameter_array_t parameters;
            hyp_type_t* return_type;
            hyp_ast_node_t* body;
            bool is_async;
            bool is_exported;
        } function_decl;
        
        /* If statement */
        struct {
            hyp_ast_node_t* condition;
            hyp_ast_node_t* then_stmt;
            hyp_ast_node_t* else_stmt;
        } if_stmt;
        
        /* While statement */
        struct {
            hyp_ast_node_t* condition;
            hyp_ast_node_t* body;
        } while_stmt;
        
        /* For statement */
        struct {
            hyp_ast_node_t* init;
            hyp_ast_node_t* condition;
            hyp_ast_node_t* update;
            hyp_ast_node_t* body;
        } for_stmt;
        
        /* Return statement */
        struct {
            hyp_ast_node_t* value;
        } return_stmt;
        
        /* Block statement */
        struct {
            hyp_ast_node_array_t statements;
        } block_stmt;
        
        /* Import statement */
        struct {
            char* module;
            char* alias;
            hyp_ast_node_array_t imports;  /* Specific imports */
        } import_stmt;
        
        /* Export statement */
        struct {
            hyp_ast_node_t* declaration;
        } export_stmt;
        
        /* Match statement */
        struct {
            hyp_ast_node_t* expression;
            hyp_match_case_array_t cases;
        } match_stmt;
        
        /* Try statement */
        struct {
            hyp_ast_node_t* try_block;
            char* catch_variable;
            hyp_ast_node_t* catch_block;
            hyp_ast_node_t* finally_block;
        } try_stmt;
        
        /* Program (root node) */
        struct {
            hyp_ast_node_array_t statements;
        } program;
    };
};

/* Parser state */
struct hyp_parser {
    hyp_lexer_t* lexer;
    hyp_token_t current;
    hyp_token_t previous;
    hyp_arena_t* arena;
    bool had_error;
    bool panic_mode;
};

/* Function declarations */

/**
 * Create a new parser instance
 * @param lexer The lexer instance to use
 * @return New parser instance, or NULL on failure
 */
hyp_parser_t* hyp_parser_create(hyp_lexer_t* lexer);

/**
 * Initialize parser with token array
 * @param parser The parser to initialize
 * @param tokens Array of tokens from lexer
 * @param arena Arena allocator for AST nodes
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hyp_parser_init(hyp_parser_t* parser, hyp_token_array_t* tokens, hyp_arena_t* arena);

/**
 * Parse tokens into an AST
 * @param parser The parser instance
 * @return Root AST node (program), or NULL on error
 */
hyp_ast_node_t* hyp_parser_parse(hyp_parser_t* parser);

/* Parsing functions for different constructs */
hyp_ast_node_t* hyp_parse_program(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_statement(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_expression(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_assignment(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_conditional(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_logical_or(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_logical_and(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_equality(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_comparison(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_term(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_factor(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_unary(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_postfix(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_primary(hyp_parser_t* parser);

/* Statement parsing */
hyp_ast_node_t* hyp_parse_variable_declaration(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_function_declaration(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_if_statement(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_while_statement(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_for_statement(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_return_statement(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_block_statement(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_import_statement(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_export_statement(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_match_statement(hyp_parser_t* parser);
hyp_ast_node_t* hyp_parse_try_statement(hyp_parser_t* parser);

/* Utility functions */
hyp_token_t* hyp_parser_current_token(hyp_parser_t* parser);
hyp_token_t* hyp_parser_previous_token(hyp_parser_t* parser);
hyp_token_t* hyp_parser_peek_token(hyp_parser_t* parser);
bool hyp_parser_check(hyp_parser_t* parser, hyp_token_type_t type);
bool hyp_parser_match(hyp_parser_t* parser, hyp_token_type_t type);
bool hyp_parser_match_any(hyp_parser_t* parser, hyp_token_type_t* types, size_t count);
hyp_token_t* hyp_parser_advance(hyp_parser_t* parser);
bool hyp_parser_is_at_end(hyp_parser_t* parser);
hyp_token_t* hyp_parser_consume(hyp_parser_t* parser, hyp_token_type_t type, const char* message);

/* AST node creation */
hyp_ast_node_t* hyp_ast_create_node(hyp_parser_t* parser, hyp_ast_node_type_t type);
hyp_ast_node_t* hyp_ast_create_number(hyp_parser_t* parser, double value);
hyp_ast_node_t* hyp_ast_create_string(hyp_parser_t* parser, const char* value);
hyp_ast_node_t* hyp_ast_create_boolean(hyp_parser_t* parser, bool value);
hyp_ast_node_t* hyp_ast_create_identifier(hyp_parser_t* parser, const char* name);
hyp_ast_node_t* hyp_ast_create_binary_op(hyp_parser_t* parser, hyp_binary_op_t op, hyp_ast_node_t* left, hyp_ast_node_t* right);
hyp_ast_node_t* hyp_ast_create_unary_op(hyp_parser_t* parser, hyp_unary_op_t op, hyp_ast_node_t* operand, bool is_postfix);

/* Type parsing */
hyp_type_t* hyp_parse_type(hyp_parser_t* parser);
hyp_parameter_t hyp_parse_parameter(hyp_parser_t* parser);

/* Error handling */
void hyp_parser_error(hyp_parser_t* parser, const char* message);
void hyp_parser_synchronize(hyp_parser_t* parser);

/* AST utilities */
void hyp_ast_print(hyp_ast_node_t* node, int indent);
void hyp_ast_free(hyp_ast_node_t* node);
const char* hyp_ast_node_type_name(hyp_ast_node_type_t type);

/* Parser cleanup */
void hyp_parser_destroy(hyp_parser_t* parser);

#endif /* HYP_PARSER_H */