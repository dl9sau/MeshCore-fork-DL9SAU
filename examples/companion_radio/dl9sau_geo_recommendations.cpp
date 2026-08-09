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
  bool        is_alias;        // true = Zusatz-Bbox fuer schon existierenden Namen
                               //        (Multi-Rectangle fuer L-foermige Regionen);
                               //        wird in Registry/Key-Cache uebersprungen.
};

// Order matters for output: broadest first. The matcher walks the table
// in order and the printed list follows that order.
// Helper-Makros fuer Tabellen-Eintraege:
//   GEO(name, lat_min, lat_max, lon_min, lon_max)        — Geo-Eintrag
//   GEO_ALIAS(name, lat_min, lat_max, lon_min, lon_max)  — zusaetzliche Bbox
//                                                          fuer schon vorhandenen
//                                                          Namen (Multi-Rectangle)
//   NOGEO_PIN(name)                                      — position-unabhaengig,
//                                                          default repeat=on
//   NOGEO_OFF(name)                                      — position-unabhaengig,
//                                                          default repeat=off
#define GEO(N, A, B, C, D)        { N, A, B, C, D, true, 0, false }
#define GEO_ALIAS(N, A, B, C, D)  { N, A, B, C, D, true, 0, true  }
// NOGEO_PIN: KEIN ADVERT_OFF mehr (Wunschliste 14). chooseGeoFallbackScope
// ueberspringt no-bbox-Eintraege bereits per has_bbox-Filter, das
// ADVERT_OFF-Bit waere also redundant gewesen und nur kosmetisch
// irritierend in 'scope list' ([RP!] -> [RP]). User kann diese Scopes
// weiterhin explizit als 'scope advert bake/default/override' nutzen --
// das war auch vorher schon erlaubt (kein Filter), jetzt aber konsistent
// im Display.
#define NOGEO_PIN(N)              { N, 0, 0, 0, 0, false, \
                                    (uint8_t)SCOPE_STATUS_REPEAT_ON, false }
// NOGEO_OFF: Sentinel mit repeat=off + advert=off. ADVERT_OFF bleibt als
// zweite Sicherung -- selbst wenn ein User local-discard versehentlich
// pinned, soll er nicht als eigener Send-Scope verwendet werden.
#define NOGEO_OFF(N)              { N, 0, 0, 0, 0, false, \
                                    (uint8_t)(SCOPE_STATUS_REPEAT_OFF | SCOPE_STATUS_ADVERT_OFF), false }

