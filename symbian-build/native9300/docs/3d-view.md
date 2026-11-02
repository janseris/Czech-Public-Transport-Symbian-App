# 3D view and street-level panoramas: plan

Status (1 Nov 2026): the 3D test (0.40-0.43) runs our renderer; the source for the 3D city is Mapy.com's own 3D (below).

## Goal

Walk through a 3D model of the streets around a place on the map, and look at Mapy.com's street-level
photos (panoramas) where they exist: a "street view" that runs on the Nokia 9300 with the 3D engine
from the Skyrim 9300 project (500-5000 triangles at 5-15 fps).

## What the scene is made of

| Part | Source | Notes |
|---|---|---|
| Ground | The map tiles the app already loads (aerial or standard), flat textured squares | ~15-25 visible tiles, ~50 triangles |
| Buildings | OpenStreetMap outlines and heights from the static data files (`maps9300-data`, `v1/b/15/x/y.z`) | Walls as quads, flat roofs, shaded by facing. Height: `height`, else `building:levels` x 3 m, else ~2-3 floors. 150-250 buildings within ~200 m: ~2,000-4,000 triangles |
| Route | The navigation line laid on the ground | A few dozen triangles |
| Sky, distance | One colour or gradient; fog at ~200-300 m | Far buildings fade instead of popping in; simpler boxes in the middle distance |
| Panoramas | Mapy.com Static Panorama API (`api.mapy.cz/v1/static/pano`) | See below |

Data measured (whole Czech Republic, first build): 5.1 M buildings, 137 MB in 80,193 cell files; a cell
is 569 B median, 4.5 KB at 90 %, 42 KB at most. A walk downloads a few KB per cell.

## Panoramas: the Static Panorama API

- `https://api.mapy.cz/v1/static/pano?lon&lat&yaw&pitch&fov&width&height&radius&apikey`: a JPEG of
  the view from the panorama nearest the point (within `radius`, default 50 m), up to 1024 px,
  `fov` from pi/20 to pi/2. 4 credits per image (250,000 free credits a month).
- **Terms:** online display only, no long-term storage: RAM only, dropped when leaving the view, no disk
  cache. The image carries the copyright text.
- **A cube map from 4-6 requests:** a square image with `fov = pi/2` is one face of a cube around the
  camera. Yaw 0, pi/2, pi, 3pi/2 at pitch 0 are the sides; pitch +-pi/2 the sky and the ground.
  16-24 credits per place; at 256 px a face is ~15-25 KB (to be measured).
- The website's own panorama server (`//panorama-mapserver.mapy.cz/compose_pano` in its config) may be
  used too (undocumented addresses are allowed since 1 Nov 2026); to look into.
- **Open question:** does the API say where the panorama it picked was taken? Needed to place
  photo textures on the 3D buildings (step 3). To check: `debug=true` and the response headers.

## The source: Mapy.com's own 3D (decided 1 Nov 2026)

The website's and the Android app's 3D view comes from Melown's VTS server at
`https://mapserver-3d.mapy.cz/scenes/latest/mapConfig.json` (VTS map configuration, reference frame
`melown2015`). No API key, referer or cookie is needed. Making 3D from the 2D tiles is dropped; the
OSM buildings stay only as a fallback where there is no mesh.

- **Surfaces** (each a tile set, `{lod}-{x}-{y}`): `cities` (photo meshes of the cities, lod 15-22),
  `landmarks` (17-24), `ev1`, `ev2@3`, `terrain-czech` (10-21), `viewfinder`, `viewfinder1` (world
  terrain). City files: `https://mapserver-3d.mapy.cz/latestStage/tilesets/cities/{lod}-{x}-{y}.bin?0`
  (mesh), `...-{sub}.jpg?0` (texture of submesh `sub`, 512 x 512, ~90-100 KB), `.meta?0` (metatile,
  only at x and y that are multiples of 32). 404 = no mesh there.
- **Tile numbers:** below lod 1 the tiles are web Mercator: VTS lod L = XYZ zoom L - 1, same x and y.
- **Mesh (`.bin`):** gzip; `"ME"`, u16 version 3, f64 mean undulation, u16 submeshes. Each: u8 flags
  (1 internal UVs, 2 external UVs), u8 surface reference, u16 texture layer, 6 f64 bounding box
  (geocentric metres), u16 vertices, u16 quantisation, zigzag deltas of x y z (1-2 bytes each,
  `value / quant * bbox size + bbox centre`); UVs likewise; u16 faces, indices as "high-water mark"
  words (`high - w`, `w == 0` is a new vertex); then the UV faces the same way. Decoder:
  `tools/vts/vtsmesh.py` (checked against vts-browser-js `submesh.js`; textured preview right).
- **Size:** ~2,000 triangles and 20-24 KB per tile at every lod (each lod is a quarter of the area):
  lod 17 (~400 m) holds a whole quarter of Prague coarsely, lod 19-20 (~100 / 50 m) single buildings
  in detail. The level of detail is the "downsizing": near tiles fine, far ones coarse, within a
  triangle budget set for the phone; vertex clustering (merging vertices closer than ~2-8 m) for
  more on top (tested: 2 m 10 %, 4 m 25 % fewer triangles at lod 19).
- **On the phone:** gzip via ezlib's raw inflate (after skipping the gzip header), positions to local
  east/north/up metres once per tile, the texture through our JPEG decoder at 256 x 256 (128 KB in RAM).

## Steps

1. **3D city:** ground tiles, buildings, walking (arrows/joystick with the map's glide and
   acceleration, Q/E to turn, eye height 1.7 m), the route; GPS/navigation mode follows the position
   and direction. Needs the engine and the building data on the phone.
2. **Panorama doorway:** at a chosen spot the scene fades into the panorama cube; look around (and
   zoom with a narrower request); step out again. Without the engine it can also be shown as a
   cylindrical strip that scrolls sideways.
3. **Photo-textured walls:** project the panorama onto the nearby OSM walls, so close up the buildings
   show their real facades and moving a few metres keeps the right depth. Only if the panorama's
   position can be found (open question above).

## Needed before step 1

- **Data hosting:** the `maps9300-data` repository on GitHub with Pages (README, *Setup*).
- **The app reading the static data files** (0.39): places, details and buildings per cell, on demand,
  cached on the memory card within a size budget (default 20 MB), Overpass only as the fallback.
- **The 3D engine** (from the Skyrim 9300 chat):
  1. Fixed-point rasterizer in C++? How does the frame reach the screen?
  2. Perspective-correct or affine texture mapping? Texture sizes and formats (RGB565, 256 or 512 px)?
  3. Frame rate when a few large textured triangles cover the whole 640x200 screen (fill rate)?
  4. Flat-shaded untextured triangles: how much faster than textured?
  5. Z-buffer? RAM needed beyond the textures? Fog or distance culling?
  6. Can textures (ground tiles) be swapped at run time?
  7. Can it be built as a module inside another app, or is it tied to its own app class?
