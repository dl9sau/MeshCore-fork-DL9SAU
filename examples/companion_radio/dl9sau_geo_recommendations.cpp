// Geo -> MeshCore-scope recommendations.
// See header for behaviour. Bounding boxes are approximate (whole-degree
// minutes), good enough for "which scopes might apply at my location"
// hints; not for fine-grained jurisdictional decisions.

#include "dl9sau_geo_recommendations.h"
#include "NodePrefs.h"
#include <stdio.h>
#include <string.h>

namespace {

struct GeoRegion {
  const char* name;
  double      lat_min, lat_max, lon_min, lon_max;
  bool        has_bbox;        // false fuer position-unabhaengige Special-Scopes
  uint8_t     default_status;  // SCOPE_STATUS_* default (= state ohne sparse-Slot)
};

// Order matters for output: broadest first. The matcher walks the table
// in order and the printed list follows that order.
// Helper-Makros fuer Tabellen-Eintraege:
//   GEO(name, lat_min, lat_max, lon_min, lon_max)  — normaler Geo-Eintrag
//   NOGEO_PIN(name)                                — position-unabhaengig,
//                                                    default repeat=on, advert=off
//   NOGEO_OFF(name)                                — position-unabhaengig,
//                                                    default repeat=off (Sentinel)
#define GEO(N, A, B, C, D)   { N, A, B, C, D, true, 0 }
#define NOGEO_PIN(N)         { N, 0, 0, 0, 0, false, \
                               (uint8_t)(SCOPE_STATUS_REPEAT_ON | SCOPE_STATUS_ADVERT_OFF) }
#define NOGEO_OFF(N)         { N, 0, 0, 0, 0, false, \
                               (uint8_t)(SCOPE_STATUS_REPEAT_OFF | SCOPE_STATUS_ADVERT_OFF) }

static const GeoRegion regions[] = {
  // Top-level / continent
  GEO("europe", 35.0, 72.0, -25.0,  60.0),
  GEO("de",     47.27, 55.06, 5.87, 15.04),   // Germany bounding box

  // Aggregate "super-regions" (union of several Bundesländer, generous boxes)
  GEO("de-nord",  51.30, 55.06,  6.65, 14.42),  // HH, HB, SH, MV, NI
  GEO("de-ost",   50.16, 54.69,  9.87, 15.04),  // MV, BE, BB, SN, ST, TH
  GEO("de-sued",  47.27, 50.56,  7.51, 13.84),  // BY, BW, parts of HE/RP
  GEO("de-west",  49.11, 53.89,  5.87, 10.24),  // HE, NI, HB, NW, RP, SL
  GEO("de-mitte", 49.40, 53.04,  6.65, 12.66),  // HE, TH, ST + parts of NW/NI/BY/SN

  // Bundesländer (16) — ISO 3166-2
  GEO("de-bw", 47.53, 49.79,  7.51, 10.50),   // Baden-Württemberg
  GEO("de-by", 47.27, 50.56,  8.97, 13.84),   // Bayern
  GEO("de-be", 52.34, 52.68, 13.09, 13.76),   // Berlin (Stadtstaat)
  GEO("de-bb", 51.36, 53.56, 11.27, 14.77),   // Brandenburg
  GEO("de-hb", 53.01, 53.61,  8.48,  8.99),   // Bremen (Stadtstaat, vereinfacht inkl. Bremerhaven)
  GEO("de-hh", 53.39, 53.74,  8.42, 10.33),   // Hamburg (Stadtstaat)
  GEO("de-he", 49.40, 51.66,  7.77, 10.24),   // Hessen
  GEO("de-mv", 53.11, 54.69, 10.59, 14.42),   // Mecklenburg-Vorpommern
  GEO("de-ni", 51.30, 53.89,  6.65, 11.60),   // Niedersachsen
  GEO("de-nw", 50.32, 52.53,  5.87,  9.46),   // Nordrhein-Westfalen
  GEO("de-rp", 48.97, 50.94,  6.11,  8.51),   // Rheinland-Pfalz
  GEO("de-sl", 49.11, 49.64,  6.36,  7.40),   // Saarland
  GEO("de-sn", 50.16, 51.69, 11.87, 15.04),   // Sachsen
  GEO("de-st", 50.94, 53.04, 10.56, 13.19),   // Sachsen-Anhalt
  GEO("de-sh", 53.36, 55.06,  7.87, 11.31),   // Schleswig-Holstein
  GEO("de-th", 50.20, 51.65,  9.87, 12.66),   // Thüringen

  // Local specials (DL9SAU's two geo-fence regions, see chooseGeoFallbackScope)
  GEO("de-bebb",      51.36, 53.56, 11.27, 14.77),  // Berlin/Brandenburg bridge (=de-bb box)
  GEO("ostfriesland", 53.10, 53.80,  6.50,  8.50),

  // Special-Scopes (Wunschliste 11 Schritte 10-12): position-unabhaengig,
  // Default-Pin damit out-of-the-box aktiv. advert-off weil sie nicht als
  // Geo-Send-Fallback dienen koennen (haben keine Bbox).
  // - local / lokal:      single-hop; bei Repeat wird Scope zu local-discard
  //                       umgeschrieben, damit kein zweiter Repeater drueber
  //                       geht. Pakete mit hops > 0 werden verworfen.
  // - region / regional:  konfigurierbarer Hop-Cap (_prefs.region_hop_limit).
  //                       Scope bleibt unveraendert beim Repeat — andere
  //                       Repeater respektieren das Limit selbst.
  // - local-discard:      sentinel; default OFF, wird nie repeated. Markiert
  //                       das Ende der single-hop-local-Reichweite.
  NOGEO_PIN("local"),
  NOGEO_PIN("lokal"),
  NOGEO_PIN("region"),
  NOGEO_PIN("regional"),
  NOGEO_OFF("local-discard"),
};
#undef GEO
#undef NOGEO_PIN
#undef NOGEO_OFF
static const size_t REGION_COUNT = sizeof(regions) / sizeof(regions[0]);

