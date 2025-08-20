/**
 * Hyper Programming Language - Package Executor (hpx)
 * 
 * Core implementation for the Hyper package executor.
 * Handles one-shot command execution of CLI tools and templates.
 */

#include "../../include/hpx.h"
#include "../../include/hyp_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

/* Create a new HPX context */
hpx_context_t* hpx_create(void) {
    hpx_context_t* hpx = HYP_MALLOC(sizeof(hpx_context_t));
    if (!hpx) return NULL;
    
    memset(hpx, 0, sizeof(hpx_context_t));
    
    /* Initialize configuration with defaults */
    hpx->config.mode = HPX_MODE_CACHE;
    hpx->config.temp_dir = HYP_MALLOC(256);
    hpx->config.cache_dir = HYP_MALLOC(256);
    hpx->config.shell = HYP_MALLOC(256);
    strcpy(hpx->config.temp_dir, "temp");
    strcpy(hpx->config.cache_dir, ".hpx_cache");
    strcpy(hpx->config.shell, "cmd");
    hpx->config.always_spawn = false;
    hpx->config.quiet = false;
    hpx->config.yes = false;
    hpx->config.timeout_seconds = 300;
    
    /* Initialize history */
    hpx->history.capacity = 32;
    hpx->history.count = 0;
    hpx->history.packages = HYP_MALLOC(sizeof(char*) * hpx->history.capacity);
    hpx->history.commands = HYP_MALLOC(sizeof(char*) * hpx->history.capacity);
    hpx->history.exit_codes = HYP_MALLOC(sizeof(int) * hpx->history.capacity);
    
    hpx->has_error = false;
    hpx->error_message[0] = '\0';
    
    return hpx;
}

/* Destroy HPX context */
void hpx_destroy(hpx_context_t* hpx) {
    if (!hpx) return;
    
    /* Cleanup configuration */
    if (hpx->config.cache_dir) HYP_FREE(hpx->config.cache_dir);
    if (hpx->config.temp_dir) HYP_FREE(hpx->config.temp_dir);
    if (hpx->config.shell) HYP_FREE(hpx->config.shell);
    
    /* Cleanup history */
    if (hpx->history.packages) {
        for (size_t i = 0; i < hpx->history.count; i++) {
            if (hpx->history.packages[i]) HYP_FREE(hpx->history.packages[i]);
            if (hpx->history.commands[i]) HYP_FREE(hpx->history.commands[i]);
        }
        HYP_FREE(hpx->history.packages);
        HYP_FREE(hpx->history.commands);
        HYP_FREE(hpx->history.exit_codes);
    }
    
    HYP_FREE(hpx);
}

/* Set error message */
void hpx_error(hpx_context_t* hpx, const char* format, ...) {
    if (!hpx || !format) return;
    
    hpx->has_error = true;
    strncpy(hpx->error_message, format, sizeof(hpx->error_message) - 1);
    hpx->error_message[sizeof(hpx->error_message) - 1] = '\0';
}

/* Get error message */
const char* hpx_get_error(hpx_context_t* hpx) {
    return hpx && hpx->has_error ? hpx->error_message : NULL;
}

/* Clear error */
void hpx_clear_error(hpx_context_t* hpx) {
    if (!hpx) return;
    hpx->has_error = false;
    hpx->error_message[0] = '\0';
}

/* Parse package specification */
hpx_package_spec_t hpx_parse_package_spec(const char* spec) {
    hpx_package_spec_t result = {0};
    if (!spec) return result;
    
    /* Simple parsing - just copy the spec as name for now */
    result.name = HYP_MALLOC(strlen(spec) + 1);
    if (result.name) {
        strcpy(result.name, spec);
    }
    
    return result;
}

/* Free package specification */
void hpx_package_spec_free(hpx_package_spec_t* spec) {
    if (!spec) return;
    
    if (spec->name) HYP_FREE(spec->name);
    if (spec->version) HYP_FREE(spec->version);
    if (spec->command) HYP_FREE(spec->command);
    memset(spec, 0, sizeof(hpx_package_spec_t));
}

/* Check if package is executable */
bool hpx_is_executable(hpx_context_t* hpx, const char* package_name, hpx_package_info_t* info) {
    if (!hpx || !package_name) return false;
    
    /* TODO: Implement actual executable check */
    return true;
}

