/**
 * Hyper Programming Language - Runtime Executable (hyprun)
 * 
 * Command-line interface for executing Hyper programs.
 * Can run compiled bytecode, interpret AST, or execute transpiled code.
 */

#include "../../include/hyp_runtime.h"
#include "../../include/lexer.h"
#include "../../include/parser.h"
#include "../../include/hyp_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HYPRUN_VERSION "0.1.0"

/* Runtime options */
typedef struct {
    char* input_file;
    bool verbose;
    bool debug;
    bool show_help;
    bool show_version;
    bool interpret_mode;
    bool bytecode_mode;
    char* module_path;
} hyprun_options_t;

/* Print usage information */
static void print_usage(const char* program_name) {
    printf("Hyper Programming Language Runtime (hyprun) v%s\n\n", HYPRUN_VERSION);
    printf("Usage: %s [options] <input-file>\n\n", program_name);
    printf("Options:\n");
    printf("  -i, --interpret         Interpret source code directly\n");
    printf("  -b, --bytecode          Execute bytecode file\n");
    printf("  -m, --module-path <dir> Add module search path\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  -d, --debug             Debug mode\n");
    printf("  -h, --help              Show this help message\n");
    printf("      --version           Show version information\n\n");
    printf("File Types:\n");
    printf("  .hxp                    Hyper source code (requires --interpret)\n");
    printf("  .hyb                    Hyper bytecode\n");
    printf("  .c                      Transpiled C code (compile and run)\n\n");
    printf("Examples:\n");
    printf("  %s program.hyb\n", program_name);
    printf("  %s --interpret src/main.hxp\n", program_name);
    printf("  %s --debug --verbose app.hyb\n", program_name);
}

/* Print version information */
static void print_version(void) {
    printf("Hyper Programming Language Runtime (hyprun) v%s\n", HYPRUN_VERSION);
    printf("Built with C99/C11 for maximum performance\n");
    printf("Copyright (c) 2024 Hyper Language Project\n");
}

/* Determine file type from extension */
typedef enum {
    FILE_TYPE_UNKNOWN,
    FILE_TYPE_HYPER_SOURCE,
    FILE_TYPE_HYPER_BYTECODE,
    FILE_TYPE_C_SOURCE
} file_type_t;

static file_type_t get_file_type(const char* filename) {
    if (!filename) return FILE_TYPE_UNKNOWN;
    
    const char* dot = strrchr(filename, '.');
    if (!dot) return FILE_TYPE_UNKNOWN;
    
    if (strcmp(dot, ".hxp") == 0) {
        return FILE_TYPE_HYPER_SOURCE;
    } else if (strcmp(dot, ".hyb") == 0) {
        return FILE_TYPE_HYPER_BYTECODE;
    } else if (strcmp(dot, ".c") == 0) {
        return FILE_TYPE_C_SOURCE;
    }
    
    return FILE_TYPE_UNKNOWN;
}

/* Parse command-line arguments */
static bool parse_arguments(int argc, char* argv[], hyprun_options_t* options) {
    /* Initialize options */
    memset(options, 0, sizeof(hyprun_options_t));
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            options->show_help = true;
            return true;
        } else if (strcmp(argv[i], "--version") == 0) {
            options->show_version = true;
            return true;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interpret") == 0) {
            options->interpret_mode = true;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bytecode") == 0) {
            options->bytecode_mode = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            options->verbose = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            options->debug = true;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--module-path") == 0) {
            if (i + 1 < argc) {
                options->module_path = argv[++i];
            } else {
                fprintf(stderr, "Error: -m/--module-path requires an argument\n");
                return false;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option %s\n", argv[i]);
            return false;
        } else {
            /* Input file */
            if (!options->input_file) {
                options->input_file = argv[i];
            } else {
                fprintf(stderr, "Error: Multiple input files specified\n");
                return false;
            }
        }
    }
    
    /* Check if input file is required */
    if (!options->input_file && !options->show_help && !options->show_version) {
        fprintf(stderr, "Error: No input file specified\n");
        return false;
    }
    
    return true;
}

