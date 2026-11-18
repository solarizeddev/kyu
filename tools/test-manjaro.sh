#!/bin/bash
# After merging an upstream release: build this checkout in a fresh Manjaro
# stable container, install it, check the installed files and the app list,
# and start it under Xvfb (it must still be running after 15 s).
# Needs docker. Usage: tools/test-manjaro.sh [OUTDIR]
# Behind a TLS-inspecting proxy: TEST_CA=/path/to/ca.pem (added to the
# container's trust store); TEST_MIRROR=URL overrides Manjaro's mirrorlist.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$(mktemp -d)}
mkdir -p "$out" && chmod 777 "$out"
ca=()
[ -n "${TEST_CA:-}" ] && ca=(-v "$TEST_CA:/run/test-ca.pem:ro")
docker run --rm --network host -e HTTPS_PROXY -e HTTP_PROXY -e https_proxy -e http_proxy -e TEST_MIRROR \
    "${ca[@]}" -v "$root:/src:ro" -v "$out:/out" manjarolinux/base:latest bash -c '
set -e
trap "echo failed at line \$LINENO, logs in the output directory" ERR
[ -e /run/test-ca.pem ] && { cp /run/test-ca.pem /etc/ca-certificates/trust-source/anchors/test-ca.crt; update-ca-trust; }
[ -n "$TEST_MIRROR" ] && printf "Server = %s\n" "$TEST_MIRROR" >/etc/pacman.d/mirrorlist
pacman -Syu --noconfirm --needed base-devel cmake ninja rust git qt6-base qt6-tools polkit-qt6 \
    xorg-server-xvfb imagemagick ttf-dejavu >/out/deps.log 2>&1
useradd -m tester
cd /src
cmake -C cmake/Manjaro.cmake -S /src -B /tmp/pi-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr >/out/cmake.log 2>&1
ninja -C /tmp/pi-build >/out/build.log 2>&1
ninja -C /tmp/pi-build install >/out/install.log 2>&1
for f in /usr/bin/packageinstaller-manjaro /usr/share/applications/packageinstaller-manjaro.desktop \
         /usr/share/packageinstaller-manjaro/pkglist.yaml /usr/share/doc/packageinstaller-manjaro/help.html; do
    [ -e "$f" ] || { echo "missing: $f"; exit 1; }
done
! grep -rqi cachyos /usr/share/applications/packageinstaller-manjaro.desktop || { echo "CachyOS in the desktop file"; exit 1; }
/src/manjaro/check-pkglist.sh
su tester -c "cd; Xvfb :99 -screen 0 1280x860x24 >/dev/null 2>&1 & sleep 2
  DISPLAY=:99 packageinstaller-manjaro >/out/run.log 2>&1 & pid=\$!; sleep 15
  DISPLAY=:99 import -window root /out/start.png
  kill -0 \$pid"
echo "all checks passed"
'
echo "screenshot: $out/start.png"
