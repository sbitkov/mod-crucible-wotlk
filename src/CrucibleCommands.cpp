#include "CrucibleSystem.h"

#include "Chat.h"
#include "CommandScript.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryResult.h"
#include "RBAC.h"

#include <fmt/format.h>

using namespace Acore::ChatCommands;

namespace
{
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
}

class CrucibleCommandScript : public CommandScript
{
public:
    CrucibleCommandScript() : CommandScript("CrucibleCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable crucibleSubcommandTable =
        {
            { "inspect", HandleInspect, rbac::RBAC_PERM_COMMAND_SERVER_INFO, Console::No },
            { "absorb", HandleAbsorb, rbac::RBAC_PERM_COMMAND_SERVER_INFO, Console::No },
            { "absorb-test", HandleAbsorbTest, rbac::RBAC_PERM_COMMAND_SERVER_INFO, Console::No },
            { "unabsorb", HandleUnabsorb, rbac::RBAC_PERM_COMMAND_SERVER_INFO, Console::No },
            { "reset", HandleReset, rbac::RBAC_PERM_COMMAND_SERVER_INFO, Console::No },
            { "stats", HandleStats, rbac::RBAC_PERM_COMMAND_SERVER_INFO, Console::No },
            { "apply", HandleApply, rbac::RBAC_PERM_COMMAND_SERVER_INFO, Console::No },
            { "unapply", HandleUnapply, rbac::RBAC_PERM_COMMAND_SERVER_INFO, Console::No },
            { "recalc", HandleRecalc, rbac::RBAC_PERM_COMMAND_SERVER_INFO, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "crucible", crucibleSubcommandTable }
        };

        return commandTable;
    }

