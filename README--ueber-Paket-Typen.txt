Wie lassen sich die fielen REQ und RESP Pakete erklaeren? Fuer Group Text braucht es   
  die ja nicht, und fuer private messages doch auch nicht. Wohl auch nicht fuer          
  traceroute.                                                                            
  Adverts (die ja ca 1.1s tx time benoetigen liegen mit 17.5% auf Platz 3                

⏺ REQ und RESP sind das authenticated Request/Response-Subprotokoll zwischen Companion
  und Repeater/Sensor. Nicht für normales Chatten. Konkrete Quellen für hohes Aufkommen:

  Was über REQ/RESP läuft:
  - REQ_TYPE_GET_STATUS — Repeater-Statistiken (Uptime, Pakete, RAM)
  - REQ_TYPE_GET_OWNER_INFO — Betreiber-Info
  - REQ_TYPE_GET_NEIGHBOURS — Repeater's Nachbar-Liste
  - REQ_TYPE_GET_TELEMETRY_DATA — Sensor-Telemetrie (Batterie, Temperatur usw.)
  - LOGIN (sendLogin) — Repeater-Auth (passwortgeschützt)

  Was NICHT über REQ/RESP läuft (du hast richtig analysiert):
  - Group Text → PAYLOAD_TYPE_GRP_TXT
  - Private DM → PAYLOAD_TYPE_TXT_MSG
  - Traceroute → PAYLOAD_TYPE_TRACE
  - ACKs → PAYLOAD_TYPE_ACK
  
  Wahrscheinliche Erklärungen für die hohe Anzahl:

  1. First-Contact-Initial-Phase. Jede Companion-App macht beim ersten Anschreiben eines
  Repeaters einen FLOOD-typed REQ (Path-Discovery + Request). Sobald ein Pfad steht, geht
   alles direct (zählt nicht mehr in FLOOD-Statistik). Bei wachsendem Netz mit vielen
  Usern × vielen Repeatern = viele Erstkontakte.
  2. App-Auto-Polling. Manche Companion-Apps (besonders auf iOS/Android) holen periodisch
   Status von allen bekannten Repeatern beim Tab-Wechsel oder App-Resume. 1 User × 5
  Repeater × 4× pro Stunde = 20 REQ-RESP-Paare/h pro User.
  3. Repeater-Monitoring-Tools. Der Analyzer selbst dürfte ein nennenswerter Quelle sein
  — er muss ja irgendwoher seine Daten ziehen. Wenn er REQ_TYPE_GET_STATUS-Polls auf alle
   Repeater im Netz macht, summiert sich das. (Ironie: Tool das den Verkehr misst,
  beeinflusst ihn.)
  4. Sensor-Polls. Wenn die Community schon SENSOR-Nodes hat, werden die mit
  REQ_TYPE_GET_TELEMETRY_DATA abgefragt. Pro App-Öffnung des Sensor-Kontakts = 1 REQ.
  5. Login-Versuche. Bei passwortgeschützten Repeatern: jeder Login = 1 REQ + 1 RESP.

  Zur 17.5%-Advert-Zahl: wenn der Analyzer Paket-Count zählt, sind 17.5% Adverts viel —
  bei Airtime-Count (1.1s pro Advert vs ~0.4s pro REQ) wäre der Adv-Anteil ein Vielfaches
   davon. Aussagekraft hängt davon ab. Bei Paket-Count ist die wichtigere Frage: wie oft 
  sendet ein Repeater seine Adverts? Standardmäßig alle 60min — viel zu kurz für stabile
  Infrastruktur. Bei 500 Repeatern × 1 Advert/h × FLOOD-Hop-Faktor x = quickly dominant
  in der Statistik.

  Was du selbst beobachten kannst: mit deinem rx flood -- repeated req=N rsp=N Counter
  siehst du jetzt direkt was du persönlich an REQ/RESP-Traffic mitkriegst. Wenn req+rsp
  die Mehrheit dort sind, deckt sich's mit der Community-Statistik.

✻ Cooked for 1m 1s

❯ Danke fuer den Ueberblick. Was laeuft ueber ANON_REQ, was ueber CONTROL?               
                                                                                         
  Um mehr zu lernen habe ich                                                             
  -  einen login bei einem repeater versucht (den ich aber gerade nicht hoere)           
  -  gerade einen Kontakt als Sensor umbenannt und Telemetrie abgefragt                  
  Dabei schaue ich mir tx-self-initiated an Statistik an.                                
  repeater login setzte keinen der Zaehlr hoch. Erwartet haette ich dass req hoch        
  zaehlt.                                                                                
  Dann habe ich 2x Telemetrie bei einem Sensor gemacht und req= stand dann auf 2.        
  Dann habe ich ping zero-hop in der App an einen Repeater gemacht. trc ging auf 1.      
  ping zero-hop ist vermutlich ein traceroute mit type DIRECT, richtig?                  
  
  Searched for 3 patterns, read 1 file (ctrl+o to expand)

⏺ Sehr gute Beobachtungen — und du hast einen echten Display-Gap entdeckt:

  ANON_REQ (0x07): unauthenticated Erstkontakt-Queries

  Wird verwendet wenn noch kein shared secret etabliert ist. Der Sender nutzt einen
  ephemeral keypair, der Empfänger kann den Dest-Hash matchen ohne den Sender vorher zu
  kennen. Konkrete Nutzer:

  - sendLogin → genau hier! Login geht über ANON_REQ (BaseChatMesh.cpp:603
  createAnonDatagram(PAYLOAD_TYPE_ANON_REQ, ...))
  - sendAnonReq generisch
  - ANON_REQ_TYPE_OWNER — Owner-Info ohne Login
  - ANON_REQ_TYPE_REGIONS — Regional-Scope-Info
  - ANON_REQ_TYPE_BASIC — Basic-Info / Clock-Sync

  CONTROL (0x0B): mesh-level admin / discovery

  Spezielle Handling-Regel (Mesh.cpp:73): nur DIRECT-routed CONTROL mit payload[0] & 0x80
   triggert onControlDataRecv. Hauptnutzer in MC:

  - CTL_TYPE_NODE_DISCOVER_REQ/RESP — Repeater-Discovery zwischen Repeatern, via
  sendZeroHop. Companion-App nutzt das nicht für normalen Verkehr.

  Im Companion-Build sehr selten — taucht im Wesentlichen auf wenn anderes Equipment in
  der Nähe Discover-Pings macht.

  ---
  Deine Test-Befunde interpretiert:

  1) Login-Versuch → req= nicht hochgezählt:
  Display-Bug, kein Code-Bug. Login geht über ANON_REQ, aber meine Stats-Display-Zeile
  zeigt nur adv=... path=... txt=... grp=... ack=... req=... rsp=... trc=... — ANON_REQ 
  ist nicht dabei. Die TX-Counter _tx_total_by_ptype[7] haben den Login zwar hochgezählt,
   aber das ist in keiner Spalte sichtbar.

  Auch ANON_REQ in rx flood würde nicht sichtbar — selbe Spaltenauswahl.

  Fix-Vorschlag: anon= Spalte hinzufügen (idx 7). Bei Gelegenheit. Soll ich gleich noch
  heute machen oder morgen mit Tests-Feedback?

  2) Sensor-Telemetry × 2 → req=2 ✓:
  Telemetry-Pull bei etabliertem Kontakt geht über PAYLOAD_TYPE_REQ (shared secret
  bekannt aus dessen Advert). Counter inkrementiert korrekt.

  3) Ping zero-hop → trc=1:
  Richtig vermutet. Das ist eine TRACE-Paket-Aussendung (PAYLOAD_TYPE_TRACE = 0x09),
  gesendet als DIRECT via sendZeroHop (Mesh.cpp:42 zeigt die Spezialbehandlung von
  "DIRECT-routed TRACE").

  In MeshCore-Protokoll-Vokabular: "Traceroute" = TRACE-Paket. "Zero-hop" = nur direkte
  Nachbarn (vs. multi-hop traceroute, das als FLOOD-typed TRACE ginge und durchs ganze
  Netz tracen würde).

  ---
  Soll ich kurz noch anon= ergänzen damit du morgen Login-Traffic siehst, oder reicht's
  dir auch ohne?

