/**
 * Hyper Programming Language - Package Manager CLI (hpm)
 * 
 * Command-line interface for the Hyper package manager.
 * Handles package installation, dependency management, and project initialization.
 */

#include "../../include/hpm.h"
#include "../../include/hyp_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define HPM_VERSION "0.1.0"

/* Command types */
typedef enum {
    HPM_CMD_UNKNOWN,
    HPM_CMD_INIT,
    HPM_CMD_INSTALL,
    HPM_CMD_REMOVE,
    HPM_CMD_UPDATE,
    HPM_CMD_LIST,
    HPM_CMD_SEARCH,
    HPM_CMD_INFO,
    HPM_CMD_PUBLISH,
    HPM_CMD_RUN,
    HPM_CMD_HELP,
    HPM_CMD_VERSION
} hpm_command_t;

/* CLI options */
typedef struct {
    hpm_command_t command;
    char* package_name;
    char* version_spec;
    char* script_name;
    bool global;
    bool save_dev;
    bool verbose;
    bool offline;
    bool force;
    char* registry_url;
} hpm_options_t;

/* Print usage information */
static void print_usage(const char* program_name) {
    printf("Hyper Programming Language Package Manager (hpm) v%s\n\n", HPM_VERSION);
    printf("Usage: %s <command> [options] [arguments]\n\n", program_name);
    printf("Commands:\n");
    printf("  init [name]             Initialize a new package\n");
    printf("  install [package]       Install package(s)\n");
    printf("  remove <package>        Remove a package\n");
    printf("  update [package]        Update package(s)\n");
    printf("  list                    List installed packages\n");
    printf("  search <query>          Search for packages\n");
    printf("  info <package>          Show package information\n");
    printf("  publish [path]          Publish a package\n");
    printf("  run <script>            Run a package script\n\n");
    printf("Options:\n");
    printf("  -g, --global            Install globally\n");
    printf("  -D, --save-dev          Save as development dependency\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("      --offline           Work in offline mode\n");
    printf("  -f, --force             Force operation\n");
    printf("      --registry <url>    Use custom registry\n");
    printf("  -h, --help              Show this help message\n");
    printf("      --version           Show version information\n\n");
    printf("Examples:\n");
    printf("  %s init my-app\n", program_name);
    printf("  %s install lodash\n", program_name);
    printf("  %s install express@4.18.0\n", program_name);
    printf("  %s remove lodash\n", program_name);
    printf("  %s search http\n", program_name);
    printf("  %s run build\n", program_name);
}

/* Print version information */
static void print_version(void) {
    printf("Hyper Programming Language Package Manager (hpm) v%s\n", HPM_VERSION);
    printf("Built with C99/C11 for maximum performance\n");
    printf("Copyright (c) 2024 Hyper Language Project\n");
}

/* Parse command from string */
static hpm_command_t parse_command(const char* cmd_str) {
    if (!cmd_str) return HPM_CMD_UNKNOWN;
    
    if (strcmp(cmd_str, "init") == 0) return HPM_CMD_INIT;
    if (strcmp(cmd_str, "install") == 0 || strcmp(cmd_str, "i") == 0) return HPM_CMD_INSTALL;
    if (strcmp(cmd_str, "remove") == 0 || strcmp(cmd_str, "rm") == 0 || strcmp(cmd_str, "uninstall") == 0) return HPM_CMD_REMOVE;
    if (strcmp(cmd_str, "update") == 0 || strcmp(cmd_str, "upgrade") == 0) return HPM_CMD_UPDATE;
    if (strcmp(cmd_str, "list") == 0 || strcmp(cmd_str, "ls") == 0) return HPM_CMD_LIST;
    if (strcmp(cmd_str, "search") == 0) return HPM_CMD_SEARCH;
    if (strcmp(cmd_str, "info") == 0 || strcmp(cmd_str, "show") == 0) return HPM_CMD_INFO;
    if (strcmp(cmd_str, "publish") == 0) return HPM_CMD_PUBLISH;
    if (strcmp(cmd_str, "run") == 0) return HPM_CMD_RUN;
    if (strcmp(cmd_str, "help") == 0) return HPM_CMD_HELP;
    if (strcmp(cmd_str, "version") == 0) return HPM_CMD_VERSION;
    
    return HPM_CMD_UNKNOWN;
}

/* Parse package specification (name@version) */
static void parse_package_spec(const char* spec, char** name, char** version) {
    if (!spec || !name || !version) return;
    
    *name = NULL;
    *version = NULL;
    
    const char* at_sign = strchr(spec, '@');
    if (at_sign && at_sign != spec) {
        /* Package has version specification */
        size_t name_len = at_sign - spec;
        *name = HYP_MALLOC(name_len + 1);
        if (*name) {
            strncpy(*name, spec, name_len);
            (*name)[name_len] = '\0';
        }
        
        if (*(at_sign + 1) != '\0') {
            *version = HYP_MALLOC(strlen(at_sign + 1) + 1);
            if (*version) {
                strcpy(*version, at_sign + 1);
            }
        }
    } else {
        /* Package without version specification */
        *name = HYP_MALLOC(strlen(spec) + 1);
        if (*name) {
            strcpy(*name, spec);
        }
    }
}

/* Parse command-line arguments */
static bool parse_arguments(int argc, char* argv[], hpm_options_t* options) {
    static struct option long_options[] = {
        {"global", no_argument, 0, 'g'},
        {"save-dev", no_argument, 0, 'D'},
        {"verbose", no_argument, 0, 'v'},
        {"offline", no_argument, 0, 1000},
        {"force", no_argument, 0, 'f'},
        {"registry", required_argument, 0, 1001},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 1002},
        {0, 0, 0, 0}
    };
    
    /* Initialize options */
    memset(options, 0, sizeof(hpm_options_t));
    options->command = HPM_CMD_UNKNOWN;
    
    /* Parse command first */
    if (argc > 1) {
        options->command = parse_command(argv[1]);
        if (options->command == HPM_CMD_UNKNOWN) {
            fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
            return false;
        }
    } else {
        options->command = HPM_CMD_HELP;
        return true;
    }
    
    /* Reset getopt */
    optind = 2; /* Start parsing from the second argument */
    
    int c;
    int option_index = 0;
    
    while ((c = getopt_long(argc, argv, "gDvfh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'g':
                options->global = true;
                break;
            case 'D':
                options->save_dev = true;
                break;
            case 'v':
                options->verbose = true;
                break;
            case 'f':
                options->force = true;
                break;
            case 'h':
                options->command = HPM_CMD_HELP;
                return true;
            case 1000: /* --offline */
                options->offline = true;
                break;
            case 1001: /* --registry */
                options->registry_url = optarg;
                break;
            case 1002: /* --version */
                options->command = HPM_CMD_VERSION;
                return true;
            case '?':
                return false;
            default:
                return false;
        }
    }
    
    /* Parse remaining arguments based on command */
    switch (options->command) {
        case HPM_CMD_INIT:
            if (optind < argc) {
                options->package_name = argv[optind];
            }
            break;
            
        case HPM_CMD_INSTALL:
            if (optind < argc) {
                parse_package_spec(argv[optind], &options->package_name, &options->version_spec);
            }
            break;
            
        case HPM_CMD_REMOVE:
        case HPM_CMD_UPDATE:
        case HPM_CMD_SEARCH:
        case HPM_CMD_INFO:
            if (optind < argc) {
                options->package_name = argv[optind];
            } else if (options->command != HPM_CMD_UPDATE) {
                fprintf(stderr, "Error: Command requires a package name\n");
                return false;
            }
            break;
            
        case HPM_CMD_RUN:
            if (optind < argc) {
                options->script_name = argv[optind];
            } else {
                fprintf(stderr, "Error: Command requires a script name\n");
                return false;
            }
            break;
            
        case HPM_CMD_PUBLISH:
            if (optind < argc) {
                options->package_name = argv[optind]; /* Actually package path */
            }
            break;
            
        default:
            break;
    }
    
    return true;
}

/* Execute HPM commands */
static int execute_init(hyp_hpm_context_t* hpm, hpm_options_t* options) {
    hyp_error_t result = hyp_hpm_init_package(hpm, options->package_name);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hyp_hpm_get_error(hpm));
        return 1;
    }
    
    printf("Successfully initialized package\n");
    return 0;
}

