#include "GameObject.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "WorldSession.h"
#include "Chat.h"

#include <string>

namespace
{
    void SendCrucibleAddonMessage(Player* player, std::string const& payload)
    {
        if (!player)
            return;

        // Use AzerothCore's normal player whisper path verbatim.
        // For LANG_ADDON it builds the addon whisper packet and sends it
        // directly to the target without ordinary whisper side effects.
        player->Whisper("CRUCIBLE\t" + payload, LANG_ADDON, player);
    }
}

class CrucibleGameObjectScript : public GameObjectScript
{
public:
    CrucibleGameObjectScript() : GameObjectScript("go_crucible") { }

    bool OnGossipHello(Player* player, GameObject* /*go*/) override
    {
        if (!player)
            return false;

        SendCrucibleAddonMessage(player, "OPEN");

        ChatHandler(player->GetSession()).SendSysMessage(
            "Crucible: OPEN message sent to client."
        );

        return true;
    }
};

void AddCrucibleGameObjectScripts()
{
    new CrucibleGameObjectScript();
}