✻ Cooked for 1m 48s

❯ 1) Ok. Du hattest vorhin gesagt "Was über REQ/RESP läuft: "- LOGIN (sendLogin) —       
  Repeater-Auth (passwortgeschützt)". Deshalb dachte ich das zaehlt hoch. Jetzt hast Du  
  das zu ANON... korrigiert.                                                             
  2) Protokollfrage: weshalb hat man discover repeater (den ein companion kann und       
  vermutlich anon_req ist? oder zero-hop-trace - weil discover repeater, wie Du mir      
  erklaert hattest nur jene anzeigt von denen wir bereits ein advert hatten) separiert   
  von einem inter-repeater-CTL_TYPE_NODE_DISCOVER_REQ/RESP-Befehl?                       
                                                                                         
  3) ja anoon= machen wir beim naechsten Anfassen vom code                               

  Searched for 1 pattern, read 1 file (ctrl+o to expand)

⏺ Erst zur Selbstkorrektur: ja, in meiner ersten Antwort hatte ich Login als REQ
  aufgeführt — falsch. Sorry. Login ist ANON_REQ. Die anderen genannten Aufzaehlungen
  (Status, Owner-Info, GET_NEIGHBOURS, Telemetry) sind tatsächlich REQ — Login war der
  einzige Ausreißer in meiner Liste.

  ---
  Zur Protokoll-Designfrage — sehr gute Beobachtung.
  
  Erstmal die Tatsachen sauber sortieren, was die Companion-App tatsächlich kann:

  App-Aktion: Kontakte sehen
  Wire-Type: (kein Netzverkehr)
  Voraussetzung: Adverts wurden bereits passiv empfangen
  ────────────────────────────────────────
  App-Aktion: "Path Discovery"
  Wire-Type: PAYLOAD_TYPE_REQ (FLOOD-forced)
  Voraussetzung: Kontakt existiert bereits (shared secret)
  ────────────────────────────────────────
  App-Aktion: "Ping zero-hop"
  Wire-Type: PAYLOAD_TYPE_TRACE (DIRECT)
  Voraussetzung: —
  ────────────────────────────────────────
  App-Aktion: "Send Advert"
  Wire-Type: PAYLOAD_TYPE_ADVERT (FLOOD/ZeroHop)
  Voraussetzung: —
  ────────────────────────────────────────
  App-Aktion: Telemetry-Pull
  Wire-Type: PAYLOAD_TYPE_REQ
  Voraussetzung: Kontakt mit pub_key
  ────────────────────────────────────────
  App-Aktion: Login
  Wire-Type: PAYLOAD_TYPE_ANON_REQ
  Voraussetzung: Pub_key bekannt

  Eine aktive "discover all repeaters"-Funktion gibt es im Companion gar nicht in dem
  Sinn dass ein Knopf einen Probe-REQ ans Netz schickt um neue Repeater zu finden. Die
  "Repeater-Liste" der App ist ein Filter auf bereits gespeicherte Kontakte (Typ REPEATER
   aus deren Adverts) — kein Netz-Traffic.

  Das Companion-äquivalente "discover" passiert passiv:
  - Adverts kommen von selbst rein (Repeater senden alle 60 Min)
  - onDiscoveredContact pflegt die Kontaktliste
  - App zeigt was bereits da ist

  ---
  Warum dann CTL_TYPE_NODE_DISCOVER_REQ/RESP separat?
  
  Das ist eine Repeater-zu-Repeater-spezifische Mechanik mit anderen Anforderungen:

  1. Zeitdruck: Ein Repeater braucht seine direkten Nachbarn sofort beim Boot (für
  Routing-Entscheidungen, GET_NEIGHBOURS-Antworten). Auf den nächsten 60-Min-Advert
  warten ist zu langsam.
  2. Scope: Adverts werden via FLOOD durchs ganze Netz gepumpt (cost = O(Netzgröße)).
  CTL_TYPE_NODE_DISCOVER ist zero-hop only (cost = O(direkte Nachbarn)). Für reine
  Nachbarschafts-Bestimmung 100x billiger.
  3. Filter: CTL_TYPE_NODE_DISCOVER_REQ trägt einen filter-Byte (z.B. 1 << 
  ADV_TYPE_REPEATER) — nur Repeater antworten. Adverts haben kein Selectivity-Feature.
  4. On-Demand: Active Probe vs. passives Warten. Ein Repeater der gerade an einen neuen
  Standort gebracht wurde, kann selbst initiieren statt "vielleicht in einer Stunde".
  5. Asymmetrische Rollen: Companion (= Client) muss kein Topologie-Wissen haben.
  Repeater (= Infrastruktur) braucht es für Forwarding-Entscheidungen +
  GET_NEIGHBOURS-Antworten an anfragende User.

  Antwort auf deine "warum separiert?": weil das Design Companion = passiv, Repeater = 
  aktiv ist. Companion-Bedarf (Pfad zu bekannten Kontakt finden) wird mit der schon
  vorhandenen Authentifizierungs-Infrastruktur (REQ) erschlagen. Repeater-Bedarf
  (Nachbarschaft schnell wissen) bekommt seinen eigenen CTL-Slot weil Anforderungen
  anders sind. Hätte man's vereinheitlicht, müsste man das CTL-Privileg "darf scharfes
  Discover schicken" auch Companions geben — wäre Spam-Vektor.

  Im Endeffekt: zwei Mechanismen weil zwei verschiedene Use-Cases mit unterschiedlichen 
  Kosten-Profilen.

