# Download benchmarks

Nokia 9300 (150 MHz, Symbian 7.0s), Maps 9300 0.33 and 0.34, 19 Oct 2026. Two connections to
the PC's internet:

- **USB cable:** IP passthrough (RNDIS, PC Suite): access point "ip passthrough", LANService over
  LANBearer, interface rndisnif.
- **Bluetooth 1.1:** "GnuBox BT", DialOutISP over ModemBearer, port BTCOMM::0, interface PPP.

Both runs used the same settings, from the logs' `settings` lines: 3 parallel downloads, TCP receive
window 64 KB, download priority same as screen, tiles in RAM only, load tiles around the screen on,
keep 8 MB free, landscape. The logs are in `android-to-j2me-kit/nokia9300/ota/uploads/`.

**How to read the numbers**

- **Tile KB/s:** the bytes of every tile downloaded in a period, divided by the period's length. A
  tile's bytes are its PNG/JPEG file as received (compressed), without HTTP headers or TLS. Every
  download has its own log line.
- **Best 10 s:** counts the tiles *finished* in the window. A tile takes 0.4-3 s, so it may have
  started before the window, which makes this figure too high. The averages over whole periods are
  right.
- **Threads busy:** the time all downloads took, divided by the period's length, out of the 3
  download threads.
- **Per connection:** the median of each tile's bytes divided by its download time.

## USB cable (0.33, 15:11-15:20)

Log `2026-10-19T13-21-12-716Z_mapynative.txt` (1234 lines). **Whole run:** 1133 tiles, 24.6 MB in
526 s = **47.8 KB/s**.

| Map type | Protocol, format | Tile KB/s | Best 10 s | Tiles/s | Threads busy (of 3) | Time per tile (median / p90) | Per connection | KB per tile | Decode |
|---|---|---|---|---|---|---|---|---|---|
| OpenTopoMap (2nd visit) | http, PNG | **75.2** | 100 | 2.32 | 2.81 | 976 / 1781 ms | 32 kB/s | 32.5 | 46 ms |
| Mapy.com outdoor | https, PNG | 58.4 | 78 | 1.66 | 1.91 | 1031 / 1671 ms | 34 kB/s | 35.2 | 46 ms |
| ČÚZK base map (2nd visit) | http, JPEG | 55.9 | 66 | 1.78 | 2.92 | 1218 / 2921 ms | 22 kB/s | 31.4 | 203 ms |
| OpenTopoMap (1st visit) | http, PNG | 54.3 | 81 | 2.16 | 2.71 | 984 / 2437 ms | 27 kB/s | 25.2 | 31 ms |
| ČÚZK aerial | http, JPEG | 52.9 | 61 | 1.87 | 2.96 | 1421 / 2937 ms | 20 kB/s | 28.4 | 203 ms |
| ČÚZK base map (1st visit) | http, JPEG | 51.3 | 66 | 1.40 | 2.80 | 1328 / 2921 ms | 27 kB/s | 36.6 | 218 ms |
| Mapy.com standard | https, PNG | 49.2 | 75 | 1.95 | 1.76 | 820 / 1421 ms | 31 kB/s | 25.2 | 46 ms |
| OpenStreetMap (2nd visit) | https, PNG | 35.4 | 57 | 2.53 | 2.28 | 609 / 1015 ms | 23 kB/s | 14.0 | 31 ms |
| Mapy.com aerial | https, JPEG | 32.9 | 54 | 2.06 | 1.66 | 695 / 1406 ms | 23 kB/s | 15.9 | 109 ms |
| OpenStreetMap (1st visit) | https, PNG | 31.6 | 54 | 2.95 | 2.53 | 406 / 921 ms | 26 kB/s | 10.7 | 31 ms |

1. **The link is the limit: ~50-75 KB/s in all.** Every map type ends up at ~20-34 kB/s per
   connection, ~50-75 KB/s together, whatever its server. The three connections share the link.
   More parallel downloads or a bigger TCP window can't add much.
2. **Maps with small tiles look slower in KB/s, not in tiles.** OpenStreetMap (10-14 KB) and Mapy.com
   aerial (16 KB) still do 2-3 tiles a second, but each request also costs a round trip. For the
   screen to fill, tiles per second is what matters, and it was 1.4-2.95 for every map type.
3. **Plain http (OpenTopoMap, ČÚZK) is a little faster than https.** The 150 MHz processor does no
   TLS for them, and a new connection takes ~50 ms with no handshake.
4. **ČÚZK JPEGs take ~200 ms each to decode** (Mapy.com aerial ~110 ms; PNGs 31-46 ms). At ~1.8 tiles/s
   that's ~40% of the processor, so panning is heavier on the ČÚZK maps.
   - Why: ČÚZK saves its JPEGs with no chroma subsampling (4:4:4) at high quality (~90). That is 3
     blocks of 8x8 per 8x8 pixels instead of 1.5 (4:2:0, as Mapy.com aerial), with many more non-zero
     coefficients: 28-37 KB per tile against 16 KB. About twice the work.
   - Possible fix: inverse-transform the chroma at half resolution, which can't be seen on the 16-bit
     screen. Estimated ~25-35% faster.
