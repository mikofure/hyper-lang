/**
 * Hyper Programming Language - Package Executor CLI (hpx)
 * 
 * Command-line interface for the Hyper package executor.
 * Executes CLI tools and templates as one-shot commands.
 */

#include "../../include/hpx.h"
#include "../../include/hyp_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
    // Windows doesn't have getopt.h, we'll use a simple alternative
    char* optarg = NULL;
    int optind = 1;
    
    // Simple argument parsing for Windows
    static int parse_args_win32(int argc, char* argv[], hpx_options_t* options) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                options->show_help = true;
                return 1;
            } else if (strcmp(argv[i], "--version") == 0) {
                options->show_version = true;
                return 1;
            } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
                options->verbose = true;
            } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
                options->list_commands = true;
            } else if (strcmp(argv[i], "-C") == 0 && i + 1 < argc) {
                options->working_dir = argv[++i];
            } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
                options->timeout = atoi(argv[++i]);
                if (options->timeout <= 0) {
                    fprintf(stderr, "Error: Invalid timeout value\n");
                    return -1;
                }
            } else if (strcmp(argv[i], "--offline") == 0) {
                options->offline = true;
            } else if (strcmp(argv[i], "--no-install") == 0) {
                options->no_install = true;
            } else if (strcmp(argv[i], "--clear-cache") == 0) {
                options->clear_cache = true;
            } else if (argv[i][0] != '-') {
                if (!options->package_spec) {
                    options->package_spec = argv[i];
                } else if (!options->command) {
                    options->command = argv[i];
                }
            }
        }
        return 0;
    }
#else
    #include <getopt.h>
#endif

#define HPX_VERSION "0.1.0"

/* CLI options */
typedef struct {
    char* package_spec;
    char* command;
    char** args;
    int arg_count;
    bool verbose;
    bool offline;
    bool no_install;
    bool clear_cache;
    bool show_help;
    bool show_version;
    bool list_commands;
    char* working_dir;
    int timeout;
} hpx_options_t;

