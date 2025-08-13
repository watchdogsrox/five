#include "ai/debug/types/VehicleDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

// rage headers
#include "fwdecorator/decoratorExtension.h"
#include "fwscript/scriptinterface.h"

// game headers
#include "ai/debug/types/TaskDebugInfo.h"
#include "ai/debug/types/VehicleTaskDebugInfo.h"
#include "ai/debug/types/VehicleNodeListDebugInfo.h"
#include "ai/debug/types/VehicleStuckDebugInfo.h"
#include "ai/debug/types/VehicleControlDebugInfo.h"
#include "ai/debug/types/VehicleFlagDebugInfo.h"
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PlayerInfo.h"
#include "Peds/Relationships.h"
#include "modelinfo/ModelSeatInfo.h"
#include "Network/General/NetworkVehicleModelDoorLockTable.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "script/commands_player.h"
#include "script/script.h"
#include "system/controlMgr.h"
#include "task/Vehicle/TaskVehicleBase.h"
#include "vehicles/vehicle.h"
#include "vehicles/Planes.h"
#include "vehicles/Heli.h"
#include "VehicleAI/VehicleAiLod.h"
#include "VehicleAI/VehicleIntelligence.h"
#include "text/messages.h"

extern const char* parser_CVehicleEnterExitFlags__Flags_Strings[];

const CVehicle* CVehicleLockDebugInfo::GetNearestVehicleToVisualise(const CPed* pPed)
{
	CEntityScannerIterator nearbyVehicles = pPed->GetPedIntelligence()->GetNearbyVehicles();
	CVehicle* pNearestVehicle = static_cast<CVehicle*>(nearbyVehicles.GetFirst());
	if (pNearestVehicle)
	{
		const Vec3V vPedPos = pPed->GetTransform().GetPosition();
		const Vec3V vVehPos = pNearestVehicle->GetTransform().GetPosition();

		static dev_float MAX_DIST_TO_CONSIDER_VEH = 15.0f;
		if (IsLessThanAll(DistSquared(vPedPos, vVehPos), ScalarVFromF32(rage::square(MAX_DIST_TO_CONSIDER_VEH))))
		{
			return pNearestVehicle;
		}
	}
	return NULL;
}

CVehicleLockDebugInfo::CVehicleLockDebugInfo(const CPed* pPed, const CVehicle* pVeh, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
	, m_Veh(pVeh)
{

}

void CVehicleLockDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("VEHICLE LOCK INFO");
		PushIndent();
			PushIndent();
				WriteHeader("GENERAL");
					PushIndent();
						PrintLn("This Ped: %s | %s | %s | %s | 0x%p | <%.2f,%.2f,%.2f>", m_Ped->PopTypeIsMission() ? "MISSION" : "AMBIENT", AILogging::GetDynamicEntityNameSafe(m_Ped), m_Ped->GetModelName(), AILogging::GetDynamicEntityOwnerGamerNameSafe(m_Ped), m_Ped.Get(), VEC3V_ARGS(m_Ped->GetTransform().GetPosition()));
						PrintLn("Vehicle: %s | %s | %s | %s | 0x%p | <%.2f,%.2f,%.2f>", m_Veh->PopTypeIsMission() ? "MISSION" : "AMBIENT", AILogging::GetDynamicEntityNameSafe(m_Veh), m_Veh->GetModelName(), AILogging::GetDynamicEntityOwnerGamerNameSafe(m_Veh), m_Veh.Get(), VEC3V_ARGS(m_Veh->GetTransform().GetPosition()));
						PrintLn("Created by : %s", m_Veh->GetScriptThatCreatedMe() ? m_Veh->GetScriptThatCreatedMe()->GetLogName() : "CODE");
						if (m_Veh->PopTypeIsMission())
						{
							CVehicle* pVeh = const_cast<CVehicle*>(m_Veh.Get());
							PrintLn("VEH_INDEX = %i", CTheScripts::GetGUIDFromEntity(*pVeh));
						}
					PopIndent();
			PopIndent();
		
			PushIndent();
				WriteHeader("VEHICLE");
					PushIndent();
						PrintPersonalVehicleDebug();
						WriteBoolValueTrueIsGood("bConsideredByPlayer", m_Veh->m_nVehicleFlags.bConsideredByPlayer);
						WriteBoolValueTrueIsBad("bNotDriveable", m_Veh->m_nVehicleFlags.bNotDriveable);
						WriteBoolValueTrueIsBad("DontTryToEnterThisVehicleIfLockedForPlayer", m_Veh->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer);
						WriteBoolValueTrueIsBad("bWasReused", m_Veh->m_nVehicleFlags.bWasReused);
						WriteBoolValueTrueIsBad("Wrecked", m_Veh->GetStatus() == STATUS_WRECKED);
						PrintLn("Lock State: %s", VehicleLockStateStrings[m_Veh->GetCarDoorLocks()]);
						PrintDoorDebug();
						PrintPartialDoorLockDebug();
						PrintVehicleLockDebug();
						for (u32 i=0; i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; ++i)
						{
							PrintLn("Vehicle Exclusive Driver [%i]: %s", i, AILogging::GetDynamicEntityNameSafe(m_Veh->GetExclusiveDriverPed(i)));
						}
						PrintLn("Last Driver: %s (%d)", AILogging::GetDynamicEntityNameSafe(m_Veh->GetLastDriver()), m_Veh->m_LastTimeWeHadADriver);
						WriteBoolValueTrueIsBad("AI can use exclusive seats", m_Veh->m_nVehicleFlags.bAICanUseExclusiveSeats);
					PopIndent();
			PopIndent();

			PushIndent();
				WriteHeader("PED");
					PushIndent();
						PrintVehicleEntryConfigDebug();
						WritePedConfigFlag(*m_Ped, CPED_CONFIG_FLAG_PreventAutoShuffleToDriversSeat, true);
						WritePedConfigFlag(*m_Ped, CPED_CONFIG_FLAG_PreventAutoShuffleToTurretSeat, true);
						WritePedConfigFlag(*m_Ped, CPED_CONFIG_FLAG_OnlyUseForcedSeatWhenEnteringHeliInGroup, true);
						WritePedConfigFlag(*m_Ped, CPED_CONFIG_FLAG_ForcedToUseSpecificGroupSeatIndex, true);
						WritePedConfigFlag(*m_Ped, CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar, true);
						ColorPrintLn(Color_green, "Ped Preferred Passenger Seat Index: %i", m_Ped->m_PedConfigFlags.GetPassengerIndexToUseInAGroup());
					PopIndent();
			PopIndent();

			if (m_Ped->IsLocalPlayer())
			{
				PushIndent();
					WriteHeader("PLAYER CONTROLS");
						PushIndent();
							WriteBoolValueTrueIsBad("Not allowed to enter any vehicle", m_Ped->GetPlayerResetFlag(CPlayerResetFlags::PRF_NOT_ALLOWED_TO_ENTER_ANY_CAR));
							WriteBoolValueTrueIsGood("INPUT_ENTER ENABLED", !CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_ENTER));
							WriteBoolValueTrueIsGood("INPUT_VEH_EXIT ENABLED", !CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_VEH_EXIT));
						PopIndent();
				PopIndent();
			}

			PushIndent();
				WriteHeader("MISC");
					PushIndent();
						PrintVehicleWeaponDebug();
					PopIndent();
			PopIndent();
}

