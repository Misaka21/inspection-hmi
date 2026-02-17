# src/ui/CMakeLists.txt
#
# hmi_ui – Qt widget layer (Engineer mode + Operator mode)
#
# Directory layout expected under src/ui/:
#
#   MainWindow.cpp / .h          – QMainWindow shell; mode switcher
#   TopBar.cpp / .h              – toolbar: new/open project, import CAD,
#                                  gateway connection status, mode toggle
#   ProjectPanel.cpp / .h        – left dock: CAD model tree, point list,
#                                  layer visibility controls
#   SceneViewport.cpp / .h       – central VTK widget wrapper
#                                  (QVTKOpenGLNativeWidget + toolbar)
#   EditPanel.cpp / .h           – right dock (Engineer): point annotation,
#                                  capture config, plan params
#   MonitorPanel.cpp / .h        – right dock (Engineer): robot status,
#                                  interlock, event list, thumbnail preview
#   StatusBar.cpp / .h           – bottom bar: step indicator, log, robot
#                                  status cards
#
#   operator/TaskCard.cpp / .h           – Operator mode: task card widget
#   operator/NavPanel.cpp / .h           – Operator mode: 2D nav map + AGV
#   operator/RobotStatusPanel.cpp / .h   – Operator mode: AGV+arm status cards
#   operator/ControlPanel.cpp / .h       – Operator mode: start/pause/resume/stop
#   operator/ResultPanel.cpp / .h        – Operator mode: captures + defects
#
# VTK/Qt note: see src/scene/CMakeLists.txt for why VTK::GUISupportQt is not
# used as a CMake target; instead ${VTK_GUISUPPORTQT_LIB} is linked directly.

# ---------------------------------------------------------------------------
# Enable AUTOUIC for this directory only (safer than enabling it globally)
# ---------------------------------------------------------------------------
set(CMAKE_AUTOUIC ON)

# Tell AUTOUIC where to look for .ui files produced by Qt Designer
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# Source files – engineer-mode widgets
# ---------------------------------------------------------------------------
set(UI_ENGINEER_SOURCES
    MainWindow.cpp
    TopBar.cpp
    ProjectPanel.cpp
    SceneViewport.cpp
    EditPanel.cpp
    StatusLog.cpp
    MonitorPanel.cpp
    StatusBar.cpp
)

set(UI_ENGINEER_HEADERS
    MainWindow.h
    TopBar.h
    ProjectPanel.h
    SceneViewport.h
    EditPanel.h
    StatusLog.h
    MonitorPanel.h
    StatusBar.h
)

# ---------------------------------------------------------------------------
# Source files – operator-mode widgets (subdirectory)
# ---------------------------------------------------------------------------
set(UI_OPERATOR_SOURCES
    operator/OperatorWindow.cpp
    operator/TaskCardWidget.cpp
    operator/NavMapWidget.cpp
    operator/RobotStatusWidget.cpp
    operator/ControlPanel.cpp
    operator/ResultPanel.cpp
    # Legacy stubs (keep for compatibility)
    operator/TaskCard.cpp
    operator/NavPanel.cpp
    operator/RobotStatusPanel.cpp
)

set(UI_OPERATOR_HEADERS
    operator/OperatorWindow.h
    operator/TaskCardWidget.h
    operator/NavMapWidget.h
    operator/RobotStatusWidget.h
    operator/ControlPanel.h
    operator/ResultPanel.h
    # Legacy stubs (keep for compatibility)
    operator/TaskCard.h
    operator/NavPanel.h
    operator/RobotStatusPanel.h
)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(hmi_ui STATIC
    ${UI_ENGINEER_SOURCES}
    ${UI_ENGINEER_HEADERS}
    ${UI_OPERATOR_SOURCES}
    ${UI_OPERATOR_HEADERS}
)

add_library(hmi::ui ALIAS hmi_ui)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
target_include_directories(hmi_ui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        # So that operator/*.h can be found as "operator/NavPanel.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/operator"
        # VTK public headers (QVTKOpenGLNativeWidget.h etc.)
        /usr/include/vtk-9.1
    PRIVATE
        # AUTOUIC places generated ui_*.h files here
        "${CMAKE_CURRENT_BINARY_DIR}"
)

# ---------------------------------------------------------------------------
# Link dependencies
# ---------------------------------------------------------------------------
target_link_libraries(hmi_ui
    PUBLIC
        # Core business logic and gateway client
        hmi_core

        # 3D scene management (also pulls in VTK non-Qt targets transitively)
        hmi_scene

        # Qt modules used in widget implementations
        Qt6::Widgets
        Qt6::OpenGLWidgets
        Qt6::Network

        # VTK non-Qt targets needed directly in the UI layer
        VTK::CommonCore
        VTK::CommonDataModel
        VTK::FiltersSources
        VTK::InteractionStyle
        VTK::InteractionWidgets
        VTK::RenderingCore
        VTK::RenderingOpenGL2
        VTK::RenderingAnnotation
        VTK::RenderingFreeType
)

# ---------------------------------------------------------------------------
# VTK autoinit
# ---------------------------------------------------------------------------
vtk_module_autoinit(
    TARGETS hmi_ui
    MODULES ${VTK_LIBRARIES}
)

# ---------------------------------------------------------------------------
# Compiler options
# ---------------------------------------------------------------------------
target_compile_options(hmi_ui PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-deprecated-declarations
)

target_compile_features(hmi_ui PUBLIC cxx_std_17)
