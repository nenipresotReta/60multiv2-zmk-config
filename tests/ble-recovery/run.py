#!/usr/bin/env python3
"""Run actual HOG/transport-watchdog code with deterministic Zephyr/BT fakes.

The fakes cover application state transitions, not real thread scheduling, radio
behavior, the Zephyr ABI, or hardware watchdog timing. A full firmware build and
on-device Bluetooth/USB testing are still required.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import sys
root = Path(__file__).resolve().parents[2]
source = Path(sys.argv[1]) if len(sys.argv) > 1 else root.parent / 'ph-design-zmk/app/src/hog.c'
s = source.read_text()
a = s.index('struct hog_tx_monitor {')
b = s.index('static int zmk_hog_init', a)
w = source.with_name('watchdog.c').read_text()
c = w.index('/* Legacy USB send failures:')
d = w.index('static int zmk_wdt_init', c)
tests = Path(__file__).parent
with tempfile.TemporaryDirectory() as tmp:
    test_c = Path(tmp) / 'test.c'
    test_c.write_text((tests / 'fakes.h').read_text() + '\n' + s[a:b] + '\n' + w[c:d] + '\n' + (tests / 'cases.c').read_text())
    exe = Path(tmp) / 'test'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined', '-g', str(test_c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
