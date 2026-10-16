# Install Runway Beta 0.1.0 (build v9)

## Correct source ROM

Use your own clean **GoldenEye 007 NTSC-U / USA, big-endian `.z64`**. Apply this patch directly to the clean ROM, never to an earlier mod.

| Check | Expected value |
| --- | --- |
| Size | 12,582,912 bytes |
| SHA-1 | `abe01e4aeb033b6c0836819f549c791b26cfde83` |
| SHA-256 | `2cdcec8a9f0cb6e36337f3ee39d8ad105dc8afa6ba1c02d466e8f5b771f9a162` |

On Windows PowerShell, check a file with:

```powershell
Get-FileHash -Algorithm SHA256 "C:\Games\GoldenEye\clean-us.z64"
```

## Apply the patch

With an xdelta patcher's apply/decode mode, select:

1. Patch: `GoldenEye_Im_The_Guard_Runway_v9.xdelta`.
2. Source: the verified clean USA `.z64`.
3. Output: a **new** `GoldenEye_Im_The_Guard_Runway_v9.z64` file.
4. Apply/decode the patch.

Or use xdelta3:

```sh
xdelta3 -d -s clean-us.z64 GoldenEye_Im_The_Guard_Runway_v9.xdelta GoldenEye_Im_The_Guard_Runway_v9.z64
```

The result is 33,554,432 bytes (32 MiB), with internal title `GE GUARD RUNWAY V9`. `BUILD_INFO.json` supplies its exact SHA-256. The patch includes the foundation's third-person/options support; no separate earlier patch is required.

## Emulator setup

Set **8 MB RAM** and **16 Kbit EEPROM** for this ROM. Restart the emulator after changing them. Start a fresh mission, with gameplay cheats/camera overrides off for the first check. Do not restore an older version's save state.

Choose **Save 1**, the **upper-left folder**, then **Solo → Runway**. Select Grunt, Secret Grunt or 00 Grunt. Other maps are not guard-mode implementations in this beta.

If patching reports a source checksum mismatch, recheck the clean ROM hash and byte order. A PAL, Japanese, byte-swapped, pre-patched or differently expanded ROM is not this patch's source.
