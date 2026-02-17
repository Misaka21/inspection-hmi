# src/core/CMakeLists.txt
#
# hmi_core – gateway communication layer
#
# Responsibilities:
#   - GatewayClient: wraps the gRPC stub for InspectionGateway, exposes a
#     Qt-friendly async API (signals/slots, QFuture) to the rest of the HMI.
#
# Public dependencies exposed to consumers via target_link_libraries PUBLIC:
#   - inspection_proto (generated pb/grpc sources + headers)
#   - Qt6::Core        (QObject, signals/slots, QFuture)
#   - gRPC::grpc++     (channel, stubs, streaming readers)
#   - protobuf::libprotobuf

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
# Only GatewayClient.cpp is required at this stage.  Additional translation
# units (e.g. SessionManager.cpp, TaskPoller.cpp) can be appended here as
# the project grows; using an explicit list is preferred over GLOB so that
# CMake re-runs automatically when files are added.
set(CORE_SOURCES
    GatewayClient.cpp
)

# Header-only files listed here are picked up by Qt Creator / CLion for
# navigation and are harmless to list explicitly.
set(CORE_HEADERS
    Types.h
    GatewayClient.h
)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(hmi_core STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

# Alias so consumers outside this directory can use the namespaced form
add_library(hmi::core ALIAS hmi_core)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
target_include_directories(hmi_core
    PUBLIC
        # Allow #include "GatewayClient.h" and #include "Types.h"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        # Allow #include "core/Types.h" from consumers (resolves via src/)
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    PRIVATE
        # Generated proto headers (inspection_proto propagates this via PUBLIC
        # include dirs, but being explicit avoids ordering surprises)
        "${CMAKE_CURRENT_BINARY_DIR}/../proto_gen"
)

# ---------------------------------------------------------------------------
# Link dependencies
# ---------------------------------------------------------------------------
target_link_libraries(hmi_core
    PUBLIC
        # Proto generated code + transitive protobuf + gRPC libs
        inspection_proto
        # Qt core for QObject, QString, QFuture, signals/slots
        Qt6::Core
        # Qt GUI for QVector3D, QQuaternion used in Types.h
        Qt6::Gui
        Qt6::Network
    PRIVATE
        # gRPC channel and completion queue – consumers only need the Qt API
        gRPC::grpc++
)

# ---------------------------------------------------------------------------
# Compiler options
# ---------------------------------------------------------------------------
target_compile_options(hmi_core PRIVATE
    -Wall
    -Wextra
    # Suppress common noise from generated gRPC headers included transitively
    -Wno-unused-parameter
    -Wno-deprecated-declarations
)

# ---------------------------------------------------------------------------
# C++ standard propagation
# ---------------------------------------------------------------------------
target_compile_features(hmi_core PUBLIC cxx_std_17)
