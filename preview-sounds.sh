#!/usr/bin/env bash
# Interactive sound preview for kwispr start/stop cues.
# Plays paired sounds from freedesktop + ocean themes so you can pick the
# combo that feels right. Writes KWISPR_SOUND_START / KWISPR_SOUND_STOP
# into .env at the end.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$SCRIPT_DIR/.env"

command -v paplay >/dev/null || { echo "paplay not found. Install pipewire-pulse / pulseaudio-utils."; exit 1; }

# Curated list of sounds that make sense for start/stop cues (short, musical,
# not anxiety-inducing). Paired — index N start, index N stop.
# format: "label|start_path|stop_path"
PAIRS=(
  "message in/out (ocean)|/usr/share/sounds/ocean/stereo/message-contact-in.oga|/usr/share/sounds/ocean/stereo/message-contact-out.oga"
  "button press pair (ocean)|/usr/share/sounds/ocean/stereo/button-pressed.oga|/usr/share/sounds/ocean/stereo/button-pressed-modifier.oga"
  "service login/logout (freedesktop)|/usr/share/sounds/freedesktop/stereo/service-login.oga|/usr/share/sounds/freedesktop/stereo/service-logout.oga"
  "desktop login/logout (ocean)|/usr/share/sounds/ocean/stereo/desktop-login.oga|/usr/share/sounds/ocean/stereo/desktop-logout.oga"
  "message new/sent (ocean)|/usr/share/sounds/ocean/stereo/message-new-instant.oga|/usr/share/sounds/ocean/stereo/message-sent-instant.oga"
  "device added/removed (freedesktop)|/usr/share/sounds/freedesktop/stereo/device-added.oga|/usr/share/sounds/freedesktop/stereo/device-removed.oga"
  "bell + complete (freedesktop, current default)|/usr/share/sounds/freedesktop/stereo/bell.oga|/usr/share/sounds/freedesktop/stereo/complete.oga"
  "dialog-info + complete (mixed)|/usr/share/sounds/freedesktop/stereo/dialog-information.oga|/usr/share/sounds/freedesktop/stereo/complete.oga"
  "completion-rotation + success (ocean)|/usr/share/sounds/ocean/stereo/completion-rotation.oga|/usr/share/sounds/ocean/stereo/completion-success.oga"
)

play_pair() {
  local start="$1" stop="$2"
  echo "  ▶ START"; paplay "$start" 2>/dev/null || echo "    (file missing: $start)"
  sleep 0.6
  echo "  ▶ STOP";  paplay "$stop"  2>/dev/null || echo "    (file missing: $stop)"
}

echo ""
echo "=== Dictate sound picker ==="
echo "For each pair: you'll hear START then STOP. Answer [y]es to pick, [n]o to skip, [r]epeat, [q]uit."
echo ""

CHOICE_START=""
CHOICE_STOP=""

for i in "${!PAIRS[@]}"; do
  IFS='|' read -r label start stop <<< "${PAIRS[$i]}"
  [[ -f "$start" && -f "$stop" ]] || { echo "[$((i+1))] $label — SKIPPED (file missing)"; continue; }

  while true; do
    echo ""
    echo "[$((i+1))/${#PAIRS[@]}] $label"
    play_pair "$start" "$stop"
    read -r -p "  pick this? [y/n/r/q] " ans
    case "$ans" in
      y|Y) CHOICE_START="$start"; CHOICE_STOP="$stop"; break 2 ;;
      r|R) continue ;;
      q|Q) echo ""; echo "Aborted — .env not changed."; exit 0 ;;
      *)   break ;;
    esac
  done
done

echo ""
if [[ -z "$CHOICE_START" ]]; then
  echo "No pair picked — .env not changed."
  exit 0
fi

echo "Picked:"
echo "  START: $CHOICE_START"
echo "  STOP:  $CHOICE_STOP"
echo ""

# Write/update .env
if [[ -f "$ENV_FILE" ]]; then
  # Remove existing KWISPR_SOUND_START / STOP lines (comments too for cleanness)
  tmp="$(mktemp)"
  grep -vE '^(# ?)?KWISPR_SOUND_(START|STOP)=' "$ENV_FILE" > "$tmp"
  {
    cat "$tmp"
    echo ""
    echo "# Custom sound cues (picked via preview-sounds.sh on $(date +%Y-%m-%d))"
    echo "KWISPR_SOUND_START=$CHOICE_START"
    echo "KWISPR_SOUND_STOP=$CHOICE_STOP"
  } > "$ENV_FILE"
  rm -f "$tmp"
  chmod 600 "$ENV_FILE"
  echo "Saved to $ENV_FILE"
else
  echo "No .env found — copy .env.example to .env first, then rerun this script."
fi
