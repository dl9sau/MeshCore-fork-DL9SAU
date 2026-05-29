
  ┌───────────────────────────────────┬───────────────────────────┬───────────┐
  │             TX-Quelle             │         TX-Power          │    CR     │
  ├───────────────────────────────────┼───────────────────────────┼───────────┤
  │ Eigene Direct-Message an Kontakt  │ _prefs.tx_power_dbm       │ _prefs.cr │
  ├───────────────────────────────────┼───────────────────────────┼───────────┤
  │ App-getriggerter Self-Advert      │ _prefs.tx_power_dbm       │ _prefs.cr │
  ├───────────────────────────────────┼───────────────────────────┼───────────┤
  │ App-getriggerte Channel-Nachricht │ _prefs.tx_power_dbm       │ _prefs.cr │
  ├───────────────────────────────────┼───────────────────────────┼───────────┤
  │ Direct-Repeat (wir sind Pfad-Hop) │ _prefs.tx_power_dbm       │ _prefs.cr │
  ├───────────────────────────────────┼───────────────────────────┼───────────┤
  │ TRACE-Forwarding                  │ _prefs.tx_power_dbm       │ _prefs.cr │
  ├───────────────────────────────────┼───────────────────────────┼───────────┤
  │ Auto-Advert, statisch (3h / 1h)   │ max(10, dbm − 6)          │ 5         │
  ├───────────────────────────────────┼───────────────────────────┼───────────┤
  │ Auto-Advert, moving (15 min)      │ _prefs.tx_power_dbm ← neu │ 5         │
  ├───────────────────────────────────┼───────────────────────────┼───────────┤
  │ Nightly Flood Auto-Advert         │ max(10, dbm − 6)          │ 5         │
  ├───────────────────────────────────┼───────────────────────────┼───────────┤
  │ Flood-Repeat (Digipeating)        │ max(10, dbm − 6)          │ 5         │
  └───────────────────────────────────┴───────────────────────────┴───────────┘


---


❯ Vergleiche mir nochmal kurz unsere Sendeleistung und CR fuer unsere eigenen Aussendungen und fuer die Pakete die wir repeaten

⏺ Eigene Aussendungen

  ┌──────────────────────────────────────────┬─────────────────────┬───────────┐
  │                  Quelle                  │        Power        │    CR     │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  │ Eigene Direct-Message an Kontakt         │ _prefs.tx_power_dbm │ _prefs.cr │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  │ App-getriggerter Self-Advert (App-Knopf) │ _prefs.tx_power_dbm │ _prefs.cr │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  │ App-getriggerte Channel-Nachricht        │ _prefs.tx_power_dbm │ _prefs.cr │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  │ Auto-Advert, statisch (3h / 1h-Slot)     │ max(10, dbm-6)      │ 5         │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  ┌──────────────────────────────────────────┬─────────────────────┬───────────┐
  │                  Quelle                  │        Power        │    CR     │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  │ Eigene Direct-Message an Kontakt         │ _prefs.tx_power_dbm │ _prefs.cr │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  │ App-getriggerter Self-Advert (App-Knopf) │ _prefs.tx_power_dbm │ _prefs.cr │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  │ App-getriggerte Channel-Nachricht        │ _prefs.tx_power_dbm │ _prefs.cr │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  │ Auto-Advert, statisch (3h / 1h-Slot)     │ max(10, dbm-6)      │ 5         │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  │ Auto-Advert, moving (15-min-Slot)        │ _prefs.tx_power_dbm │ 5         │
  ├──────────────────────────────────────────┼─────────────────────┼───────────┤
  │ Nightly Flood Auto-Advert (mit Scope)    │ max(10, dbm-6)      │ 5         │
  └──────────────────────────────────────────┴─────────────────────┴───────────┘

  Repeated Pakete

  Direct-routed Repeats (Sender hat uns als Pfad-Hop in path[] gesetzt):

  ┌───────────────────────────────────────────────┬─────────────────────┬───────────┐
  │                      Typ                      │        Power        │    CR     │
  ├───────────────────────────────────────────────┼─────────────────────┼───────────┤
  │ TXT_MSG / REQ / RESP / ACK / MULTIPART direct │ _prefs.tx_power_dbm │ _prefs.cr │
  ├───────────────────────────────────────────────┼─────────────────────┼───────────┤
  │ TRACE-Forwarding                              │ _prefs.tx_power_dbm │ _prefs.cr │
  └───────────────────────────────────────────────┴─────────────────────┴───────────┘

  Flood-routed Repeats (durch routeRecvPacket → shouldReduceFloodRetransmit):

  ┌──────────────────────────────────────────────────────────────┬─────────────────────┬───────────┐
  │                          Situation                           │        Power        │    CR     │
  ├──────────────────────────────────────────────────────────────┼─────────────────────┼───────────┤
  │ n > 0 — schon repeated von jemand anderem                    │ max(10, dbm-6)      │ 5         │
  ├──────────────────────────────────────────────────────────────┼─────────────────────┼───────────┤
  │ n == 0, ADVERT von User/Sensor/Room                          │ _prefs.tx_power_dbm │ _prefs.cr │
  ├──────────────────────────────────────────────────────────────┼─────────────────────┼───────────┤
  │ n == 0, ADVERT von REPEATER                                  │ max(10, dbm-6)      │ 5         │
  ├──────────────────────────────────────────────────────────────┼─────────────────────┼───────────┤
  │ n == 0, TXT/REQ/RESP/ACK/...                                 │ _prefs.tx_power_dbm │ _prefs.cr │
  ├──────────────────────────────────────────────────────────────┼─────────────────────┼───────────┤
  │ n == 0, PATH (mind. einer der Endpunkte zero-hop heard <48h) │ _prefs.tx_power_dbm │ _prefs.cr │
  ├──────────────────────────────────────────────────────────────┼─────────────────────┼───────────┤
  │ n == 0, PATH (nur via Contact-Cache, nicht zero-hop heard)   │ max(10, dbm-6)      │ 5         │
  └──────────────────────────────────────────────────────────────┴─────────────────────┴───────────┘

  Schnell-Merksatz

  - Wir selbst → eigene Direct-Kommunikation: immer full, full
  - Wir selbst → periodische / Network-Spam-artige Adverts: meist reduced + CR5 (Ausnahme: moving Tracker behält volle Reichweite)
  - Direct-Repeat (wir = im Pfad): immer full, full
  - Flood-Repeat: full + voll-CR nur bei direkt gehörten Endknoten, die keine Repeater sind und (bei PATH) auch in der HeardList stehen. Sonst reduced
   + CR5.

