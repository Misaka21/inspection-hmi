cmake_minimum_required(VERSION 3.22)

project(inspection_hmi
    VERSION 1.0
    DESCRIPTION "Inspection HMI - Qt6 / VTK / gRPC upper-computer interface"
    LANGUAGES C CXX
)
# NOTE: The C language is declared alongside CXX because the Ubuntu 22.04
# system VTK 9.1 package unconditionally pulls in VTK::mpi which depends on
# MPI::MPI_C.  Without the C language enabled CMake cannot resolve that
# imported target even when MPI itself is not found.

# ---------------------------------------------------------------------------
# C++ standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Qt automatic code-generation tools
# ---------------------------------------------------------------------------
set(CMAKE_AUTOMOC ON)   # moc for Q_OBJECT / Q_GADGET
set(CMAKE_AUTORCC ON)   # rcc for .qrc resource files
# AUTOUIC is intentionally left OFF; UI files will be compiled explicitly in
# each sub-library so that include paths remain predictable.
set(CMAKE_AUTOUIC OFF)

# ---------------------------------------------------------------------------
# Build type default
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
        "Build type (Debug | Release | RelWithDebInfo | MinSizeRel)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
        STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ---------------------------------------------------------------------------
# Compiler warning flags
# ---------------------------------------------------------------------------
add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations>
)

# ---------------------------------------------------------------------------
# Find Qt6
# ---------------------------------------------------------------------------
find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGLWidgets
    Network
)

message(STATUS "Qt6 version: ${Qt6_VERSION}")

# ---------------------------------------------------------------------------
# Find VTK 9.1
#
# VTK headers are used for rendering but we don't need VTK::GUISupportQt
# because we use our own Qt6-compatible QVTKWidget instead.
# ---------------------------------------------------------------------------
find_package(VTK 9.1 REQUIRED
    CONFIG
    HINTS /usr/lib/x86_64-linux-gnu/cmake/vtk-9.1
    COMPONENTS
        CommonCore
        CommonDataModel
        CommonTransforms
        FiltersSources
        FiltersGeneral
        InteractionStyle
        InteractionWidgets
        IOGeometry
        IOPLY
        RenderingCore
        RenderingOpenGL2
        RenderingAnnotation
        RenderingFreeType
)

message(STATUS "VTK version: ${VTK_VERSION}")

# ---------------------------------------------------------------------------
# Find Protobuf
# ---------------------------------------------------------------------------
find_package(Protobuf REQUIRED)

if(NOT Protobuf_FOUND)
    message(FATAL_ERROR "Protobuf not found. Install libprotobuf-dev.")
endif()

message(STATUS "Protobuf version: ${Protobuf_VERSION}")
message(STATUS "protoc: ${Protobuf_PROTOC_EXECUTABLE}")

# ---------------------------------------------------------------------------
# Find gRPC
# Prefer a CMake config package; fall back to pkg-config for the Ubuntu 22.04
# system packages (libgrpc++-dev 1.30.2) which do not ship a CMake config.
# ---------------------------------------------------------------------------
find_package(gRPC QUIET CONFIG)

if(gRPC_FOUND)
    message(STATUS "gRPC found via CMake config (version ${gRPC_VERSION})")
else()
    message(STATUS "gRPC CMake config not found; trying pkg-config ...")
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(GRPC REQUIRED IMPORTED_TARGET
        grpc++
        grpc
        gpr
    )

    # Expose a consistent imported target for downstream use even when the
    # system package does not ship CMake config files.
    if(NOT TARGET gRPC::grpc++)
        add_library(_grpc_imported INTERFACE)
        target_link_libraries(_grpc_imported INTERFACE
            PkgConfig::GRPC
        )
        add_library(gRPC::grpc++ ALIAS _grpc_imported)
    endif()

    # grpc_cpp_plugin must still be found separately.
    if(NOT GRPC_CPP_PLUGIN)
        find_program(GRPC_CPP_PLUGIN
            NAMES grpc_cpp_plugin
            HINTS /usr/bin /usr/local/bin
            REQUIRED
        )
    endif()

    message(STATUS "gRPC libraries (pkg-config): ${GRPC_LINK_LIBRARIES}")
    message(STATUS "grpc_cpp_plugin: ${GRPC_CPP_PLUGIN}")
endif()

# ---------------------------------------------------------------------------
# Proto code generation
# ---------------------------------------------------------------------------
include(cmake/Proto.cmake)

set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../inspection-api/proto")
set(PROTO_FILE "${PROTO_DIR}/inspection_gateway.proto")

if(NOT EXISTS "${PROTO_FILE}")
    message(FATAL_ERROR
        "Proto file not found: ${PROTO_FILE}\n"
        "Ensure the inspection-api repository is checked out alongside "
        "inspection-hmi."
    )
endif()

generate_grpc_cpp(PROTO_FILES "${PROTO_FILE}")

# ---------------------------------------------------------------------------
# Sub-libraries
# ---------------------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/scene)
add_subdirectory(src/ui)
add_subdirectory(src/util)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(inspection_hmi
    src/main.cpp
)

target_link_libraries(inspection_hmi
    PRIVATE
        hmi_core
        hmi_scene
        hmi_ui
        hmi_util
        Qt6::Widgets
        Qt6::OpenGLWidgets
        Qt6::Network
)

# VTK requires this call to wire up the object factory initialisation for
# every executable and shared library that uses VTK rendering.
vtk_module_autoinit(
    TARGETS inspection_hmi
    MODULES ${VTK_LIBRARIES}
)

# ---------------------------------------------------------------------------
# Install rules (optional – useful for packaging on the AGX target)
# ---------------------------------------------------------------------------
install(TARGETS inspection_hmi
    RUNTIME DESTINATION bin
)
