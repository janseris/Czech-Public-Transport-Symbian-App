# A web browser for the Nokia 9300: what a new chat needs to know

Handoff from the chats that built Mapy 9300 native, Instagram 9300, MP4 Player, Net Helper and the TLS 1.2
patch (October 2026). Everything below was measured or built on the user's own Nokia 9300. All
folders are under `C:\Users\Jan\phoneapk\` (the connected folder `phoneapk`).

## 1. The phone

| | |
|---|---|
| Model | Nokia 9300 Communicator, **Series 80 2.0, Symbian 7.0s, EKA1** (not S60; Symbian 9 / EKA2 tricks don't apply) |
| CPU | TI OMAP 1510, one ARM9 at **150 MHz**, no FPU, 16 KB I / 8 KB D cache |
| RAM | 47 MB reported to Symbian, **~20-26 MB free** for apps |
| Screen | **640×200**, 16-bit (EColor64K, RGB565). Apps can draw full screen, or leave 140 px on the right for the 4 side (CBA) buttons |
| Input | Full QWERTY keyboard, arrow keys, a joystick (its own scan codes, see Mapy 0.35), 4 side buttons. **No touch.** The window server delivers pointer events, used for a virtual cursor (Instagram 9300) |
| Storage | C: ~90 MB flash (**writes are slow**: 0.3-2 s per file save), D: MMC card |
| Network | No Wi-Fi. The internet comes from the PC: **USB IP passthrough (RNDIS, PC Suite)**, ~50-75 KB/s; or **Bluetooth 1.1 dial-up (GnuBox, PPP over BTCOMM)**, ~35 KB/s, and it takes ~half the CPU while data flows |
| TLS | Only through `CSecureSocket` → our patched **`C:\System\Libs\ssladaptor.dll`** (TLS 1.2, BearSSL, SNI, session resumption; **the certificate chain is not verified**) |

## 2. Code to reuse (read these first)

| Where | What to take |
|---|---|
| `instagram9300/` (native C++, 0.57) | The closest relative of a browser. **`src/net.cpp` `CHttpClient`**: `RConnection` attached to the open connection, TCP, TLS (`CSecureSocket`), HTTP/1.1 keep-alive, **chunked + gzip** (zlib raw inflate), redirects, a "file sink" (body straight to a file). **`core/`**: plain C89 HTTP/JSON/UTF-8 code, **unit-tested on the PC under ASan** (`test/run.sh`). Also `images.cpp` (download, disk cache, decode, RAM-aware bitmap cache), `textsel.cpp` (text selection and clipboard), `edgescroll.cpp`, the **virtual cursor** (pointer events) in `view.cpp`, `settings.cpp` (settings.txt), and `helper.cpp`: in-app update, log and screenshot upload (BMP writer). Its README has every lesson, version by version |
| `instagram9300/FOR-OTHER-CHATS-auto-update.md` | How an app updates itself from the PC: download the .sis, open it with `RApaLsSession::StartDocument`, `Exit()`. **Copy this early**: it makes every test build one menu click |
| `mapy9300-native/` (native C++, 0.38) | **`src/net.cpp`**: 3 download threads with kept-open connections, blocking-style waits (`inc/wait.h`, nested active scheduler + timeouts + clean stop), **explicit `RConnection`**: uses the connection that's up, else starts one (Settings: an access point without asking, or the phone's default), DNS 10 s and connect/TLS timeouts, TCP window `SetOpt` before `Connect`. **`src/pngdec.cpp`, `src/jpegdec.cpp`**: own decoders into RGB565, much faster than the phone's ICL (PNG tile ~40 ms, JPEG ~100 ms; ICL 170-400 ms with 3 s outliers); tested on the PC (`test/`). `src/json.cpp`. In `src/mapy.cpp`: the **log** (RAM + file `C:\Data\Mapy9300\mapy_log.txt`, the previous run kept, *send log* POSTs both to the PC), **the network connection monitor** (access point, bearer, BTCOMM vs RNDIS, byte counts), held-key gliding with acceleration and joystick learning, a settings screen (list + side legend + word wrap + Tab to scroll the help), the FPS counter, portrait mode (draw to a 200×640 bitmap and rotate) |
| `mapy9300-native/docs/benchmarks.md` | Measured download speeds (USB vs Bluetooth, per server, http vs https) and the CPU lost to Bluetooth |
| `nethelper9300/` | A native localhost HTTP proxy for Java apps (Java can't keep connections open or open ports 80/443) |
| `mp4player9300/` | Native video/audio playback; `-O2` gave ~19% faster code than the SDK default |
| `symbian-build/symbian-tls/` (branch `eka1-java-fixes`), `symbian-build/out/ssladaptor_v20-fix11*.dll` | The TLS patch. **fix11** is current: session store lock and host:port key, a placeholder `ServerCert()`, handshake reset on cancel. Install as `C:\System\Libs\ssladaptor.dll`, restart the phone. `_log` build logs per connection to `C:\Logs\SSL\SSLLog.txt` |
| `android-to-j2me-kit/nokia9300/NOTES.md`, `NOKIA_9300.md` | Platform notes (mostly Java, but the hardware, network and TLS facts hold) |
| `localserver/ota_server.js` (and `android-to-j2me-kit/nokia9300/ota/`) | The PC server on `http://192.168.137.1:8000/`: serves files (.sis with the right MIME type), `POST /results?name=x` saves a log to `uploads/`, `/upload` is a form. Start: `node ota_server.js` in that folder |

