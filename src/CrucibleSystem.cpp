#include "CrucibleSystem.h"

#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "QueryResult.h"

#include <fmt/format.h>

#include <unordered_map>

namespace
{
    struct AppliedStat
    {
        Crucible::StatId Stat;
        int32 Amount;
    };

    std::unordered_map<uint32, std::vector<AppliedStat>> sAppliedStats;

    bool TryMapItemMod(uint32 itemMod, Crucible::StatId& result)
    {
        using Crucible::StatId;

        switch (itemMod)
        {
            case ITEM_MOD_STRENGTH: result = StatId::STRENGTH; return true;
            case ITEM_MOD_AGILITY: result = StatId::AGILITY; return true;
            case ITEM_MOD_STAMINA: result = StatId::STAMINA; return true;
            case ITEM_MOD_INTELLECT: result = StatId::INTELLECT; return true;
            case ITEM_MOD_SPIRIT: result = StatId::SPIRIT; return true;

            case ITEM_MOD_HEALTH_REGEN: result = StatId::HEALTH_REGEN; return true;
            case ITEM_MOD_MANA_REGENERATION: result = StatId::MANA_REGEN; return true;

            case ITEM_MOD_DEFENSE_SKILL_RATING: result = StatId::DEFENSE_RATING; return true;
            case ITEM_MOD_DODGE_RATING: result = StatId::DODGE_RATING; return true;
            case ITEM_MOD_PARRY_RATING: result = StatId::PARRY_RATING; return true;
            case ITEM_MOD_BLOCK_RATING: result = StatId::BLOCK_RATING; return true;

            case ITEM_MOD_HIT_MELEE_RATING: result = StatId::HIT_MELEE_RATING; return true;
            case ITEM_MOD_HIT_RANGED_RATING: result = StatId::HIT_RANGED_RATING; return true;
            case ITEM_MOD_HIT_SPELL_RATING: result = StatId::HIT_SPELL_RATING; return true;
            case ITEM_MOD_HIT_RATING: result = StatId::HIT_RATING; return true;

            case ITEM_MOD_CRIT_MELEE_RATING: result = StatId::CRIT_MELEE_RATING; return true;
            case ITEM_MOD_CRIT_RANGED_RATING: result = StatId::CRIT_RANGED_RATING; return true;
            case ITEM_MOD_CRIT_SPELL_RATING: result = StatId::CRIT_SPELL_RATING; return true;
            case ITEM_MOD_CRIT_RATING: result = StatId::CRIT_RATING; return true;

            case ITEM_MOD_HASTE_MELEE_RATING: result = StatId::HASTE_MELEE_RATING; return true;
            case ITEM_MOD_HASTE_RANGED_RATING: result = StatId::HASTE_RANGED_RATING; return true;
            case ITEM_MOD_HASTE_SPELL_RATING: result = StatId::HASTE_SPELL_RATING; return true;
            case ITEM_MOD_HASTE_RATING: result = StatId::HASTE_RATING; return true;

            case ITEM_MOD_EXPERTISE_RATING: result = StatId::EXPERTISE_RATING; return true;
            case ITEM_MOD_RESILIENCE_RATING: result = StatId::RESILIENCE_RATING; return true;
            case ITEM_MOD_ARMOR_PENETRATION_RATING: result = StatId::ARMOR_PENETRATION_RATING; return true;

            case ITEM_MOD_ATTACK_POWER: result = StatId::ATTACK_POWER; return true;
            case ITEM_MOD_RANGED_ATTACK_POWER: result = StatId::RANGED_ATTACK_POWER; return true;
            case ITEM_MOD_SPELL_POWER: result = StatId::SPELL_POWER; return true;
            case ITEM_MOD_SPELL_PENETRATION: result = StatId::SPELL_PENETRATION; return true;

            case ITEM_MOD_BLOCK_VALUE: result = StatId::BLOCK_VALUE; return true;

            default:
                return false;
        }
    }

