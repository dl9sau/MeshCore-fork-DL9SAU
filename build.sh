#!/usr/bin/env bash

global_usage() {
  cat - <<EOF
Usage:
sh build.sh <command> [target]

Commands:
  help|usage|-h|--help: Shows this message.
  list|-l: List firmwares available to build.
  build-firmware <target>: Build the firmware for the given build target.
  build-firmwares: Build all firmwares for all targets.
  build-matching-firmwares <build-match-spec>: Build all firmwares for build targets containing the string given for <build-match-spec>.
  build-companion-firmwares: Build all companion firmwares for all build targets.
  build-repeater-firmwares: Build all repeater firmwares for all build targets.
  build-room-server-firmwares: Build all chat room server firmwares for all build targets.

Examples:
Build firmware for the "RAK_4631_repeater" device target
$ sh build.sh build-firmware RAK_4631_repeater

Build all firmwares for device targets containing the string "RAK_4631"
$ sh build.sh build-matching-firmwares <build-match-spec>

Build all companion firmwares
$ sh build.sh build-companion-firmwares

Build all repeater firmwares
$ sh build.sh build-repeater-firmwares

Build all chat room server firmwares
$ sh build.sh build-room-server-firmwares

Environment Variables:
  DISABLE_DEBUG=1: Disables all debug logging flags (MESH_DEBUG, MESH_PACKET_LOGGING, etc.)
                   If not set, debug flags from variant platformio.ini files are used.

Examples:
Build without debug logging:
$ export FIRMWARE_VERSION=v1.0.0
$ export DISABLE_DEBUG=1
$ sh build.sh build-firmware RAK_4631_repeater

Build with debug logging (default, uses flags from variant files):
$ export FIRMWARE_VERSION=v1.0.0
$ sh build.sh build-firmware RAK_4631_repeater
EOF
}

# get a list of pio env names that start with "env:"
get_pio_envs() {
  pio project config | grep 'env:' | sed 's/env://'
}

# Catch cries for help before doing anything else.
case $1 in
  help|usage|-h|--help)
    global_usage
    exit 1
    ;;
  list|-l)
    get_pio_envs
    exit 0
    ;;
esac

# cache project config json for use in get_platform_for_env()
PIO_CONFIG_JSON=$(pio project config --json-output)

# $1 should be the string to find (case insensitive)
get_pio_envs_containing_string() {
  shopt -s nocasematch
  envs=($(get_pio_envs))
  for env in "${envs[@]}"; do
      if [[ "$env" == *${1}* ]]; then
        echo $env
      fi
  done
}

# $1 should be the string to find (case insensitive)
get_pio_envs_ending_with_string() {
  shopt -s nocasematch
  envs=($(get_pio_envs))
  for env in "${envs[@]}"; do
    if [[ "$env" == *${1} ]]; then
      echo $env
    fi
  done
}

# get platform flag for a given environment
# $1 should be the environment name
get_platform_for_env() {
  local env_name=$1
  echo "$PIO_CONFIG_JSON" | python3 -c "
import sys, json, re
data = json.load(sys.stdin)
for section, options in data:
    if section == 'env:$env_name':
        for key, value in options:
            if key == 'build_flags':
                for flag in value:
                    match = re.search(r'(ESP32_PLATFORM|NRF52_PLATFORM|STM32_PLATFORM|RP2040_PLATFORM)', flag)
                    if match:
                        print(match.group(1))
                        sys.exit(0)
"
}

# disable all debug logging flags if DISABLE_DEBUG=1 is set
disable_debug_flags() {
  if [ "$DISABLE_DEBUG" == "1" ]; then
    export PLATFORMIO_BUILD_FLAGS="${PLATFORMIO_BUILD_FLAGS} -UMESH_DEBUG -UBLE_DEBUG_LOGGING -UWIFI_DEBUG_LOGGING -UBRIDGE_DEBUG -UGPS_NMEA_DEBUG -UCORE_DEBUG_LEVEL -UESPNOW_DEBUG_LOGGING -UDEBUG_RP2040_WIRE -UDEBUG_RP2040_SPI -UDEBUG_RP2040_CORE -UDEBUG_RP2040_PORT -URADIOLIB_DEBUG_SPI -UCFG_DEBUG -URADIOLIB_DEBUG_BASIC -URADIOLIB_DEBUG_PROTOCOL"
  fi
}

