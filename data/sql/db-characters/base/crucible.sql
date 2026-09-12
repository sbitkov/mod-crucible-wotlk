CREATE TABLE IF NOT EXISTS `character_crucible_absorption` (
    `guid` INT UNSIGNED NOT NULL,
    `item_entry` INT UNSIGNED NOT NULL,
    `absorbed_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`guid`, `item_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `character_crucible_contribution` (
    `guid` INT UNSIGNED NOT NULL,
    `item_entry` INT UNSIGNED NOT NULL,
    `stat_id` SMALLINT UNSIGNED NOT NULL,
    `source_value` DECIMAL(12,4) NOT NULL,
    `coefficient` DECIMAL(8,6) NOT NULL,
    `absorbed_value` DECIMAL(12,4) NOT NULL,
    PRIMARY KEY (`guid`, `item_entry`, `stat_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
