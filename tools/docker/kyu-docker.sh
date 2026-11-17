#!/bin/bash
# Real updates in throwaway Arch or Manjaro containers, driven like a person
# at a terminal (docs/TESTING.md, "In a container").
#
#   tools/docker/kyu-docker.sh image arch [YYYY/MM/DD]  an Arch system as of that day
#                                                      (Arch Linux Archive), Kyu built in it
#   tools/docker/kyu-docker.sh image manjaro            Manjaro's official image as it is,
#                                                      Kyu built in it
#   tools/docker/kyu-docker.sh rebuild IMAGE            Kyu again from this checkout, in IMAGE
#   tools/docker/kyu-docker.sh scenario IMAGE NAME [OUTDIR]  one case, tools/docker/scenarios/NAME.sh
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
# The proxy's CA joins the container's trust store (pacman's hooks rewrite
# the store, so it is added, not mounted over it).
trust='true'
if [ -n "${KYU_DOCKER_CA:-}" ]; then
    common+=(-v "$KYU_DOCKER_CA:/run/kyu-proxy-ca.pem:ro")
    trust='cp /run/kyu-proxy-ca.pem /etc/ca-certificates/trust-source/anchors/kyu-proxy-ca.pem && update-ca-trust'
fi

# Build Kyu from this checkout (without build trees) as the user "tester",
# whose sudo asks for the password "kyu".
build_steps='
set -e
useradd -m tester
echo tester:kyu | chpasswd
echo "tester ALL=(ALL) ALL" > /etc/sudoers.d/tester
echo "tester ALL=(ALL) NOPASSWD: /usr/bin/pkill" > /etc/sudoers.d/tester-kill
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
            $trust
            printf 'Server = https://archive.archlinux.org/repos/$date/\$repo/os/\$arch\n' > /etc/pacman.d/mirrorlist
            pacman -Syyuu --noconfirm
            pacman -S --needed --noconfirm $deps
            $build_steps"
    else
        tag=kyu-manjaro:latest
        # The image's packages stay as they are (it is months old), but for
        # what building Kyu needs: a partial upgrade, in a throwaway system.
        docker run --name "$name" "${common[@]}" -v "$repo:/src:ro" manjarolinux/base:latest bash -c "
            $trust
            printf 'Server = $MANJARO_NOW\n' > /etc/pacman.d/mirrorlist
            pacman -Sy --noconfirm
            pacman -S --needed --noconfirm $deps
            $build_steps"
    fi
    docker commit "$name" "$tag" >/dev/null
    docker rm "$name" >/dev/null
    echo "$tag"
    ;;
scenario)
    # One case from tools/docker/scenarios/NAME.sh: setup() as root, then
    # drive() as "tester" (lib.sh), then after() as root if defined. A line
    # "# docker: FLAGS" adds flags to docker run.
    image=${2:?image}
    case_=${3:?scenario}
    file=$here/scenarios/$case_.sh
    [ -f "$file" ] || { echo "no scenario $file" >&2; exit 2; }
    out=${4:-$PWD/kyu-docker-$case_}
    rm -rf "$out"; mkdir -p "$out" && chmod 777 "$out"
    read -r -a extra <<< "$(sed -n 's/^# docker: //p' "$file")"
    case "$image" in kyu-manjaro*) now=$MANJARO_NOW ;; *) now=$ARCH_NOW ;; esac
    docker run --rm "${common[@]}" "${extra[@]}" -v "$out:/out" -v "$here/drive.py:/drive.py:ro" \
        -v "$here/lib.sh:/lib.sh:ro" -v "$file:/scenario.sh:ro" "$image" bash -c "
        $trust
        echo 'tester ALL=(ALL) NOPASSWD: /usr/bin/pkill' > /etc/sudoers.d/tester-kill
        printf 'Server = $now\n' > /etc/pacman.d/mirrorlist
        source /scenario.sh
        setup > /out/setup.log 2>&1 || echo setup failed: \$? >> /out/setup.log
        su tester -c 'export HOME=/home/tester USER=tester LOGNAME=tester; cd; source /lib.sh; source /scenario.sh; drive' > /out/drive.log 2>&1
        if declare -F after >/dev/null; then after > /out/after.log 2>&1; fi
        tail -n 300 /var/log/pacman.log > /out/pacman.log
        ls -la /var/lib/pacman/db.lck > /out/lock.txt 2>&1 || true"
    echo "$out"
    ;;
rebuild)
    # Kyu again from this checkout, into an image made by "image".
    image=${2:?image}
    name=kyu-build-$$
    docker run --name "$name" "${common[@]}" -v "$repo:/src:ro" "$image" bash -c "
        set -e
        $trust
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
        $trust
        printf 'Server = $now\n' > /etc/pacman.d/mirrorlist
        cp /etc/pacman.d/mirrorlist /out/mirrorlist
        su tester -c '
            export HOME=/home/tester USER=tester LOGNAME=tester; cd

            export PATH=/opt/kyu-build/bin:\$PATH
            kyu-check --json > /out/check-before.json 2>/out/check-before.err; echo check: \$?
            python /drive.py /out/update kyu -- kyu-update; echo update: \$?
            cp ~/.local/state/kyu/last-run.json /out/ 2>/dev/null
            # After the check kyu-update started itself.
            for i in \$(seq 1 120); do
                kyu-check --json > /out/check-after.json 2>/out/check-after.err && break
                grep -q \"already running\" /out/check-after.err || break
                sleep 5
            done
            QT_QPA_PLATFORM=offscreen kyu --screenshot /out/card-after.png 2>/dev/null
            QT_QPA_PLATFORM=offscreen kyu --details --screenshot /out/card-after-details.png 2>/dev/null
        '
        pacman -Q > /out/packages-after.txt
        tail -n 400 /var/log/pacman.log > /out/pacman.log"
    echo "$out"
    ;;
*)
    sed -n '2,18p' "$0"
    exit 2
    ;;
esac
