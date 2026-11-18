cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

##
## PROJECT
## name and version
##
project(cachyos-pi
        VERSION 1.6.2
        LANGUAGES CXX)


##
## INCLUDE
##
include(GNUInstallDirs)
include(StandardProjectSettings)
include(CompilerWarnings)
include(EnableCcache)
include(Linker)
include(StaticAnalyzers)
include(Sanitizers)
include(CPM)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Qt6 COMPONENTS Widgets LinguistTools REQUIRED)

pkg_check_modules(
  LIBALPM
  REQUIRED
  IMPORTED_TARGET
  libalpm>=13.0.0)
CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 12.1.0
  EXCLUDE_FROM_ALL YES
)
CPMAddPackage(
  NAME spdlog
  GITHUB_REPOSITORY gabime/spdlog
  GIT_TAG v1.17.0
  EXCLUDE_FROM_ALL YES
)
CPMAddPackage(
  NAME cpr
  GITHUB_REPOSITORY libcpr/cpr
  GIT_TAG 1.12.0
  EXCLUDE_FROM_ALL YES
)
CPMAddPackage(
  NAME rapidyaml
  GITHUB_REPOSITORY biojppm/rapidyaml
  GIT_TAG v0.9.0
  EXCLUDE_FROM_ALL YES
)
CPMAddPackage(
  NAME Corrosion
  GITHUB_REPOSITORY corrosion-rs/corrosion
  GIT_TAG v0.5.2
  EXCLUDE_FROM_ALL YES
)

find_package(PolkitQt6-1 REQUIRED)

##
## CONFIGURATION
##
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=thin -fwhole-program-vtables")
   set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=thin -fwhole-program-vtables")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -fwhole-program -fuse-linker-plugin")
endif()

# Link this 'library' to set the c++ standard / compile-time options requested
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_23)

# Files with translation sources. You should run lupdate manually when needed to regenerate them
file(GLOB TS_FILES "${PROJECT_SOURCE_DIR}/lang/*.ts")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lang")

set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/lang") # set qm files output directory
qt_add_translation(QM_FILES ${TS_FILES})   # generate qm files

add_custom_command(
    DEPENDS "${QM_FILES}"
    OUTPUT "${CMAKE_BINARY_DIR}/cachyospi_locale.qrc"
    COMMAND cp "${CMAKE_SOURCE_DIR}/cachyospi_locale.qrc" "${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

##
## Target
##
qt_add_executable(${PROJECT_NAME}
    ${RESOURCES}
    images.qrc "${CMAKE_BINARY_DIR}/cachyospi_locale.qrc"
    src/ini.hpp
    src/utils.hpp src/utils.cpp
    src/logging_utils.hpp src/logging_utils.cpp
    src/versionnumber.hpp
    src/alpm_manager.hpp src/alpm_manager.cpp
    src/pacmancache.hpp src/pacmancache.cpp
    src/remotes.hpp src/remotes.cpp
    src/about.hpp src/about.cpp
    src/cmd.hpp src/cmd.cpp
    src/mainwindow.hpp src/mainwindow.cpp
    src/mainwindow.ui
    src/main.cpp)

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

# Add linker configuration
configure_linker(project_options)

# sanitizer options if supported by compiler
enable_sanitizers(project_options)

corrosion_import_crate(MANIFEST_PATH "backend-rustlib/Cargo.toml" FLAGS "${CARGO_FLAGS}")
corrosion_add_cxxbridge(backend-rustlib-cxxbridge CRATE backend_rustlib FILES lib.rs)

include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})

