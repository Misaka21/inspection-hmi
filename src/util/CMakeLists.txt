# src/util/CMakeLists.txt
#
# hmi_util – header-only utility library
#
# Provides shared helpers that have no compiled translation units:
#   - Type-safe unit wrappers (degrees / radians / metres)
#   - Proto <-> Qt conversion helpers  (e.g. Pose2D -> QPointF)
#   - Logging macros / colour constants for the HMI status display
#   - Throttle / debounce helpers for gRPC stream consumers
#
# Because this is a header-only library it is declared as INTERFACE so that
# CMake never tries to compile it directly; it is pure propagation of include
# paths and compile definitions to consumers.

# ---------------------------------------------------------------------------
# Generate HmiColors.h from a template so that hex color strings (which
# contain '#') can be embedded without tripping the preprocessor definition
# restriction that prevents '#' characters in -D flags.
# ---------------------------------------------------------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/HmiColors.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/HmiColors.h"
    @ONLY
)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(hmi_util INTERFACE)

add_library(hmi::util ALIAS hmi_util)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
# PUBLIC / INTERFACE means these paths are forwarded to every target that
# links against hmi_util.
target_include_directories(hmi_util
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        # Exposes the configured HmiColors.h to all consumers
        "${CMAKE_CURRENT_BINARY_DIR}"
)

# ---------------------------------------------------------------------------
# Link dependencies
# ---------------------------------------------------------------------------
# Qt6::Core is needed for QObject, QString, QPointF, etc. used in inline
# conversion helpers.
# hmi_core is needed for core/Types.h (Pose2D, Pose3D, etc.) used in
# CoordTransform.h.
target_link_libraries(hmi_util
    INTERFACE
        Qt6::Core
        hmi_core
)

# ---------------------------------------------------------------------------
# Compile features
# ---------------------------------------------------------------------------
# Enforce C++17 for all consumers (structured bindings, if constexpr, etc.)
target_compile_features(hmi_util INTERFACE cxx_std_17)

# ---------------------------------------------------------------------------
# Compile definitions (non-colour constants safe to pass via -D)
# ---------------------------------------------------------------------------
# gRPC stream throttle targets (milliseconds):
#   STATE:  ~10 Hz -> 100 ms
#   EVENT:  ~20 Hz ->  50 ms
target_compile_definitions(hmi_util
    INTERFACE
        HMI_STREAM_THROTTLE_STATE_MS=100
        HMI_STREAM_THROTTLE_EVENT_MS=50
)
