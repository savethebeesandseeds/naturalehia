# Naturalehia homepage concept assets

Generated on 2026-08-21 with the built-in OpenAI ImageGen workflow and refined on
2026-08-22 with locally processed cutouts and user-supplied hero details. The
approved full-page design is preserved as
`homepage-reference.png`. While the visual direction is being refined, the
production homepage references PNG files directly so every reviewable image has
one unambiguous filename.

No legacy Naturalehia artwork was supplied to the generator. The generated asset
set was derived from the new approved homepage concept only.

## Palette surfaces

- Dark artwork surface: deep forest `#082F27`
- Light artwork surface: warm parchment `#F4EAD7`
- Illustration accents: charcoal, muted moss, and restrained antique gold

ImageGen returned opaque RGB files even when transparent alpha was requested. The
original opaque PNG masters are retained unchanged. The three principle plates and
revised seed were then processed locally with tukevejtso's artwork-matte pipeline,
which removes paper globally (including enclosed openings), preserves faint marks,
and unmattes edge colors. Their `-transparent.png` siblings are the production
fallbacks used by the homepage.

## Which files to review

- `homepage-reference.png` is the approved complete-page concept, not a live page
  asset.
- Files ending in `-transparent.png`, plus the RGBA hero and observation plates,
  are isolated images currently displayed by the homepage. Review these when
  judging background removal and blending.
- Matching PNG names without `-transparent` are untouched ImageGen originals.
- The project panels and evidence band are full-bleed images and therefore need no
  transparent sibling.

Responsive encodings are intentionally deferred until the artwork is approved.
This folder currently contains reviewable PNG masters only—no resized WebP clones.

## Asset inventory

| File | Dimensions | Intended placement |
| --- | ---: | --- |
| `homepage-reference.png` | 768 x 2048 | Approved complete-page visual reference |
| `hero-living-observatory.png` | 1228 x 941 | Production transparent hero artwork on the deep-forest field |
| `hero-living-observatory-previous.png` | 1536 x 1024 | Preserved opaque hero iteration |
| `hero-observation-animal-identity.png` | 2172 x 724 | Animal-identity plate in the hero observation rail |
| `hero-observation-colony-state.png` | 2172 x 724 | Colony-state plate in the hero observation rail |
| `hero-observation-molecular-logic.png` | 2172 x 724 | Molecular-logic plate in the hero observation rail |
| `principle-name-leaf.png` | 1254 x 1254 | NAME principle illustration |
| `principle-name-leaf-transparent.png` | 1254 x 1254 | Production NAME cutout with genuine alpha |
| `principle-observe-binoculars.png` | 1254 x 1254 | OBSERVE principle illustration |
| `principle-observe-binoculars-transparent.png` | 1254 x 1254 | Production OBSERVE cutout with genuine alpha |
| `principle-understand-magnifier.png` | 1254 x 1254 | UNDERSTAND principle illustration |
| `principle-understand-magnifier-transparent.png` | 1254 x 1254 | Production UNDERSTAND cutout with genuine alpha |
| `project-fauna-bear-panel.png` | 1122 x 1402 | Elder Brother of Fauna project portal |
| `project-molecular-protein-panel.png` | 1000 x 1250 | Logic Gates project portal |
| `evidence-band-overlay.png` | 1672 x 941 | Claims/evidence dark band |
| `opensource-seed-v2.png` | 1254 x 1254 | Revised invitation art with a faint companion botanical |
| `opensource-seed-v2-transparent.png` | 1254 x 1254 | Production revised-seed cutout with genuine alpha |

## Transparent cutout provenance

The local `artwork` engine in tukevejtso's background-removal pipeline generated
the four transparent PNGs on 2026-08-22. The principle plates use flat parchment models
with CIE76 weak/strong thresholds of `1.25/5.0`, `1.15/5.0`, and `1.15/4.5`.
The revised seed uses a quadratic parchment model with thresholds `2.5/5.0` to
retain its faint companion botanical. Every run uses a 64px known-empty edge
guard, component-aware grain rejection, alpha floor 2, alpha ceiling 250, and
edge-color unmatting. The source PNGs are never overwritten.

## Generation brief

Common style prompt: original natural-history copperplate engraving combined with
a restrained contemporary research interface; editorial composition; deep forest,
warm parchment, charcoal, moss, and antique gold; fine botanical texture and
scientific marginalia; no readable text, logos, frames, or watermarks; avoid stock
wildlife, neon biotech, glossy 3D, glassmorphism, and science-fiction HUD clutter.

Asset-specific prompts:

- `hero-living-observatory.png`: calm front-facing spectacled bear emerging from
  cloud-forest botanicals, joined by a gold evidence thread to honeycomb colony
  geometry, a protein ribbon, waveform, specimen marks, and a small field bird.
- `principle-name-leaf.png`: centered four-leaf botanical specimen with generous
  padding and precise engraved veins.
- `principle-observe-binoculars.png`: centered classic field binoculars with
  tactile engraved metal and leather detail.
- `principle-understand-magnifier.png`: antique magnifier examining a cell, with a
  few restrained molecular-node marks.
- `project-fauna-bear-panel.png`: vertical forest field, spectacled bear in the
  lower-right, botanical layers, pawprint and nonverbal identity-timeline marks,
  preserving quiet upper-left space for accessible HTML copy.
- `project-molecular-protein-panel.png`: vertical parchment field, large protein
  ribbon in the lower-right, sparse molecular, honeycomb, orbit, and XOR-grid marks,
  preserving quiet upper-left space for accessible HTML copy.
- `evidence-band-overlay.png`: wide forest field with far-left ferns, a provenance
  fingerprint, a fine gold observation trail, and a right-edge waveform, leaving a
  broad central copy-safe region.
- `opensource-seed-v2.png`: targeted revision of the seed illustration with the
  focal seed reduced and shifted left, plus a pale archival flowering specimen on
  the right to restore the depth and botanical shadow seen in the approved concept.

Typography, navigation, buttons, rules, status labels, and truth-table values
remain semantic HTML/CSS/SVG. The three user-supplied observation plates contain
visible raster text; equivalent labels remain available as visually hidden HTML.
