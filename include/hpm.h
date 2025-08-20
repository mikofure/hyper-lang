/**
 * Hyper Programming Language - Package Manager (hpm)
 * 
 * The package manager handles dependency management, including installing,
 * updating, removing, and publishing packages. It manages the .hypkg/
 * directory and package.yml manifest files.
 */

#ifndef HYP_HPM_H
#define HYP_HPM_H

#include "hyp_common.h"

/* Package version structure */
typedef struct {
    int major;
    int minor;
    int patch;
    char* prerelease;
    char* build;
} hyp_version_t;

/* Package dependency */
typedef struct {
    char* name;
    char* version_spec;  /* e.g., "^1.2.3", ">=2.0.0", "~1.1.0" */
    char* source;        /* Optional: git URL, file path, etc. */
    bool is_dev;         /* Development dependency */
} hyp_dependency_t;

/* Package script */
typedef struct {
    char* name;
    char* command;
} hyp_script_t;

/* Package manifest (package.yml) */
typedef struct {
    char* name;
    hyp_version_t version;
    char* description;
    char* author;
    char* license;
    char* homepage;
    char* repository;
    
    /* Entry points */
    char* main;          /* Main entry point */
    char* cli;           /* CLI entry point */
    char* web;           /* Web entry point */
    
    /* Dependencies */
    hyp_dependency_t* dependencies;
    size_t dependency_count;
    hyp_dependency_t* dev_dependencies;
    size_t dev_dependency_count;
    
    /* Scripts */
    hyp_script_t* scripts;
    size_t script_count;
    
    /* Keywords for search */
    char** keywords;
    size_t keyword_count;
    
    /* Files to include in package */
    char** files;
    size_t file_count;
} hyp_package_t;

/* Package registry entry */
typedef struct {
    char* name;
    hyp_version_t* versions;
    size_t version_count;
    char* latest_version;
    char* download_url;
    size_t download_count;
    char* checksum;
} hyp_registry_entry_t;

/* Package manager configuration */
typedef struct {
    char* registry_url;
    char* cache_dir;
    char* global_dir;
    bool offline_mode;
    bool verify_checksums;
    int timeout_seconds;
    char* auth_token;
} hpm_config_t;

/* Package manager context */
typedef struct {
    hpm_config_t config;
    char* project_root;
    char* hypkg_dir;
    hyp_package_t* current_package;
    hyp_arena_t* arena;
    
    /* Installed packages cache */
    struct {
        char** names;
        hyp_version_t* versions;
        char** paths;
        size_t count;
        size_t capacity;
    } installed;
    
    /* Error handling */
    bool has_error;
    char error_message[512];
} hpm_context_t;

/* Package installation options */
typedef struct {
    bool save_dev;       /* Save as dev dependency */
    bool save_exact;     /* Save exact version */
    bool global;         /* Install globally */
    bool force;          /* Force reinstall */
    bool no_scripts;     /* Skip running scripts */
    char* version;       /* Specific version to install */
} hpm_install_options_t;

/* Function declarations */

/**
 * Initialize package manager context
 * @param hpm The package manager context to initialize
 * @param project_root Root directory of the project
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_init(hpm_context_t* hpm, const char* project_root);

/**
 * Load package manifest from package.yml
 * @param hpm The package manager context
 * @param manifest_path Path to package.yml file
 * @return Loaded package manifest, or NULL on error
 */
hyp_package_t* hpm_load_manifest(hpm_context_t* hpm, const char* manifest_path);

/**
 * Save package manifest to package.yml
 * @param hpm The package manager context
 * @param package The package manifest to save
 * @param manifest_path Path to save the manifest
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_save_manifest(hpm_context_t* hpm, const hyp_package_t* package, const char* manifest_path);

/**
 * Install a package
 * @param hpm The package manager context
 * @param package_name Name of the package to install
 * @param options Installation options
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_install(hpm_context_t* hpm, const char* package_name, const hpm_install_options_t* options);

/**
 * Install all dependencies from package.yml
 * @param hpm The package manager context
 * @param include_dev Whether to install dev dependencies
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_install_all(hpm_context_t* hpm, bool include_dev);

/**
 * Update a package to the latest version
 * @param hpm The package manager context
 * @param package_name Name of the package to update (NULL for all)
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_update(hpm_context_t* hpm, const char* package_name);

/**
 * Remove a package
 * @param hpm The package manager context
 * @param package_name Name of the package to remove
 * @param remove_from_manifest Whether to remove from package.yml
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_remove(hpm_context_t* hpm, const char* package_name, bool remove_from_manifest);

/**
 * Search for packages in the registry
 * @param hpm The package manager context
 * @param query Search query
 * @param results Array to store search results
 * @param max_results Maximum number of results
 * @return Number of results found, or -1 on error
 */