    void AddContribution(
        std::vector<Crucible::Contribution>& contributions,
        Crucible::StatId stat,
        float sourceValue)
    {
        if (sourceValue == 0.0f)
            return;

        for (Crucible::Contribution& existing : contributions)
        {
            if (existing.Stat != stat)
                continue;

            existing.SourceValue += sourceValue;
            existing.AbsorbedValue = existing.SourceValue * existing.Coefficient;
            return;
        }

        Crucible::Contribution contribution;
        contribution.Stat = stat;
        contribution.SourceValue = sourceValue;
        contribution.Coefficient = Crucible::BASE_COEFFICIENT;
        contribution.AbsorbedValue = sourceValue * contribution.Coefficient;
        contributions.push_back(contribution);
    }

    void ApplyStat(Player* player, Crucible::StatId stat, int32 amount, bool apply)
    {
        using Crucible::StatId;

        if (!player || amount == 0)
            return;

        switch (stat)
        {
            case StatId::STRENGTH:
                player->HandleStatFlatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(amount), apply);
                player->UpdateStatBuffMod(STAT_STRENGTH);
                break;
            case StatId::AGILITY:
                player->HandleStatFlatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(amount), apply);
                player->UpdateStatBuffMod(STAT_AGILITY);
                break;
            case StatId::STAMINA:
                player->HandleStatFlatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(amount), apply);
                player->UpdateStatBuffMod(STAT_STAMINA);
                break;
            case StatId::INTELLECT:
                player->HandleStatFlatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(amount), apply);
                player->UpdateStatBuffMod(STAT_INTELLECT);
                break;
            case StatId::SPIRIT:
                player->HandleStatFlatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(amount), apply);
                player->UpdateStatBuffMod(STAT_SPIRIT);
                break;

            case StatId::HEALTH_REGEN:
                player->ApplyHealthRegenBonus(amount, apply);
                break;
            case StatId::MANA_REGEN:
                player->ApplyManaRegenBonus(amount, apply);
                break;

            case StatId::DEFENSE_RATING:
                player->ApplyRatingMod(CR_DEFENSE_SKILL, amount, apply);
                break;
            case StatId::DODGE_RATING:
                player->ApplyRatingMod(CR_DODGE, amount, apply);
                break;
            case StatId::PARRY_RATING:
                player->ApplyRatingMod(CR_PARRY, amount, apply);
                break;
            case StatId::BLOCK_RATING:
                player->ApplyRatingMod(CR_BLOCK, amount, apply);
                break;

            case StatId::HIT_MELEE_RATING:
                player->ApplyRatingMod(CR_HIT_MELEE, amount, apply);
                break;
            case StatId::HIT_RANGED_RATING:
                player->ApplyRatingMod(CR_HIT_RANGED, amount, apply);
                break;
            case StatId::HIT_SPELL_RATING:
                player->ApplyRatingMod(CR_HIT_SPELL, amount, apply);
                break;
            case StatId::HIT_RATING:
                player->ApplyRatingMod(CR_HIT_MELEE, amount, apply);
                player->ApplyRatingMod(CR_HIT_RANGED, amount, apply);
                player->ApplyRatingMod(CR_HIT_SPELL, amount, apply);
                break;

            case StatId::CRIT_MELEE_RATING:
                player->ApplyRatingMod(CR_CRIT_MELEE, amount, apply);
                break;
            case StatId::CRIT_RANGED_RATING:
                player->ApplyRatingMod(CR_CRIT_RANGED, amount, apply);
                break;
            case StatId::CRIT_SPELL_RATING:
                player->ApplyRatingMod(CR_CRIT_SPELL, amount, apply);
                break;
            case StatId::CRIT_RATING:
                player->ApplyRatingMod(CR_CRIT_MELEE, amount, apply);
                player->ApplyRatingMod(CR_CRIT_RANGED, amount, apply);
                player->ApplyRatingMod(CR_CRIT_SPELL, amount, apply);
                break;

            case StatId::HASTE_MELEE_RATING:
                player->ApplyRatingMod(CR_HASTE_MELEE, amount, apply);
                break;
            case StatId::HASTE_RANGED_RATING:
                player->ApplyRatingMod(CR_HASTE_RANGED, amount, apply);
                break;
            case StatId::HASTE_SPELL_RATING:
                player->ApplyRatingMod(CR_HASTE_SPELL, amount, apply);
                break;
            case StatId::HASTE_RATING:
                player->ApplyRatingMod(CR_HASTE_MELEE, amount, apply);
                player->ApplyRatingMod(CR_HASTE_RANGED, amount, apply);
                player->ApplyRatingMod(CR_HASTE_SPELL, amount, apply);
                break;

            case StatId::EXPERTISE_RATING:
                player->ApplyRatingMod(CR_EXPERTISE, amount, apply);
                break;

            case StatId::RESILIENCE_RATING:
                player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, amount, apply);
                player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, amount, apply);
                player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, amount, apply);
                break;

            case StatId::ARMOR_PENETRATION_RATING:
                player->ApplyRatingMod(CR_ARMOR_PENETRATION, amount, apply);
                break;

            case StatId::ATTACK_POWER:
                player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(amount), apply);
                player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(amount), apply);
                break;
            case StatId::RANGED_ATTACK_POWER:
                player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(amount), apply);
                break;

            case StatId::SPELL_POWER:
                player->ApplySpellPowerBonus(amount, apply);
                break;
            case StatId::SPELL_PENETRATION:
                player->ApplySpellPenetrationBonus(amount, apply);
                break;

            case StatId::BLOCK_VALUE:
                player->HandleBaseModFlatValue(SHIELD_BLOCK_VALUE, float(amount), apply);
                break;

            case StatId::ARMOR:
                player->HandleStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(amount), apply);
                break;

            case StatId::HOLY_RESISTANCE:
                player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(amount), apply);
                break;
            case StatId::FIRE_RESISTANCE:
                player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(amount), apply);
                break;
            case StatId::NATURE_RESISTANCE:
                player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(amount), apply);
                break;
            case StatId::FROST_RESISTANCE:
                player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(amount), apply);
                break;
            case StatId::SHADOW_RESISTANCE:
                player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(amount), apply);
                break;
            case StatId::ARCANE_RESISTANCE:
                player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(amount), apply);
                break;
        }
    }

    bool HasAbsorbedItem(uint32 guid, uint32 itemEntry)
    {
        QueryResult existing = CharacterDatabase.Query(
            "SELECT 1 FROM character_crucible_absorption "
            "WHERE guid = {} AND item_entry = {} LIMIT 1",
            guid,
            itemEntry
        );

        return bool(existing);
    }

    CharacterDatabaseTransaction BuildAbsorptionTransaction(
        uint32 guid,
        uint32 itemEntry,
        std::vector<Crucible::Contribution> const& contributions)
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        CharacterDatabase.ExecuteOrAppend(
            trans,
            fmt::format(
                "INSERT INTO character_crucible_absorption "
                "(guid, item_entry) VALUES ({}, {})",
                guid,
                itemEntry
            )
        );

        for (Crucible::Contribution const& contribution : contributions)
        {
            CharacterDatabase.ExecuteOrAppend(
                trans,
                fmt::format(
                    "INSERT INTO character_crucible_contribution "
                    "(guid, item_entry, stat_id, source_value, coefficient, absorbed_value) "
                    "VALUES ({}, {}, {}, {:.4f}, {:.6f}, {:.4f})",
                    guid,
                    itemEntry,
                    static_cast<uint16>(contribution.Stat),
                    contribution.SourceValue,
                    contribution.Coefficient,
                    contribution.AbsorbedValue
                )
            );
        }

        return trans;
    }

    void RemoveAbsorptionRowsSynchronously(uint32 guid, uint32 itemEntry)
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        CharacterDatabase.ExecuteOrAppend(
            trans,
            fmt::format(
                "DELETE FROM character_crucible_contribution "
                "WHERE guid = {} AND item_entry = {}",
                guid,
                itemEntry
            )
        );

        CharacterDatabase.ExecuteOrAppend(
            trans,
            fmt::format(
                "DELETE FROM character_crucible_absorption "
                "WHERE guid = {} AND item_entry = {}",
                guid,
                itemEntry
            )
        );

        CharacterDatabase.DirectCommitTransaction(trans);
    }
}

