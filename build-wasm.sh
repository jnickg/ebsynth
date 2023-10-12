#!/usr/bin/env bash

pushd "$(dirname "$0")"
echo "Working from $(pwd)"

if [ -d dist ]; then
    rm -r dist;
fi

# User must provide a location for the emsdk repository, which will be used to set up the env
if [ -z "$EMSDK_REPO" ]; then
    echo "Please set the EMSDK environment variable to the location of the emsdk repository"
    exit 1
fi

${EMSDK_REPO}/emsdk install latest
${EMSDK_REPO}/emsdk activate latest
source ${EMSDK_REPO}/emsdk_env.sh

# Check if EBSYNCH_VERSION is set. If not, default to 0.0.0
if [ -z "$EBSYNTH_VERSION" ]; then
    echo "Warning: EBSYNTH_VERSION not set. Defaulting to 0.0.0"
    export EBSYNTH_VERSION="0.0.0"
fi

printf "\n\nInitiating build...\n"

# Sanity check whether EMSDK variable was set
if [ -z "$EMSDK" ]; then
    echo "EMSDK environment variable not set. Please make sure EMSDK_REPO was set properly and that emsdk was installed and activated."
    exit 1
fi

mkdir -p build/Release
pushd build/Release

emcmake cmake \
  -DCMAKE_TOOLCHAIN_FILE=${EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  ../.. 2>&1

emmake make ebsynth VERBOSE=1 -j 4 2>&1

popd
popd

echo "Done!"