✻ Crunched for 33s

❯ Bei meshtastic haben wir ja andere Konzepte (clients die repeaten, advert mit allem hier vs nodeinfo/telemetrie/location dort, nur automatische
  eigene adverts/telemetrie/location, unterschiedliche client*-Rollen/Sensoren/Tracker/..), und deshalb andere Strategien. Vergleiche auch
  /Users/thomas/meshtastic-firmware-src-github/firmware/README-dl9sau.txt . Ist das Verhalten jetzt kongruent?

⏺ Ich lese die Meshtastic-Doku ein und mache dann einen direkten Vergleich.

  Read 1 file, listed 1 directory (ctrl+o to expand)

⏺ Hier der direkte Vergleich beider Systeme:

  Konzept-Unterschiede

  ┌─────────────────────────┬──────────────────────────────────────────────────────────┬─────────────────────────────────────────────────────────┐
  │         Aspekt          │                        Meshtastic                        │                        MeshCore                         │
  ├─────────────────────────┼──────────────────────────────────────────────────────────┼─────────────────────────────────────────────────────────┤
  │ Auto-Aussendungen       │ Position, Telemetrie, NodeInfo getrennt                  │ Alles in einem ADVERT (Name + Position)                 │
  ├─────────────────────────┼──────────────────────────────────────────────────────────┼─────────────────────────────────────────────────────────┤
  │ Rollen                  │ CLIENT / CLIENT_BASE / CLIENT_MUTE / REPEATER / ...      │ ADV_TYPE_CHAT/REPEATER/ROOM/SENSOR + client_repeat Bool │
  ├─────────────────────────┼──────────────────────────────────────────────────────────┼─────────────────────────────────────────────────────────┤
  │ Reichweiten-Philosophie │ "Position braucht Reichweite", "Telemetrie bleibt lokal" │ Alles vereint im Advert → ein Kompromiss                │
  ├─────────────────────────┼──────────────────────────────────────────────────────────┼─────────────────────────────────────────────────────────┤
  │ Stage-4 Companion       │ LongFast-Klon der Position                               │ gibt es nicht                                           │
  └─────────────────────────┴──────────────────────────────────────────────────────────┴─────────────────────────────────────────────────────────┘

  Eigene Aussendungen — Direkt-Vergleich

  ┌───────────────────────────────┬─────────────────────────────┬────────────────────────────────────────────────────┐
  │           Use-Case            │      Meshtastic CLIENT      │                  MeshCore aktuell                  │
  ├───────────────────────────────┼─────────────────────────────┼────────────────────────────────────────────────────┤
  │ Position-Beacon               │ vanilla Power, CR=5, ≤2 hop │ Static: −6 dB, CR=5 ← abweichend!                  │
  ├───────────────────────────────┼─────────────────────────────┼────────────────────────────────────────────────────┤
  │ Position-Beacon (in Bewegung) │ (gleich) vanilla, CR=5      │ Moving: vanilla, CR=5 ✓                            │
  ├───────────────────────────────┼─────────────────────────────┼────────────────────────────────────────────────────┤
  │ Telemetrie (periodisch)       │ −6 dB, CR=5, hop=0          │ gibt es nicht (Telemetrie nur on-request)          │
  ├───────────────────────────────┼─────────────────────────────┼────────────────────────────────────────────────────┤
  │ Text/NodeInfo manuell         │ vanilla, vanilla            │ App-Advert, App-Direct-Message: vanilla, vanilla ✓ │
  └───────────────────────────────┴─────────────────────────────┴────────────────────────────────────────────────────┘

  Repeats — Direkt-Vergleich

  Situation: TRACEROUTE / ROUTING (= "Diagnose")
  Meshtastic: vanilla, vanilla
  MeshCore: TRACE → vanilla, vanilla ✓
  ────────────────────────────────────────
  Situation: Direct-DM mit lokalem Ziel (≤12 h)
  Meshtastic: vanilla, vanilla
  MeshCore: Direct-Repeat (wir = Pfad-Hop): vanilla, vanilla ✓
  ────────────────────────────────────────
  Situation: First-relay (Originator = direkter Nachbar)
  Meshtastic: vanilla, vanilla
  MeshCore: n==0 (User/Sensor/Room ADVERT, TXT, …): vanilla, vanilla ✓
  ────────────────────────────────────────
  Situation: B2 default (alles andere repeatet)
  Meshtastic: −6 dB, CR=5
  MeshCore: n>0: reduced + CR5 ✓
  ────────────────────────────────────────
  Situation: Telemetrie-Repeats
  Meshtastic: drop
  MeshCore: nicht spezifisch behandelt (MeshCore hat keine periodische Telemetrie als Flood)
  ────────────────────────────────────────
  Situation: Repeater-Advert (n==0, von Repeater)
  Meshtastic: nicht extra geregelt
  MeshCore: reduced + CR5 ← MeshCore-spezifisch
  ────────────────────────────────────────
  Situation: PATH ohne zero-hop-heard Endpunkt
  Meshtastic: nicht so kategorisiert
  MeshCore: reduced + CR5 ← MeshCore-spezifisch

  Kongruenz-Bewertung

  Im Kern kongruent (gleiche Philosophie: "First-relay full, alles andere reduced"):
  - B2-default-Verhalten matcht
  - CR=5 als Vereinheitlichungs-Default überall
  - Diagnose-Pakete (TRACE) unangetastet
  - Lokale Direct-Routes bleiben vanilla

  Bewusste Abweichung in MeshCore:
  - Static Auto-Advert mit −6 dB statt vanilla. Begründung damals: "wir sind am Standort schon bekannt"; bei Meshtastic ist die Logik aber "Position
  braucht Reichweite".
  - MeshCore hat zusätzlich zwei feinere Regeln (Repeater-Advert + PATH-zero-hop-Check), die Meshtastic so nicht hat — kein Widerspruch, eher
  Verfeinerung.

  Vorschlag zur Kongruenz

  Wenn du Meshtastic-konsistent sein willst, wäre die natürliche Anpassung: Static-Auto-Advert auch mit vanilla power (wie Moving). CR bleibt 5.
  Damit:

  ┌────────────────────────┬──────────────────────────────────────────┬─────┐
  │      Auto-Advert       │                  Power                   │ CR  │
  ├────────────────────────┼──────────────────────────────────────────┼─────┤
  │ Static (3h / 1h)       │ vanilla ← Änderung                       │ 5   │
  ├────────────────────────┼──────────────────────────────────────────┼─────┤
  │ Moving (15 min)        │ vanilla                                  │ 5   │
  ├────────────────────────┼──────────────────────────────────────────┼─────┤
  │ Nightly Flood (scoped) │ −6 dB (bleibt, da bewusst Netz-schonend) │ 5   │
  └────────────────────────┴──────────────────────────────────────────┴─────┘

  Das wäre konsistent zur Meshtastic-Logik "Position-Beacon braucht Reichweite". Allerdings: die "wir-sind-am-Standort-bekannt"-Begründung trifft auf
  Repeater-Setups durchaus zu. Geschmacks-/Use-Case-Frage.

  Soll ich's so umstellen, oder beim aktuellen Verhalten bleiben?