namespace Crucible
{
    char const* GetStatName(StatId stat)
    {
        switch (stat)
        {
            case StatId::STRENGTH: return "Strength";
            case StatId::AGILITY: return "Agility";
            case StatId::STAMINA: return "Stamina";
            case StatId::INTELLECT: return "Intellect";
            case StatId::SPIRIT: return "Spirit";
            case StatId::HEALTH_REGEN: return "HP5";
            case StatId::MANA_REGEN: return "MP5";
            case StatId::DEFENSE_RATING: return "Defense Rating";
            case StatId::DODGE_RATING: return "Dodge Rating";
            case StatId::PARRY_RATING: return "Parry Rating";
            case StatId::BLOCK_RATING: return "Block Rating";
            case StatId::HIT_MELEE_RATING: return "Melee Hit Rating";
            case StatId::HIT_RANGED_RATING: return "Ranged Hit Rating";
            case StatId::HIT_SPELL_RATING: return "Spell Hit Rating";
            case StatId::HIT_RATING: return "Hit Rating";
            case StatId::CRIT_MELEE_RATING: return "Melee Crit Rating";
            case StatId::CRIT_RANGED_RATING: return "Ranged Crit Rating";
            case StatId::CRIT_SPELL_RATING: return "Spell Crit Rating";
            case StatId::CRIT_RATING: return "Crit Rating";
            case StatId::HASTE_MELEE_RATING: return "Melee Haste Rating";
            case StatId::HASTE_RANGED_RATING: return "Ranged Haste Rating";
            case StatId::HASTE_SPELL_RATING: return "Spell Haste Rating";
            case StatId::HASTE_RATING: return "Haste Rating";
            case StatId::EXPERTISE_RATING: return "Expertise Rating";
            case StatId::RESILIENCE_RATING: return "Resilience Rating";
            case StatId::ARMOR_PENETRATION_RATING: return "Armor Penetration Rating";
            case StatId::ATTACK_POWER: return "Attack Power";
            case StatId::RANGED_ATTACK_POWER: return "Ranged Attack Power";
            case StatId::SPELL_POWER: return "Spell Power";
            case StatId::SPELL_PENETRATION: return "Spell Penetration";
            case StatId::BLOCK_VALUE: return "Block Value";
            case StatId::ARMOR: return "Armor";
            case StatId::HOLY_RESISTANCE: return "Holy Resistance";
            case StatId::FIRE_RESISTANCE: return "Fire Resistance";
            case StatId::NATURE_RESISTANCE: return "Nature Resistance";
            case StatId::FROST_RESISTANCE: return "Frost Resistance";
            case StatId::SHADOW_RESISTANCE: return "Shadow Resistance";
            case StatId::ARCANE_RESISTANCE: return "Arcane Resistance";
        }

        return "Unknown";
    }

