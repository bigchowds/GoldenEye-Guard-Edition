# Runway Beta 0.1.0 — validation

Internal build v9, final source build61. This is a Runway-only beta; no other map implementation was added.

## Build and patch

The 32 MiB US physical-code build passes linking, address/function checks and N64 checksum audit. One expected warning concerns the intentionally enlarged game ROM segment. Compressed game data matches the linked segment byte-for-byte. The xdelta decodes against the supplied clean USA ROM to exactly the delivered ROM. The full binary-capable source patch passes forward application against the pinned upstream index, reverse application and whitespace checks. BUILD_INFO.json and SHA256SUMS.txt contain hashes.

## Footage review

The supplied 135.817-second v8 playtest and four screenshots were reviewed. The Streets third-person screenshot shows a visible tank driver. V8 intentionally hid AI Bond's model, excluded cannon targets within 700 units, and retained retail player tank-deck support for the controlled guard. These explain the missing driver, close-range blind spot and guard riding the tank seen in the footage. Blood was removed as requested; no claim is made that blood was the sole cause of every observed transition issue.

## Native emulator checks

The final crush/death-view check uses build61; other final checks use build60 except the natural Grunt run (build58). Build61 only raises the run-over camera above the moving hull. Mupen64Plus 2.6.0, 8 MB RAM, 16 Kbit EEPROM. Rice rendering at 320x240 is used for presentation checks. No test RAM patches or fixture assistance is included in the release ROM.

- Final Secret Grunt and 00 Grunt collect the physical key in room16, use the tank, destroy the actual required objects, dismount and escape. Secret requires only the battery;00 requires the battery and all three heavy guns. Objective masks and the native destroyed bit of each target are checked. These observer runs use health 100, automated menus and normal NPC health, without assisting Bond or the objectives.
- The final 00 route records five real anti-guard cannon impacts and five native held-gun shots. Each cannon character impact kills a real normal-health NPC. Native allied bullets and drone fire damage Bond.
- A final rendered direct-tank view places a health 100 observer on native pad145 and faces Bond. The driver is visibly rendered above the hatch while the tank moves/turns and fires. Seven cannon character impacts include a shot at the human guard, whose health decreases. Bond completes all four objectives and escapes. No player hit on Bond or artificial objective damage is injected in this watched run.
- A final close-defence fixture relocates a health 100 observer beside the tank and invokes a native player bullet-impact helper on the occupied hull. Bond retaliates using the held weapon's native aim/bullet trace, removing 0.125 health from the human guard. The observer never enters or gains deck-support height on the tank. The shot and return damage are not injected.
- A final moving-track fixture places a health 100 observer immediately ahead of the native tank collision footprint. Actual tank movement and overlap kill the controlled guard, with no fixture call to a player-death function. It plays one elevated external native collapse without blood, above the moving hull, then the next surviving guard's opening transition. Tank entry/deck support remain disabled for the human guard. Lethal crushing bypasses the one-square bullet-damage normalization; NPC victims use the same native explosion-death path as retail tank run-overs.
- Final Third Person camera checks invoke two native deaths, one in the key room and one outdoors. Both play one normal collapse and transfer, without a replay loop or blood overlay. Animation frames advance without resetting; completion is natural. Bond is hidden only to isolate this presentation check. The rendered indoor/outdoor and run-over views were inspected.
- The final death-combat fixture preserves the other key-room NPC until the encounter, restores its normal 3 HP, then invokes native player death while Bond has ammunition. Bond fires two further rounds and damages that guard during the external death view, before respawn completes. No shot or NPC hit is injected.
- Final native hit fixtures confirm no stagger at 25 HP and native ARGH at 24 HP, exactly 15% of Bond's 160 HP. Occupied-tank handling remains seated. An actual native key pickup is refused without consuming the key.
- Five final native incoming 0.125 damage calls kill a full-health 00 Grunt after five squares. All five, including the lethal hit, select native male guard pain IDs 134 through 138 in the fixture. The source uses the native 25-sample male guard bank. Sound selection is verified; audible output quality is not claimed.
- The rendered natural Grunt run uses build58, which has identical on-foot, death-camera and voice logic. Only menus are automated; player/NPC health is normal. It records four natural deaths and four external collapse finishes, with no blood, before Bond escapes with the physical key. Changes after 58 affect tank alignment and crushing only; this difficulty never boards the tank.
- Music remains mission state 1, track 50, fade 0 throughout the recorded checks. Blood overlay frames remain zero throughout. This verifies music state rather than audio playback quality.

## Limits

No human Project64 or hardware playtest of v9 is claimed. The supplied video tested v8. Movement appearance, sound, frame rate and combat feel still benefit from beta feedback in the user's emulator. Boarding/leaving are timed state changes, not new climbing animations. The cannon has physical elevation/range limits; close defence with a held weapon still requires line of sight. A blocked or cramped death view can frame nearby scenery. Other maps/bosses and custom aircraft takeoff are outside this beta. See ROADMAP.md for ideas only.
