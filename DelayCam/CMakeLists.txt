# Minimum required CMake version
cmake_minimum_required(VERSION 3.16)

# Project, version and description
project(DelayCam
    VERSION 0.1
    LANGUAGES CXX)

# Set C++ and Qt related options
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/DelayCam)

# Disable Qt keywords
# This is needed since libcamera uses "signals", "slots", "emit", ...
# Use Q_SLOTS and Q_SIGNALS instead
set(QT_NO_KEYWORDS ON)

# Set preprocessor defines
add_compile_definitions(APP_NAME="${CMAKE_PROJECT_NAME}")
add_compile_definitions(APP_VERSION="${CMAKE_PROJECT_VERSION}")

# Find Qt - only Qt6 is supported due to compatibility with QCamera
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

# Find libcamera and dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA libcamera)
pkg_check_modules(LIBJPEG libjpeg)
pkg_check_modules(LIBEXIF libexif)

# Set project sources variable
set(PROJECT_SOURCES
    src/main.cpp
    src/application.h          src/application.cpp

    src/cam/viewfinder.h       src/cam/viewfinder.cpp
    src/cam/image.h            src/cam/image.cpp
    src/cam/shader/shaders.qrc

    src/util/logger.h          src/util/logger.cpp
    src/util/undefkeywords.h
)

# Add project sources to executable target
qt_add_executable(DelayCam MANUAL_FINALIZATION ${PROJECT_SOURCES})

# Include dir path starts in src and libcamera
target_include_directories(DelayCam PRIVATE ${CMAKE_SOURCE_DIR}/src/)
target_include_directories(DelayCam PRIVATE ${CMAKE_SOURCE_DIR}/libcamera)
target_include_directories(DelayCam PRIVATE ${LIBCAMERA_INCLUDE_DIRS}/)

# Add and link Qt, libcamera
target_link_libraries(DelayCam PRIVATE
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    camera
    camera-base
    jpeg
    exif)

# Install destinations
include(GNUInstallDirs)
install(TARGETS DelayCam
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Finalize target
qt_finalize_executable(DelayCam)
