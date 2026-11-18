// Distribution branding, set at build time (CMake cache variables PI_*, see
// cmake/Manjaro.cmake); without them, upstream's own values. Keeping these
// in one place lets a distribution build this installer under its own name
// with a configuration file instead of patches.
#pragma once

#ifndef PI_PRODUCT_NAME
#define PI_PRODUCT_NAME "CachyOS Package Installer"
#endif
#ifndef PI_DISTRO_NAME
#define PI_DISTRO_NAME "CachyOS"
#endif
#ifndef PI_HOME_URL
#define PI_HOME_URL "http://cachyos.org"
#endif
#ifndef PI_COPYRIGHT
#define PI_COPYRIGHT "Copyright (c) CachyOS"
#endif
#ifndef PI_APP_ID
#define PI_APP_ID "cachyos-pi"
#endif
#ifndef PI_ORG_NAME
#define PI_ORG_NAME "cachyos"
#endif
#ifndef PI_ORG_DOMAIN
#define PI_ORG_DOMAIN "cachyos.org"
#endif
// A theme icon's name; empty: the icon built into the program.
#ifndef PI_ICON_NAME
#define PI_ICON_NAME ""
#endif
#ifndef PI_DOC_DIR
#define PI_DOC_DIR "/usr/share/doc/cachyos-packageinstaller"
#endif
#ifndef PI_HELP_FILE
#define PI_HELP_FILE "cachyos-pi.html"
#endif
// Where the popular applications list is downloaded from at start; empty:
// only the installed file (PI_PKGLIST_FILE).
#ifndef PI_PKGLIST_URL
#define PI_PKGLIST_URL "https://raw.githubusercontent.com/cachyos/packageinstaller/develop/pkglist.yaml"
#endif
#ifndef PI_PKGLIST_FILE
#define PI_PKGLIST_FILE "/usr/lib/cachyos-pi/pkglist.yaml"
#endif
// The distribution's update announcements, offered before "Upgrade All";
// empty: none.
#ifndef PI_UPDATE_NOTES_URL
#define PI_UPDATE_NOTES_URL ""
#endif
