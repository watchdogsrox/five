// Title	:	Crime.cpp
// Author	:	
// Started	:	17/04/00

// Game headers
#include "Crime.h"
#include "audio/policescanner.h"
#include "audioengine/widgets.h"
#include "game/CrimeInformation.h"
#include "game/Dispatch/DispatchData.h"
#include "Game/Dispatch/Incidents.h"
#include "game/localisation.h"
#include "peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsMgr.h"
#include "game/wanted.h"
#include "scene/world/gameWorld.h"
#include "script/script.h"
#include "script/script_hud.h"
#include "task/Service/Swat/TaskSwat.h"
#include "text/messages.h"
#include "vehicles/heli.h"
#include "modelinfo/PedModelInfo.h"

AI_OPTIMISATIONS()

const u32 g_CrimeNamePHashes[MAX_CRIMES] = 
{
	ATPARTIALSTRINGHASH("CRIME_NONE", 0xC3346671),
	ATPARTIALSTRINGHASH("CRIME_POSSESSION_GUN", 0xE6362A4E),
	ATPARTIALSTRINGHASH("CRIME_RUN_REDLIGHT", 0x9055005F),
	ATPARTIALSTRINGHASH("CRIME_RECKLESS_DRIVING", 0xC532F032),
	ATPARTIALSTRINGHASH("CRIME_SPEEDING", 0x8152C06C),
	ATPARTIALSTRINGHASH("CRIME_DRIVE_AGAINST_TRAFFIC", 0x2552F9EB),
	ATPARTIALSTRINGHASH("CRIME_RIDING_BIKE_WITHOUT_HELMET", 0xB83B657),
	ATPARTIALSTRINGHASH("CRIME_STEAL_VEHICLE", 0x693084E),
	ATPARTIALSTRINGHASH("CRIME_STEAL_CAR", 0x5DA9475F),
	ATPARTIALSTRINGHASH("CRIME_BLOCK_POLICE_CAR", 0xA88186B2),
	ATPARTIALSTRINGHASH("CRIME_STAND_ON_POLICE_CAR", 0x596A4135),
	ATPARTIALSTRINGHASH("CRIME_HIT_PED", 0x3F3B898A),
	ATPARTIALSTRINGHASH("CRIME_HIT_COP", 0x9BFE4239),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_PED", 0xEFAC9542),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_COP", 0xF326D5BD),
	ATPARTIALSTRINGHASH("CRIME_RUNOVER_PED", 0xB7169919),
	ATPARTIALSTRINGHASH("CRIME_RUNOVER_COP", 0xE1D3D9FC),
	ATPARTIALSTRINGHASH("CRIME_DESTROY_HELI", 0x863D39F6),
	ATPARTIALSTRINGHASH("CRIME_PED_SET_ON_FIRE", 0x14C484E3),
	ATPARTIALSTRINGHASH("CRIME_COP_SET_ON_FIRE", 0xAFB906A9),
	ATPARTIALSTRINGHASH("CRIME_CAR_SET_ON_FIRE", 0xBEEF7C),
	ATPARTIALSTRINGHASH("CRIME_DESTROY_PLANE", 0x4262CFD5),
	ATPARTIALSTRINGHASH("CRIME_CAUSE_EXPLOSION", 0x74149BD),
	ATPARTIALSTRINGHASH("CRIME_STAB_PED", 0x33867C7F),
	ATPARTIALSTRINGHASH("CRIME_STAB_COP", 0x14C52012),
	ATPARTIALSTRINGHASH("CRIME_DESTROY_VEHICLE", 0x9443FBEF),
	ATPARTIALSTRINGHASH("CRIME_DAMAGE_TO_PROPERTY", 0xA1AEDA12),
	ATPARTIALSTRINGHASH("CRIME_TARGET_COP", 0x4F6A0C11),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_GUN_FIRED", 0x23F16FE0),
	ATPARTIALSTRINGHASH("CRIME_RESISTING_ARREST", 0x7C26A6E4),
	ATPARTIALSTRINGHASH("CRIME_MOLOTOV", 0xFA884198),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_PED_NONLETHAL", 0x444622ED),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_COP_NONLETHAL", 0x917BDD66),
	ATPARTIALSTRINGHASH("CRIME_KILL_COP", 0xAE2186E8),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_AT_COP", 0x174D8BC3),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_VEHICLE", 0x821135C4),
	ATPARTIALSTRINGHASH("CRIME_TERRORIST_ACTIVITY", 0x9D4927D),
	ATPARTIALSTRINGHASH("CRIME_HASSLE", 0xF411ACF4),
	ATPARTIALSTRINGHASH("CRIME_THROW_GRENADE", 0xE7FC5FA0),
	ATPARTIALSTRINGHASH("CRIME_VEHICLE_EXPLOSION", 0x1F64DC77),
	ATPARTIALSTRINGHASH("CRIME_KILL_PED", 0xF434FE7D),
	ATPARTIALSTRINGHASH("CRIME_STEALTH_KILL_COP",0x9BDBFAC7),
	ATPARTIALSTRINGHASH("CRIME_SUICIDE",0x198C7DA4),
	ATPARTIALSTRINGHASH("CRIME_DISTURBANCE",0x908F6D41),
	ATPARTIALSTRINGHASH("CRIME_CIVILIAN_NEEDS_ASSISTANCE",0x6E91CE0E),
	ATPARTIALSTRINGHASH("CRIME_STEALTH_KILL_PED",0x79EF6887),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_PED_SUPPRESSED",0x5AD295A9),
	ATPARTIALSTRINGHASH("CRIME_JACK_DEAD_PED",0x4D6D003D),
	ATPARTIALSTRINGHASH("CRIME_CHAIN_EXPLOSION",0x1CBC854F)
};

