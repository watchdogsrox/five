/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchMgrBase.cpp
// PURPOSE : base classes related to player switch management
// AUTHOR :  Ian Kiigan
// CREATED : 14/11/12
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/playerswitch/PlayerSwitchMgrBase.h"
#include "camera/CamInterface.h"
#include "fwsys/timer.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetControlFlag
// PURPOSE:		change state of control flag
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrBase::SetControlFlag(s32 controlFlag, bool bEnabled)
{
	s32	controlFlags = m_params.m_controlFlags;

	if (bEnabled)
	{
		controlFlags |= controlFlag;
	}
	else
	{
		controlFlags &= ~controlFlag;
	}

	Displayf("[Playerswitch] control flags=0x%x was 0x%x", controlFlags, m_params.m_controlFlags);

	m_params.m_controlFlags = controlFlags;
	
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Start
// PURPOSE:		base class start fn, inits params etc and calls internal start fn
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrBase::Start(CPed& oldPed, CPed& newPed, const CPlayerSwitchParams& params)
{
	Displayf("[Playerswitch] Player switch requested, control flags=0x%x", params.m_controlFlags);
	Assertf( !m_bActive, "Already switching to %4.2f, %4.2f, %4.2f", m_vDestPos.GetXf(), m_vDestPos.GetYf(), m_vDestPos.GetZf() );

	m_bActive		= true;
	m_vStartPos		= oldPed.GetTransform().GetPosition();
	m_vDestPos		= newPed.GetTransform().GetPosition();
	m_oldPed		= &oldPed;
	m_newPed		= &newPed;
	m_params		= params;
	m_startTime		= fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	//@@: location CPLAYERSWITCHMGRBASE_START
	if ((params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_START_FROM_CAMPOS) != 0)
	{
		m_vStartPos = VECTOR3_TO_VEC3V( camInterface::GetPos() );
	}

	// initialize this value
	m_onScreenSpawnDistance = 1.f;

	StartInternal(oldPed, newPed, params);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		base class update fn, updates any common systems and then calls internal update fn
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrBase::Update()
{

#if !__FINAL
	if (fwTimer::IsGamePaused())
	{
		return;
	}
#endif	//!__FINAL

	if (!m_bActive)
		return;

	UpdateInternal();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Stop
// PURPOSE:		base class stop fn, shuts down any systems and calls internal stop fn
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrBase::Stop(bool bHard)
{
	m_bActive = false;

	//@@: location CPLAYERSWITCHMGRBASE_STOP
	StopInternal(bHard);

#if !__FINAL
	float fDuration = (float) (fwTimer::GetTimeInMilliseconds_NonScaledClipped() - m_startTime) / 1000.0f;
	Displayf("[Playerswitch] Finished player switch, total time taken = %4.2f seconds", fDuration);
#endif
}
