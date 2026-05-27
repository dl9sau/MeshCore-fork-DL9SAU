// Geo -> MeshCore-scope recommendations.
// See header for behaviour. Bounding boxes are approximate (whole-degree
// minutes), good enough for "which scopes might apply at my location"
// hints; not for fine-grained jurisdictional decisions.

#include "dl9sau_geo_recommendations.h"
#include <stdio.h>
#include <string.h>

namespace {

struct GeoRegion {
  const char* name;
  double lat_min, lat_max, lon_min, lon_max;
};

// Order matters for output: broadest first. The matcher walks the table
// in order and the printed list follows that order.
static const GeoRegion regions[] = {
  // Top-level / continent
  { "europe", 35.0, 72.0, -25.0,  60.0 },
  { "de",     47.27, 55.06, 5.87, 15.04 },   // Germany bounding box

  // Aggregate "super-regions" (union of several Bundesländer, generous boxes)
  { "de-nord",  51.30, 55.06,  6.65, 14.42 },  // HH, HB, SH, MV, NI
  { "de-ost",   50.16, 54.69,  9.87, 15.04 },  // MV, BE, BB, SN, ST, TH
  { "de-sued",  47.27, 50.56,  7.51, 13.84 },  // BY, BW, parts of HE/RP
  { "de-west",  49.11, 53.89,  5.87, 10.24 },  // HE, NI, HB, NW, RP, SL
  { "de-mitte", 49.40, 53.04,  6.65, 12.66 },  // HE, TH, ST + parts of NW/NI/BY/SN

  // Bundesländer (16) — ISO 3166-2
  { "de-bw", 47.53, 49.79,  7.51, 10.50 },   // Baden-Württemberg
  { "de-by", 47.27, 50.56,  8.97, 13.84 },   // Bayern
  { "de-be", 52.34, 52.68, 13.09, 13.76 },   // Berlin (Stadtstaat)
  { "de-bb", 51.36, 53.56, 11.27, 14.77 },   // Brandenburg
  { "de-hb", 53.01, 53.61,  8.48,  8.99 },   // Bremen (Stadtstaat, vereinfacht inkl. Bremerhaven)
  { "de-hh", 53.39, 53.74,  8.42, 10.33 },   // Hamburg (Stadtstaat)
  { "de-he", 49.40, 51.66,  7.77, 10.24 },   // Hessen
  { "de-mv", 53.11, 54.69, 10.59, 14.42 },   // Mecklenburg-Vorpommern
  { "de-ni", 51.30, 53.89,  6.65, 11.60 },   // Niedersachsen
  { "de-nw", 50.32, 52.53,  5.87,  9.46 },   // Nordrhein-Westfalen
  { "de-rp", 48.97, 50.94,  6.11,  8.51 },   // Rheinland-Pfalz
  { "de-sl", 49.11, 49.64,  6.36,  7.40 },   // Saarland
  { "de-sn", 50.16, 51.69, 11.87, 15.04 },   // Sachsen
  { "de-st", 50.94, 53.04, 10.56, 13.19 },   // Sachsen-Anhalt
  { "de-sh", 53.36, 55.06,  7.87, 11.31 },   // Schleswig-Holstein
  { "de-th", 50.20, 51.65,  9.87, 12.66 },   // Thüringen

  // Local specials (DL9SAU's two geo-fence regions, see chooseGeoFallbackScope)
  { "de-bebb",      51.36, 53.56, 11.27, 14.77 },  // Berlin/Brandenburg bridge (=de-bb box)
  { "ostfriesland", 53.10, 53.80,  6.50,  8.50 },
};
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
