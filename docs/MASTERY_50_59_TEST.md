# Mastery tier 50–59 — test proposal

Status: implemented on `feature/mastery-50-59`, not compiled or tested on the user's server. No change to `main`.

The user-approved **total gold price** for the 20%→100% cycle is **9g 50s**. The step distribution and reagents below are a **new implementation proposal**, not previously approved values.

| Mastery step | Copper | Gold | Reagent ID | Count | Reagent |
|---|---:|---:|---:|---:|---|
| 20→40 | 10000 | 1g | 0 | 0 | — |
| 40→60 | 20000 | 2g | 16204 | 2 | Illusion Dust |
| 60→80 | 30000 | 3g | 16203 | 1 | Greater Eternal Essence |
| 80→100 | 35000 | 3g 50s | 14344 | 1 | Large Brilliant Shard |
| Total | 95000 | 9g 50s | | | |

The existing `RequiredLevel`→`ItemLevel` fallback is unchanged. Values 1–49 are unchanged. A normal leveling item from 50–59 is eligible; an item above 59 remains unsupported in this branch (unless a future dedicated tier is added). This intentionally does **not** implement the separately agreed Pre-Raid price 28g 50s: classification requires a validated ItemEntry registry.

## Verification before merge

- At required level 49, confirm existing 40–49 pricing.
- At required levels 50 and 59, confirm each exact currency/reagent combination and progression, including affordability errors.
- Required level 0 with item level 50/59 should use the fallback.
- Required level 60 should not silently inherit the 50–59 rate.
- Verify independently mastered BASE and random-affix essences.
- Test a 20→100 cycle and verify no charge for invalid mastery steps.
- Ensure the locally installed worldserver binary actually matches the rebuilt Release output.

No automatic modifications to existing essences or stored Mastery are performed.
