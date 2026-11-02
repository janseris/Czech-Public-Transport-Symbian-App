# Mapy.com: what a 9300 app can use (found for Maps 9300)

Checked from the phone with our own User-Agent (`Maps9300/x (Symbian map app; Nokia 9300; ...)`).
Never impersonate Mapy.com's apps, and never hardcode the website's key. Endpoints marked
*unofficial* are what Mapy.com's own app or website calls: they can change without notice.
The code that reads each one is in `lib/mapy/` (snapshots from maps9300, where the live copies are).

## Official REST API (needs your own key: `?apikey=`)

| What | URL | Notes |
|---|---|---|
| Map tiles | `api.mapy.com/v1/maptiles/{basic,outdoor,aerial}/256/{z}/{x}/{y}` | PNG / JPEG, 256 px; basic/outdoor to zoom 19, aerial 20 |
| Search | `api.mapy.com/v1/suggest?lang&limit&query&preferNear=lon,lat` | JSON; 400/422 with preferNear: try without |
| Route | `api.mapy.com/v1/routing/route?...` | JSON; `lib/mapy/route.cpp` reads it (geometry, steps) |
| Panorama | `api.mapy.cz/v1/panorama/getbest?lon&lat&radius`, `getneighbours?pid`, `tiles/<prefix>/<uriPath>/<fileMask>?sdk=` | zoom 0: the whole panorama, 1024 x 512 equirectangular JPEG, middle column north. `sdk`: "mapyapi,Armstrong,<the day's start, Unix time>" XOR "sdk", base64, as the website sends it. Terms: RAM only, no disk cache |

## 3D city (unofficial; Melown VTS, plain files)

- `mapserver-3d.mapy.cz/latestStage/tilesets/cities/<lod>-<x>-<y>.bin?0`: a mesh (gzip, VTS format
  version 3); `...-0.jpg?0`: its 512 x 512 texture; `...<lod>-<x&~31>-<y&~31>.meta?0`: a metatile
  (which tiles of a 32 x 32 block have geometry). https only (port 80 doesn't answer).
- VTS lod L is web Mercator zoom L - 1; lod 17 ~400 m, 18 ~200 m, 19 ~100 m; ~2,000 triangles a tile.
- The format and how it's decoded: `docs/3d-view.md`, `lib/mapy/vts3d.cpp`; Python readers: `tools/vts/`.
- Where 3D exists (the website's red overlay): the metatiles at a level whose tiles are ~32 px on the
  map; Czech coverage box at lod 21 (32-tile blocks): x 8745..9047, y 5486..5646.

## Places, details, reviews, photos (unofficial: the Mapy.com app's)

- `GET vectmap.mapy.cz/poi/online/v6/16-<x>-<y>.br4?al=<lang>&sl=<lang>&zh=<zoom>&zl=<zoom>`: a zoom 16
  tile's places, "barrel" binary:
  - 8-byte header (version, length), `01 <len> <header message>`, then the places;
  - a place: `<3-byte position> <n> n x (01 <value>)`; position v = b0 | b1<<8 | b2<<16,
    x = (v & 0x7ff) * 2, y = (1024 - (v >> 12)) * 8, in 1/4096 of the tile (within ~1.5 m);
  - values: a message `L 22 L-2 <utf-8>` (name, address), a raw string, or a varint (category, the id);
  - the id: source in the low 4 bits: 0 firm (srcId = id >> 4), 1 base (>> 4), 12 with
    ((id >> 4) & 31) == 17 traf (>> 9), 13 pubt (stops, >> 4). Zones and lines: longer geometry, skipped.
- `POST vectmap.mapy.cz/rpc`, Content-Type `text/xml`, `Accept: application/x-frpc`: XML-RPC
  `getDetail` {source, srcId, lang, photo sizes...}; the answer in FastRPC 2.1 binary, ~1/3 of the XML.
  FastRPC type codes (high 5 bits of the byte): 1 / 7 / 8 int, 2 bool, 3 double, 4 string, 5 date,
  6 binary, 10 struct, 11 array, 12 null, 14 response, 15 fault. Parser: `lib/mapy/mapypoi.cpp`
  (FrpcParse, integer-only double decode).
- `GET vectmap.mapy.cz/reviews/v1/review/poi/<source>/<srcId>?content=full&lang=&limit=5&offset=0`: JSON.
- Photos: `d48-a.sdn.cz/...?fl=res,,200,3`: a baseline JPEG ~300 x 200 (the detail gives the URL).
- `getDetail` by mark only answers "Pozice na mapě": useless.

## How these were found

Capture the app or the website (HAR from the browser's developer tools; the Android app through a
proxy), then replay each request with curl from the PC with our own User-Agent before writing any
phone code. Binary bodies in a HAR are UTF-8-mangled: re-encode them as Latin-1 to get the bytes.
