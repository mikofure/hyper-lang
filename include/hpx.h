/**
 * Hyper Programming Language - Package Executor (hpx)
 * 
 * The package executor runs CLI tools and templates as one-shot commands,
 * similar to npx. It can execute packages without installing them permanently
 * and provides a convenient way to run project generators and utilities.
 */

#ifndef HYP_HPX_H
#define HYP_HPX_H

#include "hyp_common.h"
#include "hpm.h"

/* Execution modes */
typedef enum {
    HPX_MODE_TEMP,       /* Download to temp directory and execute */
    HPX_MODE_CACHE,      /* Use cached version if available */
    HPX_MODE_LOCAL,      /* Execute from local .hypkg directory */
    HPX_MODE_GLOBAL      /* Execute from global installation */
} hpx_execution_mode_t;

/* Package executor configuration */
typedef struct {
    hpx_execution_mode_t mode;
    char* temp_dir;
    char* cache_dir;
    bool always_spawn;
    bool quiet;
    bool yes;            /* Auto-confirm prompts */
    int timeout_seconds;
    char* shell;         /* Shell to use for execution */
} hpx_config_t;

/* Executable package information */
typedef struct {
    char* name;
    char* version;
    char* executable;    /* Path to executable or entry point */
    char* description;
    char** commands;     /* Available commands */
    size_t command_count;
    bool is_binary;      /* True if compiled binary, false if script */
} hpx_package_info_t;

/* Package executor context */
typedef struct {
    hpx_config_t config;
    hpm_context_t* hpm;  /* Package manager for downloads */
    hyp_arena_t* arena;
    
    /* Execution history */
    struct {
        char** packages;
        char** commands;
        int* exit_codes;
        size_t count;
        size_t capacity;
    } history;
    
    /* Error handling */
    bool has_error;
    char error_message[512];
} hpx_context_t;

/* Execution options */
typedef struct {
    char* version;       /* Specific version to execute */
    char* working_dir;   /* Working directory for execution */
    char** env_vars;     /* Environment variables */
    size_t env_count;
    bool inherit_env;    /* Inherit current environment */
    bool capture_output; /* Capture stdout/stderr */
    int timeout;         /* Execution timeout in seconds */
} hpx_exec_options_t;

/* Execution result */
typedef struct {
    int exit_code;
    char* stdout_output;
    char* stderr_output;
    double execution_time;
    bool timed_out;
} hpx_exec_result_t;

/* Function declarations */

/**
 * Initialize package executor context
 * @param hpx The package executor context to initialize
 * @param config Executor configuration
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpx_init(hpx_context_t* hpx, const hpx_config_t* config);

/**
 * Execute a package command
 * @param hpx The package executor context
 * @param package_name Name of the package to execute
 * @param args Command line arguments
 * @param arg_count Number of arguments
 * @param options Execution options
 * @param result Execution result (output parameter)
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpx_execute(hpx_context_t* hpx, const char* package_name, char** args, size_t arg_count, const hpx_exec_options_t* options, hpx_exec_result_t* result);

/**
 * Execute a package with a specific command
 * @param hpx The package executor context
 * @param package_spec Package specification (name@version)
 * @param command Command to run within the package
 * @param args Additional arguments
 * @param arg_count Number of additional arguments
 * @param options Execution options
 * @param result Execution result (output parameter)
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpx_execute_command(hpx_context_t* hpx, const char* package_spec, const char* command, char** args, size_t arg_count, const hpx_exec_options_t* options, hpx_exec_result_t* result);

/**
 * Check if a package is executable
 * @param hpx The package executor context
 * @param package_name Name of the package
 * @param info Package information (output parameter)
 * @return true if executable, false otherwise
 */
bool hpx_is_executable(hpx_context_t* hpx, const char* package_name, hpx_package_info_t* info);

