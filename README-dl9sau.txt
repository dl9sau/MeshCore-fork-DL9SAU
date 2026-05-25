
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