void CVehicleLockDebugInfo::Visualise()
{
	if (!ValidateInput())
		return;

	Vec3V vPedPos = m_Ped->GetTransform().GetPosition();
	Vec3V vVehPos = m_Veh->GetTransform().GetPosition();
	grcDebugDraw::Arrow(vPedPos + Vec3V(V_Z_AXIS_WZERO), vVehPos, 0.25f, Color_blue);
}

bool CVehicleLockDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	if (!m_Veh)
		return false;

	return true;
}

void CVehicleLockDebugInfo::PrintPersonalVehicleDebug()
{
	CVehicle* pNonConstVeh = const_cast<CVehicle*>(m_Veh.Get());
	bool isMyPersonalVehicle = false;
	bool hasName = false;
	char personalOwnerName[128];
	const char* szPersonalVehicleOwnerName = GetPersonalVehicleOwnerName(pNonConstVeh, isMyPersonalVehicle);
	if (szPersonalVehicleOwnerName)
	{
		hasName = true;
		formatf(personalOwnerName, szPersonalVehicleOwnerName);
	}
	PrintLn("Vehicle Is Personal : %s (%s, %s)", AILogging::GetBooleanAsString(pNonConstVeh->IsPersonalVehicle()), pNonConstVeh->IsPersonalVehicle() ? (isMyPersonalVehicle ? "MINE" : "OTHER") : "-", hasName ? personalOwnerName : "-" );
}

void CVehicleLockDebugInfo::PrintPartialDoorLockDebug()
{
	if (m_Veh->GetCarDoorLocks() != CARLOCK_PARTIALLY)
		return;

	for (s32 iEntryPointIndex=0; iEntryPointIndex<m_Veh->GetNumberEntryExitPoints(); iEntryPointIndex++)
	{
		if (!m_Veh->IsEntryIndexValid(iEntryPointIndex))
			continue;

		const CEntryExitPoint* pEntryPoint = m_Veh->GetEntryExitPoint(iEntryPointIndex);
		if (!pEntryPoint)
			continue;
		
		if (pEntryPoint->GetDoorBoneIndex() == -1)
			continue;
		
		const CCarDoor* pDoor = m_Veh->GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());
		if (pDoor)
		{
			PrintLn("Entry Point: %i Door Lock State: %s", iEntryPointIndex, VehicleLockStateStrings[pDoor->m_eDoorLockState]);
		}	
	}
}

void CVehicleLockDebugInfo::PrintDoorDebug()
{
	TUNE_GROUP_BOOL(CARDOOR_DEBUG, RENDER_DEBUG, false);
	if (!RENDER_DEBUG)
		return;

	for (s32 iEntryPointIndex=0; iEntryPointIndex<m_Veh->GetNumberEntryExitPoints(); iEntryPointIndex++)
	{
		if (!m_Veh->IsEntryIndexValid(iEntryPointIndex))
			continue;

		const CEntryExitPoint* pEntryPoint = m_Veh->GetEntryExitPoint(iEntryPointIndex);
		if (!pEntryPoint)
			continue;

		if (pEntryPoint->GetDoorBoneIndex() == -1)
			continue;

		const CCarDoor* pDoor = m_Veh->GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());
		if (pDoor)
		{
			PrintLn("Door %i", iEntryPointIndex);
			PushIndent();
			PrintLn("m_pedRemoteUsingDoor = %s", AILogging::GetBooleanAsString(pDoor->GetPedRemoteUsingDoor()));
			PrintLn("m_fCurrentRatio = %.2f", pDoor->GetDoorRatio());
			PrintLn("m_fTargetRatio = %.2f", pDoor->GetTargetDoorRatio());
			// u32 i = 0;
			// while (i < CCarDoor::RENDER_INVISIBLE)
			// {
			// 	PrintLn("%s : %s", CCarDoor::GetDoorFlagString((CCarDoor::eDoorFlags)i), AILogging::GetBooleanAsString(pDoor->GetFlag(i)));
			// 	i = (i == 0) ? 1 : i * 2;
			// }
			PopIndent();
		}	
	}
}

