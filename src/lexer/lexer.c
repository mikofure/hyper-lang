/**
 * Hyper Programming Language - Lexer Implementation
 * 
 * Tokenizes .hxp source files into a stream of tokens for the parser.
 * Handles keywords, identifiers, literals, operators, and punctuation.
 */

#include "../../include/lexer.h"
#include "../../include/hyp_common.h"
#include <ctype.h>
#include <string.h>

/* Keyword lookup table */
static const struct {
    const char* keyword;
    hyp_token_type_t type;
} keywords[] = {
    {"let", TOKEN_LET},
    {"const", TOKEN_CONST},
    {"fn", TOKEN_FUNC},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {"while", TOKEN_WHILE},
    {"for", TOKEN_FOR},
    {"in", TOKEN_IN},
    {"return", TOKEN_RETURN},
    {"break", TOKEN_BREAK},
    {"continue", TOKEN_CONTINUE},
    {"match", TOKEN_MATCH},
    {"case", TOKEN_CASE},
    {"default", TOKEN_DEFAULT},
    {"struct", TOKEN_STRUCT},
    {"enum", TOKEN_ENUM},
    {"import", TOKEN_IMPORT},
    {"export", TOKEN_EXPORT},
    {"module", TOKEN_MODULE},
    {"true", TOKEN_TRUE},
    {"false", TOKEN_FALSE},
    {"null", TOKEN_NULL},
    {"and", TOKEN_AND},
    {"or", TOKEN_OR},
    {"not", TOKEN_NOT},
    {"async", TOKEN_ASYNC},
    {"await", TOKEN_AWAIT},
    {"try", TOKEN_TRY},
    {"catch", TOKEN_CATCH},
    {"finally", TOKEN_FINALLY},
    {"throw", TOKEN_THROW},
    {"state", TOKEN_STATE}
};

#define KEYWORD_COUNT (sizeof(keywords) / sizeof(keywords[0]))

/* Initialize lexer */
hyp_lexer_t* hyp_lexer_create(const char* source, const char* filename) {
    if (!source) return NULL;
    
    hyp_lexer_t* lexer = HYP_MALLOC(sizeof(hyp_lexer_t));
    if (!lexer) return NULL;
    
    lexer->source = source;
    lexer->source_length = strlen(source);
    lexer->current = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->jsx_depth = 0; /* Track JSX nesting depth */
    lexer->in_jsx = false; /* Track if we're inside JSX */
    lexer->has_error = false;
    lexer->error_message[0] = '\0';
    
    /* Initialize token array */
    lexer->tokens.data = NULL;
    lexer->tokens.count = 0;
    lexer->tokens.capacity = 0;
    
    lexer->arena = hyp_arena_create(8192); /* 8KB arena for tokens */
    
    if (!lexer->arena) {
        HYP_FREE(lexer);
        return NULL;
    }
    
    return lexer;
}

void hyp_lexer_destroy(hyp_lexer_t* lexer) {
    if (!lexer) return;
    
    if (lexer->arena) {
        hyp_arena_destroy(lexer->arena);
    }
    
    HYP_FREE(lexer);
}

/* Character utilities */
static bool is_at_end(hyp_lexer_t* lexer) {
    return lexer->current >= lexer->source_length || lexer->source[lexer->current] == '\0';
}

static char advance(hyp_lexer_t* lexer) {
    if (is_at_end(lexer)) return '\0';
    
    char c = lexer->source[lexer->current++];
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    
    return c;
}

static char peek(hyp_lexer_t* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->source[lexer->current];
}

static char peek_next(hyp_lexer_t* lexer) {
    if (lexer->current + 1 >= lexer->source_length) return '\0';
    return lexer->source[lexer->current + 1];
}

static bool match(hyp_lexer_t* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (lexer->source[lexer->current] != expected) return false;
    
    advance(lexer);
    return true;
}

/* Token creation */
static hyp_token_t make_token(hyp_lexer_t* lexer, hyp_token_type_t type, size_t start_pos) {
    hyp_token_t token;
    token.type = type;
    token.lexeme.data = (char*)(lexer->source + start_pos);
    token.lexeme.length = lexer->current - start_pos;
    token.lexeme.capacity = 0;
    token.line = lexer->line;
    token.column = lexer->column;
    token.position = start_pos;
    
    return token;
}

static hyp_token_t error_token(hyp_lexer_t* lexer, const char* message) {
    hyp_token_t token;
    token.type = TOKEN_ERROR;
    token.lexeme.data = (char*)message;
    token.lexeme.length = strlen(message);
    token.lexeme.capacity = 0;
    token.line = lexer->line;
    token.column = lexer->column;
    token.position = lexer->current;
    
    return token;
}

