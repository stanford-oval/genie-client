#!/usr/bin/env bash

set -ex
scp build/config.ini root@$IP_ADDRESS:/opt/genie/
