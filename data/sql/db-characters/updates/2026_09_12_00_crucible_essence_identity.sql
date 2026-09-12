-- Crucible v0.7 preparation:
-- add component identity while preserving every existing essence as BASE.

ALTER TABLE `character_crucible_absorption`
    DROP PRIMARY KEY,
    ADD COLUMN `essence_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `guid`,
    ADD COLUMN `affix_id` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `item_entry`,
    ADD PRIMARY KEY (`guid`, `essence_type`, `item_entry`, `affix_id`);

ALTER TABLE `character_crucible_contribution`
    DROP PRIMARY KEY,
    ADD COLUMN `essence_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `guid`,
    ADD COLUMN `affix_id` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `item_entry`,
    ADD PRIMARY KEY (`guid`, `essence_type`, `item_entry`, `affix_id`, `stat_id`);
