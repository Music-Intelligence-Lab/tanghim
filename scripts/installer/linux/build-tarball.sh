#!/bin/bash
# Builds the Linux release tarball.
#
# Usage: build-tarball.sh <version> <plugins-dir> <output-dir>
#
# <plugins-dir> must contain:
#   Tanghim.vst3/
#   Tanghim.clap
#   Tanghim Receiver.vst3/
#   Tanghim Receiver.clap
#
# Produces: <output-dir>/Tanghim-<version>-Linux.tar.gz

set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "Usage: $0 <version> <plugins-dir> <output-dir>" >&2
    exit 2
fi

VERSION="$1"
PLUGINS_DIR="$2"
OUTPUT_DIR="$3"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mkdir -p "${OUTPUT_DIR}"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

STAGE="${WORK_DIR}/Tanghim-${VERSION}-Linux"
mkdir -p "${STAGE}/plugins"

for item in "Tanghim.vst3" "Tanghim Receiver.vst3"; do
    cp -r "${PLUGINS_DIR}/${item}" "${STAGE}/plugins/"
done
for item in "Tanghim.clap" "Tanghim Receiver.clap"; do
    cp "${PLUGINS_DIR}/${item}" "${STAGE}/plugins/"
done

cp "${SCRIPT_DIR}/install.sh" "${STAGE}/install.sh"
cp "${SCRIPT_DIR}/README.txt" "${STAGE}/README.txt"
chmod +x "${STAGE}/install.sh"

OUTPUT_TGZ="${OUTPUT_DIR}/Tanghim-${VERSION}-Linux.tar.gz"
tar -czf "${OUTPUT_TGZ}" -C "${WORK_DIR}" "Tanghim-${VERSION}-Linux"

echo "Built ${OUTPUT_TGZ}"
ls -lh "${OUTPUT_TGZ}"
tar -tzf "${OUTPUT_TGZ}" | head -20
