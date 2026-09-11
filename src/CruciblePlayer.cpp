#include "CrucibleSystem.h"

#include "Item.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
    constexpr std::string_view CRUCIBLE_PREFIX = "CRUCIBLE";
    constexpr std::string_view ITEMGUID_COMMAND = "ITEMGUID";
    constexpr std::string_view ABSORB_COMMAND = "ABSORB";

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
                return;
            }

            const uint32 itemEntry = item->GetEntry();

            Crucible::AbsorbResult result = Crucible::AbsorbItem(player, item);

            LOG_INFO(
                "module.crucible",
                "Crucible ABSORB: player='{}' guid='{}' entry={} result={}",
                player->GetName(),
                argument,
                itemEntry,
                GetAbsorbResultName(result));
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