/* Get package information */
hyp_error_t hpx_get_package_info(hpx_context_t* hpx, const char* package_name, const char* version, hpx_package_info_t* info) {
    if (!hpx || !package_name || !info) return HYP_ERROR_INVALID_ARG;
    
    memset(info, 0, sizeof(hpx_package_info_t));
    
    /* TODO: Implement actual package info retrieval */
    info->name = HYP_MALLOC(strlen(package_name) + 1);
    if (info->name) strcpy(info->name, package_name);
    
    if (version) {
        info->version = HYP_MALLOC(strlen(version) + 1);
        if (info->version) strcpy(info->version, version);
    }
    
    info->description = HYP_MALLOC(32);
    if (info->description) strcpy(info->description, "Package description");
    
    info->is_binary = false;
    info->command_count = 0;
    
    return HYP_ERROR_NONE;
}

/* Initialize HPX context */
hyp_error_t hpx_init(hpx_context_t* hpx, const hpx_config_t* config) {
    if (!hpx) return HYP_ERROR_INVALID_ARG;
    
    /* Apply configuration if provided */
    if (config) {
        hpx->config.mode = config->mode;
        if (config->temp_dir) {
            HYP_FREE(hpx->config.temp_dir);
            hpx->config.temp_dir = HYP_MALLOC(strlen(config->temp_dir) + 1);
            if (hpx->config.temp_dir) {
                strcpy(hpx->config.temp_dir, config->temp_dir);
            }
        }
        if (config->cache_dir) {
            HYP_FREE(hpx->config.cache_dir);
            hpx->config.cache_dir = HYP_MALLOC(strlen(config->cache_dir) + 1);
            if (hpx->config.cache_dir) {
                strcpy(hpx->config.cache_dir, config->cache_dir);
            }
        }
        if (config->shell) {
            HYP_FREE(hpx->config.shell);
            hpx->config.shell = HYP_MALLOC(strlen(config->shell) + 1);
            if (hpx->config.shell) {
                strcpy(hpx->config.shell, config->shell);
            }
        }
    }
    
    return HYP_ERROR_NONE;
}

/* Resolve package path */
hyp_error_t hpx_resolve_package(hpx_context_t* hpx, const char* package_spec, char** resolved_path) {
    if (!hpx || !package_spec || !resolved_path) {
        if (hpx) hpx_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARG;
    }
    
    /* TODO: Implement package path resolution */
    *resolved_path = HYP_MALLOC(256);
    if (*resolved_path) {
        strcpy(*resolved_path, "/path/to/package");
    }
    
    return HYP_ERROR_NONE;
}

/* Download package to temp */
hyp_error_t hpx_download_temp(hpx_context_t* hpx, const char* package_name, const char* version, char** temp_path) {
    if (!hpx || !package_name || !temp_path) {
        if (hpx) hpx_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARG;
    }
    
    /* TODO: Implement package download */
    *temp_path = HYP_MALLOC(256);
    if (*temp_path) {
        strcpy(*temp_path, "/path/to/downloaded/package");
    }
    
    return HYP_ERROR_NONE;
}

/* Execute local executable */
hyp_error_t hpx_execute_local(hpx_context_t* hpx, const char* executable_path, char** args, size_t arg_count, const hpx_exec_options_t* options, hpx_exec_result_t* result) {
    if (!hpx || !executable_path || !result) return HYP_ERROR_INVALID_ARG;
    
    /* TODO: Implement local execution */
    memset(result, 0, sizeof(hpx_exec_result_t));
    result->exit_code = 0;
    result->execution_time = 0.1;
    result->stdout_output = HYP_MALLOC(64);
    if (result->stdout_output) {
        strcpy(result->stdout_output, "Execution completed successfully");
    }
    
    return HYP_ERROR_NONE;
}

/* Execute package */
hyp_error_t hpx_execute(hpx_context_t* hpx, const char* package_name, char** args, size_t arg_count, const hpx_exec_options_t* options, hpx_exec_result_t* result) {
    if (!hpx || !package_name || !result) return HYP_ERROR_INVALID_ARG;
    
    /* TODO: Implement package execution */
    memset(result, 0, sizeof(hpx_exec_result_t));
    result->exit_code = 0;
    result->execution_time = 0.1;
    result->stdout_output = HYP_MALLOC(64);
    if (result->stdout_output) {
        strcpy(result->stdout_output, "Package executed successfully");
    }
    
    return HYP_ERROR_NONE;
}

