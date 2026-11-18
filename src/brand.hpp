// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Manjaro fork: lets a distribution build show its own name and logo without
// touching the QML. Inactive unless the build sets PI_DISTRO_NAME (see
// cmake/Manjaro.cmake), so the upstream build behaves exactly as before.

class QQmlEngine;
class QTranslator;

namespace brand {

/// Whether the build carries a distribution's branding.
bool active();

/// Call once, after the translations are loaded and before anything reads
/// the settings:
/// - The texts go through a translator that puts the distribution's name in
///   place of CachyOS (and a few texts that say more than the name).
/// - The window icon, desktop file name, organisation and application name
///   follow the build's settings.
/// `loaded` is the translator main() loaded, which gets wrapped.
void apply(QTranslator* loaded);

/// Call before the QML is loaded: the app's pictures of the CachyOS logo are
/// replaced by the distribution's logo from the icon theme, at any size.
void applyToEngine(QQmlEngine& engine);

}  // namespace brand