✻ Sautéed for 1m 37s

❯ Bei meshtastic laesst sich die Hopweite einstellen, deshalb konnte ich Position feintunen.
  Bei meshcore mag die Community keine Adverts, keine Positionsbaken oder Telemetrie. Deshalb habe ich mich fuer geringere Leistung und CR5 bei
  Meshcore entschieden. Das begruendet den Unterschied in
    ┌───────────────────────────────┬─────────────────────────────┬────────────────────────────────────────────────────┐
    │           Use-Case            │      Meshtastic CLIENT      │                  MeshCore aktuell                  │
    ├───────────────────────────────┼─────────────────────────────┼────────────────────────────────────────────────────┤
    │ Position-Beacon               │ vanilla Power, CR=5, ≤2 hop │ Static: −6 dB, CR=5 ← abweichend!
  und
    ┌────────────────────────┬──────────────────────────────────────────┬─────┐
    │      Auto-Advert       │                  Power                   │ CR  │
    ├────────────────────────┼──────────────────────────────────────────┼─────┤
    │ Static (3h / 1h)       │ vanilla ← Änderung                       │ 5   │
    ├────────────────────────┼──────────────────────────────────────────┼─────┤
    │ Moving (15 min)        │ vanilla                                  │ 5   │
    ├────────────────────────┼──────────────────────────────────────────┼─────┤
    │ Nightly Flood (scoped) │ −6 dB (bleibt, da bewusst Netz-schonend) │ 5   │
    └────────────────────────┴──────────────────────────────────────────┴─────┘
  Ich ueberlege tatsaechlich, ob der Moving Fall (15min) bei meshcore nicht auch mit -6 dB gemacht werden sollte.
  Oh moment, bei MC unterscheiden wir ja zwischen zero-hop advert und flooding (repeatbar). Kannst Du mir das nochmal genauer darstelen was zero-hop
  und was flooded ist? Position miiving 15 min mit vanilla (high power) kann schon sinnvoll bleiben bei voller Leistung wenn das zero-hop advert ist

