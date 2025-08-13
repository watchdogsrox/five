#include "ai\debug\types\WantedDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

// game headers
#include "Game/Dispatch/DispatchManager.h"
#include "game\Wanted.h"
#include "Peds\Ped.h"
#include "script\script_hud.h"

CWantedDebugInfo::CWantedDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CWantedDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("WANTED");
		PushIndent();
			PushIndent();
				PushIndent();
					PrintWanted();
}

void CWantedDebugInfo::Visualise()
{
	if (!ValidateInput())
		return;

	const CWanted* pPlayerWanted = m_Ped->GetPlayerWanted();
	if (pPlayerWanted->GetWantedLevel() > WANTED_CLEAN)
	{
		grcDebugDraw::Line(pPlayerWanted->m_LastSpottedByPolice, VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition()), Color_red, Color_red);
	}
}

bool CWantedDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	if (!m_Ped->GetPlayerWanted())
		return false;

	return true;
}

void CWantedDebugInfo::PrintWanted()
{
	const CWanted* pPlayerWanted = m_Ped->GetPlayerWanted();

	ColorPrintLn(Color_orange, "Wanted Level: %d", pPlayerWanted->GetWantedLevel());
	ColorPrintLn(Color_orange, "Fake Wanted Level: %d", CScriptHud::iFakeWantedLevel);

	if (pPlayerWanted->GetWantedLevelLastSetFrom() < WL_FROM_MAX_LOCATIONS)
	{
		if (pPlayerWanted->GetWantedLevelLastSetFrom() == WL_FROM_SCRIPT)
		{
			ColorPrintLn(Color_orange, "Wanted level last set from: %s ... (%s)", sm_WantedLevelSetFromLocations[pPlayerWanted->GetWantedLevelLastSetFrom()], pPlayerWanted->m_WantedLevelLastSetFromScriptName);
		}
		else
		{
			ColorPrintLn(Color_orange, "Wanted level last set from: %s", sm_WantedLevelSetFromLocations[pPlayerWanted->GetWantedLevelLastSetFrom()]);
		}
	}
	else
	{
		ColorPrintLn(Color_orange, "Wanted level last set from: Unrecognized");
	}

	ColorPrintLn(Color_blue, "  Actual value (%d)", pPlayerWanted->m_nWantedLevel);
	ColorPrintLn(Color_blue, "  Max Level (%d)", pPlayerWanted->MaximumWantedLevel);
	ColorPrintLn(Color_blue, "  Time Outside circle (%f)", ((float)pPlayerWanted->m_TimeOutsideCircle)/1000.f);
	ColorPrintLn(Color_blue, "  Time since search refocused (%d)", pPlayerWanted->GetTimeSinceSearchLastRefocused());
	ColorPrintLn(Color_blue, "  Time since last spotted (%d)", pPlayerWanted->GetTimeSinceLastSpotted());
	ColorPrintLn(Color_blue, "  Time first spotted (%d)", pPlayerWanted->m_iTimeFirstSpotted);
	ColorPrintLn(Color_blue, "  Has been seen (%s)", (!pPlayerWanted->CopsAreSearching() && (pPlayerWanted->m_iTimeFirstSpotted>0)) ? "Yes" : "No");
	ColorPrintLn(Color_blue, "  Has left initial search area (%s)", pPlayerWanted->m_HasLeftInitialSearchArea ?  "Yes" : "No");
	int iTimeUntilSearchAreaTimesOut = pPlayerWanted->m_iTimeInitialSearchAreaWillTimeOut - fwTimer::GetTimeInMilliseconds();
	iTimeUntilSearchAreaTimesOut = MAX((int)0, iTimeUntilSearchAreaTimesOut);
	ColorPrintLn(Color_blue, "  Search area will time out in (%d)", iTimeUntilSearchAreaTimesOut);
	ColorPrintLn(Color_blue, "  Time since hidden evasion started (%d)", pPlayerWanted->m_HasLeftInitialSearchArea ? pPlayerWanted->GetTimeSinceHiddenEvasionStarted() : pPlayerWanted->m_iTimeHiddenWhenEvasionReset);
	ColorPrintLn(Color_blue, "  Max time until hidden evasion starts (%f)", pPlayerWanted->GetTimeUntilHiddenEvasionStarts());
	ColorPrintLn(Color_blue, "  Script multiplier (%f)", pPlayerWanted->m_fMultiplier);
	ColorPrintLn(Color_blue, "  Hidden evasion disabled this frame (%s)", pPlayerWanted->m_SuppressLosingWantedLevelIfHiddenThisFrame ?  "Yes" : "No");
	ColorPrintLn(Color_red, "  Last spotted (%f, %f, %f)", pPlayerWanted->m_LastSpottedByPolice.x, pPlayerWanted->m_LastSpottedByPolice.y, pPlayerWanted->m_LastSpottedByPolice.z);
	ColorPrintLn(Color_red, "  Time evading (%f)", pPlayerWanted->m_fTimeEvading);
	ColorPrintLn(Color_red, "  Search centre (%f, %f, %f)", pPlayerWanted->m_CurrentSearchAreaCentre.x, pPlayerWanted->m_CurrentSearchAreaCentre.y, pPlayerWanted->m_CurrentSearchAreaCentre.z);
	ColorPrintLn(Color_red, "  Distance to search centre (%f)", pPlayerWanted->m_CurrentSearchAreaCentre.Dist(VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition())));
	ColorPrintLn(Color_green, "  Difficulty (%f)", pPlayerWanted->GetDifficulty());
	ColorPrintLn(Color_green, "  Time Since Last Hostile Action (%f)", pPlayerWanted->m_fTimeSinceLastHostileAction);
	ColorPrintLn(Color_purple, "  All Randoms Flee (%s)", pPlayerWanted->m_AllRandomsFlee ? "yes" : "no");
	ColorPrintLn(Color_purple, "  All Neutral Randoms Flee (%s)", pPlayerWanted->m_AllNeutralRandomsFlee ? "yes" : "no");
	ColorPrintLn(Color_purple, "  All Random Peds Only Attack With Guns (%s)", pPlayerWanted->m_AllRandomsOnlyAttackWithGuns ? "yes" : "no");
	ColorPrintLn(Color_purple, "  Everybody back off (%s)", pPlayerWanted->m_EverybodyBackOff ? "yes" : "no");
	ColorPrintLn(Color_purple, "  Police back off (%s)", pPlayerWanted->PoliceBackOff() ? "yes" : "no");
	ColorPrintLn(Color_purple, "  Within Amnesty (%s)", pPlayerWanted->GetWithinAmnestyTime() ? "yes" : "no");

	for(int i = 0; i < DT_Max; i++)
	{
		// Don't bother with some dispatch types
		if(i == DT_Invalid || i == DT_FireDepartment || i == DT_AmbulanceDepartment || i == DT_PoliceAutomobileWaitPulledOver || i == DT_PoliceAutomobileWaitCruising)
		{
			continue;
		}

		// Check if the dispatch is inactive otherwise see if resource creation is blocked
		if(CDispatchManager::GetInstance().IsDispatchEnabled((DispatchType)i) == false)
		{
			// combine debug info for ones without resource creation blocked as well
			if(CDispatchManager::GetInstance().IsDispatchResourceCreationBlocked((DispatchType)i))
			{
				ColorPrintLn(Color_purple, "  %s is inactive & resource creation blocked", CDispatchManager::GetInstance().GetDispatchTypeName((DispatchType)i));
			}
			else
			{
				ColorPrintLn(Color_purple, "  %s is inactive", CDispatchManager::GetInstance().GetDispatchTypeName((DispatchType)i));
			}
		}
		else if(CDispatchManager::GetInstance().IsDispatchResourceCreationBlocked((DispatchType)i))
		{
			ColorPrintLn(Color_purple, "  %s resource creation blocked", CDispatchManager::GetInstance().GetDispatchTypeName((DispatchType)i));
		}		
	}
	
	for (int i = 0; i < pPlayerWanted->m_MaxCrimesBeingQd; i++)
	{
		if (pPlayerWanted->CrimesBeingQd[i].CrimeType != CRIME_NONE && !pPlayerWanted->CrimesBeingQd[i].bAlreadyReported)
		{
			f32 fTime = (float)((pPlayerWanted->CrimesBeingQd[i].TimeOfQing + CRIMES_GET_REPORTED_TIME) - fwTimer::GetTimeInMilliseconds())/1000.0f;
			atHashString crimeHash = atHashString(g_CrimeNameHashes[pPlayerWanted->CrimesBeingQd[i].CrimeType]);
			ColorPrintLn(Color_green, "  Crime queued: %s, Increase (%d), Time(%f)", crimeHash.GetCStr(), pPlayerWanted->CrimesBeingQd[i].WantedScoreIncrement, fTime);
		}
	}

	for (int i = 0; i < pPlayerWanted->m_ReportedCrimes.GetCount(); i++)
	{
		if (pPlayerWanted->m_ReportedCrimes[i].m_eCrimeType != CRIME_NONE)
		{
			f32 fTime = (float)(pPlayerWanted->m_ReportedCrimes[i].m_uTimeReported) / 1000.0f;
			atHashString crimeHash = atHashString(g_CrimeNameHashes[pPlayerWanted->m_ReportedCrimes[i].m_eCrimeType]);
			ColorPrintLn(Color_red, "  Crime reported: %s, Increase (%d), Time (%f)", crimeHash.GetCStr(), pPlayerWanted->m_ReportedCrimes[i].m_uIncrement, fTime);
		}
	}

	if (m_Ped->GetIsInVehicle() && m_Ped->IsPlayer())
	{
		ColorPrintLn(Color_blue, "  Is In Stolen Vehicle (%s), Has Spotter (%s)",	m_Ped->GetVehiclePedInside()->m_nVehicleFlags.bIsStolen ? "yes" : "no", 
																					m_Ped->GetPlayerInfo()->GetSpotterOfStolenVehicle() != NULL ? "yes" : "no");
	}
}
#endif // AI_DEBUG_OUTPUT_ENABLED