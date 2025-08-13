#ifndef _COMMANDS_ENTITY_H_
#define _COMMANDS_ENTITY_H_

//rage headers
#include "system/bit.h"
#include "script/script.h"

class CTaskScriptedAnimation;
class CTaskSynchronizedScene;

namespace entity_commands
{
	enum AnimType
	{
		kAnimScripted = BIT(0),
		kAnimSyncedScene = BIT(1),
		kAnimCode = BIT(2),
		kAnimDefault = kAnimScripted | kAnimSyncedScene
	};

	void SetupScriptCommands();

	void CommandSetEntityAsNoLongerNeeded(int &EntityIndex);

	CTaskScriptedAnimation* FindScriptedAnimTask(int EntityIndex, bool bModify, bool bPrimary = true, unsigned assertFlags = CTheScripts::GUID_ASSERT_FLAGS_ALL);

	const CTaskSynchronizedScene* FindSynchronizedSceneTask(s32 EntityIndex, const char * pAnimDictName,  const char * pAnimName);
	
	void PropagatePedAlpha(CPed *ped, int alpha, bool useSmoothAlpha = false);
	void PropagatePedResetAlpha(CPed *ped);

	enum eCmdOverrideAlpha
	{
		CMD_OVERRIDE_ALPHA_BY_SCRIPT,
		CMD_OVERRIDE_ALPHA_BY_NETCODE
	};
	int  CommandGetEntityAlphaEntity(CEntity* pEntity);
	void CommandSetEntityAlphaEntity(CEntity* pEntity, int alpha, bool useSmoothAlpha BANK_PARAM(eCmdOverrideAlpha src) );
	void CommandResetEntityAlphaEntity(CEntity* pEntity);
}

#endif	//	_COMMANDS_ENTITY_H_