void CVehicleLockDebugInfo::PrintVehicleLockDebug()
{
	// networked vehicles can be locked for individual players
	if (!m_Ped->IsAPlayerPed() || !m_Ped->GetNetworkObject())
		return;

	const CNetGamePlayer* pPlayer = m_Ped->GetNetworkObject()->GetPlayerOwner();
	if (!aiVerify(pPlayer->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
		return;

	if (!m_Veh->GetNetworkObject())
		return;

	const CNetObjVehicle* pNetObjVeh = static_cast<const CNetObjVehicle*>(m_Veh->GetNetworkObject());

	PhysicalPlayerIndex playerIndex = pPlayer->GetPhysicalPlayerIndex();
	const bool bLockedForPlayer = pNetObjVeh->IsLockedForPlayer(playerIndex) ? true : false;

	WriteBoolValueTrueIsBad("Locked For Player", bLockedForPlayer);
	WriteBoolValueTrueIsBad("Player Lock", (pNetObjVeh->GetPlayerLocks() & (1<<playerIndex)) != 0);
	const int teamIndex = pPlayer->GetTeam();
	WriteBoolValueTrueIsBad("Team Lock",  (pNetObjVeh->GetTeamLocks() & (1<<teamIndex)) != 0);
	WriteBoolValueTrueIsBad("Team Lock Override", (pNetObjVeh->GetTeamLockOverrides() & (1<<teamIndex)) != 0);
	WriteBoolValueTrueIsBad("Locked For Non Script Players", pNetObjVeh->IsLockedForNonScriptPlayers());

	const bool bIsScriptVeh = pNetObjVeh->GetScriptObjInfo() ? true : false;
	WriteBoolValueTrueIsBad("Script Veh", bIsScriptVeh);

	if (bIsScriptVeh && scriptInterface::IsScriptRegistered(pNetObjVeh->GetScriptObjInfo()->GetScriptId()) )
	{
		PrintLn("Owner Gamer ID", pNetObjVeh->GetPlayerOwner() ? pNetObjVeh->GetPlayerOwner()->GetLogName() : "None");
		PrintLn("Script Registered", pNetObjVeh->GetScriptObjInfo()->GetScriptId().GetLogName());
		PrintLn("Owner Script", scriptInterface::IsScriptRegistered(pNetObjVeh->GetScriptObjInfo()->GetScriptId()));
		WriteBoolValueTrueIsGood("Player Is Participant", scriptInterface::IsPlayerAParticipant(pNetObjVeh->GetScriptObjInfo()->GetScriptId(), playerIndex));
	}

	if (pPlayer->IsLocal())
	{
		int GUID = CTheScripts::GetGUIDFromEntity(*const_cast<CVehicle*>(m_Veh.Get()));

		// Am I locked by model type (e.g all infernus cars are locked)...
		const bool bModelTypeLocked = CNetworkVehicleModelDoorLockedTableManager::IsVehicleModelLocked(playerIndex, m_Veh->GetModelIndex());
		WriteBoolValueTrueIsBad("Model Type Locked", bModelTypeLocked);

		// is this vehicle registered and specifically locked?
		const bool bModelInstanceLocked = CNetworkVehicleModelDoorLockedTableManager::IsVehicleInstanceRegistered(playerIndex, GUID) && CNetworkVehicleModelDoorLockedTableManager::IsVehicleInstanceLocked(playerIndex, GUID);
		WriteBoolValueTrueIsBad("Model Instance Locked", bModelInstanceLocked);
	}
}

void CVehicleLockDebugInfo::PrintVehicleWeaponDebug()
{
	const CVehicleWeaponMgr* pWpnMgr = m_Veh->GetVehicleWeaponMgr();
	WriteBoolValueTrueIsGood("Vehicle has vehicle weapon manager", pWpnMgr ? true : false);

	if (!pWpnMgr)
		return;

	PrintLn("Num weapons: %i", pWpnMgr->GetNumVehicleWeapons());
	if (pWpnMgr->GetNumVehicleWeapons() <= 0)
		return;

	for (s32 i=0; i<m_Veh->GetSeatManager()->GetMaxSeats(); ++i)
	{
		if (!m_Veh->IsSeatIndexValid(i))
			continue;

		atArray<const CVehicleWeapon*> weapons;
		atArray<const CTurret*> turrets;
		pWpnMgr->GetWeaponsAndTurretsForSeat(i, weapons, turrets);

		if (weapons.GetCount() > 0)
		{
			PrintLn("Seat %i | Num Weapons %i", i, weapons.GetCount());
			for (s32 w=0; w<weapons.GetCount(); ++w)
			{
				PushIndent();
				PrintLn("Weapon %s", weapons[w]->GetWeaponInfo() ? weapons[w]->GetWeaponInfo()->GetName() : "NULL");
				PopIndent();
			}
		}

		if (turrets.GetCount() > 0)
		{
			PrintLn("Seat %i | Num Turrets %i", i, turrets.GetCount() );
		}
	}

}

void CVehicleLockDebugInfo::PrintVehicleEntryConfigDebug()
{
	const CSyncedPedVehicleEntryScriptConfig* pVehicleEntryConfig = m_Ped->GetVehicleEntryConfig();
	if (!pVehicleEntryConfig)
	{
		PrintLn("Vehicle Entry Config = NULL");
	}
	else
	{
		PrintLn("Vehicle Entry Config = 0x%p", pVehicleEntryConfig);
		PushIndent();
		for (s32 i=0; i<CSyncedPedVehicleEntryScriptConfig::MAX_VEHICLE_ENTRY_DATAS; ++i)
		{
			atString strFlagString;
			const s32 iSeatIndex = pVehicleEntryConfig->GetSeatIndex(i);

			if (pVehicleEntryConfig->GetFlags(i) & CSyncedPedVehicleEntryScriptConfigData::ForceUseFrontSeats)
			{
				strFlagString += "ForceUseFrontSeats ";
			}
			if (pVehicleEntryConfig->GetFlags(i) & CSyncedPedVehicleEntryScriptConfigData::ForceUseRearSeats)
			{
				strFlagString += "ForceUseRearSeats ";
			}
			if (pVehicleEntryConfig->GetFlags(i) & CSyncedPedVehicleEntryScriptConfigData::ForceForAnyVehicle)
			{
				strFlagString += "ForceForAnyVehicle ";
			}
			PrintLn("Slot %i : Vehicle = %s : Flags = %s : SeatIndex %i", i, AILogging::GetDynamicEntityNameSafe(pVehicleEntryConfig->GetVehicle(i)), strFlagString.c_str(), iSeatIndex);
		}
		PopIndent();
	}
}

const char* CVehicleLockDebugInfo::GetPersonalVehicleOwnerName(CVehicle* pNonConstVeh, bool& bIsMyVehicle)
{
	if (!pNonConstVeh->IsPersonalVehicle())
		return NULL;

	s32 iVehicleGuid = CTheScripts::GetGUIDFromEntity(*pNonConstVeh);
	if (iVehicleGuid == NULL_IN_SCRIPTING_LANGUAGE)
		return NULL;

	fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(iVehicleGuid);
	if (!pBase)
		return NULL;

	atHashWithStringBank decKey("Player_Vehicle");
	s32 iNameHash = 0;
	if (!fwDecoratorExtension::Get((*pBase),decKey,iNameHash))
		return NULL;

	CNetGamePlayer* pPlayer = (m_Ped && m_Ped->GetNetworkObject()) ? m_Ped->GetNetworkObject()->GetPlayerOwner() : NULL;
	if (pPlayer && pPlayer->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX)
	{
		const char* szPlayerName = player_commands::GetPlayerName(pPlayer->GetPhysicalPlayerIndex(), "CVehicleLockDebugInfo::GetPersonalVehicleName");
		s32 iPlayerNameHash = (s32)atStringHash(szPlayerName);
		if (iNameHash == iPlayerNameHash)
		{
			bIsMyVehicle = true;
			return szPlayerName;
		}
	}

	const s32 iPedPlayerIdx = pPlayer ? pPlayer->GetPhysicalPlayerIndex() : INVALID_PLAYER_INDEX;
	for (s32 iPlayerIndex=0; iPlayerIndex<MAX_NUM_PHYSICAL_PLAYERS; ++iPlayerIndex)
	{
		if (iPlayerIndex != iPedPlayerIdx)
		{
			const char* szPlayerName = player_commands::GetPlayerName(iPlayerIndex, "CVehicleLockDebugInfo::GetPersonalVehicleName");
			s32 iPlayerNameHash = (s32)atStringHash(szPlayerName);
			if (iNameHash == iPlayerNameHash)
			{
				bIsMyVehicle = false;
				return szPlayerName;
			}
		}
	}
	return NULL;
}

CEntryPointEarlyRejectionEvent::CEntryPointEarlyRejectionEvent(s32 iRejectedEntryPointIndex, const char* szRejectionReason, ...)
	: m_RejectedEntryPointIndex(iRejectedEntryPointIndex)
{
	char TextBuffer[256];
	va_list args;
	va_start(args,szRejectionReason);
	vsprintf(TextBuffer,szRejectionReason,args);
	va_end(args);
	formatf(m_szRejectionReason, TextBuffer);
}

void CEntryPointEarlyRejectionEvent::Print()
{
	PushIndent(4);
		PrintLn("--------------------------------------------------------------------");
			PushIndent(2);
			WriteDataValue("Rejected Entry Point Index", "%i", m_RejectedEntryPointIndex);
			WriteDataValue("Reason", m_szRejectionReason);
		PopIndent(2);
	PopIndent(4);
}

CEntryPointScoringEvent::CEntryPointScoringEvent(s32 iEntryPointIndex, s32 iEntryPointPriority, float fScore, const char* szUpdateReason, ...)
	: m_EntryPointIndex(iEntryPointIndex)
	, m_EntryPointPriority(iEntryPointPriority)
	, m_Score(fScore)
{
	char TextBuffer[256];
	va_list args;
	va_start(args,szUpdateReason);
	vsprintf(TextBuffer,szUpdateReason,args);
	va_end(args);
	formatf(m_szUpdateReason, TextBuffer);
}

void CEntryPointScoringEvent::Print()
{
	PushIndent(4);
		PrintLn("--------------------------------------------------------------------");
		PushIndent(2);
			WriteDataValue("Entry Index", "%i", m_EntryPointIndex);
			WriteDataValue("Priority", "%i", m_EntryPointPriority);
			WriteDataValue("New Score", "%.2f", m_Score);
			WriteDataValue("Reason", m_szUpdateReason);
		PopIndent(2);
	PopIndent(4);
}

CEntryPointPriorityScoringEvent::CEntryPointPriorityScoringEvent(s32 iScoredEntryPointIndex, s32 iNewPriorityScore, const char* szUpdateReason, ...)
	: m_ScoredEntryPointPriority(iScoredEntryPointIndex)
	, m_NewPriorityScore(iNewPriorityScore)
{
	char TextBuffer[256];
	va_list args;
	va_start(args,szUpdateReason);
	vsprintf(TextBuffer,szUpdateReason,args);
	va_end(args);
	formatf(m_szUpdateReason, TextBuffer);
}

void CEntryPointPriorityScoringEvent::Print()
{
	PushIndent(4);
		PrintLn("--------------------------------------------------------------------");
			PushIndent(2);
				WriteDataValue("Entry Index", "%i", m_ScoredEntryPointPriority);
				WriteDataValue("New Priority", "%i", m_NewPriorityScore);
				WriteDataValue("Reason", m_szUpdateReason);
			PopIndent(2);
	PopIndent(4);
}

CRidableEntryPointEvaluationDebugInfo::CRidableEntryPointEvaluationDebugInfo(DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
{
	Reset();
}

void CRidableEntryPointEvaluationDebugInfo::Reset()
{
	m_RejectionEvents.clear();
	m_PriorityScoringEvents.clear();
	m_ScoringEvents.clear();
	m_HasBeenInitialised = false;
	m_Warping = false;
	m_EvaluatingPed = NULL;
	m_Ridable = NULL;
	m_ChosenEntryPointIndex = -1;
	m_ChosenSeatIndex = -1;
	m_SeatRequestType = SR_Any;
	m_SeatRequested = -1;
	ResetIndent();
}

void CRidableEntryPointEvaluationDebugInfo::Init(const CPed* pEvaluatingPed, const CDynamicEntity* pRidableEnt, s32 iSeatRequest, s32 iSeatRequested, bool bWarping, VehicleEnterExitFlags iFlags)
{
	Reset();
	m_EvaluatingPed = pEvaluatingPed;
	m_Ridable = pRidableEnt;
	m_HasBeenInitialised = true;
	m_Warping = bWarping;
	m_VehicleEnterExitFlags = iFlags;
	m_SeatRequestType = iSeatRequest;
	m_SeatRequested = iSeatRequested;
}

void CRidableEntryPointEvaluationDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("ENTRY POINT EVALUATION");
		PushIndent(3);
			const CSyncedPedVehicleEntryScriptConfig* pVehicleEntryConfig = m_EvaluatingPed->GetVehicleEntryConfig();
			if (!pVehicleEntryConfig)
			{
				PrintLn("Vehicle Entry Config = NULL");
			}
			else
			{
				PrintLn("Vehicle Entry Config = 0x%p", pVehicleEntryConfig);
				PushIndent();
				for (s32 i=0; i<CSyncedPedVehicleEntryScriptConfig::MAX_VEHICLE_ENTRY_DATAS; ++i)
				{
					atString strFlagString;
					const s32 iSeatIndex = pVehicleEntryConfig->GetSeatIndex(i);
					if (pVehicleEntryConfig->GetFlags(i) & CSyncedPedVehicleEntryScriptConfigData::ForceUseFrontSeats)
					{
						strFlagString += "ForceUseFrontSeats ";
					}
					if (pVehicleEntryConfig->GetFlags(i) & CSyncedPedVehicleEntryScriptConfigData::ForceUseRearSeats)
					{
						strFlagString += "ForceUseRearSeats ";
					}
					if (pVehicleEntryConfig->GetFlags(i) & CSyncedPedVehicleEntryScriptConfigData::ForceForAnyVehicle)
					{
						strFlagString += "ForceForAnyVehicle ";
					}
					PrintLn("Slot %i : Vehicle = %s : Flags = %s : SeatIndex %i", i, AILogging::GetDynamicEntityNameSafe(pVehicleEntryConfig->GetVehicle(i)), strFlagString.c_str(), iSeatIndex);
				}
				PopIndent();
			}
			WriteDataValue("Prefered Passenger Index To Use In A Group", "%i", m_EvaluatingPed->m_PedConfigFlags.GetPassengerIndexToUseInAGroup());
			WriteDataValue("ForcedToUseSpecificGroupSeatIndex", AILogging::GetBooleanAsString(m_EvaluatingPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcedToUseSpecificGroupSeatIndex)));
			WriteDataValue("OnlyUseForcedSeatWhenEnteringHeliInGroup", AILogging::GetBooleanAsString(m_EvaluatingPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyUseForcedSeatWhenEnteringHeliInGroup)));
			if (m_EvaluatingPed->GetPedsGroup())
			{
				WriteDataValue("Group Index", "%i", m_EvaluatingPed->GetPedsGroup()->GetGroupIndex());	
			}
			const CRelationshipGroup* pRelationshipGroup = m_EvaluatingPed->GetPedIntelligence()->GetRelationshipGroup();
			const char* szRelGroupName = pRelationshipGroup ? pRelationshipGroup->GetName().GetCStr() : "NULL";
			WriteDataValue("Evaluating Ped", AILogging::GetDynamicEntityNameSafe(m_EvaluatingPed));
			WriteDataValue("Evaluating Ped Relationship Group", szRelGroupName);	
			WriteDataValue("Ridable Entity", AILogging::GetDynamicEntityNameSafe(m_Ridable));
			WriteDataValue("Seat Request Type", CSeatAccessor::GetSeatRequestTypeString((SeatRequestType)m_SeatRequestType));
			WriteDataValue("Requested Seat Index", "%i", m_SeatRequested);
			WriteDataValue("Warping", AILogging::GetBooleanAsString(m_Warping));
			PrintPedsInSeats();
			PrintVehicleEntryExitFlags();
			WriteDataValue("Chosen Entry Point Index", "%i", m_ChosenEntryPointIndex);
			WriteDataValue("Chosen Seat Index", "%i", m_ChosenSeatIndex);
			PrintEarlyRejectionEvents();
			PrintPriorityScoringEvents();
			PrintScoringEvents();
}