/* Execute Hyper source code by interpreting */
static int execute_source_code(hyprun_options_t* options) {
    if (options->verbose) {
        printf("Interpreting Hyper source: %s\n", options->input_file);
    }
    
    /* Read source file */
    if (options->verbose) {
        printf("Reading file: %s\n", options->input_file);
    }
    
    size_t source_size;
    char* source = hyp_read_file(options->input_file, &source_size);
    if (!source) {
        fprintf(stderr, "Error: Could not read file %s\n", options->input_file);
        return 1;
    }
    
    if (options->verbose) {
        printf("File read successfully, length: %zu\n", source_size);
        printf("First 100 characters: %.100s\n", source);
    }
    
    /* Create lexer */
    if (options->verbose) {
        printf("Creating lexer...\n");
    }
    
    hyp_lexer_t* lexer = hyp_lexer_create(source, options->input_file);
    if (!lexer) {
        fprintf(stderr, "Error: Could not create lexer\n");
        HYP_FREE(source);
        return 1;
    }
    
    if (options->verbose) {
        printf("Lexer created successfully\n");
    }
    
    /* Create parser */
    if (options->verbose) {
        printf("Creating parser...\n");
    }
    
    hyp_parser_t* parser = hyp_parser_create(lexer);
    if (!parser) {
        fprintf(stderr, "Error: Could not create parser\n");
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 1;
    }
    
    if (options->verbose) {
        printf("Parser created successfully\n");
    }
    
    /* Parse source */
    if (options->verbose) {
        printf("Starting to parse source code...\n");
    }
    
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
        printf("AST root type: %d\n", ast->type);
        if (ast->type == AST_PROGRAM) {
            printf("Program has %zu statements\n", ast->program.statements.count);
        }
    }
    
    /* Create runtime */
    hyp_runtime_t* runtime = hyp_runtime_create();
    if (!runtime) {
        fprintf(stderr, "Error: Could not create runtime\n");
        hyp_parser_destroy(parser);
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 1;
    }
    
    /* Execute AST */
    hyp_error_t result = hyp_runtime_execute_ast(runtime, ast);
    if (result != HYP_OK) {
        const char* error = hyp_runtime_get_error(runtime);
        fprintf(stderr, "Runtime error: %s\n", error ? error : "Unknown error");
        
        hyp_runtime_destroy(runtime);
        hyp_parser_destroy(parser);
        hyp_lexer_destroy(lexer);
        HYP_FREE(source);
        return 1;
    }
    
    if (options->verbose) {
        printf("Execution completed successfully\n");
    }
    
    /* Cleanup */
    hyp_runtime_destroy(runtime);
    hyp_parser_destroy(parser);
    hyp_lexer_destroy(lexer);
    HYP_FREE(source);
    
    return 0;
}

/* Execute Hyper bytecode */
static int execute_bytecode(hyprun_options_t* options) {
    if (options->verbose) {
        printf("Executing Hyper bytecode: %s\n", options->input_file);
    }
    
    /* TODO: Implement bytecode loading and execution */
    fprintf(stderr, "Error: Bytecode execution not yet implemented\n");
    return 1;
}

/* Compile and execute C code */
static int execute_c_code(hyprun_options_t* options) {
    if (options->verbose) {
        printf("Compiling and executing C code: %s\n", options->input_file);
    }
    
    /* TODO: Implement C compilation and execution */
    fprintf(stderr, "Error: C code execution not yet implemented\n");
    return 1;
}

/* Main execution function */
static int execute_file(hyprun_options_t* options) {
    if (!options->input_file) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }
    
    /* Check if file exists */
    if (!hyp_file_exists(options->input_file)) {
        fprintf(stderr, "Error: File '%s' does not exist\n", options->input_file);
        return 1;
    }
    
    /* Determine execution mode based on file type and options */
    file_type_t file_type = get_file_type(options->input_file);
    
    if (options->interpret_mode) {
        if (file_type != FILE_TYPE_HYPER_SOURCE) {
            fprintf(stderr, "Error: --interpret can only be used with .hxp files\n");
            return 1;
        }
        return execute_source_code(options);
    }
    
    if (options->bytecode_mode) {
        if (file_type != FILE_TYPE_HYPER_BYTECODE) {
            fprintf(stderr, "Error: --bytecode can only be used with .hyb files\n");
            return 1;
        }
        return execute_bytecode(options);
    }
    
    /* Auto-detect based on file extension */
    switch (file_type) {
        case FILE_TYPE_HYPER_SOURCE:
            fprintf(stderr, "Error: .hxp files require --interpret flag\n");
            return 1;
            
        case FILE_TYPE_HYPER_BYTECODE:
            return execute_bytecode(options);
            
        case FILE_TYPE_C_SOURCE:
            return execute_c_code(options);
            
        default:
            fprintf(stderr, "Error: Unknown file type for '%s'\n", options->input_file);
            fprintf(stderr, "Supported extensions: .hxp (with --interpret), .hyb, .c\n");
            return 1;
    }
}

/* Main entry point */
int main(int argc, char* argv[]) {
    hyprun_options_t options;
    
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
    
    /* Execute the file */
    return execute_file(&options);
}