# Kwispr

**Toggle voice dictation for Linux / Wayland / KDE Plasma via OpenAI Whisper.** Press F5 → speak → press F5 → text auto-pastes into the focused window.

![shell](https://img.shields.io/badge/shell-bash-blue) ![license](https://img.shields.io/badge/license-MIT-lightgrey) ![platform](https://img.shields.io/badge/platform-Linux%20Wayland-orange)

## Why

- **Stateless** — ~250 lines of bash, no daemons of our own, no GUI. KDE Shortcut runs the script, script exits. Next press runs it again.
- **Works anywhere on Wayland** — VS Code, Claude Code CLI, browser, Slack, terminal. Auto-paste via `ydotool` through `/dev/uinput` (kernel-level, compositor-agnostic).
- **Layout-independent** — sends raw Ctrl+V keycodes; apps pull Unicode text from the clipboard. Russian, English, mixed — works the same.
- **Three-stage audio feedback** — pip-pip-pip on start, pup-pup-pup on stop, ding-dong when transcript is ready. You always know what's happening without looking.
- **Persistent status bubble** — one notification transforms Listening → Processing → Ready, never stacks.
- **Never loses your speech** — on API/network failure, WAV is preserved and a retry command is put in your clipboard.
- **Whisper hallucination filter** — post-processing regex scrubs the subtitle artifacts Whisper leaks on short audio («Редактор субтитров», «Subtitles by», «Thanks for watching»).

**Tested on:** Ubuntu 25.10 + KDE Plasma 6 + PipeWire. Should work on any Linux with Wayland + `wl-clipboard` + `libnotify-bin`.

## Архитектура

```
F5 (KDE Custom Shortcut)
  └─ kwispr.sh toggle
       ├─ start: ffmpeg -f pulse → ~/.cache/kwispr/TS.wav (через FIFO)
       │         notify-send "🎙 Listening" (persistent)
       └─ stop:  write 'q' to FIFO → ffmpeg flushes WAV
                 notify-send replace → "⏳ Processing"
                 curl POST https://api.openai.com/v1/audio/transcriptions
                   model=whisper-1, temperature=0, prompt="Voice dictation..."
                 sed-filter subtitle hallucinations
                 wl-copy $transcript
                 ydotool key 29:1 47:1 47:0 29:0  (Ctrl+V if enabled)
                 notify-send replace → "✅ Pasted" / "Ready (in clipboard)"
```

**Никаких демонов на наш код.** Скрипт — stateless, запускается из KDE Shortcut, завершается. Состояние — lock-файл `~/.cache/kwispr/current.pid`. Единственный постоянно работающий компонент — системный `ydotoold` (опционально, для авто-вставки).

## Зависимости

| Пакет | Назначение | Источник |
|---|---|---|
| `ffmpeg` | запись с pulse (через FIFO + 'q' для правильного flush) | APT |
| `curl` | вызов OpenAI API | APT |
| `jq` | парсинг JSON ответа | APT |
| `wl-clipboard` | `wl-copy` для буфера | APT |
| `libnotify-bin` | `notify-send` для персистентного статус-бабла | APT |
| `pipewire-pulse` | pulse-compat слой на PipeWire | уже в Ubuntu 25.10 |
| `ydotool` v1.0.4 (опционально) | авто-вставка Ctrl+V через `/dev/uinput` | GitHub release → `~/.local/bin/` |

**Почему `ydotool` скачивается с GitHub, а не из APT:** в APT Ubuntu живёт древняя версия 0.1.8 без команды `key` и без демона. Нам нужна 1.0.4+.

## Установка

```bash
cd kwispr
./setup.sh                       # ставит deps, предлагает установить ydotool
cp .env.example .env
chmod 600 .env
# Впиши OPENAI_API_KEY в .env
```

При установке `ydotool`:
- скачивается v1.0.4 в `~/.local/bin/`
- создаётся udev-правило `/etc/udev/rules.d/80-uinput.rules` чтобы `/dev/uinput` принадлежал группе `input`
- юзер добавляется в группу `input` (нужен **relogin** после этого)
- создаётся system-level systemd service `/etc/systemd/system/ydotoold.service`

После relogin: `systemctl status ydotoold` должен показать `active (running)`.

## Бинд F5

KDE не знает про F5 в мультимедиа-режиме твоей клавиатуры — она шлёт что-то вроде `Meta+H` (en) / `Meta+р` (ru). Поймать реальный keysym:

```bash
sudo apt install -y wev
wev
```

Нажми F5 (mm-режим) в окне `wev`, посмотри `sym ...`, закрой.

Дальше:

1. **System Settings → Shortcuts → Shortcuts → Add New → Command/URL Shortcut**
2. Trigger: нажать F5 в mm-режиме (KDE запишет тот же keysym)
3. Action: `<абсолютный_путь>/kwispr.sh toggle`
4. Apply

Если пользуешься двумя раскладками — добавь второй trigger для другой раскладки (у меня `Meta+H` en + `Meta+P` ru, оба на тот же скрипт).

## Использование

| Шаг | Что происходит |
|---|---|
| F5 | Запись стартует, висит уведомление «🎙 Listening» |
| (говорить) | ffmpeg пишет в `~/.cache/kwispr/TS.wav` |
| F5 | Ffmpeg завершается (FIFO + 'q' = valid WAV), уведомление меняется на «⏳ Processing» |
| ~1-3 сек | Whisper транскрибирует, фильтр галлюцинаций чистит субтитровый мусор |
| готово | `wl-copy` → `ydotool Ctrl+V` → текст вставлен. Уведомление «✅ Pasted» / «Ready (in clipboard)» |

Минимум 1 сек аудио — иначе «⚠ Too short» (Whisper на <1 сек стабильно галлюцинирует).

## Команды

- `kwispr.sh toggle` — старт/стоп записи
- `kwispr.sh retry <path.wav>` — повторить транскрипцию старого файла

## Параметры `.env`

```bash
OPENAI_API_KEY=sk-...            # обязательно
KWISPR_LANGUAGE=                # пусто = autodetect (ru/en mix ок); или 'ru', 'en'
KWISPR_AUTOPASTE=1              # 1 = Ctrl+V авто; 0 = только в буфер
```

## Архив и ротация

Все записи + транскрипты в `~/.cache/kwispr/`. Файлы старше 30 дней удаляются автоматически (только `*.wav` и `*.txt`, служебные остаются).

При сбое API:
- `*.wav` остаётся в архиве
- `last-failed.txt` содержит команду повтора
- Команда копируется в буфер (чтобы сразу Ctrl+V в терминал)

## Особенности архитектуры (best practices)

**Почему FIFO + 'q' вместо SIGTERM:**
`ffmpeg -f pulse` на Wayland/PipeWire игнорирует SIGINT в некоторых сценариях ([ffmpeg trac #8369](https://trac.ffmpeg.org/ticket/8369)) и может не записать WAV-trailer при SIGTERM → 0 байт на выходе. Правильный graceful shutdown — write 'q' в stdin, что делается через FIFO, держащийся открытым фоновым `sleep`.

**Почему ydotool (не wtype/xdotool):**
- `wtype` не работает на KDE Plasma Wayland — KWin не поддерживает virtual-keyboard protocol ([источник](https://gist.github.com/danielrosehill/d3913d4c8cc69acaf3ee7772771c2f1d))
- `xdotool` — X11 only
- `ydotool` работает через `/dev/uinput` на уровне ядра, обходит Wayland security model

**Почему не ломает русскую раскладку:**
ydotool шлёт **raw keycodes** (29=Ctrl, 47=V) — это стабильный hotkey независимо от раскладки. Текст приложение тянет из `wl-copy`-clipboard, где уже лежит правильный Unicode. Мы не "печатаем" текст через ydotool — известный баг [ReimuNotMoe/ydotool#249](https://github.com/ReimuNotMoe/ydotool/issues/249) с Unicode-type не применяется.

**Prompt + filter против галлюцинаций Whisper:**
Whisper обучен на субтитрах и любит добавлять «Редактор субтитров А.Семкин», «Subtitles by ESO», «Thanks for watching» в конец на тишине или коротких записях. Два слоя защиты:
1. Bilingual neutral prompt `"Voice dictation. Голосовая диктовка."` (не смещает распознавание, но снижает вероятность галлюцинации)
2. Regex-фильтр известных паттернов после транскрипции (`sed -E 's/Редактор субтитров.*$//I'` и т.п.)

Плюс минимум 1 сек аудио перед отправкой в API (ниже — сразу «Too short», экономим API-запрос).

## Troubleshooting

| Симптом | Причина | Фикс |
|---|---|---|
| «No .env» | Не создан конфиг | `cp .env.example .env; chmod 600 .env` |
| «OPENAI_API_KEY not set» | Placeholder вместо ключа | Впиши реальный `sk-...` в `.env` |
| «Too short» при нормальной записи | pulse не успел открыться (sleep 0.05 недостаточно) | Увеличь задержку в `start_recording` |
| Записывает, но не вставляет | ydotoold не запущен или `/dev/uinput` недоступен | `systemctl status ydotoold` + `ls -la /dev/uinput` (должно быть `crw-rw---- root input`) |
| Вставляется не туда | Фокус был в другом окне в момент нажатия F5 | Ставь курсор куда надо **до** второго F5 |
| «API 401» | Ключ неверный | Проверь на platform.openai.com |
| «API 429» | Rate limit / баланс | Пополни баланс OpenAI |
| Буфер пустой после ✅ | Wayland clipboard глюк | `systemctl --user restart xdg-desktop-portal` |
| Нет уведомлений | `libnotify-bin` не стоит | `sudo apt install libnotify-bin` |
| Запись не запускается | ffmpeg не видит микрофон | `pactl list sources short` — проверь default |
| Зелёный LED-индикатор микрофона горит постоянно | Висячий ffmpeg-процесс после сбоя | `pkill -f "ffmpeg.*pulse"` |

### Отключить auto-paste

В `.env`: `KWISPR_AUTOPASTE=0`. Текст останется в буфере, вставишь руками Ctrl+V.

### Удалить ydotool полностью

```bash
sudo systemctl disable --now ydotoold
sudo rm /etc/systemd/system/ydotoold.service
sudo rm /etc/udev/rules.d/80-uinput.rules
sudo gpasswd -d "$USER" input
rm ~/.local/bin/ydotool ~/.local/bin/ydotoold
```

Потом `KWISPR_AUTOPASTE=0` в `.env`.

## Что не входит в текущую версию

- Локальный Whisper как fallback (cloud-only пока)
- Push-to-talk (только toggle)
- Бинд на кнопку мыши (через `input-remapper` если надо — можно руками)
- GUI / tray-иконка (единственное persistent уведомление — достаточно)

## Привязано к проекту

Этот инструмент — часть dev-environment проекта `macubuntu`. Вероятно будет выделен в отдельный репозиторий когда стабилизируется, пока живёт тут.
