# Rebuild Runway v9

The complete source patch targets this exact upstream revision:

```sh
git clone https://github.com/Joshua-1248/GoldenEye-007-Plus.git
cd GoldenEye-007-Plus
git checkout d9fd72c0be65ea15e542962bae8eb89a97b65868
git apply /path/to/guard-mode-source.patch
```

The patch includes the guard-mode C sources and hooks, Runway setup/briefing text, passport PNG and texture encoder, build rules, and GNU toolchain compatibility fixes. `guardmode.c` and `guardmode.h` are also provided separately for convenient inspection.

Dependencies: make, GNU host C/C++ compilers, binutils-mips-linux-gnu, and Python 3 with **Pillow** and **zopfli**. The pinned upstream includes the reconstructed IDO game compiler and host tool sources. This build uses IDO for game C and GNU MIPS assembly/link tools.

Put the clean NTSC-U ROM at `baserom.u.z64`; verify SHA-1 `abe01e4aeb033b6c0836819f549c791b26cfde83`.

```sh
make force_extract_u
python3 /path/to/prepare_build_assets.py
make VERSION=US PHYSICAL_CODE=YES PHYSICAL_FASTPATHS=YES \
  MODDED_CHEATS=YES UNLOCK_SAVE1=YES MAP_MAKER=NO \
  OPT_USE_LLVM=NO AS=scripts/toolchain/gnu-mips-as COMPARE=0 -j4
```

Output: `build/u-phys-opt-mod-eep16-unlock/ge007.u-phys-opt-mod-eep16-unlock.z64`.

The delivered file additionally changes the 20-byte internal title at offset `0x20` to `GE GUARD RUNWAY V9`, padded with spaces. This header title is outside the N64 CRC calculation range.

The asset preparation helper restores skipped Japanese text blocks from the supplied US ROM, supplies retail PP7 models for the snapshot's incomplete Gold/Silver PP7 resource slots, and ensures the modified Runway setup, text and briefing build from C rather than extracted binaries. The passport encoder preserves each original texture allocation, I4 format and all seven mip levels. No Gun Game logic is included.

Passport asset: the supplied N64 scientist screenshot was adapted into a square face crop. The final asset is `assets/guardmode/guard_portrait.png` and is included in the binary-capable source patch. Its deterministic encoder is `scripts/guardmode_portrait.py`.

The prepared `github-ready/` folder is a standalone patch/source repository layout. It does not contain the upstream checkout, extracted retail assets, ROMs or emulator saves.