# build firmware for the provided pio env in $1
build_firmware() {
  # get env platform for post build actions
  ENV_PLATFORM=($(get_platform_for_env $1))

  # get git commit sha (8 chars, wie dl9sau_version.py)
  COMMIT_HASH=$(git rev-parse --short=8 HEAD 2>/dev/null || echo nogit)

  # set firmware build date
  FIRMWARE_BUILD_DATE=$(date '+%d-%b-%Y')

  # DL9SAU 2026-07-22: Versions-/Dateiname aus der Firmware-Quelle ableiten
  # (single source of truth: MyMesh.h "#define FIRMWARE_VERSION "v1.16.0-DL9SAU"").
  # So zeigen Dateiname UND 'ver' konsistent Upstream-Version + Variante + Hash --
  # unabhaengig vom (evtl. abweichenden) git-Tag. Dateiname bekommt den vollen
  # 8-char-Hash (praeziser als der 3-char-Hash im 20-byte-Wire-Feld, den
  # dl9sau_version.py fuer das eingebettete 'ver' setzt).
  DL9SAU_BASE=$(grep -oE '#define FIRMWARE_VERSION "[^"]+"' examples/companion_radio/MyMesh.h 2>/dev/null | sed -E 's/.*"([^"]+)".*/\1/')
  if [ -n "$DL9SAU_BASE" ]; then
    # z.B: v1.16.0-DL9SAU-g4dbb2f18
    FIRMWARE_VERSION_STRING="${DL9SAU_BASE}-g${COMMIT_HASH}"
  else
    # Fallback (Upstream-Schema): Tag-Version, z.B. v1.0.0-abcdef
    if [ -z "$FIRMWARE_VERSION" ]; then
      echo "FIRMWARE_VERSION must be set in environment"
      exit 1
    fi
    FIRMWARE_VERSION_STRING="${FIRMWARE_VERSION}-${COMMIT_HASH}"
  fi

  # craft filename
  # e.g: t1000e_companion_radio_ble-v1.16.0-DL9SAU-g4dbb2f18
  FIRMWARE_FILENAME="$1-${FIRMWARE_VERSION_STRING}"

  # add firmware version info to end of existing platformio build flags in environment vars
  # DL9SAU 2026-07-22: die EINGEBETTETE Version ('ver' + App-Anzeige) setzt
  # dl9sau_version.py (pre-Script in arduino_base) -- 20-byte-Wire-safe
  # "v1.16.0-DL9SAU.g<hash3>" + 12-byte-Datum. Das ist BEWUSST so kurz gewaehlt,
  # damit die Smartphone-App Version + Datum NICHT abschneidet (1 Byte mehr ->
  # App kappt). Wir injizieren hier deshalb NICHTS mehr (weder VERSION noch
  # DATE), sonst wuerde der lange Datei-String das 20-byte-Feld sprengen und
  # local/CI divergieren. dl9sau_version.py ist alleiniger Herr ueber Version/
  # Datum/Zeit -> lokal == CI. Nur der Upstream-Fallback injiziert wie frueher.
  if [ -z "$DL9SAU_BASE" ]; then
    export PLATFORMIO_BUILD_FLAGS="${PLATFORMIO_BUILD_FLAGS} -DFIRMWARE_BUILD_DATE='\"${FIRMWARE_BUILD_DATE}\"' -DFIRMWARE_VERSION='\"${FIRMWARE_VERSION_STRING}\"'"
  fi

  # disable debug flags if requested
  disable_debug_flags

  # build firmware target
  pio run -e $1

  # build merge-bin for esp32 fresh install, copy .bins to out folder (e.g: Heltec_v3_room_server-v1.0.0-SHA.bin)
  if [ "$ENV_PLATFORM" == "ESP32_PLATFORM" ]; then
    pio run -t mergebin -e $1
    cp .pio/build/$1/firmware.bin out/${FIRMWARE_FILENAME}.bin 2>/dev/null || true
    cp .pio/build/$1/firmware-merged.bin out/${FIRMWARE_FILENAME}-merged.bin 2>/dev/null || true
  fi

  # build .uf2 for nrf52 boards, copy .uf2 and .zip to out folder (e.g: RAK_4631_Repeater-v1.0.0-SHA.uf2)
  if [ "$ENV_PLATFORM" == "NRF52_PLATFORM" ]; then
    python3 bin/uf2conv/uf2conv.py .pio/build/$1/firmware.hex -c -o .pio/build/$1/firmware.uf2 -f 0xADA52840
    cp .pio/build/$1/firmware.uf2 out/${FIRMWARE_FILENAME}.uf2 2>/dev/null || true
    cp .pio/build/$1/firmware.zip out/${FIRMWARE_FILENAME}.zip 2>/dev/null || true
  fi

  # for stm32, copy .bin and .hex to out folder
  if [ "$ENV_PLATFORM" == "STM32_PLATFORM" ]; then
    cp .pio/build/$1/firmware.bin out/${FIRMWARE_FILENAME}.bin 2>/dev/null || true
    cp .pio/build/$1/firmware.hex out/${FIRMWARE_FILENAME}.hex 2>/dev/null || true
  fi

  # for rp2040, copy .bin and .uf2 to out folder
  if [ "$ENV_PLATFORM" == "RP2040_PLATFORM" ]; then
    cp .pio/build/$1/firmware.bin out/${FIRMWARE_FILENAME}.bin 2>/dev/null || true
    cp .pio/build/$1/firmware.uf2 out/${FIRMWARE_FILENAME}.uf2 2>/dev/null || true
  fi

}

