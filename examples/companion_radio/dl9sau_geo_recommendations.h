#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Compute a comma-separated list of MeshCore region/scope names that
// reasonably apply to the given GPS position. Walks a hardcoded table of
// bounding boxes (16 Bundesländer + 5 aggregates + 2 top-level + 2 local
// specials). Output is ordered from broadest to most specific:
//
//   europe, de, de-ost, de-be, de-bebb
//
// City-state suppression rules: when the node sits inside a city-state
// box (de-be / de-hb / de-hh), the surrounding Flächenstaat is NOT
// listed (Brandenburger use de-bb among themselves and de-bebb to talk
// to Berliners — listing de-bb for a Berlin-resident node would be
// misleading).
//
// `dest` is written as a null-terminated string up to `dest_size` bytes.
// Empty output if no box matches.
void dl9sau_recommend_scopes(double lat, double lon, char* dest, size_t dest_size);

// Read-only Zugriff auf die eingebaute Region-Tabelle ('scope regions'
// CLI). Lat/Lon in Dezimalgrad (positiv = N / E).
size_t dl9sau_region_count();
bool   dl9sau_get_region(size_t idx,
                         const char** name_out,
                         double* lat_min, double* lat_max,
                         double* lon_min, double* lon_max);

// Lookup-Helper: schreibt Bbox-Koordinaten in die out-Parameter wenn
// 'name' (case-sensitive, ohne '#') in der Region-Tabelle gefunden wird.
// Returns true bei Treffer, false sonst. Erlaubt 'scope add <name>'
// (ohne explizites geo-Argument) automatisch die Default-Bbox aus der
// Build-in-Tabelle zu uebernehmen.
bool dl9sau_lookup_region_bbox(const char* name,
                               double* lat_min, double* lat_max,
                               double* lon_min, double* lon_max);

// Index-Lookup: Returns position (0..count-1) der Region in der internen
// Tabelle, oder -1 wenn name nicht gefunden. Praktisch wenn die Position
// gebraucht wird (z.B. zum Iterieren beider Storages parallel).
int  dl9sau_find_region_index(const char* name);

// FNV-1a 32-bit Hash ueber den Namen (case-sensitive). Wird vom Scope-
// Architektur-Pivot (Wunschliste 11) verwendet um non-default Status-
// Eintraege in scope_buildin_status[] zu indexieren — dadurch ueberleben
// User-Customisierungen auch Reorder/Insert/Remove in der Build-in-
// Tabelle. Schreibt die 4 Hash-Bytes (Little-Endian) in out[0..3].
void dl9sau_compute_name_hash(const char* name, uint8_t out[4]);

// Liest Meta-Felder eines Build-in-Eintrags: has_bbox (false fuer
// position-unabhaengige Special-Scopes) und default_status (SCOPE_STATUS_*
// bei abwesendem sparse-Slot in scope_buildin_status[]).
bool dl9sau_get_region_meta(size_t idx, bool* has_bbox, uint8_t* default_status);
