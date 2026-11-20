# Pre-Raid Mastery registry — research audit (2026-11-20)

**Status: incomplete seed; NOT fit for production**. Do not apply the 28g 50s tariff to items from these CSV files yet.

## Approved rules
- Mastery gold cost of the entire 20→100% cycle: 50–59 = 9g 50s; Pre-Raid = 28g 50s.
- Every equippable UBRS *drop* is Pre-Raid. Exclude quest rewards merely associated with UBRS, reagents, unequippable loot. **No arbitrary down-tier exemptions for UBRS**.
- Elsewhere, classify primary table Pre-Raid BiS entries and explicit main-table alternatives for all Vanilla classes/specs. Ignore prose-only temporary substitutes.
- Tariff is a function of ItemEntry, NOT the path by which the player obtained an instance.
- Preserve every source and reason separately; one registry row per ItemEntry.
- Source version/phase, class, specialization and table position remain part of evidence.

## What has actually been done
- Established a five-item seed of cross-checked Classic-era item IDs and relevant published evidence in `preraid_registry.csv` / `preraid_evidence.csv`.
- Icy Veins Warrior DPS Pre-Raid guide's Naxx Phase 6 and MC/Ony Phase 1 main tables contain Eye of Rend, Mask of the Unforgiven, Hand of Justice. These main-table entries include alternatives, not necessarily a single top-ranked BiS item. Source: https://www.icy-veins.com/wow-classic/warrior-dps-pre-raid-gear
- External Classic item pages identify Eye of Rend ID 12587, Mask of the Unforgiven 13404, Hand of Justice 11815, Briarwood Reed 12930, Starfire Tiara 12604. UBRS named drop examples have external source evidence, but are NOT yet locally database-verified.
- **No local AzerothCore item_template or loot table was available** in this session; the CSV explicitly marks both local verifications false.
- `ubrs_local_audit.sql` offers read-only MySQL 8 queries for creature spawn locations, known UBRS boss loot (including nested Reference templates) and validation of the five seed ItemEntries.

## Blocking completeness issues
1. Both LBRS and UBRS use Blackrock Spire map 229. Never automatically include all loot on that map. Distinguish upper-wing mobs using spawn coordinates, boss/wing rosters and local evidence.
2. Boss-only loot does not cover UBRS trash drops, chests, scripts, optional bosses and referenced/conditional drops. Build a complete wing roster and handle gameobject loot separately.
3. Validate `creature_template.lootid`, `creature_loot_template` and `reference_loot_template`; distinguish quest required and LootMode. A recursive expansion is a list of possibilities, **not computed final drop probabilities**.
4. Check every item against local `item_template` (entry/class/inventory type). Some Classic Era items may have altered or unavailable stats in WotLK 3.3.5a.
5. Expand main-table Pre-Raid BiS and explicit alternatives across all classes and specs, tracking guide phases; omit Season of Discovery, Season of Mastery and WotLK lists.
6. Resolve ambiguous/unavailable IDs into `preraid_unresolved.csv`. The no-ID entries are issue-level tasks, not item records.

## Provenance / data contract
- `preraid_registry.csv` is unique on ItemEntry. `candidate` means source evidence exists but **not cleared for production**.
- `preraid_evidence.csv` is many-to-one and may have repeated ItemEntries for independent reasons, guides or phases.
- Source links and evidence capture date are populated; all external evidence should remain auditable.
- A single quality guide is a *proposed* inclusion threshold, not explicitly approved as a binding rule.

## Implementation once audited
Keep the current progression-level rules as fallback, but check an explicit curated `ItemEntry` whitelist first. On match use 28g 50s regardless of RequiredLevel/ItemLevel. Do not infer Pre-Raid automatically from item level or difficulty, and avoid runtime web dependencies. Version the curated registry alongside the module. Ensure consistent classification of BASE and affix essences of the same item.

### Existing progress
Separate `feature/mastery-50-59` branch implements the normal 50–59 tier at 9g 50s (new proposed per-step reagents). Neither experimental economic branch is merged.
