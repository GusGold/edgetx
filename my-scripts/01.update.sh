#! /bin/sh

git fetch
git pull
git submodule update --init --recursive
# Update docker image
# Sourced from https://github.com/EdgeTX/build-edgetx/pkgs/container/edgetx-dev - Watch for non-main builds
docker pull ghcr.io/edgetx/edgetx-dev:2.8
# Remove dangling images
docker image rm $(docker image ls ghcr.io/edgetx/edgetx-dev --filter dangling=true --quiet)