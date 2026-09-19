# Sprite generation prompts

One character, one session, one reference image. Regenerate the **whole sheet**
if the character drifts, not a single cell.

## Rules that make the pipeline work

- Flat background: pure **`#00FF00`** (prepare_sprites.py keys it out).
- The pet centred, feet near the bottom, no shadow outside the silhouette.
- **Pixel art**, thick outline, chunky limbs, big eyes; ≤ 32 colours.
- Same palette and proportions across every frame; the character must be
  recognisable at 160 px.
- Export each cell as its own PNG named `<stage>_<state>_<frame>.png`,
  e.g. `baby_idle_0.png`, `baby_happy_1.png`, `baby_sleeping_0.png`.

## Sheet to generate (dog, baby stage)

8 states x 3 frames. Minimum for Phase 3 is idle; the rest fill in mood changes.

| State | Frames | Notes |
|---|---|---|
| `idle` | 3 | gentle breathing; frame 0 = neutral |
| `happy` | 2 | eyes up / closed, tail wag |
| `eating` | 2 | mouth open, little bowl optional |
| `playing` | 2 | excited, ears up |
| `sleeping` | 2 | curled, `z` optional |
| `sad` | 2 | droopy ears, downcast eyes |
| `sick` | 2 | pale, sweat drop |
| `dirty` | 2 | smudged, maybe a fly |

## Base prompt (adapt per state)

> A cute chibi **pixel-art puppy** character, front-facing, full body, thick dark
> outline, big round eyes, flat cel shading, 16-bit game sprite, limited palette
> of about 24 colours, standing on a plain flat **#00FF00** background, centred,
> no shadow, no text, no border, high contrast, crisp pixels. State: **IDLE**,
> frame N of 3 — subtle breathing, eyes open, neutral happy expression.

Reference-locking tools: attach the first approved frame as an image reference
(Midjourney `--cref`, ChatGPT image edit, PixelLab character mode) so every later
frame keeps the same face and palette.

## After generating

```bash
python3 tools/sprites/prepare_sprites.py --raw tools/sprites/raw --out assets/sprites
python3 tools/sprites/to_lvgl.py --in assets/sprites --out firmware/main/sprites
```

Then wire the sprite table in `pet_ui.c` (see the `lvgl-ui` skill) and update the
structure diagram if the module list changed. `tools/sprites/raw/` is git-ignored
(the owner's source images stay private).
