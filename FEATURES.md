# Features dieses Forks

Validierte und in der Praxis genutzte Erweiterungen gegenüber dem Original-MeshCore.
Die einzelnen Implementierungs-Details und Begründungen stehen in den
Commit-Nachrichten.

Schwerpunkt ist der **Companion-Mode** (Smartphone-App + Tracker) — aber die
**Repeater-Funktionalität ist vollständig integriert** (auch die CLI-Befehle sind
angeglichen): dasselbe Gerät kann ohne Reflash als Repeater laufen. Siehe
[Repeater-Mode](#repeater-mode).

---

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

Wochentag-Kurzformen: `Wd` (Mo-Fr), `We` (Sa+So), `Wk`/`*` (alle 7 Tage).
Cron-Aufträge überleben einen Neustart, einmalige `at`-Aufträge nicht.

### Lokaler Konfigurations-Chat `$companion`

Eingaben in den Chat-Kanal `$companion` werden lokal vom Tracker verarbeitet —
nichts geht über Funk raus. Die komplette CLI ist direkt in der App benutzbar.

### Serial-Konsole als Alternative zur App

Die identische CLI ist über USB-Serial erreichbar (z.B. mit `cu`, `screen`,
oder `picocom`). Damit lässt sich der Tracker auch ohne Smartphone-App
konfigurieren und überwachen. Auch **Remote-Befehle** an andere Repeater
können von der Serial-Konsole aus abgesetzt werden (`remote admin login`,
`remote admin cmd`), nicht nur aus der App.

### Hilfe-System

`help` zeigt alle Themen, `help <thema>` zeigt Details mit Beispielen.

### Eigener Chat-Name

Pref `chatname` — Anzeige-Name in Chats ist unabhängig vom Knoten-Namen
einstellbar (z.B. Knoten heißt `Tracker-1`, Chat-Name `Thomas`).

### Bluetooth-Namens-Bereinigung

Auf NRF52 hat der Bluetooth-Name eine technische Begrenzung. Bei zu langen
oder mit Sonderzeichen versehenen Knoten-Namen fällt das Original auf den
Bootloader-Default zurück. Dieser Fork bereinigt den Namen automatisch
(Längen-Cap, Unicode-Filter) damit der Tracker mit dem richtigen Namen
in der App erscheint.

### Eingebaute Speicherung von Nachrichten

Empfangene Nachrichten werden in fünf Buckets gehalten, die individuell
konfiguriert sind:

```
messages (offline-queue):
  DM          0/24  flash=on        Direkt-Nachrichten
  $companion  0/16  flash=on        Lokaler Konfigurations-Chat

Channels (Gruppen-Chats):
  public      0/8   flash=off       Allgemeine Public-Channel-Nachrichten
  hashtag     0/16  flash=on        Hashtag-/Themen-Channel-Nachrichten
  private     0/16  flash=on        Private/passwortgeschützte Channels
```

Buckets mit `flash=on` werden persistent gespeichert und überleben einen
Reboot. Beim Wiederverbinden der App sind alle vorherigen Nachrichten
wieder da, ohne dass die App den Tracker beim Verbinden polling muss.
Größe und Flash-Setting sind pro Bucket konfigurierbar
(`messages limit <type> <N>`, `messages flash <type> <on|off>`).

### Bewegungs-abhängige Position-Adverts (zero-hop)

Wenn der Companion-Tracker GPS-Bewegung erkennt, sendet er automatisch
**zero-hop-Adverts** mit der aktuellen Position — also nur an direkt
gehörte Nachbarn, ohne dass die Pakete durch das Mesh geflutet werden.
Die Häufigkeit passt sich an:

- in Ruhe seltener (etwa alle 3 Stunden, ohne Position),
- bei langsamer Bewegung häufiger (etwa stündlich, mit Position),
- bei schneller Bewegung sehr häufig (etwa alle 15 Minuten, mit Position).

Das reduziert unnötigen Funkverkehr im Stillstand und sorgt gleichzeitig
für aktuelle Positionsinformation bei Reisen — ohne dass weit entfernte
Mesh-Knoten mit fortlaufenden Positions-Updates belastet werden.

Real-world validiert: 4 Stunden Autofahrt (Heimat → Jever) ergab
10 Adverts.

### Automatische Zeitzone

Tracker erkennt EU-Sommerzeit automatisch, oder per Pref auf eine feste
Zeitzone setzbar. `at`/`cron`, RTC-Anzeige und Boot-Log rechnen in lokaler Zeit.

### Zeit-Sync aus gehörten Adverts

Tracker kann seine Uhrzeit aus Adverts anderer Knoten übernehmen, wenn
diese eine Zeit-Information mitschicken. Quellen sind:

- der eigene Admin-Companion-Knoten
- direkt gehörte andere Repeater
- gepinnte Knoten

Damit ist der Tracker zeitlich aktuell auch ohne GPS-Fix oder App-Verbindung,
sobald er einen Knoten mit gültiger Uhrzeit hört.

---

## Repeater-Mode

### Vollständige Repeater-Funktionalität im Companion-Build

`repeater on` schaltet das Companion-Gerät in den Repeater-Mode (`repeater off`
wieder aus). **Wichtig:** in der Smartphone-App den Repeat-Schalter erst
umlegen, **nachdem** das Profil auf `normal` steht — sonst lehnt die Firmware
im Default-Profil `defensive` das Aktivieren auf der Haupt-Frequenz ab (siehe
unten).

Zwei Profile (`repeater profile <normal|defensive>`):

- **`repeater profile normal`** — Standard-Repeater, leitet alle bekannten
  Mesh-Pakete weiter. **Nötig, damit die App den Repeat-Mode auf der
  Haupt-Frequenz aktivieren kann.**
- **`repeater profile defensive`** (Default) — Bewusst zurückhaltender Repeater.
  Das Companion-Gerät ist primär Client; mit diesem Profil hilft es dem
  Mesh nur minimal und kontrolliert weiter. Konkretes Verhalten:

  - leitet **nur Pakete mit Scope-Code** weiter (unscoped flood wird
    durchgelassen aber nicht weiterverbreitet),
  - antwortet auf Pfad-Discovery-Anfragen **nur für direkt gehörte
    Endpunkte** (nicht für entferntere),
  - sendet Wiederholungen mit **reduzierter Sendeleistung** und
    **CR5 (geringere Coding-Rate)** für minimale Funkbelastung.

  Gut geeignet für **definierte Ad-hoc-Frequenzen** wie bei SAR-Einsätzen
  (Search-and-Rescue) oder lokalen Veranstaltungen, wo nur Mesh-Traffic der
  Veranstaltung weitergeleitet werden soll und nicht der globale Mesh-Verkehr
  durchschlägt.

  Auf der **Haupt-Mesh-Frequenz** ist Client-Repeat bewusst gesperrt — dort
  arbeiten die echten Repeater. Wer es dort gezielt braucht (z. B. für einen
  Einsatz), kann es über einen bewussten Opt-in-Befehl freischalten. Fragen
  dazu beantworte ich gerne ;)

