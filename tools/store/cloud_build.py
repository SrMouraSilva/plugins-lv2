#!/usr/bin/env python3
# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""Builds the plugins of this repository on the MOD Cloud Builder and prints the page that installs each one on a unit.

    python3 tools/store/cloud_build.py hept7-switch                  # the commit that is checked out (HEAD)
    python3 tools/store/cloud_build.py --commit 1a2b3c4 gossiper kill-switch --output installs.json
    python3 tools/store/cloud_build.py all --commit "$GITHUB_SHA" --output installs.json   # every recipe, as docs.yml does
    python3 tools/store/cloud_build.py --list                        # the plugins (store/*.mk)

It does what https://builder.mod.audio/buildroot does in the browser (https://github.com/mod-audio/mod-cloud-builder,
webserver/server.py): sends the recipe store/<plugin>.mk over socket.io (`build`, type `buildroot`, `persistent`), which builds the
bundle for the Duo, the Duo X and the Dwarf and keeps the three, and waits for `buildurl`, the id of the stored build. The page
`<server>/install/<id>` is served from a mod.audio address, which is what the mod-ui of a unit accepts a connection from
(see "Documentation site" in CLAUDE.md), and installs it from the browser into a unit plugged in over USB.

The recipe is pinned to --commit in memory (store/*.mk are not touched; `tools/store/pin.py` is the one that rewrites them): the
builder clones this repository to build it, so the commit must exist on GitHub. Needs `pip install "python-socketio[client]"`.
A build takes minutes (three platforms, one after the other): the plugins are built one at a time, to be kind to a shared server.
"""
import argparse
import json
import re
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STORE = ROOT / "store"
SERVER = "http://builder.mod.audio"     # plain http on purpose: the https listener redirects clients that are not a Chromium browser


def git(*args: str) -> str:
    return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, check=True).stdout.strip()


def pinned(recipe: str, commit: str) -> str:
    text, count = re.subn(r"^(\w+_VERSION = ).*$", rf"\g<1>{commit}", recipe, count=1, flags=re.M)
    if count != 1:
        raise SystemExit("the recipe has no _VERSION line")
    return text


def build(server: str, plugin: str, recipe: str, device: str, timeout: float, verbose: bool) -> str:
    """Sends one recipe and returns the id of the stored build"""
    import socketio

    done = threading.Event()
    result = {"id": None, "error": None}
    log: list = []
    sio = socketio.Client(reconnection=False)

    @sio.on("buildlog")
    def _log(message):
        text = str(message).rstrip("\n")
        log.append(text)
        if verbose:
            print(f"  {text}", file=sys.stderr, flush=True)
        elif text.startswith("Starting build for") or text.startswith("All builds"):
            print(f"  {plugin}: {text}", file=sys.stderr, flush=True)

    @sio.on("buildurl")
    def _url(identifier):
        result["id"] = str(identifier)
        done.set()

    @sio.on("status")
    def _status(status):
        if status == "error" and not done.is_set():
            result["error"] = "the builder reported an error"
            done.set()

    @sio.event
    def disconnect():
        if not done.is_set():
            result["error"] = "the connection to the builder was closed"
            done.set()

    sio.connect(server, wait_timeout=30)
    try:
        sio.emit("build", {"type": "buildroot", "device": device, "persistent": True, "files": {f"{plugin}.mk": recipe}})
        if not done.wait(timeout):
            result["error"] = f"no answer in {timeout / 60:.0f} minutes"
    finally:
        if sio.connected:
            sio.disconnect()

    if not result["id"]:
        tail = "\n".join(f"    {line}" for line in log[-30:])
        raise RuntimeError(f"{plugin}: {result['error']}\n  the end of the log:\n{tail}")
    return result["id"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("plugins", nargs="*", help="what to build: the names of store/*.mk (without .mk), or `all`")
    parser.add_argument("--commit", default="HEAD", help="the commit to build (default: HEAD); it must be on GitHub")
    parser.add_argument("--device", default="dwarf", choices=["duo", "duox", "dwarf"],
                        help="the platform the builder starts with; it builds the other two after it (default: dwarf)")
    parser.add_argument("--server", default=SERVER)
    parser.add_argument("--timeout", type=float, default=60, help="minutes to wait for each plugin (default: 60)")
    parser.add_argument("--output", type=Path, help="write {plugin: install page} as JSON to this file")
    parser.add_argument("--verbose", action="store_true", help="print the whole log of the builder")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()

    available = sorted(p.stem for p in STORE.glob("*.mk"))
    if args.list:
        print("\n".join(available))
        return 0
    if not args.plugins:
        parser.error(f"name the plugins to build ({', '.join(available)})")
    if args.plugins == ["all"]:
        args.plugins = available
    unknown = [p for p in args.plugins if p not in available]
    if unknown:
        parser.error(f"no recipe in store/ for: {', '.join(unknown)}")

    commit = git("rev-parse", "--verify", f"{args.commit}^{{commit}}")
    if args.commit == "HEAD" and not git("branch", "-r", "--contains", commit):
        print(f"warning: {commit[:7]} is on no remote branch: the builder clones GitHub, so it will not find it", file=sys.stderr)

    pages = {}
    failed = False
    for plugin in args.plugins:
        print(f"{plugin}: building {commit[:7]} on {args.server} ...", file=sys.stderr, flush=True)
        started = time.time()
        try:
            identifier = build(args.server, plugin, pinned((STORE / f"{plugin}.mk").read_text(), commit), args.device,
                               args.timeout * 60, args.verbose)
        except Exception as error:      # one plugin failing must not lose the ones that were built
            print(f"{plugin}: FAILED: {error}", file=sys.stderr)
            failed = True
            continue
        pages[plugin] = f"{args.server}/install/{identifier}"
        print(f"{plugin}: {pages[plugin]}  ({(time.time() - started) / 60:.1f} min)")

    if args.output:
        args.output.write_text(json.dumps(pages, indent=2) + "\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