/* Skip whitespace and comments */
static void skip_whitespace(hyp_lexer_t* lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                advance(lexer);
                break;
            case '/':
                if (peek_next(lexer) == '/') {
                    /* Line comment */
                    while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                        advance(lexer);
                    }
                } else if (peek_next(lexer) == '*') {
                    /* Block comment */
                    advance(lexer); /* / */
                    advance(lexer); /* * */
                    
                    while (!is_at_end(lexer)) {
                        if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                            advance(lexer); /* * */
                            advance(lexer); /* / */
                            break;
                        }
                        advance(lexer);
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

/* Scan string literal */
static hyp_token_t scan_string(hyp_lexer_t* lexer, char quote, size_t start) {
    while (peek(lexer) != quote && !is_at_end(lexer)) {
        if (peek(lexer) == '\n') {
            /* Multi-line strings allowed */
        }
        if (peek(lexer) == '\\') {
            advance(lexer); /* Skip escape character */
            if (!is_at_end(lexer)) {
                advance(lexer); /* Skip escaped character */
            }
        } else {
            advance(lexer);
        }
    }
    
    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string");
    }
    
    /* Closing quote */
    advance(lexer);
    return make_token(lexer, TOKEN_STRING, start);
}

/* Scan number literal */
static hyp_token_t scan_number(hyp_lexer_t* lexer, size_t start) {
    while (isdigit(peek(lexer))) {
        advance(lexer);
    }
    
    /* Look for decimal part */
    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        advance(lexer); /* . */
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }
    
    /* Look for exponent */
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') {
            advance(lexer);
        }
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }
    
    return make_token(lexer, TOKEN_NUMBER, start);
}