⏺ Sehr gute Differenzierung. Hier die saubere Zuordnung was wird wie gesendet:

  Alle eigenen Aussendungen — Route-Typ

  Auslöser: Static Auto-Advert (3h / 1h)
  Route: sendZeroHop() → ROUTE_TYPE_DIRECT, path_len=0
  Kann repeated werden?: ❌ Mesh-Layer blockt zero-hop
  ────────────────────────────────────────
  Auslöser: Moving Auto-Advert (15 min)
  Route: sendZeroHop() → ROUTE_TYPE_DIRECT, path_len=0
  Kann repeated werden?: ❌
  ────────────────────────────────────────
  Auslöser: Nightly Flood (scoped)
  Route: sendFlood(codes, ..., path_hash_size=3) → ROUTE_TYPE_TRANSPORT_FLOOD
  Kann repeated werden?: ✅ bis ~21 Hops
  ────────────────────────────────────────
  Auslöser: App-Knopf zero-hop Advert
  Route: sendZeroHop()
  Kann repeated werden?: ❌
  ────────────────────────────────────────
  Auslöser: App-Knopf Flood Advert
  Route: sendFloodScoped() (mit default_scope_key)
  Kann repeated werden?: ✅
  ────────────────────────────────────────
  Auslöser: App-getriggerte Channel-Nachricht
  Route: sendFloodScoped()
  Kann repeated werden?: ✅
  ────────────────────────────────────────
  Auslöser: App-getriggerte Direct-Message an Kontakt
  Route: sendDirect() wenn Pfad bekannt, sonst sendFloodScoped()
  Kann repeated werden?: ✅ wenn Flood
  ────────────────────────────────────────
  Auslöser: App-getriggerter PATH-Request
  Route: meist sendFloodScoped()
  Kann repeated werden?: ✅

  Die Sperre für zero-hop liegt in src/Mesh.cpp:77:
  if (pkt->isRouteDirect() && pkt->getPathHashCount() > 0) {
    // ... only forward if pathHashCount > 0 — zero-hop is excluded by design
  }
  Zero-hop Pakete (ROUTE_TYPE_DIRECT mit path_len = 0) gehen einfach durch die Receiver durch, kommen aber nie in den
  routeRecvPacket/allowPacketForward-Pfad.

  Konsequenz für moving Auto-Advert mit voller Power

  Moving-Advert ist zero-hop, kann nicht repeated werden → kein Netz-weiter Flood. Bei voller Power erreichst du nur die Knoten in direkter
  Funkreichweite deiner aktuellen Position. Im Auto fährst du durch verschiedene "Funkbereiche" und wirst von immer wechselnden Nachbarn gesehen — was
   ja der Sinn ist.

  Trade-off:
  - Pro full power: bessere Tracking-Sichtbarkeit (z.B. Autobahn-Repeater 5 km weg sieht dich noch)
  - Contra full power: 4 Aussendungen/h × X Knoten = lokaler Funkkanal-Verbrauch. Bei dichtem Tracker-Verkehr summiert sich das.

  Mein Vote: 15-min moving lassen wir auf vanilla. Begründung:
  - Zero-hop = lokal, kein Echo durchs Netz
  - Bewegung impliziert tracking-Anwendung → User will Reichweite
  - 4× /h × ~440 ms (CR5 SF8 BW62.5) = 1.8 s Airtime/h pro Knoten — auch bei vielen Trackern noch OK

  Wenn du es trotzdem reduzieren willst

  Eine Zwischen-Variante: −3 dB (statt −6 dB) für moving — also halbe Power. Reichweite ungefähr halbiert (in dichter Umgebung), Strom-Ersparnis
  minimal (Senderleistung-Anteil an Idle-Strom ist klein). Aber: zusätzliche Komplexität.

  Mein Eindruck: bei der jetzigen Einstellung bleiben. Wenn du im Test feststellst dass moving-Reichweite "zu groß" wirkt (z.B. Funkkanal-Verbrauch in
   deinem Mesh stört), dann anpassen. Soll ich's so lassen?

✻ Brewed for 55s

❯ Und bei meshtastic koennte ich zwar patchen dass die Positionsinfobake nur direkt gesendet wird im Falle von smart-beaconing - aber das moechte ich
   nicht. Meshtastic ist fuer Positionen und Telemetrie gedacht.