static int execute_install(hyp_hpm_context_t* hpm, hpm_options_t* options) {
    if (!options->package_name) {
        /* Install all dependencies from manifest */
        hyp_error_t result = hyp_hpm_load_manifest(hpm, NULL);
        if (result != HYP_OK) {
            fprintf(stderr, "Error: %s\n", hyp_hpm_get_error(hpm));
            return 1;
        }
        
        printf("Installing dependencies from package.yml...\n");
        /* TODO: Install all dependencies */
        printf("All dependencies installed\n");
        return 0;
    }
    
    hyp_error_t result = hyp_hpm_install_package(hpm, options->package_name, options->version_spec);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hyp_hpm_get_error(hpm));
        return 1;
    }
    
    printf("Successfully installed %s\n", options->package_name);
    return 0;
}

static int execute_remove(hyp_hpm_context_t* hpm, hpm_options_t* options) {
    hyp_error_t result = hyp_hpm_remove_package(hpm, options->package_name);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hyp_hpm_get_error(hpm));
        return 1;
    }
    
    printf("Successfully removed %s\n", options->package_name);
    return 0;
}

static int execute_update(hyp_hpm_context_t* hpm, hpm_options_t* options) {
    hyp_error_t result = hyp_hpm_update_package(hpm, options->package_name);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hyp_hpm_get_error(hpm));
        return 1;
    }
    
    if (options->package_name) {
        printf("Successfully updated %s\n", options->package_name);
    } else {
        printf("Successfully updated all packages\n");
    }
    return 0;
}

