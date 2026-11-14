# Copyright 2021-2026 Paulo Mateus
#
# SPDX-License-Identifier: Apache-2.0

"""
A small Turtle reader and the model of a plugin bundle that the preview needs (ports, parameters,
default state, factory presets and the files of its modgui). Standard library only.

It understands the subset of Turtle used by the bundles of this repository: prefixes, IRIs, prefixed
names, strings, numbers, `a`, `;`, `,`, blank nodes `[ ... ]` and the final `.`.
"""
import pathlib
import re

RDF_TYPE = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
NS = {
    "lv2": "http://lv2plug.in/ns/lv2core#",
    "rdfs": "http://www.w3.org/2000/01/rdf-schema#",
    "pset": "http://lv2plug.in/ns/ext/presets#",
    "state": "http://lv2plug.in/ns/ext/state#",
    "patch": "http://lv2plug.in/ns/ext/patch#",
    "atom": "http://lv2plug.in/ns/ext/atom#",
    "midi": "http://lv2plug.in/ns/ext/midi#",
    "pg": "http://lv2plug.in/ns/ext/port-groups#",
    "mod": "http://moddevices.com/ns/mod#",
    "modgui": "http://moddevices.com/ns/modgui#",
}


class IRI(str):
    """A resource, as opposed to a string literal."""


TOKEN = re.compile(
    r"""
    (?P<ws>\s+|\#[^\n]*)
  | (?P<iri><[^>]*>)
  | (?P<long>\"\"\"(?:.|\n)*?\"\"\")
  | (?P<str>"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')
  | (?P<prefix>@prefix|@base|PREFIX|BASE)
  | (?P<num>[+-]?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)(?![\w:])
  | (?P<pname>(?:[A-Za-z_][\w-]*)?:(?:[\w%-]+(?:\.[\w%-]+)*)?)
  | (?P<dt>\^\^)
  | (?P<lang>@[A-Za-z][\w-]*)
  | (?P<kw>a|true|false)(?![\w:])
  | (?P<p>[.;,\[\]()])
    """,
    re.VERBOSE,
)


def tokenize(text):
    pos, tokens = 0, []

    while pos < len(text):
        match = TOKEN.match(text, pos)

        if not match:
            raise ValueError("unexpected input at: " + text[pos:pos + 40].replace("\n", " "))

        pos = match.end()
        if match.lastgroup != "ws":
            tokens.append((match.lastgroup, match.group()))

    return tokens


def unescape(raw):
    body = raw[3:-3] if raw.startswith('"""') else raw[1:-1]
    return re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t"}.get(m.group(1), m.group(1)), body)


class Parser:
    def __init__(self, text):
        self.tokens = tokenize(text)
        self.i = 0
        self.prefixes = {}
        self.graph = {}

    def peek(self):
        return self.tokens[self.i] if self.i < len(self.tokens) else (None, None)

    def take(self):
        token = self.peek()
        self.i += 1
        return token

    def expand(self, pname):
        prefix, _, local = pname.partition(":")
        return IRI(self.prefixes.get(prefix, prefix + ":") + local)

    def term(self):
        kind, value = self.take()

        if kind == "iri":
            return IRI(value[1:-1])
        if kind == "pname":
            return self.expand(value)
        if kind in ("str", "long"):
            text = unescape(value)
            if self.peek()[0] == "dt":
                self.take()
                self.take()
            elif self.peek()[0] == "lang":
                self.take()
            return text
        if kind == "num":
            return float(value) if re.search(r"[.eE]", value) else int(value)
        if kind == "kw":
            return {"a": IRI(RDF_TYPE), "true": True, "false": False}[value]
        if (kind, value) == ("p", "["):
            node = {}
            self.property_list(node, "]")
            return node
        if (kind, value) == ("p", "("):
            items = []
            while self.peek() != ("p", ")"):
                items.append(self.term())
            self.take()
            return items

        raise ValueError(f"unexpected token {kind} {value}")

    def property_list(self, node, end):
        while self.peek() != ("p", end):
            predicate = self.term()
            objects = [self.term()]

            while self.peek() == ("p", ","):
                self.take()
                objects.append(self.term())

            node.setdefault(str(predicate), []).extend(objects)

            if self.peek() == ("p", ";"):
                while self.peek() == ("p", ";"):
                    self.take()
            else:
                break

        self.take()  # the end: ']' or '.'

    def parse(self):
        while self.i < len(self.tokens):
            kind, value = self.peek()

            if kind == "prefix":
                self.take()
                _, name = self.take()
                _, iri = self.take()
                if value.lower().endswith("prefix"):
                    self.prefixes[name[:-1]] = iri[1:-1]
                if self.peek() == ("p", "."):
                    self.take()
                continue

            subject = self.term()
            node = self.graph.setdefault(str(subject), {}) if isinstance(subject, IRI) else {}
            self.property_list(node, ".")

        return self.graph


def parse_file(path):
    return Parser(pathlib.Path(path).read_text(encoding="utf-8")).parse()


def merge(graphs):
    merged = {}

    for graph in graphs:
        for subject, properties in graph.items():
            target = merged.setdefault(subject, {})
            for predicate, objects in properties.items():
                target.setdefault(predicate, []).extend(objects)

    return merged


