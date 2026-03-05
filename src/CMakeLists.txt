set(BIN_NAME dd-videowallpaper-plugin)
if(OPT_ENABLE_AUDIO_OUTPUT MATCHES ON)
    add_compile_definitions(ENABLE_AUDIO_OUTPUT)
endif()

set(QT_VERSION_MAJOR 6)
set(DTK_VERSION_MAJOR 6)

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core DBus OpenGLWidgets Widgets REQUIRED)
find_package(Dtk${DTK_VERSION_MAJOR} COMPONENTS Core Widget REQUIRED)
find_package(dfm${DTK_VERSION_MAJOR}-base REQUIRED)
find_package(dfm${DTK_VERSION_MAJOR}-framework REQUIRED)

#find_package(PkgConfig REQUIRED)
#pkg_check_modules(DFM${DTK_VERSION_MAJOR} REQUIRED
#    IMPORTED_TARGET
#    dfm${DTK_VERSION_MAJOR}-base
#    dfm${DTK_VERSION_MAJOR}-framework
#)

if (OPT_USE_LIBMPV MATCHES ON)
    add_compile_definitions(USE_LIBMPV)
    pkg_check_modules(Mpv REQUIRED IMPORTED_TARGET mpv)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS OpenGLWidgets REQUIRED)
    set(Media_INCLUDE_DIRS
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
        PkgConfig::Mpv
    )
    set(Media_LIBRARIES
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
        PkgConfig::Mpv
    )
else()
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Multimedia REQUIRED)
    set(Media_INCLUDE_DIRS
        Qt${QT_VERSION_MAJOR}::Multimedia
    )
    set(Media_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Multimedia
    )
endif()

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.json"
)

if (OPT_USE_LIBMPV MATCHES OFF)
    file(GLOB_RECURSE RM_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/*"
    )
endif()
list(REMOVE_ITEM SRC_FILES ${RM_SRC})


add_library(${BIN_NAME}
    SHARED
    ${SRC_FILES}
)

set_target_properties(${BIN_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(${BIN_NAME} PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Widgets
    ${Qt${QT_VERSION_MAJOR}Widgets_PRIVATE_INCLUDE_DIRS}
    Dtk${DTK_VERSION_MAJOR}::Core
    Dtk${DTK_VERSION_MAJOR}::Widget
    ${dfm${DTK_VERSION_MAJOR}-base_INCLUDE_DIRS}
    ${dfm${DTK_VERSION_MAJOR}-framework_INCLUDE_DIRS}
    # PkgConfig::DFM${DTK_VERSION_MAJOR}
    ${Media_INCLUDE_DIRS}
)

target_link_libraries(${BIN_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Widgets
    Dtk${DTK_VERSION_MAJOR}::Core
    Dtk${DTK_VERSION_MAJOR}::Widget
    ${dfm${DTK_VERSION_MAJOR}-base_LIBRARIES}
    ${dfm${DTK_VERSION_MAJOR}-framework_LIBRARIES}
    # PkgConfig::DFM${DTK_VERSION_MAJOR}
    ${Media_LIBRARIES}
)

# install library file
install(TARGETS ${BIN_NAME} LIBRARY DESTINATION ${DFM_PLUGIN_DESKTOP_EDGE_DIR})

dtk_add_config_meta_files(
    APPID "org.deepin.dde.file-manager"
    BASE "${CMAKE_SOURCE_DIR}/assets/configs"
    FILES "${CMAKE_SOURCE_DIR}/assets/configs/org.deepin.dde.file-manager.desktop.videowallpaper.json"
)