## 3. Building

- **Toolchain (Linux, or WSL):** `symbian-build/toolchain/setup_eka1_toolchain.sh <symbian-build dir> ~/sym`,
  then `setup_s80_sdk.sh S80_DP_2_0_SDK.zip ~/sym` (the SDK zip is in `phoneapk/`). GCC 2.9 (gcc-539),
  GnuPoc. Details: `BUILD_SYMBIAN_TLS.md`.
- **Build script pattern** (`mapy9300-native/build.sh`): `EPOCROOT=$SYM/s80_20/`, `bldmake bldfiles`,
  `abld build armi urel`, then `makesis` on a `.pkg` with the Series 80 product ID `(0x101F8ED2)`.
- **App UID:** pick an unused one in the `0x0F5A93xx` range. Used so far: 9300 Net Helper,
  9301 MP4 Player, 9302 Mapy, 9325 Instagram; grep the other projects' `.mmp` files first.
- `.mmp` libraries a browser will need (from Mapy and Instagram): `euser apparc cone eikcore
  eikcoctl bafl egul ckndlg ckncore esock insock securesocket commdb efsrv gdi fbscli bitgdi ws32
  ezlib charconv imageconversion`; add `OPTION GCC -O2`.
- **Delivery that worked:** build in the cloud, copy the .sis to the PC (OTA folder), the app's
  "update from the PC" fetches it, then commit to git on the PC.

## 4. Compiler and platform traps (each cost a crash or a build)

- **`&_L("...")` taken as a pointer is a dangling temporary**: use `_LIT` constants. `TRAP_IGNORE`
  doesn't exist on 7.0s; use `TRAPD(err, ...); (void) err;`.
