/**
 * Hyper Programming Language - Package Manager Core (hpm)
 * 
 * Core implementation for the Hyper package manager.
 * Handles package installation, dependency resolution, and manifest management.
 */

#include "../../include/hpm.h"
#include "../../include/hyp_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define mkdir(path, mode) _mkdir(path)
#define access(path, mode) _access(path, mode)
#define F_OK 0
#else
#include <unistd.h>
#endif

/* Default configuration */
static const char* DEFAULT_REGISTRY_URL = "https://registry.hyper-lang.org";
static const char* DEFAULT_CACHE_DIR = ".hypkg";
static const char* MANIFEST_FILENAME = "package.yml";
static const char* LOCK_FILENAME = "package-lock.yml";

/* Utility functions */
static bool create_directory(const char* path) {
    if (!path) return false;
    
    /* Check if directory already exists */
    if (access(path, F_OK) == 0) {
        return true;
    }
    
    /* Create directory */
    if (mkdir(path, 0755) == 0) {
        return true;
    }
    
    /* Try to create parent directories recursively */
    char* path_copy = HYP_MALLOC(strlen(path) + 1);
    if (!path_copy) return false;
    
    strcpy(path_copy, path);
    
    char* slash = strrchr(path_copy, '/');
    if (!slash) {
        slash = strrchr(path_copy, '\\');
    }
    
    if (slash && slash != path_copy) {
        *slash = '\0';
        if (create_directory(path_copy)) {
            *slash = '/';
            bool result = (mkdir(path_copy, 0755) == 0);
            HYP_FREE(path_copy);
            return result;
        }
    }
    
    HYP_FREE(path_copy);
    return false;
}

static char* join_path(const char* base, const char* path) {
    if (!base || !path) return NULL;
    
    size_t base_len = strlen(base);
    size_t path_len = strlen(path);
    size_t total_len = base_len + path_len + 2; /* +2 for separator and null terminator */
    
    char* result = HYP_MALLOC(total_len);
    if (!result) return NULL;
    
    strcpy(result, base);
    
    /* Add separator if needed */
    if (base_len > 0 && base[base_len - 1] != '/' && base[base_len - 1] != '\\') {
#ifdef _WIN32
        strcat(result, "\\");
#else
        strcat(result, "/");
#endif
    }
    
    strcat(result, path);
    return result;
}

/* Package version functions */
struct hyp_package_version_t* hyp_package_version_create(const char* version_string) {
    if (!version_string) return NULL;
    
    hyp_package_version_t* version = HYP_MALLOC(sizeof(hyp_package_version_t));
    if (!version) return NULL;
    
    /* Parse semantic version (major.minor.patch) */
    version->major = 0;
    version->minor = 0;
    version->patch = 0;
    version->prerelease = NULL;
    version->build = NULL;
    
    char* version_copy = HYP_MALLOC(strlen(version_string) + 1);
    if (!version_copy) {
        HYP_FREE(version);
        return NULL;
    }
    strcpy(version_copy, version_string);
    
    /* Parse major version */
    char* token = strtok(version_copy, ".");
    if (token) {
        version->major = (uint32_t)atoi(token);
        
        /* Parse minor version */
        token = strtok(NULL, ".");
        if (token) {
            version->minor = (uint32_t)atoi(token);
            
            /* Parse patch version */
            token = strtok(NULL, "-+");
            if (token) {
                version->patch = (uint32_t)atoi(token);
                
                /* Parse prerelease */
                token = strtok(NULL, "+");
                if (token) {
                    version->prerelease = hyp_string_create(token);
                    
                    /* Parse build metadata */
                    token = strtok(NULL, "");
                    if (token) {
                        version->build = hyp_string_create(token);
                    }
                }
            }
        }
    }
    
    HYP_FREE(version_copy);
    return version;
}

void hyp_package_version_destroy(hyp_package_version_t* version) {
    if (!version) return;
    
    if (version->prerelease) {
        hyp_string_destroy(version->prerelease);
    }
    if (version->build) {
        hyp_string_destroy(version->build);
    }
    
    HYP_FREE(version);
}

