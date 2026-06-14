# NRF52 Companion-Radio Size-Audit -- 2026-06-14

Snapshot der Build-Sizes fuer alle 75 nrf52 companion_radio-Envs.
Ausgangspunkt fuer spaetere Vergleiche (Wachstum durch neue Features).

Build-Stand: branch `feat/nearly-real-repeater`, Head `47ec50c2`
(commit "Wunschliste 64 -- Display 2-Stage Sleep / Power-Off (CLOSED)").

## Verteilung

| Kategorie  | Anzahl | Bedeutung |
| ---------- | -----: | --------- |
| OK         |     19 | Flash < 95%, RAM < 90% -- Luft fuer weitere Features |
| TIGHT      |     20 | Flash >= 95% oder RAM >= 90% -- naechstes Feature kippt sie |
| OVERFLOW   |      3 | Schon ueber 100% Flash, bauen nicht |
| EARLY-FAIL |     29 | Variant-Ini-Bug (fehlende lib_deps), nicht durch unseren Code-Wachstum |

## OVERFLOW -- 3 Boards bauen nicht

```
Heltec_t096_companion_radio_ble       Flash 100.0% (696k/696k)   3 Slots ueber
Heltec_t1_companion_radio_ble         Flash 100.2% (697k/696k)   1.2k ueber
WioTrackerL1_companion_radio_usb      Flash 100.0% (692k/692k)   am Limit
```

WioTrackerL1 ist im DACH-Mesh sehr beliebt. Heltec T1/T096 sind
weitere Top-Picks (User interessiert sich aktuell fuer T096).
Diese drei werden wir gezielt retten muessen.

## TIGHT -- 20 Boards platzen beim naechsten Feature

| Env | Flash | Frei |
|---|---:|---:|
| GAT562_Mesh_Watch13_companion_radio_ble        | 99.5% | 3.5k |
| Heltec_t1_companion_radio_usb                  | 99.4% | 4.5k |
| GAT562_Mesh_Tracker_Pro_companion_radio_usb    | 99.4% | 4.5k |
| RAK_3401_companion_radio_usb                   | 99.3% | 5.0k |
| Heltec_t096_companion_radio_usb                | 99.2% | 5.6k |
| Heltec_t114_without_display_companion_radio_ble | 98.6% | 9.7k |
| Xiao_nrf52_companion_radio_ble                 | 98.5% | 11.4k |
| RAK_WisMesh_Tag_companion_radio_ble            | 97.8% | 15.5k |
| Heltec_t114_without_display_companion_radio_usb | 97.8% | 15.5k |
| ThinkNode_M6_companion_radio_ble               | 97.5% | 17.7k |
| SenseCap_Solar_companion_radio_ble             | 97.2% | 19.5k |
| R1Neo_companion_radio_usb                      | 96.9% | 22.0k |
| RAK_WisMesh_Tag_companion_radio_usb            | 96.7% | 23.0k |
| ProMicro_companion_radio_ble                   | 96.6% | 23.5k |
| ThinkNode_M6_companion_radio_usb               | 96.4% | 25.5k |
| Xiao_nrf52_companion_radio_usb                 | 96.4% | 24.0k |
| SenseCap_Solar_companion_radio_usb             | 96.4% | 24.5k |
| Heltec_mesh_solar_companion_radio_ble          | 95.7% | 29.5k |
| ThinkNode_M1_companion_radio_ble               | 95.7% | 30.4k |
| wio_wm1110_companion_radio_ble                 | 95.0% | 34.6k |

## OK -- 19 Boards mit Wachstums-Reserve

Top-Boards mit am meisten Headroom:

| Env | Flash | Frei |
|---|---:|---:|
| Meshtiny_companion_radio_ble                  | 79.8% | 160k |
| Meshtiny_companion_radio_usb                  | 79.1% | 166k |
| ikoka_handheld_nrf_e22_30dbm_096_companion_radio_usb | 86.3% | 108k |
| ikoka_handheld_nrf_e22_30dbm_096_companion_radio_ble | 87.0% | 102k |
| t1000e_companion_radio_usb                    | 88.6% |  79k |
| t1000e_companion_radio_ble                    | 89.4% |  73k |

Meshtiny + ikoka_handheld haben 80k-160k Reserve -- das sind unsere
"Power-Boards" wenn wir weiter Features einbauen ohne TIGHT-Boards
zu killen.

## EARLY-FAIL -- 29 Boards mit Variant-Ini-Fehlern

Zwei Klassen, kein Sizing-Thema, nicht durch unseren Code-Wachstum:

- **ikoka_nano + ikoka_stick** (14 Builds): `fatal error: base64.hpp:
  No such file or directory` -- fehlende lib_deps in den Variant-Inis.
- **Minewsemi_me25ls01 + R1Neo (BLE) + WioTrackerL1 (BLE) ** (4
  Builds): `fatal error: Adafruit_GFX.h: No such file or directory`
  -- selbes Problem mit GFX-lib.

Beides ist Upstream-Bug in den Variant-Inis. Nicht in dieser Audit-
Analyse drin.

## RAM-Bild allgemein

RAM-Auslastung ueberall 58-63%. ~85-95k frei. Wir koennen problemlos
weitere Counter, Buffer und Tabellen einbauen ohne RAM-Stress.

## Konsequenzen fuer Feature-Planung

1. **Flash ist der Engpass**, nicht RAM. Jedes neue Feature darf max.
   ein paar kB Code mitbringen damit nicht weitere TIGHT-Boards
   kippen.

2. **20 TIGHT-Boards mit < 30k Frei** -- davon mehrere mit < 5k.
   Wunschliste 60 (LoRa-Error-Counter) + 61 (V/I-Telemetrie)
   muessen SLIM gebaut werden.

3. **Strings sind ein dicker Brocken**. Hilfetexte, OK-Antworten,
   Error-Messages summieren sich. Wenn wir spaeter en/de
   Bilingual machen wollen, wird das ein Showstopper. User-Idee
   2026-06-14: String-Konstanten + ggf. komprimierter External-
   Resource-Block auf LittleFS. Separater Optimierungs-Pass.

4. **Variant-Ini-Bugs sind Upstream**. Wenn die WioTrackerL1-BLE-
   oder ikoka-Builds wieder bauen, muss das in dem Variant-Ini
   geheilt werden (lib_deps ergaenzen).

5. **Reserven liegen bei Meshtiny + ikoka_handheld**. Sind die
   "Power-Plattformen" mit 80k-160k Reserve. Falls jemand
   dort neue grosse Features will (z.B. komplette
   English/German-Localization), kann das dort zuerst landen.
