"""
PlatformIO pre-build script: stamp the companion_radio firmware with the
current git short-hash and today's build date.

  FIRMWARE_VERSION  -> "v15-DL9SAU-<8charhash>"   (fits 20-byte wire field)
  FIRMWARE_BUILD_DATE -> "DD Mon YYYY"            (fits 12-byte wire field)

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
# Format: "v1.15.0gitXX-DL9SAU" (19 chars). XX = last 2 chars of the
# 8-char git short hash. Sacrifices uniqueness for readability of the
# full upstream version + DL9SAU tag in the 20-byte wire field.
firmware_version = "v1.15.0git{}-DL9SAU".format(git_hash[-2:] if git_hash != "nogit" else "00")

# Use CPPDEFINES with the value already quoted so SCons/gcc see it as a
# proper string literal. Plain BUILD_FLAGS '-DFOO=\\"bar\\"' gets the
# backslashes stripped before gcc parses it.
env.Append(CPPDEFINES=[
    ("FIRMWARE_VERSION", env.StringifyMacro(firmware_version)),
    ("FIRMWARE_BUILD_DATE", env.StringifyMacro(build_date)),
])

print("[DL9SAU] FIRMWARE_VERSION    = {}".format(firmware_version))
print("[DL9SAU] FIRMWARE_BUILD_DATE = {}".format(build_date))
