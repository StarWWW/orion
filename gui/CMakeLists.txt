# Orion desktop front end (Qt 6 Quick). It runs the orion command-line tool and drives it through
# its -json mode, so it needs no CUDA itself.
#
#   Linux:   cmake -S gui -B build/gui -DCMAKE_BUILD_TYPE=Release && cmake --build build/gui
#   Windows: python gui/build.py
cmake_minimum_required(VERSION 3.21)
project(OrionGui VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Qml Quick)
qt_standard_project_setup()

qt_add_executable(orion-gui WIN32 MACOSX_BUNDLE
    src/main.cpp
)

# Set before qt_add_qml_module, which reads them. The QML files live in qml/ but are placed at the
# module root, so they see each other without imports.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
file(GLOB qml_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} qml/*.qml qml/*.js)
foreach(f ${qml_files})
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(orion-gui
    URI Orion
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    SOURCES
        src/Engine.h src/Engine.cpp
        src/Search.h src/Search.cpp
        src/U256.h
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/StarField.qml
        qml/NavRail.qml
        qml/SetupPage.qml
        qml/MonitorPage.qml
        qml/FoundPage.qml
        qml/LearnPage.qml
        qml/Card.qml
        qml/StatTile.qml
        qml/SpeedGauge.qml
        qml/SpeedChart.qml
        qml/KeyspaceBar.qml
        qml/PipelineDiagram.qml
        qml/IconButton.qml
        qml/Icon.qml
        qml/Toast.qml
        qml/FoundOverlay.qml
        qml/Segmented.qml
        qml/PuzzlePicker.qml
        qml/Fmt.js
        qml/Puzzles.js
    RESOURCES
        icon.svg
)

target_include_directories(orion-gui PRIVATE src)
target_link_libraries(orion-gui PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick)

if(MSVC)
    target_compile_options(orion-gui PRIVATE /W3 /utf-8)
else()
    target_compile_options(orion-gui PRIVATE -Wall -Wextra)
endif()

include(GNUInstallDirs)
install(TARGETS orion-gui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
