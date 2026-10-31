# Making a skin

## The easy way: the skin editor

Open the **[skin editor](https://kondrati0.github.io/headphones-guy/editor.html)** (or tray menu → **Skin → Draw a skin...**):

- **Accessories**: draw glasses, a scar, a beard or a cap on top of the face. The layer goes on every face he makes, so you draw it once. Mirror is on by default, right click erases, Ctrl+Z undoes.
- **Colors**: recolor the face, outline, eyes, headband and headphones, or start from a built-in skin.
- **Frames**: redraw whole frames if you want to go all in.

The preview nods to a generated beat, and you can freeze it on any face. When it looks right, click **Install in Headphones Guy**: the skin lands in the app and switches on (version 1.1+). **Download .ini** saves the file instead.

## By hand

A skin is one small `.ini` file. Put it in `%APPDATA%\HeadphonesGuy\skins\` and pick it in the tray menu under **Skin**. Tray menu → **Skin → Open skins folder...** opens that folder and drops an `example.ini` in it to copy from.

The menu re-reads the folder every time it opens, so you can edit a file, open the menu, click your skin and see the change. No restart needed.

## Just colors

```ini
[skin]
name=Night shift
face=7FA8FF       ; the yellow part
outline=1B2440    ; dark outline around the face
eyes=1B2440       ; eyes and mouth
headband=C8CCD6   ; the headphone band
cups=FFC83D       ; headphone color, set once when you pick the skin (you can still change it from the menu)
tongue=FF6F7F
```

Everything is `RRGGBB`, and anything you leave out stays like the classic guy.

## Accessories

`[overlay]` is drawn on top of every frame and moves with the head. Rows are numbered 0–15, and you only list the rows you use:

```ini
[skin]
name=Cool shades
1=111111
3=E53935

[overlay]
6=...1111111111...
7=...1111..1111...
8=....11....11....
9=...........3....
10=............3...
```

## Your own frames

To change the drawing itself, replace rows of any frame (16 characters each). Rows you leave out stay as in the built-in frame:

```ini
[skin]
name=Cyclops
1=FFFFFF

[frames]
listen.0=....BBBBBBBB....
listen.1=..BBkkkkkkkkBB..
listen.2=.B.kyyyyyyyyk.B.
listen.3=.B.kyyyyyyyyk.B.
listen.4=.BkyyyyyyyyyykB.
listen.5=.DCyyyyyyyyyyCD.
listen.6=.DLyyyy11yyyyLD.
listen.7=.DCyyy1ee1yyyCD.
listen.8=.DCyyyy11yyyyCD.
listen.9=.DCyyyyyyyyyyCD.
listen.10=..kyyyyeeyyyyk..
listen.11=...kyyyyyyyyk...
listen.12=....kkkkkkkk....
listen.13=................
listen.14=................
listen.15=................
```

Letters:

| Letter | What it is |
|---|---|
| `k` | outline |
| `e` | eyes and mouth |
| `y` | face |
| `B` | headband |
| `C` `D` `L` | headphone cup: main color, darker rim, highlight (all from the headphone color) |
| `t` | tongue |
| `1` … `9` | your own colors, set them in `[skin]` as `1=RRGGBB` … `9=RRGGBB` |
| `.` | transparent |

Frames you can replace:

| Name | When he shows it |
|---|---|
| `idle`, `idle_blink` | nothing playing, headphones around the neck |
| `awake`, `awake_blink` | music started, he is figuring out the beat |
| `listen` | normal nod |
| `loud` | loud parts |
| `pause` | breakdowns and short pauses |
| `build` | build-ups (he shivers) |
| `drop` | the whole drop |
| `talk` | mouth open, used by guys for voice apps like Discord |

Keep the face in rows 0–12 like the built-in frames: he moves down a couple of rows when he nods, and the hat sits on rows 0–3.

Made a good one? Open a pull request that adds it to `skins/` in the repo, or post it in an issue.
