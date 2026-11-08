#!/usr/bin/env bash
set -euo pipefail

mkdir -p test-results
APK="$(find test-apk -type f -name '*.apk' -print -quit)"
if [[ -z "$APK" ]]; then
  echo "FAIL: no APK was downloaded." | tee test-results/test-summary.txt
  exit 1
fi

{
  echo "APK: $APK"
  echo "Android release: $(adb shell getprop ro.build.version.release | tr -d '\r')"
  echo "Android API: $(adb shell getprop ro.build.version.sdk | tr -d '\r')"
  echo "Emulator ABI: $(adb shell getprop ro.product.cpu.abilist | tr -d '\r')"
} | tee test-results/test-summary.txt

adb logcat -c
adb install -r "$APK" 2>&1 | tee test-results/install.txt
adb shell am force-stop dev.mc.legacy
adb shell am start -W -n dev.mc.legacy/.MainActivity 2>&1 | tee test-results/activity-start.txt

# Resource extraction includes the original media and sound banks. Wait for
# the native Legacy title screen, not merely for the Android Activity to stay alive.
PID=""
for attempt in $(seq 1 60); do
  PID="$(adb shell pidof -s dev.mc.legacy 2>/dev/null | tr -d '\r' || true)"
  adb logcat -d -v threadtime > test-results/logcat.txt
  if grep -q "Legacy client initialized" test-results/logcat.txt; then
    break
  fi
  if [[ -z "$PID" ]]; then
    break
  fi
  sleep 1
done

adb shell dumpsys activity exit-info dev.mc.legacy > test-results/exit-info.txt 2>&1 || true
adb shell screencap -p /sdcard/mclg-startup.png >/dev/null 2>&1 || true
adb pull /sdcard/mclg-startup.png test-results/startup.png >/dev/null 2>&1 || true
adb shell dumpsys activity exit-info dev.mc.legacy > test-results/exit-info.txt 2>&1 || true
adb shell screencap -p /sdcard/mclg-startup.png >/dev/null 2>&1 || true
adb pull /sdcard/mclg-startup.png test-results/startup.png >/dev/null 2>&1 || true

if [[ -z "$PID" ]]; then
  echo "FAIL: app process exited within 12 seconds." | tee -a test-results/test-summary.txt
  grep -Ei 'FATAL EXCEPTION|Fatal signal|am_crash|dev\.mc\.legacy|MCLG-Android|AndroidRuntime|linker' test-results/logcat.txt | tail -n 200 || true
  cat test-results/exit-info.txt || true
  exit 1
fi

if grep -E 'FATAL EXCEPTION|Fatal signal [0-9]+|am_crash|Process: dev\.mc\.legacy' test-results/logcat.txt; then
  echo "FAIL: crash markers found in logcat." | tee -a test-results/test-summary.txt
  exit 1
fi

if ! grep -q 'Native GLES3 renderer initialized' test-results/logcat.txt; then
  echo "FAIL: native GLES3 initialization was not confirmed." | tee -a test-results/test-summary.txt
  grep -Ei 'MCLG-Android|EGL|OpenGL|linker|UnsatisfiedLinkError' test-results/logcat.txt | tail -n 200 || true
  exit 1
fi

if grep -q 'Minecraft client startup failed' test-results/logcat.txt ||
   ! grep -q 'Legacy client initialized.*title screen ready' test-results/logcat.txt; then
  echo "FAIL: GLES is alive, but the original Minecraft Legacy title screen did not initialize." | tee -a test-results/test-summary.txt
  grep -Ei 'MCLG-Android|Legacy client|Minecraft bootstrap|EGL|OpenGL' test-results/logcat.txt | tail -n 250 || true
  exit 1
fi

echo "PASS: Activity, GLES3 renderer, and original Legacy title screen initialized. PID=$PID" | tee -a test-results/test-summary.txt
