/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchInterface.cpp
// PURPOSE : interface to player switch system
// AUTHOR :  Ian Kiigan
// CREATED : 02/10/12
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/playerswitch/PlayerSwitchInterface.h"

#include "camera/CamInterface.h"
#include "scene/playerswitch/PlayerSwitchEstablishingShot.h"
#include "scene/playerswitch/PlayerSwitchInterface_parser.h"
#include "scene/world/GameWorldHeightMap.h"
#include "vectormath/scalarv.h"

CPlayerSwitchInterface g_PlayerSwitch;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CPlayerSwitchInterface
// PURPOSE:		ctor, init settings etc
//////////////////////////////////////////////////////////////////////////
CPlayerSwitchInterface::CPlayerSwitchInterface()
	: m_switchType(SWITCH_TYPE_LONG)
{
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Start
// PURPOSE:		initiate a player switch of the specified type, from old ped to new ped
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchInterface::Start(s32 switchType, CPed& oldPed, CPed& newPed, s32 controlFlags)
{
#if !__FINAL
	const Vec3V vOldPos = oldPed.GetTransform().GetPosition();
	const Vec3V vNewPos = newPed.GetTransform().GetPosition();
	Displayf("[Playerswitch] PLAYER SWITCH START: from (%4.2f, %4.2f, %4.2f) to (%4.2f, %4.2f, %4.2f)",
		vOldPos.GetXf(), vOldPos.GetYf(), vOldPos.GetZf(),
		vNewPos.GetXf(), vNewPos.GetYf(), vNewPos.GetZf() );
#endif	//!__FINAL

	// if auto is requested, choose most appropriate based on 2D distance from oldPed to newPed
	m_switchType = ( (switchType==SWITCH_TYPE_AUTO) ? AutoSelectSwitchType(oldPed, newPed, controlFlags) : switchType );

	CPlayerSwitchParams params;
	SetupParams(params, m_switchType, oldPed, newPed, controlFlags);

	CPlayerSwitchMgrBase* pMgr = GetMgr(m_switchType);
	Assert(pMgr);

	pMgr->Start(oldPed, newPed, params);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates player switch managers and peripheral systems each tick
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchInterface::Update()
{

#if __ASSERT
	if(IsActive())
	{
		CPlayerInfo	*pPlayerInfo = CGameWorld::GetMainPlayerInfo();
		if (pPlayerInfo)
		{
			CPed *pPed = pPlayerInfo->GetPlayerPed();
			if(pPed && pPed->IsDead())
			{
				const Vec3V vPos = pPed->GetTransform().GetPosition();
				Assertf(false, "Player switch: current player ped %s (address %p) is dead (%4.2f, %4.2f, %4.2f)",
					pPed->GetModelName(), pPed, vPos.GetXf(), vPos.GetYf(), vPos.GetZf());
			}
		}
	}
#endif

	m_switchMgrLong.Update();
	m_switchMgrShort.Update();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Stop
// PURPOSE:		abort an active switch early
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchInterface::Stop()
{
	if (IsActive())
	{
		CPlayerSwitchMgrBase* pMgr = GetMgr(m_switchType);
		if (pMgr)
		{
			return pMgr->Stop(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AutoSelectSwitchType
// PURPOSE:		select most appropriate switch type based on distance from oldPed to newPed
//////////////////////////////////////////////////////////////////////////
s32 CPlayerSwitchInterface::AutoSelectSwitchType(CPed& oldPed, CPed& newPed, s32 UNUSED_PARAM(controlFlags))
{
	const Vec3V vStart			= oldPed.GetTransform().GetPosition();
	const Vec3V vEnd			= newPed.GetTransform().GetPosition();

	s32 idealSwitchType = GetIdealSwitchType(vStart, vEnd);

	return idealSwitchType;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetIdealSwitchType
// PURPOSE:		returns the ideal switch type based on the distance between start and end
//////////////////////////////////////////////////////////////////////////
s32 CPlayerSwitchInterface::GetIdealSwitchType(Vec3V_In vStart, Vec3V_In vEnd) const
{
	const ScalarV dist2d		= DistXYFast(vStart, vEnd);
	const ScalarV dist3d		= Dist(vStart, vEnd);
	const ScalarV shortRange	= ScalarV( m_switchSettings[SWITCH_TYPE_SHORT].GetRange() );
	const ScalarV mediumRange	= ScalarV( m_switchSettings[SWITCH_TYPE_MEDIUM].GetRange() );
	const ScalarV mediumMaxZ	= ScalarV( m_switchSettings[SWITCH_TYPE_MEDIUM].GetCeilingHeight() );

	if (	IsGreaterThanOrEqualAll(dist2d, mediumRange)
		||	IsGreaterThanOrEqualAll(vStart.GetZ(), mediumMaxZ)
		||	IsGreaterThanOrEqualAll(vEnd.GetZ(), mediumMaxZ)		)
	{
		// if XY distance is greater than range of medium switch, using long range switch
		return SWITCH_TYPE_LONG;
	}

	if (IsLessThanOrEqualAll(dist3d, shortRange))
	{
		// otherwise, check if XYZ distance is within range of short range switch
		return SWITCH_TYPE_SHORT;
	}

	return SWITCH_TYPE_MEDIUM;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetupParams
// PURPOSE:		fills in params structure for initiating a switch
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchInterface::SetupParams(CPlayerSwitchParams& params, s32 switchType, CPed& oldPed, CPed& newPed, s32 controlFlags)
{
	CPlayerSwitchSettings& settings = m_switchSettings[switchType];

	params.m_switchType			= switchType;
	params.m_controlFlags		= controlFlags;
	params.m_fPanSpeed			= settings.GetPanSpeed();
	params.m_ceilingLodLevel	= settings.GetCeilingLodLevel();

	//////////////////////////////////////////////////////////////////////////
	// calculate an acceptable ceiling height
	const bool bStartFromCamPos		= ((controlFlags & CONTROL_FLAG_START_FROM_CAMPOS) != 0);
	const Vec3V vOldPos				= bStartFromCamPos ? VECTOR3_TO_VEC3V(camInterface::GetPos()) : oldPed.GetTransform().GetPosition();
	const Vec3V vNewPos				= newPed.GetTransform().GetPosition();
	const float fRequestedHeight	= settings.GetCeilingHeight();
	const float fHeightAbove		= settings.GetMinHeightAbove();
	const float fWorldHeight		= CGameWorldHeightMap::GetMaxIntervalHeightFromWorldHeightMap(vOldPos.GetXf(), vOldPos.GetYf(), vNewPos.GetXf(), vNewPos.GetYf());

	// determine minimum acceptable height above ground	and peds
	const float fMinHeightRequiredByMap		= ( fHeightAbove + fWorldHeight );
	const float fMinHeightRequiredByPeds	= ( fHeightAbove + Max(vOldPos.GetZf(), vNewPos.GetZf()) );
	float fIntendedCeilingHeight			= Max( fRequestedHeight, fMinHeightRequiredByMap );

	params.m_fCeilingHeight					= Max( fIntendedCeilingHeight, fMinHeightRequiredByPeds );

	//////////////////////////////////////////////////////////////////////////
	// determine number of jump cuts on ascent
	const float fHeightAboveOldPed		= ( fHeightAbove + vOldPos.GetZf() );
	const float fHeightAboveNewPed		= ( fHeightAbove + vNewPos.GetZf() );
	const float fHeightAboveOldWorldPos	= ( fHeightAbove + CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vOldPos.GetXf(), vOldPos.GetYf()) );
	const float fHeightAboveNewWorldPos	= ( fHeightAbove + CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vNewPos.GetXf(), vNewPos.GetYf()) );
	params.m_fLowestHeightAscent		= Max( fHeightAboveOldPed, fHeightAboveOldWorldPos );
	params.m_fLowestHeightDescent		= Max( fHeightAboveNewPed, fHeightAboveNewWorldPos );
	const float fJumpCutRangeAscent		= ( params.m_fCeilingHeight - params.m_fLowestHeightAscent );
	const float fJumpCutRangeDescent	= ( params.m_fCeilingHeight - params.m_fLowestHeightDescent );
	
	params.m_numJumpsAscent = Clamp( (u32)(fJumpCutRangeAscent/settings.GetHeightBetweenJumps()), settings.GetNumJumpsMin(), settings.GetNumJumpsMax() );
	params.m_numJumpsDescent = Clamp( (u32)(fJumpCutRangeDescent/settings.GetHeightBetweenJumps()), settings.GetNumJumpsMin(), settings.GetNumJumpsMax() );
	
	params.m_maxTimeToStayInStartupModeOnLongRangeDescent = settings.GetMaxTimeToStayInStartupModeOnDescent();
	params.m_scenarioAnimsTimeout = settings.GetScenarioAnimsTimeout();
	params.m_fRadiusOfStreamedScenarioPedCheck = settings.GetRadiusOfScenarioAnimsCheck();

	//////////////////////////////////////////////////////////////////////////
	// auto set some control flags to solve various edge cases
	if ((controlFlags & CONTROL_FLAG_UNKNOWN_DEST) == 0)
	{
		const ScalarV dist2d = DistXYFast(vOldPos, vNewPos);
		if (dist2d.Getf() <= settings.GetSkipPanRange())
		{
			// skip very short pans
			params.m_controlFlags |= CONTROL_FLAG_SKIP_PAN;
			params.m_controlFlags |= CONTROL_FLAG_SKIP_TOP_DESCENT;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsActive
// PURPOSE:		returns true if there is a player switch currently active, false otherwise
//////////////////////////////////////////////////////////////////////////
bool CPlayerSwitchInterface::IsActive() const
{
	const CPlayerSwitchMgrBase* pMgr = GetMgrConst(m_switchType);
	if (pMgr)
	{
		return pMgr->IsActive();
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ShouldDisableRadio
// PURPOSE:		returns true if radio needs to be disabled during switch, false otherwise
//////////////////////////////////////////////////////////////////////////
bool CPlayerSwitchInterface::ShouldDisableRadio() const
{
	if ( IsActive() )
	{
		switch (m_switchType)
		{
		case SWITCH_TYPE_LONG:
		case SWITCH_TYPE_MEDIUM:
			return ( m_switchMgrLong.GetState() <= CPlayerSwitchMgrLong::STATE_JUMPCUT_DESCENT );

		case SWITCH_TYPE_SHORT:
			return true;

		default:
			break;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ShouldUseFullCircleForPopStreaming
// PURPOSE:		returns true if the population streaming needs to use
//				a full circle for creation zone
//////////////////////////////////////////////////////////////////////////
bool CPlayerSwitchInterface::ShouldUseFullCircleForPopStreaming() const
{
	if ( IsActive() && m_switchType!=SWITCH_TYPE_SHORT )
	{
		return ( m_switchMgrLong.GetState() > CPlayerSwitchMgrLong::STATE_JUMPCUT_ASCENT && m_switchMgrLong.GetState() < CPlayerSwitchMgrLong::STATE_OUTRO_HOLD );
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetOnScreenSpawnDistance
// PURPOSE:		returns the distance that we'll allow vehicles to spawn on screen during the switch
//////////////////////////////////////////////////////////////////////////
float CPlayerSwitchInterface::GetOnScreenSpawnDistance() const
{
	const CPlayerSwitchMgrBase* pMgr = GetMgrConst(m_switchType);
	if (pMgr)
	{
		return pMgr->GetOnScreenSpawnDistance();
	}
	return 1.f;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SaveSettings
// PURPOSE:		save settings to metadata
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchInterface::SaveSettings()
{
#if __BANK
	const char* apszNames[SWITCH_TYPE_TOTAL] =
	{
		"SWITCH_TYPE_AUTO",
		"SWITCH_TYPE_LONG",
		"SWITCH_TYPE_MEDIUM",
		"SWITCH_TYPE_SHORT"
	};
	for (s32 i=0; i<SWITCH_TYPE_TOTAL; i++)
	{
		m_switchSettings[i].SetName(apszNames[i]);
	}
	Verifyf(PARSER.SaveObject(PLAYERSWITCH_SETTINGS_FILE, "meta", this, parManager::XML), "Failed to save player switch settings");
#endif	//__BANK
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	LoadSettings
// PURPOSE:		load settings from metadata
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchInterface::LoadSettings()
{
	PARSER.LoadObject(PLAYERSWITCH_SETTINGS_FILE, "meta", *this);

	CPlayerSwitchEstablishingShot::Load();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetDest
// PURPOSE:		provide the destination info if it was not already present
//				for example when switch to from single player to multiplayer
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchInterface::SetDest(CPed& newPed)
{
	CPlayerSwitchParams params = m_switchMgrLong.GetParams();

	const bool bNeedsDestInfo = IsActive() && GetSwitchType()!=SWITCH_TYPE_SHORT && ((params.m_controlFlags & CONTROL_FLAG_UNKNOWN_DEST)!=0);

	if (Verifyf(bNeedsDestInfo, "Providing destination ped, but it isn't required"))
	{
		// update params to have corrected floor and num jump cuts for descent
		// TODO - re-use SetupParams instead of duplicating logic here
		CPlayerSwitchSettings& settings		= m_switchSettings[ GetSwitchType() ];
		const Vec3V vNewPos					= newPed.GetTransform().GetPosition();
		const float fHeightAbove			= settings.GetMinHeightAbove();
		const float fHeightAboveNewPed		= ( fHeightAbove + vNewPos.GetZf() );
		const float fHeightAboveNewWorldPos	= ( fHeightAbove + CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vNewPos.GetXf(), vNewPos.GetYf()) );
		params.m_fLowestHeightDescent		= Max( fHeightAboveNewPed, fHeightAboveNewWorldPos );

		const float fJumpCutRangeDescent	= ( params.m_fCeilingHeight - params.m_fLowestHeightDescent );
		params.m_numJumpsDescent = Clamp( (u32)(fJumpCutRangeDescent/settings.GetHeightBetweenJumps()), settings.GetNumJumpsMin(), settings.GetNumJumpsMax() );

		//////////////////////////////////////////////////////////////////////////
		// auto set some control flags to solve various edge cases
		{
			const ScalarV dist2d = DistXYFast(m_switchMgrLong.GetStartPos(), vNewPos);

			Displayf("2d dist is %f", dist2d.Getf() );

			if (dist2d.Getf() <= settings.GetSkipPanRange())
			{
				// skip very short pans
				params.m_controlFlags |= CONTROL_FLAG_SKIP_PAN;
				params.m_controlFlags |= CONTROL_FLAG_SKIP_TOP_DESCENT;
			}
		}

		m_switchMgrLong.SetDest(newPed, params);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ModifyLodScale
// PURPOSE:		adjust lod scale for switcher
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchInterface::ModifyLodScale(float& fLodScale)
{
	if (IsActive())
	{
		CPlayerSwitchMgrBase* pMgr = GetMgr(m_switchType);
		if (pMgr)
		{
			pMgr->ModifyLodScale(fLodScale);
		}
	}
}
