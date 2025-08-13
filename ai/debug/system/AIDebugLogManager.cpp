#include "ai\debug\system\AIDebugLogManager.h"

#if AI_DEBUG_OUTPUT_ENABLED

// rage headers
#include "fwnet\netinterface.h"

// game headers
#include "ai\debug\system\AIDebugInfo.h"
#include "ai\debug\types\AnimationDebugInfo.h"
#include "ai\debug\types\AttachmentsDebugInfo.h"
#include "ai\debug\types\CombatDebugInfo.h"
#include "ai\debug\types\DamageDebugInfo.h"
#include "ai\debug\types\EventsDebugInfo.h"
#include "ai\debug\types\LodDebugInfo.h"
#include "ai\debug\types\MovementDebugInfo.h"
#include "ai\debug\types\PedDebugInfo.h"
#include "ai\debug\types\RagdollDebugInfo.h"
#include "ai\debug\types\RelationshipDebugInfo.h"
#include "ai\debug\types\ScriptHistoryDebugInfo.h"
#include "ai\debug\types\TaskDebugInfo.h"
#include "ai\debug\types\VehicleControlDebugInfo.h"
#include "ai\debug\types\VehicleDebugInfo.h"
#include "ai\debug\types\VehicleFlagDebugInfo.h"
#include "ai\debug\types\VehicleNodeListDebugInfo.h"
#include "ai\debug\types\VehicleStuckDebugInfo.h"
#include "ai\debug\types\VehicleTaskDebugInfo.h"
#include "ai\debug\types\WantedDebugInfo.h"
#include "ai\debug\types\TargetingDebugInfo.h"
#include "camera\system\debug\CameraDebugInfo.h"
#include "Peds\Ped.h"
#include "scene\DynamicEntity.h"
#include "scene\world\GameWorld.h"

PARAM(dontLogRandomPeds, "[debug] Disable random peds logs");
PARAM(addGameplayLogOnBugCreation , "[debug] Enable automatic adding of gameplay logs when a bug is created");
PARAM(spewGameplayDebugOnBugCreation , "[debug] Enable gameplay debug spew to gameplay log when a bug is created");

void AILogging::LogEntityRejectionWithReason(const CDynamicEntity* pEntity, const char* szFunctionName, const char* szReason)
{
	CAILogManager::GetLog().Log("%s Entity %s (0x%p) rejected in %s due to %s\n", GetDynamicEntityIsCloneStringSafe(pEntity), GetDynamicEntityNameSafe(pEntity), pEntity, szFunctionName, szReason);
}

void AILogging::LogEntityRejectionWithIdentifierAndReason(const char* szIdentifier, const CDynamicEntity* pEntity, const char* szReason)
{
	CAILogManager::GetLog().Log("%s - %s Entity %s (0x%p) rejected due to %s\n", szIdentifier, GetDynamicEntityIsCloneStringSafe(pEntity), GetDynamicEntityNameSafe(pEntity), pEntity, szReason);
}

void AILogging::LogEntityAcceptanceWithReason(const CDynamicEntity* pEntity, const char* szFunctionName, const char* szReason)
{
	CAILogManager::GetLog().Log("%s Entity %s (0x%p) accepted in %s due to %s\n", GetDynamicEntityIsCloneStringSafe(pEntity), GetDynamicEntityNameSafe(pEntity), pEntity, szFunctionName, szReason);
}

void AILogging::LogEntityAcceptanceWithIdentifierAndReason(const char* szIdentifier, const CDynamicEntity* pEntity, const char* szReason)
{
	CAILogManager::GetLog().Log("%s - %s Entity %s (0x%p) accepted due to %s\n", szIdentifier, GetDynamicEntityIsCloneStringSafe(pEntity), GetDynamicEntityNameSafe(pEntity), pEntity, szReason);
}

void AILogging::FunctionLog(const char* szFunctionName, const char* szReason, ...)
{
	char TextBuffer[256];
	va_list args;
	va_start(args,szReason);
	vsprintf(TextBuffer,szReason,args);
	va_end(args);
	CAILogManager::GetLog().Log("%s\n %s\n", szFunctionName, TextBuffer);
}

void AILogDisplayStackTraceLine(size_t OUTPUT_ONLY(addr),const char *OUTPUT_ONLY(sym),size_t OUTPUT_ONLY(offset)) 
{
#	if __64BIT
	CAILogManager::GetLog().Log("0x%016" SIZETFMT "x - %s+0x%" SIZETFMT "x\n",addr,sym,offset);
#	else
	CAILogManager::GetLog().Log("0x%08" SIZETFMT "x - %s+0x%" SIZETFMT "x\n",addr,sym,offset);
#	endif
}

