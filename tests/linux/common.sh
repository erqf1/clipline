# Gemeinsame Teile der Linux-Tests (wird per "source" geladen). Erwartet BIN und OUT.
mkdir -p "$OUT/clips" "$HOME/.config/WSoftware"
rm -f "$OUT"/clips/*.mp4 "$HOME/.local/share/WSoftware/Clipline/clipline.log"
cat > "$HOME/.config/WSoftware/Clipline.conf" <<EOF
[General]
firstRunDone=true
language=en
clipsDir=$OUT/clips
clipSeconds=10
height=720
fps=30
quality=0
systemAudio=false
autostart=false
sound=false
hotkey=Ctrl+Alt+S
muteHotkey=
EOF

FAILS=0
# Clip-Taste F9, zweiter Clip-Knopf Shift+F10 (6 s), dritter Strg+Feststelltaste (6 s)
new_keys_config() {
    sed -i 's/^hotkey=.*/hotkey=F9/' "$HOME/.config/WSoftware/Clipline.conf"
    cat >> "$HOME/.config/WSoftware/Clipline.conf" <<'EOF'

[buttons]
1\hotkey=Shift+F10
1\seconds=6
1\height=480
1\fps=30
2\hotkey=Ctrl+CapsLock
2\seconds=6
2\height=480
2\fps=30
size=2
EOF
}
count() { ls "$OUT"/clips/*.mp4 2>/dev/null | wc -l; }
# wartet, bis mindestens $1 Clips da sind (max. 30 s)
wait_clips() { for _ in $(seq 1 60); do [ "$(count)" -ge "$1" ] && return 0; sleep 0.5; done; return 1; }
# Länge des neuesten Clips in Sekunden (ganzzahlig)
last_len() { ffprobe -v error -show_entries format=duration -of csv=p=0 "$(ls -t "$OUT"/clips/*.mp4 | head -1)" 2>/dev/null | cut -d. -f1; }
# check <name> <erwartete Anzahl> [<Mindestlänge>]
check() {
    if wait_clips "$2"; then
        # Der Clip wird evtl. noch geschrieben (kleinere Qualität wird beim Speichern neu kodiert): warten, bis er lesbar ist
        local len=""
        for _ in $(seq 1 20); do len=$(last_len); [ -n "$len" ] && break; sleep 0.5; done
        if [ -n "${3:-}" ] && [ "${len:-0}" -lt "$3" ]; then
            echo "FAIL $1 (clip only ${len:-0} s)"; FAILS=$((FAILS + 1))
        else
            echo "PASS $1 (${len:-?} s)"
        fi
    else
        echo "FAIL $1 (no clip)"; FAILS=$((FAILS + 1))
        # weiterzählen, als wäre er da: spätere Prüfungen sollen unabhängig bleiben
        touch "$OUT/clips/missing-$1.mp4"
    fi
}
finish() {
    echo "----- clipline.log"
    cat "$HOME/.local/share/WSoftware/Clipline/clipline.log" 2>/dev/null || echo "(no log - old version)"
    echo "----- clipline stdout/stderr"
    tail -n 40 "$OUT/stdout.txt"
    echo "----- clips"
    ls -la "$OUT/clips"
    echo "RESULT: $FAILS failed"
}