⏺ Genau, und das ist konsistent in deiner aktuellen MeshCore-Implementierung:

  Column 1: Static Position-Beacon
  Meshtastic: vanilla (Beacon ist Kernfeature, soll weit)
  MeshCore (deine Variante): −6 dB (Community-Konsens: zurückhaltend sein)
  ────────────────────────────────────────
  Column 1: Telemetrie
  Meshtastic: vanilla (Telemetrie ist Kernfeature)
  MeshCore (deine Variante): nicht periodisch (Telemetrie nur on-request)
  ────────────────────────────────────────
  Column 1: Flood-Adverts (Nightly)
  Meshtastic: n/a
  MeshCore (deine Variante): −6 dB + 3-Byte Path (extra netzschonend)
  ────────────────────────────────────────
  Column 1: Moving Position
  Meshtastic: vanilla, ≤2 hops (Tracking ist Sinn der Sache)
  MeshCore (deine Variante): vanilla, zero-hop (Tracking ja, aber nicht ins Netz fluten — durch zero-hop sowieso lokal begrenzt)

  Die −6 dB bei statischen Adverts spiegeln das MeshCore-Ethos wieder, während Meshtastic für Position-Use-Cases konzipiert ist und daher Beacons mit
  voller Reichweite akzeptiert.

  Moving 15-min mit vanilla Power ist auch konsistent: zero-hop kann durch den Mesh-Layer niemals Netz-Echo erzeugen → die Bedenken der
  MeshCore-Community (Repeater-Last) sind dort nicht relevant. Volle Power schadet keinem.

  Damit ist die jetzige Konfiguration für mich der gute Kompromiss: bewusst zurückhaltend gegenüber dem MeshCore-Netz, aber tracking-tauglich wenn der
   Use-Case es ergibt.

  Nichts zu ändern — du hast die richtige Logik gebaut.

✻ Crunched for 27s

❯ > bei der jetzigen Einstellung bleiben
  Danke fuer diese Analyse. Das ist genau auch meine Einschaetzung und wir haben das gut designt. Es bleibt so, und erklaert die Unterschiede von MC
  und MT, da wir andere Paket-Typen und Community-Ansprueche haben.

⏺ Schön — dann ist der Stand klar dokumentiert und designt-by-intent. Drei DL9SAU-Commits liegen lokal:

  1. 70e2042d — Repeater + Adverts + GPS + STATS
  2. c6b3d7e2 — ISM-Cleanup + Freq-Anzeige
  3. 56bf39aa — Flood-Repeat Power-Differenzierung

  Verhalten beim Power/CR-Management entspricht jetzt deinem Mental-Modell: zurückhaltend gegenüber dem MeshCore-Netz (Community-Ethos), aber
  tracking-tauglich beim moving Auto-Advert (zero-hop = harmlos).



================================================================================
Companion-Repeater: Loop-Detection und Netz-Impact (Diskussion 2026-05-27)
================================================================================

Frage: koennen wir als Companion-Repeater Loops verursachen, da wir keine
eigene Caching-Logik haben wie ein full repeater?

Antwort: zwei separate Mechanismen, beide auch in der Companion-Firmware
aktiv:

1) De-Duplication-Hash (8 Byte, hasSeen-Cache)
   - SHA-256(packet) erste 8 Bytes -> 64-bit Hash
   - SimpleMeshTables: 128 Slots Ring-Buffer + 64 Slots fuer ACK-CRCs
   - Aufruf VOR allowPacketForward() in Mesh.cpp -> Duplikate kommen gar
     nicht erst zum Forwarder
   - Statistik: getNumDirectDups / getNumFloodDups