bool CRidableEntryPointEvaluationDebugInfo::ValidateInput()
{
	if (!m_HasBeenInitialised)
		return false;

	if (!m_EvaluatingPed)
		return false;

	if (!m_Ridable)
		return false;

	return true;
}

void CRidableEntryPointEvaluationDebugInfo::PrintPedsInSeats()
{
	if (!m_Ridable->GetIsTypeVehicle())
		return;

	const CVehicle& rVeh = static_cast<const CVehicle&>(*m_Ridable);
	PushIndent(5);
		PrintLn("Peds In Or Using Seats");
		PushIndent(2);
		for (s32 i=0; i<rVeh.GetSeatManager()->GetMaxSeats(); i++)
		{
			const CPed* pPedInOrUsingSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(rVeh, i);
			const CRelationshipGroup* pRelationshipGroup = pPedInOrUsingSeat ? pPedInOrUsingSeat->GetPedIntelligence()->GetRelationshipGroup() : NULL;
			const char* szRelGroupName = pRelationshipGroup ? pRelationshipGroup->GetName().GetCStr() : "NULL";
			PrintLn("Seat : %i, Ped : %s, In Vehicle : %s, Relationship Group : %s, Is Friendly : %s, PlayersDontDragMeOutOfCar : %s", i, AILogging::GetDynamicEntityNameSafe(pPedInOrUsingSeat), pPedInOrUsingSeat ? AILogging::GetBooleanAsString(pPedInOrUsingSeat->GetIsInVehicle()) : "FALSE", szRelGroupName, pPedInOrUsingSeat ? AILogging::GetBooleanAsString(CTaskVehicleFSM::ArePedsFriendly(m_EvaluatingPed, pPedInOrUsingSeat)) : "FALSE", pPedInOrUsingSeat ? AILogging::GetBooleanAsString(pPedInOrUsingSeat->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar)) : "NULL"); 
		}
		PopIndent(2);
	PopIndent(5);
}

