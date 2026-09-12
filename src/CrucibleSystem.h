#pragma once

#include "Define.h"

#include <vector>

class Item;
class ItemTemplate;
class Player;

namespace Crucible
{
    constexpr float BASE_COEFFICIENT = 0.20f;

    enum class StatId : uint16
    {
        STRENGTH = 1,
        AGILITY,
        STAMINA,
        INTELLECT,
        SPIRIT,

        HEALTH_REGEN,
        MANA_REGEN,

        DEFENSE_RATING,
        DODGE_RATING,
        PARRY_RATING,
        BLOCK_RATING,

        HIT_MELEE_RATING,
        HIT_RANGED_RATING,
        HIT_SPELL_RATING,
        HIT_RATING,

        CRIT_MELEE_RATING,
        CRIT_RANGED_RATING,
        CRIT_SPELL_RATING,
        CRIT_RATING,

        HASTE_MELEE_RATING,
        HASTE_RANGED_RATING,
        HASTE_SPELL_RATING,
        HASTE_RATING,

        EXPERTISE_RATING,
        RESILIENCE_RATING,
        ARMOR_PENETRATION_RATING,

        ATTACK_POWER,
        RANGED_ATTACK_POWER,
        SPELL_POWER,
        SPELL_PENETRATION,

        BLOCK_VALUE,
        ARMOR,

        HOLY_RESISTANCE,
        FIRE_RESISTANCE,
        NATURE_RESISTANCE,
        FROST_RESISTANCE,
        SHADOW_RESISTANCE,
        ARCANE_RESISTANCE,

        // Keep new persisted StatId values appended to the enum.
        // Existing character_crucible_contribution rows already store numeric IDs 1..38.
        HOLY_SPELL_POWER,
        FIRE_SPELL_POWER,
        NATURE_SPELL_POWER,
        FROST_SPELL_POWER,
        SHADOW_SPELL_POWER,
        ARCANE_SPELL_POWER
    };

    enum class EssenceType : uint8
    {
        BASE = 0,
        RANDOM_PROPERTY = 1,
        RANDOM_SUFFIX = 2
    };

    struct Contribution
    {
        StatId Stat;
        float SourceValue;
        float Coefficient;
        float AbsorbedValue;
    };

    struct EssenceComponent
    {
        EssenceType Type = EssenceType::BASE;
        uint32 AffixId = 0;
        std::vector<Contribution> Contributions;
    };

    struct AccumulatedStat
    {
        StatId Stat;
        float StoredValue;
        int32 AppliedValue;
    };

    enum class AbsorbResult : uint8
    {
        SUCCESS = 0,
        INVALID_ARGUMENT,
        ITEM_TEMPLATE_NOT_FOUND,
        QUALITY_TOO_LOW,
        ITEM_NOT_EQUIPMENT,
        ITEM_NOT_USABLE,
        ARMOR_TYPE_NOT_ALLOWED,
        WEAPON_TYPE_NOT_ALLOWED,
        WEAPON_UNSUPPORTED,
        ALREADY_ABSORBED,
        ITEM_IN_TRADE,
        NO_SUPPORTED_STATS,
        DESTROY_FAILED
    };

    enum class MasteryUpgradeResult : uint8
    {
        SUCCESS = 0,
        INVALID_ARGUMENT,
        ESSENCE_NOT_FOUND,
        INVALID_MASTERY_STATE,
        UNSUPPORTED_BRACKET,
        NOT_ENOUGH_MONEY,
        NOT_ENOUGH_REAGENT
    };

    struct MasteryCost
    {
        uint32 MoneyCopper = 0;
        uint32 ReagentEntry = 0;
        uint32 ReagentCount = 0;
        uint32 NextMastery = 0;
    };

    char const* GetStatName(StatId stat);

    std::vector<Contribution> ExtractContributions(ItemTemplate const* proto);

    // Extracts the independently-mastered BASE and random-affix components
    // from one concrete physical item instance. Components with no recognized
    // Crucible contributions are omitted.
    std::vector<EssenceComponent> ExtractEssenceComponents(Item* item);

    std::vector<AccumulatedStat> GetAccumulatedStats(Player* player);

    // Authoritative eligibility/contribution preview for a concrete physical Item.
    // Uses the same validation rules as AbsorbItem but does not mutate inventory or DB.
    // On SUCCESS, contributions contains the exact values that would be snapshotted.
    AbsorbResult PreviewItem(
        Player* player,
        Item* item,
        std::vector<Contribution>& contributions);

    // Authoritative real absorption path.
    // The caller must supply the concrete physical Item instance selected by the player.
    // On success exactly one unit of that Item is destroyed, its contribution snapshot is
    // committed, and the player's permanent Crucible bonuses are recalculated.
    AbsorbResult AbsorbItem(Player* player, Item* item);

    // Returns the configured cost for the next mastery step.
    // v0.5 currently defines economy only for RequiredLevel 1-19.
    bool GetMasteryUpgradeCost(
        ItemTemplate const* proto,
        uint32 currentMastery,
        MasteryCost& cost);

    // Advances exactly one stored BASE/affix essence by one mastery tier.
    // Component identity is (EssenceType, itemEntry, affixId).
    // No duplicate physical copy of the absorbed equipment is required.
    MasteryUpgradeResult UpgradeMastery(
        Player* player,
        EssenceType essenceType,
        uint32 itemEntry,
        uint32 affixId);

    // Applies the current aggregated Crucible state from DB.
    // Any Crucible bonuses previously tracked for this live Player are removed first.
    void Recalculate(Player* player);

    // Removes only the Crucible bonuses tracked for this live Player.
    void Unapply(Player* player);

    // Forget session bookkeeping when the Player object is going away.
    // Do not modify stats here; the Player object is being destroyed anyway.
    void ForgetSession(Player* player);

    bool IsApplied(Player* player);
}
