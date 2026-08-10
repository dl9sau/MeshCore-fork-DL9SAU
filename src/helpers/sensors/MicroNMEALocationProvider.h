#pragma once

#include "LocationProvider.h"
#include <MicroNMEA.h>
#include <RTClib.h>
#include <helpers/RefCountedDigitalPin.h>

#ifndef GPS_EN
    #ifdef PIN_GPS_EN
        #define GPS_EN PIN_GPS_EN
    #else
        #define GPS_EN (-1)
    #endif
#endif

#ifndef GPS_EN_ACTIVE
    #ifdef PIN_GPS_EN_ACTIVE
        #define GPS_EN_ACTIVE PIN_GPS_EN_ACTIVE
    #else
        #define GPS_EN_ACTIVE HIGH
    #endif
#endif

#ifndef GPS_RESET
    #ifdef PIN_GPS_RESET
        #define GPS_RESET PIN_GPS_RESET
    #else
        #define GPS_RESET (-1)
    #endif
#endif

#ifndef GPS_RESET_ACTIVE
    #ifdef PIN_GPS_RESET_ACTIVE
        #define GPS_RESET_ACTIVE PIN_GPS_RESET_ACTIVE
    #else
        #define GPS_RESET_ACTIVE LOW
    #endif
#endif

class MicroNMEALocationProvider : public LocationProvider {
    char _nmeaBuffer[100];
    MicroNMEA nmea;
    mesh::RTCClock* _clock;
    Stream* _gps_serial;
    RefCountedDigitalPin* _peripher_power;
    int8_t _claims = 0;
    int _pin_reset;
    int _pin_en;
    unsigned long next_check = 0;
    long time_valid = 0;
    unsigned long _last_time_sync = 0;
    static const unsigned long TIME_SYNC_INTERVAL = 1800000; // Re-sync every 30 minutes
    // DL9SAU 2026-06-18 (Wunschliste 81 Phase 2): position-only Profile.
    // Bei true: NMEA-Loop laeuft normal weiter (Position-Update, Sat-Count
    // etc.), aber das tatsaechliche _clock->setCurrentTime im Block unten
    // wird unterdrueckt. Defaults false. Setter im public-Bereich.
    bool _skip_time_sync = false;

public :
    MicroNMEALocationProvider(Stream& ser, mesh::RTCClock* clock = NULL, int pin_reset = GPS_RESET, int pin_en = GPS_EN,RefCountedDigitalPin* peripher_power=NULL) :
    nmea(_nmeaBuffer, sizeof(_nmeaBuffer)), _clock(clock), _gps_serial(&ser), _peripher_power(peripher_power), _pin_reset(pin_reset), _pin_en(pin_en) {
        if (_pin_reset != -1) {
            pinMode(_pin_reset, OUTPUT);
            digitalWrite(_pin_reset, GPS_RESET_ACTIVE);
        }
        if (_pin_en != -1) {
            pinMode(_pin_en, OUTPUT);
            digitalWrite(_pin_en, !GPS_EN_ACTIVE);
        }
    }

    void claim() {
        _claims++;
        if (_peripher_power) _peripher_power->claim();
    }

    void release() {
        if (_claims == 0) return; // avoid negative _claims
        _claims--;
        if (_peripher_power) _peripher_power->release();
    }

    void begin() override {
        // Parser-State invalidieren -- ohne dies wuerde isValid()/getTimestamp()
        // den Stand vom letzten Wake-Cycle liefern, und der periodische
        // Time-Sync in loop() koennte SOFORT mit STALEM Timestamp feuern
        // bevor der GPS-Chip frische NMEA-Daten geliefert hat.
        // Symptom: RTC springt um die Sleep-Dauer rueckwaerts.
        nmea.clear();
        time_valid = 0;
        claim();
        if (_pin_en != -1) {
            digitalWrite(_pin_en, GPS_EN_ACTIVE);
        }
        if (_pin_reset != -1) {
            digitalWrite(_pin_reset, !GPS_RESET_ACTIVE);
        }
    }

    void reset() override {
        if (_pin_reset != -1) {
            digitalWrite(_pin_reset, GPS_RESET_ACTIVE);
            delay(10);
            digitalWrite(_pin_reset, !GPS_RESET_ACTIVE);
        }
    }

    void stop() override {
        // Parser-State invalidieren. Sonst behaelt MicroNMEA isValid()=true
        // und die letzten Y/M/D/H/M/S des Pre-Sleep-RMC-Frames -- siehe
        // ausfuehrliche Erklaerung in begin().
        nmea.clear();
        time_valid = 0;
        if (_pin_en != -1) {
            digitalWrite(_pin_en, !GPS_EN_ACTIVE);
        }
        if (_pin_reset != -1) {
            digitalWrite(_pin_reset, GPS_RESET_ACTIVE);
        }
        release();
    }

    bool isEnabled() override {
        // directly read the enable pin if present as gps can be
        // activated/deactivated outside of here ...
        if (_pin_en != -1) {
            return digitalRead(_pin_en) == GPS_EN_ACTIVE;
        } else {
            return true; // no enable so must be active
        }
    }

    void syncTime() override { nmea.clear(); LocationProvider::syncTime(); }

