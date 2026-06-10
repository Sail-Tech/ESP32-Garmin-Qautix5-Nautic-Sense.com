#!/usr/bin/env bash
# Build Marine Console — Hybrid (universal + MOB).
# so one binary targets every generic-BLE Garmin (2019+). Build against any
# installed listed device (e.g. fenix843mm / venu445mm / fr965).
#
#   ./build.sh               # default device below
#   ./build.sh venu445mm     # any installed product from the manifest
set -euo pipefail
SDK="${CIQ_SDK:-$HOME/Library/Application Support/Garmin/ConnectIQ/Sdks/connectiq-sdk-mac-9.1.0-2026-03-09-6a872a80b}"
KEY="${DEV_KEY:-$HOME/Documents/Garmin - Quatix/developer_key.der}"
DEVICE="${1:-fenix843mm}"
export JAVA_HOME="${JAVA_HOME:-/usr/local/opt/openjdk@17}"
export PATH="$JAVA_HOME/bin:$PATH"
cd "$(dirname "$0")"
mkdir -p bin
"$SDK/bin/monkeyc" -f monkey.jungle -d "$DEVICE" -o "bin/MarineHybrid.prg" -y "$KEY"
echo "OK -> bin/MarineHybrid.prg (device=$DEVICE)"
