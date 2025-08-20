cmake_minimum_required(VERSION 3.16)
project(Hyper VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(MSVC)
    # MSVC compiler flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /Zi /DDEBUG")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /O2")
    endif()
else()
    # GCC/Clang compiler flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DDEBUG")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
    endif()
endif()

# Include directories
include_directories(include)

# Source files
set(COMMON_SOURCES
    src/common/hyp_common.c
)

set(COMPILER_SOURCES
    src/hypc/main.c
    src/lexer/lexer.c
    src/parser/parser.c
    src/transpiler/transpiler.c
)

set(RUNTIME_SOURCES
    src/hyprun/main.c
    src/runtime/hyp_runtime.c
    src/lexer/lexer.c
    src/parser/parser.c
)

set(HPM_SOURCES
    src/hpm/main.c
    src/hpm/hpm.c
)

set(HPX_SOURCES
    src/hpx/main.c
    src/hpx/hpx.c
)

# Create executables
add_executable(hypc ${COMPILER_SOURCES} ${COMMON_SOURCES})
add_executable(hyprun ${RUNTIME_SOURCES} ${COMMON_SOURCES})
add_executable(hpm ${HPM_SOURCES} ${COMMON_SOURCES})
add_executable(hpx ${HPX_SOURCES} ${COMMON_SOURCES})

# Set output directory
set_target_properties(hypc hyprun hpm hpx PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    set_target_properties(hypc hyprun hpm hpx PROPERTIES
        OUTPUT_NAME_DEBUG "${TARGET_NAME}_d"
    )
endif()

# Install configuration
if(WIN32)
    set(CMAKE_INSTALL_PREFIX "$ENV{LOCALAPPDATA}/Miko/Hyper" CACHE PATH "Installation directory" FORCE)
else()
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Installation directory" FORCE)
endif()

# Install targets
install(TARGETS hypc hyprun hpm hpx
    RUNTIME DESTINATION bin
)

# Custom targets
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMENT "Cleaning all build artifacts"
)

# Development target with debug flags
add_custom_target(dev
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    COMMENT "Building in development mode with debug flags"
)

# Test target placeholder
add_custom_target(test
    COMMAND echo "Running tests..."
    COMMENT "Running test suite"
)

# Print build information
message(STATUS "Hyper Programming Language Build Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C Flags: ${CMAKE_C_FLAGS}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")