    std::vector<Contribution> ExtractContributions(ItemTemplate const* proto)
    {
        std::vector<Contribution> contributions;

        if (!proto)
            return contributions;

        for (uint32 i = 0; i < proto->StatsCount; ++i)
        {
            uint32 itemMod = proto->ItemStat[i].ItemStatType;
            int32 sourceValue = proto->ItemStat[i].ItemStatValue;

            if (sourceValue == 0)
                continue;

            StatId stat;
            if (TryMapItemMod(itemMod, stat))
                AddContribution(contributions, stat, float(sourceValue));
        }

        AddContribution(contributions, StatId::ARMOR, float(proto->Armor));
        AddContribution(contributions, StatId::HOLY_RESISTANCE, float(proto->HolyRes));
        AddContribution(contributions, StatId::FIRE_RESISTANCE, float(proto->FireRes));
        AddContribution(contributions, StatId::NATURE_RESISTANCE, float(proto->NatureRes));
        AddContribution(contributions, StatId::FROST_RESISTANCE, float(proto->FrostRes));
        AddContribution(contributions, StatId::SHADOW_RESISTANCE, float(proto->ShadowRes));
        AddContribution(contributions, StatId::ARCANE_RESISTANCE, float(proto->ArcaneRes));

        return contributions;
    }