/**
 * Get package information for execution
 * @param hpx The package executor context
 * @param package_name Name of the package
 * @param version Specific version (NULL for latest)
 * @param info Package information (output parameter)
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpx_get_package_info(hpx_context_t* hpx, const char* package_name, const char* version, hpx_package_info_t* info);

/**
 * Resolve package location for execution
 * @param hpx The package executor context
 * @param package_spec Package specification (name@version)
 * @param resolved_path Resolved path (output parameter)
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpx_resolve_package(hpx_context_t* hpx, const char* package_spec, char** resolved_path);

/**
 * Download package to temporary location
 * @param hpx The package executor context
 * @param package_name Name of the package
 * @param version Version to download
 * @param temp_path Temporary path (output parameter)
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpx_download_temp(hpx_context_t* hpx, const char* package_name, const char* version, char** temp_path);

/**
 * Execute a local script or binary
 * @param hpx The package executor context
 * @param executable_path Path to the executable
 * @param args Command line arguments
 * @param arg_count Number of arguments
 * @param options Execution options
 * @param result Execution result (output parameter)
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpx_execute_local(hpx_context_t* hpx, const char* executable_path, char** args, size_t arg_count, const hpx_exec_options_t* options, hpx_exec_result_t* result);

/**
 * List available commands in a package
 * @param hpx The package executor context
 * @param package_name Name of the package
 * @param commands Array of command names (output parameter)
 * @param command_count Number of commands (output parameter)
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpx_list_commands(hpx_context_t* hpx, const char* package_name, char*** commands, size_t* command_count);

/**
 * Show help for a package or command
 * @param hpx The package executor context
 * @param package_name Name of the package
 * @param command Specific command (NULL for package help)
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpx_show_help(hpx_context_t* hpx, const char* package_name, const char* command);

/**
 * Create a new project from a template
 * @param hpx The package executor context
 * @param template_name Name of the template package
 * @param project_name Name of the new project
 * @param target_dir Directory to create the project in
 * @param options Template options
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpx_create_from_template(hpx_context_t* hpx, const char* template_name, const char* project_name, const char* target_dir, char** options);

/* Package specification parsing */
typedef struct {
    char* name;
    char* version;
    char* command;
} hpx_package_spec_t;

hpx_package_spec_t hpx_parse_package_spec(const char* spec);
void hpx_package_spec_free(hpx_package_spec_t* spec);

/* Execution options utilities */
hpx_exec_options_t hpx_default_exec_options(void);
void hpx_exec_options_add_env(hpx_exec_options_t* options, const char* name, const char* value);
void hpx_exec_options_free(hpx_exec_options_t* options);

/* Execution result utilities */
void hpx_exec_result_free(hpx_exec_result_t* result);
void hpx_exec_result_print(const hpx_exec_result_t* result);

/* Package info utilities */
void hpx_package_info_free(hpx_package_info_t* info);
void hpx_package_info_print(const hpx_package_info_t* info);

/* Cache management */
hyp_error_t hpx_clear_cache(hpx_context_t* hpx);
hyp_error_t hpx_clean_temp(hpx_context_t* hpx);
size_t hpx_get_cache_size(hpx_context_t* hpx);

/* History management */
hyp_error_t hpx_add_to_history(hpx_context_t* hpx, const char* package, const char* command, int exit_code);
hyp_error_t hpx_show_history(hpx_context_t* hpx, size_t limit);
hyp_error_t hpx_clear_history(hpx_context_t* hpx);

/* Process management */
typedef struct {
    int pid;
    bool is_running;
    int exit_code;
    char* stdout_buffer;
    char* stderr_buffer;
    size_t stdout_size;
    size_t stderr_size;
} hpx_process_t;

hpx_process_t* hpx_spawn_process(const char* executable, char** args, size_t arg_count, const hpx_exec_options_t* options);
hyp_error_t hpx_wait_process(hpx_process_t* process, int timeout_ms);
hyp_error_t hpx_kill_process(hpx_process_t* process);
void hpx_process_free(hpx_process_t* process);

/* Platform-specific utilities */
#ifdef HYP_PLATFORM_WINDOWS
char* hpx_find_executable_windows(const char* name);
#else
char* hpx_find_executable_unix(const char* name);
#endif

/* Configuration */
hpx_config_t hpx_load_config(const char* config_path);
hyp_error_t hpx_save_config(const hpx_config_t* config, const char* config_path);
hpx_config_t hpx_default_config(void);

/* Error handling */
void hpx_error(hpx_context_t* hpx, const char* format, ...);
const char* hpx_get_error(hpx_context_t* hpx);
void hpx_clear_error(hpx_context_t* hpx);

/* Cleanup */
void hpx_destroy(hpx_context_t* hpx);

/* CLI command handlers */
int hpx_cmd_run(int argc, char** argv);
int hpx_cmd_exec(int argc, char** argv);
int hpx_cmd_create(int argc, char** argv);
int hpx_cmd_list(int argc, char** argv);
int hpx_cmd_info(int argc, char** argv);
int hpx_cmd_cache(int argc, char** argv);
int hpx_cmd_history(int argc, char** argv);
int hpx_cmd_help(int argc, char** argv);

/* Built-in templates and generators */
extern const char* hpx_builtin_templates[];
extern const size_t hpx_builtin_template_count;

hyp_error_t hpx_create_hyp_app(hpx_context_t* hpx, const char* name, const char* target_dir, char** options);
hyp_error_t hpx_create_hyp_lib(hpx_context_t* hpx, const char* name, const char* target_dir, char** options);
hyp_error_t hpx_create_hyp_cli(hpx_context_t* hpx, const char* name, const char* target_dir, char** options);

#endif /* HYP_HPX_H */