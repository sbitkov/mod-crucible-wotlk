-- Crucible v0.7 preparation:
-- add component identity while preserving every existing essence as BASE.
-- On a fresh install the base SQL creates the final schema, so this update
-- only migrates legacy tables.

SET @crucible_absorption_needs_identity := (
    SELECT
        EXISTS (
            SELECT 1
            FROM INFORMATION_SCHEMA.TABLES
            WHERE TABLE_SCHEMA = DATABASE()
              AND TABLE_NAME = 'character_crucible_absorption'
        )
        AND NOT EXISTS (
            SELECT 1
            FROM INFORMATION_SCHEMA.COLUMNS
            WHERE TABLE_SCHEMA = DATABASE()
              AND TABLE_NAME = 'character_crucible_absorption'
              AND COLUMN_NAME = 'essence_type'
        )
);

SET @crucible_absorption_identity_sql := IF(
    @crucible_absorption_needs_identity,
    'ALTER TABLE `character_crucible_absorption` DROP PRIMARY KEY, ADD COLUMN `essence_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `guid`, ADD COLUMN `affix_id` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `item_entry`, ADD PRIMARY KEY (`guid`, `essence_type`, `item_entry`, `affix_id`)',
    'SELECT 1'
);

PREPARE crucible_absorption_identity_stmt
    FROM @crucible_absorption_identity_sql;
EXECUTE crucible_absorption_identity_stmt;
DEALLOCATE PREPARE crucible_absorption_identity_stmt;

SET @crucible_contribution_needs_identity := (
    SELECT
        EXISTS (
            SELECT 1
            FROM INFORMATION_SCHEMA.TABLES
            WHERE TABLE_SCHEMA = DATABASE()
              AND TABLE_NAME = 'character_crucible_contribution'
        )
        AND NOT EXISTS (
            SELECT 1
            FROM INFORMATION_SCHEMA.COLUMNS
            WHERE TABLE_SCHEMA = DATABASE()
              AND TABLE_NAME = 'character_crucible_contribution'
              AND COLUMN_NAME = 'essence_type'
        )
);

SET @crucible_contribution_identity_sql := IF(
    @crucible_contribution_needs_identity,
    'ALTER TABLE `character_crucible_contribution` DROP PRIMARY KEY, ADD COLUMN `essence_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `guid`, ADD COLUMN `affix_id` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `item_entry`, ADD PRIMARY KEY (`guid`, `essence_type`, `item_entry`, `affix_id`, `stat_id`)',
    'SELECT 1'
);

PREPARE crucible_contribution_identity_stmt
    FROM @crucible_contribution_identity_sql;
EXECUTE crucible_contribution_identity_stmt;
DEALLOCATE PREPARE crucible_contribution_identity_stmt;