void AILogging::StackTraceLog(const unsigned destCount)
{
	static const unsigned suMaxDestCount = 6;
	size_t stackTrace[suMaxDestCount];
	const unsigned uEffectiveDestCount = Min(destCount, suMaxDestCount);
	sysStack::CaptureStackTrace(stackTrace, uEffectiveDestCount, 2);
	sysStack::PrintCapturedStackTrace(stackTrace, uEffectiveDestCount, &AILogDisplayStackTraceLine);
}

rage::atString AILogging::GetEntityLocalityNameAndPointer(const CDynamicEntity* pEntity)
{
	char TextBuffer[128];
	if (!pEntity)
		return atString("NULL");
	if(pEntity->IsNetworkClone())
	{
		sprintf(TextBuffer, "CLONE (%s) %s (0x%p)", GetDynamicEntityOwnerGamerNameSafe(pEntity), GetDynamicEntityNameSafe(pEntity), pEntity);
	}
	else
	{
		sprintf(TextBuffer, "LOCAL %s (0x%p)", GetDynamicEntityNameSafe(pEntity), pEntity);
	}

	return atString(TextBuffer);
}

const char* AILogging::GetDynamicEntityIsCloneStringSafe(const CDynamicEntity* pEntity)
{
	if (!pEntity)
		return "NULL";

	return pEntity->IsNetworkClone() ? "CLONE" : "LOCAL";
}

const char* AILogging::GetDynamicEntityOwnerGamerNameSafe(const CDynamicEntity* pEntity)
{
	if (!pEntity)
		return "NULL";

	if (!pEntity->GetNetworkObject())
		return "Local Machine";

	if (!pEntity->GetNetworkObject()->GetPlayerOwner())
		return "No Owner";

	return pEntity->GetNetworkObject()->GetPlayerOwner()->GetLogName();
}

const char* AILogging::GetDynamicEntityNameSafe(const CDynamicEntity* pEntity)
{
	if (!pEntity)
		return "NULL";

	if (!pEntity->GetNetworkObject())
	{
		if(pEntity->GetIsPhysical())
		{
			return static_cast<const CPhysical*>(pEntity)->GetDebugNameFromObjectID();
		}

		return pEntity->GetModelName();
	}

	return pEntity->GetNetworkObject()->GetLogName();
}

const char* AILogging::GetBooleanAsString(bool bVal)
{
	return bVal ? "TRUE" : "FALSE";
}

const char* AILogging::GetStringWithoutPrefix(const char* szPrefixToStrip, const char* szString)
{
	// TODO: Assert prefix exists at start of string or don't strip anything?
	size_t prefixLength = strlen(szPrefixToStrip);
	return szString + prefixLength;
}

rage::atString AILogging::GetEntityDescription(const CEntity* pEntity)
{
	if (pEntity)
	{
		static const u32 uBUFFER_SIZE = 256;
		static char szBuffer[uBUFFER_SIZE];

		if (pEntity->GetIsDynamic())
		{
			const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(pEntity);
			snprintf(szBuffer, uBUFFER_SIZE, "%s Entity %s (0x%p)", AILogging::GetDynamicEntityIsCloneStringSafe(pDynamicEntity), AILogging::GetDynamicEntityNameSafe(pDynamicEntity), pDynamicEntity);
		}
		else
		{
			snprintf(szBuffer, uBUFFER_SIZE, "NonDynamicEntity %s (0x%p)", pEntity->GetModelName(), pEntity);
		}

		return atString(szBuffer);
	}

	return atString("NoEntity");
}

netLog* CAILogManager::m_Log = NULL;
bool CAILogManager::m_IsInitialised = false;
bool CAILogManager::m_ShouldSpewOutputLogWhenBugAdded = false;
bool CAILogManager::m_ShouldSpewPedDetails = true;
bool CAILogManager::m_ShouldSpewVehicleDetails = true;
bool CAILogManager::m_ShouldAutoAddLogWhenBugAdded = false;