void CRidableEntryPointEvaluationDebugInfo::PrintVehicleEntryExitFlags()
{
	PushIndent(5);
		PrintLn("Vehicle Entry Exit Flags");
		PushIndent(2);
		for (s32 i=0; i<m_VehicleEnterExitFlags.BitSet().GetNumBits(); i++)
		{
			if (m_VehicleEnterExitFlags.BitSet().IsSet((CVehicleEnterExitFlags::Flags)i))
			{
				PrintLn(parser_CVehicleEnterExitFlags__Flags_Strings[i]); 
			}
		}
		PopIndent(2);
	PopIndent(5);
}

void CRidableEntryPointEvaluationDebugInfo::PrintEarlyRejectionEvents()
{
	PushIndent(2);
		PrintLn("--------------------------------------------------------------------");
		PrintLn("Early Rejected Entry Point Events");
		PushIndent();
			for (s32 i=0; i<m_RejectionEvents.GetCount(); ++i)
			{
				m_RejectionEvents[i].Print();
			}
		PopIndent();
	PopIndent(2);
}

void CRidableEntryPointEvaluationDebugInfo::PrintScoringEvents()
{
	PushIndent(2);
		PrintLn("--------------------------------------------------------------------");
		PrintLn("Scored Entry Point Events");
		PushIndent();
			for (s32 i=0; i<m_ScoringEvents.GetCount(); ++i)
			{
				m_ScoringEvents[i].Print();
			}
		PopIndent();
	PopIndent(2);
}

