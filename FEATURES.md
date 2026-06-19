# Features dieses Forks

Validierte und in der Praxis genutzte Erweiterungen gegenüber dem Original-MeshCore.
Die einzelnen Implementierungs-Details und Commit-Verläufe stehen im git log.

## Companion-Mode (Smartphone-App + Tracker)

### Zeit-gesteuerte Befehle: `at` und `cron`

Tracker führt Aktionen zu festen Zeitpunkten aus, ohne dass die App ständig
verbunden sein muss.

```
at +5m advert                  Einmaliger Befehl in 5 Minuten
at 22:00 reboot                Einmaliger Befehl zu absoluter Uhrzeit
at list                        Aktive Einmal-Aufträge anzeigen

cron 10m gps sync              Alle 10 Minuten GPS-Sync
cron Wd 08:00 advert            Montag bis Freitag um 08:00 Advert senden
cron * * uptime                 Jede Minute jeden Tag uptime senden
cron list                       Wiederkehrende Aufträge anzeigen
cron suspend / resume           Alle Aufträge pausieren / fortsetzen
```

Wochentag-Kurzformen: `Wd` (Mo-Fr), `We` (Sa+So), `Wk` (alle 7 Tage), `*` (alle 7 Tage).
Cron-Aufträge überleben einen Neustart, einmalige `at`-Aufträge nicht.

### Lokaler Konfigurations-Chat `$companion`

Eingaben in den Chat-Kanal `$companion` werden lokal vom Tracker verarbeitet —
nichts geht über Funk raus. Damit ist die komplette CLI direkt in der App
benutzbar.

### Hilfe-System

`help` zeigt alle Themen, `help <thema>` zeigt Details mit Beispielen.

### Automatische Zeitzone

Tracker erkennt EU-Sommerzeit automatisch, oder per Pref auf eine feste
Zeitzone setzbar. `at`/`cron` rechnen in lokaler Zeit.

## Stromspar-Mechanismen

### Bluetooth-Schlaf-Zyklus (ESP32)

Bluetooth schläft automatisch wenn keine App verbunden ist. Auf Heltec Wireless
Tracker spart das bis zu 80 mA. Cycle: 5 Minuten Boot-Phase, dann 40s Schlaf
plus 20s Listen-Phase. Eingehende Direkt-Nachrichten oder Admin-Befehle
wecken Bluetooth für 5 Minuten auf.

(Auf NRF52 nicht aktiv — Bluetooth ist dort hardware-bedingt schon im µA-Bereich
im Leerlauf.)

### Empfänger-Abschaltung (RX-Disable)

Pref `rx_disabled` schaltet den LoRa-Empfangsteil komplett ab. Spart auf
SenseCAP T1000-E etwa 40 mA. Kurze Aufwach-Phasen bei jedem Sendevorgang
für Bestätigungs-Empfang. Nur im Companion-Mode (nicht im Repeater-Mode).

### GPS-Schlafmodus

Pref `gps_lead_secs` steuert wie viele Sekunden vor dem nächsten Advert das
GPS aufwacht um einen Fix zu bekommen. Default 5 Minuten — bei 15-Minuten-
Adverts ist GPS dann nur zu einem Drittel der Zeit aktiv.

### USB-Loss-Shutdown

Pref `usb_loss_shutdown_min` — Tracker fährt selbst herunter wenn USB-Strom
für N Minuten weg ist. Anwendungsfall: Tracker im Auto an Bord-Stromversorgung,
Zündung aus, nach z.B. 1 Minute sauberer Shutdown.

### Akku-Schutz

Pref `batt_chemistry` (lipo/lifepo4) plus `batt_min_mv` — Tracker schaltet
bei Unterspannung sauber ab. 3-Stufen-Sample (10 Minuten normal, 10 Sekunden
im Verdachts-Burst) gegen falsche Auslösung durch Sendespitzen.

### Phantom-Wake-Filter (T1000-E)

Nach `shutdown` blockiert ein persistenter Marker im Flash unbeabsichtigte
Reaktivierungen (Watchdog-Trigger, Spannungs-Schwankungen, USB-Wackel).
Tracker bleibt verlässlich aus bis User USB stabil einsteckt oder Button
drückt.

## Repeater- und Filter-Funktionen

### Differenzierte Reichweiten-Begrenzung

Statt eines globalen Hop-Caps gibt es separate Werte für:

- `flood_max` — Standard für Chat-Nachrichten
- `flood_max_infra` — Repeater/Sensor/Room-Adverts
- `flood_max_req_resp` — REQ/RESP-Nachrichten
- `flood_max_unknown_chan` — Nachrichten auf unbekannten Kanälen
- `flood_max_unscoped_companions` — Companion-Adverts ohne Scope

Plus `flood.max.advert` als Alias auf `flood_max_infra` (für
Upstream-Kompatibilität).

### Filter

Sender-, Channel- und Text-Filter mit Wildcards. Blendet unerwünschte
Nachrichten lokal aus, ohne die Mesh-Weiterleitung zu beeinflussen.

### Scope-Konzept

Pakete mit Scope-Code im Header bleiben in regionalen Mesh-Bereichen statt
weltweit zu fluten. Tracker erkennt Scope-Code beim Empfang und entscheidet
ob er weiterleitet.

## Diagnose und Recovery

### Boot-Log

Nach jedem Reset wird der Grund im persistenten Log festgehalten:
COLD (Power-on), WARM (User-Reboot), WDT (Watchdog), PANIC (Hard-Fault),
plus Sub-Causes wie `WARM(cli)`, `WDT(stay-off)`, `shutdown-pending(no-USB)`.
Befehl `log read` zeigt die letzten 10 Einträge.

### Watchdog

90-Sekunden-Watchdog gegen hängende Firmware. Pet-Cycle in jeder
Loop-Iteration.

### Buzzer-Signale

Auf Geräten mit Buzzer (T1000-E): Startup-Sound beim Hochfahren,
Shutdown-Sound vor `shutdown`/`reboot`/automatischem Abschalten.
Konfigurierbar per Event-Typ über `set buzzer.dm/channel/private/ack/app_disc`.

### Recovery-Befehle

- `shutdown` — sauberes Herunterfahren (mit Sound)
- `reboot` — Neustart (mit Sound)
- `dfu` — direkt in den UF2-Bootloader (NRF52)

## Hardware-Unterstützung

Aktiv getestet auf:

- **Heltec Wireless Tracker V1.1** (ESP32-S3) — Primary Development Target
- **SenseCAP T1000-E** (NRF52840) — Für mobile Tracker-Anwendungen

Andere MeshCore-unterstützte Hardware sollte ebenfalls funktionieren, ist aber
nicht systematisch getestet.

## Build-Hinweise

NRF52-Plattformen brauchen einen erhöhten Loop-Task-Stack (8 KB statt 4 KB
Default) wegen größerer CLI-Handler-Puffer. Ein Pre-Build-Skript patcht das
Framework automatisch bei jedem Build:

```
extra_scripts = ${arduino_base.extra_scripts}
  pre:dl9sau_patch_nrf52_loop_stack.py
```

Ist in `platformio.ini` für die NRF52-Builds bereits aktiv. Funktioniert auch
nach PlatformIO-Framework-Updates (idempotent).
