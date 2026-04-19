#!/usr/bin/env bash
# Install dependencies for voice dictation with optional auto-paste via ydotool.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YDOTOOL_VERSION="v1.0.4"
YDOTOOL_ARCH="ubuntu-latest"

echo "==> Checking PipeWire/PulseAudio compatibility..."
if ! pactl info 2>/dev/null | grep -q "PipeWire"; then
  echo "WARN: PipeWire not detected. Expected on Ubuntu 25.10 — install pipewire-pulse manually."
fi

echo "==> Installing APT packages..."
sudo apt install -y ffmpeg curl jq wl-clipboard libnotify-bin

# -----------------------------------------------------------------------------
# Optional: ydotool for auto-paste (Ctrl+V simulation on Wayland)
# -----------------------------------------------------------------------------

install_ydotool() {
  local target_dir="$HOME/.local/bin"
  mkdir -p "$target_dir"

  echo "==> Downloading ydotool ${YDOTOOL_VERSION}..."
  curl -sSL -o "$target_dir/ydotool" \
    "https://github.com/ReimuNotMoe/ydotool/releases/download/${YDOTOOL_VERSION}/ydotool-release-${YDOTOOL_ARCH}"
  curl -sSL -o "$target_dir/ydotoold" \
    "https://github.com/ReimuNotMoe/ydotool/releases/download/${YDOTOOL_VERSION}/ydotoold-release-${YDOTOOL_ARCH}"
  chmod +x "$target_dir/ydotool" "$target_dir/ydotoold"
  echo "    installed to $target_dir/"
}

setup_uinput_udev() {
  echo "==> Configuring /dev/uinput udev rule..."
  sudo tee /etc/udev/rules.d/80-uinput.rules > /dev/null <<'EOF'
KERNEL=="uinput", GROUP="input", MODE="0660", OPTIONS+="static_node=uinput"
EOF
  sudo udevadm control --reload-rules
  sudo udevadm trigger
  if ! groups "$USER" | grep -qw input; then
    echo "==> Adding $USER to 'input' group..."
    sudo usermod -aG input "$USER"
    NEED_RELOGIN=1
  fi
  # Reload module to apply new permissions to existing /dev/uinput node
  sudo modprobe -r uinput 2>/dev/null || true
  sudo modprobe uinput
}

install_ydotoold_service() {
  echo "==> Installing ydotoold systemd service..."
  sudo tee /etc/systemd/system/ydotoold.service > /dev/null <<EOF
[Unit]
Description=ydotool Daemon (keyboard automation via /dev/uinput)
Documentation=https://github.com/ReimuNotMoe/ydotool
After=multi-user.target

[Service]
Type=simple
User=$USER
Group=$USER
ExecStart=$HOME/.local/bin/ydotoold --socket-path=/run/user/$(id -u)/.ydotool_socket --socket-perm=0600
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF
  sudo systemctl daemon-reload
  sudo systemctl enable --now ydotoold.service
  sleep 0.5
  if systemctl is-active --quiet ydotoold.service; then
    echo "    ydotoold running (pid $(systemctl show -p MainPID --value ydotoold.service))"
  else
    echo "    WARN: ydotoold failed to start. Check: systemctl status ydotoold"
  fi
}

NEED_RELOGIN=0

echo ""
read -r -p "==> Install ydotool for auto-paste (Ctrl+V simulation)? [Y/n] " YN
YN="${YN:-Y}"
if [[ "$YN" =~ ^[Yy]$ ]]; then
  install_ydotool
  setup_uinput_udev
  install_ydotoold_service
else
  echo "    Skipped. Auto-paste disabled — set KWISPR_AUTOPASTE=0 in .env"
fi

# -----------------------------------------------------------------------------
# Finalize
# -----------------------------------------------------------------------------

echo "==> Making kwispr.sh executable..."
chmod +x "$SCRIPT_DIR/kwispr.sh"

echo "==> Creating cache directory..."
mkdir -p "$HOME/.cache/kwispr"

echo "==> Generating sound cues..."
mkdir -p "$SCRIPT_DIR/sounds"
if [[ ! -f "$SCRIPT_DIR/sounds/start.wav" ]]; then
  ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i "sine=frequency=1000:duration=0.06,apad=pad_dur=0.07,volume=0.25" \
    -filter_complex "[0:a][0:a][0:a]concat=n=3:v=0:a=1[out]" \
    -map "[out]" -ar 44100 -ac 1 "$SCRIPT_DIR/sounds/start.wav"
  echo "    start.wav (1000 Hz pip×3) generated"
fi
if [[ ! -f "$SCRIPT_DIR/sounds/stop.wav" ]]; then
  ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i "sine=frequency=500:duration=0.06,apad=pad_dur=0.07,volume=0.25" \
    -filter_complex "[0:a][0:a][0:a]concat=n=3:v=0:a=1[out]" \
    -map "[out]" -ar 44100 -ac 1 "$SCRIPT_DIR/sounds/stop.wav"
  echo "    stop.wav (500 Hz pup×3) generated"
fi
if [[ ! -f "$SCRIPT_DIR/sounds/ready.wav" ]]; then
  ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i "sine=frequency=800:duration=0.06,apad=pad_dur=0.05,volume=0.25" \
    -f lavfi -i "sine=frequency=1200:duration=0.06,apad=pad_dur=0.03,volume=0.25" \
    -filter_complex "[0:a][1:a]concat=n=2:v=0:a=1[out]" \
    -map "[out]" -ar 44100 -ac 1 "$SCRIPT_DIR/sounds/ready.wav"
  echo "    ready.wav (800→1200 Hz ding-dong) generated"
fi

echo ""
echo "==> Setup complete."
echo ""
if [[ ! -f "$SCRIPT_DIR/.env" ]]; then
  echo "NEXT STEPS:"
  echo "  1. cp $SCRIPT_DIR/.env.example $SCRIPT_DIR/.env"
  echo "  2. chmod 600 $SCRIPT_DIR/.env"
  echo "  3. Edit $SCRIPT_DIR/.env and put your OpenAI API key"
  echo "  4. Bind F5 to: $SCRIPT_DIR/kwispr.sh toggle"
  echo "     System Settings → Shortcuts → Custom Shortcuts → Add New → Command"
else
  echo ".env already exists — you're ready."
  echo "Bind F5 to: $SCRIPT_DIR/kwispr.sh toggle"
fi

if [[ "$NEED_RELOGIN" == "1" ]]; then
  echo ""
  echo "⚠  You were added to the 'input' group. Re-login (or reboot) for the"
  echo "   group membership to take effect. Auto-paste won't work until then."
fi
