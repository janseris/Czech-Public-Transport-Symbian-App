# Reverse-engineering the "Jízdní řády" app (cz.fhejl.pubtran) → own C# client

This folder contains everything used to capture and decode the API of the Android app **Jízdní řády** (`cz.fhejl.pubtran` 5.24.3). From that we built our own WinForms client in `PubtranClient/`.

## What's in this folder

| File / folder | What it is |
|---|---|
| `base.apk`, `split_config.*.apk` | The original app, pulled from the phone. It is split into a base file plus three config parts. |
| `app.apks` | The four APK files above zipped together, as input for apk-mitm. |
| `apktool.jar` | Newer apktool. The one bundled with apk-mitm was too old for this app. |
| `app-patched.zip`, `patched/` | The patched app, which trusts user-installed certificates, and its extracted APK files. |
| `logs/` | apk-mitm logs. |
| `pubtran.flow` | mitmproxy capture in its raw binary format. **Use this one for analysis.** |
| `pubtran.har` | The same capture as HAR. The binary request and response bodies are mangled in it, so it's only good for an overview. |
| `PubtranClient/` | Our client: a Visual Studio solution with an API library and a WinForms UI. `PubtranClient/API.md` is the full API reference. |


## Nokia 9300 (J2ME) port

The J2ME app for the Nokia 9300 (Series 80 v2, Symbian 7.0s) and the TLS patch it needs
live in separate repositories. See [NOKIA_9300.md](NOKIA_9300.md) for the layout,
installation and why signed MIDlets can't be used, and [BUILD_SYMBIAN_TLS.md](BUILD_SYMBIAN_TLS.md)
for building `SSLADAPTOR.dll`.

