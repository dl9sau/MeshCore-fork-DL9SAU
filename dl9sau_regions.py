"""
PlatformIO pre-build script: scrape the German MeshCore community wiki for
the canonical region list, parse it, and write a C header consumed by the
firmware to identify scopes in incoming packets.

Sources (community-maintained):
  https://meshcore-de.fyi/meshcore:allgemeines:regions:basis
  https://meshcore-de.fyi/meshcore:allgemeines:regions:reale-regions-in-repeatern

Behaviour:
  * Day-cache in .dl9sau-cache/regions-YYYY-MM-DD.json — fetched once per day.
  * On network failure: fall back to the most recent cached file (any date).
  * On no cache at all: fall back to a tiny hardcoded list (#bebb, #ostfriesland).
  * Generates generated/dl9sau_regions.h with:
      #define DL9SAU_REGION_COUNT N
      struct dl9sau_region { const char* name; };
      static const struct dl9sau_region dl9sau_regions[N] = { ... };
  * The directory generated/ is git-ignored.
"""
Import("env")
import os
import re
import json
import urllib.request
import urllib.error
from datetime import datetime
from html.parser import HTMLParser


PROJECT_DIR = env["PROJECT_DIR"]
CACHE_DIR = os.path.join(PROJECT_DIR, ".dl9sau-cache")
OUT_DIR = os.path.join(PROJECT_DIR, "generated")
OUT_FILE = os.path.join(OUT_DIR, "dl9sau_regions.h")

URL_BASE = "https://meshcore-de.fyi/meshcore:allgemeines:regions:basis"
URL_SUB = "https://meshcore-de.fyi/meshcore:allgemeines:regions:reale-regions-in-repeatern"

FETCH_TIMEOUT = 10  # seconds, total per URL

# Names must be lower-case ASCII letters / digits / '-', up to 30 chars.
# This filters out free-text lines like "Stand: 09.03.2026" or
# "Flood größtenteils inaktiv, ..." that share the <pre> blocks.
NAME_RE = re.compile(r"^[a-z0-9-]{1,30}$")

# Always added to the parsed list (deduplicated). Keeps the firmware aware
# of regions we use locally even if they aren't (or no longer) documented
# on the wiki — e.g. our chooseGeoFallbackScope() in MyMesh.cpp uses these.
EXTRA_REGIONS = ["bebb", "ostfriesland"]

# Fallback if nothing else is reachable.
FALLBACK_REGIONS = list(EXTRA_REGIONS)


def _today_str():
    return datetime.now().strftime("%Y-%m-%d")


def _cache_path_for_today():
    return os.path.join(CACHE_DIR, "regions-{}.json".format(_today_str()))


def _latest_cache_path():
    if not os.path.isdir(CACHE_DIR):
        return None
    files = [f for f in os.listdir(CACHE_DIR) if f.startswith("regions-") and f.endswith(".json")]
    if not files:
        return None
    files.sort(reverse=True)
    return os.path.join(CACHE_DIR, files[0])


def _http_get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "MeshCore-DL9SAU-build/1.0"})
    with urllib.request.urlopen(req, timeout=FETCH_TIMEOUT) as resp:
        return resp.read().decode("utf-8", errors="replace")


def _strip_em_u(html):
    """Pull region names out of basis page from <em class="u">name</em>."""
    return re.findall(r'<em class="u">([a-z0-9-]+)</em>', html)


class _PreCodeExtractor(HTMLParser):
    """Collects the text content of every <pre class="code"> block."""
    def __init__(self):
        super().__init__()
        self._in_pre = False
        self._buf = []
        self.blocks = []

    def handle_starttag(self, tag, attrs):
        if tag == "pre":
            cls = dict(attrs).get("class", "")
            if "code" in cls.split():
                self._in_pre = True
                self._buf = []

    def handle_endtag(self, tag):
        if tag == "pre" and self._in_pre:
            self._in_pre = False
            self.blocks.append("".join(self._buf))

    def handle_data(self, data):
        if self._in_pre:
            self._buf.append(data)


