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
#include <getopt.h>

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
    if (!target_str) return HYP_TARGET_C;
    
    if (strcmp(target_str, "c") == 0) {
        return HYP_TARGET_C;
    } else if (strcmp(target_str, "js") == 0 || strcmp(target_str, "javascript") == 0) {
        return HYP_TARGET_JAVASCRIPT;
    } else if (strcmp(target_str, "bytecode") == 0) {
        return HYP_TARGET_BYTECODE;
    } else if (strcmp(target_str, "asm") == 0 || strcmp(target_str, "assembly") == 0) {
        return HYP_TARGET_ASSEMBLY;
    } else if (strcmp(target_str, "llvm") == 0) {
        return HYP_TARGET_LLVM_IR;
    }
    
    return HYP_TARGET_C; /* Default */
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
        case HYP_TARGET_C: extension = ".c"; break;
        case HYP_TARGET_JAVASCRIPT: extension = ".js"; break;
        case HYP_TARGET_BYTECODE: extension = ".hyb"; break;
        case HYP_TARGET_ASSEMBLY: extension = ".s"; break;
        case HYP_TARGET_LLVM_IR: extension = ".ll"; break;
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
    options->target = HYP_TARGET_C;
    
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
    
    return true;
}

/* Print AST for debugging */
static void print_ast_node(hyp_ast_node_t* node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    
    switch (node->type) {
        case HYP_AST_PROGRAM:
            printf("Program\n");
            for (size_t i = 0; i < node->as.program.declarations.count; i++) {
                print_ast_node(node->as.program.declarations.data[i], indent + 1);
            }
            break;
        case HYP_AST_LITERAL:
            printf("Literal: ");
            switch (node->as.literal.type) {
                case HYP_LITERAL_NULL: printf("null"); break;
                case HYP_LITERAL_BOOLEAN: printf("%s", node->as.literal.as.boolean ? "true" : "false"); break;
                case HYP_LITERAL_NUMBER: printf("%.17g", node->as.literal.as.number); break;
                case HYP_LITERAL_STRING: printf("\"%s\"", node->as.literal.as.string ? node->as.literal.as.string : ""); break;
            }
            printf("\n");
            break;
        case HYP_AST_IDENTIFIER:
            printf("Identifier: %s\n", node->as.identifier.name ? node->as.identifier.name : "<unknown>");
            break;
        case HYP_AST_BINARY:
            printf("Binary: %d\n", node->as.binary.operator);
            print_ast_node(node->as.binary.left, indent + 1);
            print_ast_node(node->as.binary.right, indent + 1);
            break;
        case HYP_AST_FUNCTION:
            printf("Function: %s\n", node->as.function.name ? node->as.function.name : "<anonymous>");
            print_ast_node(node->as.function.body, indent + 1);
            break;
        case HYP_AST_VAR_DECL:
            printf("VarDecl: %s (%s)\n", 
                   node->as.var_decl.name ? node->as.var_decl.name : "<unknown>",
                   node->as.var_decl.is_const ? "const" : "let");
            if (node->as.var_decl.initializer) {
                print_ast_node(node->as.var_decl.initializer, indent + 1);
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
        printf("Compiling %s to %s...\n", 
               options->input_file, 
               hyp_target_name(options->target));
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
            token = hyp_lexer_scan_token(lexer);
            hyp_token_print(&token);
        } while (token.type != HYP_TOKEN_EOF && token.type != HYP_TOKEN_ERROR);
        
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
    if (!ast || hyp_parser_had_error(parser)) {
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
        .optimize = options->optimize,
        .debug = options->debug
    };
    
    hyp_codegen_t* codegen = hyp_codegen_create(options->target, &codegen_opts);
    if (!codegen) {
        fprintf(stderr, "Error: Could not create code generator\n");
        hyp_parser_destroy(parser);
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 1;
    }
    
    /* Generate code */
    hyp_error_t result = hyp_codegen_generate(codegen, ast);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: Code generation failed\n");
        hyp_codegen_destroy(codegen);
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
            hyp_codegen_destroy(codegen);
            hyp_parser_destroy(parser);
            hyp_lexer_destroy(lexer);
            HYP_FREE(source);
            return 1;
        }
    }
    
    /* Write output file */
    result = hyp_codegen_write_to_file(codegen, output_file);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: Could not write output file '%s'\n", output_file);
        if (free_output_file) HYP_FREE(output_file);
        hyp_codegen_destroy(codegen);
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
    hyp_codegen_destroy(codegen);
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