int hyp_package_version_compare(hyp_package_version_t* a, hyp_package_version_t* b) {
    if (!a || !b) return 0;
    
    /* Compare major version */
    if (a->major != b->major) {
        return (a->major > b->major) ? 1 : -1;
    }
    
    /* Compare minor version */
    if (a->minor != b->minor) {
        return (a->minor > b->minor) ? 1 : -1;
    }
    
    /* Compare patch version */
    if (a->patch != b->patch) {
        return (a->patch > b->patch) ? 1 : -1;
    }
    
    /* Compare prerelease (if any) */
    if (a->prerelease && !b->prerelease) return -1;
    if (!a->prerelease && b->prerelease) return 1;
    if (a->prerelease && b->prerelease) {
        return strcmp(a->prerelease->chars, b->prerelease->chars);
    }
    
    return 0; /* Equal */
}

/* Package dependency functions */
hyp_package_dependency_t* hyp_package_dependency_create(const char* name, const char* version_spec) {
    if (!name) return NULL;
    
    hyp_package_dependency_t* dep = HYP_MALLOC(sizeof(hyp_package_dependency_t));
    if (!dep) return NULL;
    
    dep->name = hyp_string_create(name);
    dep->version_spec = version_spec ? hyp_string_create(version_spec) : NULL;
    dep->optional = false;
    dep->dev_only = false;
    
    if (!dep->name) {
        hyp_package_dependency_destroy(dep);
        return NULL;
    }
    
    return dep;
}

void hyp_package_dependency_destroy(hyp_package_dependency_t* dep) {
    if (!dep) return;
    
    if (dep->name) hyp_string_destroy(dep->name);
    if (dep->version_spec) hyp_string_destroy(dep->version_spec);
    
    HYP_FREE(dep);
}

/* Package manifest functions */
hyp_package_manifest_t* hyp_package_manifest_create(void) {
    hyp_package_manifest_t* manifest = HYP_MALLOC(sizeof(hyp_package_manifest_t));
    if (!manifest) return NULL;
    
    manifest->name = NULL;
    manifest->version = NULL;
    manifest->description = NULL;
    manifest->author = NULL;
    manifest->license = NULL;
    manifest->homepage = NULL;
    manifest->repository = NULL;
    manifest->main = NULL;
    manifest->dependencies = NULL;
    manifest->dependency_count = 0;
    manifest->scripts = NULL;
    manifest->script_count = 0;
    
    return manifest;
}

void hyp_package_manifest_destroy(hyp_package_manifest_t* manifest) {
    if (!manifest) return;
    
    if (manifest->name) hyp_string_destroy(manifest->name);
    if (manifest->version) hyp_package_version_destroy(manifest->version);
    if (manifest->description) hyp_string_destroy(manifest->description);
    if (manifest->author) hyp_string_destroy(manifest->author);
    if (manifest->license) hyp_string_destroy(manifest->license);
    if (manifest->homepage) hyp_string_destroy(manifest->homepage);
    if (manifest->repository) hyp_string_destroy(manifest->repository);
    if (manifest->main) hyp_string_destroy(manifest->main);
    
    for (size_t i = 0; i < manifest->dependency_count; i++) {
        hyp_package_dependency_destroy(manifest->dependencies[i]);
    }
    HYP_FREE(manifest->dependencies);
    
    for (size_t i = 0; i < manifest->script_count; i++) {
        if (manifest->scripts[i].name) hyp_string_destroy(manifest->scripts[i].name);
        if (manifest->scripts[i].command) hyp_string_destroy(manifest->scripts[i].command);
    }
    HYP_FREE(manifest->scripts);
    
    HYP_FREE(manifest);
}

/* HPM context functions */
hyp_hpm_context_t* hyp_hpm_create(void) {
    hyp_hpm_context_t* hpm = HYP_MALLOC(sizeof(hyp_hpm_context_t));
    if (!hpm) return NULL;
    
    hpm->config.registry_url = hyp_string_create(DEFAULT_REGISTRY_URL);
    hpm->config.cache_dir = hyp_string_create(DEFAULT_CACHE_DIR);
    hpm->config.offline_mode = false;
    hpm->config.verbose = false;
    
    hpm->current_manifest = NULL;
    hpm->lock_file = NULL;
    hpm->error_message = NULL;
    
    if (!hpm->config.registry_url || !hpm->config.cache_dir) {
        hyp_hpm_destroy(hpm);
        return NULL;
    }
    
    /* Create cache directory if it doesn't exist */
    create_directory(hpm->config.cache_dir->chars);
    
    return hpm;
}

void hyp_hpm_destroy(hyp_hpm_context_t* hpm) {
    if (!hpm) return;
    
    if (hpm->config.registry_url) hyp_string_destroy(hpm->config.registry_url);
    if (hpm->config.cache_dir) hyp_string_destroy(hpm->config.cache_dir);
    if (hpm->current_manifest) hyp_package_manifest_destroy(hpm->current_manifest);
    if (hpm->lock_file) hyp_package_manifest_destroy(hpm->lock_file);
    if (hpm->error_message) hyp_string_destroy(hpm->error_message);
    
    HYP_FREE(hpm);
}

