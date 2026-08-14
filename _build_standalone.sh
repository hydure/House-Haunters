#!/usr/bin/env sh
set -eu

# Build a native standalone release on macOS or Linux. SFML is compiled from
# the pinned 2.6.2 source tag and resources are embedded in the executable.
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$repo_root"

cmake -S . -B build-standalone -DCMAKE_BUILD_TYPE=Release \
    -DHH_STATIC_SFML=ON \
    -DHH_FETCH_SFML=ON \
    -DHH_EMBED_RESOURCES=ON \
    -DBUILD_TESTING=OFF
cmake --build build-standalone --target HH --parallel

mkdir -p dist
arch=$(uname -m)
case "$(uname -s)" in
    Darwin)
        codesign --force --deep --sign - build-standalone/HH.app
        rm -f "dist/HouseHaunters-macOS-${arch}.zip"
        ditto -c -k --sequesterRsrc --keepParent build-standalone/HH.app \
            "dist/HouseHaunters-macOS-${arch}.zip"
        echo "Created dist/HouseHaunters-macOS-${arch}.zip"
        ;;
    Linux)
        cp build-standalone/HH "dist/HouseHaunters-Linux-${arch}"
        chmod +x "dist/HouseHaunters-Linux-${arch}"
        tar -C dist -czf "dist/HouseHaunters-Linux-${arch}.tar.gz" \
            "HouseHaunters-Linux-${arch}"
        echo "Created dist/HouseHaunters-Linux-${arch}.tar.gz"
        ;;
    *)
        echo "Unsupported operating system: $(uname -s)" >&2
        exit 1
        ;;
esac