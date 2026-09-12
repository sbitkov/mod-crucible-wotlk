#include "CrucibleSystem.h"

#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

#include <fmt/format.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
    constexpr std::string_view CRUCIBLE_PREFIX = "CRUCIBLE";
    constexpr std::string_view ITEMGUID_COMMAND = "ITEMGUID";
    constexpr std::string_view ABSORB_COMMAND = "ABSORB";
    constexpr std::string_view PREVIEW_COMMAND = "PREVIEW";
    constexpr std::string_view STATS_COMMAND = "STATS";
    constexpr std::string_view ESSENCES_COMMAND = "ESSENCES";
    constexpr std::string_view MASTERY_PREVIEW_COMMAND = "MASTERY_PREVIEW";
    constexpr std::string_view MASTERY_UPGRADE_COMMAND = "MASTERY_UPGRADE";

    bool TryParseCrucibleAddonMessage(
        std::string const& msg,
        std::string_view& payload)
    {
        const std::size_t separator = msg.find('\t');
        if (separator == std::string::npos)
            return false;

        const std::string_view prefix(msg.data(), separator);
        if (prefix != CRUCIBLE_PREFIX)
            return false;

        payload = std::string_view(
            msg.data() + separator + 1,
            msg.size() - separator - 1);

        return true;
    }

    void SplitCommand(
        std::string_view payload,
        std::string_view& command,
        std::string_view& argument)
    {
        const std::size_t separator = payload.find('\t');

        if (separator == std::string_view::npos)
        {
            command = payload;
            argument = {};
            return;
        }

        command = payload.substr(0, separator);
        argument = payload.substr(separator + 1);
    }

    bool TryParseItemGuid(
        std::string_view text,
        uint64& rawGuid)
    {
        if (text.size() < 3)
            return false;

        if (!(text[0] == '0' && (text[1] == 'x' || text[1] == 'X')))
            return false;

        text.remove_prefix(2);

        rawGuid = 0;

        const char* begin = text.data();
        const char* end = text.data() + text.size();

        const auto result = std::from_chars(begin, end, rawGuid, 16);

        return result.ec == std::errc{} && result.ptr == end;
    }

    Item* ResolvePlayerItemByClientGuid(
        Player* player,
        std::string_view guidText)
    {
        if (!player)
            return nullptr;

        uint64 rawGuid = 0;
        if (!TryParseItemGuid(guidText, rawGuid))
            return nullptr;

        const uint32 lowGuid = static_cast<uint32>(rawGuid & 0xFFFFFFFFULL);
        const ObjectGuid itemGuid = ObjectGuid::Create<HighGuid::Item>(lowGuid);

        // Do not accept an arbitrary low counter with a forged high part.
        if (itemGuid.GetRawValue() != rawGuid)
            return nullptr;

        return player->GetItemByGuid(itemGuid);
    }

    char const* GetAbsorbResultName(Crucible::AbsorbResult result)
    {
        switch (result)
        {
            case Crucible::AbsorbResult::SUCCESS:
                return "SUCCESS";
            case Crucible::AbsorbResult::INVALID_ARGUMENT:
                return "INVALID_ARGUMENT";
            case Crucible::AbsorbResult::ITEM_TEMPLATE_NOT_FOUND:
                return "ITEM_TEMPLATE_NOT_FOUND";
            case Crucible::AbsorbResult::QUALITY_TOO_LOW:
                return "QUALITY_TOO_LOW";
            case Crucible::AbsorbResult::ITEM_NOT_EQUIPMENT:
                return "ITEM_NOT_EQUIPMENT";
            case Crucible::AbsorbResult::ITEM_NOT_USABLE:
                return "ITEM_NOT_USABLE";
            case Crucible::AbsorbResult::ARMOR_TYPE_NOT_ALLOWED:
                return "ARMOR_TYPE_NOT_ALLOWED";
            case Crucible::AbsorbResult::WEAPON_TYPE_NOT_ALLOWED:
                return "WEAPON_TYPE_NOT_ALLOWED";
            case Crucible::AbsorbResult::WEAPON_UNSUPPORTED:
                return "WEAPON_UNSUPPORTED";
            case Crucible::AbsorbResult::ALREADY_ABSORBED:
                return "ALREADY_ABSORBED";
            case Crucible::AbsorbResult::ITEM_IN_TRADE:
                return "ITEM_IN_TRADE";
            case Crucible::AbsorbResult::NO_SUPPORTED_STATS:
                return "NO_SUPPORTED_STATS";
            case Crucible::AbsorbResult::DESTROY_FAILED:
                return "DESTROY_FAILED";
        }

        return "UNKNOWN";
    }

    void SendCrucibleResult(
        Player* player,
        std::string_view result,
        std::string_view guidText)
    {
        if (!player)
            return;

        std::string message = "CRUCIBLE\tRESULT\t";
        message.append(result.data(), result.size());
        message.push_back('\t');
        message.append(guidText.data(), guidText.size());

        // Use AzerothCore's normal addon-whisper path.
        player->Whisper(message, LANG_ADDON, player);
    }

    void SendCruciblePreviewBegin(
        Player* player,
        std::string_view result,
        std::string_view guidText)
    {
        if (!player)
            return;

        std::string message = "CRUCIBLE\tPREVIEW_BEGIN\t";
        message.append(result.data(), result.size());
        message.push_back('\t');
        message.append(guidText.data(), guidText.size());

        player->Whisper(message, LANG_ADDON, player);
    }

    void SendCruciblePreviewStat(
        Player* player,
        std::string_view guidText,
        Crucible::Contribution const& contribution)
    {
        if (!player)
            return;

        std::string message = fmt::format(
            "CRUCIBLE\tPREVIEW_STAT\t{}\t{}\t{:.4f}",
            guidText,
            Crucible::GetStatName(contribution.Stat),
            contribution.AbsorbedValue);

        player->Whisper(message, LANG_ADDON, player);
    }

    void SendCruciblePreviewEnd(
        Player* player,
        std::string_view guidText)
    {
        if (!player)
            return;

        std::string message = "CRUCIBLE\tPREVIEW_END\t";
        message.append(guidText.data(), guidText.size());

        player->Whisper(message, LANG_ADDON, player);
    }

    void SendCrucibleStatsBegin(Player* player)
    {
        if (!player)
            return;

        player->Whisper("CRUCIBLE\tSTATS_BEGIN", LANG_ADDON, player);
    }

    void SendCrucibleStatsRow(
        Player* player,
        Crucible::AccumulatedStat const& stat)
    {
        if (!player)
            return;

        std::string message = fmt::format(
            "CRUCIBLE\tSTATS_ROW\t{}\t{}\t{:.4f}\t{}",
            static_cast<uint16>(stat.Stat),
            Crucible::GetStatName(stat.Stat),
            stat.StoredValue,
            stat.AppliedValue);

        player->Whisper(message, LANG_ADDON, player);
    }

    void SendCrucibleStatsEnd(Player* player)
    {
        if (!player)
            return;

        player->Whisper("CRUCIBLE\tSTATS_END", LANG_ADDON, player);
    }

    void SendCrucibleEssencesBegin(Player* player)
    {
        if (!player)
            return;

        player->Whisper("CRUCIBLE\tESSENCES_BEGIN", LANG_ADDON, player);
    }

    void SendCrucibleEssenceRow(
        Player* player,
        Crucible::EssenceType essenceType,
        uint32 itemEntry,
        uint32 affixId,
        uint32 masteryPercent)
    {
        if (!player)
            return;

        std::string message = fmt::format(
            "CRUCIBLE\tESSENCE_ROW\t{}\t{}\t{}\t{}",
            static_cast<uint32>(essenceType),
            itemEntry,
            affixId,
            masteryPercent);

        player->Whisper(message, LANG_ADDON, player);
    }

    void SendCrucibleEssencesEnd(Player* player)
    {
        if (!player)
            return;

        player->Whisper("CRUCIBLE\tESSENCES_END", LANG_ADDON, player);
    }

    char const* GetMasteryUpgradeResultName(Crucible::MasteryUpgradeResult result)
    {
        switch (result)
        {
            case Crucible::MasteryUpgradeResult::SUCCESS: return "SUCCESS";
            case Crucible::MasteryUpgradeResult::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
            case Crucible::MasteryUpgradeResult::ESSENCE_NOT_FOUND: return "ESSENCE_NOT_FOUND";
            case Crucible::MasteryUpgradeResult::INVALID_MASTERY_STATE: return "INVALID_MASTERY_STATE";
            case Crucible::MasteryUpgradeResult::UNSUPPORTED_BRACKET: return "UNSUPPORTED_BRACKET";
            case Crucible::MasteryUpgradeResult::NOT_ENOUGH_MONEY: return "NOT_ENOUGH_MONEY";
            case Crucible::MasteryUpgradeResult::NOT_ENOUGH_REAGENT: return "NOT_ENOUGH_REAGENT";
        }

        return "UNKNOWN";
    }

    struct EssenceIdentity
    {
        Crucible::EssenceType Type = Crucible::EssenceType::BASE;
        uint32 ItemEntry = 0;
        uint32 AffixId = 0;
    };

    bool TryParseUint32(std::string_view text, uint32& value)
    {
        value = 0;
        if (text.empty())
            return false;

        const char* begin = text.data();
        const char* end = text.data() + text.size();
        const auto result = std::from_chars(begin, end, value, 10);
        return result.ec == std::errc{} && result.ptr == end;
    }

    bool TryParseEssenceIdentity(std::string_view text, EssenceIdentity& identity)
    {
        const std::size_t first = text.find('\t');
        if (first == std::string_view::npos)
            return false;

        const std::size_t second = text.find('\t', first + 1);
        if (second == std::string_view::npos)
            return false;

        if (text.find('\t', second + 1) != std::string_view::npos)
            return false;

        uint32 typeValue = 0;
        uint32 itemEntry = 0;
        uint32 affixId = 0;

        if (!TryParseUint32(text.substr(0, first), typeValue) ||
            !TryParseUint32(text.substr(first + 1, second - first - 1), itemEntry) ||
            !TryParseUint32(text.substr(second + 1), affixId))
        {
            return false;
        }

        if (typeValue > static_cast<uint32>(Crucible::EssenceType::RANDOM_SUFFIX) ||
            itemEntry == 0)
        {
            return false;
        }

        Crucible::EssenceType type =
            static_cast<Crucible::EssenceType>(typeValue);

        if ((type == Crucible::EssenceType::BASE && affixId != 0) ||
            (type != Crucible::EssenceType::BASE && affixId == 0))
        {
            return false;
        }

        identity.Type = type;
        identity.ItemEntry = itemEntry;
        identity.AffixId = affixId;
        return true;
    }

    void SendMasteryPreview(Player* player, EssenceIdentity const& identity)
    {
        if (!player)
            return;

        const uint32 guid = player->GetGUID().GetCounter();
        const uint32 typeValue = static_cast<uint32>(identity.Type);

        auto SendEnd = [&]()
        {
            player->Whisper(
                fmt::format(
                    "CRUCIBLE\tMASTERY_END\t{}\t{}\t{}",
                    typeValue,
                    identity.ItemEntry,
                    identity.AffixId),
                LANG_ADDON,
                player);
        };

        QueryResult masteryResult = CharacterDatabase.Query(
            "SELECT mastery_percent "
            "FROM character_crucible_absorption "
            "WHERE guid = {} AND essence_type = {} AND item_entry = {} "
            "AND affix_id = {} LIMIT 1",
            guid,
            typeValue,
            identity.ItemEntry,
            identity.AffixId
        );

        if (!masteryResult)
        {
            player->Whisper(
                fmt::format(
                    "CRUCIBLE\tMASTERY_BEGIN\t{}\t{}\t{}\tESSENCE_NOT_FOUND",
                    typeValue,
                    identity.ItemEntry,
                    identity.AffixId),
                LANG_ADDON,
                player);
            SendEnd();
            return;
        }

        const uint32 currentMastery = masteryResult->Fetch()[0].Get<uint32>();
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(identity.ItemEntry);

        if (!proto)
        {
            player->Whisper(
                fmt::format(
                    "CRUCIBLE\tMASTERY_BEGIN\t{}\t{}\t{}\tINVALID_ARGUMENT",
                    typeValue,
                    identity.ItemEntry,
                    identity.AffixId),
                LANG_ADDON,
                player);
            SendEnd();
            return;
        }

        Crucible::MasteryCost cost;
        const bool hasNext =
            Crucible::GetMasteryUpgradeCost(proto, currentMastery, cost);

        if (!hasNext)
        {
            const char* state =
                currentMastery == 100 ? "MAX_MASTERY" : "UNSUPPORTED_BRACKET";

            player->Whisper(
                fmt::format(
                    "CRUCIBLE\tMASTERY_BEGIN\t{}\t{}\t{}\t{}\t{}\t0\t0\t0\t0",
                    typeValue,
                    identity.ItemEntry,
                    identity.AffixId,
                    state,
                    currentMastery),
                LANG_ADDON,
                player);
            SendEnd();
            return;
        }

        player->Whisper(
            fmt::format(
                "CRUCIBLE\tMASTERY_BEGIN\t{}\t{}\t{}\tSUCCESS\t{}\t{}\t{}\t{}\t{}",
                typeValue,
                identity.ItemEntry,
                identity.AffixId,
                currentMastery,
                cost.NextMastery,
                cost.MoneyCopper,
                cost.ReagentEntry,
                cost.ReagentCount),
            LANG_ADDON,
            player);

        QueryResult contributionResult = CharacterDatabase.Query(
            "SELECT stat_id, absorbed_value "
            "FROM character_crucible_contribution "
            "WHERE guid = {} AND essence_type = {} AND item_entry = {} "
            "AND affix_id = {} ORDER BY stat_id",
            guid,
            typeValue,
            identity.ItemEntry,
            identity.AffixId
        );

        if (contributionResult)
        {
            const float scale =
                static_cast<float>(cost.NextMastery) /
                static_cast<float>(currentMastery);

            do
            {
                Field* fields = contributionResult->Fetch();
                const Crucible::StatId stat =
                    static_cast<Crucible::StatId>(fields[0].Get<uint16>());
                const float currentValue = fields[1].Get<float>();
                const float delta = currentValue * (scale - 1.0f);

                player->Whisper(
                    fmt::format(
                        "CRUCIBLE\tMASTERY_STAT\t{}\t{}\t{}\t{}\t{:.4f}",
                        typeValue,
                        identity.ItemEntry,
                        identity.AffixId,
                        Crucible::GetStatName(stat),
                        delta),
                    LANG_ADDON,
                    player);
            }
            while (contributionResult->NextRow());
        }

        SendEnd();
    }
}