const u32 g_CrimeNameHashes[MAX_CRIMES] = 
{
	atHashString("CRIME_NONE",0xDE51030A),
	atHashString("CRIME_POSSESSION_GUN",0xD80E0051),
	atHashString("CRIME_RUN_REDLIGHT",0xC17ADCF7),
	atHashString("CRIME_RECKLESS_DRIVING",0xC31DA88C),
	atHashString("CRIME_SPEEDING",0x6B63BED4),
	atHashString("CRIME_DRIVE_AGAINST_TRAFFIC",0xE9F0341A),
	atHashString("CRIME_RIDING_BIKE_WITHOUT_HELMET",0x363E9D22),
	atHashString("CRIME_STEAL_VEHICLE",0x5317AFD7),
	atHashString("CRIME_STEAL_CAR",0xB90E5C27),
	atHashString("CRIME_BLOCK_POLICE_CAR",0x38AADF5),
	atHashString("CRIME_STAND_ON_POLICE_CAR",0x9362DD54),
	atHashString("CRIME_HIT_PED",0xB4A0F720),
	atHashString("CRIME_HIT_COP",0x1104AA0B),
	atHashString("CRIME_SHOOT_PED",0xBBD71D75),
	atHashString("CRIME_SHOOT_COP",0x90568815),
	atHashString("CRIME_RUNOVER_PED",0xBC0D188D),
	atHashString("CRIME_RUNOVER_COP",0x44312789),
	atHashString("CRIME_DESTROY_HELI",0xBED38D47),
	atHashString("CRIME_PED_SET_ON_FIRE",0xB676F6EE),
	atHashString("CRIME_COP_SET_ON_FIRE",0x736F8BD6),
	atHashString("CRIME_CAR_SET_ON_FIRE",0x657F3D91),
	atHashString("CRIME_DESTROY_PLANE",0x461DE154),
	atHashString("CRIME_CAUSE_EXPLOSION",0x20AF3ED7),
	atHashString("CRIME_STAB_PED",0x9B41173B),
	atHashString("CRIME_STAB_COP",0x79AC7D66),
	atHashString("CRIME_DESTROY_VEHICLE",0xC1F3171C),
	atHashString("CRIME_DAMAGE_TO_PROPERTY",0xD63BCE17),
	atHashString("CRIME_TARGET_COP",0x688D3BD4),
	atHashString("CRIME_FIREARM_DISCHARGE",0xE7A82959),
	atHashString("CRIME_RESIST_ARREST",0xDF577BA5),
	atHashString("CRIME_MOLOTOV",0x9A5C1711),
	atHashString("CRIME_SHOOT_NONLETHAL_PED",0x488EC657),
	atHashString("CRIME_SHOOT_NONLETHAL_COP",0x1306EFDB),
	atHashString("CRIME_KILL_COP",0x4CFDDB9F),
	atHashString("CRIME_SHOOT_AT_COP",0xC116DEE6),
	atHashString("CRIME_SHOOT_VEHICLE",0xEAE4B0B8),
	atHashString("CRIME_TERRORIST_ACTIVITY",0x6D12A941),
	atHashString("CRIME_HASSLE",0x58488776),
	atHashString("CRIME_THROW_GRENADE",0xFB81274B),
	atHashString("CRIME_VEHICLE_EXPLOSION",0x63341157),
	atHashString("CRIME_KILL_PED",0xBACBC9FB),
	atHashString("CRIME_STEALTH_KILL_COP",0xBE770785),
	atHashString("CRIME_SUICIDE",0x505154C9),
	atHashString("CRIME_DISTURBANCE",0x5011F613),
	atHashString("CRIME_CIVILIAN_NEEDS_ASSISTANCE",0x1078DA79),
	atHashString("CRIME_STEALTH_KILL_PED",0xA5881EA),
	atHashString("CRIME_SHOOT_PED_SUPPRESSED",0xE86DEE19),
	atHashString("CRIME_JACK_DEAD_PED",0xC5049885),
	atHashString("CRIME_CHAIN_EXPLOSION",0x8089FBD2)
};