/* Execute package */
hyp_error_t hpx_execute_package(hpx_context_t* hpx, const char* package_spec, const hpx_exec_options_t* options, hpx_exec_result_t* result) {
    if (!hpx || !package_spec || !result) {
        if (hpx) hpx_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(hpx_exec_result_t));
    
    /* Parse package specification */
    hpx_package_spec_t parsed;
    hyp_error_t parse_result = parse_package_spec(package_spec, &parsed);
    if (parse_result != HYP_OK) {
        hpx_error(hpx, "Failed to parse package specification");
        return parse_result;
    }
    
    /* Check if package is executable */
    bool is_executable;
    hyp_error_t check_result = hpx_is_executable(hpx, package_spec, &is_executable);
    if (check_result != HYP_OK) {
        free_package_spec(&parsed);
        return check_result;
    }
    
    if (!is_executable) {
        hpx_error(hpx, "Package is not executable");
        free_package_spec(&parsed);
        return HYP_ERROR_NOT_EXECUTABLE;
    }
    
    /* Download package if needed */
    hyp_string_t* package_path;
    hyp_error_t download_result = hpx_download_package(hpx, package_spec, &package_path);
    if (download_result != HYP_OK) {
        free_package_spec(&parsed);
        return download_result;
    }
    
    /* Execute package */
    /* TODO: Implement hpx_execute_local_script function */
    result->exit_code = 0;
    result->stdout_output = NULL;
    result->stderr_output = NULL;
    result->execution_time = 0.0;
    result->timed_out = false;
    
    /* Add to execution history */
    if (hpx->history.count < hpx->history.capacity) {
        /* TODO: Implement proper history storage */
        hpx->history.count++;
    }
    
    /* Cleanup */
    hyp_string_destroy(package_path);
    free_package_spec(&parsed);
    
    return exec_result;
}

/* List available commands */
hyp_error_t hpx_list_commands(hpx_context_t* hpx, const char* package_spec, hyp_string_t*** commands, size_t* command_count) {
    if (!hpx || !package_spec || !commands || !command_count) {
        if (hpx) hpx_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARG;
    }
    
    /* TODO: Implement command listing */
    *command_count = 2;
    *commands = HYP_MALLOC(sizeof(hyp_string_t*) * 2);
    if (!*commands) {
        hpx_error(hpx, "Out of memory");
        return HYP_ERROR_MEMORY;
    }
    
    (*commands)[0] = HYP_MALLOC(sizeof(hyp_string_t));
    (*commands)[1] = HYP_MALLOC(sizeof(hyp_string_t));
    if (!(*commands)[0] || !(*commands)[1]) {
        if ((*commands)[0]) HYP_FREE((*commands)[0]);
        if ((*commands)[1]) HYP_FREE((*commands)[1]);
        HYP_FREE(*commands);
        return HYP_ERROR_MEMORY;
    }
    *(*commands)[0] = hyp_string_create("build");
    *(*commands)[1] = hyp_string_create("start");
    
    return HYP_OK;
}

/* Show help */
hyp_error_t hpx_show_help(hpx_context_t* hpx, const char* package_spec) {
    if (!hpx) {
        return HYP_ERROR_INVALID_ARG;
    }
    
    if (package_spec) {
        printf("Help for package: %s\n", package_spec);
        /* TODO: Show package-specific help */
    } else {
        printf("HPX - Hyper Package Executor\n");
        printf("Usage: hpx <package[@version]> [command] [args...]\n");
        printf("\nExamples:\n");
        printf("  hpx create-hyp-app my-app\n");
        printf("  hpx @hyper/cli@latest init\n");
        printf("  hpx typescript tsc --version\n");
    }
    
    return HYP_OK;
}

/* Create project from template */
hyp_error_t hpx_create_project_from_template(hpx_context_t* hpx, const char* template_spec, const char* project_name, const char* target_dir) {
    if (!hpx || !template_spec || !project_name) {
        if (hpx) hpx_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARG;
    }
    
    /* TODO: Implement template-based project creation */
    printf("Creating project '%s' from template '%s'\n", project_name, template_spec);
    
    return HYP_OK;
}

/* Clear cache */
hyp_error_t hpx_clear_cache(hpx_context_t* hpx) {
    if (!hpx) return HYP_ERROR_INVALID_ARG;
    
    /* TODO: Implement cache clearing */
    printf("Cache cleared\n");
    
    return HYP_OK;
}

/* Get execution history */
hyp_error_t hpx_get_execution_history(hpx_context_t* hpx, hpx_exec_result_t** history, size_t* history_count) {
    if (!hpx || !history || !history_count) {
        if (hpx) hpx_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARG;
    }
    
    *history = NULL; /* TODO: Implement history storage */
    *history_count = 0;
    
    return HYP_OK;
}

/* Add search path */
hyp_error_t hpx_add_search_path(hpx_context_t* hpx, const char* path) {
    if (!hpx || !path) {
        if (hpx) hpx_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARG;
    }
    
    /* TODO: Implement search path management */
    hpx_error(hpx, "Search path management not implemented");
    return HYP_ERROR_RUNTIME;
}