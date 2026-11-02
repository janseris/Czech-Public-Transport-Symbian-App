Update: Java HTTPS on the Nokia 9300 (S80v2, EKA1) now works reliably with a patched build. Everything is in my fork:
**[janseris/symbian-tls `eka1-java-fixes`](https://github.com/janseris/symbian-tls/compare/master...janseris:symbian-tls:eka1-java-fixes)** (tag `pubtran-v1.1`, tested build "v10"), built against [janseris/bearssl-symbian `eka1-fixes`](https://github.com/janseris/bearssl-symbian/tree/eka1-fixes).

The commits are split by topic, so you can take any part of it. The real-world test is a J2ME public transport client ([janseris/pubtran-j2me](https://github.com/janseris/pubtran-j2me)): suggestions, route search with 50–70 KB responses, many requests in a row, and downloads up to 1 MB.

## Results on the Nokia 9300 (Java `HttpConnection`, `https://`)

| | before | now |
|---|---|---|
| Sites that need SNI (jsonplaceholder, Google, seznam, pubtran-backend.mapy.cz) | 403 / alerts / failed | OK |
| seznam, duckduckgo, cnn ("Unexpected end of stream") | failed after 7–15 s | OK |
| pubtran-backend.mapy.cz (ECDSA P-256 leaf, P-384 chain to ISRG X2) | phone froze, USB link dropped (-29) | OK, ~1 s full handshake |
| Full handshake | 2–6 s | ~1.0 s (seznam ~1.8 s) |
| Resumed handshake | – | ~0.25 s |
| HTTPS download | ~2 KB/s | 100–140 KB/s, the same as plain HTTP on the same link |

## What was wrong and how it's fixed (all EKA1-only, behind `#ifndef EKA2` where it matters)

1. **No SNI from Java.** MIDP's `HttpsConnection` never calls `SetOpt(KSoSSLDomainName)`, but the browser does. In `StartClientHandshake`, when no host name is set, the handshake is **deferred until the first `Send`**. The host is then taken from the HTTP request's `Host:` header and used for SNI. There is an optional fallback file, `C:\System\Data\ssl_sni.txt` (`<IPv4> <host>` lines).
   - Java sends a POST as two writes, headers then body. A second `Send` that arrives during the deferred handshake is queued.

2. **"Unexpected end of stream".** Java reads in 512-byte chunks and reuses a full buffer. `Recv`/`RecvOneOrMore` appended to it, read 0 bytes, and reported that as EOF. They now **replace** the descriptor contents, like `RSocket` does (`aDesc.Zero()`). `close_notify` is reported as `KErrEof`, not -1.

3. **Phone freeze / -29 on pubtran-backend.** `x509_minimal` verified every signature of the 4-certificate P-384 chain, and the result was ignored anyway (`NO_VERIFY`). On the ARM9 this took long enough for the USB link to drop. The fix is a **leaf-only X.509 handler** built on `br_x509_decoder`: it decodes only the leaf's public key and skips the chain maths.

4. **Slow downloads (~2 KB/s).** `CBio` did one asynchronous socket read per TLS field: a 5-byte header, then the record body. It now **reads ahead** up to the whole 16 KB buffer, and `recv_callback` serves the TLS engine from that buffer synchronously.

5. **Session resumption across connections.** The DLL is unloaded after every connection (`UnloadDll`), so there is no in-memory cache. Sessions are saved by host name to `C:\System\Data\ssl_sessions.dat` (`br_ssl_engine_get/set_session_parameters`). Servers that resume by session ID (cnn, example.com, npr, cdnjs, pubtran-backend) go from ~1 s to ~0.25 s. Cloudflare and Google only resume with session tickets, which the BearSSL client doesn't support.

6. **Closing safely.**
   - `Close`/`CancelAll` complete every pending client request and call `iSocket->CancelAll()`, so nothing completes on a deleted object.
   - A `close_notify` that arrives during a read is not answered: the reply is dropped instead of starting a write on a closing connection.

7. **Hang after a failed handshake: this one froze the phone.** Sometimes the connection is closed right after the ClientHello (handshake completes with `KErrEof`). The deferred `Send` was completed with the error, but Java then issued another `Send`/`Recv` on the same `CTlsConnection`. That request stayed `KRequestPending` forever: `iHandshaked` was false and nothing was running. The Java thread waited, and the phone froze. Now the handshake error is kept, and every later `Send`, `Recv` or `StartClientHandshake` completes with it at once.

8. **Building on Linux with GnuPoc (EKA1, ARMI, gcc 2.9-psion-98r2)**, see [BUILD_LINUX_EKA1.md](https://github.com/janseris/symbian-tls/blob/eka1-java-fixes/BUILD_LINUX_EKA1.md):
   - The file-name case must match the includes.
   - petran rejects **DLLs with initialised writable data**. So the stub certificate is `const`, and in bearssl-symbian `x509_minimal_full.c` there is no `static` array of pointers ([commit](https://github.com/janseris/bearssl-symbian/commit/5b817d5)).

## Tips and pitfalls (EKA1)

- **Logging changes timing a lot.** One line in a log file costs ~15 ms on the 9300. A per-record log made downloads run at ~2 KB/s, doubled handshake times, and **hid** timing bugs: with the verbose log they disappeared. My `SSL_LOG` build therefore writes one summary line per connection and flushes every line, so a crash keeps the log. `SSL_LOG_VERBOSE` has the old per-read lines.
- **Never complete or leave a client `TRequestStatus` in a state where nothing will run.** On EKA1 the Java comms thread (`jes-dd-java-comms`) just waits, and the phone becomes unusable. Every error path needs to complete pending Send, Recv and handshake requests.
- **Don't close a Java `HttpConnection` from another thread** while the comms thread is inside the adaptor. That gave KERN-EXEC 3 in `jes-dd-java-comms` and a freeze on the next request. This is on the app side, but worth knowing when you test.
- **CPU starvation is real.** A Java UI that repainted the full 640×200 screen every 120 ms while a request ran made handshakes stall: the adaptor runs in the same Java process. Apps should keep animations cheap during requests.
- **Protocol / cipher / ServerCert reporting on EKA1 is still wrong:** Java shows "SSL 3.0" or TLS 1.0, cipher `0x0000` and no certificate. The connections are TLS 1.2 (ChaCha20-Poly1305 / AES-GCM). I haven't fixed this.

Thanks for the patch; without it the 9300 couldn't reach any modern HTTPS server.
