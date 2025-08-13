
#if __XENON
#include "system/xtl.h"
#endif

// Rage headers
#include "script/wrapper.h"

// Game headers
#include "control/replay/ReplayControl.h"
#include "Core/UserEntitlementService.h"
#include "script/script.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "system/EntitlementManager.h"
#include "Peds/ped.h"
#include "Peds/rendering/PedVariationPack.h"
#include "frontend/loadingscreens.h"
#include "scene/ExtraContent.h"
#include "system/system.h"
#include "streaming/IslandHopper.h"
#include "commands_dlc.h"

SCRIPT_OPTIMISATIONS()

// Keep in sync with definitions in commands_dlc.sch
struct scrUserEntitlements
{
	scrValue storyMode;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrUserEntitlements);


namespace dlc_commands
{

int CommandGetExtraContentCloudResult()
{
#if EC_CLOUD_MANIFEST
	return EXTRACONTENT.GetCloudContentResult();
#endif
	return 0;
}

bool CommandIsDLCPresent(int hash)
{
	// bug 1860606 - piggy back on this script function to do some tampering checks too.
	if (hash == (int)ATSTRINGHASH("DEBUG_DETECT",0xb119f6d))
	{
		return(CSystem::ms_bDebuggerDetected);
	}
	else if (hash == (int)ATSTRINGHASH("XX_I$RAWKST4H_D3V_XX",0x96f02ee6))
	{
		return NetworkInterface::IsRockstarDev();
	}
#if RSG_PC
    //This DLC is a pure backend entitlement on PC.
    else if ( hash == (int)ATSTRINGHASH("spPreorder",0xFC1D67B6))
    {
        return CEntitlementManager::IsEntitledToSpPreorderBonus();
    }
#endif
	else
	{
		return EXTRACONTENT.IsContentPresent((u32)hash);
	}
}

bool CommandCheckIsLoadingScreenActive()
{
#if __BANK
	if(EXTRACONTENT.BANK_EnableCloudWidgetsOverride())
	{
		return EXTRACONTENT.BANK_GetForceLoadingScreensState();
	}
#endif
	return CLoadingScreens::AreActive();
}

bool CommandCheckIsInitialLoadingScreenActive()
{
	return CLoadingScreens::IsInitialLoadingScreenActive();
}

bool CommandCheckCompatPackConfiguration()
{
#if EC_CLOUD_MANIFEST
	return EXTRACONTENT.CheckCompatPackConfiguration();
#endif // EC_DO_MANIFEST_CHECKS
	return true;
}

bool CommandCheckSaveGamePackConfiguration()
{
	return EXTRACONTENT.VerifySaveGameInstalledPackages();
}

int CommandGetCorruptedPacksCount()
{
	return EXTRACONTENT.GetCorruptedContentCount();
}

const char *GetCorruptedPackFileName(int index)
{
	return EXTRACONTENT.GetCorruptedContentFileName(index);
}

void CommandGetLocalPropData(int pedIndex, int anchorId, int propIndx, int& outVarInfoHash, int& outLocalIndex)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);

	if (pPed)
	{
		u32 tempOutHash = 0;

		CPedPropsMgr::GetLocalPropData(pPed, (eAnchorPoints)anchorId, propIndx, tempOutHash, outLocalIndex);
		outVarInfoHash = tempOutHash;
	}
}

int CommandGetGlobalPropData(int pedIndex, int anchorId, int varInfoHash, int localIndex)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);

	if (pPed)
	{
		return CPedPropsMgr::GetGlobalPropData(pPed, (eAnchorPoints)anchorId, varInfoHash, localIndex);
	}

	return -1;
}

void CommandGetLocalCompData(int pedIndex, int compId, int compIdx, int& outVarInfoHash, int& outLocalIndex)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);

	if (pPed)
	{
		u32 tempOutHash = 0;
		u32 tempOutLocalIdx = 0;

		CPedVariationPack::GetLocalCompData(pPed, (ePedVarComp)compId, (u8)compIdx, tempOutHash, tempOutLocalIdx);
		outVarInfoHash = tempOutHash;
		outLocalIndex = tempOutLocalIdx;
	}
}

int CommandGetGlobalCompData(int pedIndex, int compId, int varInfoHash, int localIndex)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex);

	if (pPed)
	{
		return CPedVariationPack::GetGlobalCompData(pPed, (ePedVarComp)compId, varInfoHash, localIndex);
	}

	return 0;
}

void CommandBailCloudDataFailed()
{
	CNetwork::Bail(sBailParameters(BAIL_CLOUD_FAILED, BAIL_CTX_NONE));
}