# firmwares containing $1 will be built
build_all_firmwares_matching() {
  envs=($(get_pio_envs_containing_string "$1"))
  for env in "${envs[@]}"; do
      build_firmware $env
  done
}

# firmwares ending with $1 will be built
build_all_firmwares_by_suffix() {
  envs=($(get_pio_envs_ending_with_string "$1"))
  for env in "${envs[@]}"; do
    build_firmware $env
  done
}

build_repeater_firmwares() {

#  # build specific repeater firmwares
#  build_firmware "Heltec_v2_repeater"
#  build_firmware "Heltec_v3_repeater"
#  build_firmware "Xiao_C3_Repeater_sx1262"
#  build_firmware "Xiao_S3_WIO_Repeater"
#  build_firmware "LilyGo_T3S3_sx1262_Repeater"
#  build_firmware "RAK_4631_Repeater"

  # build all repeater firmwares
  build_all_firmwares_by_suffix "_repeater"

}

build_companion_firmwares() {

#  # build specific companion firmwares
#  build_firmware "Heltec_v2_companion_radio_usb"
#  build_firmware "Heltec_v2_companion_radio_ble"
#  build_firmware "Heltec_v3_companion_radio_usb"
#  build_firmware "Heltec_v3_companion_radio_ble"
#  build_firmware "Xiao_S3_WIO_companion_radio_ble"
#  build_firmware "LilyGo_T3S3_sx1262_companion_radio_usb"
#  build_firmware "LilyGo_T3S3_sx1262_companion_radio_ble"
#  build_firmware "RAK_4631_companion_radio_usb"
#  build_firmware "RAK_4631_companion_radio_ble"
#  build_firmware "t1000e_companion_radio_ble"

  # build all companion firmwares
  build_all_firmwares_by_suffix "_companion_radio_usb"
  build_all_firmwares_by_suffix "_companion_radio_ble"

}

build_room_server_firmwares() {

#  # build specific room server firmwares
#  build_firmware "Heltec_v3_room_server"
#  build_firmware "RAK_4631_room_server"

  # build all room server firmwares
  build_all_firmwares_by_suffix "_room_server"

}

build_firmwares() {
  build_companion_firmwares
  build_repeater_firmwares
  build_room_server_firmwares
}

# clean build dir
rm -rf out
mkdir -p out

# handle script args
if [[ $1 == "build-firmware" ]]; then
  TARGETS=${@:2}
  if [ "$TARGETS" ]; then
    for env in $TARGETS; do
      build_firmware $env
    done
  else
    echo "usage: $0 build-firmware <target>"
    exit 1
  fi
elif [[ $1 == "build-matching-firmwares" ]]; then
  if [ "$2" ]; then
     build_all_firmwares_matching $2
  else
     echo "usage: $0 build-matching-firmwares <build-match-spec>"
    exit 1
  fi
elif [[ $1 == "build-firmwares" ]]; then
  build_firmwares
elif [[ $1 == "build-companion-firmwares" ]]; then
  build_companion_firmwares
elif [[ $1 == "build-repeater-firmwares" ]]; then
  build_repeater_firmwares
elif [[ $1 == "build-room-server-firmwares" ]]; then
  build_room_server_firmwares
fi
