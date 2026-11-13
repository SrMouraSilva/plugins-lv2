#!/usr/bin/env python3
# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
Previews the modgui of Hept7 Switch in a browser, without installing anything on a device.

It serves, side by side:
  /modui/   the web interface of MOD UI (its real stylesheets and scripts: jQuery, Mustache, pedals.css...)
  /plugin/  plugins/resources/hept7-switch.lv2/modgui (read from disk on every request, just reload the page)
  /         this folder (the preview page)

Usage: python3 tools/modgui-preview/serve.py [--mod-ui PATH_TO/mod-ui/html] [--port 8000]
"""
import argparse
import http.server
import os
import pathlib
import posixpath
import sys
import urllib.parse

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[1]
PLUGIN_MODGUI = REPO / "plugins" / "resources" / "hept7-switch.lv2" / "modgui"


def find_mod_ui(argument):
    candidates = [argument, os.environ.get("MOD_UI_HTML"), REPO.parent / "mod-ui" / "html"]

    for candidate in candidates:
        if candidate and (pathlib.Path(candidate) / "css" / "pedals.css").is_file():
            return pathlib.Path(candidate).resolve()

    sys.exit(
        "Could not find the MOD UI web interface (a folder with css/pedals.css).\n"
        "Clone https://github.com/moddevices/mod-ui next to this repository, or pass --mod-ui PATH/mod-ui/html "
        "(or set MOD_UI_HTML)."
    )


def make_handler(mod_ui):
    roots = {"modui": mod_ui, "plugin": PLUGIN_MODGUI}

    class Handler(http.server.SimpleHTTPRequestHandler):
        def translate_path(self, path):
            path = posixpath.normpath(urllib.parse.unquote(urllib.parse.urlparse(path).path))
            parts = [part for part in path.split("/") if part and part not in (".", "..")]
            root = HERE

            if parts and parts[0] in roots:
                root = roots[parts[0]]
                parts = parts[1:]

            return str(root.joinpath(*parts))

        def end_headers(self):
            self.send_header("Cache-Control", "no-store")
            super().end_headers()

        def log_message(self, *args):
            pass

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mod-ui", help="folder html/ of a mod-ui checkout (default: ../mod-ui/html)")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()

    mod_ui = find_mod_ui(args.mod_ui)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(mod_ui))

    print(f"MOD UI:  {mod_ui}")
    print(f"Plugin:  {PLUGIN_MODGUI}")
    print(f"Open http://localhost:{args.port}/   (Ctrl+C to stop)")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
