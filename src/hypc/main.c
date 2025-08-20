/**
 * Hyper Programming Language - Compiler CLI (hypc)
 * 
 * Command-line interface for the Hyper compiler.
 * Supports building, transpiling, and various compilation options.
 */

#include "../../include/lexer.h"
#include "../../include/parser.h"
#include "../../include/transpiler.h"
#include "../../include/hyp_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
    // Windows doesn't have getopt.h, we'll use a simple alternative
    char* optarg = NULL;
    int optind = 1;
    
    #define no_argument 0
    #define required_argument 1
    #define optional_argument 2
    
    struct option {
        const char *name;
        int has_arg;
        int *flag;
        int val;
    };
    

#else
    #include <getopt.h>
#endif

#define HYPC_VERSION "0.1.0"

/* Command-line options */
typedef struct {
    char* input_file;
    char* output_file;
    hyp_target_t target;
    bool verbose;
    bool debug;
    bool optimize;
    bool show_help;
    bool show_version;
    bool show_ast;
    bool show_tokens;
} hypc_options_t;

#ifdef _WIN32
/* Simple argument parsing for Windows */
static bool parse_args_win32(int argc, char* argv[], hypc_options_t* options) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            options->show_help = true;
            return 1;
        } else if (strcmp(argv[i], "--version") == 0) {
            options->show_version = true;
            return 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            options->verbose = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            options->debug = true;
        } else if (strcmp(argv[i], "-O") == 0 || strcmp(argv[i], "--optimize") == 0) {
            options->optimize = true;
        } else if (strcmp(argv[i], "--show-ast") == 0) {
            options->show_ast = true;
        } else if (strcmp(argv[i], "--show-tokens") == 0) {
            options->show_tokens = true;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            options->output_file = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "c") == 0) {
                options->target = TARGET_C;
            } else if (strcmp(argv[i], "js") == 0) {
                options->target = TARGET_JAVASCRIPT;
            } else if (strcmp(argv[i], "bytecode") == 0) {
                options->target = TARGET_BYTECODE;
            } else {
                fprintf(stderr, "Error: Unknown target '%s'\n", argv[i]);
                return 0;
            }
        } else if (argv[i][0] != '-') {
            options->input_file = argv[i];
        }
    }
    return 0;
}
#endif

/* Print usage information */
static void print_usage(const char* program_name) {
    printf("Hyper Programming Language Compiler (hypc) v%s\n\n", HYPC_VERSION);
    printf("Usage: %s [options] <input-file>\n\n", program_name);
    printf("Options:\n");
    printf("  -o, --output <file>     Output file (default: auto-generated)\n");
    printf("  -t, --target <target>   Target language (c, js, bytecode, asm, llvm)\n");
    printf("  -O, --optimize          Enable optimizations\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  -d, --debug             Debug mode\n");
    printf("      --show-ast          Print AST and exit\n");
    printf("      --show-tokens       Print tokens and exit\n");
    printf("  -h, --help              Show this help message\n");
    printf("      --version           Show version information\n\n");
    printf("Targets:\n");
    printf("  c                       Transpile to C code\n");
    printf("  js, javascript          Transpile to JavaScript\n");
    printf("  bytecode                Compile to bytecode\n");
    printf("  asm, assembly           Compile to assembly\n");
    printf("  llvm                    Generate LLVM IR\n\n");
    printf("Examples:\n");
    printf("  %s build src/main.hxp\n", program_name);
    printf("  %s transpile src/app.hxp --target js -o app.js\n", program_name);
    printf("  %s --show-ast src/test.hxp\n", program_name);
}

/* Print version information */
static void print_version(void) {
    printf("Hyper Programming Language Compiler (hypc) v%s\n", HYPC_VERSION);
    printf("Built with C99/C11 for maximum performance\n");
    printf("Copyright (c) 2024 Hyper Language Project\n");
}

/* Parse target string */
static hyp_target_t parse_target(const char* target_str) {
    if (!target_str) return TARGET_C;
    
    if (strcmp(target_str, "c") == 0) {
        return TARGET_C;
    } else if (strcmp(target_str, "js") == 0 || strcmp(target_str, "javascript") == 0) {
        return TARGET_JAVASCRIPT;
    } else if (strcmp(target_str, "bytecode") == 0) {
        return TARGET_BYTECODE;
    } else if (strcmp(target_str, "asm") == 0 || strcmp(target_str, "assembly") == 0) {
        return TARGET_ASSEMBLY;
    } else if (strcmp(target_str, "llvm") == 0) {
        return TARGET_LLVM_IR;
    }
    
    return TARGET_C; /* Default */
}

