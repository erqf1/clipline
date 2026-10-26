#!/bin/bash
# KDE Plasma (KWin ohne Bildschirm, kglobalaccel, KDE-Portal): Clip-Taste über das Portal "GlobalShortcuts".
# Erst startet die alte Version zweimal (so wie bei Nutzern, die sie schon hatten), dann die neue - zweimal.
# Die neue muss die Taste bekommen und auf den Tastendruck reagieren.
# Aufruf: kde.sh <neues-clipline> <altes-clipline> <arbeitsordner>
set -u
if [ -z "${CLIPLINE_INNER:-}" ]; then
    export CLIPLINE_INNER=1
    exec dbus-run-session -- bash "$0" "$@"
fi
NEW="$(realpath "$1")"
OLD="$(realpath "$2")"
OUT="$3"
DIR="$(cd "$(dirname "$0")" && pwd)"
source "$DIR/common.sh"
LOG="$HOME/.local/share/WSoftware/Clipline/clipline.log"

export XDG_RUNTIME_DIR="$OUT/xdg"
mkdir -p -m 700 "$XDG_RUNTIME_DIR"
export XDG_CURRENT_DESKTOP=KDE XDG_SESSION_DESKTOP=KDE XDG_SESSION_TYPE=wayland KDE_FULL_SESSION=true KDE_SESSION_VERSION=6

kwin_wayland --virtual --no-lockscreen --socket wayland-kde --width 1280 --height 720 > "$OUT/kwin.log" 2>&1 &
for _ in $(seq 1 50); do [ -S "$XDG_RUNTIME_DIR/wayland-kde" ] && break; sleep 0.2; done
export WAYLAND_DISPLAY=wayland-kde
dbus-update-activation-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE XDG_RUNTIME_DIR KDE_FULL_SESSION
accel() { dbus-send --session --print-reply --dest=org.kde.kglobalaccel "$@"; }
sleep 2
if ! accel /kglobalaccel org.kde.KGlobalAccel.allComponents > /dev/null 2>&1; then
    echo "kglobalaccel not in KWin, starting kglobalacceld"
    /usr/lib/kglobalacceld > "$OUT/kglobalacceld.log" 2>&1 &
    sleep 2
fi
/usr/lib/xdg-desktop-portal-kde > "$OUT/xdpk.log" 2>&1 &
sleep 2
/usr/lib/xdg-desktop-portal -r -v > "$OUT/xdp.log" 2>&1 &
sleep 2
dbus-monitor --session "type='signal',interface='org.freedesktop.portal.Request'" "type='signal',interface='org.freedesktop.portal.GlobalShortcuts'" > "$OUT/portal-signals.txt" 2>&1 &

components() { accel /kglobalaccel org.kde.KGlobalAccel.allComponents 2>/dev/null | grep -o '/component/[A-Za-z0-9_]*'; }
# Taste "drücken": kglobalaccel löst die Clipline-Aktion der Gruppe aus (wie ein echter Tastendruck)
press() { accel "$1" org.kde.kglobalaccel.Component.invokeShortcut string:clipline-ctrl-alt-s string:default > /dev/null 2>&1; }
keys_of() { accel /kglobalaccel org.kde.KGlobalAccel.shortcutKeys "array:string:$1,clipline-ctrl-alt-s" 2>/dev/null | grep -c int32; }

start() { QT_QPA_PLATFORM=offscreen CLIPLINE_TEST_NODIALOG=1 "$1" --background > "$OUT/stdout-$2.txt" 2>&1 & PID=$!; sleep 10; }
stop() { kill "$PID"; wait "$PID" 2>/dev/null; sleep 1; }

echo "== old version, two starts"
for run in 1 2; do
    start "$OLD" "old$run"
    echo "groups after old start $run: $(components | tr '\n' ' ')"
    stop
done
if components | grep -qi clipline; then echo "INFO old version: KDE has a Clipline shortcut group"
else echo "INFO old version: KDE has NO Clipline shortcut group - the key could never work"; fi

