#include "Chat.h"
#include "GameObject.h"
#include "Player.h"
#include "ScriptMgr.h"

class CrucibleGameObjectScript : public GameObjectScript
{
public:
    CrucibleGameObjectScript() : GameObjectScript("go_crucible") { }

    bool OnGossipHello(Player* player, GameObject* /*go*/) override
    {
        if (!player)
            return false;

        ChatHandler(player->GetSession()).SendSysMessage(
            "Crucible: interaction detected."
        );

        return true;
    }
};

void AddCrucibleGameObjectScripts()
{
    new CrucibleGameObjectScript();
}