/* Generate output filename based on target */
static char* generate_output_filename(const char* input_file, hyp_target_t target) {
    if (!input_file) return NULL;
    
    /* Find the last dot to replace extension */
    const char* dot = strrchr(input_file, '.');
    const char* slash = strrchr(input_file, '/');
    const char* backslash = strrchr(input_file, '\\');
    
    /* Use the rightmost path separator */
    const char* last_sep = slash > backslash ? slash : backslash;
    
    /* If dot comes before the last path separator, ignore it */
    if (dot && last_sep && dot < last_sep) {
        dot = NULL;
    }
    
    size_t base_len = dot ? (size_t)(dot - input_file) : strlen(input_file);
    
    const char* extension;
    switch (target) {
        case TARGET_C: extension = ".c"; break;
        case TARGET_JAVASCRIPT: extension = ".js"; break;
        case TARGET_BYTECODE: extension = ".hyb"; break;
        case TARGET_ASSEMBLY: extension = ".s"; break;
        case TARGET_LLVM_IR: extension = ".ll"; break;
        default: extension = ".out"; break;
    }
    
    size_t output_len = base_len + strlen(extension) + 1;
    char* output_file = HYP_MALLOC(output_len);
    if (!output_file) return NULL;
    
    strncpy(output_file, input_file, base_len);
    output_file[base_len] = '\0';
    strcat(output_file, extension);
    
    return output_file;
}

