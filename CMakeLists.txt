cmake_minimum_required(VERSION 3.21)
project(Clipline VERSION 1.0.1 LANGUAGES CXX)
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(CLIPLINE_SOURCES
    src/main.cpp
    src/config.h src/config.cpp
    src/i18n.h src/i18n.cpp
    src/look.h src/look.cpp
    src/platform.h src/platform_common.cpp
    src/recorder.h src/recorder.cpp
    src/setup.h src/setup.cpp
    src/gallery.h src/gallery.cpp
    src/controller.h src/controller.cpp
    assets/photos.qrc
)

if(WIN32)
    add_executable(clipline WIN32 ${CLIPLINE_SOURCES} src/platform_win.cpp assets/app.rc)
    set_target_properties(clipline PROPERTIES OUTPUT_NAME Clipline)
    target_link_libraries(clipline PRIVATE ole32 winmm dxgi user32 advapi32)
elseif(APPLE)
    set(CLIPLINE_ICNS ${CMAKE_SOURCE_DIR}/assets/icon.icns)
    set_source_files_properties(${CLIPLINE_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(clipline MACOSX_BUNDLE ${CLIPLINE_SOURCES} src/platform_mac.cpp ${CLIPLINE_ICNS})
    set_target_properties(clipline PROPERTIES
        OUTPUT_NAME Clipline
        MACOSX_BUNDLE_BUNDLE_NAME Clipline
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.clipline
        MACOSX_BUNDLE_ICON_FILE icon.icns
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/assets/Info.plist.in)
    target_link_libraries(clipline PRIVATE "-framework Carbon")
else()
    add_executable(clipline ${CLIPLINE_SOURCES} src/platform_linux.cpp)
    find_package(X11)
    if(X11_FOUND)
        target_compile_definitions(clipline PRIVATE HAVE_X11)
        target_link_libraries(clipline PRIVATE X11::X11)
    endif()
endif()

target_link_libraries(clipline PRIVATE Qt6::Widgets Qt6::Network)
if(MSVC)
    target_compile_options(clipline PRIVATE /utf-8 /W3 /Zc:__cplusplus)
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS clipline RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES assets/clipline.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/icon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME clipline.png)
endif()