### Automatische Abschaltung bei Bewegung

Wenn der Repeater per GPS Bewegung erkennt, schaltet er sich automatisch ab
(Repeater sind als statische Knoten gedacht; ein bewegtes Gerät würde die
Topologie für andere Knoten dauerhaft verändern). Sobald das Gerät wieder
stillsteht, schaltet sich der Repeater-Mode wieder ein.

### Nightly-Advert mit Scope

Der Repeater sendet sein Advert einmal pro Nacht zu einem zufälligen
Zeitpunkt (verhindert Lastspitzen wenn viele Repeater synchron senden).
Repeater-Adverts sind grundsätzlich mit Scope-Code versehen, bleiben also
in regionalen Mesh-Bereichen statt global zu fluten.

### CAD-Threshold (`int.thresh`)

Sendeunterdrückung wenn der Kanal bereits belegt ist (Channel-Activity-Detection).
Tunable per `set int.thresh <N>` — bei kleinen Werten sendet der Repeater
auch in stark frequentierten Kanälen, bei großen Werten ist er rücksichtsvoll.

### AGC-Reset

`set agc.reset.interval <sec>` — periodischer Reset der Automatic Gain Control
am SX1262. Hilft auf Geräten die nach längeren Empfangs-Phasen taub werden.

### Repeater-Statistiken

`stats-packets` zeigt detaillierte Repeater-Metriken: empfangen, gesendet,
wiederholt, abgewiesen, plus Trace-basierte Untergruppierung. `stats-radio`
zeigt RSSI/SNR, Noise-Floor, CAD-Aktivierungen.

### Reboot-Vermeidung

Watchdog-überwachte Repeater-Operation. Bei Lockup automatischer Watchdog-Reset
mit Eintrag im Boot-Log (`WDT`). Plus `LORA(dead)` als Marker wenn die SPI-
Verbindung zum SX1262 keinen sinnvollen RSSI mehr liefert.