/* Parse command-line arguments */
static bool parse_arguments(int argc, char* argv[], hypc_options_t* options) {
    static struct option long_options[] = {
        {"output", required_argument, 0, 'o'},
        {"target", required_argument, 0, 't'},
        {"optimize", no_argument, 0, 'O'},
        {"verbose", no_argument, 0, 'v'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 1000},
        {"show-ast", no_argument, 0, 1001},
        {"show-tokens", no_argument, 0, 1002},
        {0, 0, 0, 0}
    };
    
    /* Initialize options */
    memset(options, 0, sizeof(hypc_options_t));
    options->target = TARGET_C;
    
#ifdef _WIN32
    if (parse_args_win32(argc, argv, options)) {
        return true;
    }
#else
    int c;
    int option_index = 0;
    
    while ((c = getopt_long(argc, argv, "o:t:Ovdh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'o':
                options->output_file = optarg;
                break;
            case 't':
                options->target = parse_target(optarg);
                break;
            case 'O':
                options->optimize = true;
                break;
            case 'v':
                options->verbose = true;
                break;
            case 'd':
                options->debug = true;
                break;
            case 'h':
                options->show_help = true;
                return true;
            case 1000: /* --version */
                options->show_version = true;
                return true;
            case 1001: /* --show-ast */
                options->show_ast = true;
                break;
            case 1002: /* --show-tokens */
                options->show_tokens = true;
                break;
            case '?':
                return false;
            default:
                return false;
        }
    }
    
    /* Get input file */
    if (optind < argc) {
        options->input_file = argv[optind];
    } else if (!options->show_help && !options->show_version) {
        fprintf(stderr, "Error: No input file specified\n");
        return false;
    }
#endif
    
    /* Validate input file for both platforms */
    if (!options->input_file && !options->show_help && !options->show_version) {
        fprintf(stderr, "Error: No input file specified\n");
        return false;
    }
    
    return true;
}

/* Print AST for debugging */
static void print_ast_node(hyp_ast_node_t* node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    
    switch (node->type) {
        case AST_BLOCK_STMT:
            printf("Block\n");
            for (size_t i = 0; i < node->block_stmt.statements.count; i++) {
                print_ast_node(node->block_stmt.statements.data[i], indent + 1);
            }
            break;
        case AST_NUMBER:
            printf("Number: %.17g\n", node->number.value);
            break;
        case AST_STRING:
            printf("String: \"%s\"\n", node->string.value ? node->string.value : "");
            break;
        case AST_BOOLEAN:
            printf("Boolean: %s\n", node->boolean.value ? "true" : "false");
            break;
        case AST_NULL:
            printf("Null\n");
            break;
        case AST_IDENTIFIER:
            printf("Identifier: %s\n", node->identifier.name ? node->identifier.name : "<unknown>");
            break;
        case AST_BINARY_OP:
            printf("Binary: %d\n", node->binary_op.op);
            print_ast_node(node->binary_op.left, indent + 1);
            print_ast_node(node->binary_op.right, indent + 1);
            break;
        case AST_FUNCTION_DECL:
            printf("Function: %s\n", node->function_decl.name ? node->function_decl.name : "<anonymous>");
            print_ast_node(node->function_decl.body, indent + 1);
            break;
        case AST_VARIABLE_DECL:
            printf("VarDecl: %s (%s)\n", 
                   node->variable_decl.name ? node->variable_decl.name : "<unknown>",
                   node->variable_decl.is_const ? "const" : "let");
            if (node->variable_decl.initializer) {
                print_ast_node(node->variable_decl.initializer, indent + 1);
            }
            break;
        default:
            printf("Node type: %d\n", node->type);
            break;
    }
}

/* Main compilation function */
static int compile_file(hypc_options_t* options) {
    if (options->verbose) {
        printf("Compiling %s...\n", 
               options->input_file);
    }
    
    /* Read source file */
    size_t source_size;
    char* source = hyp_read_file(options->input_file, &source_size);
    if (!source) {
        fprintf(stderr, "Error: Could not read file '%s'\n", options->input_file);
        return 1;
    }
    
    /* Create lexer */
    hyp_lexer_t* lexer = hyp_lexer_create(source, options->input_file);
    if (!lexer) {
        fprintf(stderr, "Error: Could not create lexer\n");
        HYP_FREE(source);
        return 1;
    }
    
    /* Show tokens if requested */
    if (options->show_tokens) {
        printf("Tokens for %s:\n", options->input_file);
        hyp_token_t token;
        do {
            token = hyp_lexer_next_token(lexer);
            hyp_token_print(&token);
        } while (token.type != TOKEN_EOF && token.type != TOKEN_ERROR);
        
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 0;
    }
    
    /* Create parser */
    hyp_parser_t* parser = hyp_parser_create(lexer);
    if (!parser) {
        fprintf(stderr, "Error: Could not create parser\n");
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 1;
    }
    
    /* Parse source */
    hyp_ast_node_t* ast = hyp_parser_parse(parser);
    if (!ast || parser->had_error) {
        fprintf(stderr, "Error: Parsing failed\n");
        hyp_parser_destroy(parser);
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 1;
    }
    
    if (options->verbose) {
        printf("Parsing completed successfully\n");
    }
    
    /* Show AST if requested */
    if (options->show_ast) {
        printf("AST for %s:\n", options->input_file);
        print_ast_node(ast, 0);
        
        hyp_parser_destroy(parser);
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 0;
    }
    
    /* Create code generator */
    hyp_codegen_options_t codegen_opts = {
        .target = options->target,
        .optimize = options->optimize,
        .debug_info = options->debug
    };
    
    hyp_codegen_t codegen;
    hyp_error_t result = hyp_codegen_init(&codegen, &codegen_opts, NULL);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: Could not initialize code generator\n");
        hyp_parser_destroy(parser);
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 1;
    }
    
    /* Generate code */
    result = hyp_codegen_generate(&codegen, ast);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: Code generation failed\n");
        hyp_codegen_destroy(&codegen);
        hyp_parser_destroy(parser);
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 1;
    }
    
    if (options->verbose) {
        printf("Code generation completed successfully\n");
    }
    
    /* Determine output file */
    char* output_file = options->output_file;
    bool free_output_file = false;
    
    if (!output_file) {
        output_file = generate_output_filename(options->input_file, options->target);
        free_output_file = true;
        
        if (!output_file) {
            fprintf(stderr, "Error: Could not generate output filename\n");
            hyp_codegen_destroy(&codegen);
        hyp_parser_destroy(parser);
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 1;
    }
    }
    
    /* Write output file */
    /* Write output to file */
     FILE* output = fopen(output_file, "w");
     if (!output) {
         fprintf(stderr, "Error: Could not open output file '%s'\n", output_file);
         result = HYP_ERROR_IO;
     } else {
         const char* generated_code = hyp_codegen_get_output(&codegen);
         if (generated_code) {
             fputs(generated_code, output);
             fclose(output);
             result = HYP_OK;
         } else {
             fclose(output);
             result = HYP_ERROR_RUNTIME;
         }
     }
    if (result != HYP_OK) {
        fprintf(stderr, "Error: Could not write output file '%s'\n", output_file);
        if (free_output_file) HYP_FREE(output_file);
        hyp_codegen_destroy(&codegen);
        hyp_parser_destroy(parser);
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 1;
    }
    
    if (options->verbose) {
        printf("Output written to %s\n", output_file);
    }
    
    /* Cleanup */
    if (free_output_file) HYP_FREE(output_file);
    hyp_codegen_destroy(&codegen);
    hyp_parser_destroy(parser);
    hyp_lexer_destroy(lexer);
    HYP_FREE(source);
    
    return 0;
}

/* Main entry point */
int main(int argc, char* argv[]) {
    hypc_options_t options;
    
    /* Parse command-line arguments */
    if (!parse_arguments(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Handle special options */
    if (options.show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (options.show_version) {
        print_version();
        return 0;
    }
    
    /* Compile the file */
    return compile_file(&options);
}