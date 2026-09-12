CREATE TABLE IF NOT EXISTS `character_crucible_absorption` (
    `guid` INT UNSIGNED NOT NULL,
    `essence_type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `item_entry` INT UNSIGNED NOT NULL,
    `affix_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `mastery_percent` TINYINT UNSIGNED NOT NULL DEFAULT 20,
    `absorbed_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`guid`, `essence_type`, `item_entry`, `affix_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `character_crucible_contribution` (
    `guid` INT UNSIGNED NOT NULL,
    `essence_type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `item_entry` INT UNSIGNED NOT NULL,
    `affix_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `stat_id` SMALLINT UNSIGNED NOT NULL,
    `source_value` DECIMAL(12,4) NOT NULL,
    `coefficient` DECIMAL(8,6) NOT NULL,
    `absorbed_value` DECIMAL(12,4) NOT NULL,
    PRIMARY KEY (`guid`, `essence_type`, `item_entry`, `affix_id`, `stat_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
