# Sourced by the Flatpak tests and tools/preview-panel.sh. Builds a local
# Flatpak repository with flatpak's own tools and a throwaway installation.
# Needs FLATPAK_USER_DIR and FLATPAK_SYSTEM_DIR set to scratch directories.
#
#   make_repo DIR [NAME] repo with a runtime and two apps at version 1,
#                        remote NAME (default testrepo) added, all installed
#   publish DIR ID N     publishes version N of app ID (or of the runtime
#                        if ID is org.kyu.Test.Runtime)

KYU_TEST_RUNTIME=org.kyu.Test.Runtime

publish() {
    dir=$1; id=$2; version=$3
    arch=$(flatpak --default-arch)
    build="$dir/build-$id-$version"
    if [ "$id" = "$KYU_TEST_RUNTIME" ]; then
        # build-init needs the runtime installed already, so lay it out by hand.
        mkdir -p "$build/usr"
        ln -sfn usr "$build/files"
        echo "$version" >"$build/usr/version"
        printf '[Runtime]\nname=%s\nruntime=%s/%s/stable\nsdk=%s/%s/stable\n' \
            "$id" "$KYU_TEST_RUNTIME" "$arch" "$KYU_TEST_RUNTIME" "$arch" >"$build/metadata"
        flatpak build-export --runtime "$dir/repo" "$build" stable >/dev/null
    else
        mkdir -p "$build/files/bin" "$build/export"
        printf '#!/bin/sh\necho %s\n' "$version" >"$build/files/bin/app"
        chmod +x "$build/files/bin/app"
        printf '[Application]\nname=%s\nruntime=%s/%s/stable\nsdk=%s/%s/stable\ncommand=app\n' \
            "$id" "$KYU_TEST_RUNTIME" "$arch" "$KYU_TEST_RUNTIME" "$arch" >"$build/metadata"
        flatpak build-export "$dir/repo" "$build" stable >/dev/null
    fi
}

make_repo() {
    dir=$1
    remote=${2:-testrepo}
    publish "$dir" "$KYU_TEST_RUNTIME" 1
    publish "$dir" org.kyu.Test.App 1
    publish "$dir" org.kyu.Test.Other 1
    flatpak --user remote-add --no-gpg-verify "$remote" "file://$dir/repo" >/dev/null
    flatpak --user install -y --noninteractive "$remote" "$KYU_TEST_RUNTIME" org.kyu.Test.App org.kyu.Test.Other >/dev/null 2>&1
}
