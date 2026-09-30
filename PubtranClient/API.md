# pubtran-backend.mapy.cz – reverse-engineered API

This is the API used by the **Jízdní řády** app (`cz.fhejl.pubtran` 5.24.3). It was worked out from a mitmproxy capture (`pubtran.har` / `pubtran.flow`) and from the decompiled APK.

## Transport

| | |
|---|---|
| Base URL | `https://pubtran-backend.mapy.cz/api/v1/` (the app also knows a dev backend: `http://jizdnirady-pubtran-backend.mapy-master.ops.dszn.cz/api/v1/`) |
| Method | `POST {base}{method}` |
| Headers | `Content-Type: application/x-frpc-rest`, `Accept: application/x-frpc-rest`, `User-Agent: okhttp/5.4.0` |
| Auth | **none**: no API key, token or signature. The server sets an `sznlbr` load-balancer cookie, which is optional. |
| Body | **Seznam FastRPC** binary, protocol 2.1: magic `CA 11 02 01` followed by one value (the request is a struct of parameters, the response is a struct). |

### FastRPC 2.1 encoding

Each value starts with a type byte: `type << 3 | info`.

| type | meaning | payload |
|---|---|---|
| 1 | INT (legacy) | `info+1` bytes, little-endian, signed |
| 2 | BOOL | bit 0 of info |
| 3 | DOUBLE | 8 bytes, IEEE, little-endian |
| 4 | STRING | length (`info+1` bytes, little-endian), then UTF-8 |
| 5 | DATETIME | zone (1 byte, quarter-hours, e.g. +02:00 = 8), unix time (4 bytes), 5 packed bytes (bits: weekday 3, sec 6, min 6, hour 5, day 5, month 4, year−1600 11) |
| 6 | BINARY | length, then bytes |
| 7 / 8 | positive / negative INT | magnitude, `info+1` bytes |
| 10 | STRUCT | member count, then per member: 1-byte key length, key, value |
| 11 | ARRAY | item count, then values |
| 12 | NULL | |
| 15 | FAULT | code, message |

`Frpc/FrpcCodec.cs` implements this. It re-encodes all 34 captured requests **byte-for-byte**.

---

## Endpoints

### `suggest`: place autocomplete (Odkud / Kam)
```
{ query:"olomouc", count:10, lon:17.25, category:"municipality|street|address|poi|area|firm|pubt", lang:"cs", lat:49.59 }
```
`lat`/`lon` are optional; the app sends the phone's position to rank the results.

Response: `{ result:[ { category, userData:{ suggestFirstRow, suggestSecondRow, source, id, latitude, longitude, municipality, district, region, bbox, … } } ] }`
- `source`: `muni` = town/city, `pubt` = stop or station, plus others (street, address, POI…).
- The "recent places" shown before typing are stored locally by the app, not fetched from the server.

### `getroutesopt`: connection search
```
{
  isDeparture: true,                         // false = "arrive by"
  flags: { bus,tram,trolley,metro,cable,ferry,train: 1|0, lowfloor, bike, stroller: 0|1,
           unixt:1, mobile:1, version:3, geometry:0, hashes_used:[int…], tcount:0 (only direct) },
  searchOpts: { start:{x:lon,y:lat,source,id}, end:{…}, mid:[{…}] (via, optional),
                count:5, index:0, lang:["cs"] },
  when: "2026-09-30T20:56:00"                 // local wall-clock time, no offset
}
```
**Paging** (scrolling down and up in the list):
- Keep the same `when` for every page.
- `index = 0` is the first page, `+5` loads later connections, `−5` loads earlier ones.
- `hashes_used` = the `hash` of every connection already shown, so the server skips them.

Response:
```
{ status:200, stops:[{id,name,coord:[lon,lat],parent_id}], agenciesinfo:[{name,short_name,url}], origins:[],
  routes:[ { hash, departureISO, arrivalISO, time(sec), transferCount, startStopIndex, endStopIndex,
             payment:[{type:"price|partialprice", price, currency_type, eshopName, url, stopFrom, stopTo}],
             items:[ … ] } ] }
```
The `items` in a route have one of three types:
- `type 0` = ride. It has `partDescription`, `routeName`, `routeLongName`, `vehicleType`, `agencyIndex`, `startStopTripIndex`, `endStopTripIndex`, `startPlatform`, `endPlatform`, `info[]`, and `tripData[]`.
  - `tripData[]` covers the **whole** trip. Each entry is `{stopIndex → stops[], arrivalISO, departureISO, in, stCodes[]}`.
