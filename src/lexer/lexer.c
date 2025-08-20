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
    {"let", HYP_TOKEN_LET},
    {"const", HYP_TOKEN_CONST},
    {"fn", HYP_TOKEN_FN},
    {"if", HYP_TOKEN_IF},
    {"else", HYP_TOKEN_ELSE},
    {"while", HYP_TOKEN_WHILE},
    {"for", HYP_TOKEN_FOR},
    {"in", HYP_TOKEN_IN},
    {"return", HYP_TOKEN_RETURN},
    {"break", HYP_TOKEN_BREAK},
    {"continue", HYP_TOKEN_CONTINUE},
    {"match", HYP_TOKEN_MATCH},
    {"case", HYP_TOKEN_CASE},
    {"default", HYP_TOKEN_DEFAULT},
    {"struct", HYP_TOKEN_STRUCT},
    {"enum", HYP_TOKEN_ENUM},
    {"import", HYP_TOKEN_IMPORT},
    {"export", HYP_TOKEN_EXPORT},
    {"module", HYP_TOKEN_MODULE},
    {"true", HYP_TOKEN_TRUE},
    {"false", HYP_TOKEN_FALSE},
    {"null", HYP_TOKEN_NULL},
    {"and", HYP_TOKEN_AND},
    {"or", HYP_TOKEN_OR},
    {"not", HYP_TOKEN_NOT},
    {"async", HYP_TOKEN_ASYNC},
    {"await", HYP_TOKEN_AWAIT},
    {"try", HYP_TOKEN_TRY},
    {"catch", HYP_TOKEN_CATCH},
    {"finally", HYP_TOKEN_FINALLY},
    {"throw", HYP_TOKEN_THROW},
    {"state", HYP_TOKEN_STATE}
};

#define KEYWORD_COUNT (sizeof(keywords) / sizeof(keywords[0]))

/* Initialize lexer */
hyp_lexer_t* hyp_lexer_create(const char* source, const char* filename) {
    if (!source) return NULL;
    
    hyp_lexer_t* lexer = HYP_MALLOC(sizeof(hyp_lexer_t));
    if (!lexer) return NULL;
    
    lexer->source = source;
    lexer->filename = filename ? filename : "<unknown>";
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->start = source;
    lexer->arena = hyp_arena_create(8192); /* 8KB arena for tokens */
    lexer->jsx_depth = 0; /* Track JSX nesting depth */
    lexer->in_jsx = false; /* Track if we're inside JSX */
    
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
    return *lexer->current == '\0';
}

static char advance(hyp_lexer_t* lexer) {
    if (is_at_end(lexer)) return '\0';
    
    char c = *lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    
    return c;
}

static char peek(hyp_lexer_t* lexer) {
    return *lexer->current;
}

