# Manjaro's build of the package installer: cmake -C cmake/Manjaro.cmake ...
# Upstream's code, Manjaro's name, app list, help and announcements.
set(PI_PRODUCT_NAME "Package Installer" CACHE STRING "")
set(PI_DISTRO_NAME "Manjaro" CACHE STRING "")
set(PI_HOME_URL "" CACHE STRING "")
set(PI_COPYRIGHT "Free software under the GNU GPL v3" CACHE STRING "")
set(PI_APP_ID "packageinstaller-manjaro" CACHE STRING "")
set(PI_BINARY_NAME "packageinstaller-manjaro" CACHE STRING "")
set(PI_ORG_NAME "packageinstaller-manjaro" CACHE STRING "")
set(PI_ORG_DOMAIN "" CACHE STRING "")
set(PI_ICON_NAME "system-software-install" CACHE STRING "")
set(PI_DOC_DIR "/usr/share/doc/packageinstaller-manjaro" CACHE STRING "")
set(PI_HELP_FILE "help.html" CACHE STRING "")
set(PI_HELP_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../manjaro/help" CACHE PATH "")
# The list ships with the program: never downloaded (its names reach a
# shell command).
set(PI_PKGLIST_URL "" CACHE STRING "")
set(PI_PKGLIST_FILE "/usr/share/packageinstaller-manjaro/pkglist.yaml" CACHE STRING "")
set(PI_PKGLIST_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../manjaro/pkglist.yaml" CACHE FILEPATH "")
set(PI_DESKTOP_FILE "${CMAKE_CURRENT_LIST_DIR}/../manjaro/packageinstaller-manjaro.desktop" CACHE FILEPATH "")
set(PI_ICON_FILE "" CACHE FILEPATH "")
set(PI_POLICY_FILE "" CACHE FILEPATH "")
set(PI_UPDATE_NOTES_URL "https://forum.manjaro.org/c/announcements/stable-updates/12" CACHE STRING "")