5. **Mapy.com standard and aerial threads were busy only 1.7 of 3.** Either the panning didn't need more
   tiles, or the next ones were asked for too late. Not decided yet.
6. **Stalls:**
   - 3 TLS handshakes timed out after 40-88 s, one each on Mapy.com standard, outdoor and aerial. Each
     held one of the 3 downloads that long.
   - One handshake took 10.9 s. A new connection is opened after a map type switch, so the first tiles
     of a map can wait.
7. **OpenTopoMap at zoom 17:** 19 HTTP 404 answers. This area has no z17 tiles.

## Bluetooth (0.34, 16:23:39-16:30:37)

Log `2026-10-19T14-48-56-898Z_mapynative.txt`. Bluetooth worked for 7 minutes, then dropped.

| Map type | Bluetooth: tile KB/s | USB: tile KB/s | Bluetooth per connection | USB per connection |
|---|---|---|---|---|
| ČÚZK base map (JPEG) | 30.8 | 51.3-55.9 | 11 kB/s | 22-27 kB/s |
| ČÚZK aerial (JPEG) | 30.5 | 52.9 | 11 kB/s | 20 kB/s |
| Mapy.com standard | 22.0 (2.0 tiles/s, 11 KB each) | 49.2 (1.95 tiles/s) | 12 kB/s | 31 kB/s |
| Mapy.com outdoor | 31.4 | 58.4 | 12 kB/s | 34 kB/s |
| Mapy.com aerial (JPEG) | 24.3 | 32.9 | 16 kB/s | 23 kB/s |
| **All** | **27.4** (558 tiles in 420 s) | **47.8** (1133 tiles in 526 s) | | |

- **The Bluetooth link carries ~35.5 KB/s, steadily.** The connection's own byte counter shows
  34.6-35.7 KB/s received in every 30 s period while tiles loaded, and 0.75-0.9 KB/s sent. That is
  ~284 kbit/s, ~39% of Bluetooth 1.1's 723 kbit/s one-way maximum, and the same for every map type.
  The USB cable gave ~1.7x as much.
  - 35.5 KB/s with PPP overhead fits a 460,800 bit/s serial port. 0.38 logs the port's rate to check.
- **Overhead:** tile bytes were ~31-35 KB/s of the connection's ~35.5 KB/s. So headers, TLS and TCP cost
  ~5-10%, and the app's tile speed is a fair measure.
- **New https connections were slow:** TLS handshakes took 1.7-11 s (USB: 0.25-0.6 s).

### Processor time lost to Bluetooth

The same work, timed in both runs. Decoding a tile is fixed work in the app's own thread, so its time
shows how much of the processor was left to the app.

| Work | USB cable | Bluetooth | Processor left to the app |
|---|---|---|---|
| ČÚZK base JPEG, median | 218 ms (35.8 KB) | 437 ms (39.5 KB) | 54% (6.0 → 11.2 ms per KB) |
| ČÚZK aerial JPEG, median | 203 ms (26.2 KB) | 398 ms (25.0 KB) | 51% (7.8 → 15.3 ms per KB) |
| Mapy.com aerial JPEG, median | 109 ms (16.6 KB) | 218 ms (18.5 KB) | 55% (6.6 → 12.1 ms per KB) |
| Mapy.com outdoor PNG, median | 46 ms (36.5 KB) | 93 ms (31.4 KB) | 51% (1.4 → 2.9 ms per KB) |
| Drawing a frame, average | 45.5 ms | 78.5 ms | 58% |

Mapy.com standard is left out: its Bluetooth tiles were much smaller (6 KB against 26 KB), so they
don't compare.

**While tiles download over Bluetooth, the app gets only ~50-58% of the processor it gets over the
cable.** The other ~42-49% goes to the phone's Bluetooth, serial (BTCOMM) and PPP stack.

- Panning, decoding and drawing run about half as fast.
- Bluetooth also delivers only ~35 KB/s against the cable's 50-75, so each KB downloaded costs the
  processor roughly 3x as much.
- The cable's passthrough isn't free either. These figures are Bluetooth's extra cost on top of it.

## Open ideas

- **A tile proxy on the PC:** plain http to the phone (no TLS on the phone), and smaller re-encoded
  tiles. Biggest gain over Bluetooth.
- **ČÚZK:** chroma at half resolution in the JPEG decoder.
- **OpenTopoMap:** stop at zoom 16 and enlarge its tiles beyond that.
- **More runs:** Bluetooth with the 0.38 settings (no ring, disk cache, 2 downloads), and with the
  serial port's rate logged.
