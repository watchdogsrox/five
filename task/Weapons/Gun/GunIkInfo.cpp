// Class header
#include "Task/Weapons/Gun/GunIkInfo.h"

// Game headers
#include "Ik/solvers/TorsoSolver.h"
#include "Peds/Ped.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CGunIkInfo
//////////////////////////////////////////////////////////////////////////

CGunIkInfo::CGunIkInfo()
: m_fTorsoOffsetYaw(0.0f)
, m_fTorsoMinYaw(CTorsoIkSolver::ms_torsoInfo.minYaw)
, m_fTorsoMaxYaw(CTorsoIkSolver::ms_torsoInfo.maxYaw)
, m_fTorsoMinPitch(CTorsoIkSolver::ms_torsoInfo.minPitch)
, m_fTorsoMaxPitch(CTorsoIkSolver::ms_torsoInfo.maxPitch)
, m_bDisablePitchFixUp(false)
, m_fSpineMatrixInterpRate(1.0f)
{
}

CGunIkInfo::CGunIkInfo(float fTorsoOffsetYaw, float fTorsoMinYaw, float fTorsoMaxYaw, float fTorsoMinPitch, float fTorsoMaxPitch)
: m_fTorsoOffsetYaw(fTorsoOffsetYaw)
, m_fTorsoMinYaw(fTorsoMinYaw)
, m_fTorsoMaxYaw(fTorsoMaxYaw)
, m_fTorsoMinPitch(fTorsoMinPitch)
, m_fTorsoMaxPitch(fTorsoMaxPitch)
, m_bDisablePitchFixUp(false)
, m_fSpineMatrixInterpRate(1.0f)
{
}

void CGunIkInfo::ApplyIkInfo(CPed* pPed)
{
	// Set the offset, yaw and pitch to be used by the torso Ik
	pPed->GetIkManager().SetTorsoOffsetYaw(m_fTorsoOffsetYaw);
	pPed->GetIkManager().SetTorsoMinYaw(m_fTorsoMinYaw);
	pPed->GetIkManager().SetTorsoMaxYaw(m_fTorsoMaxYaw);
	pPed->GetIkManager().SetTorsoMinPitch(m_fTorsoMinPitch);
	pPed->GetIkManager().SetTorsoMaxPitch(m_fTorsoMaxPitch);
	pPed->GetIkManager().SetDisablePitchFixUp(m_bDisablePitchFixUp);
}
