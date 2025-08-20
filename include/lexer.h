/**
 * Hyper Programming Language - Lexer
 * 
 * The lexer converts Hyper source code (.hxp files) into a stream of tokens
 * for the parser to process. It handles keywords, identifiers, literals,
 * operators, and punctuation.
 */

#ifndef HYP_LEXER_H
#define HYP_LEXER_H

#include "hyp_common.h"

/* Token types for the Hyper language */
typedef enum {
    /* Literals */
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_IDENTIFIER,
    TOKEN_BOOLEAN,
    
    /* Keywords */
    TOKEN_LET,
    TOKEN_CONST,
    TOKEN_FUNC,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_RETURN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_IMPORT,
    TOKEN_EXPORT,
    TOKEN_TYPE,
    TOKEN_STRUCT,
    TOKEN_ENUM,
    TOKEN_MATCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_MODULE,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_THROW,
    TOKEN_ASYNC,
    TOKEN_AWAIT,
    TOKEN_TRY,
    TOKEN_CATCH,
    TOKEN_FINALLY,
    TOKEN_STATE,         /* state keyword for reactive state */
    
    /* Operators */
    TOKEN_PLUS,          /* + */
    TOKEN_MINUS,         /* - */
    TOKEN_STAR,          /* * */
    TOKEN_MULTIPLY,      /* * */
    TOKEN_SLASH,         /* / */
    TOKEN_DIVIDE,        /* / */
    TOKEN_PERCENT,       /* % */
    TOKEN_MODULO,        /* % */
    TOKEN_POWER,         /* ** */
    TOKEN_ASSIGN,        /* = */
    TOKEN_PERCENT_EQUAL, /* %= */
    TOKEN_PLUS_ASSIGN,   /* += */
    TOKEN_MINUS_ASSIGN,  /* -= */
    TOKEN_MUL_ASSIGN,    /* *= */
    TOKEN_DIV_ASSIGN,    /* /= */
    TOKEN_EQUAL,         /* == */
    TOKEN_NOT_EQUAL,     /* != */
    TOKEN_LESS,          /* < */
    TOKEN_LESS_EQUAL,    /* <= */
    TOKEN_GREATER,       /* > */
    TOKEN_GREATER_EQUAL, /* >= */
    TOKEN_AND,           /* && */
    TOKEN_OR,            /* || */
    TOKEN_NOT,           /* ! */
    TOKEN_AMPERSAND,     /* & */
    TOKEN_BITWISE_AND,   /* & */
    TOKEN_AND_EQUAL,     /* &= */
    TOKEN_BITWISE_OR,    /* | */
    TOKEN_OR_EQUAL,      /* |= */
    TOKEN_CARET,         /* ^ */
    TOKEN_BITWISE_XOR,   /* ^ */
    TOKEN_XOR_EQUAL,     /* ^= */
    TOKEN_TILDE,         /* ~ */
    TOKEN_BITWISE_NOT,   /* ~ */
    TOKEN_LEFT_SHIFT,    /* << */
    TOKEN_RIGHT_SHIFT,   /* >> */
    TOKEN_INCREMENT,     /* ++ */
    TOKEN_DECREMENT,     /* -- */
    TOKEN_ARROW,         /* -> */
    TOKEN_PIPE,          /* |> */
    TOKEN_QUESTION,      /* ? */
    TOKEN_COLON,         /* : */
    TOKEN_DOUBLE_COLON,  /* :: */
    
    /* Punctuation */
    TOKEN_LEFT_PAREN,    /* ( */
    TOKEN_RIGHT_PAREN,   /* ) */
    TOKEN_LEFT_BRACE,    /* { */
    TOKEN_RIGHT_BRACE,   /* } */
    TOKEN_LEFT_BRACKET,  /* [ */
    TOKEN_RIGHT_BRACKET, /* ] */
    TOKEN_SEMICOLON,     /* ; */
    TOKEN_COMMA,         /* , */
    TOKEN_DOT,           /* . */
    TOKEN_DOUBLE_DOT,    /* .. */
    TOKEN_TRIPLE_DOT,    /* ... */
    
    /* JSX/HTML Tokens */
    TOKEN_JSX_OPEN_TAG,      /* < for JSX opening tag */
    TOKEN_JSX_CLOSE_TAG,     /* > for JSX closing tag */
    TOKEN_JSX_SELF_CLOSE,    /* /> for self-closing JSX tag */
    TOKEN_JSX_END_TAG,       /* </ for JSX ending tag */
    TOKEN_JSX_EXPRESSION,    /* {expression} in JSX */
    TOKEN_JSX_TEXT,          /* text content in JSX */
    TOKEN_JSX_ATTRIBUTE,     /* attribute name in JSX */
    TOKEN_JSX_EQUALS,        /* = in JSX attributes */
    
    /* Special */
    TOKEN_NEWLINE,
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_COMMENT
} hyp_token_type_t;