// Berlin/Brandenburg has an explicit bridge-scope (#de-bebb) so a Berlin
// node listing #de-bb in addition would be misleading — Brandenburger
// stay among themselves on #de-bb and use #de-bebb to reach Berlin.
// No equivalent bridge-scope exists for Bremen <-> Niedersachsen or
// Hamburg <-> Niedersachsen/Schleswig-Holstein, so we let the surrounding
// Flächenstaat show up in those recommendations.
struct CityStateRule {
  const char* city_state;
  const char* skip;
};
static const CityStateRule city_rules[] = {
  { "de-be", "de-bb" },   // Berlin in Brandenburg-box -> suppress de-bb (de-bebb bridge)
};
static const size_t CITY_RULE_COUNT = sizeof(city_rules) / sizeof(city_rules[0]);

inline bool inside(const GeoRegion& r, double lat, double lon) {
  return lat >= r.lat_min && lat <= r.lat_max
      && lon >= r.lon_min && lon <= r.lon_max;
}

int find_idx(const char* name) {
  for (size_t i = 0; i < REGION_COUNT; i++) {
    if (strcmp(regions[i].name, name) == 0) return (int)i;
  }
  return -1;
}

} // namespace


void dl9sau_recommend_scopes(double lat, double lon, char* dest, size_t dest_size) {
  if (dest == NULL || dest_size == 0) return;
  dest[0] = 0;

  // Pass 1: bbox containment
  bool match[REGION_COUNT];
  for (size_t i = 0; i < REGION_COUNT; i++) {
    match[i] = inside(regions[i], lat, lon);
  }

  // Pass 2: apply city-state suppression rules
  for (size_t r = 0; r < CITY_RULE_COUNT; r++) {
    int cs_idx   = find_idx(city_rules[r].city_state);
    int skip_idx = find_idx(city_rules[r].skip);
    if (cs_idx >= 0 && skip_idx >= 0 && match[cs_idx]) {
      match[skip_idx] = false;
    }
  }

  // Pass 3: emit in table order (broadest -> most specific)
  bool first = true;
  size_t used = 0;
  for (size_t i = 0; i < REGION_COUNT; i++) {
    if (!match[i]) continue;
    size_t nlen = strlen(regions[i].name);
    size_t need = nlen + (first ? 0 : 2);
    if (used + need + 1 > dest_size) break;
    if (!first) {
      memcpy(&dest[used], ", ", 2);
      used += 2;
    }
    memcpy(&dest[used], regions[i].name, nlen);
    used += nlen;
    dest[used] = 0;
    first = false;
  }
}

size_t dl9sau_region_count() {
  return REGION_COUNT;
}

bool dl9sau_get_region(size_t idx,
                       const char** name_out,
                       double* lat_min, double* lat_max,
                       double* lon_min, double* lon_max) {
  if (idx >= REGION_COUNT) return false;
  if (name_out) *name_out = regions[idx].name;
  if (lat_min)  *lat_min  = regions[idx].lat_min;
  if (lat_max)  *lat_max  = regions[idx].lat_max;
  if (lon_min)  *lon_min  = regions[idx].lon_min;
  if (lon_max)  *lon_max  = regions[idx].lon_max;
  return true;
}

bool dl9sau_lookup_region_bbox(const char* name,
                               double* lat_min, double* lat_max,
                               double* lon_min, double* lon_max) {
  if (!name || !*name) return false;
  for (size_t i = 0; i < REGION_COUNT; i++) {
    if (strcmp(regions[i].name, name) == 0) {
      if (lat_min) *lat_min = regions[i].lat_min;
      if (lat_max) *lat_max = regions[i].lat_max;
      if (lon_min) *lon_min = regions[i].lon_min;
      if (lon_max) *lon_max = regions[i].lon_max;
      return true;
    }
  }
  return false;
}

int dl9sau_find_region_index(const char* name) {
  if (!name || !*name) return -1;
  for (size_t i = 0; i < REGION_COUNT; i++) {
    if (strcmp(regions[i].name, name) == 0) return (int)i;
  }
  return -1;
}

bool dl9sau_get_region_meta(size_t idx, bool* has_bbox, uint8_t* default_status) {
  if (idx >= REGION_COUNT) return false;
  if (has_bbox)       *has_bbox       = regions[idx].has_bbox;
  if (default_status) *default_status = regions[idx].default_status;
  return true;
}

void dl9sau_compute_name_hash(const char* name, uint8_t out[4]) {
  // FNV-1a 32-bit. Klein, schnell, ausreichende Distribution fuer
  // Region-Namen-Set. Kein crypto, nur Index-Lookup.
  uint32_t h = 0x811c9dc5u;
  if (name) {
    while (*name) {
      h ^= (uint8_t)(*name++);
      h *= 0x01000193u;
    }
  }
  out[0] = (uint8_t)(h & 0xFF);
  out[1] = (uint8_t)((h >> 8) & 0xFF);
  out[2] = (uint8_t)((h >> 16) & 0xFF);
  out[3] = (uint8_t)((h >> 24) & 0xFF);
}
