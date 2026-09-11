#include "CrucibleSystem.h"

#include "Item.h"
#include "Log.h"
#include "ObjectGuid.h"
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
