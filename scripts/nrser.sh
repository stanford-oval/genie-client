#!/usr/bin/env bash

# Common / useful `set` commands
set -Ee # Exit on error
set -o pipefail # Check status of piped commands
set -u # Error on undefined vars
# set -v # Print everything
# set -x # Print commands (with expanded vars)

export BLOB=1
export ARCH=armhf
export IP_ADDRESS=xd-bs

ROOT="$(git rev-parse --show-toplevel)"

cd "$ROOT"
kubectl delete -n almond-dev pod shared-backend-0
./scripts/build-docker.sh
./scripts/deploy.sh