2) Path-Hash (1/2/3/4 Byte pro Hop, im Packet-Header packet->path[])
   - "Wer war schon Hop in diesem Paket?"
   - path_hash_size bestimmt wie viel Bytes pro Hop in der Hop-Liste
     gespeichert werden (1 Byte = Default, hoehere Werte caps MAX_PATH_SIZE
     auf weniger Hops)
   - Mesh::routeRecvPacket ruft removeSelfFromPath() -> wenn wir bereits in
     der Hop-Liste sind, kein Repeat
   - Bei 1 Byte (256 Werte) gibt es Kollisionen (= "false-positive Loops");
     bei 2/3 Byte praktisch null Kollisionen
   - Loop-Erkennung greift auch wenn der hasSeen-Cache schon einen
     verdraengten Hash hat (siehe Artikel: https://nodakmesh.org/blog/meshcore-path-hash-explained )

Beide Mechanismen sind UNABHAENGIG und greifen kumulativ.

  Impact des Companion-Repeaters aufs Netz:

  ┌──────────────────────┬──────────────────────────┬──────────────────────────────┐
  │       Faktor         │        Companion         │         full Repeater        │
  ├──────────────────────┼──────────────────────────┼──────────────────────────────┤
  │ hasSeen-Cache        │ 128 x 8 Byte (gleich)    │ 128 x 8 Byte                 │
  ├──────────────────────┼──────────────────────────┼──────────────────────────────┤
  │ Path-Hash-Selfcheck  │ removeSelfFromPath ja    │ removeSelfFromPath ja        │
  ├──────────────────────┼──────────────────────────┼──────────────────────────────┤
  │ Hop-Cap              │ 16 (CR_MAX_REPEAT_PATH)  │ meist 64 (konfigurierbar)    │
  ├──────────────────────┼──────────────────────────┼──────────────────────────────┤
  │ Filter               │ scoped-only + PATH-local │ alles                        │
  ├──────────────────────┼──────────────────────────┼──────────────────────────────┤
  │ TX-Power-Reduktion   │ -6 dB                    │ full                         │
  ├──────────────────────┼──────────────────────────┼──────────────────────────────┤
  │ Duty-Cycle           │ 80% soft / 100% hard SW  │ meist nur HW-Brownout-Schutz │
  ├──────────────────────┼──────────────────────────┼──────────────────────────────┤
  │ Direct-routed Pakete │ wir repeaten NICHT       │ Repeater repeated            │
  └──────────────────────┴──────────────────────────┴──────────────────────────────┘

Companion-Repeater ist DEUTLICH konservativer als ein full repeater:
  - Loop-Risiko praktisch null (8-Byte-Hash * 128 Slots + Path-Selfcheck
    plus 16-Hop-Cap)
  - Netz-Impact mehrere Groessenordnungen geringer
  - Theoretischer Cache-Druck: bei >128 Paketen in kurzer Zeit koennten
    aeltere hashes verdraengt werden. Bei EU 869 Narrow-Band ~1000 bps
    Bitrate praktisch unerreichbar.

Klarstellung zur Begrifflichkeit "1/3-byte-Hash":
  - De-Duplication-Hash = 8 Byte (sehr kollisionsresistent)
  - path_hash_size = 1/2/3/4 Byte (Multi-Hop-Trail im Packet-Header,
    NICHT De-Dup; haengt mit Routing-Reichweite zusammen)
  Die zwei sind orthogonal.



================================================================================
Companion-CLI — Uebersicht (Stand 2026-05-28)
================================================================================

Der Companion hat eine textbasierte Konfig-Schnittstelle ueber den lokalen
"companion"-Channel (Changelog 14q). Befehle werden als TXT-Message in
diesen Channel geschickt, Antworten kommen als synthetische incoming-
Channel-Messages zurueck. Erreichbar via App (BLE oder USB-Frame-Bridge).
Befehle und ihre Top-Level-Argumente sind per Prefix abkuerzbar
(match_choice, 14r); zerstoerende Aktionen (reset, remove, clear) muessen
voll ausgeschrieben werden (no_abbrev-Schutz).

System-Befehle:
  help / help <topic>     Hilfe-Index bzw. Detail zu einem Befehl
  status                  Firmware-Version, Uptime, GPS, Adv/Digi-Counter
  uptime                  nur Uptime
  stats                   detaillierte Stats nach Pkt-Typ + Airtime
  reboot                  Deferred reboot (3s delay fuer Frame-Auslieferung)
  clock / date / time     RTC anzeigen (ohne Arg: Unix-sec, UTC, lokal)
  time <unix-epoch>       RTC setzen (Sanity 1500000000..4000000000)
  gps [on|off|sync]       GPS-Modul-Kontrolle
  gps power [...]         GPS Power-Cycle-Mode (Wunschliste-1 verwandt)
  neighbors               Contacts mit Advert <48h, sortiert nach hop+SNR
  tempradio <f,b,s,c>     Temporaere Radio-Parameter (kein savePrefs)
  ver / board             (existieren noch nicht — Wunschliste-folgende)

App-Settings (CommonCLI-Mirror, Changelog 14r/14v):
  set <key> <value>       persistente Settings setzen
  get <key>               Einzelwert lesen
  get                     nur geaenderte Settings (analog 'prefs')
  get all                 alle Settings mit [default]-Marker

  Keys: name, freq, sf, bw, cr, tx_power, lat, lon, repeat, gps,
  gps_interval, advert_loc_policy, airtime_factor, rx_boosted_gain,
  manual_add_contacts, multi_acks, path_hash_mode, autoadd_config,
  autoadd_max_hops, telemetry_mode_base/loc/env, buzzer_quiet, rxdelay,
  txdelay, direct_txdelay. (ble_pin bewusst NICHT exposiert.)

DL9SAU-spezifische Settings:
  prefs                   nur veraenderte DL9SAU-Vars (inkl. Registry)
  prefs all               alle DL9SAU-Vars mit [default]-Marker
  prefs reset             alle DL9SAU-Vars auf Default; Registry-Eintraege
                          bleiben, nur deren Repeat-/Geo-Flags werden
                          gecleared

Trace (RAM-only Live-Logging + persistente Auswahl, Changelog 14r):
  trace [list]            Kategorien-Uebersicht
  trace <cat> on|off      Kategorie ein/aus (in Auswahl + aktiv)
  trace on / trace off    aktiv = gespeicherte Auswahl / pausiert
  trace all on|off        beide auf alle/keine

Repeater + Duty-Cycle:
  repeater [on|off] [force]   client_repeat-Master-Schalter + ISM-Check
  duty [soft|hard <N>|reset]  Duty-Cycle-Schwellen (80% / 100% Default)
  duty                        Status (current/soft/hard + blocked counter)
  autoadv [on|off|zerohop|nightly] periodische Auto-Adverts (15min/1h/3h)
  chatname [default|N|custom|hex]  Sendername-Modus fuer Group-Channels

Advert (Manual + Status):
  advert [zero-hop|flood]    sofort senden (Default zero-hop)

Scope-System (Changelog 14t + 14v..14z):
  scope                       Status der drei Send-Quellen (default/bake/
                              override) + Registry-Count
  scope default <name>|clear  Send-Default (wirkt auch fuer regulaere
                              Sends, gleich App-Default)
  scope bake <name>|clear     Nightly-Bake-Scope (darf weiter sein als
                              default, z.B. de-be -> de-bebb)
  scope override <name> [12h|3d]|clear  Hoechste Send-Prioritaet,
                              persistent ueber Reboots, max 30d TTL

  Scope-Storage (Schritt 11 Pivot, ab 2026-05):
    Build-in-Tabelle (read-only): ~30 Eintraege, kommen aus
      dl9sau_geo_recommendations.cpp — Bundeslaender, Aggregate, plus
      Special-Scopes local/lokal/region/regional/local-discard.
    Build-in-Status-Array (sparse, max 32 Slots): User-Override eines
      Build-in-Eintrags. Indexiert via FNV-1a Hash des Namens, nicht
      via Position — ueberlebt Tabellen-Reorder.
    User-Extras (max 16 Slots): Eintraege mit Namen die NICHT in der
      Build-in-Tabelle stehen (Kiez-Scope, eigener Vereins-Scope).

  Browsing:
    scope list                Eintraege mit Repeat-Mode != off
    scope list all            ALLE Eintraege (auch off/deleted)
    scope info <name>         hash, mode, advert, bbox, in_bbox, etc.
    scope regions             Build-in Geo-Tabelle (read-only)

  Anlegen (nur fuer Namen NICHT in Build-in-Tabelle):
    scope add <name> [<lat_min,lon_min,lat_max,lon_max>]
                              Eintrag in User-Extras; mit Bbox -> per
                              Default auto-managed (geo). '#'-optional
    scope remove <name>       Extras: Eintrag loeschen; Build-in:
                              user_deleted + repeat=off (no_abbrev)

  Per-Eintrag Aktion:
    scope <name> pin          immer aktiv (Tag P)
    scope <name> geo          aktiv wenn GPS in Bbox (Tag A, ausserhalb A-)
    scope <name> off          nie aktiv
    scope <name> disable      temporaer aus (Mode bleibt erhalten)
    scope <name> enable       temporaer wieder an
    scope <name> delete       dauerhaft verstecken (nur Build-in)
    scope <name> undelete     wieder herstellen
    scope <name> advert auto|off  Geo-Send-Fallback ein/aus
    scope <name> info         Details
    'repeat <verb>' bleibt als alter Pfad erlaubt (verb = on/off/auto/pin/geo).

  Globale Policy:
    scope repeater                       Status + Liste
    scope repeater mode all|allowlist    all = jeden scoped repeaten,
                                         allowlist = nur Eintraege im
                                         Repeat-Set

  Flags in scope list:
    R = aktiv jetzt  (Mode-aktiv UND nicht disabled)
    A = auto-mode (Bbox-Match)
    P = pin (immer aktiv)
    D = disabled (temporaer)
    X = deleted (versteckt)
    b = hat bbox
    ! = Eigenes geo auto-Advert waehlt diesen Scope nie aus
        (= chooseGeoFallbackScope skippt diesen Eintrag).
        Betrifft NUR die Auto-Auswahl meines eigenen Send-Adverts;
        Repeat-Verhalten und explizite 'scope advert bake/default/
        override' bleiben davon unberuehrt.

  Flags in scope rep:
    (A)  aktiv (auto Bbox)
    (A-) inaktiv (auto Bbox — ausserhalb)
    (P)  pin (immer aktiv)
    (D)  disabled

  Special-Scopes (Build-in, position-unabhaengig, Default-Pin):
    #local / #lokal           single-hop. Beim Repeat wird der Scope
                              zu #local-discard umgeschrieben, damit
                              kein zweiter Repeater drueber geht.
    #region / #regional       konfigurierbarer Hop-Cap; Default 3.
                              CLI: set scope_regional_hops <1..flood_max>
    #local-discard            SENTINEL — wird NIE repeated, auch nicht
                              im 'scope repeater mode all'-Modus. Der
                              Block ist im allowPacketForward fest
                              verdrahtet (vor der allowlist-Pruefung).
                              Damit ist die Single-Hop-Garantie von
                              #local / #lokal sicher: nach dem Rewrite
                              durch den ersten Repeater landet das
                              Paket auf #local-discard und KEIN weiterer
                              Repeater leitet es weiter.
                              Diese Eintraege koennen mit 'scope advert
                              bake/default/override <name>' explizit
                              als eigener Send-Scope gewaehlt werden;
                              das geht heute schon — '!' (advert auto
                              off) auf diesen Eintraegen war frueher
                              gesetzt aber funktional No-Op (Wunschliste
                              14), heute deshalb entfernt.

Wire-Frame-Limit:
  pushCompanionMessage prefixt mit Sender-Name (z.B. 'Generic ESP32: ',
  ~15 Byte); MAX_TEXT_LEN = 160. Effektives Text-Budget ~145 Byte pro
  Message. Multi-Output-Befehle (prefs all, get all, scope list, scope
  regions) flushen vor jedem Append bei Threshold 130 — keine Wire-
  Truncation, mehrere Messages werden geschickt. (14y)


================================================================================
Konzept-Erklaerungen (Stand 2026-05-29)
================================================================================

Send-Scope-Hierarchie (wo greift was):
  Eigene Auto-Adverts und Channel-/Recipient-Sends gehen durch eine
  Hierarchie. Reihenfolge (hoechste Prio zuerst):
    1) override   temp Pin mit TTL, ueberlebt Reboot.
                  CLI: scope advert override <n> [<h>h|<d>d]
    2) bake       persistent. WIRKT NUR fuer NIGHTLY-Flood-Advert.
                  CLI: scope advert bake <n>
                  Beispiel: default=#de-be, bake=#de-bebb -- Tag-Sends
                  bleiben auf Berlin, einmal pro Nacht weiter via
                  Brandenburg-Bridge.
    3) default-oder-geo (per scope_advert_auto):
                  off:    Geo nie verwendet
                  on:     Default gewinnt, Geo nur als Fallback wenn
                          Default leer  (Default-Verhalten)
                  prefer: Geo schlaegt Default wenn oertliche Region
                          eine andere ist als das Default
                  CLI: scope advert auto off|on|prefer
                  CLI: scope advert default <n>
    4) chooseGeoFallbackScope -- zweite Geo-Chance.
    5) #local (LAST-RESORT) -- single-hop, harmlos, immer verfuegbar.
                  Greift nur wenn ALLES andere leer ist.

  'scope' und 'scope advert' (no-arg) zeigen den vollen State plus
  Annotation, was gerade aktiv ist und warum.

