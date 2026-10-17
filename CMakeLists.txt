cmake_minimum_required(VERSION 3.21)

project(JAV-Qt
    VERSION 0.1.0
    DESCRIPTION "Organize a local JAV collection"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Windows packages are a flat folder: the executable, its DLLs and fonts/
if(WIN32)
    set(CMAKE_INSTALL_BINDIR "." CACHE STRING "")
endif()
include(GNUInstallDirs)

find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets Network Concurrent)
qt_standard_project_setup()

#
# Dependencies: system packages when found, else built from pinned sources.
# Release builds set JAVQT_BUNDLED_DEPS so every platform links the same.
#
option(JAVQT_BUNDLED_DEPS "Build spdlog and gumbo from source instead of using system packages" OFF)
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)  # downloaded files get the extraction time, so changes rebuild
endif()

if(NOT JAVQT_BUNDLED_DEPS)
    find_package(spdlog CONFIG QUIET)
endif()
if(NOT spdlog_FOUND)
    message(STATUS "Building spdlog from source")
    FetchContent_Declare(spdlog
        URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
        URL_HASH SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
endif()

if(NOT JAVQT_BUNDLED_DEPS)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GUMBO QUIET IMPORTED_TARGET gumbo)
    endif()
endif()
if(GUMBO_FOUND)
    set(GUMBO_TARGET PkgConfig::GUMBO)