for run in 1 2; do
    echo "== new version, start $run"
    start "$NEW" "new$run"
    echo "groups: $(components | tr '\n' ' ')"
    # Die Tastengruppe legt KDE an, sobald sein Dialog vom ersten Start zu ist
    if [ "$run" = 1 ]; then :
    elif components | grep -q win_mert_Clipline; then echo "PASS new start $run: KDE knows Clipline (win.mert.Clipline)"
    else echo "FAIL new start $run: KDE has no Clipline shortcut group"; FAILS=$((FAILS + 1)); fi
    # Beim ersten Mal fragt KDE per Dialog nach (hier klickt niemand) - ohne Taste muss Clipline Bescheid sagen
    if grep -q "portal: bound clipline-ctrl-alt-s -> Ctrl+Alt+S" "$LOG" 2>/dev/null; then echo "PASS new start $run: key assigned"
    elif [ "$run" = 1 ]; then echo "INFO new start 1: KDE is asking in its dialog (nobody clicks here)"
    elif grep -q "notice: .*did not set up the hotkey" "$LOG" 2>/dev/null; then echo "PASS new start $run: no key yet -> Clipline tells the user and opens the KDE dialog"
    else echo "FAIL new start $run: no key and no hint"; FAILS=$((FAILS + 1)); fi
    if [ "$run" = 2 ]; then
        before=$(grep -c "portal: clipline-ctrl-alt-s pressed, 1 action" "$LOG" 2>/dev/null)
        press /component/win_mert_Clipline
        sleep 2
        after=$(grep -c "portal: clipline-ctrl-alt-s pressed, 1 action" "$LOG" 2>/dev/null)
        if [ "$after" -gt "$before" ]; then echo "PASS new start $run: key press reaches Clipline"; else echo "FAIL new start $run: key press not received"; FAILS=$((FAILS + 1)); fi
    fi
    stop
done

# Clip-Taste in Clipline ändern (Strg+Alt+S -> Strg+Alt+D): KDE muss die neue Taste kennen, die alte darf
# nicht in Cliplines Gruppe bleiben (sonst bleibt Strg+Alt+S belegt), und der neue Druck muss ankommen.
sed -i 's/^hotkey=.*/hotkey=Ctrl+Alt+D/' "$HOME/.config/WSoftware/Clipline.conf"
names() { accel /component/win_mert_Clipline org.kde.kglobalaccel.Component.shortcutNames string:default 2>/dev/null | grep -o 'clipline-[a-z0-9-]*' | tr '\n' ' '; }
for run in 1 2; do
    echo "== new version with Ctrl+Alt+D, start $run"
    start "$NEW" "changed$run"
    echo "Clipline keys at KDE: $(names)"
    if [ "$run" = 2 ]; then
        if names | grep -q clipline-ctrl-alt-d; then echo "PASS changed key: KDE knows the new key"; else echo "FAIL changed key: new key missing at KDE"; FAILS=$((FAILS + 1)); fi
        if names | grep -q clipline-ctrl-alt-s; then echo "FAIL changed key: old key still registered at KDE"; FAILS=$((FAILS + 1)); else echo "PASS changed key: old key removed from KDE"; fi
        before=$(grep -c "portal: clipline-ctrl-alt-d pressed, 1 action" "$LOG" 2>/dev/null)
        accel /component/win_mert_Clipline org.kde.kglobalaccel.Component.invokeShortcut string:clipline-ctrl-alt-d string:default > /dev/null 2>&1
        sleep 2
        after=$(grep -c "portal: clipline-ctrl-alt-d pressed, 1 action" "$LOG" 2>/dev/null)
        if [ "$after" -gt "$before" ]; then echo "PASS changed key: press reaches Clipline"; else echo "FAIL changed key: press not received"; FAILS=$((FAILS + 1)); fi
    fi
    stop
done

echo "----- portal signals (responses)"
grep -A12 -E 'member=(Response|Activated)' "$OUT/portal-signals.txt" | grep -E 'member=|string "(clipline|trigger|session)|string "[A-Z][a-z]+\+' | head -60
echo "----- xdg-desktop-portal-kde"
tail -n 20 "$OUT/xdpk.log"
echo "----- kwin"
tail -n 10 "$OUT/kwin.log"
finish
exit $FAILS
