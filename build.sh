#!/bin/sh
# Build Nextbot on Linux/macOS using the amd64 "oo-ps4-build" container.
#   ./build.sh | ./build.sh pkg | ./build.sh host-test | ./build.sh clean
set -e
cd "$(dirname "$0")"
PROJECT="$(pwd)"
TOOLCHAIN="${OO_PS4_TOOLCHAIN:-$(cd ../PS4Toolchain && pwd)}"
IMAGE=oo-ps4-build

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  if [ -f ../oo-ps4-build.tar.gz ]; then docker load -i ../oo-ps4-build.tar.gz
  else docker build --platform linux/amd64 -t "$IMAGE" docker; fi
fi

docker run --rm --platform linux/amd64 \
  -v "$TOOLCHAIN":/toolchain:ro -v "$PROJECT":/project -w /project \
  -e OO_PS4_TOOLCHAIN=/toolchain -e DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 \
  "$IMAGE" make "$@"
