#!/usr/bin/env bash
# Builds the firmware and pushes it to the device over HTTP OTA.
# Bump FIRMWARE_VERSION in include/Config.h before each release.
# Usage: tools/deploy.sh [host]   (default host: 192.168.1.100)
set -euo pipefail
cd "$(dirname "$0")/.."

HOST="${1:-192.168.1.100}"
VERSION=$(sed -n 's/^#define FIRMWARE_VERSION "\(.*\)"/\1/p' include/Config.h)

echo "Deploying DeyeBridge v${VERSION} to ${HOST}"
python3 -m platformio run -e esp32s3

curl -sS -m 300 -X POST --data-binary @.pio/build/esp32s3/firmware.bin \
     -w "\nHTTP %{http_code} in %{time_total}s\n" "http://${HOST}/update"

echo "Waiting for the device to come back..."
for _ in $(seq 1 30); do
    if curl -s -m 3 -o /dev/null "http://${HOST}/diag"; then
        echo "Device is back online: http://${HOST}/"
        curl -s "http://${HOST}/diag"; echo
        exit 0
    fi
    sleep 2
done
echo "Device did not come back online within 60s" >&2
exit 1