int hpm_search(hpm_context_t* hpm, const char* query, hyp_registry_entry_t* results, size_t max_results);

/**
 * Publish a package to the registry
 * @param hpm The package manager context
 * @param package_dir Directory containing the package to publish
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_publish(hpm_context_t* hpm, const char* package_dir);

/**
 * List installed packages
 * @param hpm The package manager context
 * @param global Whether to list global packages
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_list(hpm_context_t* hpm, bool global);

/**
 * Show package information
 * @param hpm The package manager context
 * @param package_name Name of the package
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_info(hpm_context_t* hpm, const char* package_name);

/**
 * Run a script defined in package.yml
 * @param hpm The package manager context
 * @param script_name Name of the script to run
 * @param args Additional arguments to pass to the script
 * @param arg_count Number of additional arguments
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_run_script(hpm_context_t* hpm, const char* script_name, char** args, size_t arg_count);

/**
 * Initialize a new package (create package.yml)
 * @param hpm The package manager context
 * @param package_name Name of the new package
 * @param interactive Whether to prompt for details
 * @return HYP_OK on success, error code on failure
 */
hyp_error_t hpm_init_package(hpm_context_t* hpm, const char* package_name, bool interactive);

/* Version utilities */
hyp_version_t hpm_parse_version(const char* version_str);
char* hpm_version_to_string(const hyp_version_t* version);
int hpm_version_compare(const hyp_version_t* a, const hyp_version_t* b);
bool hpm_version_satisfies(const hyp_version_t* version, const char* spec);
void hpm_version_free(hyp_version_t* version);

/* Dependency resolution */
hyp_error_t hpm_resolve_dependencies(hpm_context_t* hpm, hyp_dependency_t* deps, size_t dep_count);
bool hpm_check_dependency_conflicts(hpm_context_t* hpm);

/* Package utilities */
hyp_package_t* hpm_create_package(const char* name, const char* version);
void hpm_package_add_dependency(hyp_package_t* package, const char* name, const char* version_spec, bool is_dev);
void hpm_package_add_script(hyp_package_t* package, const char* name, const char* command);
void hpm_package_free(hyp_package_t* package);

/* File operations */
hyp_error_t hpm_download_package(hpm_context_t* hpm, const char* package_name, const char* version, const char* dest_dir);
hyp_error_t hpm_extract_package(hpm_context_t* hpm, const char* archive_path, const char* dest_dir);
hyp_error_t hpm_create_package_archive(hpm_context_t* hpm, const char* package_dir, const char* archive_path);

/* Registry operations */
hyp_error_t hpm_fetch_package_info(hpm_context_t* hpm, const char* package_name, hyp_registry_entry_t* entry);
hyp_error_t hpm_upload_package(hpm_context_t* hpm, const char* archive_path, const hyp_package_t* package);

/* Configuration */
hpm_config_t hpm_load_config(const char* config_path);
hyp_error_t hpm_save_config(const hpm_config_t* config, const char* config_path);
hpm_config_t hpm_default_config(void);

/* Cache management */
hyp_error_t hpm_clear_cache(hpm_context_t* hpm);
hyp_error_t hpm_rebuild_cache(hpm_context_t* hpm);

/* Lock file operations */
hyp_error_t hpm_create_lock_file(hpm_context_t* hpm);
hyp_error_t hpm_read_lock_file(hpm_context_t* hpm);

/* Validation */
bool hpm_validate_package_name(const char* name);
bool hpm_validate_version(const char* version);
hyp_error_t hpm_validate_manifest(const hyp_package_t* package);

/* Error handling */
void hpm_error(hpm_context_t* hpm, const char* format, ...);
const char* hpm_get_error(hpm_context_t* hpm);
void hpm_clear_error(hpm_context_t* hpm);

/* Cleanup */
void hpm_destroy(hpm_context_t* hpm);

/* CLI command handlers */
int hpm_cmd_install(int argc, char** argv);
int hpm_cmd_update(int argc, char** argv);
int hpm_cmd_remove(int argc, char** argv);
int hpm_cmd_search(int argc, char** argv);
int hpm_cmd_publish(int argc, char** argv);
int hpm_cmd_list(int argc, char** argv);
int hpm_cmd_info(int argc, char** argv);
int hpm_cmd_run(int argc, char** argv);
int hpm_cmd_init(int argc, char** argv);
int hpm_cmd_help(int argc, char** argv);

#endif /* HYP_HPM_H */