CCrime::eReportCrimeMethod CCrime::sm_eReportCrimeMethod = REPORT_CRIME_DEFAULT;

// Purpose:	To work out the vehicle type and report the proper crime
void CCrime::ReportDestroyVehicle(CVehicle* pVehicle, CPed* pCommitedby)
{
	if(pVehicle)
	{
		// Don't report the crime if it's our vehicle and we are in the vehicle or nobody is in the vehicle
		if( pCommitedby && pCommitedby->GetMyVehicle() == pVehicle &&
			(pCommitedby->GetIsInVehicle() || !pVehicle->HasAlivePedsInIt()) )
		{
			return;
		}

		if(pVehicle->GetVehicleType( )== VEHICLE_TYPE_HELI)
		{
			// If this heli was a police dispatch (non-rappelling and not dropping off swat) then we should register it being destroyed
			if(pCommitedby && static_cast<CHeli*>(pVehicle)->GetIsPoliceDispatched())
			{
				CWanted* pWanted = pCommitedby->GetPlayerWanted();
				if( pWanted && pWanted->GetWantedLevel() > WANTED_CLEAN &&
					pWanted->m_wantedLevelIncident && pWanted->m_wantedLevelIncident->GetType() == CIncident::IT_Wanted )
				{
					static_cast<CWantedIncident*>(pWanted->m_wantedLevelIncident.Get())->SetTimeLastPoliceHeliDestroyed(fwTimer::GetTimeInMilliseconds());
				}
			}

			CCrime::ReportCrime(CRIME_DESTROY_HELI, pVehicle, pCommitedby);
		}
		else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE)
		{
			CCrime::ReportCrime(CRIME_DESTROY_PLANE, pVehicle, pCommitedby);
		}
		else
		{
			CCrime::ReportCrime(CRIME_DESTROY_VEHICLE, pVehicle, pCommitedby);
		}
	}
}