---

## Filter- und Routing-System

### Lokale Filter (App-seitig)

Pref-Familie `filter.*` — blendet Nachrichten in der App aus, ohne die
Mesh-Weiterleitung zu beeinflussen.

- `filter sender drop/keep <pattern>` — Sender-basiert
- `filter text drop/keep <pattern>` — Text-Inhalt
- `filter scope drop/keep <region>` — Scope-Code
- Wildcards möglich, Patterns sind literal (kein Auto-Trim von Satzzeichen)
- UND-verknüpft (mehrere Modifikatoren = alle müssen passen)

### Weiterleitungs-Filter (für Repeater)

Pakete die im lokalen Filter abgelehnt werden, werden auch nicht weitergeleitet.
Damit kann ein Repeater bestimmte Sender, Texte oder Scope-Codes systematisch
unterdrücken.

### Hop-Cap je Channel

`set ch.hops <channel-name> <N>` — pro bekanntem Channel separate Hop-
Begrenzung. Standardmäßig folgt jeder Channel dem globalen `flood_max`,
kann aber individuell hochgesetzt oder eingeschränkt werden. Plus
`ch.hops unknown <N>` für unbekannte Channels.

### Differenzierte globale Hop-Caps

Statt eines globalen Wertes gibt es separate Limits für:

- `flood_max` — Standard für Chat-Nachrichten
- `flood_max_infra` — Repeater/Sensor/Room-Adverts
- `flood_max_req_resp` — REQ/RESP-Nachrichten
- `flood_max_unknown_chan` — Nachrichten auf unbekannten Kanälen
- `flood_max_unscoped_companions` — Companion-Adverts ohne Scope

Plus `flood.max.advert` als Alias auf `flood_max_infra` (für
Upstream-Kompatibilität).

---

## Scope-System (Regionale Begrenzung)

### Konzept

Pakete bekommen einen Scope-Code im Header. Tracker entscheiden beim Empfang
ob sie weiterleiten — basierend auf einer lokalen Allowlist von Scopes.
So bleibt regionaler Traffic regional, statt weltweit zu fluten.

### Scope-Konfiguration

- `scope add/del <region>` — Lokale Scope-Liste verwalten
- `scope default <name>` — Default-Scope für ausgehende Pakete (auch in der App
  als Default-Scope setzbar). Magic-Werte wie `#geo` siehe unten.
- `flood_max_scope_region <N>` — Hop-Begrenzung speziell für Scope-Pakete

### `#geo` — automatischer Regional-Scope

Setzt man den **Default-Scope** (in der App oder via `scope default #geo`) auf
**`#geo`**, löst die Firmware beim Senden automatisch die **zutreffende,
möglichst kleine** Region auf — aus der eingebauten Regionen-Tabelle plus der
aktuellen GPS-Position. Beispiel: in Berlin geht die Nachricht als `#de-be`
raus, **nicht** als das größere `#de-bebb`. Beim Reisen wechselt der Scope
nahtlos mit der Position (geo-fenced).

- Es ist ein **Fallback/Default**: pro Nachricht kann in der App ein anderer
  Scope gesetzt werden, der `#geo` überschreibt.
- Ohne GPS-Fix: Rückfall auf `#local` (bzw. unscoped) — es geht nie ein roher,
  unaufgelöster `#geo`-Code raus.

### Auto-Scope (`scope use auto`)

`#geo` oben ist der Fall, dass man Geo **explizit** als Default wählt — das ist
der übliche Weg. Aber auch **ohne** gesetzten Default-Scope bestimmt die Firmware
den Scope für eigene Pakete/Adverts automatisch. Gesteuert über
`scope use auto <on|off|prefer>`:

- **on** (Default): der Default-Scope gewinnt; ist keiner gesetzt, greift Geo
  als Fallback (kleinste passende Region per GPS).
- **prefer**: Geo schlägt den Default, wenn die örtliche Region eine andere ist
  als der Default (»User ist nicht zu Hause«) — praktisch beim Reisen.
- **off**: Geo wird nie verwendet, nur Default/Override.

Praktisch beim Reisen: der Tracker schaltet sich so nahtlos zwischen regionalen
Bereichen um, ohne dass man pro Region etwas umkonfigurieren muss.

### Magic-Scopes (per Nachricht aus der App)

Als Scope-Name kann eines dieser **Sonder-Keywords** gesetzt werden; die
Firmware interpretiert sie beim Senden speziell, statt sie als normalen Scope
zu hashen:

