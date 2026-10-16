# GoldenEye: But I Am the Guard

**Runway Beta 0.1.0 · internal build v9 · Nintendo 64 / NTSC-U**

Play as the guards trying to stop an AI-controlled James Bond from completing Runway. When you die, take control of another surviving guard already in the level. Kill Bond to defend the runway; lose if he escapes or no eligible guard remains.

This first public beta supports **Runway only**. Streets, Dam and Facility are future ideas, not playable guard-mode stages in this release.

## Download and play

Download the `.xdelta` patch from this repository's **Releases**. Apply it directly to your own clean NTSC-U `.z64` using [PATCHING.md](PATCHING.md). This is an all-in-one patch built on GoldenEye 007 Plus; an earlier third-person or guard-mode patch is not needed.

Use **8 MB RAM** and **16 Kbit EEPROM**, restart the emulator, and start a fresh mission. Older save states contain older game code/state and must not be loaded for this version.

Choose **Save 1 (upper-left folder) → Solo → Runway**. Save 1 has the solo missions unlocked. Leave gameplay cheats and camera overrides off for an initial test.

| Difficulty | Bond must do before escaping | Your full-health guard |
| --- | --- | --- |
| Grunt | Collect the aircraft key; reach the plane | 3 health squares |
| Secret Grunt | Key; destroy the missile battery; plane | 3 health squares |
| 00 Grunt | Key; destroy the missile battery and all three heavy guns; plane | 5 health squares; 50% stronger bullet damage |

The tank is optional in the original mission. This mod deliberately sends AI Bond to the tank on the two destruction difficulties. Your objective remains **Eliminate James Bond**.

## How the beta plays

- Your first life is one of the two existing key-room guards. Transfers use surviving, unused guards elsewhere, preferring a distant location, and inherit the guard's appearance, weapon and remaining health. Nine existing guards are eligible; the original starting-building guard is excluded.
- Every life uses the native opening camera transition. Death shows one elevated external view of your guard collapsing, then transfers to the next guard. There is **no blood-screen overlay or repeated replay**. Runway music continues.
- You cannot pick up keys, guns, grenades, ammunition, armour or gadgets. Bond must enter the actual key room and physically reach the key.
- Bond starts with a silenced PP7 and a seven-round magazine. He can retrieve and equip an actual gun dropped by a killed guard. He moves while shooting and reloading. Sidesteps are spaced apart by a 240-tick cooldown.
- Bond normally selects among visible guards. Shooting him gives the real attacker temporary priority. Other guards and drone guns fight him too; they continue fighting during your death and transfer.
- Bond has 160 HP and resists stagger until **24 HP or lower (15%)** on foot. Damage persists between your lives. He stays seated while driving.
- AI Bond is visible above the tank hatch. The cannon fires real shells at reachable guards and mission targets. At close range he can use his held gun from the hatch, with normal magazine/reload logic. The human guard and NPC guards use the same acquisition rules.
- A moving tank can run over guards, including you. Your guard cannot climb onto or enter Bond's tank. Bullets that hit the occupied hull damage Bond at half strength.
- Your hurt sounds use native male guard voices. The passport uses the requested scientist face; difficulty labels are Grunt, Secret Grunt and 00 Grunt.

## Scope and feedback

This is a playable beta, with room to improve combat balance, movement and camera behavior. Tank boarding/leaving are timed state transitions rather than newly authored climbing animations. The cannon retains range and elevation limits; the held gun covers close defence when there is line of sight. Original first-person gun/arm assets remain. There is no custom aircraft takeoff scene.

See [VALIDATION.md](VALIDATION.md) for the specific native emulator checks and their limits. To report a problem, include the build, difficulty, emulator/version, graphics plugin, RAM/save settings, reproduction steps, and ideally a short clip. Use [BUG_REPORT.md](BUG_REPORT.md).

## Source and rebuilding

- `guardmode.c` and `guardmode.h`: readable core implementation.
- `guard-mode-source.patch`: complete binary-capable source changes, including hooks, setup/text, portrait asset/encoder and build integration.
- [REBUILD.md](REBUILD.md): pinned upstream, dependencies and build commands.
- [BUILD_INFO.json](BUILD_INFO.json) and `SHA256SUMS.txt`: release hashes and validation metadata.
- [ROADMAP.md](ROADMAP.md): future map concepts only.

The patch/source project contains no game ROM or emulator save. `guardmode.c` alone is not a standalone program: apply the complete source patch to the pinned upstream to rebuild.

## Credits and notices

Concept, direction, playtesting and project development: **Big Chowds**. Built on [GoldenEye 007 Plus](https://github.com/Joshua-1248/GoldenEye-007-Plus), pinned to `d9fd72c0be65ea15e542962bae8eb89a97b65868`, and the GoldenEye decompilation. Rare created the original game.

Preserve [CREDITS.txt](CREDITS.txt), [LICENSES_AND_NOTICES.txt](LICENSES_AND_NOTICES.txt) and [REFERENCES.txt](REFERENCES.txt). This project does not assign a new blanket licence to upstream game-derived material.
