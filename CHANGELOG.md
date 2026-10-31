# Changelog

## Unreleased

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