# ---------------------------------------------------------------------------
# The model

def first(node, predicate, default=None):
    values = node.get(predicate) or []
    return values[0] if values else default


def local_name(iri):
    return re.split(r"[#/]", str(iri))[-1]


def port_model(node):
    types = {local_name(t) for t in node.get(RDF_TYPE, [])}
    properties = {local_name(p) for p in node.get(NS["lv2"] + "portProperty", [])}
    supports = {str(s) for s in node.get(NS["atom"] + "supports", [])}

    if "ControlPort" in types:
        kind = "control"
    elif "AudioPort" in types:
        kind = "audio"
    elif "CVPort" in types:
        kind = "cv"
    elif "AtomPort" in types and NS["midi"] + "MidiEvent" in supports:
        kind = "midi"
    else:
        return None

    port = {
        "kind": kind,
        "direction": "output" if "OutputPort" in types else "input",
        "index": first(node, NS["lv2"] + "index"),
        "symbol": first(node, NS["lv2"] + "symbol"),
        "name": first(node, NS["lv2"] + "name"),
        "comment": first(node, NS["rdfs"] + "comment", ""),
    }
    port["shortName"] = port["name"]

    if kind == "control":
        minimum = first(node, NS["lv2"] + "minimum", 0)
        port.update({
            "default": first(node, NS["lv2"] + "default", minimum),
            "minimum": minimum,
            "maximum": first(node, NS["lv2"] + "maximum", 1),
            "toggled": "toggled" in properties,
            "integer": "integer" in properties,
            "enumeration": "enumeration" in properties,
            "scalePoints": [
                {"label": first(p, NS["rdfs"] + "label"), "value": first(p, "http://www.w3.org/1999/02/22-rdf-syntax-ns#value")}
                for p in node.get(NS["lv2"] + "scalePoint", [])
            ],
        })

    return port


def preset_model(graph, subject):
    node = graph[subject]
    ports = {}
    for port in node.get(NS["lv2"] + "port", []):
        ports[first(port, NS["lv2"] + "symbol")] = first(port, NS["pset"] + "value")

    state = {}
    for block in node.get(NS["state"] + "state", []):
        state.update({predicate: objects[0] for predicate, objects in block.items()})

    return {"label": first(node, NS["rdfs"] + "label", local_name(subject)), "ports": ports, "state": state}


def load_bundle(bundle):
    """Everything the preview needs to know about a `<name>.lv2` bundle."""
    bundle = pathlib.Path(bundle)
    manifest = parse_file(bundle / "manifest.ttl")
    graphs = [manifest]

    for properties in manifest.values():
        for see_also in properties.get(NS["rdfs"] + "seeAlso", []):
            path = bundle / str(see_also)
            if path.is_file() and path.suffix == ".ttl":
                graphs.append(parse_file(path))

    graph = merge(graphs)
    uri = next(s for s, p in graph.items() if NS["lv2"] + "Plugin" in map(str, p.get(RDF_TYPE, [])) and NS["lv2"] + "port" in p)
    plugin = graph[uri]

    ports = {kind: {"input": [], "output": []} for kind in ("audio", "midi", "cv", "control")}
    for node in plugin.get(NS["lv2"] + "port", []):
        port = port_model(node)
        if port:
            ports[port["kind"]][port["direction"]].append(port)
    for kind in ports.values():
        for direction in kind.values():
            direction.sort(key=lambda p: p["index"])

    parameters = []
    for parameter in plugin.get(NS["patch"] + "writable", []):
        parameters.append({"uri": str(parameter), "label": first(graph.get(str(parameter), {}), NS["rdfs"] + "label", local_name(parameter))})

    state = {}
    for block in plugin.get(NS["state"] + "state", []):
        state.update({predicate: objects[0] for predicate, objects in block.items()})

    presets = [
        preset_model(graph, s) for s, p in graph.items()
        if NS["pset"] + "Preset" in map(str, p.get(RDF_TYPE, [])) and NS["lv2"] + "port" in p
    ]
    presets.sort(key=lambda p: p["label"])

    gui = {}
    modgui = {}
    modgui_file = bundle / "modgui.ttl"
    if modgui_file.is_file():
        modgui_graph = parse_file(modgui_file)
        block = first(modgui_graph.get(uri, {}), NS["modgui"] + "gui", {})
        modgui = {local_name(k): v[0] for k, v in block.items()}

    resources = str(modgui.get("resourcesDirectory", "modgui")).rstrip("/")

    def relative(key):
        value = modgui.get(key)
        return None if value is None else str(value)[len(resources) + 1:] if str(value).startswith(resources + "/") else str(value)

    gui = {key: relative(key) for key in ("iconTemplate", "settingsTemplate", "stylesheet", "javascript")}

    return {
        "name": bundle.name[: -len(".lv2")],
        "uri": uri,
        "title": first(plugin, "http://usefulinc.com/ns/doap#name", bundle.name),
        "gui": gui,
        "brand": modgui.get("brand", ""),
        "label": modgui.get("label", ""),
        "color": modgui.get("color", ""),
        "ports": ports,
        "parameters": parameters,
        "state": state,
        "presets": presets,
    }
