#!/usr/bin/env python3
"""Render a product's installer resources and Read Me PDF from the Windward Audio installer
standard (docs/INSTALLER_STANDARD.md). Python 3 standard library only.

    python3 installer/render.py --pkg-version <component version> --disk-space "<n> MB" \
        --resources <dir> --distribution <file> [--readme-pdf <file>] [--product installer/product.json]

Writes into --resources: welcome.html, readme.html, conclusion.html, EULA.rtf, background.png,
background-dark.png; the productbuild distribution to --distribution; the Read Me PDF to
--readme-pdf. The wording is in installer/templates; installer/product.json holds the only
per-product values.
"""
import argparse
import json
import re
import shutil
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
TEMPLATES = HERE / "templates"


# ---------------------------------------------------------------- templates

def load_product(path):
    product = json.loads(Path(path).read_text(encoding="utf-8"))
    for key in ("product", "id", "version", "description", "accent", "min_macos", "min_macos_name", "plugins"):
        if key not in product:
            sys.exit(f"render.py: {path} has no '{key}'")
    if not re.fullmatch(r"[a-z0-9]+", product["id"]):
        sys.exit("render.py: 'id' must be lower-case letters and digits (it is used in com.windwardaudio.pkg.<id>)")
    if not re.fullmatch(r"#[0-9a-fA-F]{6}", product["accent"]):
        sys.exit("render.py: 'accent' must be #rrggbb")
    return product


def substitute(text, values):
    def one(match):
        key = match.group(1)
        if key not in values:
            sys.exit(f"render.py: unknown placeholder @{key}@")
        return values[key]
    return re.sub(r"@([A-Z_]+)@", one, text)


def render(text, values, plugins):
    """@EACH_PLUGIN@ ... @END@ repeats its body once per plug-in, then placeholders are filled."""
    def each(match):
        return "".join(substitute(match.group(1), {**values, **p}) for p in plugins)
    text = re.sub(r"@EACH_PLUGIN@(.*?)@END@\n?", each, text, flags=re.S)
    return substitute(text, values)


def plugin_values(product, html):
    kinds = json.loads((TEMPLATES / "kinds.json").read_text(encoding="utf-8"))
    out = []
    for plugin in product["plugins"]:
        kind = kinds.get(plugin["kind"])
        if kind is None:
            sys.exit(f"render.py: plug-in kind must be one of {', '.join(kinds)}")
        name = esc(plugin["name"]) if html else plugin["name"]
        v = {"NAME": name, "KIND": kind["kind"], "KIND_ARTICLE": "an" if kind["kind"][0] in "aeiou" else "a"}
        for key in ("garageband", "logic", "other"):
            v[key.upper() + "_PATH"] = substitute(esc(kind[key]) if html else kind[key], {"NAME": name})
        out.append(v)
    return out


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


# ---------------------------------------------------------------- PDF (standard Type 1 fonts)

# Advance widths (1/1000 em) of the standard Helvetica and Helvetica-Bold, characters 32-126.
_HELV = [278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278,
         556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556,
         1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778,
         667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556,
         333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,
         556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584]
_HELV_BOLD = [278, 333, 474, 556, 556, 889, 722, 238, 333, 333, 389, 584, 278, 333, 278, 278,
              556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 333, 333, 584, 584, 584, 611,
              975, 722, 722, 722, 722, 667, 611, 778, 722, 278, 556, 722, 611, 833, 722, 778,
              667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 333, 278, 333, 584, 556,
              333, 556, 611, 556, 611, 556, 333, 611, 611, 278, 278, 556, 278, 889, 611, 611,
              611, 611, 389, 556, 333, 611, 556, 778, 556, 556, 500, 389, 280, 389, 584]
FONTS = {"F1": ("Helvetica", _HELV), "F2": ("Helvetica-Bold", _HELV_BOLD), "F3": ("Courier", None)}
BULLET = "•"   # WinAnsi 0x95


