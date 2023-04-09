#include "Script_TeleportPartyMembers.h"

#include "util\Memory.h"
#include "util\Logging.h"
#include "util\Hook.h"
#include "util/Util.h"

#include "Script.h"

gSScriptInit & GetScriptInit()
{
	static gSScriptInit s_ScriptInit;
	return s_ScriptInit;
}

static mCFunctionHook Hook_MagicTeleport;
GEInt GE_STDCALL MagicTeleport(gCScriptProcessingUnit * a_pSPU, Entity * a_pSelfEntity, Entity * a_pOtherEntity, GEU32 a_iArgs)
{
	INIT_SCRIPT();
	//Get Player entity
	Entity Player = Entity::GetPlayer();

	//Get the player party list before executing original function
	//NPCs that are tamed or under word of control spell leave the party after teleporting away from them, so we need to get the list before
	bTValArray<eCEntity*> PartyList = SelfEntity.Party.GetMembers(GEFalse);

	//Little hack to prevent that annoying "NPC is waiting" message that shows up after teleporting away from NPC
	for (GEInt i = 0; i < PartyList.GetCount(); i++)
		Entity(PartyList.GetAt(i)).Party.SetPartyLeader(None);

	//Let the game do original MagicTeleport stuff
	GEInt result = Hook_MagicTeleport.GetOriginalFunction(&MagicTeleport)(a_pSPU, a_pSelfEntity, a_pOtherEntity, a_iArgs);

	//Check if entity casting the Teleport spell is Player
	if (SelfEntity == Player)
	{
		//Iterate through party list
		for (GEInt x = 0; x < PartyList.GetCount(); x++)
		{
			Entity PartyEnt = Entity(PartyList.GetAt(x));
			gESpecies PartyEntSpecies = PartyEnt.NPC.Species;

			switch (PartyEnt.Party.PartyMemberType)
			{
			case gEPartyMemberType_Party:
			case gEPartyMemberType_Slave:
			case gEPartyMemberType_Summoned:
				break;
			case gEPartyMemberType_Controlled:
				//We do not want to teleport Human/Orc NPCs that are under Word of Control spell
				if (PartyEntSpecies == gESpecies_Human || PartyEntSpecies == gESpecies_Orc)
					continue;
				break;
			default:
				continue;
			}

			//Teleport party member to player
			PartyEnt.Teleport(SelfEntity);

			//After teleporting NPCs that were tamed or under Word of Control spell return to their previous attitude and Summoned monsters become hostile
			//And Human/Orc NPCs that are in player party are put into waiting mode
			//So here is a fix for that
			PartyEnt.Party.SetPartyLeader(SelfEntity);
			PartyEnt.Party.Waiting = GEFalse;
		}
	}

	//Return the result of the original func
	return result;
}

extern "C" __declspec(dllexport)
gSScriptInit const * GE_STDCALL ScriptInit(void)
{
	// Ensure that that Script_Game.dll is loaded.
	GetScriptAdmin().LoadScriptDLL("Script_Game.dll");

	//Hook into original script function that is executed after casting Teleport spell
	Hook_MagicTeleport.Hook(GetScriptAdminExt().GetScript("MagicTeleport")->m_funcScript, &MagicTeleport, mCBaseHook::mEHookType_OnlyStack);

	return &GetScriptInit();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		::DisableThreadLibraryCalls(hModule);
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
