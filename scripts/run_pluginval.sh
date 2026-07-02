#!/usr/bin/env bash
# Validates the built plugin at max strictness. Install pluginval first:
#   brew install --cask pluginval
set -euo pipefail
cd "$(dirname "$0")/.."

PLUGIN="${1:-build/plugin/OrbitDelay_artefacts/Debug/VST3/Orbit.vst3}"

if ! command -v pluginval >/dev/null 2>&1; then
    echo "pluginval not found — install with: brew install --cask pluginval" >&2
    exit 1
fi

pluginval --strictness-level 10 --validate "$PLUGIN"