✻ Worked for 1m 41s

❯ Verstehe. Interessant das mit dem kurzen                                               
  repeater-to-repeaterCTL_TYPE_NODE_DISCOVER_REQ/RESP-Befehl. Allerdings treffen aktuell 
   repeater gar keine routing Entscheidung. Entweder sie repeaten, oder repeaten nicht   
  (flood max erreicht, oder keine passenden scopes)                                      

⏺ Sehr guter Punkt — du hast recht, ich habe da übertrieben mit "Routing-Entscheidungen".

  MeshCore hat aktuell tatsächlich kein klassisches Routing im Sinne von Path-Selection.
  Repeater entscheiden nur forward / not forward (binär), basierend auf:
  - client_repeat an?
  - duty soft/hard erreicht?            
  - flood_max (hop cap)?
  - flood_max_adv_infra für Infrastruktur-Adverts?
  - scope-allowlist hits?
  - loop_detect (in profile=normal)?

  Keine "welcher Nachbar als next hop?"-Entscheidung. Bei FLOOD: einfach broadcasten. Bei
   DIRECT-routed: der Pfad steckt schon im Paket (selbst nicht entschieden, sondern via
  isHashMatch auf den nächsten Slot geguckt).

  Damit ist meine ursprüngliche Begründung "schnell Routing-Wissen brauchen" nicht das 
  Hauptmotiv für CTL_TYPE_NODE_DISCOVER.

  Was es tatsächlich liefert:

  1. GET_NEIGHBOURS-Antwort-Befüllung. Wenn ein User per REQ_TYPE_GET_NEIGHBOURS einen
  Repeater abfragt, soll der sinnvoll antworten können — also braucht der eine
  Nachbar-Liste. Klar.
  2. Symmetrische SNR-Info. Schau dir DISCOVER_RESP an (simple_repeater MyMesh.cpp:792):
  data[1] = packet->_snr; — der Antwortende sendet seinen SNR-Empfangswert mit zurück.
  Damit weiß der Anfragende: "Ich höre dich mit -5 dB, du hörst mich mit -12 dB". Adverts
   liefern nur eine Richtung.
  3. Schneller Bootstrap. Beim Repeater-Boot dauert's bis Adverts hereinrollen (default
  60 Min Cadence). CTL aktiv proben = sofort.
  4. Selektivität. filter = 1 << ADV_TYPE_REPEATER — nur Repeater antworten. Adverts hört
   man von ALLEN Typen, muss man dann lokal filtern.

  Eigentlicher Use-Case ist also Observability / Diagnostics, nicht Routing.
  Topologie-Maps, Operator-Sicht "wen höre ich direkt", Symmetrie-Check für
  Antennen-Tuning. Dazu Sub-Sekunden-Bootstrap statt 60-Min-Warten.

  In unserem Companion-Build ist onControlDataRecv zwar vorhanden (MyMesh.cpp:2018), aber
   die Logik ist eher schmal — wir sind ja kein Repeater im Sinne von
  "Discover-Knotenpunkt". Companions interessieren sich nicht für Repeater-Nachbarschaft
  direkt.

  ---
  Damit kommt eine spannendere Frage: sollte MeshCore irgendwann mal echtes Routing
  bekommen (z.B. "Pfad mit niedrigster Latenz / höchster Qualität bevorzugen"), wäre
  CTL_TYPE_NODE_DISCOVER der natürliche Aufhänger weil die SNR-Asymmetrie schon
  transportiert wird. Aber das ist Zukunftsmusik — heutige Implementation = stumpf
  flooden.

  Also danke für die Korrektur — meine "Routing-Entscheidungen"-Formulierung war zu
  großzügig.



