#!/usr/bin/env python3
"""
Builds the console update of the launcher itself, separate from the pack updates.

Takes the channel files of a build (the CI artifact "vanzakart-launcher", or build/release) and writes:
  <out>/updates/VanzaKartChannel-<version>.zip   the launcher files, at their paths on the SD card
  <out>/VanzaKartChannel.txt                      "<version> <url of the zip>", read by the launcher

Upload both next to VanzaKartVersion.txt. Launchers older than <version> install the ZIP the next time
they check for updates (see RRC_LAUNCHER_VERSION_URL in source/update/update.h). The version must be the
one the build was made with (shared/version.h), otherwise launchers keep reinstalling it.

Usage:
  python tools/make_launcher_update.py --channel D:\\vk-channel --version 1.0.0 --out D:\\dist-console
"""

import argparse
import pathlib
import re
import zipfile

# Only these folders may be in the ZIP: the launcher refuses anything else.
FOLDERS = ("apps/VanzaKart", "VanzaKartChannel")
BASE_URL = "http://vanzakart.net:8000/VanzaKart"

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--channel", required=True, help="folder containing apps/VanzaKart and VanzaKartChannel")
parser.add_argument("--version", required=True, help="launcher version of that build, e.g. 1.0.0")
parser.add_argument("--out", required=True, help="output folder, e.g. D:\\dist-console")
parser.add_argument("--base-url", default=BASE_URL, help=f"server folder the files are uploaded to (default {BASE_URL})")
args = parser.parse_args()

if not re.fullmatch(r"\d+\.\d+\.\d+", args.version):
    raise SystemExit("The version must be three numbers, e.g. 1.0.0")

channel = pathlib.Path(args.channel)
files = []
for folder in FOLDERS:
    root = channel / folder
    if not root.is_dir():
        raise SystemExit(f"Missing {root}")
    files += [p for p in sorted(root.rglob("*")) if p.is_file()]

meta = channel / "apps" / "VanzaKart" / "meta.xml"
built = re.search(r"<version>v?([\d.]+)</version>", meta.read_text(encoding="utf-8")) if meta.is_file() else None
if built and built.group(1) != args.version:
    raise SystemExit(f"meta.xml says the build is {built.group(1)}, not {args.version}")

out = pathlib.Path(args.out)
zip_name = f"VanzaKartChannel-{args.version}.zip"
zip_path = out / "updates" / zip_name
zip_path.parent.mkdir(parents=True, exist_ok=True)

with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
    for path in files:
        # forward slashes: these are the paths the launcher extracts to on the SD card
        z.write(path, path.relative_to(channel).as_posix())

(out / "VanzaKartChannel.txt").write_text(f"{args.version} {args.base_url.rstrip('/')}/updates/{zip_name}\n", encoding="ascii", newline="\n")

print(f"{zip_path} ({len(files)} files)")
print(f"{out / 'VanzaKartChannel.txt'}: {(out / 'VanzaKartChannel.txt').read_text().strip()}")
