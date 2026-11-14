#!/usr/bin/env python3
# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
Previews the modgui of the plugins of this repository in a browser, without installing anything on a device.

It serves, side by side:
  /modui/            the web interface of MOD UI (its real stylesheets and scripts: jQuery, Mustache, pedals.css...)
  /plugin/<name>/    plugins/resources/<name>.lv2/modgui (read from disk on every request, just reload the page)
  /api/plugins       the bundles found in plugins/resources
  /api/plugin/<name> what the preview needs from a bundle: ports, parameters, state, factory presets, modgui files
  /                  this folder (the preview page)

Usage: python3 tools/modgui-preview/serve.py [--mod-ui PATH_TO/mod-ui/html] [--port 8000]
"""
import argparse
import http.server
import json
import os
import pathlib
import posixpath
import sys
import urllib.parse

# Importing ttl.py must not leave a __pycache__ in the repository
sys.dont_write_bytecode = True

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[1]
RESOURCES = REPO / "plugins" / "resources"

sys.path.insert(0, str(HERE))
import ttl  # noqa: E402


def bundles():
    return sorted(p.name[: -len(".lv2")] for p in RESOURCES.glob("*.lv2") if (p / "modgui").is_dir())


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
    class Handler(http.server.SimpleHTTPRequestHandler):
        def send_json(self, payload, status=200):
            body = json.dumps(payload).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            path = urllib.parse.urlparse(self.path).path

            if path == "/api/plugins":
                return self.send_json(bundles())

            if path.startswith("/api/plugin/"):
                name = path[len("/api/plugin/"):]
                if name not in bundles():
                    return self.send_json({"error": "unknown plugin"}, 404)
                try:
                    return self.send_json(ttl.load_bundle(RESOURCES / f"{name}.lv2"))
                except Exception as error:  # a TTL that this small reader does not understand
                    return self.send_json({"error": f"could not read the TTL of {name}: {error}"}, 500)

            return super().do_GET()

        def translate_path(self, path):
            path = posixpath.normpath(urllib.parse.unquote(urllib.parse.urlparse(path).path))
            parts = [part for part in path.split("/") if part and part not in (".", "..")]
            root = HERE

            if parts and parts[0] == "modui":
                root, parts = mod_ui, parts[1:]
            elif len(parts) > 1 and parts[0] == "plugin" and parts[1] in bundles():
                root, parts = RESOURCES / f"{parts[1]}.lv2" / "modgui", parts[2:]

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
    print(f"Plugins: {', '.join(bundles())}")
    print(f"Open http://localhost:{args.port}/   (Ctrl+C to stop)")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