void CAILogManager::Init(unsigned UNUSED_PARAM(initMode))
{
	if (!m_IsInitialised)
	{
		m_ShouldSpewOutputLogWhenBugAdded = PARAM_spewGameplayDebugOnBugCreation.Get();
		m_ShouldAutoAddLogWhenBugAdded = PARAM_addGameplayLogOnBugCreation.Get();

		const unsigned LOG_BUFFER_SIZE = 16 *1024;
		static bool s_HasCreatedLog = false;
		m_Log = rage_new netLog("Gameplay.log", s_HasCreatedLog ? LOGOPEN_APPEND : LOGOPEN_CREATE, netInterface::GetDefaultLogFileTargetType(), netInterface::GetDefaultLogFileBlockingMode(), LOG_BUFFER_SIZE);
		m_IsInitialised = true;
		s_HasCreatedLog = true;
	}
}

void CAILogManager::ShutDown(unsigned UNUSED_PARAM(initMode))
{
	if (m_Log)
	{
		m_Log->Flush(true);
		delete m_Log;
	}

	m_Log = NULL;
	m_IsInitialised = false;
}

void CAILogManager::PrintAllGameplayLogs()
{
	m_Log->SetBlockingMode(LOG_BLOCKING);
	PrintAllDebugInfosForAllPeds();
	PrintAllDebugInfosForAllVehicles();
	PrintAllCameraSystemLogs();
	m_Log->Flush(true);
	m_Log->SetBlockingMode(LOG_NON_BLOCKING);
}

void CAILogManager::PrintAllDebugInfosForAllPeds()
{
	if(m_ShouldSpewPedDetails)
	{
		CPed* pPed = CGameWorld::FindLocalPlayer();
		PrintDebugInfosForPedToLog(*pPed);

		CPed::Pool *PedPool = CPed::GetPool();
		s32 i=PedPool->GetSize();
		while (i--)
		{
			pPed = PedPool->GetSlot(i);
			if (pPed && !pPed->IsLocalPlayer() && (!PARAM_dontLogRandomPeds.Get() || !pPed->PopTypeIsRandom()))
			{
				PrintDebugInfosForPedToLog(*pPed);
			}
		}
	}
}

void CAILogManager::PrintDebugInfosForPedToLog(CPed& rPed, const CVehicle* pVehicle)
{
	atArray<CAIDebugInfo*> debugInfos;

	CTasksFullDebugInfo taskFullInfo(&rPed);
	CAnimationDebugInfo animInfo(&rPed);
	CIkDebugInfo ikInfo(&rPed);
	CAttachmentsDebugInfo attachInfo(&rPed);
	CCombatTextDebugInfo combatInfo(&rPed);
	CDamageDebugInfo damageInfo(&rPed, false);
	CMovementTextDebugInfo moveInfo(&rPed);
	CPedFlagsDebugInfo pedFlagsInfo(&rPed);
	CRelationshipDebugInfo relationshipInfo(&rPed);
	CScriptHistoryDebugInfo scriptHistoryInfo(&rPed);
	CWantedDebugInfo wantedInfo(&rPed);
	CVehicleLockDebugInfo lockInfo(&rPed, NULL);
	CEventsDebugInfo eventsInfo(&rPed);
	CTargetingDebugInfo targetingInfo(&rPed);
	CPedNamesDebugInfo pedNamesInfo(&rPed);
	CPedGroupTextDebugInfo pedGroupTextInfo(&rPed);
	CStealthDebugInfo stealthInfo(&rPed);
	CMotivationDebugInfo motivationInfo(&rPed);
	CPedPersonalitiesDebugInfo pedPersonalitiesInfo(&rPed);
	CPedPopulationTypeDebugInfo	pedPopulationTypeInfo(&rPed);
	CPedGestureDebugInfo pedGestureInfo(&rPed);
	CRagdollDebugInfo ragdollInfo(&rPed);
	CPedTaskHistoryDebugInfo pedTaskHistoryInfo(&rPed);
	CPlayerDebugInfo playerInfo(&rPed);
	CParachuteDebugInfo ParachuteInfo(&rPed);
	

	debugInfos.PushAndGrow(&taskFullInfo);
	debugInfos.PushAndGrow(&animInfo);
	debugInfos.PushAndGrow(&ikInfo);
	debugInfos.PushAndGrow(&attachInfo);
	debugInfos.PushAndGrow(&combatInfo);
	debugInfos.PushAndGrow(&damageInfo);
	debugInfos.PushAndGrow(&moveInfo);
	debugInfos.PushAndGrow(&pedFlagsInfo);
	debugInfos.PushAndGrow(&relationshipInfo);
	debugInfos.PushAndGrow(&scriptHistoryInfo);
	debugInfos.PushAndGrow(&wantedInfo);
	debugInfos.PushAndGrow(&eventsInfo);
	debugInfos.PushAndGrow(&targetingInfo);
	debugInfos.PushAndGrow(&pedNamesInfo);
	debugInfos.PushAndGrow(&pedGroupTextInfo);
	debugInfos.PushAndGrow(&stealthInfo);
	debugInfos.PushAndGrow(&motivationInfo);
	debugInfos.PushAndGrow(&pedPersonalitiesInfo);
	debugInfos.PushAndGrow(&pedPopulationTypeInfo);
	debugInfos.PushAndGrow(&pedGestureInfo);
	debugInfos.PushAndGrow(&ragdollInfo);
	debugInfos.PushAndGrow(&pedTaskHistoryInfo);
	debugInfos.PushAndGrow(&playerInfo);
	debugInfos.PushAndGrow(&ParachuteInfo);

	if (rPed.IsLocalPlayer())
	{
		const CVehicle* pVehicleToVisualise = pVehicle ? pVehicle : CVehicleLockDebugInfo::GetNearestVehicleToVisualise(&rPed);
		if (pVehicleToVisualise)
		{
			lockInfo.SetVehicle(pVehicleToVisualise);
			debugInfos.PushAndGrow(&lockInfo);
		}
	}

	for (s32 i=0; i<debugInfos.GetCount(); ++i)
	{
		debugInfos[i]->Print();
	}
}