The reusable parts (capture scripts, flow and FastRPC decoders, decompiling notes, Nokia 9300
notes, OTA server, TLS DLL) are collected for the next app in
[janseris/android-to-j2me-kit](https://github.com/janseris/android-to-j2me-kit).

---

## 1. Tools

- **mitmproxy** on Windows (`mitmweb`).
- **adb**, from the Android SDK that Visual Studio installed: `C:\Program Files (x86)\Android\android-sdk\platform-tools`. We added it permanently to the user PATH with this PowerShell snippet:
  ```powershell
  $sdk = "C:\Program Files (x86)\Android\android-sdk"
  $p = [Environment]::GetEnvironmentVariable("Path","User")
  [Environment]::SetEnvironmentVariable("Path", "$p;$sdk\platform-tools;$sdk\emulator", "User")
  ```
  Only terminals opened **after** this change see it. Windows Terminal has to be restarted completely.
- **Node.js** (for `npx apk-mitm`) and **Java** 11 or newer (for apktool).
- A real Android phone with USB debugging enabled.

### Why not the emulator

We first tried the Visual Studio Android emulator. Its image was a *Google Play* image, and those can't be rooted: `adb root` answers "adbd cannot run as root in production builds". Without root you can't install mitmproxy's certificate as a system certificate.

The alternative is a *Google APIs* image (API 33 or lower) started with `-writable-system`, but that image has no Play Store. So we switched to the real phone and patched the app instead of the system.

## 2. Patch the app so it trusts mitmproxy

Since Android 7, apps only trust *system* certificates. A certificate installed by the user (from mitm.it) is ignored unless the app opts in. **apk-mitm** makes the app opt in and also removes certificate pinning.

```powershell
$pkg = "cz.fhejl.pubtran"
cd $env:USERPROFILE\phoneapk

# pull the installed app (base + split APKs)
adb shell pm path $pkg | ForEach-Object { adb pull ($_ -replace '^package:','') }

# bundle and patch
Compress-Archive *.apk app.zip -Force
Rename-Item app.zip app.apks
npx apk-mitm app.apks --apktool apktool.jar
```

- Without `--apktool apktool.jar`, apk-mitm uses its bundled apktool 2.9.3. The rebuild then fails with `attribute android:defaultLocale not found`, because the app targets SDK 36.
- We downloaded the latest `apktool_*.jar` from the iBotPeaches/Apktool releases on GitHub and passed it in.

Then we replaced the app on the phone. The original has a different signature, so it must be uninstalled first, and its data is lost:

```powershell
adb uninstall $pkg
Rename-Item app-patched.apks app-patched.zip
Expand-Archive app-patched.zip -DestinationPath patched -Force
adb install-multiple (Get-ChildItem patched -Recurse -Filter *.apk).FullName
```

Before patching we checked that it isn't a Flutter app: `split_config.arm64_v8a.apk` contains no `libflutter.so`. Flutter apps ignore the proxy and need a different approach.

## 3. Route the phone's traffic through mitmproxy (over USB)

```powershell
adb reverse tcp:8080 tcp:8080                              # phone's 127.0.0.1:8080 -> PC's 8080
adb shell settings put global http_proxy 127.0.0.1:8080    # system-wide proxy on the phone
```

Then install the mitmproxy certificate on the phone:

1. Open `http://mitm.it` in the phone's browser.
2. Tap **Android** and download the certificate.
3. Go to Settings → search for "CA certificate" → install it.

Only intercept the app's backend, so other apps (Spotify etc.) keep working and don't flood the log with TLS errors. This command also saves the capture:

```bat
cd %USERPROFILE%\phoneapk
mitmweb --listen-port 8080 --allow-hosts "pubtran-backend\.mapy\.cz" --set save_stream_file=pubtran.flow --set hardump=pubtran.har
```

Recorded user flow:

1. Odkud → typed "Olomouc", picked the city.
2. Kam → "Brno" (city).
3. Pressed Hledat.
4. Scrolled down to load more connections, then up to load earlier ones.
5. Opened a connection, then opened it again to see all stops.
6. Opened the connection with a transfer in Prostějov and opened both buses.
7. Swiped to the next runs of the second bus and opened one of them.

**When finished, turn the proxy off**, otherwise the phone has no internet whenever mitmproxy isn't running or the cable is unplugged:

```powershell
adb shell settings put global http_proxy :0
adb reverse --remove-all
```

## 4. Decoding the traffic

- All 34 requests are `POST https://pubtran-backend.mapy.cz/api/v1/<method>` with `Content-Type: application/x-frpc-rest`.
- There is **no authentication, API key or request signing.**
- The bodies are **Seznam FastRPC 2.1**, a binary format that starts with the magic bytes `CA 11 02 01`.
- We wrote a small FastRPC decoder, read the requests and responses straight out of `pubtran.flow`, and decoded all of them.

To give names to the numeric codes, we decompiled `classes.dex` from `base.apk` with **androguard**. The relevant classes:

- `TransportType.find()`: vehicle types.
- `JourneyPart.INFO_*`: info ids such as Wi-Fi, air conditioning, delays and warnings.
- `RouteItem.FLAG_*`: stop flags such as request stop and tariff zone.
- The request builders for `getroutesopt`, `suggest` and `getnextdepartures`, which show all the optional search flags (via, direct only, transport modes, low-floor, bike, stroller).

Endpoints found: `suggest`, `getroutesopt`, `getnextdepartures`, `gettripinfos`, `gettripdetail`, `getwalkgeom`. The full description, including paging and the swipe logic, is in **`PubtranClient/API.md`**.

## 5. The C# client (`PubtranClient/`)

- **`Pubtran.Api`** (net8.0 class library):
  - `FrpcCodec`: FastRPC encoder and decoder.
  - `PubtranApi`: typed methods for all six endpoints.
  - Models: `Place`, `Connection`, `RidePart`, `TripStop`, `InfoItem`, …
  - Verified by replaying the capture: all 34 recorded requests re-encode **byte-for-byte identically**, and every recorded response parses.
- **`Pubtran.WinForms`** (net8.0-windows) imitates the app:
  - Odkud / Kam / Přes pickers with suggestions and recent places.
  - Time, departure/arrival, transport filters, and Hledat.
  - "▲ Předchozí spoje" / "▼ Další spoje" instead of scrolling.
  - Connection detail, with a toggle for all stops.
  - Ride detail, with "◀ Předchozí spoj" / "Následující spoj ▶" instead of swiping.
  - Live delays, refreshed every 60 s.

Open `PubtranClient\PubtranClient.sln` in Visual Studio and press F5.

The recorded traffic, patched APK and API notes are for personal and interoperability use. Check the service's terms before publishing or distributing a client.