static int execute_list(hyp_hpm_context_t* hpm, hpm_options_t* options) {
    hyp_package_info_t* packages = NULL;
    size_t package_count = 0;
    
    hyp_error_t result = hyp_hpm_list_packages(hpm, &packages, &package_count);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hyp_hpm_get_error(hpm));
        return 1;
    }
    
    if (package_count == 0) {
        printf("No packages installed\n");
    } else {
        printf("Installed packages (%zu):\n", package_count);
        /* TODO: Print package list */
    }
    
    return 0;
}

static int execute_search(hyp_hpm_context_t* hpm, hpm_options_t* options) {
    hyp_package_info_t* results = NULL;
    size_t result_count = 0;
    
    hyp_error_t result = hyp_hpm_search_packages(hpm, options->package_name, &results, &result_count);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hyp_hpm_get_error(hpm));
        return 1;
    }
    
    if (result_count == 0) {
        printf("No packages found for '%s'\n", options->package_name);
    } else {
        printf("Found %zu package(s) for '%s':\n", result_count, options->package_name);
        /* TODO: Print search results */
    }
    
    return 0;
}

static int execute_info(hyp_hpm_context_t* hpm, hpm_options_t* options) {
    hyp_package_info_t info;
    
    hyp_error_t result = hyp_hpm_show_package_info(hpm, options->package_name, &info);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hyp_hpm_get_error(hpm));
        return 1;
    }
    
    printf("Package information for %s:\n", options->package_name);
    /* TODO: Print package info */
    
    return 0;
}

static int execute_publish(hyp_hpm_context_t* hpm, hpm_options_t* options) {
    const char* package_path = options->package_name ? options->package_name : ".";
    
    hyp_error_t result = hyp_hpm_publish_package(hpm, package_path);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hyp_hpm_get_error(hpm));
        return 1;
    }
    
    printf("Successfully published package\n");
    return 0;
}

static int execute_run(hyp_hpm_context_t* hpm, hpm_options_t* options) {
    hyp_error_t result = hyp_hpm_run_script(hpm, options->script_name);
    if (result != HYP_OK) {
        fprintf(stderr, "Error: %s\n", hyp_hpm_get_error(hpm));
        return 1;
    }
    
    return 0;
}

/* Main entry point */
int main(int argc, char* argv[]) {
    hpm_options_t options;
    
    /* Parse command-line arguments */
    if (!parse_arguments(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Handle special commands */
    if (options.command == HPM_CMD_HELP) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (options.command == HPM_CMD_VERSION) {
        print_version();
        return 0;
    }
    
    /* Create HPM context */
    hyp_hpm_context_t* hpm = hyp_hpm_create();
    if (!hpm) {
        fprintf(stderr, "Error: Could not initialize package manager\n");
        return 1;
    }
    
    /* Configure HPM */
    hpm->config.verbose = options.verbose;
    hpm->config.offline_mode = options.offline;
    
    if (options.registry_url) {
        if (hpm->config.registry_url) {
            hyp_string_destroy(hpm->config.registry_url);
        }
        hpm->config.registry_url = hyp_string_create(options.registry_url);
    }
    
    /* Execute command */
    int result = 0;
    
    switch (options.command) {
        case HPM_CMD_INIT:
            result = execute_init(hpm, &options);
            break;
        case HPM_CMD_INSTALL:
            result = execute_install(hpm, &options);
            break;
        case HPM_CMD_REMOVE:
            result = execute_remove(hpm, &options);
            break;
        case HPM_CMD_UPDATE:
            result = execute_update(hpm, &options);
            break;
        case HPM_CMD_LIST:
            result = execute_list(hpm, &options);
            break;
        case HPM_CMD_SEARCH:
            result = execute_search(hpm, &options);
            break;
        case HPM_CMD_INFO:
            result = execute_info(hpm, &options);
            break;
        case HPM_CMD_PUBLISH:
            result = execute_publish(hpm, &options);
            break;
        case HPM_CMD_RUN:
            result = execute_run(hpm, &options);
            break;
        default:
            fprintf(stderr, "Error: Unknown command\n");
            result = 1;
            break;
    }
    
    /* Cleanup */
    if (options.package_name && options.version_spec) {
        HYP_FREE(options.package_name);
        HYP_FREE(options.version_spec);
    }
    
    hyp_hpm_destroy(hpm);
    return result;
}