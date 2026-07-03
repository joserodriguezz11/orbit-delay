#!/usr/bin/env bash
# Validates the built plugin at max strictness. Install pluginval first:
#   brew install --cask pluginval
set -euo pipefail
cd "$(dirname "$0")/.."

PLUGIN="${1:-build/plugin/OrbitDelay_artefacts/Debug/VST3/Orbit.vst3}"

PLUGINVAL="$(command -v pluginval || true)"
if [ -z "$PLUGINVAL" ] && [ -x "/Applications/pluginval.app/Contents/MacOS/pluginval" ]; then
    PLUGINVAL="/Applications/pluginval.app/Contents/MacOS/pluginval"
fi

if [ -z "$PLUGINVAL" ]; then
    echo "pluginval not found — install with: brew install --cask pluginval" >&2
    exit 1
fi

"$PLUGINVAL" --strictness-level 10 --validate "$PLUGIN"
