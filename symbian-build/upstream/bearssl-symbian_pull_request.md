**Title:** EKA1: no static pointer array in x509_minimal_full.c ("Dll has initialised data")

When building symbian-tls for EKA1 (S80v2 / S60v2, ARMI, gcc 2.9-psion-98r2), petran rejects `ssladaptor.dll` with "Dll has initialised data". EKA1 DLLs may not contain writable static data.

`x509_minimal_full.c` has `static const br_hash_class *hashes[]`: the pointers are initialised at load time and end up in writable `.data`. This change makes it a local array, filled on the stack at run time. With it, symbian-tls builds and runs on a Nokia 9300; see shinovon/symbian-tls#13.
