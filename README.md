<p align="center">
  <img src="docs/banner.png" alt="Headphones Guy" width="760">
</p>

<p align="center">
  <b>A tiny pixel smiley that lives on your Windows taskbar and nods to whatever you're playing.</b><br>
  Spotify, YouTube, a DJ set, a game — if Windows plays it, he hears it.
</p>

<p align="center">
  <a href="../../releases/latest"><img alt="Download" src="https://img.shields.io/badge/download-HeadphonesGuy.exe-ff5d73?style=for-the-badge"></a>
  <a href="https://kondrati0.github.io/headphones-guy/"><img alt="Try it in the browser" src="https://img.shields.io/badge/try_it-in_the_browser-3db8ff?style=for-the-badge"></a>
  <img alt="Windows 10/11" src="https://img.shields.io/badge/Windows-10%20%7C%2011-3d7dff?style=for-the-badge">
  <img alt="Size" src="https://img.shields.io/badge/size-~400%20KB-ffc83d?style=for-the-badge">
  <img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-c8ccd6?style=for-the-badge">
</p>

<p align="center">
  <img src="docs/demo.gif" alt="Headphones Guy on the taskbar" width="900">
</p>

**Not sure yet? [Try him in the browser](https://kondrati0.github.io/headphones-guy/)**: drop a track and watch him nod. Same beat tracker as the app, compiled to WebAssembly, and your file never leaves the page.

## What it does

He sits next to your tray icons and listens to the sound your PC is putting out, then **nods exactly on the beat**. No setup, no audio routing, no virtual cables.

- **Beat tracking in real time**: tempo and beat position from the system audio, re-locks within seconds when the track changes or you scrub.
- **Built for fast music**: tuned on hardcore, uptempo, terror, breakcore, DnB, neurofunk and dubstep. Handles **130–300 BPM**, including the nasty 130-vs-260 and 3:4 polyrhythm cases.
- **Knows the song structure**: nods deeper on loud parts, chills on breakdowns, shivers during build-up snare rolls, and **loses it on the drop**, for the whole drop rather than just a second.
- **Has a life**: headphones around his neck when nothing plays, blinks, wakes up when music starts. Sweats at 270+ BPM, steams out of his ears near 300, falls asleep at night when it's quiet.
- **Featherweight**: one ~400 KB `.exe`, no installer, no dependencies, ~2% of one CPU core.
- **Private**: no internet access at all (OpenRGB sync only talks to OpenRGB on your own PC). Audio is analysed in memory and thrown away.

<p align="center">
  <img src="docs/closeup.gif" alt="Up close: nod, loud, build-up, drop" height="200">
  &nbsp;&nbsp;
  <img src="docs/faces.png" alt="All his faces" width="620">
</p>

## Make him yours

Five built-in skins (classic, pumpkin, robot, demon, zombie), seasonal hats that show up by themselves (a witch hat for Halloween week, a Santa hat for the New Year), and the tempo easter eggs:

<p align="center">
  <img src="docs/looks.png" alt="Skins, seasonal hats and tempo easter eggs" width="900">
</p>

Or draw your own in the **[skin editor](https://kondrati0.github.io/headphones-guy/editor.html)**. Put glasses, a scar or a cap on him once and they stay on every face he makes, recolor him, or redraw whole frames. Watch him nod with it to a beat, then hit **Install in Headphones Guy**: the skin lands in the app and switches on. Prefer files? It's [one small .ini](docs/SKINS.md).

<p align="center">
  <img src="docs/editor.png" alt="The skin editor" width="820">
</p>

## Extras

- **A guy per app**: give Spotify its own guy, or Discord one that talks along when people speak. Up to 3, next to the main one (Windows 10 2004+).
- **Stream window**: a big guy and the BPM on a green background. In OBS, add *Window Capture* plus a *Chroma Key* filter and he nods on your stream.
- **RGB in the beat**: your keyboard, mouse and strips flash the headphone color on every beat and strobe on build-ups, through [OpenRGB](https://openrgb.org).
- **Monthly card**: hours listened, average tempo, drops survived and your top steady tempo, as a picture to post wherever.
- **Nodding tray icon**: for people who hide the taskbar.

<p align="center">
  <img src="docs/stats-card.png" alt="Monthly stats card" width="640">
</p>

## Download & run

1. Grab **`HeadphonesGuy.exe`** from the [latest release](../../releases/latest).
2. Run it. He appears on the taskbar, left of the tray icons.
3. Play some music.

> **"Windows protected your PC"?** The exe isn't code-signed (certificates cost money), so SmartScreen warns on first launch. Click **More info → Run anyway**. The full source is right here, and you can build it yourself in a minute (see below).

To quit, change settings or make him start with Windows, **click his icon in the tray**.

## Tray menu

| Item | What it does |
|---|---|
| Move left / Move right / Reset position | Nudge him along the taskbar if he overlaps something |
| Nod earlier / later (±10 ms, ±50 ms) | Sync the nod to your ears. **Bluetooth headphones** are usually 150–250 ms late, so use *Much later* a few times |
| Headphone color | 8 presets or any custom color |
| Skin | Built-in skins, your own from the skins folder, *Draw a skin...* (opens the web editor) and *Open skins folder...* |
| Hat | By the date (default), no hat, or any hat all year |
| Go harder on loud parts and drops | Turn the dynamic moves (deep nods, drop face, build-up shiver) on or off |
| Tempo easter eggs | Sweat, steam and sleeping at night |
| Nodding tray icon | The tray icon nods too, handy with an auto-hiding taskbar |
| Guys for single apps | Up to 3 extra guys, each listening to one app that is playing sound right now. Voice apps (Discord, Telegram, TeamSpeak, Zoom...) get a talking guy instead |
| Stream window (for OBS) | Opens a resizable window with a big guy and the BPM. In OBS: *Window Capture* → that window, then a *Chroma Key* filter. Right-click it for the menu |
| Sync RGB lighting (OpenRGB) | Start OpenRGB, turn on *SDK Server* there, then tick this |
| Monthly stats card... | Saves this month's card as a PNG and opens it |
| Start with Windows | Adds or removes him from autostart (current user, no admin) |

He hides automatically when a fullscreen game or video is on that monitor, and comes back afterwards.

## Settings file

Everything from the menu is saved to `%APPDATA%\HeadphonesGuy\settings.ini`. A few extra knobs only live there:

```ini
[guy]
offset=0            ; horizontal shift in pixels
nod_offset_ms=10    ; nod timing, -500..500
cups_color=FF5D73   ; headphone color, RRGGBB
moves=1             ; deep nods / drop / build-up animations
skin=classic        ; classic | pumpkin | robot | demon | zombie | custom:<file name>
hat=auto            ; auto (by the date) | off | witch | santa | party
easter_eggs=1       ; sweat, steam, sleeping at night
tray_nod=0          ; nodding tray icon
openrgb=0           ; RGB sync through OpenRGB
apps=               ; per-app guys, e.g. spotify.exe;discord.exe
language=auto       ; auto | en | ru
sharp_onsets=1      ; 0 = older, softer onset timing (try it if the nod feels late on your music)
log=0               ; 1 = write mode/BPM to log.txt once per second

[stream]
open=0              ; stream window open
background=00B140   ; chroma key color
show_bpm=1          ; BPM under the guy
```

Monthly numbers live next to it in `stats.ini`, the cards are saved as `stats-YYYY-MM.png`, and your skins go in `skins\`.

## How it works

```
system audio (WASAPI loopback)
   └─ spectral flux, 2048-pt FFT every 5 ms ──► tempo: autocorrelation + long tempo memory (prior ~170 BPM)
   └─ kick-weighted flux, 11 ms window ──────► beat phase: long + short beat histograms, seek / track-change recovery
   └─ bass share of the mix ─────────────────► sections: loud / breakdown / build-up (snare roll density) / drop
                                                    │
                              render thread ◄───────┘  nod frames scheduled on the predicted beat grid
```

A few things that make it behave on extreme music:

- **Track changes**: a fresh 6 s window keeps voting against the long tempo memory, and when the old tempo loses its support the memory restarts. Back-to-back tracks re-lock in about 4–7 s, crossfades included.
- **130 or 260?** At 240+ BPM hi-hats lie, so the double tempo is only accepted when the snare/body band agrees over several seconds.
- **3:4 polyrhythms**: triplet rolls and dotted kick patterns can't drag the tempo away unless kick *and* snares really move.
- **Drops** are detected from how much of the mix the bass takes, so the overall loudness of the master doesn't matter. A real break or build has to come first, and the drop face stays on until the bass gives way again.
- **Seeking**: a short hole in the audio (or a discontinuity flag from Windows) triggers a fresh phase lock from the next second of audio. Gapless jumps are caught by a short beat histogram within a few seconds.

All the DSP is in one portable header, [`src/tracker.h`](src/tracker.h), with no Windows code in it. So are the sprites, skins and hats ([`src/sprites.h`](src/sprites.h)) and the animation ([`src/animator.h`](src/animator.h)): the browser demo draws him with the very same code. The Win32 side (taskbar placement, layered windows, tray, WASAPI, per-app capture, OpenRGB, the stats card) is [`src/main.cpp`](src/main.cpp) plus a few headers next to it.

## Build it yourself

You need MinGW-w64. From Linux or WSL:

```sh
sudo apt install g++-mingw-w64-x86-64-win32 binutils-mingw-w64-x86-64
sh build.sh            # -> build/HeadphonesGuy.exe
```

On Windows with [MSYS2](https://www.msys2.org/):

```sh
pacman -S mingw-w64-x86_64-gcc
CXX=g++ WINDRES=windres sh build.sh
```

## Tests

The tracker builds with any C++17 compiler, no Windows needed:

```sh
cd tests
g++ -O2 -std=c++17 -I../src test_tracker.cpp     -o t && ./t   # synthetic genres 132–260 BPM
g++ -O2 -std=c++17 -I../src test_transitions.cpp -o t && ./t   # back to back, crossfades, same-BPM shifts, breakdowns
g++ -O2 -std=c++17 -I../src test_chaos.cpp       -o t && ./t   # breakcore triplet rolls must not fool it
g++ -O2 -std=c++17 -I../src test_sprites.cpp     -o t && ./t   # frames, hats, easter eggs, drawing stays inside the box
```

Each test exits with an error when accuracy drops below its floor, so CI catches regressions.

### Browser demo

```sh
pip install ziglang
sh web/build.sh                      # -> web/guy.wasm
python3 -m http.server -d web 8000   # open http://localhost:8000
```

`test_seek.cpp` and `eval.cpp` run on your own tracks (decoded with ffmpeg to raw float32, 48 kHz mono). See the comment at the top of each file.

## FAQ

**My antivirus flags it.** Small unsigned exes that capture audio and sit on top of the taskbar sometimes trip heuristics. It makes no network calls and writes only to its own settings folder. Build it from source if you want to be sure.

**He nods a bit late or early.** Use *Nod earlier / later* in the tray menu. Bluetooth adds 150–250 ms. For wired headphones the default is usually right.

**He picked half or double the tempo.** That happens on tracks where both feel valid (dubstep at 150/300, halftime breaks). He usually corrects himself within a few seconds once the drums make it clear.

**Multiple monitors / vertical taskbar?** He lives on the primary taskbar and supports horizontal and vertical layouts, plus per-monitor DPI.

**Windows 11?** Developed and used on Windows 10. Windows 11 uses the same taskbar window classes, so it should work. If he shows up in a weird place, open an issue with a screenshot.

**"Install in Headphones Guy" in the skin editor does nothing.** The app has to be version 1.1 or newer and has to have run at least once (that's when it registers itself for those links). Your browser asks whether to open Headphones Guy: allow it. Or use *Download .ini* and put the file in the skins folder.

**The guys for single apps menu is greyed out.** It needs Windows 10 version 2004 (May 2020) or newer. Only apps that are playing sound right now show up in the list, so start the music first.

**Does it work with any player?** Yes. He listens to the Windows output device, not to a specific app. If you switch the default output device he follows it.

## License

[MIT](LICENSE). Do whatever you want, a link back is appreciated.
