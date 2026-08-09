"""
PlatformIO pre-build script: stamp the companion_radio firmware with the
current git short-hash and today's build date.

  FIRMWARE_VERSION       -> "v15-DL9SAU-<8charhash>"   (fits 20-byte wire field)
  FIRMWARE_BUILD_DATE    -> "DD Mon YYYY"               (fits 12-byte wire field)
  FIRMWARE_BUILD_TIME    -> "HH:MM"                     (CLI-only display)

If the build is run outside a git checkout the hash falls back to "nogit"
so the firmware still has a deterministic version string.

The defines are passed via -D so the #ifndef FIRMWARE_VERSION block in
MyMesh.h keeps the source-default as a fallback for builds that do not
include this script.
"""
Import("env")
import subprocess
from datetime import datetime


def _git_short_hash(repo_dir):
    try:
        out = subprocess.check_output(
            ["git", "-C", repo_dir, "rev-parse", "--short=8", "HEAD"],
            stderr=subprocess.DEVNULL,
        )
        return out.decode().strip()
    except Exception:
        return "nogit"


git_hash = _git_short_hash(env["PROJECT_DIR"])
build_date = datetime.now().strftime("%d %b %Y")              # 11 chars
build_time = datetime.now().strftime("%H:%M")                 # 5 chars (CLI only)
# Format: "v1.16.2-DL9SAU.gXXX" (19 chars). 'g'-Prefix folgt der
# 'git describe'-Konvention; Punkt-Separator statt SemVer '+' weil
# jede Build-Variante materielle Aenderungen mitbringt (Features /
# Bugfixes), nicht nur Metadata. XXX = ERSTE 3 hex chars (= 12 Bit)
# vom 8-char git short hash. Mehr Eindeutigkeit als der frueher
# 2-char-Hash, ohne im 20-byte Wire-Feld zu wachsen.
# Aenderung 2026-06-14 (User-Wunsch): Lesbarkeit + Kollisions-Margin.
# Aenderung 2026-06-20 (User-Wunsch): Prefix statt Suffix. Frueher
# git_hash[-3:] = letzten 3 vom 8-char short hash = Midfix vom Vollhash.
# Mit git_hash[:3] (Prefix) sehen wir die gleichen 3 Chars auch wenn
# wir das full hash sehen (24e52ef1... -> 24e). Direkt vergleichbar
# mit 'git log --oneline'.
firmware_version = "v1.16.2-DL9SAU.g{}".format(git_hash[:3] if git_hash != "nogit" else "000")

# Use CPPDEFINES with the value already quoted so SCons/gcc see it as a
# proper string literal. Plain BUILD_FLAGS '-DFOO=\\"bar\\"' gets the
# backslashes stripped before gcc parses it.
env.Append(CPPDEFINES=[
    ("FIRMWARE_VERSION", env.StringifyMacro(firmware_version)),
    ("FIRMWARE_BUILD_DATE", env.StringifyMacro(build_date)),
    ("FIRMWARE_BUILD_TIME", env.StringifyMacro(build_time)),
])

print("[DL9SAU] FIRMWARE_VERSION    = {}".format(firmware_version))
print("[DL9SAU] FIRMWARE_BUILD_DATE = {}".format(build_date))
print("[DL9SAU] FIRMWARE_BUILD_TIME = {}".format(build_time))
