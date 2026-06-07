#!/usr/bin/env bash
# Build Marine Console — Venu 3.
#
# Requires: Connect IQ SDK + JDK 17 (install via `brew install openjdk@17`).
# The production target `venu3` must be installed via the SDK Manager
# (ConnectIQ.app, requires a Garmin login). For local validation you can build
# against an already-installed Venu device (e.g. venu445mm).
#
# Usage:
#   ./build.sh                # venu3 target (production)
#   ./build.sh venu445mm      # validation against an installed Venu device
set -euo pipefail

SDK="${CIQ_SDK:-$HOME/Library/Application Support/Garmin/ConnectIQ/Sdks/connectiq-sdk-mac-9.1.0-2026-03-09-6a872a80b}"
KEY="${DEV_KEY:-$HOME/Documents/Garmin - Quatix/developer_key.der}"
DEVICE="${1:-venu3}"

export JAVA_HOME="${JAVA_HOME:-/usr/local/opt/openjdk@17}"
export PATH="$JAVA_HOME/bin:$PATH"

cd "$(dirname "$0")"
mkdir -p bin
"$SDK/bin/monkeyc" -f monkey.jungle -d "$DEVICE" -o "bin/MarineConsoleVenu3.prg" -y "$KEY"
echo "OK -> bin/MarineConsoleVenu3.prg (device=$DEVICE)"
