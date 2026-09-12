#include "CrucibleSystem.h"

#include "DatabaseEnv.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
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

    void ExtractEnchantContributions(
        SpellItemEnchantmentEntry const* enchant,
        float suffixScaledValue,
        std::vector<Crucible::Contribution>& contributions)
    {
        if (!enchant)
            return;

        for (uint32 effectIndex = 0;
             effectIndex < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS;
             ++effectIndex)
        {
            const uint32 type = enchant->type[effectIndex];
            const uint32 arg = enchant->spellid[effectIndex];

            if (type == ITEM_ENCHANTMENT_TYPE_STAT)
            {
                Crucible::StatId stat;
                if (!TryMapItemMod(arg, stat))
                    continue;

                float sourceValue = float(enchant->amount[effectIndex]);

                // RandomSuffix stat enchants commonly store amount=0 and derive
                // the instance value from AllocationPct * suffixFactor / 10000.
                if (sourceValue == 0.0f && suffixScaledValue != 0.0f)
                    sourceValue = suffixScaledValue;

                AddContribution(contributions, stat, sourceValue);
                continue;
            }

        }
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

            case StatId::HOLY_SPELL_POWER:
            case StatId::FIRE_SPELL_POWER:
            case StatId::NATURE_SPELL_POWER:
            case StatId::FROST_SPELL_POWER:
            case StatId::SHADOW_SPELL_POWER:
            case StatId::ARCANE_SPELL_POWER:
                // Reserved for future school-specific spell power support.
                // These values are intentionally not applied in v0.7.
                break;
        }
    }

    bool IsV04RangedWeaponSubclass(uint32 subClass)
    {
        switch (subClass)
        {
            case ITEM_SUBCLASS_WEAPON_BOW:
            case ITEM_SUBCLASS_WEAPON_GUN:
            case ITEM_SUBCLASS_WEAPON_CROSSBOW:
            case ITEM_SUBCLASS_WEAPON_THROWN:
            case ITEM_SUBCLASS_WEAPON_WAND:
                return true;
            default:
                return false;
        }
    }

    uint32 GetHistoricalArmorSubclass(Player const* player, ItemTemplate const* proto)
    {
        if (!player || !proto)
            return ITEM_SUBCLASS_ARMOR_MISC;

        const bool level40Plus = proto->RequiredLevel >= 40;

        switch (player->getClass())
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
                return level40Plus
                    ? ITEM_SUBCLASS_ARMOR_PLATE
                    : ITEM_SUBCLASS_ARMOR_MAIL;

            case CLASS_HUNTER:
            case CLASS_SHAMAN:
                return level40Plus
                    ? ITEM_SUBCLASS_ARMOR_MAIL
                    : ITEM_SUBCLASS_ARMOR_LEATHER;

            case CLASS_ROGUE:
            case CLASS_DRUID:
                return ITEM_SUBCLASS_ARMOR_LEATHER;

            case CLASS_MAGE:
            case CLASS_PRIEST:
            case CLASS_WARLOCK:
                return ITEM_SUBCLASS_ARMOR_CLOTH;

            case CLASS_DEATH_KNIGHT:
                return ITEM_SUBCLASS_ARMOR_PLATE;

            default:
                return ITEM_SUBCLASS_ARMOR_MISC;
        }
    }

    Crucible::AbsorbResult CheckV04Eligibility(
        Player* player,
        Item* item,
        ItemTemplate const* proto)
    {
        using Crucible::AbsorbResult;

        if (!player || !item || !proto)
            return AbsorbResult::INVALID_ARGUMENT;

        if (proto->Quality < ITEM_QUALITY_UNCOMMON)
            return AbsorbResult::QUALITY_TOO_LOW;

        if (proto->InventoryType == INVTYPE_NON_EQUIP)
            return AbsorbResult::ITEM_NOT_EQUIPMENT;

        if (proto->Class != ITEM_CLASS_ARMOR && proto->Class != ITEM_CLASS_WEAPON)
            return AbsorbResult::ITEM_NOT_EQUIPMENT;

        if (player->CanUseItem(proto) != EQUIP_ERR_OK)
            return AbsorbResult::ITEM_NOT_USABLE;

        if (proto->Class == ITEM_CLASS_WEAPON)
        {
            if (!IsV04RangedWeaponSubclass(proto->SubClass))
                return AbsorbResult::WEAPON_UNSUPPORTED;

            const uint32 weaponSkill = item->GetSkill();
            if (weaponSkill != 0 && player->GetSkillValue(weaponSkill) == 0)
                return AbsorbResult::WEAPON_TYPE_NOT_ALLOWED;

            return AbsorbResult::SUCCESS;
        }

        if (proto->InventoryType == INVTYPE_CLOAK)
            return AbsorbResult::SUCCESS;

        if (proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
        {
            if (player->IsClass(CLASS_PALADIN, CLASS_CONTEXT_EQUIP_SHIELDS) ||
                player->IsClass(CLASS_WARRIOR, CLASS_CONTEXT_EQUIP_SHIELDS) ||
                player->IsClass(CLASS_SHAMAN, CLASS_CONTEXT_EQUIP_SHIELDS))
            {
                return AbsorbResult::SUCCESS;
            }

            return AbsorbResult::ARMOR_TYPE_NOT_ALLOWED;
        }

        // Relic class restrictions are already enforced by Player::CanUseItem().
        if (proto->InventoryType == INVTYPE_RELIC)
            return AbsorbResult::SUCCESS;

        if (proto->SubClass == ITEM_SUBCLASS_ARMOR_MISC)
            return AbsorbResult::SUCCESS;

        switch (proto->SubClass)
        {
            case ITEM_SUBCLASS_ARMOR_CLOTH:
            case ITEM_SUBCLASS_ARMOR_LEATHER:
            case ITEM_SUBCLASS_ARMOR_MAIL:
            case ITEM_SUBCLASS_ARMOR_PLATE:
                break;
            default:
                return AbsorbResult::ARMOR_TYPE_NOT_ALLOWED;
        }

        const uint32 expectedSubclass =
            GetHistoricalArmorSubclass(player, proto);

        if (proto->SubClass != expectedSubclass)
            return AbsorbResult::ARMOR_TYPE_NOT_ALLOWED;

        return AbsorbResult::SUCCESS;
    }
    bool HasAbsorbedComponent(
        uint32 guid,
        uint32 itemEntry,
        Crucible::EssenceType essenceType,
        uint32 affixId)
    {
        QueryResult existing = CharacterDatabase.Query(
            "SELECT 1 FROM character_crucible_absorption "
            "WHERE guid = {} AND essence_type = {} AND item_entry = {} "
            "AND affix_id = {} LIMIT 1",
            guid,
            static_cast<uint32>(essenceType),
            itemEntry,
            affixId
        );

        return bool(existing);
    }

    std::vector<Crucible::EssenceComponent> GetNewEssenceComponents(
        uint32 guid,
        uint32 itemEntry,
        std::vector<Crucible::EssenceComponent> const& extracted)
    {
        std::vector<Crucible::EssenceComponent> result;

        for (Crucible::EssenceComponent const& component : extracted)
        {
            if (component.Contributions.empty())
                continue;

            if (HasAbsorbedComponent(
                    guid,
                    itemEntry,
                    component.Type,
                    component.AffixId))
            {
                continue;
            }

            result.push_back(component);
        }

        return result;
    }

    CharacterDatabaseTransaction BuildAbsorptionTransaction(
        uint32 guid,
        uint32 itemEntry,
        std::vector<Crucible::EssenceComponent> const& components)
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        for (Crucible::EssenceComponent const& component : components)
        {
            CharacterDatabase.ExecuteOrAppend(
                trans,
                fmt::format(
                    "INSERT INTO character_crucible_absorption "
                    "(guid, essence_type, item_entry, affix_id, mastery_percent) "
                    "VALUES ({}, {}, {}, {}, 20)",
                    guid,
                    static_cast<uint32>(component.Type),
                    itemEntry,
                    component.AffixId
                )
            );

            for (Crucible::Contribution const& contribution :
                 component.Contributions)
            {
                CharacterDatabase.ExecuteOrAppend(
                    trans,
                    fmt::format(
                        "INSERT INTO character_crucible_contribution "
                        "(guid, essence_type, item_entry, affix_id, stat_id, "
                        "source_value, coefficient, absorbed_value) "
                        "VALUES ({}, {}, {}, {}, {}, {:.4f}, {:.6f}, {:.4f})",
                        guid,
                        static_cast<uint32>(component.Type),
                        itemEntry,
                        component.AffixId,
                        static_cast<uint16>(contribution.Stat),
                        contribution.SourceValue,
                        contribution.Coefficient,
                        contribution.AbsorbedValue
                    )
                );
            }
        }

        return trans;
    }

    void RemoveAbsorptionRowsSynchronously(
        uint32 guid,
        uint32 itemEntry,
        std::vector<Crucible::EssenceComponent> const& components)
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        for (Crucible::EssenceComponent const& component : components)
        {
            CharacterDatabase.ExecuteOrAppend(
                trans,
                fmt::format(
                    "DELETE FROM character_crucible_contribution "
                    "WHERE guid = {} AND essence_type = {} AND item_entry = {} "
                    "AND affix_id = {}",
                    guid,
                    static_cast<uint32>(component.Type),
                    itemEntry,
                    component.AffixId
                )
            );

            CharacterDatabase.ExecuteOrAppend(
                trans,
                fmt::format(
                    "DELETE FROM character_crucible_absorption "
                    "WHERE guid = {} AND essence_type = {} AND item_entry = {} "
                    "AND affix_id = {}",
                    guid,
                    static_cast<uint32>(component.Type),
                    itemEntry,
                    component.AffixId
                )
            );
        }

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
            case StatId::HOLY_SPELL_POWER: return "Holy Spell Power";
            case StatId::FIRE_SPELL_POWER: return "Fire Spell Power";
            case StatId::NATURE_SPELL_POWER: return "Nature Spell Power";
            case StatId::FROST_SPELL_POWER: return "Frost Spell Power";
            case StatId::SHADOW_SPELL_POWER: return "Shadow Spell Power";
            case StatId::ARCANE_SPELL_POWER: return "Arcane Spell Power";
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

        if (proto->Class == ITEM_CLASS_ARMOR &&
            proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
        {
            if (proto->Armor != 0)
            {
                Crucible::Contribution shieldArmor;
                shieldArmor.Stat = StatId::ARMOR;
                shieldArmor.SourceValue = float(proto->Armor);
                shieldArmor.Coefficient = 0.04f;
                shieldArmor.AbsorbedValue =
                    shieldArmor.SourceValue * shieldArmor.Coefficient;
                contributions.push_back(shieldArmor);
            }
        }
        else
        {
            AddContribution(contributions, StatId::ARMOR, float(proto->Armor));
        }
        AddContribution(contributions, StatId::HOLY_RESISTANCE, float(proto->HolyRes));
        AddContribution(contributions, StatId::FIRE_RESISTANCE, float(proto->FireRes));
        AddContribution(contributions, StatId::NATURE_RESISTANCE, float(proto->NatureRes));
        AddContribution(contributions, StatId::FROST_RESISTANCE, float(proto->FrostRes));
        AddContribution(contributions, StatId::SHADOW_RESISTANCE, float(proto->ShadowRes));
        AddContribution(contributions, StatId::ARCANE_RESISTANCE, float(proto->ArcaneRes));

        return contributions;
    }

    std::vector<EssenceComponent> ExtractEssenceComponents(Item* item)
    {
        std::vector<EssenceComponent> components;

        if (!item)
            return components;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return components;

        std::vector<Contribution> baseContributions =
            ExtractContributions(proto);

        if (!baseContributions.empty())
        {
            EssenceComponent base;
            base.Type = EssenceType::BASE;
            base.AffixId = 0;
            base.Contributions = std::move(baseContributions);
            components.push_back(std::move(base));
        }

        const int32 randomPropertyId = item->GetItemRandomPropertyId();
        if (randomPropertyId == 0)
            return components;

        EssenceComponent affix;

        if (randomPropertyId > 0)
        {
            affix.Type = EssenceType::RANDOM_PROPERTY;
            affix.AffixId = static_cast<uint32>(randomPropertyId);

            ItemRandomPropertiesEntry const* property =
                sItemRandomPropertiesStore.LookupEntry(affix.AffixId);

            if (!property)
                return components;

            for (uint32 i = 0; i < MAX_ITEM_ENCHANTMENT_EFFECTS; ++i)
            {
                const uint32 enchantId = property->Enchantment[i];
                if (enchantId == 0)
                    continue;

                SpellItemEnchantmentEntry const* enchant =
                    sSpellItemEnchantmentStore.LookupEntry(enchantId);

                ExtractEnchantContributions(
                    enchant,
                    0.0f,
                    affix.Contributions);
            }
        }
        else
        {
            affix.Type = EssenceType::RANDOM_SUFFIX;
            affix.AffixId = static_cast<uint32>(-randomPropertyId);

            ItemRandomSuffixEntry const* suffix =
                sItemRandomSuffixStore.LookupEntry(affix.AffixId);

            if (!suffix)
                return components;

            const uint32 suffixFactor = item->GetItemSuffixFactor();

            for (uint32 i = 0; i < MAX_ITEM_ENCHANTMENT_EFFECTS; ++i)
            {
                const uint32 enchantId = suffix->Enchantment[i];
                if (enchantId == 0)
                    continue;

                const float scaledValue =
                    suffixFactor != 0
                        ? float(
                            (static_cast<uint64>(suffix->AllocationPct[i]) *
                             static_cast<uint64>(suffixFactor)) / 10000)
                        : 0.0f;

                SpellItemEnchantmentEntry const* enchant =
                    sSpellItemEnchantmentStore.LookupEntry(enchantId);

                ExtractEnchantContributions(
                    enchant,
                    scaledValue,
                    affix.Contributions);
            }
        }

        // Unknown/unsupported affix effects do not create an essence component.
        if (!affix.Contributions.empty())
            components.push_back(std::move(affix));

        return components;
    }

    std::vector<AccumulatedStat> GetAccumulatedStats(Player* player)
    {
        std::vector<AccumulatedStat> stats;

        if (!player)
            return stats;

        uint32 guid = player->GetGUID().GetCounter();

        QueryResult result = CharacterDatabase.Query(
            "SELECT stat_id, SUM(absorbed_value) "
            "FROM character_crucible_contribution "
            "WHERE guid = {} "
            "GROUP BY stat_id "
            "ORDER BY stat_id",
            guid
        );

        if (!result)
            return stats;

        do
        {
            Field* fields = result->Fetch();

            StatId stat = static_cast<StatId>(fields[0].Get<uint16>());
            float total = fields[1].Get<float>();

            stats.push_back({
                stat,
                total,
                static_cast<int32>(total)
            });
        }
        while (result->NextRow());

        return stats;
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

        AbsorbResult eligibility = CheckV04Eligibility(player, item, proto);
        if (eligibility != AbsorbResult::SUCCESS)
            return eligibility;

        if (item->IsInTrade())
            return AbsorbResult::ITEM_IN_TRADE;

        const uint32 guid = player->GetGUID().GetCounter();

        std::vector<EssenceComponent> extracted =
            ExtractEssenceComponents(item);

        if (extracted.empty())
            return AbsorbResult::NO_SUPPORTED_STATS;

        std::vector<EssenceComponent> newComponents =
            GetNewEssenceComponents(guid, itemEntry, extracted);

        if (newComponents.empty())
            return AbsorbResult::ALREADY_ABSORBED;

        // The current client protocol is still a flat preview. Show only the
        // contributions that this concrete absorption would newly unlock.
        for (EssenceComponent const& component : newComponents)
        {
            for (Contribution const& contribution : component.Contributions)
            {
                bool merged = false;

                for (Contribution& existing : contributions)
                {
                    if (existing.Stat != contribution.Stat)
                        continue;

                    existing.SourceValue += contribution.SourceValue;
                    existing.AbsorbedValue += contribution.AbsorbedValue;
                    merged = true;
                    break;
                }

                if (!merged)
                    contributions.push_back(contribution);
            }
        }

        return contributions.empty()
            ? AbsorbResult::NO_SUPPORTED_STATS
            : AbsorbResult::SUCCESS;
    }

    AbsorbResult AbsorbItem(Player* player, Item* item)
    {
        if (!player || !item)
            return AbsorbResult::INVALID_ARGUMENT;

        ItemTemplate const* proto = item->GetTemplate();
        if (!proto)
            return AbsorbResult::ITEM_TEMPLATE_NOT_FOUND;

        AbsorbResult eligibility = CheckV04Eligibility(player, item, proto);
        if (eligibility != AbsorbResult::SUCCESS)
            return eligibility;

        if (item->IsInTrade())
            return AbsorbResult::ITEM_IN_TRADE;

        const uint32 itemEntry = item->GetEntry();
        const uint32 guid = player->GetGUID().GetCounter();

        std::vector<EssenceComponent> extracted =
            ExtractEssenceComponents(item);

        if (extracted.empty())
            return AbsorbResult::NO_SUPPORTED_STATS;

        std::vector<EssenceComponent> newComponents =
            GetNewEssenceComponents(guid, itemEntry, extracted);

        // Nothing new means the physical item must remain untouched.
        if (newComponents.empty())
            return AbsorbResult::ALREADY_ABSORBED;

        CharacterDatabaseTransaction trans =
            BuildAbsorptionTransaction(guid, itemEntry, newComponents);
        CharacterDatabase.DirectCommitTransaction(trans);

        // Destroy exactly one unit only after at least one new component was
        // committed for this concrete physical item.
        uint32 destroyCount = 1;
        player->DestroyItemCount(item, destroyCount, true);

        // Roll back only the component rows inserted by this attempt. Existing
        // BASE or affix progression for the same item_entry must never be lost.
        if (destroyCount != 0)
        {
            RemoveAbsorptionRowsSynchronously(
                guid,
                itemEntry,
                newComponents);
            return AbsorbResult::DESTROY_FAILED;
        }

        Recalculate(player);
        return AbsorbResult::SUCCESS;
    }

    bool GetMasteryUpgradeCost(
        ItemTemplate const* proto,
        uint32 currentMastery,
        MasteryCost& cost)
    {
        cost = MasteryCost{};

        if (!proto)
            return false;

        // Economy is intentionally defined only for the first playtest bracket.
        // Higher brackets will be added from observed gameplay data later.
        if (proto->RequiredLevel < 1 || proto->RequiredLevel > 19)
            return false;

        switch (currentMastery)
        {
            case 20:
                // 20% -> 40%: 5 silver.
                cost.MoneyCopper = 500;
                cost.NextMastery = 40;
                return true;

            case 40:
                // 40% -> 60%: 10 silver + 2 Strange Dust.
                cost.MoneyCopper = 1000;
                cost.ReagentEntry = 10940;
                cost.ReagentCount = 2;
                cost.NextMastery = 60;
                return true;

            case 60:
                // 60% -> 80%: 15 silver + 1 Greater Magic Essence.
                cost.MoneyCopper = 1500;
                cost.ReagentEntry = 10939;
                cost.ReagentCount = 1;
                cost.NextMastery = 80;
                return true;

            case 80:
                // 80% -> 100%: 25 silver + 1 Small Glimmering Shard.
                cost.MoneyCopper = 2500;
                cost.ReagentEntry = 10978;
                cost.ReagentCount = 1;
                cost.NextMastery = 100;
                return true;

            default:
                return false;
        }
    }

    MasteryUpgradeResult UpgradeMastery(
        Player* player,
        EssenceType essenceType,
        uint32 itemEntry,
        uint32 affixId)
    {
        if (!player || itemEntry == 0)
            return MasteryUpgradeResult::INVALID_ARGUMENT;

        const uint32 essenceTypeValue = static_cast<uint32>(essenceType);
        if (essenceTypeValue > static_cast<uint32>(EssenceType::RANDOM_SUFFIX))
            return MasteryUpgradeResult::INVALID_ARGUMENT;

        // BASE is always (type=BASE, affix=0); random affixes always have an id.
        if ((essenceType == EssenceType::BASE && affixId != 0) ||
            (essenceType != EssenceType::BASE && affixId == 0))
        {
            return MasteryUpgradeResult::INVALID_ARGUMENT;
        }

        const uint32 guid = player->GetGUID().GetCounter();

        QueryResult masteryResult = CharacterDatabase.Query(
            "SELECT mastery_percent "
            "FROM character_crucible_absorption "
            "WHERE guid = {} AND essence_type = {} AND item_entry = {} "
            "AND affix_id = {} LIMIT 1",
            guid,
            essenceTypeValue,
            itemEntry,
            affixId
        );

        if (!masteryResult)
            return MasteryUpgradeResult::ESSENCE_NOT_FOUND;

        Field* masteryFields = masteryResult->Fetch();
        const uint32 currentMastery = masteryFields[0].Get<uint32>();

        if (currentMastery != 20 &&
            currentMastery != 40 &&
            currentMastery != 60 &&
            currentMastery != 80)
        {
            return MasteryUpgradeResult::INVALID_MASTERY_STATE;
        }

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
        if (!proto)
            return MasteryUpgradeResult::INVALID_ARGUMENT;

        MasteryCost cost;
        if (!GetMasteryUpgradeCost(proto, currentMastery, cost))
            return MasteryUpgradeResult::UNSUPPORTED_BRACKET;

        QueryResult contributionResult = CharacterDatabase.Query(
            "SELECT 1 FROM character_crucible_contribution "
            "WHERE guid = {} AND essence_type = {} AND item_entry = {} "
            "AND affix_id = {} LIMIT 1",
            guid,
            essenceTypeValue,
            itemEntry,
            affixId
        );

        if (!contributionResult)
            return MasteryUpgradeResult::INVALID_MASTERY_STATE;

        if (!player->HasEnoughMoney(cost.MoneyCopper))
            return MasteryUpgradeResult::NOT_ENOUGH_MONEY;

        if (cost.ReagentEntry != 0 &&
            !player->HasItemCount(cost.ReagentEntry, cost.ReagentCount, false))
        {
            return MasteryUpgradeResult::NOT_ENOUGH_REAGENT;
        }

        const float scale =
            static_cast<float>(cost.NextMastery) /
            static_cast<float>(currentMastery);

        // Resource checks are complete before anything is consumed.
        if (cost.ReagentEntry != 0)
            player->DestroyItemCount(cost.ReagentEntry, cost.ReagentCount, true);

        if (cost.MoneyCopper != 0)
            player->ModifyMoney(-static_cast<int32>(cost.MoneyCopper));

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        // Scale only this exact essence component. BASE and every concrete affix
        // on the same item_entry keep independent mastery.
        CharacterDatabase.ExecuteOrAppend(
            trans,
            fmt::format(
                "UPDATE character_crucible_contribution "
                "SET coefficient = coefficient * {:.8f}, "
                "absorbed_value = absorbed_value * {:.8f} "
                "WHERE guid = {} AND essence_type = {} AND item_entry = {} "
                "AND affix_id = {}",
                scale,
                scale,
                guid,
                essenceTypeValue,
                itemEntry,
                affixId
            )
        );

        CharacterDatabase.ExecuteOrAppend(
            trans,
            fmt::format(
                "UPDATE character_crucible_absorption "
                "SET mastery_percent = {} "
                "WHERE guid = {} AND essence_type = {} AND item_entry = {} "
                "AND affix_id = {} AND mastery_percent = {}",
                cost.NextMastery,
                guid,
                essenceTypeValue,
                itemEntry,
                affixId,
                currentMastery
            )
        );

        CharacterDatabase.DirectCommitTransaction(trans);

        Recalculate(player);
        return MasteryUpgradeResult::SUCCESS;
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
