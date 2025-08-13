
#ifndef _SCRIPT_CARS_AND_PEDS_H_
	#define _SCRIPT_CARS_AND_PEDS_H_

#include "Peds/ped.h"

// Rage headers
#include "vector/vector3.h"

// Framework headers
#include "streaming/streamingmodule.h"

namespace rage
{
	class aiTask;
};

class CEntity;
class CPed;
class CVehicle;
class CTaskSequenceList;
class CPedModelInfo;

/////////////////////////////////////////////////////////////////////////////////
// Class for checking for specific cars being stuck on their roofs
/////////////////////////////////////////////////////////////////////////////////

#define UPSIDEDOWN_CAR_UPZ_THRESHOLD		(0.3f)//(-0.97f)

#define MAX_UPSIDEDOWN_CAR_CHECKS	(6)

class CUpsideDownCarCheck
{
	struct upsidedown_car_data
	{
		s32 CarID;
		u32 UpsideDownTimer;
	};

public :
	static bool IsCarUpsideDown(const CVehicle* pVehicleToCheck);
	bool IsCarInUpsideDownCarArray(s32 CarToCheckID);

	void Init(void);

	void UpdateTimers(void);
	
	void AddCarToCheck(s32 NewCarID);
	void RemoveCarFromCheck(s32 RemoveCarID);
	
	bool HasCarBeenUpsideDownForAWhile(s32 CheckCarID);

private:
	upsidedown_car_data Cars[MAX_UPSIDEDOWN_CAR_CHECKS];

	bool IsCarUpsideDown(s32 CarToCheckID);
};


/////////////////////////////////////////////////////////////////////////////////
// Class for checking for specific cars being stuck
/////////////////////////////////////////////////////////////////////////////////

#define MAX_STUCK_CAR_CHECKS	(16)

class CStuckCarCheck
{
	struct stuck_car_data
	{
		s32 m_CarID;
		Vector3 LastPos;
		s32 LastChecked;
		float StuckRadius;
		s32 CheckTime;
		bool bStuck;
		
		bool bWarpCar;
		bool bWarpIfStuckFlag;
		bool bWarpIfUpsideDownFlag;
		bool bWarpIfInWaterFlag;
		s8 NumberOfNodesToCheck;	//	Number of nth closest car nodes to check - if the value is negative then check the last route node instead
	};
	
public :
	void Init(void);
	
	void Process(void);

	void AddCarToCheck(s32 NewCarID, float StuckRad, u32 CheckEvery, bool bWarpCar = false, bool bStuckFlag = false, bool bUpsideDownFlag = false, bool bInWaterFlag = false, s8 NumberOfNodesToCheck = 0);
	
	void RemoveCarFromCheck(s32 RemoveCarID);
	
	void ClearStuckFlagForCar(s32 CheckCarID);
	
	bool IsCarInStuckCarArray(s32 CheckCarID);

private:
	void ResetArrayElement(u32 Index);
	
	bool AttemptToWarpVehicle(CVehicle *pVehicle, Vector3 &NewCoords, float NewHeading);

private:
	stuck_car_data Cars[MAX_STUCK_CAR_CHECKS];
};


class CBoatIsBeachedCheck
{
public:
	static bool IsBoatBeached(const CVehicle& in_VehicleToCheck);
};



/////////////////////////////////////////////////////////////////////////////////
// CSuppressedModels
/////////////////////////////////////////////////////////////////////////////////

#define MAX_SUPPRESSED_MODELS			(24)

class CSuppressedModels
{
	struct suppressed_model_struct
	{
		u32 SuppressedModel;
		strStreamingObjectName NameOfScriptThatHasSuppressedModel;
	};

public:
	void ClearAllSuppressedModels(void);

	bool AddToSuppressedModelArray(u32 NewModel, const strStreamingObjectName pScriptName);
	bool RemoveFromSuppressedModelArray(u32 ModelToRemove);

	bool HasModelBeenSuppressed(u32 Model);

#if __BANK
	void PrintSuppressedModels();
#endif

	void DisplaySuppressedModels();

private:
	atFixedArray<suppressed_model_struct, MAX_SUPPRESSED_MODELS> SuppressedModels;
};

/////////////////////////////////////////////////////////////////////////////////
// CRestrictedModels (limit to one)
/////////////////////////////////////////////////////////////////////////////////

#define MAX_RESTRICTED_MODELS			(24)

class CRestrictedModels
{
public:
	CRestrictedModels();
	void ClearAllRestrictedModels();
	void AddToRestrictedModelArray(u32 model, const strStreamingObjectName scriptName);
	void RemoveFromRestrictedModelArray(u32 model);
	bool HasModelBeenRestricted(u32 model) const;
#if __BANK
	void PrintRestrictedModels();
#endif
	void DisplayRestrictedModels();
private:
	struct RestrictedModelStruct
	{
		u32 m_model;
		strStreamingObjectName m_scriptName;
	};
	atFixedArray<RestrictedModelStruct, MAX_RESTRICTED_MODELS> RestrictedModels;
};