- An EKA1 `.app` is a DLL: **no writable static data**, only `const` tables.
- GCC 2.9: forward-declare static functions before use; avoid name clashes with SDK types (a local
  `TProfile` collided with the SDK's).
- **BITGDI 3** = drawing text with no font selected (`UseFont` before every `DrawText` path).
- **KERN-EXEC 3** from touching UI objects outside the UI thread: hand results over through a queue.
- **Timers run late while the screen is busy** (a 100 ms `CPeriodic` ran ~3x slow): measure time
  with `TTime`, and do follow-up work in a `CIdle`, not only in the tick.
- **Disk writes are slow** (0.3-2 s each): never in the drawing or download path; write logs in
  batches (every 10 s).
- `CommDb` columns are read through an attached `RConnection` (`GetIntSetting(_L("IAP\\Id"))`,
  `"ModemBearer\\Rate"`...). `TCommDbConnPref` + `ECommDbDialogPrefDoNotPrompt` starts an access
  point without the dialog.
- Fonts: `iEikonEnv->DenseFont()` (small, readable), `NormalFont()`. Word wrap with
  `CFont::TextCount` + break at the last space (Mapy's `WrapLines`).
- Text from the web is UTF-8 (or windows-1250 / ISO-8859-2 for old Czech pages):
  `CnvUtfConverter` (charconv.lib), and `CCnvCharacterSetConverter` for the 8-bit charsets.

## 5. Network: what we measured and what works best

- **Use the connection that's already up**: one `RConnection` per thread, `Attach` to the active
  one (`EnumerateConnections` / `GetConnectionInfo`), open sockets and `RHostResolver` on it. Without
  this, each socket starts the default access point and fights the user's USB/Bluetooth switching.
- **Keep connections open** (HTTP/1.1 keep-alive, close after ~50 s idle). A new https connection
  costs a handshake: 0.25-0.6 s over USB, 1.7-11 s over Bluetooth. TLS sessions resume across
  connections and apps (fix11 keeps them in `C:\System\Data\ssl_sessions.dat`).
- **2-3 parallel connections** fill the link; more don't help. The link is the limit (USB ~50-75
  KB/s, Bluetooth ~35 KB/s), not the servers.
- **TCP receive window: `SetOpt(KSoTcpRecvWinSize, KSolInetTcp, 32768..65536)` before `Connect`**
  (the default 8 KB caps a Bluetooth connection at ~26 kB/s).
- **SNI:** `CSecureSocket::SetOpt(KSoSSLDomainName, KSolInetSSL, host)` before the handshake;
  `SetDialogMode(EDialogModeUnattended)`.
- **Time limits:** DNS 10 s, connect 15 s, TLS 20 s, cancelling the socket under the TLS too.
- Send `Accept-Encoding: gzip` (zlib is on the phone; Instagram's `DecodeBodyL` inflates).
- Plain http is a bit faster than https (no TLS work on the CPU).
- Log per request: bytes, total ms, DNS/connect/TLS ms, reused or new. It made every
  speed problem findable.

## 6. Browser design notes (suggestions, not decided)

- **Realistic scope:** HTML parsing, simple CSS (display block/inline, a few font sizes, bold,
  colours, margins), text layout with word wrap, links, images, forms (text fields, buttons, GET and
  POST), history, bookmarks, cookies. **No JavaScript** in a first version: the CPU and RAM don't
  allow a modern engine.
- **Two architectures to weigh:**
  1. **On the phone:** fetch, tokenizer (tolerant of bad HTML), DOM-light tree, layout into lines
     and boxes, paint into an off-screen bitmap, scroll. Full control, works without the PC, but
     heavy modern pages (MBs of HTML/JS/CSS, WebP/AVIF images) will be slow or unusable.
  2. **A PC-side helper, as Opera Mini did:** the PC (always present as the gateway) fetches the
     page with a real engine, removes scripts and simplifies (or pre-renders), re-encodes images to
     JPEG/PNG sized for 640 px, and sends a compact format over plain http. No TLS on the phone,
     far fewer bytes; works for modern sites. `localserver/ota_server.js` already converts video with
     ffmpeg for MP4 Player, the same pattern.
  A mix is likely best: phone-side rendering of simple pages, the PC helper for heavy ones.
- Images: PNG and JPEG with Mapy's decoders; GIF through the phone's ICL (`CImageDecoder`); WebP/AVIF
  only through the PC helper. Decode in an idle callback, scale to the layout width, keep a
  RAM-aware cache (Instagram's `images.cpp`).
- Navigation without touch: a virtual cursor (Instagram), plus Tab/arrow jumping between links
  and form fields, as the phone's own browser does. Held arrows should glide and speed up (Mapy).
- Text input: an `CEikEdwin` in a dialog, or an own one-line editor (Mapy's search box).
- Memory: keep the DOM compact (offsets into the source text), drop images first when RAM gets low
  (`UserHal::MemoryInfo`, Mapy keeps 8 MB free).

## 7. Working with the user

- Versions: bump the app version in the `.pkg` and the UA on every delivered build, and add a short
  README status entry per version (Mapy and Instagram do).
- Git on the PC: commits as `Jan Seriš <jan.seris@seznam.cz>` with the session's
  `Co-Authored-By` / `Claude-Session` trailers. Deleting is off in the connected folder, so git leaves
  `.lock` files: move them aside (`.git/stale-...`) before and after each git command.
- Never commit API keys (`*.key`) or third-party artwork (e.g. Seznam's place icons).
- The user tests on the phone and sends logs to the PC (`ota/uploads/`, `localserver/uploads/`);
  analyse them and keep measured results in the repo.