class CruciblePlayerScript : public PlayerScript
{
public:
    CruciblePlayerScript() : PlayerScript(
        "CruciblePlayerScript",
        {
            PLAYERHOOK_ON_LOGIN,
            PLAYERHOOK_ON_LOGOUT,
            PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE
        })
    {
    }

    void OnPlayerLogin(Player* player) override
    {
        Crucible::Recalculate(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        Crucible::ForgetSession(player);
    }

    void OnPlayerBeforeSendChatMessage(
        Player* player,
        uint32& type,
        uint32& lang,
        std::string& msg) override
    {
        if (lang != LANG_ADDON)
            return;

        std::string_view payload;
        if (!TryParseCrucibleAddonMessage(msg, payload))
            return;

        std::string_view command;
        std::string_view argument;
        SplitCommand(payload, command, argument);

        if (command == "PING")
        {
            LOG_INFO(
                "module.crucible",
                "Crucible transport: player='{}' type={} command='PING'",
                player->GetName(),
                type);
            return;
        }

        if (command == ITEMGUID_COMMAND)
        {
            Item* item = ResolvePlayerItemByClientGuid(player, argument);

            if (!item)
            {
                LOG_INFO(
                    "module.crucible",
                    "Crucible ITEMGUID: player='{}' guid='{}' item not found or invalid",
                    player->GetName(),
                    argument);
                return;
            }

            LOG_INFO(
                "module.crucible",
                "Crucible ITEMGUID: player='{}' guid='{}' entry={} bag={} slot={} count={}",
                player->GetName(),
                argument,
                item->GetEntry(),
                item->GetBagSlot(),
                item->GetSlot(),
                item->GetCount());
            return;
        }

        if (command == STATS_COMMAND)
        {
            std::vector<Crucible::AccumulatedStat> stats =
                Crucible::GetAccumulatedStats(player);

            LOG_INFO(
                "module.crucible",
                "Crucible STATS: player='{}' stats={}",
                player->GetName(),
                stats.size());

            SendCrucibleStatsBegin(player);

            for (Crucible::AccumulatedStat const& stat : stats)
                SendCrucibleStatsRow(player, stat);

            SendCrucibleStatsEnd(player);
            return;
        }

        if (command == ESSENCES_COMMAND)
        {
            const uint32 guid = player->GetGUID().GetCounter();

            QueryResult result = CharacterDatabase.Query(
                "SELECT essence_type, item_entry, affix_id, mastery_percent "
                "FROM character_crucible_absorption "
                "WHERE guid = {} "
                "ORDER BY item_entry, essence_type, affix_id",
                guid
            );

            uint32 count = 0;
            SendCrucibleEssencesBegin(player);

            if (result)
            {
                do
                {
                    Field* fields = result->Fetch();
                    const Crucible::EssenceType essenceType =
                        static_cast<Crucible::EssenceType>(fields[0].Get<uint8>());
                    const uint32 itemEntry = fields[1].Get<uint32>();
                    const uint32 affixId = fields[2].Get<uint32>();
                    const uint32 masteryPercent = fields[3].Get<uint32>();

                    SendCrucibleEssenceRow(
                        player,
                        essenceType,
                        itemEntry,
                        affixId,
                        masteryPercent);
                    ++count;
                }
                while (result->NextRow());
            }

            SendCrucibleEssencesEnd(player);

            LOG_INFO(
                "module.crucible",
                "Crucible ESSENCES: player='{}' essences={}",
                player->GetName(),
                count);

            return;
        }

        if (command == MASTERY_PREVIEW_COMMAND)
        {
            EssenceIdentity identity;
            if (!TryParseEssenceIdentity(argument, identity))
            {
                LOG_INFO(
                    "module.crucible",
                    "Crucible MASTERY_PREVIEW: player='{}' invalid identity='{}'",
                    player->GetName(),
                    argument);
                return;
            }

            SendMasteryPreview(player, identity);
            return;
        }

        if (command == MASTERY_UPGRADE_COMMAND)
        {
            EssenceIdentity identity;
            if (!TryParseEssenceIdentity(argument, identity))
                return;

            Crucible::MasteryUpgradeResult result =
                Crucible::UpgradeMastery(
                    player,
                    identity.Type,
                    identity.ItemEntry,
                    identity.AffixId);

            player->Whisper(
                fmt::format(
                    "CRUCIBLE\tMASTERY_RESULT\t{}\t{}\t{}\t{}",
                    static_cast<uint32>(identity.Type),
                    identity.ItemEntry,
                    identity.AffixId,
                    GetMasteryUpgradeResultName(result)),
                LANG_ADDON,
                player);

            if (result == Crucible::MasteryUpgradeResult::SUCCESS)
                SendMasteryPreview(player, identity);

            return;
        }

        if (command == PREVIEW_COMMAND)
        {
            Item* item = ResolvePlayerItemByClientGuid(player, argument);

            if (!item)
            {
                LOG_INFO(
                    "module.crucible",
                    "Crucible PREVIEW: player='{}' guid='{}' item not found or invalid",
                    player->GetName(),
                    argument);

                SendCruciblePreviewBegin(player, "ITEM_NOT_FOUND", argument);
                SendCruciblePreviewEnd(player, argument);
                return;
            }

            std::vector<Crucible::Contribution> contributions;
            Crucible::AbsorbResult result =
                Crucible::PreviewItem(player, item, contributions);

            char const* resultName = GetAbsorbResultName(result);

            LOG_INFO(
                "module.crucible",
                "Crucible PREVIEW: player='{}' guid='{}' entry={} result={} stats={}",
                player->GetName(),
                argument,
                item->GetEntry(),
                resultName,
                contributions.size());

            SendCruciblePreviewBegin(player, resultName, argument);

            if (result == Crucible::AbsorbResult::SUCCESS)
            {
                for (Crucible::Contribution const& contribution : contributions)
                    SendCruciblePreviewStat(player, argument, contribution);
            }

            SendCruciblePreviewEnd(player, argument);
            return;
        }

        if (command == ABSORB_COMMAND)
        {
            Item* item = ResolvePlayerItemByClientGuid(player, argument);

            if (!item)
            {
                LOG_INFO(
                    "module.crucible",
                    "Crucible ABSORB: player='{}' guid='{}' item not found or invalid",
                    player->GetName(),
                    argument);

                SendCrucibleResult(player, "ITEM_NOT_FOUND", argument);
                return;
            }

            const uint32 itemEntry = item->GetEntry();

            Crucible::AbsorbResult result = Crucible::AbsorbItem(player, item);

            char const* resultName = GetAbsorbResultName(result);

            LOG_INFO(
                "module.crucible",
                "Crucible ABSORB: player='{}' guid='{}' entry={} result={}",
                player->GetName(),
                argument,
                itemEntry,
                resultName);

            SendCrucibleResult(player, resultName, argument);
            return;
        }

        LOG_INFO(
            "module.crucible",
            "Crucible transport: player='{}' unknown command='{}'",
            player->GetName(),
            command);
    }
};

void AddCruciblePlayerScripts()
{
    new CruciblePlayerScript();
}