void CAILogManager::PrintAllDebugInfosForAllVehicles()
{
	if(m_ShouldSpewVehicleDetails)
	{
		CPed* pPed = CGameWorld::FindLocalPlayer();
		CVehicle* pLocalVehicle = pPed->GetMyVehicle();
		if(pLocalVehicle)
		{
			PrintDebugInfosForVehicleToLog(*pLocalVehicle);
		}

		CVehicle::Pool *VehiclePool = CVehicle::GetPool();
		s32 i = (s32)VehiclePool->GetSize();
		CVehicle* pVehicle = 0;
		while (i--)
		{
			pVehicle = VehiclePool->GetSlot(i);
			if (pVehicle && pVehicle != pLocalVehicle && pVehicle->GetIsVisibleInSomeViewportThisFrame())
			{
				PrintDebugInfosForVehicleToLog(*pVehicle);
			}
		}
	}
}

void CAILogManager::PrintDebugInfosForVehicleToLog(CVehicle& rVehicle)
{
	atArray<CVehicleDebugInfo*> debugInfos;

	CVehicleStatusDebugInfo vehicleStatusInfo(&rVehicle);
	CVehicleTaskDebugInfo taskFullInfo(&rVehicle);
	CVehicleNodeListDebugInfo nodeListInfo(&rVehicle);
	CVehicleStuckDebugInfo stuckInfo(&rVehicle);
	CVehicleControlDebugInfo controlInfo(&rVehicle);
	CVehicleFlagDebugInfo flagInfo(&rVehicle);

	debugInfos.PushAndGrow(&vehicleStatusInfo);
	debugInfos.PushAndGrow(&taskFullInfo);
	debugInfos.PushAndGrow(&nodeListInfo);
	debugInfos.PushAndGrow(&stuckInfo);
	debugInfos.PushAndGrow(&controlInfo);
	debugInfos.PushAndGrow(&flagInfo);

	for (s32 i=0; i<debugInfos.GetCount(); ++i)
	{
		debugInfos[i]->Print();
	}
}

void CAILogManager::PrintAllCameraSystemLogs()
{
	CCameraDebugInfo cameraDebugInfo;
	cameraDebugInfo.Print();
}

#if __BANK
bkBank* CAILogManager::ms_pBank	= NULL;

void CAILogManager::InitBank()
{
	if (ms_pBank)
	{
		ShutdownBank();
	}

	// Create the weapons bank
	ms_pBank = &BANKMGR.CreateBank("AI Logging", 0, 0, false); 
	if (ms_pBank)
	{
		AddWidgets(*ms_pBank);
	}
}

void CAILogManager::AddWidgets(bkBank& bank)
{
	bank.AddToggle("Should Spew Output Log When Bug Added", &m_ShouldSpewOutputLogWhenBugAdded);
	bank.AddToggle("Should Auto Add Gameplay Log When Bug Added", &m_ShouldAutoAddLogWhenBugAdded);
	bank.AddToggle("Should Spew Ped details", &m_ShouldSpewPedDetails);
	bank.AddToggle("Should Spew Vehicle details", &m_ShouldSpewVehicleDetails);
}

void CAILogManager::ShutdownBank()
{
	if (ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}

	ms_pBank = NULL;
}
#endif // __BANK

#endif // AI_DEBUG_OUTPUT_ENABLED