/* Print usage information */
static void print_usage(const char* program_name) {
    printf("Hyper Programming Language Package Executor (hpx) v%s\n\n", HPX_VERSION);
    printf("Usage: %s [options] <package[@version]> [command] [args...]\n\n", program_name);
    printf("Description:\n");
    printf("  Execute CLI tools and templates as one-shot commands.\n");
    printf("  Similar to npx, but for the Hyper ecosystem.\n\n");
    printf("Arguments:\n");
    printf("  package[@version]       Package to execute (with optional version)\n");
    printf("  command                 Command to run (optional)\n");
    printf("  args                    Arguments to pass to the command\n\n");
    printf("Options:\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("      --offline           Work in offline mode\n");
    printf("      --no-install        Don't install missing packages\n");
    printf("      --clear-cache       Clear package cache\n");
    printf("  -l, --list-commands     List available commands for package\n");
    printf("  -C, --directory <dir>   Change to directory before execution\n");
    printf("  -t, --timeout <sec>     Set execution timeout in seconds\n");
    printf("  -h, --help              Show this help message\n");
    printf("      --version           Show version information\n\n");
    printf("Examples:\n");
    printf("  %s create-hyp-app my-app\n", program_name);
    printf("  %s @hyper/cli@latest init\n", program_name);
    printf("  %s typescript tsc --version\n", program_name);
    printf("  %s --list-commands webpack\n", program_name);
    printf("  %s --clear-cache\n", program_name);
    printf("\nTemplate Creation:\n");
    printf("  %s create-hyp-app my-project\n", program_name);
    printf("  %s @hyper/template-cli my-cli-tool\n", program_name);
    printf("  %s @hyper/template-web my-web-app\n", program_name);
}

/* Print version information */
static void print_version(void) {
    printf("Hyper Programming Language Package Executor (hpx) v%s\n", HPX_VERSION);
    printf("Built with C99/C11 for maximum performance\n");
    printf("Copyright (c) 2024 Hyper Language Project\n");
}

/* Parse command-line arguments */
static bool parse_arguments(int argc, char* argv[], hpx_options_t* options) {
    static struct option long_options[] = {
        {"verbose", no_argument, 0, 'v'},
        {"offline", no_argument, 0, 1000},
        {"no-install", no_argument, 0, 1001},
        {"clear-cache", no_argument, 0, 1002},
        {"list-commands", no_argument, 0, 'l'},
        {"directory", required_argument, 0, 'C'},
        {"timeout", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 1003},
        {0, 0, 0, 0}
    };
    
    /* Initialize options */
    memset(options, 0, sizeof(hpx_options_t));
    options->timeout = 300; /* Default 5 minutes */
    
#ifdef _WIN32
    if (parse_args_win32(argc, argv, options)) {
        return true;
    }
#else
    int c;
    int option_index = 0;
    
    while ((c = getopt_long(argc, argv, "vlC:t:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'v':
                options->verbose = true;
                break;
            case 'l':
                options->list_commands = true;
                break;
            case 'C':
                options->working_dir = optarg;
                break;
            case 't':
                options->timeout = atoi(optarg);
                if (options->timeout <= 0) {
                    fprintf(stderr, "Error: Invalid timeout value\n");
                    return false;
                }
                break;
            case 'h':
                options->show_help = true;
                return true;
            case 1000: /* --offline */
                options->offline = true;
                break;
            case 1001: /* --no-install */
                options->no_install = true;
                break;
            case 1002: /* --clear-cache */
                options->clear_cache = true;
                return true;
            case 1003: /* --version */
                options->show_version = true;
                return true;
            case '?':
                return false;
            default:
                return false;
        }
    }
#endif
    
    /* Parse remaining arguments */
    if (optind < argc && !options->clear_cache) {
        options->package_spec = argv[optind++];
        
        if (optind < argc && !options->list_commands) {
            options->command = argv[optind++];
        }
        
        /* Collect remaining arguments */
        if (optind < argc) {
            options->arg_count = argc - optind;
            options->args = &argv[optind];
        }
    } else if (!options->clear_cache && !options->show_help && !options->show_version) {
        fprintf(stderr, "Error: Package specification required\n");
        return false;
    }
    
    return true;
}

/* Execute clear cache command */
static int execute_clear_cache(hpx_context_t* hpx, hpx_options_t* options) {
    if (options->verbose) {
        printf("Clearing package cache...\n");
    }
    
    hyp_error_t result = hpx_clear_cache(hpx);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hpx_get_error(hpx));
        return 1;
    }
    
    printf("Package cache cleared successfully\n");
    return 0;
}

/* Execute list commands */
static int execute_list_commands(hpx_context_t* hpx, hpx_options_t* options) {
    if (!options->package_spec) {
        fprintf(stderr, "Error: Package specification required for listing commands\n");
        return 1;
    }
    
    if (options->verbose) {
        printf("Listing commands for package: %s\n", options->package_spec);
    }
    
    hyp_string_t** commands;
    size_t command_count;
    
    hyp_error_t result = hpx_list_commands(hpx, options->package_spec, &commands, &command_count);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hpx_get_error(hpx));
        return 1;
    }
    
    if (command_count == 0) {
        printf("No commands available for package '%s'\n", options->package_spec);
    } else {
        printf("Available commands for '%s':\n", options->package_spec);
        for (size_t i = 0; i < command_count; i++) {
            printf("  %s\n", commands[i]->data);
            hyp_string_destroy(commands[i]);
        }
        HYP_FREE(commands);
    }
    
    return 0;
}

