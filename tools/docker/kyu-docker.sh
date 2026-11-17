#!/bin/bash
# Real updates in throwaway Arch or Manjaro containers, driven like a person
# at a terminal (docs/TESTING.md, "In a container").
#
#   tools/docker/kyu-docker.sh image arch [YYYY/MM/DD]  an Arch system as of that day
#                                                      (Arch Linux Archive), Kyu built in it
#   tools/docker/kyu-docker.sh image manjaro            Manjaro's official image as it is,
#                                                      Kyu built in it
#   tools/docker/kyu-docker.sh rebuild IMAGE            Kyu again from this checkout, in IMAGE
#   tools/docker/kyu-docker.sh run IMAGE [OUTDIR]       today's mirrors, then kyu-check and
#                                                      the real kyu-update as user "tester"
#
# The update is real: pacman downloads and installs, inside the container
# only. Nothing on this machine changes but Docker's images. Behind an
# HTTPS proxy, HTTPS_PROXY (and friends) pass through, and KYU_DOCKER_CA
# names a CA bundle to use in the container.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
ARCH_NOW='https://geo.mirror.pkgbuild.com/$repo/os/$arch'
MANJARO_NOW='https://mirrors.ocf.berkeley.edu/manjaro/stable/$repo/$arch'

common=(--network host -e TERM=xterm-256color)
for v in HTTPS_PROXY HTTP_PROXY https_proxy http_proxy NO_PROXY no_proxy; do
    [ -n "${!v:-}" ] && common+=(-e "$v")
done
[ -n "${KYU_DOCKER_CA:-}" ] && common+=(-v "$KYU_DOCKER_CA:/etc/ssl/certs/ca-certificates.crt:ro")

# Build Kyu from this checkout (without build trees) as the user "tester",
# whose sudo asks for the password "kyu".
build_steps='
set -e
useradd -m tester
echo tester:kyu | chpasswd
echo "tester ALL=(ALL) ALL" > /etc/sudoers.d/tester
# Behind a proxy, sudo keeps it (as a proxy user sets up sudo).
echo "Defaults env_keep += \"HTTPS_PROXY HTTP_PROXY https_proxy http_proxy NO_PROXY no_proxy\"" > /etc/sudoers.d/proxy
mkdir -p /opt/kyu-src
tar -C /src --exclude=./build --exclude="./build-*" --exclude=./.git -cf - . | tar -C /opt/kyu-src -xf -
cmake -S /opt/kyu-src -B /opt/kyu-build -G Ninja -DKYU_FLATPAK=OFF >/dev/null
cmake --build /opt/kyu-build --target kyu-update kyu-check kyu
'
deps='base-devel cmake ninja qt6-base pacman-contrib sudo python ttf-dejavu'

case "${1:-}" in
image)
    kind=${2:?arch or manjaro}
    name=kyu-build-$$
    if [ "$kind" = arch ]; then
        date=${3:-$(date -d '-6 weeks' +%Y/%m/%d)}
        tag=kyu-arch:${date//\//-}
        docker run --name "$name" "${common[@]}" -v "$repo:/src:ro" archlinux:latest bash -c "
            printf 'Server = https://archive.archlinux.org/repos/$date/\$repo/os/\$arch\n' > /etc/pacman.d/mirrorlist
            pacman -Syyuu --noconfirm
            pacman -S --needed --noconfirm $deps
            $build_steps"
    else
        tag=kyu-manjaro:latest
        # The image's packages stay as they are (it is months old), but for
        # what building Kyu needs: a partial upgrade, in a throwaway system.
        docker run --name "$name" "${common[@]}" -v "$repo:/src:ro" manjarolinux/base:latest bash -c "
            printf 'Server = $MANJARO_NOW\n' > /etc/pacman.d/mirrorlist
            pacman -Sy --noconfirm
            pacman -S --needed --noconfirm $deps
            $build_steps"
    fi
    docker commit "$name" "$tag" >/dev/null
    docker rm "$name" >/dev/null
    echo "$tag"
    ;;
rebuild)
    # Kyu again from this checkout, into an image made by "image".
    image=${2:?image}
    name=kyu-build-$$
    docker run --name "$name" "${common[@]}" -v "$repo:/src:ro" "$image" bash -c "
        set -e
        echo 'Defaults env_keep += \"HTTPS_PROXY HTTP_PROXY https_proxy http_proxy NO_PROXY no_proxy\"' > /etc/sudoers.d/proxy
        rm -rf /opt/kyu-src && mkdir -p /opt/kyu-src
        tar -C /src --exclude=./build --exclude='./build-*' --exclude=./.git -cf - . | tar -C /opt/kyu-src -xf -
        cmake --build /opt/kyu-build --target kyu-update kyu-check kyu"
    docker commit "$name" "$image" >/dev/null
    docker rm "$name" >/dev/null
    echo "$image"
    ;;
run)
    image=${2:?image}
    out=${3:-$PWD/kyu-docker-$(echo "$image" | tr ':/' '--')}
    mkdir -p "$out" && chmod 777 "$out"
    case "$image" in kyu-manjaro*) now=$MANJARO_NOW ;; *) now=$ARCH_NOW ;; esac
    docker run --rm "${common[@]}" -v "$out:/out" -v "$here/drive.py:/drive.py:ro" "$image" bash -c "
        printf 'Server = $now\n' > /etc/pacman.d/mirrorlist
        cp /etc/pacman.d/mirrorlist /out/mirrorlist
        su tester -c '
            export HOME=/home/tester USER=tester LOGNAME=tester; cd

            export PATH=/opt/kyu-build/bin:\$PATH
            kyu-check --json > /out/check-before.json 2>/out/check-before.err; echo check: \$?
            python /drive.py /out/update kyu -- kyu-update; echo update: \$?
            cp ~/.local/state/kyu/last-run.json /out/ 2>/dev/null
            kyu-check --json > /out/check-after.json 2>/out/check-after.err
            QT_QPA_PLATFORM=offscreen kyu --screenshot /out/card-after.png 2>/dev/null
            QT_QPA_PLATFORM=offscreen kyu --details --screenshot /out/card-after-details.png 2>/dev/null
        '
        pacman -Q > /out/packages-after.txt
        tail -n 400 /var/log/pacman.log > /out/pacman.log"
    echo "$out"
    ;;
*)
    sed -n '2,17p' "$0"
    exit 2
    ;;
esac