static const GeoRegion regions[] = {
  // Top-level / continent
  GEO("europe", 35.0, 72.0, -25.0,  60.0),
  GEO("de",     47.27, 55.06, 5.87, 15.04),   // Germany bounding box

  // Aggregate "super-regions" (union of several Bundesländer, generous boxes)
  // Sued-Grenze 51.30 -> 52.30 (Feedback 2026-07-04): Halle-Region
  // (~51.5N) und Sued-Sachsen-Anhalt gehoerten faelschlich zu 'Nord'.
  // Hannover ~52.4 bleibt knapp drin, Osnabrueck ~52.28 faellt raus.
  GEO("de-nord",  52.30, 55.06,  6.65, 14.42),  // HH, HB, SH, MV, NI
  GEO("de-ost",   50.16, 54.69,  9.87, 15.04),  // MV, BE, BB, SN, ST, TH
  GEO("de-sued",  47.27, 50.56,  7.51, 13.84),  // BY, BW, parts of HE/RP
  GEO("de-west",  49.11, 53.89,  5.87, 10.24),  // HE, NI, HB, NW, RP, SL
  // Nord-Grenze 53.04 -> 52.40 (User 2026-08-09): die alte Nordkante war nur
  // wegen Sachsen-Anhalt (Altmark) so hoch und zog die NW-Ecke der Box ueber
  // Bremen (53.03N/8.60E) -- Bremen ist Nord/West, nicht Mitte. ST gehoert lt.
  // Community (meshcore-de.fyi) ohnehin zu de-ost (dort bereits gelistet), nicht
  // de-mitte. Hannover (52.37) bleibt bewusst drin (User-Wunsch).
  GEO("de-mitte", 49.40, 52.40,  6.65, 12.66),  // HE, TH + parts of NI/NW/BY/SN

  // Bundesländer (16) — ISO 3166-2
  // Bboxes wurden bewusst gegenueber den OSM-Rechtecken in zwei Faellen
  // angepasst (Test-Bericht 2026-05-30):
  //   - de-sh: lon_min 7.87 -> 9.00 (Mainland). Die OSM-Box war so weit
  //     westlich dass Jever (53.575,7.90 in NI/Ostfriesland) sie traf.
  //     Sylt + Nordfriesische Inseln werden ueber GEO_ALIAS unten
  //     separat abgedeckt (Dedup-Logik in Pass 3).
  //   - de-hh: lon_min 8.42 -> 9.70. Vorher fielen Bremerhaven (8.58, HB!),
  //     Stade (9.48, NI) in die Hamburg-Bbox.
  //   - de-mv: lon_min 10.59 -> 11.00. Vorher fiel Luebeck (10.69, SH) in
  //     die MV-Bbox. Side-effect: MV-Salient Boizenburg (10.72) faellt
  //     raus und wird nur ueber de-nord erreicht -- akzeptabel.
  //   - de-bb: lat_max 53.56 -> 53.20. Vorher fielen MV-Suedstaedte
  //     (Parchim 53.43, Plau 53.46, Ludwigslust 53.32) in die BB-Bbox.
  GEO("de-bw", 47.53, 49.79,  7.51, 10.50),   // Baden-Württemberg
  GEO("de-by", 47.27, 50.56,  8.97, 13.84),   // Bayern
  GEO("de-be", 52.34, 52.68, 13.09, 13.76),   // Berlin (Stadtstaat)
  GEO("de-bb", 51.36, 53.20, 11.27, 14.77),   // Brandenburg (lat_max gegenueber OSM verkleinert)
  GEO("de-hb", 53.01, 53.61,  8.48,  8.99),   // Bremen (Stadtstaat, vereinfacht inkl. Bremerhaven)
  GEO("de-hh", 53.39, 53.74,  9.70, 10.33),   // Hamburg (Stadtstaat, lon_min gegenueber OSM verkleinert)
  GEO("de-he", 49.40, 51.66,  7.77, 10.24),   // Hessen
  GEO("de-mv", 53.11, 54.69, 11.00, 14.42),   // Mecklenburg-Vorpommern (lon_min gegenueber OSM verkleinert)
  GEO("de-ni", 51.30, 53.89,  6.65, 11.60),   // Niedersachsen
  GEO("de-nw", 50.32, 52.53,  5.87,  9.46),   // Nordrhein-Westfalen
  GEO("de-rp", 48.97, 50.94,  6.11,  8.51),   // Rheinland-Pfalz
  GEO("de-sl", 49.11, 49.64,  6.36,  7.40),   // Saarland
  GEO("de-sn", 50.16, 51.69, 11.87, 15.04),   // Sachsen
  GEO("de-st", 50.94, 53.04, 10.56, 13.19),   // Sachsen-Anhalt
  GEO("de-sh", 53.36, 55.06,  9.00, 11.00),   // Schleswig-Holstein Mainland (s. Hinweis oben)
  GEO_ALIAS("de-sh", 54.45, 55.20, 8.30, 9.00),  // SH Nordfriesische Inseln (Sylt/Foehr/Amrum)
  GEO("de-th", 50.20, 51.65,  9.87, 12.66),   // Thüringen

  // Local specials (DL9SAU's two geo-fence regions, see chooseGeoFallbackScope)
  // West-Grenze 11.27 -> 12.40 und Sued-Grenze 51.36 -> 51.80
  // (Feedback 2026-07-04): Halle (51.5N, 11.97E) ist Sachsen-Anhalt,
  // nicht Berlin/Brandenburg. Berlin (13.09-13.76E, >52.34N) bleibt
  // komplett drin; Cottbus ~51.75N knapp drin.
  GEO("de-bebb",      51.80, 53.56, 12.40, 14.77),  // Berlin/Brandenburg bridge
  GEO("ostfriesland", 52.95, 53.80,  6.50,  8.50),   // Sued-Grenze 53.10 -> 52.95 (Papenburg + ~10km Margin, User-Wunsch 2026-06-05)

  // Special-Scopes (Wunschliste 11 Schritte 10-12): position-unabhaengig,
  // Default-Pin damit out-of-the-box aktiv. advert-off weil sie nicht als
  // Geo-Send-Fallback dienen koennen (haben keine Bbox).
  // - local / lokal:      single-hop; bei Repeat wird Scope zu local-discard
  //                       umgeschrieben, damit kein zweiter Repeater drueber
  //                       geht. Pakete mit hops > 0 werden verworfen.
  // - region / regional:  konfigurierbarer Hop-Cap (_prefs.flood_max_scope_region).
  //                       Scope bleibt unveraendert beim Repeat — andere
  //                       Repeater respektieren das Limit selbst.
  // - local-discard:      sentinel; default OFF, wird nie repeated. Markiert
  //                       das Ende der single-hop-local-Reichweite.
  // - direct / direkt / norepeat / no-repeat (Wunschliste 38, 2026-06-03):
  //                       User-adressierbare No-Repeat-Sentinels. Wer eine
  //                       Nachricht mit einem dieser Scopes sendet, signali-
  //                       siert ALLEN Repeatern "nicht weiterleiten". Hart
  //                       geblockt in allowPacketForward auch im 'repeat
  //                       all'-Modus.
  NOGEO_PIN("local"),
  NOGEO_PIN("lokal"),
  NOGEO_PIN("region"),
  NOGEO_PIN("regional"),
  NOGEO_OFF("local-discard"),
  NOGEO_OFF("direct"),
  NOGEO_OFF("direkt"),
  NOGEO_OFF("norepeat"),
  NOGEO_OFF("no-repeat"),
};
#undef GEO
#undef GEO_ALIAS
#undef NOGEO_PIN
#undef NOGEO_OFF
static const size_t REGION_COUNT = sizeof(regions) / sizeof(regions[0]);

