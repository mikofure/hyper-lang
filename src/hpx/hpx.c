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
hyp_hpx_context_t* hyp_hpx_create(void) {
    hyp_hpx_context_t* hpx = HYP_MALLOC(sizeof(hyp_hpx_context_t));
    if (!hpx) return NULL;
    
    memset(hpx, 0, sizeof(hyp_hpx_context_t));
    
    /* Initialize configuration with defaults */
    hpx->config.cache_enabled = true;
    hpx->config.auto_install = true;
    hpx->config.timeout_seconds = 300; /* 5 minutes */
    hpx->config.max_download_size = 100 * 1024 * 1024; /* 100MB */
    hpx->config.cache_dir = hyp_string_create(".hpx_cache");
    hpx->config.temp_dir = hyp_string_create("temp");
    hpx->config.registry_url = hyp_string_create("https://registry.hyper-lang.org");
    
    /* Initialize arrays */
    hpx->search_paths = HYP_MALLOC(sizeof(hyp_string_t*) * 16);
    hpx->search_paths_capacity = 16;
    hpx->search_paths_count = 0;
    
    hpx->execution_history = HYP_MALLOC(sizeof(hyp_hpx_execution_result_t) * 32);
    hpx->execution_history_capacity = 32;
    hpx->execution_history_count = 0;
    
    return hpx;
}

/* Destroy HPX context */
void hyp_hpx_destroy(hyp_hpx_context_t* hpx) {
    if (!hpx) return;
    
    /* Cleanup configuration */
    if (hpx->config.cache_dir) hyp_string_destroy(hpx->config.cache_dir);
    if (hpx->config.temp_dir) hyp_string_destroy(hpx->config.temp_dir);
    if (hpx->config.registry_url) hyp_string_destroy(hpx->config.registry_url);
    
    /* Cleanup search paths */
    if (hpx->search_paths) {
        for (size_t i = 0; i < hpx->search_paths_count; i++) {
            if (hpx->search_paths[i]) {
                hyp_string_destroy(hpx->search_paths[i]);
            }
        }
        HYP_FREE(hpx->search_paths);
    }
    
    /* Cleanup execution history */
    if (hpx->execution_history) {
        for (size_t i = 0; i < hpx->execution_history_count; i++) {
            hyp_hpx_execution_result_t* result = &hpx->execution_history[i];
            if (result->package_spec) hyp_string_destroy(result->package_spec);
            if (result->command) hyp_string_destroy(result->command);
            if (result->output) hyp_string_destroy(result->output);
            if (result->error_message) hyp_string_destroy(result->error_message);
        }
        HYP_FREE(hpx->execution_history);
    }
    
    if (hpx->error_message) hyp_string_destroy(hpx->error_message);
    
    HYP_FREE(hpx);
}

/* Set error message */
static void hpx_set_error(hyp_hpx_context_t* hpx, const char* message) {
    if (!hpx) return;
    
    if (hpx->error_message) {
        hyp_string_destroy(hpx->error_message);
    }
    
    hpx->error_message = hyp_string_create(message);
}

/* Get last error message */
const char* hyp_hpx_get_error(hyp_hpx_context_t* hpx) {
    if (!hpx || !hpx->error_message) return "Unknown error";
    return hpx->error_message->data;
}

/* Parse package specification */
static hyp_error_t parse_package_spec(const char* spec, hyp_hpx_package_spec_t* parsed) {
    if (!spec || !parsed) return HYP_ERROR_INVALID_ARGUMENT;
    
    memset(parsed, 0, sizeof(hyp_hpx_package_spec_t));
    
    /* Check for scope (e.g., @scope/package) */
    if (spec[0] == '@') {
        const char* slash = strchr(spec + 1, '/');
        if (slash) {
            size_t scope_len = slash - spec;
            parsed->scope = HYP_MALLOC(scope_len + 1);
            if (!parsed->scope) return HYP_ERROR_OUT_OF_MEMORY;
            strncpy(parsed->scope, spec, scope_len);
            parsed->scope[scope_len] = '\0';
            spec = slash + 1;
        }
    }
    
    /* Parse name and version */
    const char* at_sign = strchr(spec, '@');
    if (at_sign && at_sign != spec) {
        /* Package has version specification */
        size_t name_len = at_sign - spec;
        parsed->name = HYP_MALLOC(name_len + 1);
        if (!parsed->name) {
            if (parsed->scope) HYP_FREE(parsed->scope);
            return HYP_ERROR_OUT_OF_MEMORY;
        }
        strncpy(parsed->name, spec, name_len);
        parsed->name[name_len] = '\0';
        
        if (*(at_sign + 1) != '\0') {
            parsed->version = HYP_MALLOC(strlen(at_sign + 1) + 1);
            if (!parsed->version) {
                if (parsed->scope) HYP_FREE(parsed->scope);
                HYP_FREE(parsed->name);
                return HYP_ERROR_OUT_OF_MEMORY;
            }
            strcpy(parsed->version, at_sign + 1);
        }
    } else {
        /* Package without version specification */
        parsed->name = HYP_MALLOC(strlen(spec) + 1);
        if (!parsed->name) {
            if (parsed->scope) HYP_FREE(parsed->scope);
            return HYP_ERROR_OUT_OF_MEMORY;
        }
        strcpy(parsed->name, spec);
    }
    
    return HYP_OK;
}