static char peek_next(hyp_lexer_t* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static bool match(hyp_lexer_t* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    
    advance(lexer);
    return true;
}

/* Token creation */
static hyp_token_t make_token(hyp_lexer_t* lexer, hyp_token_type_t type) {
    hyp_token_t token;
    token.type = type;
    token.start = lexer->start;
    token.length = (size_t)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    token.filename = lexer->filename;
    
    return token;
}

static hyp_token_t error_token(hyp_lexer_t* lexer, const char* message) {
    hyp_token_t token;
    token.type = HYP_TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    token.filename = lexer->filename;
    
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
static hyp_token_t scan_string(hyp_lexer_t* lexer, char quote) {
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
    return make_token(lexer, HYP_TOKEN_STRING);
}

/* Scan number literal */
static hyp_token_t scan_number(hyp_lexer_t* lexer) {
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
    
    return make_token(lexer, HYP_TOKEN_NUMBER);
}

/* Check if identifier is a keyword */
static hyp_token_type_t check_keyword(const char* start, size_t length) {
    for (size_t i = 0; i < KEYWORD_COUNT; i++) {
        if (strlen(keywords[i].keyword) == length &&
            memcmp(start, keywords[i].keyword, length) == 0) {
            return keywords[i].type;
        }
    }
    return HYP_TOKEN_IDENTIFIER;
}

/* Scan identifier or keyword */
static hyp_token_t scan_identifier(hyp_lexer_t* lexer) {
    while (isalnum(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }
    
    hyp_token_type_t type = check_keyword(lexer->start, 
                                         (size_t)(lexer->current - lexer->start));
    return make_token(lexer, type);
}

/* Scan JSX tag name or attribute */
static hyp_token_t scan_jsx_identifier(hyp_lexer_t* lexer) {
    while (isalnum(peek(lexer)) || peek(lexer) == '_' || peek(lexer) == '-') {
        advance(lexer);
    }
    return make_token(lexer, HYP_TOKEN_JSX_ATTRIBUTE);
}

/* Scan JSX text content */
static hyp_token_t scan_jsx_text(hyp_lexer_t* lexer) {
    while (!is_at_end(lexer) && peek(lexer) != '<' && peek(lexer) != '{') {
        if (peek(lexer) == '\n') lexer->line++;
        advance(lexer);
    }
    return make_token(lexer, HYP_TOKEN_JSX_TEXT);
}

/* Scan JSX expression inside {} */
static hyp_token_t scan_jsx_expression(hyp_lexer_t* lexer) {
    int brace_count = 1;
    advance(lexer); /* consume opening { */
    
    while (!is_at_end(lexer) && brace_count > 0) {
        char c = peek(lexer);
        if (c == '{') brace_count++;
        else if (c == '}') brace_count--;
        else if (c == '\n') lexer->line++;
        advance(lexer);
    }
    
    return make_token(lexer, HYP_TOKEN_JSX_EXPRESSION);
}

/* Main tokenization function */
hyp_token_t hyp_lexer_scan_token(hyp_lexer_t* lexer) {
    if (!lexer) {
        hyp_token_t error = {0};
        error.type = HYP_TOKEN_ERROR;
        return error;
    }
    
    skip_whitespace(lexer);
    
    lexer->start = lexer->current;
    
    if (is_at_end(lexer)) {
        return make_token(lexer, HYP_TOKEN_EOF);
    }
    
    char c = advance(lexer);
    
    /* JSX text content */
    if (lexer->in_jsx && !isalpha(c) && c != '<' && c != '{' && c != '}' && c != '>' && c != '/') {
        return scan_jsx_text(lexer);
    }
    
    /* Identifiers and keywords */
    if (isalpha(c) || c == '_') {
        if (lexer->in_jsx) {
            return scan_jsx_identifier(lexer);
        }
        return scan_identifier(lexer);
    }
    
    /* Numbers */
    if (isdigit(c)) {
        return scan_number(lexer);
    }
    
    /* Single-character tokens */
    switch (c) {
        case '(': return make_token(lexer, HYP_TOKEN_LEFT_PAREN);
        case ')': return make_token(lexer, HYP_TOKEN_RIGHT_PAREN);
        case '{':
            if (lexer->in_jsx) {
                return scan_jsx_expression(lexer);
            }
            return make_token(lexer, HYP_TOKEN_LEFT_BRACE);
        case '}': return make_token(lexer, HYP_TOKEN_RIGHT_BRACE);
        case '[': return make_token(lexer, HYP_TOKEN_LEFT_BRACKET);
        case ']': return make_token(lexer, HYP_TOKEN_RIGHT_BRACKET);
        case ',': return make_token(lexer, HYP_TOKEN_COMMA);
        case '.': return make_token(lexer, HYP_TOKEN_DOT);
        case ';': return make_token(lexer, HYP_TOKEN_SEMICOLON);
        case ':': return make_token(lexer, HYP_TOKEN_COLON);
        case '?': return make_token(lexer, HYP_TOKEN_QUESTION);
        case '~': return make_token(lexer, HYP_TOKEN_TILDE);
        
        /* String literals */
        case '"':
        case '\'':
            return scan_string(lexer, c);
        
        /* Two-character tokens */
        case '!':
            return make_token(lexer, match(lexer, '=') ? HYP_TOKEN_BANG_EQUAL : HYP_TOKEN_BANG);
        case '=':
            if (match(lexer, '=')) {
                return make_token(lexer, HYP_TOKEN_EQUAL_EQUAL);
            } else if (match(lexer, '>')) {
                return make_token(lexer, HYP_TOKEN_ARROW);
            }
            return make_token(lexer, HYP_TOKEN_EQUAL);
        case '<':
            if (match(lexer, '=')) {
                return make_token(lexer, HYP_TOKEN_LESS_EQUAL);
            } else if (match(lexer, '<')) {
                return make_token(lexer, HYP_TOKEN_LEFT_SHIFT);
            } else if (match(lexer, '/')) {
                return make_token(lexer, HYP_TOKEN_JSX_END_TAG);
            } else if (isalpha(peek(lexer))) {
                /* Potential JSX tag */
                lexer->in_jsx = true;
                lexer->jsx_depth++;
                return make_token(lexer, HYP_TOKEN_JSX_OPEN_TAG);
            }
            return make_token(lexer, HYP_TOKEN_LESS);
        case '>':
            if (match(lexer, '=')) {
                return make_token(lexer, HYP_TOKEN_GREATER_EQUAL);
            } else if (match(lexer, '>')) {
                return make_token(lexer, HYP_TOKEN_RIGHT_SHIFT);
            } else if (lexer->in_jsx) {
                return make_token(lexer, HYP_TOKEN_JSX_CLOSE_TAG);
            }
            return make_token(lexer, HYP_TOKEN_GREATER);
        case '+':
            if (match(lexer, '=')) {
                return make_token(lexer, HYP_TOKEN_PLUS_EQUAL);
            } else if (match(lexer, '+')) {
                return make_token(lexer, HYP_TOKEN_PLUS_PLUS);
            }
            return make_token(lexer, HYP_TOKEN_PLUS);
        case '-':
            if (match(lexer, '=')) {
                return make_token(lexer, HYP_TOKEN_MINUS_EQUAL);
            } else if (match(lexer, '-')) {
                return make_token(lexer, HYP_TOKEN_MINUS_MINUS);
            }
            return make_token(lexer, HYP_TOKEN_MINUS);
        case '*':
            if (match(lexer, '=')) {
                return make_token(lexer, HYP_TOKEN_STAR_EQUAL);
            } else if (match(lexer, '*')) {
                return make_token(lexer, HYP_TOKEN_STAR_STAR);
            }
            return make_token(lexer, HYP_TOKEN_STAR);
        case '/':
            if (match(lexer, '=')) {
                return make_token(lexer, HYP_TOKEN_SLASH_EQUAL);
            } else if (match(lexer, '>') && lexer->in_jsx) {
                /* JSX self-closing tag */
                lexer->jsx_depth--;
                if (lexer->jsx_depth == 0) {
                    lexer->in_jsx = false;
                }
                return make_token(lexer, HYP_TOKEN_JSX_SELF_CLOSE);
            }
            return make_token(lexer, HYP_TOKEN_SLASH);
        case '%':
            return make_token(lexer, match(lexer, '=') ? HYP_TOKEN_PERCENT_EQUAL : HYP_TOKEN_PERCENT);
        case '&':
            if (match(lexer, '&')) {
                return make_token(lexer, HYP_TOKEN_AND_AND);
            } else if (match(lexer, '=')) {
                return make_token(lexer, HYP_TOKEN_AND_EQUAL);
            }
            return make_token(lexer, HYP_TOKEN_AMPERSAND);
        case '|':
            if (match(lexer, '|')) {
                return make_token(lexer, HYP_TOKEN_OR_OR);
            } else if (match(lexer, '=')) {
                return make_token(lexer, HYP_TOKEN_OR_EQUAL);
            }
            return make_token(lexer, HYP_TOKEN_PIPE);
        case '^':
            return make_token(lexer, match(lexer, '=') ? HYP_TOKEN_XOR_EQUAL : HYP_TOKEN_CARET);
    }
    
    return error_token(lexer, "Unexpected character");
}

/* Utility functions */
const char* hyp_token_type_name(hyp_token_type_t type) {
    switch (type) {
        case HYP_TOKEN_IDENTIFIER: return "IDENTIFIER";
        case HYP_TOKEN_NUMBER: return "NUMBER";
        case HYP_TOKEN_STRING: return "STRING";
        case HYP_TOKEN_LET: return "LET";
        case HYP_TOKEN_CONST: return "CONST";
        case HYP_TOKEN_FN: return "FN";
        case HYP_TOKEN_IF: return "IF";
        case HYP_TOKEN_ELSE: return "ELSE";
        case HYP_TOKEN_WHILE: return "WHILE";
        case HYP_TOKEN_FOR: return "FOR";
        case HYP_TOKEN_RETURN: return "RETURN";
        case HYP_TOKEN_TRUE: return "TRUE";
        case HYP_TOKEN_FALSE: return "FALSE";
        case HYP_TOKEN_NULL: return "NULL";
        case HYP_TOKEN_LEFT_PAREN: return "LEFT_PAREN";
        case HYP_TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case HYP_TOKEN_LEFT_BRACE: return "LEFT_BRACE";
        case HYP_TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
        case HYP_TOKEN_COMMA: return "COMMA";
        case HYP_TOKEN_DOT: return "DOT";
        case HYP_TOKEN_SEMICOLON: return "SEMICOLON";
        case HYP_TOKEN_PLUS: return "PLUS";
        case HYP_TOKEN_MINUS: return "MINUS";
        case HYP_TOKEN_STAR: return "STAR";
        case HYP_TOKEN_SLASH: return "SLASH";
        case HYP_TOKEN_EQUAL: return "EQUAL";
        case HYP_TOKEN_EQUAL_EQUAL: return "EQUAL_EQUAL";
        case HYP_TOKEN_BANG_EQUAL: return "BANG_EQUAL";
        case HYP_TOKEN_LESS: return "LESS";
        case HYP_TOKEN_GREATER: return "GREATER";
        case HYP_TOKEN_EOF: return "EOF";
        case HYP_TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void hyp_token_print(const hyp_token_t* token) {
    if (!token) return;
    
    printf("Token: %s \"%.*s\" at %s:%zu:%zu\n",
           hyp_token_type_name(token->type),
           (int)token->length, token->start,
           token->filename, token->line, token->column);
}