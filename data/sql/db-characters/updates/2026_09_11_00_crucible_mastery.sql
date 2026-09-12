-- Crucible v0.5: stored essence mastery state.
-- Existing absorbed items become 20% mastery.
-- On a fresh install the base SQL creates the final schema, so this update
-- only migrates an already existing legacy table.

SET @crucible_mastery_table_exists := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.TABLES
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'character_crucible_absorption'
);

SET @crucible_mastery_column_exists := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'character_crucible_absorption'
      AND COLUMN_NAME = 'mastery_percent'
);

SET @crucible_mastery_sql := IF(
    @crucible_mastery_table_exists > 0
        AND @crucible_mastery_column_exists = 0,
    'ALTER TABLE `character_crucible_absorption` ADD COLUMN `mastery_percent` TINYINT UNSIGNED NOT NULL DEFAULT 20 AFTER `item_entry`',
    'SELECT 1'
);

PREPARE crucible_mastery_stmt FROM @crucible_mastery_sql;
EXECUTE crucible_mastery_stmt;
DEALLOCATE PREPARE crucible_mastery_stmt;