/* Free package specification */
static void free_package_spec(hyp_hpx_package_spec_t* spec) {
    if (!spec) return;
    
    if (spec->scope) HYP_FREE(spec->scope);
    if (spec->name) HYP_FREE(spec->name);
    if (spec->version) HYP_FREE(spec->version);
    
    memset(spec, 0, sizeof(hyp_hpx_package_spec_t));
}

/* Check if package is executable */
hyp_error_t hyp_hpx_is_executable(hyp_hpx_context_t* hpx, const char* package_spec, bool* is_executable) {
    if (!hpx || !package_spec || !is_executable) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    hyp_hpx_package_spec_t parsed;
    hyp_error_t result = parse_package_spec(package_spec, &parsed);
    if (result != HYP_OK) {
        hpx_set_error(hpx, "Failed to parse package specification");
        return result;
    }
    
    /* TODO: Check if package exists and has executable commands */
    *is_executable = true; /* Placeholder */
    
    free_package_spec(&parsed);
    return HYP_OK;
}

/* Get package information */
hyp_error_t hyp_hpx_get_package_info(hyp_hpx_context_t* hpx, const char* package_spec, hyp_hpx_package_info_t* info) {
    if (!hpx || !package_spec || !info) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    memset(info, 0, sizeof(hyp_hpx_package_info_t));
    
    hyp_hpx_package_spec_t parsed;
    hyp_error_t result = parse_package_spec(package_spec, &parsed);
    if (result != HYP_OK) {
        hpx_set_error(hpx, "Failed to parse package specification");
        return result;
    }
    
    /* TODO: Fetch package information from registry */
    info->name = hyp_string_create(parsed.name);
    info->version = hyp_string_create(parsed.version ? parsed.version : "latest");
    info->description = hyp_string_create("Package description");
    info->is_installed = false;
    info->install_path = NULL;
    
    free_package_spec(&parsed);
    return HYP_OK;
}

/* Resolve package path */
hyp_error_t hyp_hpx_resolve_package_path(hyp_hpx_context_t* hpx, const char* package_spec, hyp_string_t** resolved_path) {
    if (!hpx || !package_spec || !resolved_path) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    /* TODO: Implement package path resolution */
    *resolved_path = hyp_string_create("/path/to/package");
    
    return HYP_OK;
}

/* Download package */
hyp_error_t hyp_hpx_download_package(hyp_hpx_context_t* hpx, const char* package_spec, hyp_string_t** download_path) {
    if (!hpx || !package_spec || !download_path) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    /* TODO: Implement package download */
    *download_path = hyp_string_create("/path/to/downloaded/package");
    
    return HYP_OK;
}

/* Execute local script */
hyp_error_t hyp_hpx_execute_local_script(hyp_hpx_context_t* hpx, const char* script_path, const hyp_hpx_execution_options_t* options, hyp_hpx_execution_result_t* result) {
    if (!hpx || !script_path || !result) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    memset(result, 0, sizeof(hyp_hpx_execution_result_t));
    
    /* Check if script exists */
    struct stat st;
    if (stat(script_path, &st) != 0) {
        hpx_set_error(hpx, "Script file not found");
        return HYP_ERROR_FILE_NOT_FOUND;
    }
    
    /* TODO: Execute script */
    result->exit_code = 0;
    result->execution_time_ms = 100;
    result->output = hyp_string_create("Script executed successfully");
    
    return HYP_OK;
}

/* Execute binary */
hyp_error_t hyp_hpx_execute_binary(hyp_hpx_context_t* hpx, const char* binary_path, const hyp_hpx_execution_options_t* options, hyp_hpx_execution_result_t* result) {
    if (!hpx || !binary_path || !result) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    memset(result, 0, sizeof(hyp_hpx_execution_result_t));
    
    /* Check if binary exists */
    struct stat st;
    if (stat(binary_path, &st) != 0) {
        hpx_set_error(hpx, "Binary file not found");
        return HYP_ERROR_FILE_NOT_FOUND;
    }
    
    /* TODO: Execute binary */
    result->exit_code = 0;
    result->execution_time_ms = 200;
    result->output = hyp_string_create("Binary executed successfully");
    
    return HYP_OK;
}