- `type 1` = walk: `distance`, `oznStartCoord`, `oznEndCoord`.
- `type 2` = transfer wait.

**`partDescription`** identifies a ride and is used as the `tripId` in the other calls. Its format:
`line;;startStopId;;depISO;;endStopId;;arrISO;;startTripIndex;;endTripIndex`

**`vehicleType`**: 1 bus, 2 tram, 3 cable car, 4 metro (line A/B/C = `routeName`), 5 boat, 6 trolleybus, 7 train.

### `getnextdepartures`: other runs of the same line (the "swipe")
```
{ params: [ [ partDescription, { reqindex:-1|1|2|3…, startindex, endindex, time:"<leg departure, local>",
                                 stops:1, tripdata:1, geometry:0, unixt:1, lang:["cs"], bus…train:1 } ], … ] }
```
- `reqindex` is always **relative to the original ride**:
  - −1 = previous run, 1 = next run, 2 = the run after that, and so on.
- After showing results, the app sends `-1` and `+1` for every ride so swipes feel instant.

Response: `{ results:[ {status, trip:{ partDescription, stops[] (with times), tripData[], agenciesinfo[], info[], … }} ] }`, in the same order as the requests.

### `gettripinfos`: realtime data (delays, disruptions)
```
{ tripIds: [ { tripId: partDescription, time: <FastRPC DATETIME of the ride's departure> } ] }
```
Response: `{ results:[ { status, info:[…] } ] }`

### `gettripdetail`: stops and geometry of a ride (used for the map)
```
{ tripId: partDescription, params: {} }
```
Response: `{ trip:{ direction, geom:"<Seznam-encoded polyline>", stops:[{id,name,coord,distance,geomIndex,typeId}] } }`

The stops here have no times; times come from `tripData`.

### `getwalkgeom`: walking path
```
{ start:{x,y}, end:{x,y} }
```
Response: `{ geom:{ distance, geom, attributes } }`

---

## `info[]` ids (per ride)

Names come from the app's constants (`JourneyPart.INFO_*`).

| id | meaning |
|---|---|
| 14 | mandatory reservation (text); also treated as a warning |
| 15 | wheelchair accessible |
| 20 | bicycles carried |
| 42 | departure platform or stand (text) |
| 43 | booking needed |
| 45 | Wi-Fi |
| 46 | air conditioning |
| 101–108, 201–205 | disruptions and warnings: `text` + optional `url` |
| 123, 124 | wagon info |
| 300 | realtime "no delay": `delay` in seconds, can be negative |
| 302 | realtime delay: `delay` in seconds + text |
| 303, 305 | vehicle position `lat`/`lon` |
| 400 | platform at `stopID`: text "arrival / departure" |
| 500 | usual (statistical) delay at `stopID`: `delay` in seconds; text "p: probability, d: delay" |

## `stCodes[]` ids (per stop)

| id | meaning |
|---|---|
| 7 | tariff zone |
| 24 | accessible stop |
| 28 | request stop (na znamení) |
| 37 | alighting only |
| 38 | boarding only |
| 42 | platform |
| 44 | booking needed |
| 443 | Prague metro entrance/exit hint |

## Captured user flow → calls

| App action | Calls |
|---|---|
| Typing "olomo…" | `suggest` ×3 (debounced) |
| Hledat | `getroutesopt` index 0, then `getnextdepartures` (±1 for every ride) + `gettripinfos` |
| Scrolling down / up | `getroutesopt` index +5 / −5 with `hashes_used` |
| Opening a ride | `gettripdetail` (+ `getwalkgeom` for walks) + `getnextdepartures` ±1 |
| Swiping to the next run | `getnextdepartures` reqindex 2, 3, … followed by `gettripdetail` of the new run |
