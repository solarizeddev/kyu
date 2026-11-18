// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/// What the root helper and the GUI have to agree on.
namespace helper {

// Manjaro fork: the build can name another program and its arguments (argv[0] first).
#ifndef PI_RATE_MIRRORS
#define PI_RATE_MIRRORS "/usr/bin/cachyos-rate-mirrors"
#define PI_RATE_MIRRORS_ARGV "cachyos-rate-mirrors"
#endif

/// The only program besides pacman that the helper starts.
inline constexpr char kRateMirrors[] = PI_RATE_MIRRORS;

/// Printed before the helper hands over to pacman, followed by the command. Output
/// without it means pkexec never started the helper.
inline constexpr char kStarted[] = "cachyos-pi-helper: running ";

}  // namespace helper
