#ifndef _COMMANDS_OBJECT_H_
#define _COMMANDS_OBJECT_H_

// Forward declarations
class CObject;

namespace object_commands
{
	// bCalledByScriptCommand will be false if this function is called when loading from memory card
	// PoolIndex is only used when loading from the memory card. It will be -1 in all other cases
	void ObjectCreationFunction(int ObjectModelHashKey, float NewX, float NewY, float NewZ, bool bWithZOffset, bool bCalledByScriptCommand, s32 PoolIndex, int& iNewObjectIndex, bool bRegisterAsNetworkObject, bool bScriptHostObject, bool bForceToBeObject = false);

	void DeleteScriptObject(CObject *pObject);

	int GetInteriorAtCoords(Vector3 &vecCoords);

	void SetupScriptCommands();

	s32 CommandGetCompositeEntityAt(const scrVector & scrVecCoors, float Radius, const char* pRayFireName ); 
	
	s32 CommandGetCompositeEntityAtInternal(const scrVector & scrVecCoors, float Radius, const atHashString& RayfireName ); 

	void CommandSetCompositeEntityState(int iCompEntityIndex, int targetState); 
	
	s32 CommandGetCompositeEntityState(int iCompEntityIndex); 

	void CommandDoorSystemSetState(int doorEnumHash, int doorState, bool network, bool flushState);
	bool CommandIsDoorClosed(int doorEnumHash);

	enum eAnimatedBuildingRateFlags
	{
		AB_RATE_RANDOM = (1<<0),
	};

	enum eAnimatedBuildingPhaseFlags
	{
		AB_PHASE_RANDOM = (1<<0),
	};

	void CommandSetNearestBuildingAnimRate(const scrVector & scrVecCentreCoors, float Radius, s32 ModelHashKey, float Rate, int RateFlags);
	void CommandGetNearestBuildingAnimRate(const scrVector & scrVecCentreCoors, float Radius, s32 ModelHashKey, float &ReturnRate);
	void CommandSetNearestBuildingAnimPhase(const scrVector & scrVecCentreCoors, float Radius, s32 ModelHashKey, float Phase, int PhaseFlags);
	void CommandGetNearestBuildingAnimPhase(const scrVector & scrVecCentreCoors, float Radius, s32 ModelHashKey, float &ReturnPhase);
}

#endif	//	_COMMANDS_OBJECT_H_