    static bool HandleInspect(ChatHandler* handler, uint32 itemEntry)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);

        if (!proto)
        {
            handler->PSendSysMessage("Crucible: item template {} not found.", itemEntry);
            return true;
        }

        handler->PSendSysMessage("Crucible inspection: {} - {}", itemEntry, proto->Name1);

        if (proto->Class == ITEM_CLASS_WEAPON)
        {
            handler->SendSysMessage("Crucible: weapon absorption is not implemented in v0.1.");
            return true;
        }

        std::vector<Crucible::Contribution> contributions = Crucible::ExtractContributions(proto);

        if (contributions.empty())
        {
            handler->SendSysMessage("Crucible: no supported v0.1 stats found on this item.");
            return true;
        }

        for (Crucible::Contribution const& contribution : contributions)
        {
            int32 applied = static_cast<int32>(contribution.AbsorbedValue);

            handler->PSendSysMessage(
                "{}: source = {:.2f}, absorbed = {:.4f}, applied = {}",
                Crucible::GetStatName(contribution.Stat),
                contribution.SourceValue,
                contribution.AbsorbedValue,
                applied
            );
        }

        return true;
    }

    static bool HandleAbsorb(ChatHandler* handler, uint32 itemEntry)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);
        if (!proto)
        {
            handler->PSendSysMessage("Crucible: item template {} not found.", itemEntry);
            return true;
        }

        // The debug command still selects by entry. The real UI/transport path will
        // supply an exact Item* resolved from the instance GUID.
        Item* item = player->GetItemByEntry(itemEntry);
        if (!item)
        {
            handler->PSendSysMessage(
                "Crucible: you do not have item {} - {} in your inventory or equipment.",
                itemEntry,
                proto->Name1
            );
            return true;
        }

        switch (Crucible::AbsorbItem(player, item))
        {
            case Crucible::AbsorbResult::SUCCESS:
                handler->PSendSysMessage(
                    "Crucible: absorbed 1x {} - {}. Permanent bonuses recalculated.",
                    itemEntry,
                    proto->Name1
                );
                break;

            case Crucible::AbsorbResult::WEAPON_UNSUPPORTED:
                handler->SendSysMessage(
                    "Crucible: weapon absorption is not implemented in v0.1."
                );
                break;

            case Crucible::AbsorbResult::ALREADY_ABSORBED:
                handler->PSendSysMessage(
                    "Crucible: item {} - {} has already been absorbed.",
                    itemEntry,
                    proto->Name1
                );
                break;

            case Crucible::AbsorbResult::ITEM_IN_TRADE:
                handler->SendSysMessage("Crucible: that item is currently in trade.");
                break;

            case Crucible::AbsorbResult::NO_SUPPORTED_STATS:
                handler->SendSysMessage(
                    "Crucible: no supported v0.1 stats found on this item."
                );
                break;

            case Crucible::AbsorbResult::DESTROY_FAILED:
                handler->PSendSysMessage(
                    "Crucible ERROR: failed to destroy item {} - {}. "
                    "The absorption record was rolled back.",
                    itemEntry,
                    proto->Name1
                );
                break;

            case Crucible::AbsorbResult::ITEM_TEMPLATE_NOT_FOUND:
                handler->PSendSysMessage(
                    "Crucible: item template {} not found.",
                    itemEntry
                );
                break;

            case Crucible::AbsorbResult::INVALID_ARGUMENT:
            default:
                handler->SendSysMessage("Crucible ERROR: invalid absorption request.");
                break;
        }

        return true;
    }

    static bool HandleAbsorbTest(ChatHandler* handler, uint32 itemEntry)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry);

        if (!proto)
        {
            handler->PSendSysMessage("Crucible: item template {} not found.", itemEntry);
            return true;
        }

        if (proto->Class == ITEM_CLASS_WEAPON)
        {
            handler->SendSysMessage("Crucible: weapon absorption is not implemented in v0.1.");
            return true;
        }

        uint32 guid = player->GetGUID().GetCounter();

        if (HasAbsorbedItem(guid, itemEntry))
        {
            handler->PSendSysMessage(
                "Crucible: item {} - {} has already been absorbed.",
                itemEntry,
                proto->Name1
            );
            return true;
        }

        std::vector<Crucible::Contribution> contributions = Crucible::ExtractContributions(proto);

        if (contributions.empty())
        {
            handler->SendSysMessage("Crucible: no supported v0.1 stats found on this item.");
            return true;
        }

        CharacterDatabaseTransaction trans =
            BuildAbsorptionTransaction(guid, itemEntry, contributions);
        CharacterDatabase.DirectCommitTransaction(trans);

        Crucible::Recalculate(player);

        handler->PSendSysMessage(
            "Crucible TEST: recorded {} - {} with {} contribution(s). Item was NOT destroyed.",
            itemEntry,
            proto->Name1,
            contributions.size()
        );

        return true;
    }

    static bool HandleUnabsorb(ChatHandler* handler, uint32 itemEntry)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();

        if (!HasAbsorbedItem(guid, itemEntry))
        {
            handler->PSendSysMessage(
                "Crucible DEBUG: item {} is not currently absorbed.",
                itemEntry
            );
            return true;
        }

        RemoveAbsorptionRowsSynchronously(guid, itemEntry);
        Crucible::Recalculate(player);

        handler->PSendSysMessage(
            "Crucible DEBUG: removed absorption record for item {} and recalculated bonuses.",
            itemEntry
        );

        return true;
    }

    static bool HandleReset(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        uint32 guid = player->GetGUID().GetCounter();

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        CharacterDatabase.ExecuteOrAppend(
            trans,
            fmt::format(
                "DELETE FROM character_crucible_contribution WHERE guid = {}",
                guid
            )
        );

        CharacterDatabase.ExecuteOrAppend(
            trans,
            fmt::format(
                "DELETE FROM character_crucible_absorption WHERE guid = {}",
                guid
            )
        );

        CharacterDatabase.DirectCommitTransaction(trans);
        Crucible::Recalculate(player);

        handler->SendSysMessage(
            "Crucible DEBUG: all absorption records were removed and bonuses recalculated."
        );

        return true;
    }

    static bool HandleStats(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        std::vector<Crucible::AccumulatedStat> stats =
            Crucible::GetAccumulatedStats(player);

        if (stats.empty())
        {
            handler->SendSysMessage("Crucible: no absorbed stats recorded.");
            return true;
        }

        handler->SendSysMessage("Crucible accumulated stats:");

        for (Crucible::AccumulatedStat const& stat : stats)
        {
            handler->PSendSysMessage(
                "{}: stored = {:.4f}, applied = {}",
                Crucible::GetStatName(stat.Stat),
                stat.StoredValue,
                stat.AppliedValue
            );
        }

        return true;
    }

    static bool HandleApply(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (Crucible::IsApplied(player))
        {
            handler->SendSysMessage(
                "Crucible DEBUG: bonuses are already tracked as applied. Use .crucible recalc instead."
            );
            return true;
        }

        Crucible::Recalculate(player);
        handler->SendSysMessage("Crucible DEBUG: accumulated bonuses applied.");
        return true;
    }

    static bool HandleUnapply(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        Crucible::Unapply(player);
        handler->SendSysMessage("Crucible DEBUG: accumulated bonuses unapplied.");
        return true;
    }

    static bool HandleRecalc(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        Crucible::Recalculate(player);
        handler->SendSysMessage(
            "Crucible DEBUG: bonuses recalculated from the current database state."
        );
        return true;
    }
};

void AddCrucibleCommandScripts()
{
    new CrucibleCommandScript();
}
