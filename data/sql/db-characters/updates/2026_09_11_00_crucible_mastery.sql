-- Crucible v0.5: stored essence mastery state.
-- Existing absorbed items become 20% mastery.
-- Safe to run more than once on MySQL 8.x.

SET @crucible_mastery_column_exists := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'character_crucible_absorption'
      AND COLUMN_NAME = 'mastery_percent'
);

SET @crucible_mastery_sql := IF(
    @crucible_mastery_column_exists = 0,
    'ALTER TABLE `character_crucible_absorption` ADD COLUMN `mastery_percent` TINYINT UNSIGNED NOT NULL DEFAULT 20 AFTER `item_entry`',
    'SELECT 1'
);

PREPARE crucible_mastery_stmt FROM @crucible_mastery_sql;
EXECUTE crucible_mastery_stmt;
DEALLOCATE PREPARE crucible_mastery_stmt;
