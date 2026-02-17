# src/scene/CMakeLists.txt
#
# hmi_scene – VTK-based 3D scene management
#
# Responsibilities:
#   - CadScene:      loads STL / OBJ / PLY models via VTK readers,
#                    owns the vtkRenderer and the scene graph.
#   - PointAnnotator: manages inspection-point actors (sphere + normal arrow +
#                    camera frustum), translates UI pick events into
#                    InspectionTarget proto messages.
#
# VTK/Qt note:
#   The Ubuntu 22.04 system VTK 9.1 package was built against Qt5.
#   VTK::GUISupportQt is therefore NOT used as a CMake target here because it
#   injects Qt5::Widgets into the link interface, which conflicts with our
#   Qt6 build.  Instead, the vtkGUISupportQt shared library is linked directly
#   via the VTK_GUISUPPORTQT_LIB path variable set in the root CMakeLists.txt.

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
set(SCENE_SOURCES
    CadScene.cpp
    PointAnnotator.cpp
    QVTKWidget.cpp
)

set(SCENE_HEADERS
    CadScene.h
    PointAnnotator.h
    QVTKWidget.h
)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(hmi_scene STATIC
    ${SCENE_SOURCES}
    ${SCENE_HEADERS}
)

add_library(hmi::scene ALIAS hmi_scene)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
target_include_directories(hmi_scene
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        # VTK public headers (required for QVTKOpenGLNativeWidget.h etc.)
        /usr/include/vtk-9.1
)

# ---------------------------------------------------------------------------
# Link dependencies
# ---------------------------------------------------------------------------
target_link_libraries(hmi_scene
    PUBLIC
        # Core Qt widget support
        Qt6::Widgets
        Qt6::OpenGLWidgets

        # HMI core: GatewayClient types, proto-generated structs
        hmi_core

        # VTK modules available as proper CMake targets (Qt5-free)
        VTK::CommonCore
        VTK::CommonDataModel
        VTK::CommonTransforms
        VTK::FiltersSources
        VTK::FiltersGeneral
        VTK::InteractionStyle
        VTK::InteractionWidgets
        VTK::IOGeometry
        VTK::IOPLY
        VTK::RenderingCore
        VTK::RenderingOpenGL2
        VTK::RenderingAnnotation
        VTK::RenderingFreeType
)

# ---------------------------------------------------------------------------
# VTK autoinit
# vtk_module_autoinit wires the static-initialiser registration that VTK
# requires so that object-factory overrides (e.g. OpenGL2 render window) are
# loaded.  Called on each library target; the final executable call in the
# root CMakeLists.txt is the authoritative one for the runtime.
# ---------------------------------------------------------------------------
vtk_module_autoinit(
    TARGETS hmi_scene
    MODULES ${VTK_LIBRARIES}
)

# ---------------------------------------------------------------------------
# Compiler options
# ---------------------------------------------------------------------------
target_compile_options(hmi_scene PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-deprecated-declarations
)

target_compile_features(hmi_scene PUBLIC cxx_std_17)