def _parse_sub_blocks(html):
    """Extract region names from the <pre class="code"> blocks. Each line is
    'name  // optional comment' OR free-text noise we skip."""
    p = _PreCodeExtractor()
    p.feed(html)
    names = []
    for block in p.blocks:
        for raw_line in block.splitlines():
            # Strip comment if present
            line = raw_line.split("//", 1)[0].strip()
            if not line:
                continue
            if NAME_RE.match(line):
                names.append(line)
    return names


def _parse_basis(html):
    names = list(_strip_em_u(html))
    # Aggregate regions are written as "Region <em class="u">de-nord</em>"
    # in strong-marker rows — _strip_em_u already grabs them.
    return names


def _merge(*lists):
    """Concatenate, preserve order, drop duplicates."""
    seen = set()
    out = []
    for L in lists:
        for name in L:
            if name in seen:
                continue
            seen.add(name)
            out.append(name)
    return out


def _fetch_and_parse():
    """Returns (region_list, source_info_dict) or raises on network failure.
    EXTRA_REGIONS are appended even if missing on the wiki."""
    basis_html = _http_get(URL_BASE)
    sub_html = _http_get(URL_SUB)
    basis = _parse_basis(basis_html)
    sub = _parse_sub_blocks(sub_html)
    merged = _merge(basis, sub, EXTRA_REGIONS)
    return merged, {"fetched_at": datetime.now().isoformat(timespec="seconds")}


def _load_regions():
    """Returns (region_list, source_label)."""
    cache_today = _cache_path_for_today()
    if os.path.isfile(cache_today):
        with open(cache_today, "r", encoding="utf-8") as f:
            data = json.load(f)
        # Apply EXTRA_REGIONS late as well — set could have been edited
        # since the cache was written.
        return _merge(data["regions"], EXTRA_REGIONS), "cache (today, {})".format(_today_str())

    try:
        regions, info = _fetch_and_parse()
        os.makedirs(CACHE_DIR, exist_ok=True)
        with open(cache_today, "w", encoding="utf-8") as f:
            json.dump({"regions": regions, "fetched_at": info["fetched_at"]}, f, indent=2)
        return regions, "fetched (cached as {})".format(os.path.basename(cache_today))
    except (urllib.error.URLError, OSError, ValueError) as e:
        print("[DL9SAU regions] WARN: live fetch failed ({}). Falling back to cache.".format(e))

    latest = _latest_cache_path()
    if latest:
        with open(latest, "r", encoding="utf-8") as f:
            data = json.load(f)
        return _merge(data["regions"], EXTRA_REGIONS), "stale cache ({})".format(os.path.basename(latest))

    print("[DL9SAU regions] WARN: no cache available — using hardcoded fallback.")
    return FALLBACK_REGIONS, "hardcoded fallback"


def _write_header(regions, source_label):
    os.makedirs(OUT_DIR, exist_ok=True)
    # Make sure generated/ is git-ignored (silent if already)
    gi = os.path.join(OUT_DIR, ".gitignore")
    if not os.path.isfile(gi):
        with open(gi, "w") as f:
            f.write("*\n!.gitignore\n")
    with open(OUT_FILE, "w", encoding="utf-8") as f:
        f.write("// AUTO-GENERATED by dl9sau_regions.py — do not edit by hand.\n")
        f.write("// Source: {}\n".format(source_label))
        f.write("// Generated: {}\n".format(datetime.now().isoformat(timespec="seconds")))
        f.write("#pragma once\n\n")
        f.write("#define DL9SAU_REGION_COUNT {}\n".format(len(regions)))
        f.write("struct dl9sau_region {\n")
        f.write("  const char* name;   // hashtag name WITHOUT leading '#'\n")
        f.write("};\n\n")
        f.write("static const struct dl9sau_region dl9sau_regions[DL9SAU_REGION_COUNT] = {\n")
        for name in regions:
            # names are validated ASCII (NAME_RE), no further escaping needed
            f.write('  {{ "{}" }},\n'.format(name))
        f.write("};\n")


regions, source_label = _load_regions()
_write_header(regions, source_label)

# Expose the include directory to the build, so a source file can do
# `#include "dl9sau_regions.h"` without knowing the absolute path.
env.Append(CPPPATH=[OUT_DIR])

print("[DL9SAU regions] loaded {} regions from {}".format(len(regions), source_label))
print("[DL9SAU regions] header: {}".format(os.path.relpath(OUT_FILE, PROJECT_DIR)))