/* Execute package command */
static int execute_package(hpx_context_t* hpx, hpx_options_t* options) {
    if (!options->package_spec) {
        fprintf(stderr, "Error: Package specification required\n");
        return 1;
    }
    
    /* Check if package is executable */
    bool is_executable;
    hyp_error_t check_result = hpx_is_executable(hpx, options->package_spec, &is_executable);
    if (check_result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hpx_get_error(hpx));
        return 1;
    }
    
    if (!is_executable) {
        fprintf(stderr, "Error: Package '%s' is not executable\n", options->package_spec);
        return 1;
    }
    
    /* Prepare execution options */
    hpx_exec_options_t exec_options;
    memset(&exec_options, 0, sizeof(hpx_exec_options_t));
    
    exec_options.command = options->command;
    exec_options.args = options->args;
    exec_options.arg_count = options->arg_count;
    exec_options.working_directory = options->working_dir;
    exec_options.timeout_seconds = options->timeout;
    exec_options.capture_output = true;
    exec_options.inherit_environment = true;
    
    if (options->verbose) {
        printf("Executing package: %s\n", options->package_spec);
        if (options->command) {
            printf("Command: %s\n", options->command);
        }
        if (options->arg_count > 0) {
            printf("Arguments:");
            for (int i = 0; i < options->arg_count; i++) {
                printf(" %s", options->args[i]);
            }
            printf("\n");
        }
    }
    
    /* Execute package */
    hpx_exec_result_t result;
    hyp_error_t exec_result = hpx_execute_package(hpx, options->package_spec, &exec_options, &result);
    
    if (exec_result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hpx_get_error(hpx));
        return 1;
    }
    
    /* Print output */
    if (result.output && result.output->data) {
        printf("%s", result.output->data);
    }
    
    if (result.error_message && result.error_message->data) {
        fprintf(stderr, "%s", result.error_message->data);
    }
    
    if (options->verbose) {
        printf("\nExecution completed in %lu ms\n", result.execution_time_ms);
        printf("Exit code: %d\n", result.exit_code);
    }
    
    /* Cleanup result */
    if (result.package_spec) hyp_string_destroy(result.package_spec);
    if (result.command) hyp_string_destroy(result.command);
    if (result.output) hyp_string_destroy(result.output);
    if (result.error_message) hyp_string_destroy(result.error_message);
    
    return result.exit_code;
}

/* Check if package spec looks like a template */
static bool is_template_package(const char* package_spec) {
    if (!package_spec) return false;
    
    /* Check for common template patterns */
    if (strstr(package_spec, "create-") == package_spec) return true;
    if (strstr(package_spec, "template-") != NULL) return true;
    if (strstr(package_spec, "generator-") == package_spec) return true;
    
    return false;
}

/* Execute template creation */
static int execute_template(hpx_context_t* hpx, hpx_options_t* options) {
    if (!options->package_spec) {
        fprintf(stderr, "Error: Template specification required\n");
        return 1;
    }
    
    const char* project_name = NULL;
    const char* target_dir = ".";
    
    /* Get project name from arguments */
    if (options->arg_count > 0) {
        project_name = options->args[0];
    } else if (options->command) {
        project_name = options->command;
    } else {
        fprintf(stderr, "Error: Project name required for template creation\n");
        return 1;
    }
    
    /* Get target directory if specified */
    if (options->working_dir) {
        target_dir = options->working_dir;
    }
    
    if (options->verbose) {
        printf("Creating project '%s' from template '%s'\n", project_name, options->package_spec);
        printf("Target directory: %s\n", target_dir);
    }
    
    hyp_error_t result = hpx_create_project_from_template(hpx, options->package_spec, project_name, target_dir);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hpx_get_error(hpx));
        return 1;
    }
    
    printf("Project '%s' created successfully\n", project_name);
    return 0;
}

/* Main entry point */
int main(int argc, char* argv[]) {
    hpx_options_t options;
    
    /* Parse command-line arguments */
    if (!parse_arguments(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Handle special commands */
    if (options.show_help) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (options.show_version) {
        print_version();
        return 0;
    }
    
    /* Create HPX context */
    hpx_context_t* hpx = hpx_create();
    if (!hpx) {
        fprintf(stderr, "Error: Could not initialize package executor\n");
        return 1;
    }
    
    /* Configure HPX */
    hpx->config.verbose = options.verbose;
    hpx->config.offline_mode = options.offline;
    hpx->config.auto_install = !options.no_install;
    hpx->config.timeout_seconds = options.timeout;
    
    /* Execute command */
    int result = 0;
    
    if (options.clear_cache) {
        result = execute_clear_cache(hpx, &options);
    } else if (options.list_commands) {
        result = execute_list_commands(hpx, &options);
    } else if (is_template_package(options.package_spec)) {
        result = execute_template(hpx, &options);
    } else {
        result = execute_package(hpx, &options);
    }
    
    /* Cleanup */
    hpx_destroy(hpx);
    return result;
}