| Keyword(s) | Wirkung |
|---|---|
| `#geo` | zur kleinsten passenden Region auflösen (s.o.) |
| `#unscoped` | **ohne** Scope-Code fluten — auch wenn der Default ein Scope oder zero-hop ist |
| `#region` / `#regional` | Regional-Scope (konfigurierbarer Hop-Cap, `flood_max_scope_region`) |
| `#local` / `#lokal` | nur **1 Hop** (direkte Nachbarn) |
| `#direct` / `#direkt` / `#norepeat` / `#no-repeat` | **zero-hop**, kein Repeater leitet weiter |

Die Doppel-Synonyme (deutsch/englisch, mit/ohne Bindestrich) sind Absicht.

### Eingrenzung von unscoped Flood-Traffic

Unscoped Pakete (= Pakete ohne Scope-Code) werden global geflutet. Das ist
wichtig damit neue Teilnehmer überhaupt einen Mesh entdecken können.
Pref `flood_max_unscoped_companions` begrenzt die Hop-Reichweite für
unscoped Companion-Adverts auf ein nötiges Minimum (Default: 3 Hops).

### Unscoped Channel-Messages direkt adressiert

Pref `unscoped-channelmessages direct|flood` — entscheidet ob Nachrichten
auf bekannten Channels ohne Scope-Code wie normale Flood-Pakete versendet
werden oder direkt an bekannte Mesh-Mitglieder adressiert werden.

### Scope-Anzeige in Chats

Pref `messages_append_scope_to_name` — empfangene Text-Nachrichten zeigen
den verwendeten Scope-Code beim Sender-Namen an. Damit ist nachvollziehbar
ob eine Nachricht aus dem regionalen oder globalen Mesh kam.

---

## Diagnose und Monitoring

### Status- und Statistik-Befehle

```
status                Allgemeine Übersicht (Hardware, Firmware, Uhrzeit, GPS)
stats                 Ausführliche Paket-Statistiken: empfangene, gesendete und
                      weitergeleitete (repeated) Pakete je Paket-Typ (DM,
                      Advert, Channel-Message, REQ/RESP, Path-Discovery, ...),
                      plus Airtime-Bilanz und Duty-Cycle-Werte
stats-core            Akku, Uptime, Message-Queue, letzte Reset-Ursache
stats-radio           RSSI/SNR, Noise-Floor, Sende-/Empfangs-Zähler
stats-packets         Repeater-Metriken (rx/tx/repeat/drop pro Trace-Kategorie)
uptime                Boot-Zeit + Uptime
sensors               Sensor-Werte (Batterie, Temperatur, GPS, ...)
neighbors             Direkt gehörte oder N-Hop entfernte Knoten mit RSSI/SNR,
                      Hop-Anzahl, Richtung und Distanz
```

### Trace-Kategorien

`trace <kategorie> <level>` (Level `0`/`off`, `1`/`on`, `2`, `3`) aktiviert
detaillierte Logs für einzelne
Bereiche der Firmware, ohne dass die anderen Module zugeschüttet werden.
Hilfreich zum gezielten Debuggen ohne dass der Output unleserlich wird.

Verfügbare Kategorien (Auswahl):

- `gps` — GPS-Empfang, NMEA-Parsing, Schlaf-/Wake-Vorgänge
- `filter` — wie Pakete vom Filter-System behandelt wurden (durchgelassen,
  ausgeblendet, weitergeleitet)
- `scope` — gehörte Scope-Codes, Region-Auflösung
- `bluetooth` (auch `ble`) — Bluetooth-Stack-Übergänge (Connect, Disconnect,
  Sleep-Cycle-Phasen, BLE-OTA-Zustände)
- `rtc` — Zeit-Sync-Ereignisse (App-Sync, GPS-Sync, Advert-Sync), Drift
- `heard` — direkt gehörte Knoten (neue Entdeckungen oder jeder Empfang)
- `discover` — Discovery-Vorgänge (Repeater-Liste, Regions-Anfragen)
- `duty` — Duty-Cycle-Beobachtung, Schwellwert-Überschreitungen
- `ack` — Bestätigungs-Empfang und Timeouts
- `repeat` — Repeater-Entscheidungen (warum weitergeleitet/abgewiesen)

Plus mehrere weitere Kategorien für spezialisierte Bereiche.
Nicht alle gleichzeitig aktivierbar (RAM-Limit für Trace-Buffer).

### Discovery-Befehle

