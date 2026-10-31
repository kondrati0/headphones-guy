# Changelog

## 1.1.0

- Skins: classic, pumpkin, robot, demon, zombie, plus your own from a small .ini file (colors, an accessories layer drawn over every frame, whole frames). See docs/SKINS.md.
- Web skin editor: draw accessories, recolor, redraw frames with a live preview, then install into the app with one click (`headphonesguy://` links).
- Seasonal hats by the date (witch hat for Halloween week, Santa hat for the New Year), or any hat all year.
- Tempo easter eggs: sweats at 270+ BPM, steams out of the ears near 300, falls asleep at night when it's quiet.
- Guys for single apps (Windows 10 2004+): up to 3 extra guys, each listening to one program. Voice apps get a talking guy.
- Stream window for OBS: big guy and BPM on a chroma-key background.
- RGB sync through OpenRGB: the headphone color flashes on every beat, strobes on build-ups.
- Monthly stats card: hours, average tempo, drops, top steady tempo, saved as a PNG.
- Nodding tray icon (optional).
- Browser demo on GitHub Pages, running the same tracker and sprites compiled to WebAssembly.

## 1.0.1

- The exe is stripped: ~290 KB instead of ~830 KB.
- "Start with Windows" keeps working after you move the exe: the autostart entry follows it on the next launch.
- Tracker tests now fail (and break CI) when beat accuracy drops below set floors.
- Small code cleanups.

## 1.0.0 — first public release

- Real-time beat tracking from the Windows output (WASAPI loopback), 70–300 BPM, tuned for 130–300.
- Re-locks on track changes, crossfades, same-BPM transitions and seeks.
- 240+ BPM tempo check and 3:4 polyrhythm guard for terror / breakcore.
- Section awareness: deeper nods on loud parts, half-time on breakdowns, shiver on build-up rolls, drop face for the whole drop.
- Headphones-on-the-neck idle state, blinking, wake-up face.
- Tray menu: position, nod timing (±10 / ±50 ms), headphone color, animations toggle, start with Windows.
- Hides during fullscreen apps, follows default output device changes, per-monitor DPI aware.
- English UI, Russian on Russian Windows (`language=` in settings.ini to override).
