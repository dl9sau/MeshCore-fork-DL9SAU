================================================================================
MeshCore + Amateurfunk: Eignungs-Analyse
================================================================================
Erstellt:        2026-05-30
Autor:           Thomas (DL9SAU)
Anlass:          Wiederkehrende Frage ob MeshCore auf Amateurfunkbaendern
                 (insbesondere 70cm) betrieben werden darf.

================================================================================
Kurzantwort
================================================================================

MeshCore ist in seiner aktuellen Architektur NICHT amateurfunkkonform
betreibbar. Grund: das Protokoll verschluesselt Payloads end-to-end, was
in Amateurfunkdiensten weltweit untersagt ist (ITU Radio Regulations
Art. 25.2A "no codes or ciphers", DE: BNetzA Vfg 33/2014 / AFuV "offene
Sprache"). Ein Plain-Mode-Flag genuegt nicht -- weite Teile des
Routings, der Channel-Adressierung und der Path-Discovery setzen
verschluesselte Pakete voraus. Ein AFU-tauglicher Fork waere ein
ANDERES Protokoll, nicht "MC mit Schalter".

================================================================================
Im Detail -- wo MeshCore verschluesselt
================================================================================

1) Channel-Messages
   AES mit Group-PSK. Payload (inkl. "Name: text" Sender-Identitaet im
   Klartext-Format) wird verschluesselt im Funk. Der channel.hash
   (sha256(PSK)) steckt als Plain-Hint im Header -- Empfaenger probiert
   seine bekannten Channels durch (Trial-Decryption).

2) Direct Messages (DM)
   ECDH-Schluesselaustausch zwischen Absender-Pubkey und Empfaenger-
   Pubkey -> Shared Secret -> AES. Payload nicht lesbar fuer Aussen-
   stehende. Sender-Adress-Hash (1 Byte) ist im Header sichtbar.

3) Path-Discovery / Path-Return
   Routing-Antworten werden piggyback auf verschluesselte Direct-
   Pakete gesendet. Auch hier: Payload AES.

4) Adverts -- Discovery-Pakete
   Pub-Key + Sender-Name liegen im PLAIN (notwendig fuer den Discovery-
   Prozess). Diese WAEREN amateurfunkkonform.

5) Signaturen
   Authentizitaets-Stempel ueber Paketinhalte. Stellt KEINE Confiden-
   tiality her -- nur Echtheit. Signaturen waeren amateurfunkkonform.

================================================================================
Warum "encryption off"-Flag nicht reicht
================================================================================

Wenn man die Payload-Verschluesselung deaktivieren wuerde, fielen die
folgenden Teil-Funktionen aus:

 - Channel-Routing: der channel.hash dient als Routing-Hint, das
   eigentliche Channel-Matching laeuft ueber Trial-Decryption mit
   gespeicherten PSKs. Plain-Payload -> jede Node muss den Channel
   anhand des Klartextes erkennen (Channel-Name im Header noetig).
 - DM-Adressierung: aktuell ueber gemeinsamen ECDH-Schluessel. Plain-
   Mode braucht eigene Header-Felder fuer Empfaenger-Identifikation.
 - Path-Discovery: muss komplett neu konstruiert werden, Routing-
   Antworten ohne Verschluesselung sind moeglich aber das aktuelle
   Wire-Format passt nicht.

Effektive Konsequenz: das Protokoll muesste neu spezifiziert werden.
Eine Variante mit reinen Signaturen + Plain-Payload existiert konzep-
tionell (siehe Reticulum's "lightweight authenticated" Mode), wuerde
aber mit der aktuellen Wire-Spec inkompatibel sein.

================================================================================
Code-Indizien im DL9SAU-Branch
================================================================================

examples/companion_radio/MyMesh.cpp ~Zeile 2389:

  // Amateur radio 70cm (430.000 - 439.999 MHz). CAVE: MeshCore encrypts
  // payloads end-to-end, which is generally not permitted on amateur
  // radio frequencies (open-mode requirement). Only uncomment if you
  // are sure your local regulation allows it for your usage:
  //, { 430000, 439999 }

Der 70cm-Amateurfunkbereich ist im strict-Repeat-Frequenz-Set bewusst
auskommentiert.

================================================================================
Vergleich mit anderen Mesh-Projekten
================================================================================

Meshtastic:
   Hat einen "licensed_operator" Modus (auch "is_licensed" genannt) in
   dem die Channel-Verschluesselung deaktiviert wird. Vorgesehen fuer
   geprueft-lizenzierte Funkamateure. Nicht alle Features in dem Modus
   nutzbar (Channels mit private PSKs sind dann inkompatibel).
   Limit: nur ein bestimmter Channel kann unencrypted laufen, kein
   vollstaendiger AFU-Mode.

Meshcom:
   Eigenstaendiges Projekt (Oesterreich, OE1KFR + Team), fork-aehnlich
   abgespalten von Meshtastic, mit Fokus auf Amateurfunk-Konformitaet
   und APRS-Integration. Inkompatibel zu Meshtastic und MeshCore am
   Wire-Layer.

LoRa-APRS:
   APRS (klassisches Ham-Protokoll) ueber LoRa-Modulation. Plain-Text
   per Design, kein Meshing wie MeshCore. Verbreitet im Ham-LoRa-
   Bereich.

VARA, PACTOR, Winlink:
   HF-Modems mit Plain-Payload (Winlink hat Gateway-seitig optionale
   Kompression, aber kein Verschluesseln).

================================================================================
Quellen / Hinweise
================================================================================

- ITU Radio Regulations Art. 25.2A (Amateurfunk-Verkehrsformen).
- DE: BNetzA Vfg 33/2014 (Amateurfunkverordnung, AFuV § 16 / § 5):
  "Verkehr in offener Sprache, keine geheime Bedeutung der Zeichen".
- Meshtastic 'is_licensed' Diskussion:
  github.com/meshtastic/firmware -- Stichwort "licensed_operator_mode".
- Meshcom Projekt-Seite (DE/AT-Ham-Community).
- Reticulum "signed-but-not-encrypted" Modus (analog Konzept):
  github.com/markqvist/Reticulum.

================================================================================
Stand: 2026-05-30. Bei aenderungen am MeshCore-upstream-Protokoll, das
fundamentale Plain-Mode-Routing einfuehrt, ist diese Bewertung neu zu
pruefen.
================================================================================