// Suppression-Regeln: wenn der Knoten in der Bbox von 'matched' liegt,
// wird 'skip' aus der Empfehlung ausgeblendet. Zwei Anwendungsfaelle:
//
//   1) Stadtstaaten-Suppression: HH/HB/BE-Buerger sehen den
//      umgebenden Flaechenstaat nicht. Berliner verwenden de-be
//      untereinander und de-bebb als Bruecke zu Brandenburg;
//      analog HH/HB gegenueber NI.
//   2) Bayern verdraengt Aggregat de-ost: die OSM-Bbox catcht die
//      noerdlichen ~40 km von Bayern wegen lat_min an der SN/TH-
//      Suedgrenze (50.16) waehrend BY-lat_max bei 50.56 liegt --
//      Bayer soll nicht de-ost empfohlen bekommen (Test-Bericht
//      2026-05-30).
struct CityStateRule {
  const char* city_state;   // wenn diese Bbox passt
  const char* skip;         // wird die hier genannte Region ausgeblendet
};
static const CityStateRule city_rules[] = {
  { "de-be", "de-bb" },   // Berlin -> suppress de-bb (de-bebb bridge stattdessen)
  // Niedersachsen ist das groesste Bundesland und #de-ni traegt entsprechend
  // viel Traffic. HH/HB-Knoten in dieser Suppression-Liste mindern das,
  // ohne die HH/HB-Repeater-Reichweite einzuschraenken (App zeigt nur die
  // Empfehlung; was geroutet wird steuert die scope-Registry).
  { "de-hh", "de-ni" },   // Hamburg -> suppress de-ni (NI umschliesst HH)
  { "de-hb", "de-ni" },   // Bremen  -> suppress de-ni
                          // (TODO Test-Bericht 2026-05-30: Bremen ist sehr
                          //  klein -- ggf. Regel rausnehmen wenn Praxis zeigt
                          //  dass HB-Knoten doch de-ni-Repeats brauchen.)
  { "de-by", "de-ost" },  // Bayern (noerdl. Teile) im de-ost-Aggregat -> suppress
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

  // Pass 2: apply suppression rules.
  // Robust gegen Multi-Rectangle: ein Name kann mehrere Eintraege haben
  // (z.B. de-sh Mainland + Inseln). Wir pruefen "irgendein Eintrag mit
  // diesem Namen matched" und blenden ALLE Eintraege des skip-Namens aus.
  for (size_t r = 0; r < CITY_RULE_COUNT; r++) {
    bool cs_matched = false;
    for (size_t i = 0; i < REGION_COUNT; i++) {
      if (match[i] && strcmp(regions[i].name, city_rules[r].city_state) == 0) {
        cs_matched = true;
        break;
      }
    }
    if (!cs_matched) continue;
    for (size_t i = 0; i < REGION_COUNT; i++) {
      if (strcmp(regions[i].name, city_rules[r].skip) == 0) {
        match[i] = false;
      }
    }
  }

  // Pass 3: emit in table order (broadest -> most specific), dedup by name.
  // Erlaubt mehrere Bbox-Eintraege unter gleichem Namen (Multi-Rectangle
  // fuer L-foermige Regionen wie de-sh Mainland + Nordfriesische Inseln).
  bool first = true;
  size_t used = 0;
  for (size_t i = 0; i < REGION_COUNT; i++) {
    if (!match[i]) continue;
    // dup-check gegen alle frueheren matches
    bool dup = false;
    for (size_t j = 0; j < i; j++) {
      if (match[j] && strcmp(regions[i].name, regions[j].name) == 0) {
        dup = true;
        break;
      }
    }
    if (dup) continue;
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

bool dl9sau_is_alias(size_t idx) {
  if (idx >= REGION_COUNT) return false;
  return regions[idx].is_alias;
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