/* Manifest loading and saving */
hyp_error_t hyp_hpm_load_manifest(hyp_hpm_context_t* hpm, const char* path) {
    if (!hpm) return HYP_ERROR_NULL_POINTER;
    
    const char* manifest_path = path ? path : MANIFEST_FILENAME;
    
    /* Check if manifest file exists */
    if (!hyp_file_exists(manifest_path)) {
        hpm->error_message = hyp_string_create("package.yml not found");
        return HYP_ERROR_FILE_NOT_FOUND;
    }
    
    /* TODO: Implement YAML parsing */
    /* For now, create a basic manifest */
    hpm->current_manifest = hyp_package_manifest_create();
    if (!hpm->current_manifest) {
        hpm->error_message = hyp_string_create("Failed to create manifest");
        return HYP_ERROR_OUT_OF_MEMORY;
    }
    
    /* Set default values */
    hpm->current_manifest->name = hyp_string_create("example-package");
    hpm->current_manifest->version = hyp_package_version_create("1.0.0");
    hpm->current_manifest->description = hyp_string_create("A Hyper package");
    
    return HYP_OK;
}

hyp_error_t hyp_hpm_save_manifest(hyp_hpm_context_t* hpm, const char* path) {
    if (!hpm || !hpm->current_manifest) return HYP_ERROR_NULL_POINTER;
    
    const char* manifest_path = path ? path : MANIFEST_FILENAME;
    
    /* TODO: Implement YAML generation */
    /* For now, create a basic YAML file */
    FILE* file = fopen(manifest_path, "w");
    if (!file) {
        hpm->error_message = hyp_string_create("Failed to create package.yml");
        return HYP_ERROR_FILE_WRITE;
    }
    
    fprintf(file, "name: %s\n", hpm->current_manifest->name ? hpm->current_manifest->name->chars : "unknown");
    
    if (hpm->current_manifest->version) {
        fprintf(file, "version: %u.%u.%u\n", 
                hpm->current_manifest->version->major,
                hpm->current_manifest->version->minor,
                hpm->current_manifest->version->patch);
    }
    
    if (hpm->current_manifest->description) {
        fprintf(file, "description: %s\n", hpm->current_manifest->description->chars);
    }
    
    if (hpm->current_manifest->author) {
        fprintf(file, "author: %s\n", hpm->current_manifest->author->chars);
    }
    
    if (hpm->current_manifest->license) {
        fprintf(file, "license: %s\n", hpm->current_manifest->license->chars);
    }
    
    if (hpm->current_manifest->main) {
        fprintf(file, "main: %s\n", hpm->current_manifest->main->chars);
    }
    
    /* Write dependencies */
    if (hpm->current_manifest->dependency_count > 0) {
        fprintf(file, "dependencies:\n");
        for (size_t i = 0; i < hpm->current_manifest->dependency_count; i++) {
            hyp_package_dependency_t* dep = hpm->current_manifest->dependencies[i];
            fprintf(file, "  %s: %s\n", 
                    dep->name->chars,
                    dep->version_spec ? dep->version_spec->chars : "*");
        }
    }
    
    /* Write scripts */
    if (hpm->current_manifest->script_count > 0) {
        fprintf(file, "scripts:\n");
        for (size_t i = 0; i < hpm->current_manifest->script_count; i++) {
            fprintf(file, "  %s: %s\n", 
                    hpm->current_manifest->scripts[i].name->chars,
                    hpm->current_manifest->scripts[i].command->chars);
        }
    }
    
    fclose(file);
    return HYP_OK;
}

/* Package operations */
hyp_error_t hyp_hpm_install_package(hyp_hpm_context_t* hpm, const char* package_name, const char* version_spec) {
    if (!hpm || !package_name) return HYP_ERROR_NULL_POINTER;
    
    if (hpm->config.verbose) {
        printf("Installing package: %s\n", package_name);
    }
    
    /* TODO: Implement package installation */
    /* This would involve:
     * 1. Resolving the package version
     * 2. Downloading the package
     * 3. Extracting to cache directory
     * 4. Installing dependencies
     * 5. Updating manifest and lock file
     */
    
    hpm->error_message = hyp_string_create("Package installation not yet implemented");
    return HYP_ERROR_NOT_IMPLEMENTED;
}

