#ifndef INC_VEHICLE_DEBUG_H
#define INC_VEHICLE_DEBUG_H

#if __BANK

// Rage headers
#include "Bank/Bank.h"
#include "system/param.h"
#include "Vector/vector3.h"
#include "vectormath/vec3v.h"

// Game headers
#include "AI/AITarget.h"
#include "ai\debug\types\VehicleDebugInfo.h"
#include "Scene/RegdRefTypes.h"
#include "Vehicles/Metadata/VehicleEnterExitFlags.h"

// Forward declarations
class CVehicle;
class CEntryExitPoint;
class CComponentReservation;
class CPed;

namespace rage
{
	class bkBank;
	class bkCombo;
	class bkButton;
}

// Typedefs
typedef CVehicleEnterExitFlags::FlagsBitSet VehicleEnterExitFlags;

XPARAM(visualisepedsinvehicle);

class CVehicleDebug
{
public:
	
	// Bank Functions
	static void		InitBank();
	static void		AddWidgets(bkBank& bank);
	static void		ShutdownBank(); 

	// Spawn A Ped
	static void		SpawnPedCB();
	static void		SpawnVehicleAndPedCB();
	static Vector3	GenerateSpawnLocation();
	static void		CreatePed(u32 pedModelIndex, const Vector3& pos);
	static void		SetupPed();

	// Vehicle Tasks
	static void		SetFocusPedInSeatCB();
	static void		SetFocusPedExclusiveDriverCB();
	static void		ToggleAICanUseExclusiveSeatsCB();
	static void		GivePedSelectedTaskCB();
	static void		AddStateToArrayCB();
	static void		ClearLastStateFromArrayCB();
	static void		ClearAllStatesFromArrayCB();
	static void		DisplayStateList();
	static VehicleEnterExitFlags		GetFlags();

	// Debug rendering
	static void		RenderDebug();
	static void		RenderTestCapsuleAtPosition(const CPed* pPed, Vec3V_In vPos, Vec3V_In vUpVec, Color32 colour, u32 uTime, const CVehicle& rVeh, float fStartZOffset = 0.0f);
	static CVehicle* GetFocusVehicle(void);
	static CPed*	GetFocusPed();

	// Init the general vehicle widgets
	static void		InitWidgets(bkBank& bank);

	// Debug info functions
	static void		RenderEntryPoints(CVehicle* pVehicle, const CEntryExitPoint* pEntryPoint, s32 iEntryIndex, s32 iNumTexts);
	static void		RenderSeats(CVehicle* pVehicle, s32 iNumTexts);
	static void		RenderDoorComponentReservation(CVehicle* pVehicle, const CEntryExitPoint* pEntryPoint, s32 iEntryIndex, CComponentReservation* pComponentReservation, s32 iNumTexts);
	static void		RenderSeatComponentReservation(CVehicle* pVehicle, const CEntryExitPoint* pEntryPoint, s32 iEntryIndex, CComponentReservation* pComponentReservation, s32 iNumTexts);
	static void		RenderComponentUsage(CVehicle* pVehicle, CComponentReservation* pComponentReservation, Color32 colour);
	static void		RenderEntryPointCollisionCheck(CVehicle* pVehicle, s32 iEntryIndex);
	static void		RenderEntryPointExtraVehicleNodes(CVehicle* pVehicle, s32 iEntryIndex);

	// Set the vehicle lock state
	static void		LockVehicleDoorsCB();
	static void		SetMayOnlyEnterThisVehicleCB();
	static void		SetVehicleAsPersonalCB();
	static void		ProcessPersonalVehicle();

	// Forced vehicle usage
	static s32		GetForcedUsageFlags();
	static void		SetForcedVehicleUsageCB();
	static void		ClearForcedVehicleUsageCB();
	static void		ValidateScriptConfigCB();

	// Misc
	static void		RollDownAllWindowsCB();
	static void		SetOverrideInVehicleContextCB();
	static void		ResetOverrideInVehicleContextCB();
	static void		SetPlayerJackRateCB();
	static void		SetTurretDesiredHeadingAndPitchCB();
	static void		RotateTurretToMatchPlayerCB();
	static void		RenderEnterTurretSeats();
	static void		SetFocusPedPreferredPassengerSeatIndexCB();
	static void		MakeFocusPedPopTypeScriptCB();
	static void		SetInVehicleContextOverrideCB();
	static void		ClearInVehicleContextOverrideCB();
	static void     CreateMule5();
	static void     DeleteMule5();


	//
	// Members
	//

	// Spawn A Ped
	static RegdPed	ms_pLastCreatedPed;
	static RegdVeh  ms_pPersonalVehicle;
	static RegdPed	ms_pPersonalVehicleOwner;
	static Vector3  ms_vSpecificSpawnLocation;
	static bool		ms_bUseSpecificSpawnLocation;
	static bool		ms_bPutInPlayersGroup;