class CScriptCars
{
public:
//	Public functions
	static void Init(unsigned initMode);
	static void Process(void);

	static bool IsVehicleStopped(const CVehicle *pVehicleToCheck);

	static CUpsideDownCarCheck &GetUpsideDownCars() { return UpsideDownCars; }
	static CStuckCarCheck &GetStuckCars() { return StuckCars; }

	static CSuppressedModels &GetSuppressedCarModels() { return SuppressedCarModels; }

private:
	static CUpsideDownCarCheck UpsideDownCars;

	static CStuckCarCheck StuckCars;

	static CSuppressedModels SuppressedCarModels;
};

class CScriptPeds
{
public:
//	Public functions
	static void Init(unsigned initMode);
	static void Process(void);

	static const int SetPedCoordFlag_NoFlags = BIT(0);
	static const int SetPedCoordFlag_WarpGang = BIT(1);
	static const int SetPedCoordFlag_WarpCar = BIT(2);
	static const int SetPedCoordFlag_OffsetZ = BIT(3);
	static const int SetPedCoordFlag_KeepTasks = BIT(4);
	static const int SetPedCoordFlag_AllowClones = BIT(5);
	static const int SetPedCoordFlag_KeepIK = BIT(6);

	//	bool bWarpGang, bool bOffset=true
	static void SetPedCoordinates(CPed* pPed, float NewX,float NewY, float NewZ, int Flags, bool bWarp=true, bool bResetPlants=true);

	static void GivePedScriptedTask(s32 pedId, CTaskSequenceList* pTaskList, s32 CurrCommand, const char* commandName);

	static void GivePedScriptedTask(s32 pedId, aiTask* pTask, s32 CurrCommand, const char* commandName);

	static bool IsPedStopped(const CPed *pPedToCheck);

	static CSuppressedModels &GetSuppressedPedModels() { return SuppressedPedModels; }
	static CRestrictedModels  &GetRestrictedPedModels() { return RestrictedPedModels; }
	static bool HasPedModelBeenRestrictedOrSuppressed(u32 modelIndex);
	static bool HasPedModelBeenRestrictedOrSuppressed(u32 modelIndex, const CPedModelInfo *pPedModelInfo);

private:
	static CSuppressedModels SuppressedPedModels;
	static CRestrictedModels RestrictedPedModels;
};

class CScriptEntities
{
public:
//	Public functions
	static void Init(unsigned initMode);

	static void ClearSpaceForMissionEntity(CEntity *pEnt);
};

class CRaceTimes
{
public:
    struct sSplitTime
	{
        s32 time;
        s32 position;
	};
    struct sPlayer
	{
        sPlayer() : lastCheckpoint(-1), pedIndex(-1) {}

        s32 pedIndex;
        s32 lastCheckpoint;
	};

    static void Init(s32 numCheckpoints, s32 numLaps, s32 numPlayers, s32 localPlayerIndex);
    static void Shutdown();
    static void CheckpointHit(s32 pedIndex, s32 checkpoint, s32 lap, s32 time);
    static bool GetPlayerTime(s32 pedIndex, s32& time, s32& position);

private:
    static bool UpdateTimes();

    static atArray<s32> ms_records;
    static atArray<sSplitTime> ms_splitTimes;
    static atArray<sPlayer> ms_players;
    static s32 ms_numCheckPoints;
    static s32 ms_numLaps;
    static s32 ms_numPlayers;
    static s32 ms_lastLocalCheckpoint;
    static s32 ms_localPlayerIndex;
    static bool ms_modified;
};

struct MissionCreatorAssetData
{
    u32 memory;

	PAR_SIMPLE_PARSABLE;
};

class MissionCreatorAssets
{
public:

    static void Init();
    static void Shutdown();

    static bool HasRoomForModel(fwModelId modelId);
    static bool AddModel(fwModelId modelId);
    static void RemoveModel(fwModelId modelId);
    static float GetUsedBudget();

private:
	struct sAssetEntry
	{
		strIndex streamIdx;
		s32 refCount;
	};

	static void GetAdditionalModelCost(const atUserArray<strIndex>& deps, u32& main, u32& vram);
	static void AddAdditionalModelCost(const atUserArray<strIndex>& deps);
	static void RemoveAdditionalModelCost(const atUserArray<strIndex>& deps);

    static atArray<sAssetEntry> ms_assets;
    static MissionCreatorAssetData* ms_data;

    static s32 ms_freeMemory;
};

#endif	//	_SCRIPT_CARS_AND_PEDS_H_

