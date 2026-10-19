#!/usr/bin/env python3
"""Fault-inject the actual advertising state machine and connection callbacks.

Usage: run.py [ZMK_SOURCE_ROOT | BLE_C]
This is a host test, not a Zephyr scheduler, controller, or radio integration test.
"""

from pathlib import Path
import os
import subprocess
import sys
import tempfile


here = Path(__file__).resolve().parent
root = here.parents[1]
source = Path(sys.argv[1]) if len(sys.argv) > 1 else root.parent / "ph-design-zmk"
if source.is_dir():
    source /= "app/src/ble.c"
text = source.read_text()
state_machine = text[text.index("#define CHECKED_ADV_STOP()") : text.index("static void clear_profile_bond(")]
callbacks = text[text.index("static bool is_conn_active_profile(") : text.index("static void security_changed(")]

with tempfile.TemporaryDirectory() as directory:
    generated = Path(directory) / "test.c"
    generated.write_text(
        (here / "fakes.h").read_text()
        + "\n"
        + state_machine
        + "\n"
        + callbacks
        + "\n"
        + (here / "cases.c").read_text()
    )
    executable = Path(directory) / "test"
    subprocess.run(
        [
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-fsanitize=address,undefined", "-g",
            str(generated), "-o", str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)
