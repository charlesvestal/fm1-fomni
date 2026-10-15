#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The pages' scripts parse: the installer as tools/make_pages.py puts it together (fm1pkg.js and fm1ota.js
inlined), and the emulator's. A script that doesn't parse leaves the page dead: no labels on its buttons, nothing
installs (0.4's installer declared STOCK_SHA256 a second time).   python3 tests/page_script_test.py   (needs node)"""
import re
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import strip_module  # noqa: E402


def check(name, js, module):
    r = subprocess.run(["node", "--check", "--input-type=" + ("module" if module else "commonjs")], input=js,
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"{name}: the script does not parse:\n{r.stderr}")
    print(f"ok {name}")


page = (SRC / "web" / "omni_installer.html").read_text(encoding="utf-8")
lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
    strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
page = page.replace("/*LIB*/", lib).replace("/*META*/", '{"version": "0", "product": "FM-1_8000000", "pkg": ""}')
check("installer", re.search(r'<script type="module">(.*?)</script>', page, re.S).group(1), True)
emu = (SRC / "web" / "emu" / "index.html").read_text(encoding="utf-8")
check("emulator", re.search(r"<script>(.*?)</script>", emu, re.S).group(1), False)
