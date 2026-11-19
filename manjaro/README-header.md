# packageinstaller-manjaro

> Community project, not affiliated with or endorsed by Manjaro GmbH & Co. KG
> or CachyOS. The Manjaro name and logo are trademarks of Manjaro GmbH & Co. KG.

An app store for [Manjaro](https://manjaro.org): repository packages, AUR,
Flatpak and AppImages in one window. It is a port of
[CachyOS's package installer](https://github.com/CachyOS/packageinstaller) 2.0.

![Package Installer for Manjaro](manjaro/screenshots/discover.png)

Install: clone, then `cd packaging && makepkg -sif`.

Merge a new CachyOS release: `tools/update-upstream.sh` merges it, runs the
Manjaro checks, then builds and installs. Nothing is pushed.

- [MANJARO.md](MANJARO.md): what differs from CachyOS's version.
- [update-notifier-manjaro](https://github.com/solarizeddev/update-notifier-manjaro): the tray icon and update notifications.

---

*Below is CachyOS's README. For CachyOS read Manjaro, and for `cachyos-pi`
read `packageinstaller-manjaro`. Skip its Dependencies, Building and Packaging
sections: they build the CachyOS version. Use the install line above, or
[MANJARO.md](MANJARO.md).*

