-- Crucible gameobject template.
-- Required so the registered C++ GameObjectScript "go_crucible"
-- is assigned to an actual world database template.

INSERT INTO `gameobject_template`
(
    `entry`,
    `type`,
    `displayId`,
    `name`,
    `size`,
    `AIName`,
    `ScriptName`,
    `VerifiedBuild`
)
VALUES
(
    900000,
    10,
    8304,
    'Crucible',
    1,
    '',
    'go_crucible',
    12340
)
ON DUPLICATE KEY UPDATE
    `type` = VALUES(`type`),
    `displayId` = VALUES(`displayId`),
    `name` = VALUES(`name`),
    `size` = VALUES(`size`),
    `AIName` = VALUES(`AIName`),
    `ScriptName` = VALUES(`ScriptName`),
    `VerifiedBuild` = VALUES(`VerifiedBuild`);