Repeater Profile:
  defensive (Default) -- 'client-Repeater':
    PATH-Discovery nur fuer lokal-bekannte Endpoints (HeardList <48h
    ODER Contact <48h). Repeats mit reduzierter TX-Power (-6 dB,
    Floor 10 dBm) + forced CR5.
  normal -- 'wie ein echter Repeater':
    PATH-Discovery alle Pakete. Repeats mit voller Power +
    konfigurierter CR.
  CLI: repeater profile [defensive | normal]

Loop-Detection (nur in profile=normal aktiv):
  off (Default) | minimal | moderate | strict
  Profile=defensive braucht keine Loop-Detection (PATH nur lokal,
  Schleifen unmoeglich). Status-Ausgaben kennzeichnen das mit
  '(inaktiv -- profile=defensive)'.
  CLI: set loop_detect off|minimal|moderate|strict

flood_max:
  Globale Hop-Obergrenze (1..64, Default 16). Wirkt in beiden
  Profiles. CLI: set flood_max <N>

Discoverability (advert_role + owner_info):
  Companion advertet+antwortet je nach effectiveAdvertRole():
    advert_role=auto:
      client_repeat=0                      -> CHAT
      client_repeat=1 + profile=defensive  -> CHAT (chat-tauglich)
      client_repeat=1 + profile=normal     -> REPEATER (vollwertig)
    advert_role=fixed chat|repeater|sensor|room -> fester Type.

  Antwort-Endpoints (gegated by Role):
    Anon (jeder kann fragen):
      BASIC (Clock + Features)       -- CHAT/REPEATER/SENSOR
      OWNER (Name + owner_info)      -- nur REPEATER
      REGIONS (Repeat-Set)           -- nur REPEATER
    Authenticated (Contact mit shared_secret):
      GET_STATUS                     -- nur REPEATER
      GET_OWNER_INFO                 -- nur REPEATER
      GET_NEIGHBOURS                 -- nur REPEATER
      GET_TELEMETRY_DATA             -- nach telemetry_mode_*
  CLI: advert role [auto | fixed <type>]
  CLI: set owner_info <text>  ('|' wird zu Newline, max 119 Zeichen)

Runtime-Neighbours (fuer GET_NEIGHBOURS):
  RAM-Tabelle, gefuellt aus zero-hop gehoerten Adverts.
  Max 32 Eintraege, LRU. Alle adv_types ('liberal').
  Funktioniert ab Boot ohne RTC -- 'heard seconds ago' aus
  millis()-Delta wenn RTC nicht gesetzt.
  Augmentation: juengere Contacts (lastmod < 1 Woche) werden
  zusaetzlich beigemischt -- nur wenn RTC gesetzt.