    // DL9SAU Wunschliste 81 Phase 2: bei position-only Profile setzen wir
    // hier true -- der NMEA-Loop unten skippt dann den setCurrentTime-Call.
    void setSkipTimeSync(bool s) override { _skip_time_sync = s; }
    bool getSkipTimeSync() const override { return _skip_time_sync; }
    // DL9SAU 2026-07-14: bei !isValid() 0 statt MicroNMEAs Sentinel 999000000
    // (=999.0 nach /1e6) zurueckgeben. 999 ist ein GEFAEHRLICHER Out-of-Range-
    // Wert (bricht Distanz/Bbox/Advert-Rechnungen, wurde faelschlich via setloc
    // persistiert). 0,0 ist eindeutig "keine Position" UND benign -- die
    // bestehenden "!= 0.0"-Guards ueberall fangen es ab. Leakt trotzdem mal was,
    // macht 0,0 keinen Schaden.
    long getLatitude() override { return nmea.isValid() ? nmea.getLatitude() : 0; }
    long getLongitude() override { return nmea.isValid() ? nmea.getLongitude() : 0; }
    long getAltitude() override { 
        long alt = 0;
        nmea.getAltitude(alt);
        return alt;
    }
    long satellitesCount() override { return nmea.getNumSatellites(); }
    bool isValid() override { return nmea.isValid(); }

    long getTimestamp() override { 
        DateTime dt(nmea.getYear(), nmea.getMonth(),nmea.getDay(),nmea.getHour(),nmea.getMinute(),nmea.getSecond());
        return dt.unixtime();
    } 

    void sendSentence(const char *sentence) override {
        nmea.sendSentence(*_gps_serial, sentence);
    }

    void loop() override {

        while (_gps_serial->available()) {
            char c = _gps_serial->read();
            #ifdef GPS_NMEA_DEBUG
            Serial.print(c);
            #endif
            nmea.process(c);
        }

        if (!isValid()) time_valid = 0;

        if ((long)(millis() - next_check) > 0) {
            next_check = millis() + 1000;
            // Re-enable time sync periodically when GPS has valid fix
            if (!_time_sync_needed && _clock != NULL && (millis() - _last_time_sync) > TIME_SYNC_INTERVAL) {
                _time_sync_needed = true;
            }
            if (_time_sync_needed && time_valid > 2) {
                if (_clock != NULL) {
                    // Defensive Sanity-Schranke (Test-Bericht 2026-05-30):
                    // bei GPS-Week-Rollover-Bugs oder NMEA-Frankenframes liefert
                    // die Lib gelegentlich ein Jahr ~2038 oder noch weiter weg.
                    // Wenn die RTC schon plausibel gesetzt ist und der GPS-
                    // Timestamp mehr als 1 Jahr (in beide Richtungen) abweicht,
                    // den Sync VERWERFEN statt RTC kaputt machen. Bei frisch
                    // gebooteter RTC (cur < 1.5e9 = pre-2017) kein Vergleich,
                    // da haben wir keinen Anker.
                    long ts = getTimestamp();
                    long cur = (long)_clock->getCurrentTime();
                    const long ONE_YEAR_SECS = 365L * 86400L;
                    bool reject = (cur > 1500000000L)
                               && (ts > cur + ONE_YEAR_SECS
                                || ts < cur - ONE_YEAR_SECS);
                    if (!reject && !_skip_time_sync) {
                        _clock->setCurrentTime(ts);
                    } else if (_skip_time_sync) {
                        // Wunschliste 81 Phase 2: position-only -- Sync
                        // bewusst weggelassen. _time_sync_needed bleibt
                        // gesetzt, _last_time_sync wird trotzdem markiert
                        // damit der TIME_SYNC_INTERVAL-Re-Trigger nicht
                        // bei jedem Frame neu greift.
                        _last_time_sync = millis();
                        _time_sync_needed = false;
                    } else {
                        // Verwerfungs-Log (User-Wunsch 2026-05-30): rare
                        // Diagnose-Event, ungueltige Sync-Versuche sichtbar
                        // machen. Geht nur ueber Serial (kein pushDebugLog
                        // verfuegbar in dieser Layer), ohne millis-Prefix
                        // anders als die MyMesh-Logs.
                        // gps_ts/rtc als unsigned drucken -- getTimestamp()
                        // liefert uint32 unix-secs; bei GPS-Week-Rollover-
                        // Bug oder Frankenframe rutscht der Wert ueber 2^31
                        // und wird sonst als negative %ld dargestellt
                        // (User-Bug 2026-06-12: gps_ts=-1981000019). Delta
                        // bleibt signed weil tatsaechlich Differenz.
                        Serial.printf(
                            "[+%lums] [NMEA] sync REJECTED: gps_ts=%lu vs rtc=%lu (delta %+lds)\r\n",
                            (unsigned long)millis(),
                            (unsigned long)ts, (unsigned long)cur,
                            (long)((int32_t)ts - (int32_t)cur));
                    }
                    _time_sync_needed = false;
                    _last_time_sync = millis();
                }
            }
            if (isValid()) {
                time_valid ++;
            }
        }
    }
};