    AbsorbResult PreviewItem(
        Player* player,
        Item* item,
        std::vector<Contribution>& contributions)
    {
        contributions.clear();

        if (!player || !item)
            return AbsorbResult::INVALID_ARGUMENT;

        const uint32 itemEntry = item->GetEntry();
        ItemTemplate const* proto = item->GetTemplate();

        if (!proto)
            return AbsorbResult::ITEM_TEMPLATE_NOT_FOUND;

        if (proto->Class == ITEM_CLASS_WEAPON)
            return AbsorbResult::WEAPON_UNSUPPORTED;

        const uint32 guid = player->GetGUID().GetCounter();

        if (HasAbsorbedItem(guid, itemEntry))
            return AbsorbResult::ALREADY_ABSORBED;

        if (item->IsInTrade())
            return AbsorbResult::ITEM_IN_TRADE;

        contributions = ExtractContributions(proto);
        if (contributions.empty())
            return AbsorbResult::NO_SUPPORTED_STATS;

        return AbsorbResult::SUCCESS;
    }

    AbsorbResult AbsorbItem(Player* player, Item* item)
    {
        std::vector<Contribution> contributions;
        AbsorbResult previewResult = PreviewItem(player, item, contributions);

        if (previewResult != AbsorbResult::SUCCESS)
            return previewResult;

        const uint32 itemEntry = item->GetEntry();
        const uint32 guid = player->GetGUID().GetCounter();

        CharacterDatabaseTransaction trans =
            BuildAbsorptionTransaction(guid, itemEntry, contributions);
        CharacterDatabase.DirectCommitTransaction(trans);

        // Destroy exactly one unit from the concrete Item* supplied by the caller.
        uint32 destroyCount = 1;
        player->DestroyItemCount(item, destroyCount, true);

        // DB cannot be atomic with the in-memory inventory operation. If destruction
        // unexpectedly fails, remove the just-committed snapshot immediately so the
        // player can never retain both the item and the permanent gain.
        if (destroyCount != 0)
        {
            RemoveAbsorptionRowsSynchronously(guid, itemEntry);
            return AbsorbResult::DESTROY_FAILED;
        }

        Recalculate(player);
        return AbsorbResult::SUCCESS;
    }

    void Unapply(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();

        auto itr = sAppliedStats.find(guid);
        if (itr == sAppliedStats.end())
            return;

        for (AppliedStat const& applied : itr->second)
            ApplyStat(player, applied.Stat, applied.Amount, false);

        sAppliedStats.erase(itr);
    }

    void Recalculate(Player* player)
    {
        if (!player)
            return;

        Unapply(player);

        uint32 guid = player->GetGUID().GetCounter();

        QueryResult result = CharacterDatabase.Query(
            "SELECT stat_id, SUM(absorbed_value) "
            "FROM character_crucible_contribution "
            "WHERE guid = {} "
            "GROUP BY stat_id "
            "ORDER BY stat_id",
            guid
        );

        std::vector<AppliedStat> appliedStats;

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();

                StatId stat = static_cast<StatId>(fields[0].Get<uint16>());
                float stored = fields[1].Get<float>();
                int32 amount = static_cast<int32>(stored);

                if (amount == 0)
                    continue;

                ApplyStat(player, stat, amount, true);
                appliedStats.push_back({ stat, amount });
            }
            while (result->NextRow());
        }

        sAppliedStats[guid] = std::move(appliedStats);
    }

    void ForgetSession(Player* player)
    {
        if (!player)
            return;

        sAppliedStats.erase(player->GetGUID().GetCounter());
    }

    bool IsApplied(Player* player)
    {
        if (!player)
            return false;

        return sAppliedStats.find(player->GetGUID().GetCounter()) != sAppliedStats.end();
    }
}