void CRidableEntryPointEvaluationDebugInfo::PrintPriorityScoringEvents()
{
	PushIndent(2);
		PrintLn("--------------------------------------------------------------------");
		PrintLn("Priority Scored Entry Point Events");
		PushIndent();
		for (s32 i=0; i<m_PriorityScoringEvents.GetCount(); ++i)
		{
			m_PriorityScoringEvents[i].Print();
		}
		PopIndent();
	PopIndent(2);
}

CVehicleDebugInfo::CVehicleDebugInfo(const CVehicle* pVeh, DebugInfoPrintParams printParams, s32 iNumberOfLines)
: CAIDebugInfo(printParams),
m_Vehicle(pVeh)
{
	m_NumberOfLines = iNumberOfLines;
}



CVehicleStatusDebugInfo::CVehicleStatusDebugInfo(const CVehicle* pVeh, DebugInfoPrintParams rPrintParams, s32 iNumberOfLines)
: CVehicleDebugInfo(pVeh, rPrintParams, iNumberOfLines)
{

}

const char* GetStatusString(s32 popType)
{
	switch (popType)
	{
	case STATUS_PLAYER: return "PLAYER";
	case STATUS_PHYSICS: return "PHYSICS";
	case STATUS_ABANDONED: return "ABANDONED";
	case STATUS_WRECKED: return "WRECKED";
	case STATUS_PLAYER_DISABLED: return "PLAYER_DISABLED";
	case STATUS_OUT_OF_CONTROL: return "OUT_OF_CONTROL";
	default: break;
	}
	return "UNKNOWN";
}