/* Check if identifier is a keyword */
static hyp_token_type_t check_keyword(const char* start, size_t length) {
    for (size_t i = 0; i < KEYWORD_COUNT; i++) {
        if (strlen(keywords[i].keyword) == length &&
            memcmp(start, keywords[i].keyword, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENTIFIER;
}

/* Scan identifier or keyword */
static hyp_token_t scan_identifier(hyp_lexer_t* lexer, size_t start) {
    while (isalnum(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }
    
    hyp_token_type_t type = check_keyword(lexer->source + start, 
                                         lexer->current - start);
    return make_token(lexer, type, start);
}

/* Scan JSX tag name or attribute */
static hyp_token_t scan_jsx_identifier(hyp_lexer_t* lexer, size_t start) {
    while (isalnum(peek(lexer)) || peek(lexer) == '_' || peek(lexer) == '-') {
        advance(lexer);
    }
    return make_token(lexer, TOKEN_JSX_ATTRIBUTE, start);
}

/* Scan JSX text content */
static hyp_token_t scan_jsx_text(hyp_lexer_t* lexer, size_t start) {
    while (!is_at_end(lexer) && peek(lexer) != '<' && peek(lexer) != '{') {
        if (peek(lexer) == '\n') lexer->line++;
        advance(lexer);
    }
    return make_token(lexer, TOKEN_JSX_TEXT, start);
}

/* Scan JSX expression inside {} */
static hyp_token_t scan_jsx_expression(hyp_lexer_t* lexer, size_t start) {
    int brace_count = 1;
    advance(lexer); /* consume opening { */
    
    while (!is_at_end(lexer) && brace_count > 0) {
        char c = peek(lexer);
        if (c == '{') brace_count++;
        else if (c == '}') brace_count--;
        else if (c == '\n') lexer->line++;
        advance(lexer);
    }
    
    return make_token(lexer, TOKEN_JSX_EXPRESSION, start);
}

/* Main tokenization function */
hyp_token_t hyp_lexer_scan_token(hyp_lexer_t* lexer) {
    if (!lexer) {
        hyp_token_t error = {0};
        error.type = TOKEN_ERROR;
        return error;
    }
    
    skip_whitespace(lexer);
    
    size_t start = lexer->current;
    
    if (is_at_end(lexer)) {
        return make_token(lexer, TOKEN_EOF, start);
    }
    
    char c = advance(lexer);
    
    /* JSX text content */
    if (lexer->in_jsx && !isalpha(c) && c != '<' && c != '{' && c != '}' && c != '>' && c != '/') {
        return scan_jsx_text(lexer, start);
    }
    
    /* Identifiers and keywords */
    if (isalpha(c) || c == '_') {
        if (lexer->in_jsx) {
            return scan_jsx_identifier(lexer, start);
        }
        return scan_identifier(lexer, start);
    }
    
    /* Numbers */
    if (isdigit(c)) {
        return scan_number(lexer, start);
    }
    
    /* Single-character tokens */
    switch (c) {
        case '(': return make_token(lexer, TOKEN_LEFT_PAREN, start);
        case ')': return make_token(lexer, TOKEN_RIGHT_PAREN, start);
        case '{':
            if (lexer->in_jsx) {
                return scan_jsx_expression(lexer, start);
            }
            return make_token(lexer, TOKEN_LEFT_BRACE, start);
        case '}': return make_token(lexer, TOKEN_RIGHT_BRACE, start);
        case '[': return make_token(lexer, TOKEN_LEFT_BRACKET, start);
        case ']': return make_token(lexer, TOKEN_RIGHT_BRACKET, start);
        case ',': return make_token(lexer, TOKEN_COMMA, start);
        case '.': return make_token(lexer, TOKEN_DOT, start);
        case ';': return make_token(lexer, TOKEN_SEMICOLON, start);
        case ':': return make_token(lexer, TOKEN_COLON, start);
        case '?': return make_token(lexer, TOKEN_QUESTION, start);
        case '~': return make_token(lexer, TOKEN_TILDE, start);
        
        /* String literals */
        case '"':
        case '\'':
            return scan_string(lexer, c, start);
        
        /* Two-character tokens */
        case '!':
            return make_token(lexer, match(lexer, '=') ? TOKEN_NOT_EQUAL : TOKEN_NOT, start);
        case '=':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_EQUAL, start);
            } else if (match(lexer, '>')) {
                return make_token(lexer, TOKEN_ARROW, start);
            }
            return make_token(lexer, TOKEN_ASSIGN, start);
        case '<':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_LESS_EQUAL, start);
            } else if (match(lexer, '<')) {
                return make_token(lexer, TOKEN_LEFT_SHIFT, start);
            } else if (match(lexer, '/')) {
                return make_token(lexer, TOKEN_JSX_END_TAG, start);
            } else if (isalpha(peek(lexer))) {
                /* Potential JSX tag */
                lexer->in_jsx = true;
                lexer->jsx_depth++;
                return make_token(lexer, TOKEN_JSX_OPEN_TAG, start);
            }
            return make_token(lexer, TOKEN_LESS, start);
        case '>':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_GREATER_EQUAL, start);
            } else if (match(lexer, '>')) {
                return make_token(lexer, TOKEN_RIGHT_SHIFT, start);
            } else if (lexer->in_jsx) {
                return make_token(lexer, TOKEN_JSX_CLOSE_TAG, start);
            }
            return make_token(lexer, TOKEN_GREATER, start);
        case '+':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_PLUS_ASSIGN, start);
            } else if (match(lexer, '+')) {
                return make_token(lexer, TOKEN_INCREMENT, start);
            }
            return make_token(lexer, TOKEN_PLUS, start);
        case '-':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_MINUS_ASSIGN, start);
            } else if (match(lexer, '-')) {
                return make_token(lexer, TOKEN_DECREMENT, start);
            }
            return make_token(lexer, TOKEN_MINUS, start);
        case '*':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_MUL_ASSIGN, start);
            } else if (match(lexer, '*')) {
                return make_token(lexer, TOKEN_POWER, start);
            }
            return make_token(lexer, TOKEN_STAR, start);
        case '/':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_DIV_ASSIGN, start);
            } else if (match(lexer, '>') && lexer->in_jsx) {
                /* JSX self-closing tag */
                lexer->jsx_depth--;
                if (lexer->jsx_depth == 0) {
                    lexer->in_jsx = false;
                }
                return make_token(lexer, TOKEN_JSX_SELF_CLOSE, start);
            }
            return make_token(lexer, TOKEN_SLASH, start);
        case '%':
            return make_token(lexer, match(lexer, '=') ? TOKEN_PERCENT_EQUAL : TOKEN_PERCENT, start);
        case '&':
            if (match(lexer, '&')) {
                return make_token(lexer, TOKEN_AND, start);
            } else if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_AND_EQUAL, start);
            }
            return make_token(lexer, TOKEN_AMPERSAND, start);
        case '|':
            if (match(lexer, '|')) {
                return make_token(lexer, TOKEN_OR, start);
            } else if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_OR_EQUAL, start);
            }
            return make_token(lexer, TOKEN_PIPE, start);
        case '^':
            return make_token(lexer, match(lexer, '=') ? TOKEN_XOR_EQUAL : TOKEN_CARET, start);
    }
    
    return error_token(lexer, "Unexpected character");
}

/* Utility functions */
const char* hyp_token_type_name(hyp_token_type_t type) {
    switch (type) {
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_LET: return "LET";
        case TOKEN_CONST: return "CONST";
        case TOKEN_FUNC: return "FUNC";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_NULL: return "NULL";
        case TOKEN_LEFT_PAREN: return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case TOKEN_LEFT_BRACE: return "LEFT_BRACE";
        case TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_DOT: return "DOT";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_EQUAL: return "EQUAL";
        case TOKEN_NOT_EQUAL: return "NOT_EQUAL";
        case TOKEN_LESS: return "LESS";
        case TOKEN_GREATER: return "GREATER";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

hyp_token_t hyp_lexer_next_token(hyp_lexer_t* lexer) {
    return hyp_lexer_scan_token(lexer);
}

void hyp_token_print(const hyp_token_t* token) {
    if (!token) return;
    
    printf("Token: %s \"%.*s\" at line %zu, column %zu\n",
           hyp_token_type_name(token->type),
           (int)token->lexeme.length, token->lexeme.data,
           token->line, token->column);
}