/* Token structure */
typedef struct {
    hyp_token_type_t type;
    hyp_string_t lexeme;  /* The actual text */
    size_t line;
    size_t column;
    size_t position;      /* Absolute position in source */
    
    /* Token value (for literals) */
    union {
        double number;
        bool boolean;
        char* string;
    } value;
} hyp_token_t;

/* Token array */
typedef HYP_ARRAY(hyp_token_t) hyp_token_array_t;

/* Lexer state */
typedef struct {
    const char* source;
    size_t source_length;
    size_t current;
    size_t line;
    size_t column;
    hyp_token_array_t tokens;
    hyp_arena_t* arena;  /* For memory management */
    bool has_error;
    char error_message[256];
    int jsx_depth;       /* Track JSX nesting depth */
    bool in_jsx;         /* Track if we're inside JSX */
} hyp_lexer_t;

/* Keyword lookup table entry */
typedef struct {
    const char* keyword;
    hyp_token_type_t token_type;
} hyp_keyword_entry_t;

/* Function declarations */

/**
 * Initialize a lexer with source code
 * @param lexer The lexer to initialize
 * @param source The source code to tokenize
 * @param arena Arena allocator for memory management
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hyp_lexer_init(hyp_lexer_t* lexer, const char* source, hyp_arena_t* arena);

/**
 * Tokenize the entire source code
 * @param lexer The lexer instance
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hyp_lexer_tokenize(hyp_lexer_t* lexer);

/**
 * Get the next token from the source
 * @param lexer The lexer instance
 * @return The next token
 */
hyp_token_t hyp_lexer_next_token(hyp_lexer_t* lexer);

/**
 * Peek at the current character without consuming it
 * @param lexer The lexer instance
 * @return The current character
 */
char hyp_lexer_peek(hyp_lexer_t* lexer);

/**
 * Peek at the next character without consuming it
 * @param lexer The lexer instance
 * @return The next character
 */
char hyp_lexer_peek_next(hyp_lexer_t* lexer);

/**
 * Advance to the next character
 * @param lexer The lexer instance
 * @return The consumed character
 */
char hyp_lexer_advance(hyp_lexer_t* lexer);

/**
 * Check if we've reached the end of source
 * @param lexer The lexer instance
 * @return true if at end, false otherwise
 */
bool hyp_lexer_is_at_end(hyp_lexer_t* lexer);

/**
 * Skip whitespace characters
 * @param lexer The lexer instance
 */
void hyp_lexer_skip_whitespace(hyp_lexer_t* lexer);

/**
 * Scan a string literal
 * @param lexer The lexer instance
 * @return The string token
 */
hyp_token_t hyp_lexer_scan_string(hyp_lexer_t* lexer);

/**
 * Scan a number literal
 * @param lexer The lexer instance
 * @return The number token
 */
hyp_token_t hyp_lexer_scan_number(hyp_lexer_t* lexer);

/**
 * Scan an identifier or keyword
 * @param lexer The lexer instance
 * @return The identifier/keyword token
 */
hyp_token_t hyp_lexer_scan_identifier(hyp_lexer_t* lexer);

/**
 * Scan a comment
 * @param lexer The lexer instance
 * @return The comment token
 */
hyp_token_t hyp_lexer_scan_comment(hyp_lexer_t* lexer);

/**
 * Look up a keyword
 * @param text The text to look up
 * @return The token type, or TOKEN_IDENTIFIER if not a keyword
 */
hyp_token_type_t hyp_lexer_lookup_keyword(const char* text);

/**
 * Create a token
 * @param lexer The lexer instance
 * @param type The token type
 * @param start The start position of the token
 * @return The created token
 */
hyp_token_t hyp_lexer_make_token(hyp_lexer_t* lexer, hyp_token_type_t type, size_t start);

/**
 * Create an error token
 * @param lexer The lexer instance
 * @param message The error message
 * @return The error token
 */
hyp_token_t hyp_lexer_error_token(hyp_lexer_t* lexer, const char* message);

/**
 * Get token type name for debugging
 * @param type The token type
 * @return String representation of the token type
 */
const char* hyp_token_type_name(hyp_token_type_t type);

/**
 * Print token for debugging
 * @param token The token to print
 */
void hyp_token_print(const hyp_token_t* token);

/**
 * Free token resources
 * @param token The token to free
 */
void hyp_token_free(hyp_token_t* token);

/**
 * Create a new lexer instance
 * @param source The source code to tokenize
 * @param filename The filename for error reporting
 * @return New lexer instance, or NULL on failure
 */
hyp_lexer_t* hyp_lexer_create(const char* source, const char* filename);

/**
 * Destroy lexer and free resources
 * @param lexer The lexer to destroy
 */
void hyp_lexer_destroy(hyp_lexer_t* lexer);

/* Character classification helpers */
HYP_INLINE bool hyp_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

HYP_INLINE bool hyp_is_digit(char c) {
    return c >= '0' && c <= '9';
}

HYP_INLINE bool hyp_is_alnum(char c) {
    return hyp_is_alpha(c) || hyp_is_digit(c);
}

HYP_INLINE bool hyp_is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

#endif /* HYP_LEXER_H */