// Name			:	ReportCrime
// Purpose		:	When an event gets triggered we also report the crime.
// Parameters	:	EventType - type of event to check for
//					IDKey Key being used by the wanted stuff to identify the event
// Returns		:
void CCrime::ReportCrime(eCrimeType CrimeType, CEntity* pVictim, CPed* pCommitedby, u32 uTime)
{
	CompileTimeAssert(sizeof(g_CrimeNamePHashes)/sizeof(g_CrimeNamePHashes[0]) == MAX_CRIMES);
	CompileTimeAssert(sizeof(g_CrimeNameHashes)/sizeof(g_CrimeNameHashes[0]) == MAX_CRIMES);

	// If a pigeon pickup has been shot recently the gun shot should not trigger a wanted level.
//	if ((CrimeType == CRIME_POSSESSION_GUN || CrimeType == CRIME_FIREARM_DISCHARGE) && fwTimer::GetTimeInMilliseconds() - CPickups::LastTimePigeonShot < 5000)
//	{
//		return;
//	}

    CPed *pPlayerPed = pCommitedby && pCommitedby->IsPlayer() ? pCommitedby : NULL;

	if ( (!pPlayerPed) || (pPlayerPed->IsControlledByLocalAi())) return;

	switch (sm_eReportCrimeMethod)
	{
	case REPORT_CRIME_DEFAULT:
		// default crime reporting is handled below
		break;
	case REPORT_CRIME_ARCADE_CNC:

		if(CrimeType == CRIME_NONE) return;

		// for CnC, just register the crime then bail		
		const Vec3V vRawPlayerPosition = pPlayerPed->GetTransform().GetPosition();
		const Vector3 PlayerPosition = VEC3V_TO_VECTOR3(vRawPlayerPosition);

		CWanted& playerWanted = pPlayerPed->GetPlayerInfo()->GetWanted();
		// do not care about victim for CnC so Victim Id and pVictim are null
		playerWanted.RegisterCrime(CrimeType, PlayerPosition, nullptr, true, nullptr);

		// NFA for CnC
		return;
	}
	
	if (pPlayerPed && (CRIME_PED_SET_ON_FIRE == CrimeType || CRIME_COP_SET_ON_FIRE == CrimeType || CRIME_CAR_SET_ON_FIRE == CrimeType || CRIME_MOLOTOV == CrimeType))
	{
		//@@: location CCRIME_REPORTCRIME_FIRES_STARTED
		StatId stat = StatsInterface::GetStatsModelHashId("FIRES_STARTED");
		StatsInterface::IncrementStat(stat, 1.0f);

		// Don't report setting ourselves on fire as a crime
		if(pPlayerPed == pVictim)
		{
			return;
		}
	}

	if (NetworkInterface::IsGameInProgress())
	{
		if (!NetworkInterface::AllowCrimeOrThreatResponsePlayerJustResurrected(pCommitedby,pVictim))
		{
			wantedDebugf3("CCrime::ReportCrime -- !NetworkInterface::AllowCrimeOrThreatResponsePlayerJustResurrected --> return");
			return;
		}
	}

	// Early our if this crime is being reported due to something happening before our WL was cleared
	CWanted& playerWanted = pPlayerPed->GetPlayerInfo()->GetWanted();
	if(playerWanted.GetWantedLevel() == WANTED_CLEAN && uTime < playerWanted.GetTimeWantedLevelCleared())
	{
		return;
	}

	bool bCrimeAgainstEmergencyPed = false;
	if (pVictim)
	{
		// Some peds can be set to not influence the wanted level (can attack/kill/burn/crush them with impunity)
		if(pVictim->GetIsTypePed())
		{
			CPed *pVictimPed = static_cast<CPed*>(pVictim);
			if (pVictimPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontInfluenceWantedLevel))
				return;

			CRelationshipGroup* pRelGroup = pVictimPed->GetPedIntelligence()->GetRelationshipGroup();
			if (pRelGroup && !pRelGroup->GetAffectsWantedLevel())
			{
				return;
			}

			// Is this a crime directly against an emergency type ped
			if( CPedType::IsEmergencyType(pVictimPed->GetPedType()) &&
				(CrimeType == CRIME_HIT_PED || CrimeType == CRIME_SHOOT_PED || CrimeType == CRIME_RUNOVER_PED || CrimeType == CRIME_STAB_PED) )
			{
				bCrimeAgainstEmergencyPed = true;
			}
		}
		// vehicles can also be set to not influence the wanted level
		else if(pVictim->GetIsTypeVehicle())
		{
			CVehicle *pVictimVehicle = static_cast<CVehicle*>(pVictim);
			if(!pVictimVehicle->m_nVehicleFlags.bInfluenceWantedLevel)
			{
				return;
			}
		}
	}

	// Check if this is some sort of vehicle destruction and make sure we don't give the crime if a similar crime is already queued or reported recently
	if(ShouldIgnoreVehicleCrime(CrimeType, pVictim, pPlayerPed))
	{
		return;
	}

	// Instantly register the crime
	if(CrimeType!=CRIME_NONE && pPlayerPed)
	{
		//@@: location CCRIME_REPORTCRIME_REGISTER_CRIME
		// If there is police presence the crime gets dealt with immediately
		const Vec3V vCriminalPosition = pPlayerPed->GetTransform().GetPosition();
		const Vector3 PlayerPosition = VEC3V_TO_VECTOR3(vCriminalPosition);
		float fRangeForCrimeType = CCrimeInformationManager::GetInstance().GetImmediateDetectionRange(CrimeType);
		s32 iPreviousWantedLevel = playerWanted.m_WantedLevel;
		bool bCanReportCrime = playerWanted.m_fMultiplier > 0.0f && !playerWanted.IsCrimeSuppressedThisFrame(CrimeType);
		bool bPoliceInArea = false;

		if (bCanReportCrime && playerWanted.MaximumWantedLevel > WANTED_CLEAN)
		{
			bPoliceInArea = CWanted::WorkOutPolicePresence(vCriminalPosition, fRangeForCrimeType) > 0;
			if (bPoliceInArea || bCrimeAgainstEmergencyPed ||
				((CrimeType == CRIME_SHOOT_PED || CrimeType == CRIME_SHOOT_COP || CrimeType == CRIME_SHOOT_NONLETHAL_COP || CrimeType == CRIME_SHOOT_NONLETHAL_PED
					|| CrimeType == CRIME_PED_SET_ON_FIRE || CrimeType == CRIME_COP_SET_ON_FIRE || CrimeType == CRIME_SHOOT_PED_SUPPRESSED) && CLocalisation::GermanGame()))
			{
				if(bPoliceInArea && playerWanted.GetWantedLevel() == WANTED_LEVEL1)
				{
					playerWanted.SetResistingArrest(CrimeType);
				}

				playerWanted.RegisterCrime_Immediately(CrimeType, PlayerPosition, pVictim, false, pVictim);

				if (CrimeType <= CRIME_LAST_MINOR_CRIME)		// Some crimes are minor and will only trigger a parole state which can go away if the player doesn't commit any crimes.
				{
					if (CScriptHud::iFakeWantedLevel == 0)		// Don't do this if the player has a fake wanted level as it's less important than that.
					{
						playerWanted.SetOneStarParoleNoDrop();
					}
				}
				else if (!playerWanted.GetWithinAmnestyTime() || (CrimeType != CRIME_DAMAGE_TO_PROPERTY && CrimeType != CRIME_FIREARM_DISCHARGE))
				{
					// If there is police around the player ALWAYS gets a wanted level
					s32 delay = WANTED_CHANGE_DELAY_QUICK;
					s32 wantedToGoUpTo = WANTED_LEVEL1;

					if (CrimeType==CRIME_HIT_COP || CrimeType==CRIME_SHOOT_COP || CrimeType==CRIME_SHOOT_NONLETHAL_COP || CrimeType==CRIME_RUNOVER_COP ||
						CrimeType==CRIME_STAB_COP || CrimeType==CRIME_TARGET_COP || CrimeType==CRIME_SHOOT_AT_COP)
					{
						delay = WANTED_CHANGE_DELAY_INSTANT;
					}

					if (CrimeType==CRIME_HIT_COP || CrimeType==CRIME_SHOOT_PED || CrimeType==CRIME_SHOOT_COP || CrimeType==CRIME_SHOOT_NONLETHAL_COP ||
						CrimeType==CRIME_SHOOT_NONLETHAL_PED || CrimeType==CRIME_TARGET_COP || CrimeType==CRIME_SHOOT_PED_SUPPRESSED)
					{
						wantedToGoUpTo = WANTED_LEVEL2;
					}

					playerWanted.SetWantedLevelNoDrop(PlayerPosition, wantedToGoUpTo, delay, WL_FROM_REPORTED_CRIME);
				}
			}
			else
			{
				playerWanted.RegisterCrime(CrimeType, PlayerPosition, pVictim, false, pVictim);
			}
		}

		// If the player is on parols any crime will reinstate the old wanted level.
		if (playerWanted.m_WantedLevelBeforeParole != WANTED_CLEAN)
		{
			if (CrimeType > CRIME_LAST_MINOR_CRIME)
			{
				playerWanted.SetWantedLevelNoDrop(PlayerPosition, playerWanted.m_WantedLevelBeforeParole, 0, WL_FROM_BROKE_PAROLE );
				playerWanted.ClearParole();
			}
		}

		if(bCanReportCrime)
		{
			bool bOnlyReportCrime = NetworkInterface::IsGameInProgress() && pVictim && pVictim->GetIsTypePed() && static_cast<CPed*>(pVictim)->IsDead();
			if(!bOnlyReportCrime)
			{
				// If the player is shooting at a cop the wanted level goes up to at least 3 immediately
				if ( CrimeType == CRIME_SHOOT_COP || CrimeType == CRIME_STAB_COP || CrimeType == CRIME_SHOOT_NONLETHAL_COP || CrimeType == CRIME_COP_SET_ON_FIRE )
				{
					//@@: location CCRIME_REPORTCRIME_SHOOT_COP
					playerWanted.SetWantedLevelNoDrop(PlayerPosition, WANTED_LEVEL3, bPoliceInArea ? WANTED_CHANGE_DELAY_INSTANT : WANTED_CHANGE_DELAY_QUICK, WL_FROM_REPORTED_CRIME);
				}
				else if (CrimeType == CRIME_HIT_COP)
				{
					// If the player hits the police wanted goes up to at least level one
					playerWanted.SetWantedLevelNoDrop(PlayerPosition, WANTED_LEVEL2, bPoliceInArea ? WANTED_CHANGE_DELAY_INSTANT : WANTED_CHANGE_DELAY_QUICK, WL_FROM_REPORTED_CRIME);
				}	
			}
			
			if(CrimeType == CRIME_KILL_COP)
			{
				playerWanted.SetWantedLevelNoDrop(PlayerPosition, WANTED_LEVEL3, bPoliceInArea ? WANTED_CHANGE_DELAY_INSTANT : WANTED_CHANGE_DELAY_QUICK, WL_FROM_REPORTED_CRIME);
			}

			if ((CrimeType == CRIME_STEAL_CAR || CrimeType == CRIME_STEAL_VEHICLE) && pVictim && pVictim->GetIsTypeVehicle() )
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pVictim);
				if( pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedType() == PEDTYPE_COP )
					playerWanted.SetWantedLevelNoDrop(PlayerPosition, WANTED_LEVEL1, WANTED_CHANGE_DELAY_INSTANT, WL_FROM_REPORTED_CRIME);
	
			}

			if (CrimeType == CRIME_SHOOT_PED && pVictim->GetIsTypePed() && static_cast<CPed*>(pVictim)->GetPedResetFlag(CPED_RESET_FLAG_ForceWantedLevelWhenKilled))
			{
				playerWanted.SetWantedLevelNoDrop(PlayerPosition, WANTED_LEVEL1, WANTED_CHANGE_DELAY_INSTANT, WL_FROM_REPORTED_CRIME);
			}
		}

		// Report crime to police radio (only on gaining a new star).
		eWantedLevel oldWantedLevel = playerWanted.GetWantedLevel();
		if(pPlayerPed->IsLocalPlayer() && playerWanted.m_WantedLevel > oldWantedLevel)
		{
			NA_POLICESCANNER_ENABLED_ONLY(g_PoliceScanner.ReportPlayerCrime(PlayerPosition, CrimeType));
		}

		// If we didn't have a wanted level and have one or are going to have one then make sure any nearby cops respond
		if(iPreviousWantedLevel == WANTED_CLEAN && playerWanted.m_nNewWantedLevel >= CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL1))
		{
			CWanted::AlertNearbyPolice(vCriminalPosition, fRangeForCrimeType, pPlayerPed, playerWanted.m_nNewWantedLevel < CDispatchData::GetWantedLevelThreshold(WANTED_LEVEL2));
		}
	}
}