void CommandBailCompatPackMissing()
{
	CNetwork::Bail(sBailParameters(BAIL_COMPAT_PACK_CONFIG_INCORRECT, BAIL_CTX_NONE));
}

bool CommandCheckDLCCloudData()
{
#if EC_CLOUD_MANIFEST
	return EXTRACONTENT.HasCloudContentSuccessfullyLoaded();
#endif // EC_DO_MANIFEST_CHECKS
	return true;
}

#if EC_CLOUD_MANIFEST
bool CommandHasCloudRequestsFinished(int& bTimedOut, int uWaitDuration)
{
	return EXTRACONTENT.GetCloudContentState(bTimedOut,uWaitDuration);
#else
bool CommandHasCloudRequestsFinished(int& bTimedOut, int /*uWaitDuration*/)
{
	bTimedOut = false;
	return true;
#endif // EC_DO_MANIFEST_CHECKS
}

void CommandSetMapChangeState(s32 value)
{
	EXTRACONTENT.SetMapChangeState((eMapChangeStates)value);
}

s32 CommandGetMapChangeState()
{
	return EXTRACONTENT.GetMapChangeState();
}

void CommandRequestECManifest()
{
#if EC_CLOUD_MANIFEST
	Displayf("REQUEST_DLC_MANIFEST - Called by: %s",CTheScripts::GetCurrentScriptNameAndProgramCounter());
	EXTRACONTENT.EnsureManifestLoaded();
#endif // EC_DO_MANIFEST_CHECKS
}

bool CommandEverHadBadPackOrder()
{
	return EXTRACONTENT.GetEverHadBadPackOrder();
}

void CommandOnEnterSP()
{
#if !__FINAL
	Displayf("CommandOnEnterSP");
#endif

	CLoadingScreens::SetLoadingSP(true);

	g_MapDataStore.PerformStreamingIntegrityCheck();

	EXTRACONTENT.RevertContentChangeSetGroupForAll((u32)CCS_GROUP_MAP);
	EXTRACONTENT.ExecuteContentChangeSetGroupForAll((u32)CCS_GROUP_MAP_SP);

	g_MapDataStore.PerformStreamingIntegrityCheck();

#if GTA_REPLAY
	CReplayControl::ShutdownSession();
#endif // GTA_REPLAY
}

void CommandOnEnterMP()
{
#if !__FINAL
	Displayf("CommandOnEnterMP");
#endif

	CLoadingScreens::SetLoadingSP(false);

	g_MapDataStore.PerformStreamingIntegrityCheck();

	EXTRACONTENT.RevertContentChangeSetGroupForAll((u32)CCS_GROUP_MAP_SP);
	EXTRACONTENT.ExecuteContentChangeSetGroupForAll((u32)CCS_GROUP_MAP);

	g_MapDataStore.PerformStreamingIntegrityCheck();

	CFileLoader::MarkArchetypesWhichCannotBeDummy();

	CIslandHopper::GetInstance().DisableAllPreemptedImaps();

#if GTA_REPLAY
	CReplayControl::ShutdownSession();
#endif // GTA_REPLAY
}

bool CommandAreAnyCCSPending()
{
#if !__FINAL
	Displayf("CommandAreAnyCCSPending %i", EXTRACONTENT.AreAnyCCSPending());
#endif

	return EXTRACONTENT.AreAnyCCSPending();
}

bool CommandAreUserEntitlementsUpToDate()
{
#if RSG_GEN9
	const CUserEntitlementService& entitlementService = SUserEntitlementService::GetInstance();
	const bool success = entitlementService.AreEntitlementsUpToDate();
	return success;
#else // RSG_GEN9
	return false;
#endif // RSG_GEN9
}

bool CommandTryGetUserEntitlements(scrUserEntitlements* GEN9_STANDALONE_ONLY(out_entitlements))
{
#if RSG_GEN9
	const CUserEntitlementService& entitlementService = SUserEntitlementService::GetInstance();
	if (entitlementService.AreEntitlementsUpToDate()) 
	{
		UserEntitlements retrievedEntitlements = entitlementService.GetEntitlements();
		out_entitlements->storyMode.Uns = (u32)retrievedEntitlements.storyMode;
		return true;
	}
	else 
	{
		return false;
	}
#else // RSG_GEN9
	return false;
#endif // RSG_GEN9
}

#if RSG_GEN9
void CommandDeclareInMultiplayerThisFrame()
{
	SUserEntitlementService::GetInstance().SetInMultiplayerThisFrame();
}
#endif //RSG_GEN9

void SetupScriptCommands()
{
	// DLC
	SCR_REGISTER_SECURE(ARE_ANY_CCS_PENDING,0x10d127b791140a41, CommandAreAnyCCSPending);
	SCR_REGISTER_SECURE(IS_DLC_PRESENT,0x014d8c5910a5b01b, CommandIsDLCPresent );
	SCR_REGISTER_SECURE(DLC_CHECK_CLOUD_DATA_CORRECT,0xa9ed124071c968be, CommandCheckDLCCloudData );
	SCR_REGISTER_SECURE(GET_EXTRACONTENT_CLOUD_RESULT,0xdfe8bafb2c491fed, CommandGetExtraContentCloudResult );
	SCR_REGISTER_UNUSED(BAIL_COMPAT_PACK_MISSING,0xe6b10be161e4e720, CommandBailCompatPackMissing );
	SCR_REGISTER_UNUSED(BAIL_CLOUD_DATA_FAILED,0x45b4e36c590d5d27, CommandBailCloudDataFailed );
	SCR_REGISTER_SECURE(DLC_CHECK_COMPAT_PACK_CONFIGURATION,0x8346229c60b14feb, CommandCheckCompatPackConfiguration );
	SCR_REGISTER_UNUSED(DLC_CHECK_SAVEGAME_PACK_CONFIGURATION,0x3a51fbcf38249657, CommandCheckSaveGamePackConfiguration );
	SCR_REGISTER_UNUSED(DLC_GET_CORRUPTED_PACKS_COUNT,0x274cc1797dba5a17, CommandGetCorruptedPacksCount);
	SCR_REGISTER_UNUSED(DLC_GET_CORRUPTED_PACK_FILENAME,0x0a33d1d08b4fc5d9, GetCorruptedPackFileName);

	SCR_REGISTER_SECURE(GET_EVER_HAD_BAD_PACK_ORDER,0x87803acdd52e54f1, CommandEverHadBadPackOrder);

	SCR_REGISTER_UNUSED(SET_MAP_CHANGE_STATE,0x52747c299ffb11d3, CommandSetMapChangeState);

	SCR_REGISTER_UNUSED(GET_MAP_CHANGE_STATE,0xcda69baeff79542b, CommandGetMapChangeState);
	SCR_REGISTER_UNUSED(GET_LOCAL_PROP_DATA,0x00aa9b730d4e646b, CommandGetLocalPropData);
	SCR_REGISTER_UNUSED(GET_GLOBAL_PROP_DATA,0x6f620aed55686106, CommandGetGlobalPropData);
	SCR_REGISTER_UNUSED(GET_LOCAL_COMP_DATA,0x6e8de2d847355622, CommandGetLocalCompData);
	SCR_REGISTER_UNUSED(GET_GLBOAL_COMP_DATA,0x86ef1c19e089bd88, CommandGetGlobalCompData);
	SCR_REGISTER_SECURE(GET_IS_LOADING_SCREEN_ACTIVE,0x36f8d1c6ab25af2b, CommandCheckIsLoadingScreenActive);
	SCR_REGISTER_SECURE(GET_IS_INITIAL_LOADING_SCREEN_ACTIVE,0xc0bf59543a218f2c, CommandCheckIsInitialLoadingScreenActive);
	SCR_REGISTER_SECURE(HAS_CLOUD_REQUESTS_FINISHED,0x1b2f8af8aaba3482, CommandHasCloudRequestsFinished );
	SCR_REGISTER_UNUSED(REQUEST_DLC_MANIFEST,0x815e1c9a3854d573, CommandRequestECManifest);

	SCR_REGISTER_SECURE(ON_ENTER_SP,0x5cdcf73e81e343a7, CommandOnEnterSP);
	SCR_REGISTER_SECURE(ON_ENTER_MP,0xfa1496e29f396739, CommandOnEnterMP);

#if RSG_GEN9	
	SCR_REGISTER_UNUSED(ARE_USER_ENTITLEMENTS_UP_TO_DATE,0x5b2e954ce345fb7f, CommandAreUserEntitlementsUpToDate);
	SCR_REGISTER_UNUSED(TRY_GET_USER_ENTITLEMENTS,0x67cef75da3ff4c05, CommandTryGetUserEntitlements);
	DLC_SCR_REGISTER_SECURE(DECLARE_IN_MULTIPLAYER_THIS_FRAME, 0xaa3eabf6286b9f4e, CommandDeclareInMultiplayerThisFrame);
#endif // #if RSG_GEN9
}

}	//	end of namespace dlc_commands
