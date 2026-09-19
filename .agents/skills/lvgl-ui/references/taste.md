# Taste

Layout and hierarchy rules that separate "designed" from "widget dump".

## Hierarchy

- One primary element per screen (the thing the screen is about). Make it bigger
  or more central, not just a different colour.
- Secondary text (labels, captions) is smaller and `UI_TEXT_DIM`.
- Group by proximity: related things close, unrelated things further apart. Do
  not rely on borders to separate everything.

## Spacing

- 8 px grid. Consistent outer margin.
- Siblings 8–12 px apart; groups 16–24 px apart.
- Internal padding ≥ 12 px; never let text touch a border.
- Equal gutters in a row; equal row heights.

## Density

- Cards are a last resort. Use whitespace, alignment and type for structure.
- No nested cards.
- Do not box every row; a label + value on a background is usually enough.

## Colour and contrast

- Body text ≥ 4.5:1; large/bold ≥ 3:1. Dim grey on a mid surface usually fails.
- ≤ 5 semantic colours; each encodes state or category.
- Accent is for the active/checked thing only.

## Motion

- Animate state changes (press, appear/disappear, value change), not decoration.
- 100–200 ms, ease-out, small distances.
- No bounce/elastic.

## Small-screen specifics

- Touch target ≥ 48 px. Extend the click area ~20 px for parallax-prone panels.
- Keep a status strip and a single control row; avoid multi-level menus until
  the flat screen is right.
- Fonts: one family, 2–3 sizes (e.g. 14 / 18 / 24). Do not mix families.
- Test the exact long strings ("brightness 100%") in the actual font/size.

## Pre-ship checklist

- [ ] No raw hex in screen code; only tokens.
- [ ] Every button built by the one helper; pressed/disabled/active all defined.
- [ ] Disabled controls visibly disabled and driven by a rule predicate.
- [ ] Contrast passes for body and dim text.
- [ ] Touch targets ≥ 48 px with extended click area.
- [ ] One primary element; clear hierarchy; consistent gutters.
- [ ] Icons from a single set, one size, no emoji.
- [ ] Background is deliberate (black on AMOLED for power/burn-in).
- [ ] Longest strings fit at the real font.
- [ ] Motion is state feedback only, 100–200 ms ease-out.