else()
    message(STATUS "Building gumbo from source")
    FetchContent_Declare(gumbo
        URL https://codeberg.org/gumbo-parser/gumbo-parser/archive/0.13.2.tar.gz
        URL_HASH SHA256=90bea83283760339da194fb90112a532854c13cd1eabdabc7ef7a4dede1dbc9d)
    FetchContent_MakeAvailable(gumbo)  # no CMake project of its own: sources only
    add_library(gumbo STATIC
        ${gumbo_SOURCE_DIR}/src/attribute.c
        ${gumbo_SOURCE_DIR}/src/char_ref.c
        ${gumbo_SOURCE_DIR}/src/error.c
        ${gumbo_SOURCE_DIR}/src/parser.c
        ${gumbo_SOURCE_DIR}/src/string_buffer.c
        ${gumbo_SOURCE_DIR}/src/string_piece.c
        ${gumbo_SOURCE_DIR}/src/tag.c
        ${gumbo_SOURCE_DIR}/src/tokenizer.c
        ${gumbo_SOURCE_DIR}/src/utf8.c
        ${gumbo_SOURCE_DIR}/src/util.c
        ${gumbo_SOURCE_DIR}/src/vector.c)
    set_target_properties(gumbo PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
    target_include_directories(gumbo SYSTEM PUBLIC ${gumbo_SOURCE_DIR}/src)
    if(MSVC)
        target_include_directories(gumbo PRIVATE ${gumbo_SOURCE_DIR}/visualc/include)  # strings.h
        target_compile_options(gumbo PRIVATE /w)
    else()
        target_compile_options(gumbo PRIVATE -w)
    endif()
    set(GUMBO_TARGET gumbo)
endif()

#
# The application
#
qt_add_executable(jav-qt WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/dbm.cpp src/dbm.hpp
    src/library.cpp src/library.hpp
    src/log.hpp
    src/models/video.hpp
    src/network.cpp src/network.hpp
    src/options.cpp src/options.hpp
    src/scraper.cpp src/scraper.hpp
    src/thumbnails.cpp src/thumbnails.hpp
    src/providers/dmm.cpp src/providers/dmm.hpp
    src/providers/javlib.cpp src/providers/javlib.hpp
    src/gui/display.hpp
    src/gui/imagecache.cpp src/gui/imagecache.hpp
    src/gui/librarypage.cpp src/gui/librarypage.hpp
    src/gui/mainwindow.cpp src/gui/mainwindow.hpp
    src/gui/style.cpp src/gui/style.hpp
    src/gui/videolist.cpp src/gui/videolist.hpp
    src/gui/videopage.cpp src/gui/videopage.hpp)

target_include_directories(jav-qt PRIVATE src)
target_link_libraries(jav-qt PRIVATE Qt6::Widgets Qt6::Network Qt6::Concurrent spdlog::spdlog ${GUMBO_TARGET})
target_compile_definitions(jav-qt PRIVATE
    JAVQT_VERSION="${PROJECT_VERSION}"
    JAVQT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")  # fonts when run from the build tree, see paths::fontsDir()
if(MSVC)
    target_compile_options(jav-qt PRIVATE /utf-8)  # the sources are UTF-8
endif()

qt_add_resources(jav-qt icons
    PREFIX /icons
    BASE packaging/icons
    FILES packaging/icons/jav-qt.png)

if(APPLE OR WIN32)
    set_target_properties(jav-qt PROPERTIES OUTPUT_NAME JAV-Qt)
endif()

#
# Fonts: Inter (static 18pt cut) and the Noto Sans CJK JP weights the GUI uses
#
set(FONTS_DIR ${CMAKE_SOURCE_DIR}/assets/fonts)
file(GLOB INTER_FONTS ${FONTS_DIR}/inter/static/Inter_18pt-*.ttf)
set(INTER_FILES ${INTER_FONTS} ${FONTS_DIR}/inter/OFL.txt)
set(NOTO_FILES
    ${FONTS_DIR}/noto-sans-cjk-jp/NotoSansCJKjp-Regular.otf
    ${FONTS_DIR}/noto-sans-cjk-jp/NotoSansCJKjp-Bold.otf
    ${FONTS_DIR}/noto-sans-cjk-jp/LICENSE)

if(APPLE)
    set(ICNS ${CMAKE_SOURCE_DIR}/packaging/icons/jav-qt.icns)
    target_sources(jav-qt PRIVATE ${ICNS} ${INTER_FILES} ${NOTO_FILES})
    set_source_files_properties(${ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(${INTER_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/fonts/inter/static)
    set_source_files_properties(${FONTS_DIR}/inter/OFL.txt PROPERTIES MACOSX_PACKAGE_LOCATION Resources/fonts/inter)
    set_source_files_properties(${NOTO_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/fonts/noto-sans-cjk-jp)
    set_target_properties(jav-qt PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME JAV-Qt
        MACOSX_BUNDLE_GUI_IDENTIFIER org.jav-qt.JAV-Qt
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE jav-qt.icns)
else()
    if(WIN32)
        set(FONTS_DESTINATION fonts)
        set(RC_FILE ${CMAKE_BINARY_DIR}/jav-qt.rc)
        file(WRITE ${RC_FILE} "IDI_ICON1 ICON \"${CMAKE_SOURCE_DIR}/packaging/icons/jav-qt.ico\"\n")
        target_sources(jav-qt PRIVATE ${RC_FILE})
    else()
        set(FONTS_DESTINATION ${CMAKE_INSTALL_DATADIR}/jav-qt/fonts)
    endif()
    install(FILES ${INTER_FONTS} DESTINATION ${FONTS_DESTINATION}/inter/static)
    install(FILES ${FONTS_DIR}/inter/OFL.txt DESTINATION ${FONTS_DESTINATION}/inter)
    install(FILES ${NOTO_FILES} DESTINATION ${FONTS_DESTINATION}/noto-sans-cjk-jp)
endif()

#
# Installing: the app, plus on Linux its desktop entry and icon. On macOS and
# Windows, `cmake --install` also copies Qt in (macdeployqt, windeployqt).
#
install(TARGETS jav-qt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/jav-qt.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/icons/jav-qt-256.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME jav-qt.png)
endif()

if(APPLE OR WIN32)
    qt_generate_deploy_app_script(TARGET jav-qt OUTPUT_SCRIPT DEPLOY_SCRIPT NO_TRANSLATIONS)
    install(SCRIPT ${DEPLOY_SCRIPT})
endif()

if(WIN32)
    # The Microsoft C++ runtime DLLs, so users need not install it themselves
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    include(InstallRequiredSystemLibraries)
endif()
