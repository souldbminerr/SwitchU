# qlaunch layout reference (extracted from retail BFLYT)

Source: retail qlaunch `0100000000001000` v1477443784, `lyt/*.szs`
(Yaz0 SARC → BFLYT). All layouts are **1280×720, center-origin, +Y up**.
Pixel conversion: `px = 640 + x`, `py = 360 - y`.

## Home screen (ResidentMenu.szs → RdtBase.bflyt)

- **Game tile row** (`N_GameRoot` @ y=+38 → **py=322 center**):
  - Icon cell **270w × 244h**, horizontal **pitch = 270px**.
  - Visible icon art is 256×256; gap between cells = 270−256 = **14px** (7 per side).
  - Focus/cursor box (`RdtBtnIconGame/N_BtnFocusKey`) = **264 × 264** (art + 4px each side).
- **Dock** (`N_System` @ (−54, −184), size 1160×80 → row center **py=544**, centered at px=586):
  - Button hit-box (`N_BtnFocusKey`) = **80 × 80**.
  - FullLauncher button = **170 × 170** (larger).
- **User avatar** (`N_MyPage` @ (−303, +295), 544w bar → left edge ≈ px=65, **avatar ≈ px=88, py=65**):
  - MyPage focus box = 64×64.
- **Separator lines**: 2px tall (`Line_Root`, `LineHeader_Root`).

## Top HUD (ResidentMenu.szs → Hud.bflyt)

Cluster anchored top-right; children laid out right→left from the anchor
(origin=right). Each element is 40px tall.

| element        | x (rel anchor) | size    | note                    |
|----------------|----------------|---------|-------------------------|
| N_BatteryNum   | −50            | 32×40   | battery % + gauge       |
| N_Margin_00    | −102           | 16×40   | gap                     |
| N_Signal (wifi)| −118           | 32×40   | wifi arcs               |
| N_Margin_01    | −150           | 20×40   | gap                     |
| N_Time         | −170           | (16)×40 | clock                   |

So right→left: **Battery · 16px · WiFi(32) · 20px · Time**.
- HudTime: digits `N_Num`, colon/mark `N_Mark` @ x=−41, `N_AMPM` 30×40 (hidden in 24h).
- BatteryConsole: gauge `N_GaugeNml` (green normal / other state), `N_Num` %.

## Full launcher grid (Flaunch.szs)

- `FlcBtnIconGame/N_BtnFocusKey` = **178 × 178** (grid icon, smaller than home row).
- `FlcCntMain`: scroll container; `AreaNmlNav/N_Root` nav width 410.

## Settings (Set.szs — real System Settings)

- **Left category rail** (`AreaNmlNav/N_Root`) = **410w × 720h**.
- **Nav category button** (`BtnNav_Root/N_BtnCnt`) = **300 × 70**.
- Category rows (`SetBaseNav`) stacked; most **70px** tall, section headers 100px,
  divider spacers 30px. Order: Sup, FlightMode, Display, Sound, Lock, Child,
  Internet, DataMng, Player, Mii, Amiibo, —line—, Theme, Ntf, Sleep,
  Controller, Tv, Hardware.
- **Content list row** (`BtnListNml/N_BtnCnt`) = **840 × 60**.
- Content area (`AreaNml`) = full 1280×720; header/footer separators 2px.

## Fonts & text sizes (verified from Set.szs txt1 + fnl1)

- UI font is **`nintendo_udsg-r_std_003`** (Nintendo UD Shin Go). SwitchU's
  `romfs/fonts/font.otf` IS this exact font (name table confirms) — typeface is 1:1.
- Point sizes / rendered pixel line-height (`fsY`):
  - Page title `TextH1` → **udsgr_40** (~40 pt, 42 px), left-aligned.
  - List row `BtnListNml` → **udsgr_24** (24 pt, 36 px), left, v-centred.
  - Nav category `BtnNav` → **udsgr_40 @ 22×33** (33 px), **center/center**.
  - Normal `TextNml` → **udsgr_20** (20 pt, 30 px), left.
  - Small/sub → **udsgr_16** (16 pt, 24 px).
- SwitchU loads font.otf at 24 pt; to hit a qlaunch N-pt size, draw at scale N/24.

## HUD is textures, not text (ResidentMenu.szs → timg/__Combined.bntx)

Time/battery/wifi use bitmap glyph + icon textures (BC4/R8 single-channel alpha
masks, tinted by material colour), NOT the UI font. Key textures:

| texture              | fmt  | size    | use                        |
|----------------------|------|---------|----------------------------|
| OceanFontNumL        | R8   | 14×240  | 10 digits stacked (each 14×24) — battery %/clock |
| OceanFontMM          | BC4  | 16×16   | medium digits (clock)      |
| OceanFontColonM      | BC4  | 8×28    | clock colon                |
| OceanFontAM/PM       | BC4  | 16×16   | AM/PM                      |
| OceanFontPerM        | BC5  | 16×16   | percent sign               |
| IcoWifi_00.._03_32   | BC4  | 32×32   | wifi signal (0–3 bars)     |
| IcoWifiUnset_32      | BC5  | 32×32   | wifi searching/none        |
| IcoPlane             | BC4  | —       | airplane mode              |
| IcoBattM_64          | BC4  | 36×20   | battery gauge body         |
| IcoChargingM_22      | BC4  | 22×22   | charging bolt              |
| IcoUser_128          | BC4  | 128×128 | default user icon          |

To use 1:1: BC4/R8 deswizzle+decode → PNG alpha, bundle in romfs, render tinted.
BC4 = 8 bytes/4×4 block (2 endpoints + 3-bit indices); simpler than BC7.

## Icon-font glyphs (romfs/fonts/switch_icons.ttf — Nintendo Ext, verified by render)

| glyph            | cp     | | glyph          | cp     |
|------------------|--------|-|----------------|--------|
| A/B/X/Y          | E0E0-3 | | Handheld console| E121   |
| L/R/ZL/ZR        | E0E4-7 | | Pro Controller  | E12C   |
| Dpad (all)       | E0EA   | | User/Mii face   | E131   |
| Plus / Minus     | E0F1/2 | | Chat/message    | E132   |
| Home             | E0F4   | | eShop bag       | E133   |
| Power            | E0F3   | | Album           | E134   |
| Settings gear    | E130   | | All-apps grid   | E135   |
