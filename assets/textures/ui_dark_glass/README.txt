# assets/textures/ui_dark_glass/ — UI Texture Assets
# Task 164

## Purpose
These textures are used by QML for subtle UI surface treatments.
All textures follow the neo-brutalist theme: near-black, low-opacity.

## Required files

| File                  | Size      | Use                                      |
|-----------------------|-----------|------------------------------------------|
| panel_bg.png          | 256×256   | Main panel background (repeatable tile)  |
| card_bg.png           | 256×256   | Device card background                   |
| overlay_bg.png        | 256×256   | Stats overlay background                 |
| separator.png         | 4×1       | Horizontal rule (1px white, 25% opacity) |

## Design spec (neo-brutalist)
- Background: pure black (#000000) — NO gradients, NO blur
- Borders: 2px solid white (#FFFFFF)
- All textures should be FLAT and SOLID — not glass-like despite the folder name
- The "dark_glass" folder name is legacy — treat all as flat black fills

## How to generate on your PC

### Option A — ImageMagick (create solid black tiles)
```
magick convert -size 256x256 xc:#000000 panel_bg.png
magick convert -size 256x256 xc:#0A0A0A card_bg.png
magick convert -size 256x256 xc:#050505 overlay_bg.png
magick convert -size 4x1 xc:"rgba(255,255,255,0.25)" separator.png
```

### Option B — GIMP or Photoshop
Create 256×256 black fills with subtle noise (5%) for texture.

## Note
In the current QML implementation (Theme.qml), these textures are referenced
as colour values, not image files. The PNG textures are optional enhancements.
The app works correctly without them — black fallback is built into Theme.qml.
