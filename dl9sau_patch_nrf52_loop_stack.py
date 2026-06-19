"""
PlatformIO pre-build script (NRF52 only): patch Adafruit-nRF52 framework's
LOOP_STACK_SZ from 4 KB to 8 KB.

Problem:
  Framework default in ~/.platformio/packages/framework-arduinoadafruitnrf52/
  cores/nRF5/main.cpp Z 42:
    #define LOOP_STACK_SZ       (256*4)   // 1024 words = 4096 bytes

  Adafruit-Bluefruit-Stack's loop()-Task hat damit nur 4 KB Stack. MeshCore-
  Companion-Code mit grossen Locals in handleCompanionCommand + sub-calls
  hat Peak >4 KB -> Stack-Overflow -> Hard-Fault -> Watchdog-Reset.
  Symptom: 'Output ok, dann Freeze + USB-PHY haengt + Auto-Recovery nur
  via Reset-Doppelklick'.

  Da #define KEIN #ifndef hat, ist '-D LOOP_STACK_SZ=...' build-flag
  wirkungslos. Der einzige Weg ist Datei-Patch.

Empirisch (User-Befund 2026-06-17):
  vor Patch  (4KB):  Boot 5400B free -> 'backup save' triggert Hard-Fault
  nach Patch (8KB):  Boot 5400B free -> 'backup save' lief mit 4540B free
  2x backup+x:       3980B free -- noch komfortabel

Patch: (256*4) -> (512*4)  also 4KB -> 8KB.

Idempotent: wenn schon gepatcht, nichts tun. Auto-runs vor jedem Build.

Frueher manuell editiert; ging beim naechsten PlatformIO-Framework-
Update verloren. Dieses Skript stellt sicher dass der Patch IMMER da
ist bevor compiliert wird.
"""
Import("env")
import os
import re
import sys


def _patch():
    # PlatformIO framework path is platform-dependent. Adafruit nRF52
    # is unpacked under ~/.platformio/packages/framework-arduinoadafruitnrf52/
    # or wherever PIO's PACKAGES_DIR points to.
    pkg_dir = env.PioPlatform().get_package_dir("framework-arduinoadafruitnrf52")
    if not pkg_dir:
        print("[dl9sau_patch_nrf52_loop_stack] framework-arduinoadafruitnrf52 "
              "package not installed; skip.")
        return

    target = os.path.join(pkg_dir, "cores", "nRF5", "main.cpp")
    if not os.path.isfile(target):
        print("[dl9sau_patch_nrf52_loop_stack] target file not found: %s; skip."
              % target)
        return

    with open(target, "r") as f:
        content = f.read()

    # Idempotent: already at 8KB?
    if re.search(r"#define\s+LOOP_STACK_SZ\s+\(512\*4\)", content):
        print("[dl9sau_patch_nrf52_loop_stack] already patched (8 KB).")
        return

    # Original 4 KB pattern -- patch to 8 KB
    new_content, n = re.subn(
        r"#define\s+LOOP_STACK_SZ\s+\(256\*4\)",
        "#define LOOP_STACK_SZ       (512*4)   // DL9SAU 4KB->8KB",
        content,
        count=1,
    )
    if n == 0:
        print("[dl9sau_patch_nrf52_loop_stack] WARNING: LOOP_STACK_SZ "
              "(256*4) not found. Framework may have changed format. "
              "Manual review of %s needed." % target)
        return

    # Backup once (don't overwrite existing .bak)
    bak = target + ".dl9sau.bak"
    if not os.path.isfile(bak):
        try:
            with open(bak, "w") as f:
                f.write(content)
        except OSError as e:
            print("[dl9sau_patch_nrf52_loop_stack] could not write backup "
                  "(%s); proceeding anyway." % e)

    with open(target, "w") as f:
        f.write(new_content)
    print("[dl9sau_patch_nrf52_loop_stack] patched LOOP_STACK_SZ 4KB -> 8KB "
          "in %s" % target)


_patch()
