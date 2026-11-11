# Experimental fix: passive Equip spell stats

Branch: `fix/equip-spell-passive-stats` (based on `main`). **Not yet built or tested in-game. Do not merge before testing.**

## Scope

Some WotLK items store a permanent stat bonus in `item_template.spellid_N` with `spelltrigger_N = 1` instead of `stat_typeN`/`stat_valueN`. Previously Crucible's BASE extraction only inspected `ItemStat[]`, armor and resistances.

For each `ITEM_SPELLTRIGGER_ON_EQUIP` spell, the BASE extractor now checks that every active spell effect is a recognized permanent `SPELL_EFFECT_APPLY_AURA` with a positive, constant amount. It recognizes:
- Individual primary stats (`SPELL_AURA_MOD_STAT`) but excludes ambiguous "all stats" masks.
- Combat rating masks (`SPELL_AURA_MOD_RATING`) for exact single ratings or standard melee/ranged/spell bundles of crit, hit or haste.
- Flat melee and ranged attack power auras.

Unrecognized effects, multi-effect spells containing unknown effects, nonpositive amounts, On Use effects and Chance on Hit effects are skipped. Existing random affix extraction, mastery and applied-rating code are unchanged.

**Test target:** Ornate Mithril Helm, item ID `7937`, has `spellid_1=7597`, `spelltrigger_1=1`. Spell `7597` is `Increased Critical 14`, a `SPELL_AURA_MOD_RATING` aura with +14 to critical strike rating. At 20% absorption the expected BASE essence includes **+2.8 Crit Rating** (as well as existing Strength and Armor).

## Test sequence

1. Before reinstalling, inspect local uncommitted modifications to `src/CrucibleSystem.cpp`. If you currently have the experimental school-specific Spell Power branch installed, save it first. This fix is based on `main` and restoring the file from this branch replaces your local experimental school-power extraction.
2. Restore only `src/CrucibleSystem.cpp` from this branch; do not touch the local AzerothCore core hook or the other files.
3. Build the server and replace the running worldserver binary after shutdown.
4. **Important:** Crucible keys an already absorbed BASE essence by item entry. Because your existing character already absorbed Ornate Mithril Helm (7937), a second physical copy on that character will show `ALREADY_ABSORBED` and will not preview a new BASE contribution. Test on a **fresh test character** that has never absorbed item 7937 (and meets item eligibility), or choose another unabsorbed item using Equip spell 7597. On the fresh character, preview the helm **without absorbing it**. Expected +2.8 Crit Rating.
5. Verify On Use and proc items do not suddenly display proc effects as permanent stats; also verify a normal `ItemStat[]` item continues to preview correctly.
6. Once preview is correct, absorb the new item on the fresh test character and verify critical-rating increase (allow for summation and integer truncation when calculating applied rating); test recalc and relog.

**Existing absorbed items are not migrated or backfilled.** Their per-item contribution snapshots remain unchanged. Plan safe backfill separately after gameplay tests, respecting per-essence Mastery and avoiding double credit.

No client MPQ, DBC or additional server core patch is required for this fix itself.
