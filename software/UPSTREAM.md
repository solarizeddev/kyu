# Upstream

This directory starts as CachyOS's package installer (cachyos-pi), itself a
fork of MX Linux's mx-packageinstaller, GPL-3.0:

- https://github.com/CachyOS/packageinstaller
- commit 47da60b ("fix: remove not found entries"), version 1.6.2

The first commit adding this directory is that source unmodified; Kyu's
changes (Manjaro branding and app list, safety patches, the update through
kyu-update) follow as separate commits. The original copyright notices stay.
