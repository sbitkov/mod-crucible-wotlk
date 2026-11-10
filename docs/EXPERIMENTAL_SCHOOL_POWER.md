# Experimental school-specific spell power (server-only)

Status: **experimental / not compiled or tested in-game**. Do **not** merge to main until verified. This branch changes no DB tables, client DBC, MPQ, or Spell IDs.

## Why a small AzerothCore patch is required

Existing UnitScript damage hooks run after the caster's spell power has been multiplied by the spell coefficient (and/or after further modifiers). They cannot faithfully inject an additional *school-limited spell power* contribution.

The patch `patches/azerothcore-school-spell-power-hook.patch` adds a single UnitScript hook from `Unit::SpellBaseDamageBonusDone()`. It is invoked *before* the existing direct/DoT coefficient, level penalty and spell modifiers. The hook is inert in the absence of a subscribing module.

The Crucible module relies on this server hook and **will not build against an unpatched core**. The patch is supplied as an integration artifact, not incorporated into AzerothCore upstream or mod main.

## Install / build

1. Back up your working trees and inspect uncommitted changes.
2. In a PowerShell terminal, from `C:\Azerothcore\modules\mod-crucible`:
   ```powershell
   git fetch origin experimental/school-specific-spell-power
   git restore --source=origin/experimental/school-specific-spell-power -- src/CrucibleSystem.cpp src/CrucibleSystem.h src/CruciblePlayer.cpp patches/azerothcore-school-spell-power-hook.patch
   ```
3. The command above also restores the core patch into the module's `patches` folder.
4. From `C:\Azerothcore`, first check and only then apply:
   ```powershell
   git apply --check modules/mod-crucible/patches/azerothcore-school-spell-power-hook.patch
   git apply modules/mod-crucible/patches/azerothcore-school-spell-power-hook.patch
   ```
   If the check fails, stop. Local AzerothCore may differ from the upstream version the patch was prepared against.
5. Reconfigure CMake if it does not pick up changed headers and rebuild the Release worldserver:
   ```powershell
   cmake --build C:\Azerothcore\build --config Release --target worldserver --parallel 6
   ```
6. Stop the running server and install the rebuilt `worldserver.exe` into `C:\Program Files\AzerothCore`, with a backup and matching hash verification.

## Design

- Random property/suffix `ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL` effects with precisely one nonphysical `SPELL_AURA_MOD_DAMAGE_DONE` school are extracted as the existing school-power `StatId` values.
- Random suffix scaling uses the particular instance's suffix factor rather than a generic default.
- No generic spell power is added; the hook adds only the corresponding school values already tracked by Crucible's active `sAppliedStats` session cache.
- The normal AzerothCore `SpellDamageBonusDone()` processing subsequently applies all coefficient, level, direct/DoT and spell modifier calculations.
- No new auras or spell IDs; no client patch, no extra DB columns.

## Mandatory tests before merging

- Source item with a known single-school affix: the preview must display the *correct school* and *source amount*, with the correct 20% initial coefficient.
- After absorption, verify the accumulated school bonus and the result on a spell of the matching school.
- Negative control: cast a spell of another school, which must remain unchanged.
- Check one direct and one periodic damage spell, including spells with coefficients below 1 and above 1 if available. Do not expect the full advertised +X as flat final damage.
- Verify multiple suffix values, Mastery upgrades, `.crucible recalc`, equipment changes, relog and reconnect; observe no bonus duplication.
- Test a class with no relevant school-damage spells for regressions.
- Verify standard client without MPQ/DBC changes.

**Caveat:** No local AzerothCore build or runtime execution is available in this environment. Server compilation and numerical behavior must be verified on the user's build before merging.