bool CCrime::ShouldIgnoreVehicleCrime(eCrimeType CrimeType, CEntity* pVictim, CPed* pCommitedby)
{
	// Must have a victim
	if(!pVictim)
	{
		return false;
	}

	// Make sure this is a crime type that we care about
	if(CrimeType != CRIME_VEHICLE_EXPLOSION && CrimeType != CRIME_DESTROY_VEHICLE && CrimeType != CRIME_CAUSE_EXPLOSION && CrimeType != CRIME_CAR_SET_ON_FIRE && CrimeType != CRIME_CHAIN_EXPLOSION)
	{
		return false;
	}

	// Verify who committed the crime
	if(!pCommitedby)
	{
		return false;
	}

	// Verify the wanted of who committed the crime
	CWanted* pPlayerWanted = pCommitedby->GetPlayerWanted();
	if(!pPlayerWanted)
	{
		return false;
	}

	for(int i = 0; i < pPlayerWanted->m_MaxCrimesBeingQd; i++)
	{
		if(pPlayerWanted->CrimesBeingQd[i].m_CrimeVictimId == pVictim)
		{
			// If the queued crime is any of the three major crimes then do not report the new crime
			if( pPlayerWanted->CrimesBeingQd[i].CrimeType == CRIME_DESTROY_VEHICLE)
			{
				if(CrimeType == CRIME_CHAIN_EXPLOSION || CrimeType == CRIME_CAR_SET_ON_FIRE)
				{
					return true;
				}
			}
			if( pPlayerWanted->CrimesBeingQd[i].CrimeType == CRIME_VEHICLE_EXPLOSION || pPlayerWanted->CrimesBeingQd[i].CrimeType == CRIME_CHAIN_EXPLOSION )
			{
				return true;
			}
			else if( pPlayerWanted->CrimesBeingQd[i].CrimeType == CRIME_CAUSE_EXPLOSION &&
				(CrimeType == CRIME_CAUSE_EXPLOSION || CrimeType == CRIME_CAR_SET_ON_FIRE || CrimeType == CRIME_CHAIN_EXPLOSION) )
			{
				return true;
			}
		}
	}

	for(int i = 0; i < pPlayerWanted->m_ReportedCrimes.GetCount(); i++)
	{
		if( pPlayerWanted->m_ReportedCrimes[i].m_pCrimeVictim == pVictim &&
			pPlayerWanted->m_ReportedCrimes[i].m_uTimeReported > fwTimer::GetTimeInMilliseconds() - CRIMES_GET_REPORTED_TIME)
		{
			// If the queued crime is any of the three major crimes then do not report the new crime
			if( pPlayerWanted->m_ReportedCrimes[i].m_eCrimeType == CRIME_VEHICLE_EXPLOSION ||
				pPlayerWanted->m_ReportedCrimes[i].m_eCrimeType == CRIME_DESTROY_VEHICLE ||
				pPlayerWanted->m_ReportedCrimes[i].m_eCrimeType == CRIME_CHAIN_EXPLOSION )
			{
				return true;
			}
			else if( pPlayerWanted->m_ReportedCrimes[i].m_eCrimeType == CRIME_CAUSE_EXPLOSION &&
				(CrimeType == CRIME_CAUSE_EXPLOSION || CrimeType == CRIME_CAR_SET_ON_FIRE || CrimeType == CRIME_CHAIN_EXPLOSION) )
			{
				return true;
			}
		}
	}

	return false;
}
