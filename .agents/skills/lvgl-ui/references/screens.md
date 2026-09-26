# The hero screen (pet / character apps)

For a virtual-pet or companion screen the winning pattern is the reverse of a
toolbar app: **the character is the screen, controls are summoned, needs are
coarse, and one "call" points at the fix.** This mirrors the original Tamagotchi
(three buttons + a summoned icon row) and modern pets (Tamagotchi On shows *no
icons* until you press A).

## Zones

```
top      status: name/stage + clock/battery   (small, dim, NOT interactive)
need     two or three coarse meters (hearts), not five bars
middle   the character, large and centred, on a soft pedestal
         a single pulsing "call" badge over its shoulder when it needs something
bottom   3 big rounded buttons (the frequent actions) + one small gear
overlays everything else is summoned: the icon menu, the meter, settings
```

## Rules

1. **The character owns the middle ~60%.** Reserve it; nothing else competes.
2. **At most three persistent actions**, sized to the panel. On ~1.8" / 368 px,
   7 mm ≈ 90 px, so three ~94 px tiles is the honest maximum. Top corners are
   hard to reach — keep interactive things at the bottom.
3. **Summon the rest.** A gear opens the full icon grid (Feed/Snack/Lights/Clean/
   Med/Meter/Settings/Close). This is the original's icon row, not a deep menu.
4. **Coarse needs.** Hearts, not bars: `hearts = (stat * 4 + 50) / 100`, filled vs
   ~20 % opacity. Show the two a user can act on; let the face and the world carry
   the rest (dirty sprite, sick look, sleeping).
5. **One call.** Compute the single most urgent need in the model
   (`pet_need()`), show one pulsing badge with a matching icon, and **make the
   badge tap perform the fix**. Also highlight the matching bottom button. This
   replaces a wall of meters and a "not hungry" toast.
6. **No hidden gestures for the core loop.** Long-presses are invisible to
   children; put snack/lights/med in the summoned menu instead. Reserve long-press
   for destructive adult things (reset), and let a tap on a dead pet start over.
7. **No persistent text a pre-reader can't use.** A one-word caption is fine;
   sentence-length status is not.

## State wiring

- `pet_need()` (pure, host-tested) → snapshot `need` → the badge.
- `pet_action_enabled()` (pure) → the same snapshot → each tile's `DISABLED`
  state. Never re-derive rules in the UI.
- Highlight, don't scold: a lit call beats a disabled button plus a toast.

## What to remove from a toolbar-style screen

A five-bar stat row, a six- or seven-button bar, always-on clock/stage text,
long-press snack, and a floating caption bubble are all things a hero screen
does without. Keep them only behind the summoned menu.