# Branding (src/branding.hpp): upstream's values unless a distribution's
# configuration sets them (cmake -C cmake/Manjaro.cmake).
set(PI_PRODUCT_NAME "CachyOS Package Installer" CACHE STRING "Name shown in the window and dialogs")
set(PI_DISTRO_NAME "CachyOS" CACHE STRING "The distribution's name")
set(PI_HOME_URL "http://cachyos.org" CACHE STRING "Link in About (empty: none)")
set(PI_COPYRIGHT "Copyright (c) CachyOS" CACHE STRING "Line in About")
set(PI_APP_ID "cachyos-pi" CACHE STRING "Application and desktop file id")
set(PI_BINARY_NAME "cachyos-pi" CACHE STRING "Installed program name")
set(PI_ORG_NAME "cachyos" CACHE STRING "Settings organisation")
set(PI_ORG_DOMAIN "cachyos.org" CACHE STRING "Settings domain")
set(PI_ICON_NAME "" CACHE STRING "Theme icon (empty: the built-in one)")
set(PI_DOC_DIR "${CMAKE_INSTALL_PREFIX}/share/doc/cachyos-packageinstaller" CACHE STRING "Help and licence pages")
set(PI_HELP_FILE "cachyos-pi.html" CACHE STRING "Help page in PI_DOC_DIR")
set(PI_HELP_SOURCE "${PROJECT_SOURCE_DIR}/help" CACHE PATH "Help pages to install")
set(PI_PKGLIST_URL "https://raw.githubusercontent.com/cachyos/packageinstaller/develop/pkglist.yaml" CACHE STRING "Popular applications list downloaded at start (empty: none)")
set(PI_PKGLIST_FILE "${CMAKE_INSTALL_PREFIX}/lib/cachyos-pi/pkglist.yaml" CACHE STRING "Installed popular applications list")
set(PI_PKGLIST_SOURCE "${PROJECT_SOURCE_DIR}/pkglist.yaml" CACHE FILEPATH "Popular applications list to install")
set(PI_DESKTOP_FILE "${PROJECT_SOURCE_DIR}/cachyos-pi.desktop" CACHE FILEPATH "Desktop file to install")
set(PI_ICON_FILE "${PROJECT_SOURCE_DIR}/icons/cachyos-pi.png" CACHE FILEPATH "Icon to install (empty: none)")
set(PI_POLICY_FILE "${PROJECT_SOURCE_DIR}/org.cachyos.cachyos-pi.pkexec.policy" CACHE FILEPATH "Polkit policy to install (empty: none)")
set(PI_UPDATE_NOTES_URL "" CACHE STRING "Update announcements offered before Upgrade All (empty: none)")
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PI_PRODUCT_NAME="${PI_PRODUCT_NAME}" PI_DISTRO_NAME="${PI_DISTRO_NAME}" PI_HOME_URL="${PI_HOME_URL}"
    PI_COPYRIGHT="${PI_COPYRIGHT}" PI_APP_ID="${PI_APP_ID}" PI_ORG_NAME="${PI_ORG_NAME}" PI_ORG_DOMAIN="${PI_ORG_DOMAIN}"
    PI_ICON_NAME="${PI_ICON_NAME}" PI_DOC_DIR="${PI_DOC_DIR}" PI_HELP_FILE="${PI_HELP_FILE}"
    PI_PKGLIST_URL="${PI_PKGLIST_URL}" PI_PKGLIST_FILE="${PI_PKGLIST_FILE}" PI_UPDATE_NOTES_URL="${PI_UPDATE_NOTES_URL}")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PI_BINARY_NAME}")

target_link_libraries(${PROJECT_NAME} PRIVATE project_warnings project_options Qt6::Widgets Threads::Threads spdlog::spdlog fmt::fmt ryml::ryml cpr::cpr backend-rustlib-cxxbridge PkgConfig::LIBALPM)

option(ENABLE_UNITY "Enable Unity builds of projects" OFF)
if(ENABLE_UNITY)
   # Add for any project you want to apply unity builds for
   set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)
endif()

install(
   TARGETS ${PROJECT_NAME}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(PI_POLICY_FILE)
install(
   FILES "${PI_POLICY_FILE}"
   DESTINATION "${POLKITQT-1_POLICY_FILES_INSTALL_DIR}"
)
endif()

get_filename_component(PI_PKGLIST_DIR "${PI_PKGLIST_FILE}" DIRECTORY)
get_filename_component(PI_PKGLIST_NAME "${PI_PKGLIST_FILE}" NAME)
install(
   FILES "${PI_PKGLIST_SOURCE}"
   DESTINATION "${PI_PKGLIST_DIR}"
   RENAME "${PI_PKGLIST_NAME}"
)
file(GLOB HELP_FILES "${PI_HELP_SOURCE}/*")
install(
   FILES ${HELP_FILES}
   DESTINATION "${PI_DOC_DIR}"
)

install(
   FILES "${PI_DESKTOP_FILE}"
   DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
   RENAME "${PI_APP_ID}.desktop"
)
if(PI_ICON_FILE)
install(
   FILES "${PI_ICON_FILE}"
   DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
endif()
