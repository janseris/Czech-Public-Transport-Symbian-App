**EKA1 (S80v2 / S60v2) build: "Dll has initialised data" from `x509_minimal_full.c`**

When building symbian-tls for EKA1 (ARMI, gcc 2.9-psion-98r2, GnuPoc), petran rejects `ssladaptor.dll` with *"Dll has initialised data"*. EKA1 DLLs may not contain writable static data.

The cause is in `src/x509/x509_minimal_full.c`. `static const br_hash_class *hashes[] = { ... };` is a static array of non-const pointers, which are initialised at load time and so end up in writable `.data`. My fix makes it a local array, filled on the stack at run time:

```c
const br_hash_class *hashes[] = { ... };   /* was: static const br_hash_class *hashes[] */
```

`static const br_hash_class *const hashes[]` might also work, if the toolchain puts it in read-only data. I haven't tried that. The fix is [janseris/bearssl-symbian@5b817d5](https://github.com/janseris/bearssl-symbian/commit/5b817d5), branch `eka1-fixes`. With it, symbian-tls builds and runs on a Nokia 9300 (details in shinovon/symbian-tls#13).
