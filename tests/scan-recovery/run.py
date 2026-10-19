#!/usr/bin/env python3
"""Exercise the actual scan/recovery and layout-selection source with C fakes.

Usage: run.py [path-to-zmk-root]
This is not a Zephyr/hold-tap integration or hardware test. The event sink checks
paired positions and models a momentary layer; work/driver interleavings are
injected deterministically. CI must also compile the complete firmware.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

tests = Path(__file__).resolve().parent
root = tests.parents[1]
zmk = Path(sys.argv[1]) if len(sys.argv) > 1 else root.parent / "ph-design-zmk"
source = (zmk / "app/src/physical_layouts.c").read_text()
start = source.index("#define ZMK_KSCAN_EVENT_STATE_PRESSED")
end = source.index("int zmk_physical_layouts_select(uint8_t index)", start)
fragment = source[start:end]
map_start = source.index("int zmk_physical_layouts_get_position_map(uint8_t source")
map_end = source.index("#if IS_ENABLED(CONFIG_SETTINGS)", map_start)
map_fragment = source[map_start:map_end]
conf = (root / "boards/shields/PH60-Multi_V2/PH60-Multi_V2.conf").read_text()
queue_size = int(re.search(r"^CONFIG_ZMK_KSCAN_EVENT_QUEUE_SIZE=(\d+)$", conf, re.M)[1])
assert queue_size >= 75, "A complete 5 x 15 matrix scan must fit"

with tempfile.TemporaryDirectory(prefix="zmk-scan-recovery-") as tmp:
    c = Path(tmp) / "test.c"
    c.write_text((tests / "fakes.h").read_text() + "\n" + fragment + "\n" +
                 (tests / "cases.c").read_text())
    for capacity in (3, queue_size):
        exe = Path(tmp) / f"scan-{capacity}"
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-Wno-sign-compare", "-fsanitize=address,undefined",
            "-g", f"-DCONFIG_ZMK_KSCAN_EVENT_QUEUE_SIZE={capacity}", str(c), "-o", str(exe),
        ], check=True)
        subprocess.run([str(exe)], check=True)
    c = Path(tmp) / "mapping.c"
    c.write_text((tests / "mapping-fakes.h").read_text() + "\n" + map_fragment + "\n" +
                 (tests / "mapping-cases.c").read_text())
    exe = Path(tmp) / "mapping"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-Wno-sign-compare", "-fsanitize=address,undefined", "-g", str(c), "-o", str(exe),
    ], check=True)
    subprocess.run([str(exe)], check=True)