```
discover repeater     Pro-aktive REQ an direkte Nachbarn, listet
                      Repeater-Adverts mit RSSI/SNR
discover regions      Auflistung verfügbarer Scope-Regionen aus
                      ANON-REQ-Antworten
```

### Boot-Log

Nach jedem Reset wird der Grund im persistenten Log festgehalten:

- `COLD` — Power-on / Brown-Out / unbestimmt
- `WARM` — Software-Reset, evtl. User-getriggert
- `WARM(cli)` — User-Befehl `reboot` oder `shutdown`
- `WDT` — Watchdog-Timeout
- `PANIC` — CPU-Lockup / HardFault
- `UF2` — UF2-Bootloader-Eintrag
- Plus Sub-Causes wie `WDT(stay-off)`, `shutdown-pending(no-USB)`,
  `LORA(dead)`, `VBUS(stay-off)` für spezifische Diagnose

Befehl `log read` zeigt die letzten 10 Boot-Einträge inkl. Uptime der jeweils
vorherigen Session.

### Log-Output-Routing

Debug- und Trace-Ausgaben können wahlweise auf USB-Serial oder in den
`$companion`-Chat (oder beides) ausgegeben werden. Praktisch wenn man
über die App alleine arbeitet und keine serielle Verbindung hat — die
Logs erscheinen dann als Nachrichten im lokalen Konfigurations-Chat.

### Watchdog

90-Sekunden-Watchdog gegen hängende Firmware. Pet-Cycle in jeder
Loop-Iteration. Pausiert im Standard-Sleep (WFE/WFI) und im Debugger-Halt.

### Backup und Restore

```
backup save           Komplette Pref-Konfiguration als JSON-Block
                      (auf Serial oder via App)
backup restore        JSON-Block einspielen, alle Prefs setzen
```

Für Migrations zwischen Geräten, oder als Sicherung vor experimentellen
Settings.

---

## Stromspar-Mechanismen

### Bluetooth-Schlaf-Zyklus (ESP32)

Bluetooth schläft automatisch wenn keine App verbunden ist. Auf dem Heltec
Wireless Tracker macht das einen deutlichen Unterschied (Bluetooth permanent
an vs. Cycler-Modus etwa 169 mA vs. 95 mA, also rund 70 mA Ersparnis). Cycle:
10 Minuten Boot-Phase, dann 40s Schlaf plus 20s Listen-Phase.

Bluetooth wird automatisch für 5 Minuten geweckt wenn:
- eine **Direkt-Nachricht** ankommt,
- ein **Remote-Konfigurations-Befehl** ankommt — letzteres aber nur wenn
  der Knoten gerade als Repeater fungiert (typisch wenn der Operator
  einen entfernten Repeater warten will).

Per Remote-Konfigurations-Befehl kann das Bluetooth-Profil auch direkt
gesetzt werden (`bluetooth on`, `bluetooth power cycle|always-on`).
Pref `bluetooth power cycle|always-on` ist auch lokal über die CLI
einstellbar.

Auf NRF52 nicht aktiv — Bluetooth ist dort hardware-bedingt bereits im
µA-Bereich im Leerlauf. Auf T1000-E ist `bluetooth off` technisch als
"Advertising aus" umgesetzt (statt SoftDevice-Disable). Damit ist das
Gerät nicht mehr in der Bluetooth-Umgebung sichtbar, der Stack bleibt
aber initialisiert und stabil — vermeidet SoftDevice-Lifecycle-Risiken
und kostet praktisch nichts an Strom.

### Empfänger-Abschaltung (RX-Disable)

Pref `rx_disabled` schaltet den LoRa-Empfangsteil komplett ab. Kurze
Aufwach-Phasen bei jedem Sendevorgang für Bestätigungs-Empfang. Nur im
Companion-Mode (nicht im Repeater-Mode).

Hinweis: auf einem leeren Funkkanal ist der Strom-Spareffekt nicht direkt
messbar (das Empfangsteil ist nicht aktiv-busy). Auf einem stark belegten
Kanal mit häufigen Demodulations-Vorgängen kann der Effekt deutlicher sein.
Hauptanwendung ist eher reine "Schweige"-Konfiguration als pure
Strom-Optimierung.

### Sender-Abschaltung (TX-Disable)

`tx disable` schaltet das Senden temporär ab (z.B. für Wartung, Antenne
abgeklemmt). `tx suspend <N>` mit Auto-Re-Enable nach N Minuten. Im
Repeater-Mode nur `suspend`, nicht permanentes `disable` (damit kein
versehentlich stummer Repeater).

