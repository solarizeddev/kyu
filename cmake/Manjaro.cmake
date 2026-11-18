# Manjaro fork: the Manjaro build. Use with  cmake -C cmake/Manjaro.cmake ...
# Every value here is read by CMakeLists.txt; see MANJARO.md.
set(PI_DISTRO_NAME "Manjaro" CACHE STRING "")
set(PI_APP_ID "packageinstaller-manjaro" CACHE STRING "")
# from manjaro-icons, the name /etc/os-release gives as LOGO
set(PI_LOGO_ICON "manjarolinux" CACHE STRING "")
# Manjaro builds all of core, extra and multilib itself
set(PI_OWN_REPO_PREFIX "" CACHE STRING "")
set(PI_RATE_MIRRORS "/usr/bin/pacman-mirrors" CACHE STRING "")
set(PI_RATE_MIRRORS_ARGV "pacman-mirrors;--fasttrack" CACHE STRING "")
set(PI_PKGLIST "${CMAKE_CURRENT_LIST_DIR}/../manjaro/pkglist.yaml" CACHE FILEPATH "")
set(PI_DESKTOP_FILE "${CMAKE_CURRENT_LIST_DIR}/../manjaro/packageinstaller-manjaro.desktop" CACHE FILEPATH "")