hyp_error_t hyp_hpm_remove_package(hyp_hpm_context_t* hpm, const char* package_name) {
    if (!hpm || !package_name) return HYP_ERROR_NULL_POINTER;
    
    if (hpm->config.verbose) {
        printf("Removing package: %s\n", package_name);
    }
    
    /* TODO: Implement package removal */
    hpm->error_message = hyp_string_create("Package removal not yet implemented");
    return HYP_ERROR_NOT_IMPLEMENTED;
}

hyp_error_t hyp_hpm_update_package(hyp_hpm_context_t* hpm, const char* package_name) {
    if (!hpm) return HYP_ERROR_NULL_POINTER;
    
    if (package_name) {
        if (hpm->config.verbose) {
            printf("Updating package: %s\n", package_name);
        }
    } else {
        if (hpm->config.verbose) {
            printf("Updating all packages\n");
        }
    }
    
    /* TODO: Implement package updates */
    hpm->error_message = hyp_string_create("Package updates not yet implemented");
    return HYP_ERROR_NOT_IMPLEMENTED;
}

hyp_error_t hyp_hpm_init_package(hyp_hpm_context_t* hpm, const char* package_name) {
    if (!hpm) return HYP_ERROR_NULL_POINTER;
    
    /* Create new manifest */
    if (hpm->current_manifest) {
        hyp_package_manifest_destroy(hpm->current_manifest);
    }
    
    hpm->current_manifest = hyp_package_manifest_create();
    if (!hpm->current_manifest) {
        hpm->error_message = hyp_string_create("Failed to create manifest");
        return HYP_ERROR_OUT_OF_MEMORY;
    }
    
    /* Set package name */
    const char* name = package_name ? package_name : "my-hyper-package";
    hpm->current_manifest->name = hyp_string_create(name);
    hpm->current_manifest->version = hyp_package_version_create("1.0.0");
    hpm->current_manifest->description = hyp_string_create("A new Hyper package");
    hpm->current_manifest->main = hyp_string_create("src/main.hxp");
    hpm->current_manifest->license = hyp_string_create("MIT");
    
    /* Save manifest */
    hyp_error_t result = hyp_hpm_save_manifest(hpm, NULL);
    if (result != HYP_OK) {
        return result;
    }
    
    /* Create basic directory structure */
    create_directory("src");
    create_directory("tests");
    create_directory("docs");
    
    /* Create basic main.hxp file */
    FILE* main_file = fopen("src/main.hxp", "w");
    if (main_file) {
        fprintf(main_file, "// %s - Main entry point\n\n", name);
        fprintf(main_file, "fn main() {\n");
        fprintf(main_file, "    print(\"Hello from %s!\");\n", name);
        fprintf(main_file, "}\n");
        fclose(main_file);
    }
    
    if (hpm->config.verbose) {
        printf("Initialized new Hyper package: %s\n", name);
    }
    
    return HYP_OK;
}

/* Error handling */
const char* hyp_hpm_get_error(hyp_hpm_context_t* hpm) {
    if (!hpm || !hpm->error_message) return "Unknown error";
    return hpm->error_message->chars;
}

void hyp_hpm_clear_error(hyp_hpm_context_t* hpm) {
    if (!hpm) return;
    
    if (hpm->error_message) {
        hyp_string_destroy(hpm->error_message);
        hpm->error_message = NULL;
    }
}

/* Placeholder implementations for remaining functions */
hyp_error_t hyp_hpm_search_packages(hyp_hpm_context_t* hpm, const char* query, hyp_package_info_t** results, size_t* result_count) {
    return HYP_ERROR_NOT_IMPLEMENTED;
}

hyp_error_t hyp_hpm_publish_package(hyp_hpm_context_t* hpm, const char* package_path) {
    return HYP_ERROR_NOT_IMPLEMENTED;
}

hyp_error_t hyp_hpm_list_packages(hyp_hpm_context_t* hpm, hyp_package_info_t** packages, size_t* package_count) {
    return HYP_ERROR_NOT_IMPLEMENTED;
}

hyp_error_t hyp_hpm_show_package_info(hyp_hpm_context_t* hpm, const char* package_name, hyp_package_info_t* info) {
    return HYP_ERROR_NOT_IMPLEMENTED;
}

hyp_error_t hyp_hpm_run_script(hyp_hpm_context_t* hpm, const char* script_name) {
    return HYP_ERROR_NOT_IMPLEMENTED;
}