const char* planeSectionNames[] = 
{
	"WING_L",
	"WING_R",
	"TAIL",
	"ENGINE_L",
	"ENGINE_R",
	"ELEVATOR_L",
	"ELEVATOR_R",
	"AILERON_L",
	"AILERON_R",
	"RUDDER",
	"RUDDER_2",
	"AIRBRAKE_L",
	"AIRBRAKE_R",
	"LANDING_GEAR"
};
void CVehicleStatusDebugInfo::PrintRenderedSelection(bool bPrintAll)
{
	if (!m_Vehicle || !m_Vehicle->GetIntelligence())
		return;

	if(!CVehicleIntelligence::m_bDisplayStatusInfo && !bPrintAll)
		return;

	WriteLogEvent("VEHICLE INFO");
	PushIndent();
	PushIndent();
	PushIndent();

	char popString[32];
	switch (m_Vehicle->PopTypeGet())
	{
		case POPTYPE_UNKNOWN: sprintf(popString, "UNKNOWN"); break;
		case POPTYPE_RANDOM_PERMANENT: sprintf(popString, "RANDOM_PERMANENT"); break;
		case POPTYPE_RANDOM_PARKED: sprintf(popString, "RANDOM_PARKED"); break;
		case POPTYPE_RANDOM_PATROL: sprintf(popString, "RANDOM_PATROL"); break;
		case POPTYPE_RANDOM_SCENARIO: sprintf(popString, "RANDOM_SCENARIO"); break;
		case POPTYPE_RANDOM_AMBIENT: sprintf(popString, "RANDOM_AMBIENT"); break;
		case POPTYPE_PERMANENT: sprintf(popString, "PERMANENT"); break;
		case POPTYPE_MISSION: sprintf(popString, "MISSION"); break;
		case POPTYPE_REPLAY: sprintf(popString, "REPLAY"); break;
		case POPTYPE_CACHE: sprintf(popString, "CACHE"); break;
		case POPTYPE_TOOL: sprintf(popString, "TOOL"); break;
		default: sprintf(popString, "UNKNOWN"); break;
	}

	const Vector3 vVelocity = m_Vehicle->GetVelocity(); 
	PrintLn("%s Vehicle: %s | %s | %s | %s | 0x%p", AILogging::GetDynamicEntityIsCloneStringSafe(m_Vehicle), popString, 
											AILogging::GetDynamicEntityNameSafe(m_Vehicle), m_Vehicle->GetModelName(), AILogging::GetDynamicEntityOwnerGamerNameSafe(m_Vehicle), m_Vehicle.Get());
	const CPed* pPed = m_Vehicle->GetDriver();
	char driverInfo[16];
	if(!pPed) sprintf(driverInfo, "");
	else if(pPed->IsPlayer()) sprintf(driverInfo, "Local Player");
	else if(pPed->IsNetworkPlayer()) sprintf(driverInfo, "Network Player");
	else if(pPed->IsNetworkClone()) sprintf(driverInfo, "Network Clone");
	else sprintf(driverInfo, "Local AI");

	PrintLn("Driver %s %s. Passengers: %d", AILogging::GetDynamicEntityNameSafe(pPed), driverInfo, m_Vehicle->GetNumberOfPassenger());	
	PrintLn("Position: <%.2f,%.2f,%.2f> Velocity <%.2f,%.2f,%.2f> (%.2f)", VEC3V_ARGS(m_Vehicle->GetTransform().GetPosition()), vVelocity.x, vVelocity.y, vVelocity.z, vVelocity.Mag());
	PrintLn("Status: %s", GetStatusString(m_Vehicle->GetStatus()));
	PrintLn("Health: %.3f", m_Vehicle->GetHealth());
	if(m_Vehicle->InheritsFromPlane())
	{
		PushIndent();
		const CPlane* pPlane = static_cast<const CPlane*>(m_Vehicle.Get());
		for(int i =0; i < CAircraftDamage::NUM_DAMAGE_SECTIONS; i++)
		{
			PrintLn("%s: %.1f/%.1f", planeSectionNames[i], pPlane->GetAircraftDamage().GetSectionHealth((CAircraftDamage::eAircraftSection)i), 
				pPlane->GetAircraftDamage().GetSectionMaxHealth((CAircraftDamage::eAircraftSection)i));
		}
		PopIndent();
	}
	else if(m_Vehicle->InheritsFromHeli())
	{
		PushIndent();
		const CHeli* pHeli = static_cast<const CHeli*>(m_Vehicle.Get());
		PrintLn("Main Rotor: %.1f", pHeli->GetMainRotorHealth());
		PrintLn("Rear Rotor: %.1f", pHeli->GetRearRotorHealth());
		PrintLn("Tail Boom: %.1f", pHeli->GetTailBoomHealth());
		PopIndent();
	}

	//TODO put this in own class and expand a lot
	if(m_Vehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy))
	{
		PrintLn("LOD: SuperDummy. Collider: %s", m_Vehicle->GetCollider() ? "True" : "False");
	}
	else if(m_Vehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodDummy))
	{
		PrintLn("LOD: Dummy. Collider: %s", m_Vehicle->GetCollider() ? "True" : "False");
	}
	else
	{
		PrintLn("LOD: Real. Collider: %s", m_Vehicle->GetCollider() ? "True" : "False");
	}

	PopIndent();
	PopIndent();
	PopIndent();
}