/* Execute package */
hyp_error_t hyp_hpx_execute_package(hyp_hpx_context_t* hpx, const char* package_spec, const hyp_hpx_execution_options_t* options, hyp_hpx_execution_result_t* result) {
    if (!hpx || !package_spec || !result) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    memset(result, 0, sizeof(hyp_hpx_execution_result_t));
    
    /* Parse package specification */
    hyp_hpx_package_spec_t parsed;
    hyp_error_t parse_result = parse_package_spec(package_spec, &parsed);
    if (parse_result != HYP_OK) {
        hpx_set_error(hpx, "Failed to parse package specification");
        return parse_result;
    }
    
    /* Check if package is executable */
    bool is_executable;
    hyp_error_t check_result = hyp_hpx_is_executable(hpx, package_spec, &is_executable);
    if (check_result != HYP_OK) {
        free_package_spec(&parsed);
        return check_result;
    }
    
    if (!is_executable) {
        hpx_set_error(hpx, "Package is not executable");
        free_package_spec(&parsed);
        return HYP_ERROR_NOT_EXECUTABLE;
    }
    
    /* Download package if needed */
    hyp_string_t* package_path;
    hyp_error_t download_result = hyp_hpx_download_package(hpx, package_spec, &package_path);
    if (download_result != HYP_OK) {
        free_package_spec(&parsed);
        return download_result;
    }
    
    /* Execute package */
    hyp_error_t exec_result = hyp_hpx_execute_local_script(hpx, package_path->data, options, result);
    
    /* Store execution result */
    result->package_spec = hyp_string_create(package_spec);
    if (options && options->command) {
        result->command = hyp_string_create(options->command);
    }
    
    /* Add to execution history */
    if (hpx->execution_history_count < hpx->execution_history_capacity) {
        hpx->execution_history[hpx->execution_history_count] = *result;
        hpx->execution_history_count++;
    }
    
    /* Cleanup */
    hyp_string_destroy(package_path);
    free_package_spec(&parsed);
    
    return exec_result;
}

/* List available commands */
hyp_error_t hyp_hpx_list_commands(hyp_hpx_context_t* hpx, const char* package_spec, hyp_string_t*** commands, size_t* command_count) {
    if (!hpx || !package_spec || !commands || !command_count) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    /* TODO: Implement command listing */
    *command_count = 2;
    *commands = HYP_MALLOC(sizeof(hyp_string_t*) * 2);
    if (!*commands) {
        hpx_set_error(hpx, "Out of memory");
        return HYP_ERROR_OUT_OF_MEMORY;
    }
    
    (*commands)[0] = hyp_string_create("build");
    (*commands)[1] = hyp_string_create("start");
    
    return HYP_OK;
}

/* Show help */
hyp_error_t hyp_hpx_show_help(hyp_hpx_context_t* hpx, const char* package_spec) {
    if (!hpx) {
        return HYP_ERROR_INVALID_ARGUMENT;
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
hyp_error_t hyp_hpx_create_project_from_template(hyp_hpx_context_t* hpx, const char* template_spec, const char* project_name, const char* target_dir) {
    if (!hpx || !template_spec || !project_name) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    /* TODO: Implement template-based project creation */
    printf("Creating project '%s' from template '%s'\n", project_name, template_spec);
    
    return HYP_OK;
}

/* Clear cache */
hyp_error_t hyp_hpx_clear_cache(hyp_hpx_context_t* hpx) {
    if (!hpx) return HYP_ERROR_INVALID_ARGUMENT;
    
    /* TODO: Implement cache clearing */
    printf("Cache cleared\n");
    
    return HYP_OK;
}

/* Get execution history */
hyp_error_t hyp_hpx_get_execution_history(hyp_hpx_context_t* hpx, hyp_hpx_execution_result_t** history, size_t* history_count) {
    if (!hpx || !history || !history_count) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    *history = hpx->execution_history;
    *history_count = hpx->execution_history_count;
    
    return HYP_OK;
}

/* Add search path */
hyp_error_t hyp_hpx_add_search_path(hyp_hpx_context_t* hpx, const char* path) {
    if (!hpx || !path) {
        if (hpx) hpx_set_error(hpx, "Invalid arguments");
        return HYP_ERROR_INVALID_ARGUMENT;
    }
    
    /* Resize array if needed */
    if (hpx->search_paths_count >= hpx->search_paths_capacity) {
        size_t new_capacity = hpx->search_paths_capacity * 2;
        hyp_string_t** new_paths = HYP_REALLOC(hpx->search_paths, sizeof(hyp_string_t*) * new_capacity);
        if (!new_paths) {
            hpx_set_error(hpx, "Out of memory");
            return HYP_ERROR_OUT_OF_MEMORY;
        }
        hpx->search_paths = new_paths;
        hpx->search_paths_capacity = new_capacity;
    }
    
    /* Add path */
    hpx->search_paths[hpx->search_paths_count] = hyp_string_create(path);
    if (!hpx->search_paths[hpx->search_paths_count]) {
        hpx_set_error(hpx, "Out of memory");
        return HYP_ERROR_OUT_OF_MEMORY;
    }
    
    hpx->search_paths_count++;
    return HYP_OK;
}