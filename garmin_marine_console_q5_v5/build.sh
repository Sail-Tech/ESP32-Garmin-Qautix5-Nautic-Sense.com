#!/usr/bin/env bash
# Build Marine Console Q5 V5.
#
# Requires: Connect IQ SDK + JDK 17 (install via `brew install openjdk@17`).
# For the production target `quatix5`, that device must be installed via the
# SDK Manager (ConnectIQ.app, requires a Garmin login). The fenix5 device covers
# the quatix 5 part number (006-B2697-00).
#
# Usage:
#   ./build.sh                # quatix5 target (production)
#   ./build.sh fenix5         # already-installed test device (validation)
set -euo pipefail

SDK="${CIQ_SDK:-$HOME/Library/Application Support/Garmin/ConnectIQ/Sdks/connectiq-sdk-mac-9.1.0-2026-03-09-6a872a80b}"
KEY="${DEV_KEY:-$HOME/Documents/Garmin - Quatix/developer_key.der}"
DEVICE="${1:-quatix5}"

export JAVA_HOME="${JAVA_HOME:-/usr/local/opt/openjdk@17}"
export PATH="$JAVA_HOME/bin:$PATH"

cd "$(dirname "$0")"
mkdir -p bin
"$SDK/bin/monkeyc" -f monkey.jungle -d "$DEVICE" -o "bin/MarineConsoleQ5V5.prg" -y "$KEY"
echo "OK -> bin/MarineConsoleQ5V5.prg (device=$DEVICE)"