	static int		ms_iPreferredPassengerSeatIndex;
	static bool		ms_bUseSpawnOffset;
	static float	ms_fSpawnOffsetX;
	static float	ms_fSpawnOffsetY;
	static u16		ms_iPlayerJackRate;
	static atArray<s32> ms_aDesiredStates;
	static bool		ms_bAttachToBike;
	static bool		ms_bTestOnBike;
	static bool		ms_bUseDefaultAimExitStateList;
	static bool		ms_bUseDefaultStateList;
	static bool		ms_bUseDefaultBikeStateList;
	static bool		ms_bUseDefaultPlaneStateList;
	static bool		ms_bDisableGroundAndOrientationFixup;
	static bool		ms_bUseFastBikeAnims;

	// Vehicle Tasks
	static s32		ms_iSelectedTaskState;
	static s32		ms_iSelectedTask;
	static s32		ms_eSeatRequestType;
	static s32		ms_iSeatRequested;
	static s32		ms_iEntryPoint;
	static s32		ms_iShuffleLink;

	// Enter/Exit Flags
	static bool		ms_bUseDirectEntryOnly;
	static bool		ms_bJumpOut;
	static bool		ms_bDelayForTime;
	static bool		ms_bJustPullPedOut;
	static bool		ms_bJackAnyOne;
	static bool		ms_bWarp;
	static bool		ms_bWarpAfterTime;
	static bool		ms_bWarpIfDoorIsBlocked;
	static bool		ms_bJustOpenDoor;
	static bool		ms_bDontCloseDoor;
	static bool		ms_bWarpToEntryPoint;
	static bool		ms_bWaitForEntryToBeClear;
	static float	ms_fDelayTime;
	static float	ms_fWarpTime;
	static float	ms_fMoveBlendRatio;

	static float	ms_fAimBlendDirection;

	// Drive By Params
	static s32		ms_iDriveByMode;
	static CAITarget ms_PedTarget;

	// Forced Vehicle Seat Usage
	static bool		ms_ForceUseRearSeatsOnly;
	static bool		ms_ForceUseFrontSeatsOnly;
	static bool		ms_ForceForAnyVehicle;
	static s32		ms_SelectedForcedVehicleUsageSlot;
	static s32		ms_iSeatIndex;

	// Debug rendering
	static bool		ms_bRenderDebug;
	static bool		ms_renderEnterTurret;
	static bool		ms_bRenderExclusiveDriverDebug;
	static bool		ms_bRenderLastDriverDebug;
	static bool		ms_bRenderEntryPoints;
	static bool		ms_bRenderLayoutInfo;
	static bool		ms_bRenderEntryAnim;
	static bool		ms_bRenderExitAnim;
	static bool		ms_bRenderThroughWindscreenAnim;
	static bool		ms_bRenderPickUpAnim;
	static bool		ms_bRenderSeats;
	static bool		ms_bRenderEntrySeatLinks;
	static bool		ms_bRenderComponentReservationText;
	static bool		ms_bRenderComponentReservationUsage;
	static bool		ms_bRenderEntryCollisionCheck;
	static bool		ms_bRenderEntryPointEvaluationDebug;
	static bool		ms_bRenderAnimStreamingDebug;
	static bool		ms_bRenderAnimStreamingText;
	static s32		ms_iDebugVerticalOffset;
	static bool		ms_bRenderEntryPointEvaluationTaskDebug;
	static bool		ms_bRenderDriveByTaskDebug;
	static bool		ms_bRenderDriveByYawLimitsForPed;
	static bool		ms_bOutputSeatRelativeOffsets;
	static bool		ms_bForcePlayerToUseSpecificSeat;
	static CRidableEntryPointEvaluationDebugInfo ms_EntryPointsDebug;
	static bool		ms_bRenderInitialSpawnPoint;
	static s32		ms_iDebugEntryPoint;
	static char		ms_szInVehicleContextOverride[128];

	// Vehicle locks
	static s32		ms_iVehicleLockState;
	static bool		ms_bRepairOnDebugVehicleLock;
	static float	ms_fDesiredTurretHeading;
	static float	ms_fDesiredTurretPitch;

	// Anim Streaming
	static bool		ms_bLoadAnimsWithVehicle;

	static u32		ms_ExclusiveDriverIndex;

	// RAG widget stuff
	static bkBank*		ms_pBank;
	static bkCombo*		ms_pTasksCombo;
	static bkButton*	ms_pCreateButton;
};
#endif // __BANK

#endif // INC_VEHICLE_DEBUG_H