### GPS-Schlafmodus

Pref `gps_lead_secs` steuert wie viele Sekunden vor dem nächsten Advert das
GPS aufwacht um einen Fix zu bekommen. Default 5 Minuten — bei 15-Minuten-
Adverts ist GPS dann nur zu einem Drittel der Zeit aktiv. Plus
`gps_profile full|position-only|time-only` für verschiedene
Anwendungsfälle.

### USB-Loss-Shutdown

Pref `usb_loss_shutdown_min` — Tracker fährt selbst herunter wenn USB-Strom
für N Minuten weg ist. Anwendungsfall: Tracker im Auto an Bord-Stromversorgung,
Zündung aus, nach z.B. 30 Minuten sauberer Shutdown.

### Akku-Schutz

Pref `batt_chemistry` (lipo/lifepo4) plus `batt_min_mv` — Tracker schaltet
bei Unterspannung sauber ab. 3-Stufen-Sample (10 Minuten normal, 10 Sekunden
im Verdachts-Burst) gegen falsche Auslösung durch Sendespitzen.

### Phantom-Wake-Filter (T1000-E)

Nach `shutdown` blockiert ein persistenter Marker im Flash unbeabsichtigte
Reaktivierungen (Watchdog-Trigger, Spannungs-Schwankungen, USB-Wackel).
Tracker bleibt verlässlich aus bis User USB stabil einsteckt oder Button
drückt.

---

## Audio (Buzzer)

Auf Geräten mit Buzzer (T1000-E) feintunebar:

```
set buzzer_quiet on|off        Master-Mute
set buzzer.dm on|off            Direkt-Nachricht-Sound
set buzzer.channel on|off       Channel-Nachricht-Sound
set buzzer.private on|off       Private-Channel-Sound
set buzzer.ack on|off           Bestätigungs-Sound
set buzzer.app_disc on|off      App-Disconnect-Sound
```

Plus Startup-Sound beim Hochfahren und Shutdown-Sound vor `shutdown`/`reboot`/
automatischem Abschalten.

---

## Befehle für Routine-Aktionen

```
shutdown                       Sauberes Herunterfahren mit Sound
reboot                         Neustart mit Sound
clear                          Bildschirm / Companion-Chat löschen
contact ...                    Kontakt-Liste verwalten
remote admin login/cmd         Remote-Befehle an Repeater
```

## Firmware-Update

### `dfu` (NRF52)

Befehl `dfu uf2` versetzt den Tracker direkt in den UF2-Bootloader-Modus.
Der Tracker erscheint dann als USB-Massenspeicher, und eine neue `.uf2`-Datei
kann einfach drauf kopiert werden — kein Reset-Doppelklick, kein
zusätzliches Tool nötig.

Alternativ `dfu serial` für klassischen Serial-DFU mit `nrfutil`.

### `start ota` (ESP32)

Befehl `start ota` startet den Tracker in einen WiFi-Access-Point-Modus, in
dem über eine kleine Web-Oberfläche eine neue Firmware hochgeladen werden
kann. Praktisch wenn das Gerät schwer zugänglich ist und ein USB-Kabel
ranlegen nicht in Frage kommt.

---

## Hardware-Unterstützung

Aktiv getestet auf:

- **Heltec Wireless Tracker V1.1** (ESP32-S3) — Primary Development Target,
  Companion-Mode und Repeater-Mode
- **SenseCAP T1000-E** (NRF52840) — Mobile Tracker-Anwendung, Companion-Mode

Andere MeshCore-unterstützte Hardware sollte ebenfalls funktionieren, ist aber
nicht systematisch getestet.

---

## Build-Hinweise

### NRF52 Loop-Stack-Patch

NRF52-Plattformen brauchen einen erhöhten Loop-Task-Stack (8 KB statt 4 KB
Default) wegen größerer CLI-Handler-Puffer. Ein Pre-Build-Skript patcht das
Framework automatisch bei jedem Build:

```
extra_scripts = ${arduino_base.extra_scripts}
  pre:dl9sau_patch_nrf52_loop_stack.py
```

Ist in `platformio.ini` für die NRF52-Builds bereits aktiv. Funktioniert auch
nach PlatformIO-Framework-Updates (idempotent).

### Build-Flags

```
-D CFG_BLE_TASK_STACKSIZE=2048   Bluefruit BLE-Stack 8 KB (statt 5 KB Default)
```
