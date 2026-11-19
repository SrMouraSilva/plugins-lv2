#!/bin/sh
# Copyright 2021-2026 Paulo Mateus
# SPDX-License-Identifier: Apache-2.0
#
# Renders social-preview.html to social-preview.png (1280 x 640) with headless Chrome.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
chrome=${CHROME:-"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"}
profile=$(mktemp -d)
trap 'rm -rf "$profile"' EXIT
"$chrome" --headless=new --disable-gpu --hide-scrollbars --user-data-dir="$profile" \
  --window-size=1280,640 --force-device-scale-factor=1 \
  --screenshot="$here/social-preview.png" "file://$here/social-preview.html"