====

1) Full-repeater mode → senden + antworten wir?

  In simple_repeater ist's so:
  - Senden: nur on-demand via CLI discover.neighbors. Kein periodisches Aussenden.
  - Antworten: ja, auf eingehende REQ, mit Rate-Limit (discover_limiter(4, 120) = max 4
  pro 2 Min) + filter-Check + disable_fwd == false.
  
  In unserer Companion-Firmware aktuell: onControlDataRecv ist leer (MyMesh.cpp:2018
  returnt früh wenn App offline, sonst pusht den Frame an die App weiter). Wir antworten
  also gar nicht. Müsste man explizit einbauen.

  2) REQ/RESP Byte-Struktur

  REQ = 10 Bytes:
  [0] = CTL_TYPE_NODE_DISCOVER_REQ (0x80) | prefix_only-bit
  [1] = filter-Maske (z.B. 1<<ADV_TYPE_REPEATER = 0x04)
  [2..5] = tag (random uint32 für Matching)
  [6..9] = since (uint32 timestamp, 0 = "alle")

  RESP = 38 Bytes (full) ODER 14 Bytes (prefix_only):
  [0] = CTL_TYPE_NODE_DISCOVER_RESP (0x90) | adv_type (low 4 bits)
  [1] = SNR (Q4 = SNR×4) -- der EMPFANGENE SNR der REQ beim Antwortenden
  [2..5] = tag echo
  [6..N] = pub_key (32 byte full ODER 8 byte prefix)

  Name ist NICHT enthalten. Nur pub_key + adv_type + SNR.

  3) Reicht das für Contact-Liste?

  Nein, nur für Tentative Neighbor (pub_key + Reichweite-Bestätigung). Für eine
  vollwertige Contact-Liste brauchst du den Advert (= Name + signed timestamp + app_data
  + ggf. GPS).

  Pragmatische Strategie wäre: pub_key aus RESP merken, beim nächsten passenden Advert
  die Felder befüllen. Oder Advert vom Knoten triggern (gibt's kein passendes REQ_TYPE,
  müsste man via separater Mechanik).

  4) Airtime pro Paket (EU Narrow SF8/BW62.5/CR8)

  ┌─────────────┬───────┬─────────┬─────────┐
  │    Paket    │ Bytes │ Symbols │ Airtime │
  ├─────────────┼───────┼─────────┼─────────┤
  │ REQ         │ 10    │ 32      │ ~0.18 s │
  ├─────────────┼───────┼─────────┼─────────┤
  │ RESP prefix │ 14    │ 40      │ ~0.21 s │
  ├─────────────┼───────┼─────────┼─────────┤
  │ RESP full   │ 38    │ 88      │ ~0.41 s │
  └─────────────┴───────┴─────────┴─────────┘

  Worst case: 10 Repeater in Reichweite, alle antworten mit RESP full = 10 × 0.41 s = 4.1
   s Burst. simple_repeater jittert mit getRetransmitDelay*4 und limitiert per
  discover_limiter. Trotzdem nicht ohne — bei Spitzen-Density im urbanen Mesh kann das
  Funkfeld kurz dicht werden.

  Minimal-Antwort (1 Nachbar, prefix): 0.18 + 0.21 = 0.39 s round-trip.
  Maximal-Antwort (10 Nachbarn, full, sequentiell): 0.18 + 10×0.41 = 4.3 s + Jitter
  (mehrere Sekunden).

  5) Advert-Type für unsere Firmware

  Wenn ANTWORTEN: ja, sinnvoll nur wenn effectiveAdvertRole() == ADV_TYPE_REPEATER (= wir
   uns als Repeater advertisen). Sonst würden wir lügen — als Chat-Node behaupten "ich
  bin Repeater". Konkrete Checks beim Antworten:
  - effectiveAdvertRole() == ADV_TYPE_REPEATER (= wir sind effektiv Repeater)
  - _prefs.client_repeat != 0 (= aktiviert) und ggf. _prefs.repeater_profile == 1 (=
  full, nicht defensive)
  - filter & (1 << ADV_TYPE_REPEATER) aus der REQ (= wurden wir gemeint?)
  - discover_limiter.allow(...) (= Spam-Schutz)
  - discovery_mod_timestamp >= since (= unsere Konfig hat sich geändert)
  
  Wenn nur SENDEN (= "zeig mir Repeater in Reichweite" CLI-Diagnose im Companion): unser
  eigener advert-type ist irrelevant. Anyone can ask. Die Empfänger entscheiden ob sie
  antworten.

  ---
  Konkret für deine Wunschliste 27: zwei separate Features möglich:
  - a) Companion sendet REQ on-demand (Diagnose) — unabhängig vom advert-role
  - b) Companion antwortet auf REQs — nur wenn als REPEATER advertisiert

  (b) ist heikler weil es Air-Time kostet. (a) ist relativ harmlos (jittered Burst alle
  paar Minuten möglich).


  ===


