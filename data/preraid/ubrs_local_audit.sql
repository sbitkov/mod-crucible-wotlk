-- NON-DESTRUCTIVE scouting queries for local acore_world (MySQL 8).
-- IMPORTANT: Blackrock Spire map 229 includes BOTH UBRS and LBRS.
-- These outputs are RAW candidates: do not classify all map 229 entries as UBRS.
-- ItemTemplate source: item_template.entry; equip types class=2 (weapon), class=4 (armor).
-- Apply manual upper-wing source vetting before marking registry rows approved.
USE acore_world;

-- A. List EVERY spawned creature on shared map 229, with coordinates.
-- Export and label UBRS vs LBRS bosses and trash before resolving their loot.
SELECT DISTINCT
    c.id1 AS creature_entry,
    ct.name AS creature_name,
    ct.lootid AS loot_template,
    c.position_x, c.position_y, c.position_z,
    c.spawnDifficulties
FROM creature c
JOIN creature_template ct ON ct.entry = c.id1
WHERE c.map = 229
ORDER BY ct.name, c.position_x, c.position_y;

-- B. Known UBRS named bosses only (starter set, not the complete UBRS wing).
-- Verify exact local creature identities; naming conventions and loot may differ.
SELECT
    ct.entry AS creature_entry, ct.name AS creature_name,
    ct.lootid AS loot_template
FROM creature_template ct
WHERE ct.name IN (
    'Pyroguard Emberseer',
    'Solakar Flamewreath',
    'Goraluk Anvilcrack',
    'Warchief Rend Blackhand',
    'Gyth',
    'The Beast',
    'General Drakkisath',
    'Jed Runewatcher',
    'Lord Valthalak'
)
ORDER BY ct.name;

-- C. Resolve creature loot for the same boss seed through reference_loot_template.
-- This is intentionally NOT a complete UBRS equipment registry. Excludes quest-only drops.
-- Never multiply nested Chance values naively: groups, references and loot modes matter.
WITH RECURSIVE
bosses AS (
    SELECT ct.entry AS creature_entry, ct.name AS creature_name, ct.lootid
    FROM creature_template ct
    WHERE ct.name IN (
        'Pyroguard Emberseer', 'Solakar Flamewreath', 'Goraluk Anvilcrack',
        'Warchief Rend Blackhand', 'Gyth', 'The Beast',
        'General Drakkisath', 'Jed Runewatcher', 'Lord Valthalak'
    ) AND ct.lootid <> 0
),
loot_walk AS (
    SELECT b.creature_entry, b.creature_name, b.lootid AS origin_loot,
           l.Item AS item_entry, l.Reference AS next_ref,
           l.Chance AS chance_at_this_step,
           l.QuestRequired AS quest_required,
           l.LootMode AS loot_mode,
           l.GroupId AS group_id,
           0 AS depth,
           CAST(CONCAT('/', b.lootid, '/') AS CHAR(512)) AS seen
    FROM bosses b
    JOIN creature_loot_template l ON l.Entry = b.lootid
    UNION ALL
    SELECT w.creature_entry, w.creature_name, w.origin_loot,
           l.Item, l.Reference, l.Chance, l.QuestRequired,
           l.LootMode, l.GroupId, w.depth + 1,
           CONCAT(w.seen, w.next_ref, '/')
    FROM loot_walk w
    JOIN reference_loot_template l ON l.Entry = w.next_ref
    WHERE w.next_ref > 0 AND w.depth < 8
      AND LOCATE(CONCAT('/', w.next_ref, '/'), w.seen) = 0
)
SELECT DISTINCT
    w.creature_entry, w.creature_name, w.origin_loot,
    w.item_entry, it.name AS item_name,
    it.class AS item_class, it.subclass AS item_subclass,
    it.InventoryType AS inventory_type, it.RequiredLevel AS required_level,
    w.depth, w.chance_at_this_step, w.loot_mode, w.group_id
FROM loot_walk w
JOIN item_template it ON it.entry = w.item_entry
WHERE w.next_ref = 0 AND w.quest_required = 0
  AND it.class IN (2, 4)
  AND it.InventoryType <> 0
ORDER BY w.creature_name, w.item_entry;

-- D. Validate candidate registry items against the LOCAL item templates.
SELECT entry, name, class, subclass, InventoryType, RequiredLevel, ItemLevel
FROM item_template
WHERE entry IN (11815, 12587, 12604, 12930, 13404)
ORDER BY entry;
