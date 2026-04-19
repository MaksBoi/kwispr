# Kwispr

**Toggle voice dictation for Linux / Wayland / KDE Plasma via OpenAI Whisper.** Press your hotkey → speak → press it again → text is copied to your clipboard and auto-pasted into the focused window.

![shell](https://img.shields.io/badge/shell-bash-blue) ![license](https://img.shields.io/badge/license-MIT-lightgrey) ![platform](https://img.shields.io/badge/platform-Linux%20Wayland-orange)

![demo](demo.gif)

## Why

- **Stateless** — ~250 lines of bash, no daemons of our own, no GUI. The KDE Shortcut runs the script, the script exits. Next press runs it again.
- **Clipboard first, auto-paste second** — the transcript always goes into the clipboard via `wl-copy`. On top of that, `ydotool` simulates Ctrl+V in the focused window for zero-friction pasting. If the focus was elsewhere, the text is still one Ctrl+V away.
- **Works anywhere on Wayland** — VS Code, Claude Code CLI, browser, Slack, terminal. `ydotool` goes through `/dev/uinput` at the kernel level, bypassing Wayland's input-injection restrictions.
- **Layout-independent** — sends raw Ctrl+V keycodes; apps pull Unicode text from the clipboard. Russian, English, and mixed text all work the same.
- **Three-stage audio feedback** — pip-pip-pip on start, pup-pup-pup on stop, ding-dong when the transcript is ready. You always know what's happening without looking.
- **Persistent status bubble** — one notification transforms Listening → Processing → Ready, never stacks.
- **Never loses your speech** — on API/network failure the WAV is preserved and a retry command is copied into your clipboard.
- **Whisper hallucination filter** — a post-processing regex scrubs the subtitle artifacts Whisper leaks on short audio («Редактор субтитров», «Subtitles by», «Thanks for watching»).

**Tested on:** Kubuntu 25.10 + KDE Plasma 6 + PipeWire. Should work on any Linux with Wayland + `wl-clipboard` + `libnotify-bin`.

## Architecture

```
Hotkey (KDE Custom Shortcut)
  └─ kwispr.sh toggle
       ├─ start: ffmpeg -f pulse → ~/.cache/kwispr/TS.wav (via FIFO)
       │         notify-send "🎙 Listening" (persistent)
       └─ stop:  write 'q' to FIFO → ffmpeg flushes WAV
                 notify-send replace → "⏳ Processing"
                 curl POST https://api.openai.com/v1/audio/transcriptions
                   model=whisper-1, temperature=0
                 sed-filter subtitle hallucinations
                 wl-copy $transcript                        (always)
                 ydotool key 29:1 47:1 47:0 29:0            (Ctrl+V, if enabled)
                 notify-send replace → "✅ Pasted" or "Ready (in clipboard)"
```

**No daemons of our own.** The script is stateless, launched from the KDE Shortcut and exits after each press. State lives in lockfiles under `~/.cache/kwispr/`. The only persistent component is the optional `ydotoold` systemd service (for auto-paste).

## Dependencies

| Package | Purpose | Source |
|---|---|---|
| `ffmpeg` | record from pulse (via FIFO + 'q' for proper flush) | APT |
| `curl` | OpenAI API request | APT |
| `jq` | parse JSON response | APT |
| `wl-clipboard` | `wl-copy` for clipboard | APT |
| `libnotify-bin` | `notify-send` for the persistent status bubble | APT |
| `pipewire-pulse` | pulse-compat layer on PipeWire | ships with Kubuntu 25.10 |
| `ydotool` v1.0.4 (optional) | auto-paste Ctrl+V via `/dev/uinput` | GitHub release → `~/.local/bin/` |

**Why `ydotool` is downloaded from GitHub instead of APT:** the Ubuntu APT version is an ancient 0.1.8 without the `key` command or daemon. We need 1.0.4+.

## Install

```bash
git clone git@github.com:MaksBoi/kwispr.git
cd kwispr
./setup.sh                       # installs deps, prompts for ydotool install
cp .env.example .env
chmod 600 .env
# Put your OPENAI_API_KEY into .env
```

When `ydotool` is installed by `setup.sh`:

- v1.0.4 downloaded into `~/.local/bin/`
- udev rule `/etc/udev/rules.d/80-uinput.rules` makes `/dev/uinput` owned by the `input` group
- your user is added to the `input` group (**re-login required** after this)
- system-level systemd service `/etc/systemd/system/ydotoold.service` is created

After re-login: `systemctl status ydotoold` should show `active (running)`.

## Binding a hotkey

Pick any key or combination — a regular `F5`, a modifier combo, a multimedia key. Bind it through KDE:

1. **System Settings → Shortcuts → Shortcuts → Add New → Command/URL Shortcut**
2. **Trigger:** press the key (or combo) you want to use
3. **Action:** `<absolute_path>/kwispr.sh`
4. **Apply**

### Multimedia / Fn-row keys

Some keyboards have a "multimedia mode" that remaps F-keys to send different keysyms (e.g. Meta+H instead of F5). KDE won't capture them as plain F-keys — you need to press them in that mode when setting the trigger. If KDE reports unusual keysyms, capture the real one first:

```bash
sudo apt install -y wev
wev
```

Press the key inside the `wev` window, read the `sym ...` line, close.

### Multiple keyboard layouts

The same physical key can send different keysyms depending on the active layout (e.g. `Meta+H` on en vs. `Meta+Р` on ru). Add a second trigger for each layout — all pointing to the same script.

## Usage

| Step | What happens |
|---|---|
| Press hotkey | Recording starts. Persistent "🎙 Listening" notification. |
| (speak) | ffmpeg writes to `~/.cache/kwispr/TS.wav` |
| Press hotkey again | ffmpeg shuts down gracefully (FIFO + 'q' → valid WAV). Notification → "⏳ Processing" |
| ~1-3 s | Whisper transcribes, hallucination filter cleans subtitle artifacts |
| done | Text lands in the clipboard, and ydotool simulates Ctrl+V in the focused window. Notification → "✅ Pasted" (auto-paste worked) or "Ready (in clipboard)" (paste manually with Ctrl+V) |

The text is **always** in the clipboard — even if the wrong window was focused, or auto-paste is disabled, Ctrl+V still works.

Minimum 1 second of audio is required — otherwise "⚠ Too short" (Whisper reliably hallucinates on <1s).

## Commands

- `kwispr.sh` (or `kwispr.sh toggle`) — start/stop recording
- `kwispr.sh retry <path.wav>` — retry transcription of an old WAV file

## Configuration (`.env`)

```bash
OPENAI_API_KEY=sk-...            # required
KWISPR_LANGUAGE=                 # empty = autodetect (ok for mixed ru/en); or 'ru', 'en'
KWISPR_AUTOPASTE=1               # 1 = auto Ctrl+V; 0 = clipboard only
KWISPR_SOUNDS=1                  # 1 = audio cues; 0 = silent
# KWISPR_SOUND_START=/path.wav   # optional custom sounds
# KWISPR_SOUND_STOP=/path.wav
# KWISPR_SOUND_READY=/path.wav
```

## Archive and rotation

All recordings + transcripts are kept in `~/.cache/kwispr/`. Files older than 30 days are deleted automatically on each run (only `*.wav` and `*.txt`, service files are not touched).

On API failure:
- the WAV stays in the archive
- `last-failed.txt` holds the retry command
- the retry command is also copied into the clipboard (paste it into a terminal)

## Why these design choices (best practices)

**Why FIFO + 'q' instead of SIGTERM for ffmpeg:**
`ffmpeg -f pulse` on Wayland/PipeWire sometimes ignores SIGINT ([ffmpeg trac #8369](https://trac.ffmpeg.org/ticket/8369)) and can leave a 0-byte WAV on SIGTERM (no trailer written). The documented graceful shutdown is writing `q` to stdin — done here via a FIFO held open by a background `sleep`.

**Why ydotool (not wtype / xdotool):**
- `wtype` doesn't work on KDE Plasma Wayland — KWin doesn't support the virtual-keyboard protocol ([reference](https://gist.github.com/danielrosehill/d3913d4c8cc69acaf3ee7772771c2f1d))
- `xdotool` is X11 only
- `ydotool` uses `/dev/uinput` at the kernel level, bypassing Wayland's input injection restrictions

**Why keyboard layouts don't break:**
ydotool sends **raw keycodes** (29=Ctrl, 47=V) — a stable hotkey regardless of layout. The target app pulls the text from `wl-copy`'s clipboard, where the correct Unicode already lives. We never "type" the text through ydotool — so the known [unicode-type bug](https://github.com/ReimuNotMoe/ydotool/issues/249) doesn't apply.

**Prompt-less transcription:**
Whisper was tested with various prompts. A bilingual prompt (e.g. "Voice dictation. Голосовая диктовка.") made the model occasionally **translate** speech into the prompt's language instead of transcribing as-is. We removed the prompt entirely — Whisper transcribes what it hears, and known hallucinations («Редактор субтитров», «Subtitles by ...», «Thanks for watching») are scrubbed by a post-processing regex.

Plus a minimum 1 second of audio before the API call (below that — immediate "Too short", saving an API round-trip).

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| "No .env" | Config not created | `cp .env.example .env; chmod 600 .env` |
| "OPENAI_API_KEY not set" | Placeholder instead of a key | Put a real `sk-...` into `.env` |
| "Too short" on normal speech | pulse hadn't opened yet (0.05s sleep too short) | Increase the `sleep` in `start_recording` |
| Records but doesn't paste | ydotoold not running or `/dev/uinput` not accessible | `systemctl status ydotoold` + `ls -la /dev/uinput` (should be `crw-rw---- root input`) |
| Pasted into wrong window | Focus was elsewhere when you pressed the hotkey | Place cursor in the target **before** pressing the hotkey to stop |
| "API 401" | Wrong API key | Verify on platform.openai.com |
| "API 429" | Rate limit / billing | Top up OpenAI balance |
| Empty clipboard after ✅ | Wayland clipboard glitch | `systemctl --user restart xdg-desktop-portal` |
| No notifications | `libnotify-bin` missing | `sudo apt install libnotify-bin` |
| Recording doesn't start | ffmpeg can't see mic | `pactl list sources short` — check default |
| Green mic LED stays on after a crash | Stale ffmpeg process | `pkill -f "ffmpeg.*pulse"` |

### Disable auto-paste

In `.env`: `KWISPR_AUTOPASTE=0`. Text stays in the clipboard — paste manually with Ctrl+V.

### Remove ydotool entirely

```bash
sudo systemctl disable --now ydotoold
sudo rm /etc/systemd/system/ydotoold.service
sudo rm /etc/udev/rules.d/80-uinput.rules
sudo gpasswd -d "$USER" input
rm ~/.local/bin/ydotool ~/.local/bin/ydotoold
```

Then set `KWISPR_AUTOPASTE=0` in `.env`.

## Not in scope (yet)

- Local Whisper as a fallback (cloud-only for now)
- Push-to-talk mode (toggle only)
- GUI / tray icon (the single persistent notification is enough)

> Note: KDE Custom Shortcuts can't bind to mouse buttons directly. If you want a mouse-button trigger, use a tool like `input-remapper` to remap the button to a keyboard shortcut, then bind that shortcut to kwispr.

## License

MIT — see [LICENSE](LICENSE).
