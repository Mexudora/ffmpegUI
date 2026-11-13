# настройка минимаьной версии cmake
cmake_minimum_required(VERSION 3.21)

# настройка описания проекта: название, версия, язык разработки
project(ffmpegUI VERSION 0.1.0 LANGUAGES CXX)

# настройка c++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# поиск qt
find_package(Qt6 6.9.3 EXACT REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2 LinguistTools)

# настройка qt и target
qt_standard_project_setup(REQUIRES 6.9)
qt_add_executable(ffmpegui src/main.cpp)

# свойства платформы
set_target_properties(ffmpegui PROPERTIES
    WIN32_EXECUTABLE TRUE)

# подключение библиотек
target_link_libraries(ffmpegui PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
target_include_directories(ffmpegui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

# qml и ресурсы
set_source_files_properties(qml/Main.qml PROPERTIES
    QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/ConversionForm.qml PROPERTIES
    QT_RESOURCE_ALIAS ConversionForm.qml)
set_source_files_properties(qml/QueuePanel.qml PROPERTIES
    QT_RESOURCE_ALIAS QueuePanel.qml)
set_source_files_properties(qml/MatrixPanel.qml PROPERTIES
    QT_RESOURCE_ALIAS MatrixPanel.qml)
set_source_files_properties(qml/InspectorPanel.qml PROPERTIES
    QT_RESOURCE_ALIAS InspectorPanel.qml)
set_source_files_properties(qml/CommandPreviewWindow.qml PROPERTIES
    QT_RESOURCE_ALIAS CommandPreviewWindow.qml)

qt_add_qml_module(ffmpegui
    URI FFmpegUI
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/ConversionForm.qml
        qml/QueuePanel.qml
        qml/InspectorPanel.qml
        qml/CommandPreviewWindow.qml
        qml/MatrixPanel.qml
    SOURCES
        src/mediaprobe.h
        src/conversioncommand.h
        src/conversionrunner.h
        src/conversionjob.h
        src/conversionqueue.h
        src/mediaworkspace.h
        src/mediaprobe.cpp
        src/conversioncommand.cpp
        src/conversionrunner.cpp
        src/conversionjob.cpp
        src/conversionqueue.cpp
        src/mediaworkspace.cpp
)

# перевод
qt_add_translations(
    TARGETS ffmpegui
    SOURCE_TARGETS ffmpegui
    TS_FILES translations/ffmpegui_ru_RU.ts
    NO_GENERATE_PLURALS_TS_FILE
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET update_translations
    LRELEASE_TARGET release_translations
)