❯ Wenn ich in app "Discover repeaters" auswaehle, bekommt unser code das dann auch mit?

⏺ Teilweise. Genauer:

  Wenn DU per App "Discover repeaters" auslöst:
  - Die App schickt einen CTL_TYPE_NODE_DISCOVER_REQ-Frame an unsere Firmware via BLE
  - Unsere Firmware sendet diesen REQ via sendZeroHop aufs Funk
  - Die Nachbarn empfangen + antworten
  - Die RESPs treffen unsere Firmware auf dem Funk und werden weitergeleitet an die App
  via PUSH_CODE_CONTROL_DATA

  Unser Code "sieht" den Verkehr also durchgehend (vor allem onControlDataRecv für die
  eingehenden RESPs). Aber: unsere discoverHandleResp-Logik (= "RESPs sammeln und in
  $companion als Tabelle ausgeben") wird nicht ausgelöst, weil _discover_active nur dann
  true ist wenn DU per discover-CLI angefangen hast. Bei App-getriggertem Discover macht
  die App die Auswertung selbst.

  Wenn jemand ANDERS (anderer Tracker) per App "Discover repeaters" auslöst und in
  unserer Funkreichweite ist:
  - Wir bekommen den REQ über onControlDataRecv mit
  - discoverableHandleReq entscheidet ob wir antworten (= nur im full-repeater-Mode)
  - Falls ja: wir antworten, der RESP landet bei deren Tracker

  ---
  Möchtest du dass unser Code App-initiated Discover auch mitloggt (z.B. in $companion
  "discover (App-initiated) heard from X")? Würde Doppelarbeit vermeiden — App zeigt's
  UND wir zeigen's. Aktuell macht's nur die App.

  Falls ja, müsste ich:
  - Beim CMD-Send vom App den REQ-Tag mitschneiden
  - RESPs mit App-Tag erkennen und parallel in $companion ausgeben
  - Aber: könnte für den User verwirrend sein (zwei Anzeigen).

  Mein Tipp: aktueller Zustand belassen. App-Trigger = App-Anzeige, CLI-Trigger =
  CLI-Anzeige. Saubere Trennung der Workflows.

  Soll ich was anpassen oder ist das OK?

✻ Cogitated for 49s

❯ > Bei App-getriggertem Discover macht
    die App die Auswertung selbst.
  Ok. Muss man auch nicht doppelt haben.

  Wir belassen es.

  Frage zu "discover prefix": ist es dieses inter-repeater-discover feature
  (CTL_TYPE_NODE_DISCOVER.) fuer schnelle discovery?

⏺ Ja — discover (mit oder ohne prefix) IST das CTL_TYPE_NODE_DISCOVER-Feature aus
  simple_repeater + simple_sensor. prefix ist nur ein Option-Flag innerhalb desselben
  Protokolls.

  Was prefix konkret tut:

  Im REQ-Payload-Byte[0] wird Bit 0 gesetzt (= prefix_only=1). Empfänger sehen das und
  antworten dann mit kürzerem RESP:
  - Ohne prefix: RESP enthält vollen 32-Byte pub_key → RESP = 38 Bytes total → ~0.41s
  Airtime
  - Mit prefix: RESP enthält nur 8-Byte pub_key-Prefix → RESP = 14 Bytes → ~0.21s Airtime

  Bei N Repeatern in Reichweite spart das pro REQ: N × 0.20s. In dichten Mesh-Regionen
  (z.B. 10 Repeater) = 2 Sekunden weniger Funkfeld-Belegung pro Discover-Burst.

  Trade-off: mit 8-Byte-Prefix können wir den Knoten nicht voll identifizieren — nur
  Match gegen schon-bekannte Kontakte (via lookupContactByPubKey(prefix, 8)). Unbekannte
  Nodes bleiben "anonym" mit nur Hex-Prefix. Mit prefix reicht das normalerweise für
  Diagnose ("ist X erreichbar?"), nicht für vollständige Topologie-Discovery.

  Nicht zu verwechseln mit path_hash_mode / path_hash_size:
  - path_hash steuert wie viele Bytes pro Repeater-Hop in den path[]-Field gepackt werden
   (1/2/3 byte) — das ist Flood-Routing, anderer Mechanismus
  - discover prefix steuert nur das RESP-Format, hat nichts mit Routing zu tun

  Beide sind "Airtime-Spar"-Optionen, aber an verschiedenen Stellen im Protokoll.

✻ Crunched for 23s

❯ Das ist verwirrend. Ist prefix nun fuer ausgehende Anfragen oder das eigene
  response-Verhalten bei eingehenden requests wenn wir im mode full-repeater sind?

⏺ Sorry für die Verwirrung — präzise:

  prefix ist eine Bitte AN den Antwortenden, beim Antworten kurz zu fassen. Es wird IN
  DER REQ gesetzt — der Sender sagt "antworte mir bitte mit 8-Byte-Prefix, nicht 32".

  Wer setzt es: der Sender (= die discover-CLI auf unserer Seite).
  Wer honoriert es: der Empfänger des REQ (= unser discoverableHandleReq falls wir
  gefragt werden, oder andere Repeater wenn wir fragen).

  Zwei Pfade konkret:

  Ausgehend (wir fragen):
  discover prefix
  → Bit gesetzt in unserer REQ → Nachbarn antworten kurz → wir sehen 8-Byte-pubkey

  Eingehend (wir werden gefragt, im full-repeater-Mode):
  bool prefix_only = (packet->payload[0] & 1) != 0;
  size_t resp_len = prefix_only ? (6 + 8) : (6 + PUB_KEY_SIZE);
  → wir lesen das Bit aus der eingegangenen REQ und antworten entsprechend kurz oder
  lang. Wir entscheiden NICHT selbst ob wir kurz antworten — die anfragende Seite hat das
   festgelegt.

  Es gibt also keinen Schalter "ich antworte immer kurz". Das wäre vermutlich auch nicht
  sinnvoll — der Anfragende weiß besser was er braucht.


===

Korrektur: CTL_TYPE_NODE_DISCOVER ist nicht repeater-zu-repeater-spezifisch. Korrekt
  ist:
  - Sender: jeder kann fragen — App-User, Repeater (via discover.neighbors-CLI), unser
  Companion (via discover-CLI)
  - Responder: nur Infrastruktur-Nodes (REPEATER + SENSOR). Companions/Chat antworten
  nicht. Room-Server auch nicht.

  Was wirklich am Protokoll separat ist (im Vergleich zu Adverts):
  - Zero-hop only (Adverts gehen via FLOOD durchs ganze Netz)
  - On-demand (Adverts sind periodisch im 60-Min-Zyklus)
  - Typ-Filter im Request (Adverts ohne Filter)
  - Symmetrische SNR-Info im RESP (Adverts nur eine Richtung)

  Die "Repeater-zu-Repeater"-Beschreibung war meine Fehl-Verkürzung weil ich auf den
  simple_repeater-Code geschaut hatte und übersehen hatte dass das Protokoll allgemeiner
  ist.

  Status in deiner Firmware:

  Use-Case: Du fragst per CLI discover
  Implementiert: ✅ (Wunschliste 27 a)
  ────────────────────────────────────────
  Use-Case: Du fragst per App "Discover repeaters"
  Implementiert: ✅ — geht durchs Firmware via existing CTL-Relay-Pfad (war schon vor
    meiner Änderung da)
  ────────────────────────────────────────
  Use-Case: Andere fragen, wir antworten (im full-repeater-Mode)
  Implementiert: ✅ (Wunschliste 27 b)

  Es fehlt also nichts.

  Wenn du in der App "Discover repeaters" drückst, schickt sie einen CMD-Frame an unsere
  Firmware, die baut den REQ und sendet via Funk. RESPs kommen rein → Firmware leitet sie
   via PUSH_CODE_CONTROL_DATA an die App, die wertet sie selbst aus. Das passiert ohne
  unsere discover*-CLI-Logik (= läuft parallel).


================================================================================
ANON_REQ_TYPE_REGIONS / OWNER / BASIC -- Response-Verhalten
================================================================================

Quellen: simple_repeater/MyMesh.cpp:565-604, BaseChatMesh.cpp:620-643,
companion_radio/MyMesh.cpp:1602+. Recherche-Session 2026-06-01.

--- Payload-Format der Anfrage (ANON_REQ) ---
  data[0..3]    timestamp (vom Sender; Empfaenger echoed dies als 'tag')
  data[4]       request type (1=REGIONS, 2=OWNER, 3=BASIC, sonst LOGIN)
  data[5]       reply_path_meta = ((hash_size-1) << 6) | (path_len & 63)
  data[6..]     reply_path bytes (path_len * hash_size byte)

  reply_path_meta = 0x00 bedeutet: erwarte zero-hop reply (kein Pfad).

--- Empfaenger-Logik (simple_repeater) ---
  ANON_REQ_TYPE_REGIONS:  nur wenn packet->isRouteDirect() -- d.h.
                          sender hat sendDirect oder sendZeroHop verwendet.
                          Flood wird ignoriert (gedroppt ohne Reply).
  ANON_REQ_TYPE_OWNER:    gleiche Regel.
  ANON_REQ_TYPE_BASIC:    gleiche Regel.
  LOGIN (data[4]==0 / >= ' '): akzeptiert auch flood (typisch fuer Login).

  Begruendung: ANON_REQ via flood ist teuer (multi-hop broadcast).
  Diese Info-Requests sind nicht zeitkritisch + sollten geziehlt sein,
  daher nur direct-routed.

--- Empfaenger-Logik (companion_radio, MyMesh.cpp:1644) ---
  Wir uebernehmen die gleiche Regel: ANON_REQ_TYPE_REGIONS nur dann
  beantwortet wenn packet->isRouteDirect() UND wir uns als REPEATER
  advertisen (effectiveAdvertRole() == ADV_TYPE_REPEATER).
  OWNER nur bei REPEATER, BASIC bei CHAT/REPEATER/SENSOR.

--- Reply-Pfad-Wahl (Responder-Seite) ---
  Falls reply_path_len > 0: sendDirect entlang reply_path (= Sender hat
    den Reverse-Pfad mitgeschickt; typisch fuer Multi-Hop).
  Falls reply_path_len == 0: sendDirect mit empty path -- effektiv
    zero-hop direct. Nur sinnvoll wenn Sender direkter Nachbar ist.
  Falls reply_path_len < 0 (default-Init, kommt nur ueber LOGIN-Pfad
    der reply_path_len = -1 setzt): sendFloodReply -- response wird
    geflooded zurueck. Macht zukuenftige Discovery moeglich, kostet
    aber Netz-Traffic.

--- Spezial: DL9SAU Phase 1b (BaseChatMesh.cpp, commit c5e02b26) ---
  In unserer Firmware ist ein Patch drin der bei PAYLOAD_TYPE_REQ /
  PAYLOAD_TYPE_TXT_MSG / Path-Returns generell zero-hop antwortet
  wenn das Request-Paket path_len==0 hatte (= Sender war direkter
  Nachbar). Upstream-simple_repeater (ohne diesen Patch) floodet
  Responses sobald das Original-Paket isRouteFlood() war.

  Konsequenz: in unserer Firmware sind ANON-/REQ-Round-Trips zwischen
  direkten Nachbarn vollstaendig zero-hop. Upstream-Repeater (die unsere
  Patch nicht haben) wuerden ihre Responses bei flooded incoming
  fluten. Bei zero-hop incoming senden sie aber sendDirect-with-empty-
  path zurueck (gleichwertig zu zero-hop) -- so dass auch dort der
  Round-Trip nicht das Netz belastet.

--- Pubkey-Anforderung beim Sender ---
  ANON_REQ-Payload wird mit shared_secret = ECDH(sender.prv, dest.pub)
  verschluesselt. Sender braucht den vollen Empfaenger-Pubkey (32 byte).
  Auf dem Wire steht aber nur 1-Byte dest_hash (PATH_HASH_SIZE=1, siehe
  Identity.h:19 copyHashTo). Hash-Kollisionen (1:256) sind moeglich --
  nur der echte Empfaenger entschluesselt sauber wegen MAC-Verify.

--- Multi-Hop ANON_REQ_TYPE_REGIONS in der Praxis ---
  Theoretisch moeglich via sendDirect entlang gespeichertem
  ContactInfo.out_path mit reverse(out_path) als reply_path. Erfordert
  aber:
    1) Symmetrie-Annahme: Reverse-Pfad == Reverse(out_path).
    2) Storage der Hash-Size-Encoding fuer out_path (uint8_t Feld in
       ContactInfo ist ambivalent).
  Nicht in unserer aktuellen Firmware -- 'discover regions <name>'
  unterstuetzt nur out_path_len==0 (direkte Nachbarn).

--- DL9SAU CLI -- 'discover regions' ---
  Ohne Arg:  CTL-Discover (REPEATER-Filter), dann pro Antwort eine
             zero-hop ANON_REQ_TYPE_REGIONS. Antworten im $companion.
  Mit Name:  searchContactsByPrefix, direkt zero-hop wenn
             out_path_len==0.
  App-Mirror: Wenn die iOS/Android-App per CMD_SEND_ANON_REQ mit
             type=REGIONS arbeitet, mirrored die Firmware das Response
             in den $companion-Channel.

--- Pubkey-Anforderung in DL9SAU CLI ---
  Name-basiert (searchContactsByPrefix). Public keys werden bewusst
  NICHT in der CLI verlangt -- nicht eintippbar fuer Menschen.