static const char * pVehicleDebuggingText[] =
{
	"Off",
	"Tasks",
	"Status",
	"Task History",
	"Flags",
	"Destinations",
	"Future Nodes",
	"Follow Route",
	"Control Info",
	"Lowlevel Control",
	"Transmission",
	"Stuck"
};

//Singleton class used to visualise lots of vehicle AI information to the screen
CVehicleDebugVisualiser*		CVehicleDebugVisualiser::sm_pInstance = NULL;
CVehicleDebugVisualiser::eVehicleDebugVisMode			CVehicleDebugVisualiser::eVehicleDisplayDebugInfo = CVehicleDebugVisualiser::eOff;
bool							CVehicleDebugVisualiser::bFlagState = false;
CVehicleDebugVisualiser::CVehicleDebugVisualiser()
{
	debugInfos.PushAndGrow(rage_new CVehicleStatusDebugInfo());
	debugInfos.PushAndGrow(rage_new CVehicleTaskDebugInfo());
	debugInfos.PushAndGrow(rage_new CVehicleNodeListDebugInfo());
	debugInfos.PushAndGrow(rage_new CVehicleStuckDebugInfo());
	debugInfos.PushAndGrow(rage_new CVehicleControlDebugInfo());
	debugInfos.PushAndGrow(rage_new CVehicleFlagDebugInfo());

	for (s32 i=0; i<debugInfos.GetCount(); ++i)
	{		
		debugInfos[i]->SetLogType(AILogging::Screen);
	}
}

CVehicleDebugVisualiser::~CVehicleDebugVisualiser()
{
	int iCount = debugInfos.GetCount();
	for (s32 i = iCount - 1; i >= 0; --i)
	{
		delete debugInfos[i];
	}
	debugInfos.clear();
}

void CVehicleDebugVisualiser::Visualise(const CVehicle* pVeh)
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if (!CPedDebugVisualiserMenu::IsFocusPedDisplayIn2D() || (pFocusEntity == pVeh))
	{
		int iLines = 0;
		Vector3 vDrawPosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
		for (s32 i = 0; i < debugInfos.GetCount(); ++i)
		{
			debugInfos[i]->LoadNextVehicle(pVeh, vDrawPosition, iLines);
			debugInfos[i]->Visualise();
			iLines = debugInfos[i]->GetNumberOfLines();
		}
	}
}

bool& CVehicleDebugVisualiser::GetDebugFlagFromEnum(eVehicleDebugVisMode mode)
{
	switch(mode)
	{
	case eTasks: return CVehicleIntelligence::m_bDisplayCarAiTaskInfo; break;
	case eStatus: return CVehicleIntelligence::m_bDisplayStatusInfo; break;
	case eTaskHistory: return CVehicleIntelligence::m_bDisplayCarAiTaskHistory; break;
	case eVehicleFlags: return CVehicleIntelligence::m_bDisplayVehicleFlagsInfo; break;
	case eDestinations: return CVehicleIntelligence::m_bDisplayCarFinalDestinations; break;
	case eFutureNodes: return CVehicleIntelligence::m_bDisplayDebugFutureNodes; break;
	case eFollowRoute: return CVehicleIntelligence::m_bDisplayDebugFutureFollowRoute; break;
	case eControlInfo: return CVehicleIntelligence::m_bDisplayControlDebugInfo; break;
	case eLowlevelControl: return CVehicleIntelligence::m_bDisplayAILowLevelControlInfo; break;
	case eTransmission: return CVehicleIntelligence::m_bDisplayControlTransmissionInfo; break;
	case eStuck: return CVehicleIntelligence::m_bDisplayStuckDebugInfo; break;
	default: return bFlagState; break;
	}
}

void CVehicleDebugVisualiser::Process()
{
	int iPreviousMode = eVehicleDisplayDebugInfo;
	int iMode = (int)eVehicleDisplayDebugInfo;
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_A, KEYBOARD_MODE_DEBUG_ALT, "AI display increment"))
	{
		iMode++;
		if(iMode==(int)eNumModes)
			iMode=(int)eOff;
	}
	else if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_A, KEYBOARD_MODE_DEBUG_SHIFT_ALT, "AI display decrement"))
	{
		iMode--;
		if(iMode<(int)eOff)
			iMode=(int)eNumModes-1;
	}
	else if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_A, KEYBOARD_MODE_DEBUG_CNTRL_ALT, "Toggle AI display off"))
	{
		iMode = 0;
	}

	if(iMode != iPreviousMode)
	{
		eVehicleDisplayDebugInfo = (eVehicleDebugVisMode)iMode;
		//reset previous flag if it was off originally
		if(iPreviousMode != eOff && !bFlagState)
		{
			bool& flag = GetDebugFlagFromEnum((eVehicleDebugVisMode)iPreviousMode);
			flag = false;
		}

		//update current flag
		if(iMode != eOff)
		{			
			bool& newFlag = GetDebugFlagFromEnum((eVehicleDebugVisMode)iMode);
			bFlagState = newFlag;
			newFlag = true;
		}

		Color32 txtCol(255,255,0,255);
		static char text[256];
		sprintf(text, "Vehicle Display : %s", pVehicleDebuggingText[(int)eVehicleDisplayDebugInfo]);
		CMessages::AddMessage(text, -1, 1000, true, false, PREVIOUS_BRIEF_NO_OVERRIDE, NULL, 0, NULL, 0, false);
	}
}

#endif // AI_DEBUG_OUTPUT_ENABLED