def text_width(s, font, size):
    widths = FONTS[font][1]
    total = 0
    for ch in s:
        if widths is None:
            total += 600
        elif ch == BULLET:
            total += 350
        else:
            o = ord(ch)
            total += widths[o - 32] if 32 <= o <= 126 else 600
    return total * size / 1000.0


def wrap(s, font, size, width):
    lines, line = [], ""
    for word in s.split(" "):
        trial = word if not line else line + " " + word
        if line and text_width(trial, font, size) > width:
            lines.append(line)
            line = word
        else:
            line = trial
    lines.append(line)
    return lines


def pdf_string(s):
    data = s.encode("cp1252", errors="replace")
    return "(" + data.decode("latin-1").replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)") + ")"


def hex_rgb(h):
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (1, 3, 5))


def readme_pdf(source, product, out):
    W, H, M = 595.28, 841.89, 64.0          # A4, margin
    width = W - 2 * M
    pages, ops = [], []
    y = H - M
    ink, grey, accent = (0.11, 0.11, 0.12), (0.43, 0.43, 0.45), tuple(c * 0.62 for c in hex_rgb(product["accent"]))   # as on the light background
    footer = f"{product['product']} by Windward Audio  -  support@windwardaudio.com"

    def new_page():
        nonlocal ops, y
        if ops:
            pages.append(ops)
        ops, y = [], H - M

    def need(h):
        if y - h < M + 24:
            new_page()

    def put(x, s, font, size, colour):
        ops.append(f"BT /{font} {size:.2f} Tf {colour[0]:.3f} {colour[1]:.3f} {colour[2]:.3f} rg "
                   f"{x:.2f} {y:.2f} Td {pdf_string(s)} Tj ET")

    def block(s, font="F1", size=10.5, colour=ink, indent=0.0, lead=None, before=0.0, marker=None, keep=0.0):
        nonlocal y
        lead = lead or size * 1.42
        lines = wrap(s, font, size, width - indent)
        need(before + lead * min(len(lines), 2) + keep)   # keep: room for what follows a heading
        y -= before
        for i, line in enumerate(lines):
            need(lead)
            y -= lead
            if i == 0 and marker:
                put(M + indent - 12, marker, "F1", size, colour)
            put(M + indent, line, font, size, colour)

    for raw in source.splitlines():
        line = raw.rstrip()
        if not line:
            y -= 5
        elif line.startswith("= "):
            block(line[2:], "F2", 22, ink, lead=26)
            y -= 8
            ops.append(f"{accent[0]:.3f} {accent[1]:.3f} {accent[2]:.3f} RG 2 w {M:.2f} {y:.2f} m {M + 36:.2f} {y:.2f} l S")
            y -= 4
        elif line.startswith("~ "):
            block(line[2:], "F1", 10, grey)
        elif line.startswith("== "):
            block(line[3:], "F2", 13, ink, before=14, lead=18, keep=40)
            y -= 2
        elif line.startswith(":: "):
            block(line[3:], "F2", 10.5, ink, before=6, keep=30)
        elif line.startswith("- "):
            block(line[2:], indent=14, marker=BULLET)
        elif re.match(r"\d+\. ", line):
            n, rest = line.split(" ", 1)
            block(rest, indent=14, marker=n)
        elif line.startswith("    "):
            block(line[4:], "F3", 9.5, ink, indent=14, lead=13)
        elif line.startswith("> "):
            block(line[2:], "F1", 9.5, grey)
        else:
            block(line)
    pages.append(ops)

    objects = []                     # PDF objects 1..n
    def add(body):
        objects.append(body)
        return len(objects)
    catalog = add(None)
    pages_obj = add(None)
    font_ids = {}
    for key, (name, _) in FONTS.items():
        font_ids[key] = add(f"<< /Type /Font /Subtype /Type1 /BaseFont /{name} /Encoding /WinAnsiEncoding >>")
    fonts = " ".join(f"/{k} {v} 0 R" for k, v in font_ids.items())
    kids = []
    for number, page_ops in enumerate(pages, 1):
        foot = (f"BT /F1 8 Tf {grey[0]:.3f} {grey[1]:.3f} {grey[2]:.3f} rg {M:.2f} {M - 24:.2f} Td {pdf_string(footer)} Tj ET "
                f"BT /F1 8 Tf {W - M - 20:.2f} {M - 24:.2f} Td {pdf_string(f'{number} / {len(pages)}')} Tj ET")
        stream = zlib.compress(("\n".join(page_ops + [foot])).encode("latin-1"))
        content = add(f"<< /Length {len(stream)} /Filter /FlateDecode >>\nstream\n".encode("latin-1") + stream + b"\nendstream")
        kids.append(add(f"<< /Type /Page /Parent {pages_obj} 0 R /MediaBox [0 0 {W} {H}] "
                        f"/Resources << /Font << {fonts} >> >> /Contents {content} 0 R >>"))
    objects[catalog - 1] = f"<< /Type /Catalog /Pages {pages_obj} 0 R >>"
    objects[pages_obj - 1] = f"<< /Type /Pages /Kids [{' '.join(f'{k} 0 R' for k in kids)}] /Count {len(kids)} >>"
    info = add(f"<< /Title {pdf_string(product['product'] + ' - Read Me')} /Author (Windward Audio) /Producer (Windward Audio installer) >>")

    data = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = []
    for i, body in enumerate(objects, 1):
        offsets.append(len(data))
        data += f"{i} 0 obj\n".encode()
        data += body if isinstance(body, bytes) else body.encode("latin-1")
        data += b"\nendobj\n"
    xref = len(data)
    data += f"xref\n0 {len(objects) + 1}\n0000000000 65535 f \n".encode()
    for off in offsets:
        data += f"{off:010d} 00000 n \n".encode()
    data += f"trailer\n<< /Size {len(objects) + 1} /Root {catalog} 0 R /Info {info} 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode()
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    Path(out).write_bytes(bytes(data))


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--product", default=str(HERE / "product.json"))
    ap.add_argument("--pkg-version", required=True, help="the component packages' version (the bundles' version)")
    ap.add_argument("--disk-space", required=True, help='e.g. "420 MB"')
    ap.add_argument("--resources", required=True)
    ap.add_argument("--distribution", required=True)
    ap.add_argument("--readme-pdf")
    args = ap.parse_args()

    product = load_product(args.product)
    names = [p["name"] for p in product["plugins"]]
    joined = names[0] if len(names) == 1 else ", ".join(names[:-1]) + " and " + names[-1]
    values = {
        "PRODUCT": product["product"], "ID": product["id"], "VERSION": product["version"],
        "PKG_VERSION": args.pkg_version, "DESCRIPTION": product["description"],
        "MIN_MACOS": product["min_macos"], "MIN_MACOS_NAME": product["min_macos_name"],
        "DISK_SPACE": args.disk_space, "PLUGIN_NAMES": joined,
    }
    html_values = {k: esc(v) for k, v in values.items()}
    html_values["STYLE"] = (TEMPLATES / "style.css").read_text(encoding="utf-8").strip()

    res = Path(args.resources)
    res.mkdir(parents=True, exist_ok=True)
    for page in ("welcome", "readme", "conclusion"):
        text = (TEMPLATES / f"{page}.html.in").read_text(encoding="utf-8")
        (res / f"{page}.html").write_text(render(text, html_values, plugin_values(product, True)), encoding="utf-8")
    for asset in ("EULA.rtf", "background.png", "background-dark.png"):
        shutil.copyfile(HERE / asset, res / asset)
    dist = (TEMPLATES / "distribution.xml.in").read_text(encoding="utf-8")
    Path(args.distribution).write_text(render(dist, html_values, []), encoding="utf-8")
    if args.readme_pdf:
        text = (TEMPLATES / "readme-pdf.txt.in").read_text(encoding="utf-8")
        readme_pdf(render(text, values, plugin_values(product, False)), product, args.readme_pdf)
    print(f"render.py: {product['product']} installer resources in {res}"
          + (f", Read Me {args.readme_pdf}" if args.readme_pdf else ""))


if __name__ == "__main__":
    main()
