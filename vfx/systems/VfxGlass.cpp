///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxGlass.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	04 Jun 2007
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxGlass.h"

// rage

// framework
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxhistory.h"
#include "vfx/ptfx/ptfxwrapper.h"

// game
#include "Scene/Physical.h"
#include "Vfx/Particles/PtFxCollision.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/VfxHelper.h"
#include "audio/vehicleaudioentity.h"
#include "vehicles/Vehicle.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxGlass		g_vfxGlass;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxGlass
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CVfxGlass::Init								(unsigned UNUSED_PARAM(initMode))
{	
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CVfxGlass::Shutdown							(unsigned UNUSED_PARAM(shutdownMode))
{	
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxGlass::InitWidgets						()
{	
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxVehicleWindscreenSmash
///////////////////////////////////////////////////////////////////////////////

void			CVfxGlass::TriggerPtFxVehicleWindscreenSmash(CPhysical* pPhysical, Vec3V_In vPos, Vec3V_In vNormal, float force)
{
	// don't update effect if the vehicle is invisible
	if (pPhysical && !pPhysical->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("glass_windscreen",0x971D09B1));
	if (pFxInst)
	{
		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecZAlign(fxMat, vPos, vNormal, Vec3V(V_Z_AXIS_WZERO));

		// make the fx matrix relative to the entity
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pPhysical->GetMatrix(), fxMat);

		ptfxAttach::Attach(pFxInst, pPhysical, -1);

		pFxInst->SetOffsetMtx(relFxMat);

		ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pPhysical->GetVelocity()));

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("force", 0x2B56B5F7), force);

		pFxInst->Start();

		if(pPhysical->GetIsTypeVehicle())
		{
			((CVehicle*)pPhysical)->GetVehicleAudioEntity()->TriggerWindscreenSmashSound(vPos);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxVehicleWindowSmash
///////////////////////////////////////////////////////////////////////////////

void			CVfxGlass::TriggerPtFxVehicleWindowSmash	(CPhysical* pPhysical, Vec3V_In vPos, Vec3V_In vNormal, float force)
{
	// don't update effect if the vehicle is invisible
	if (pPhysical && !pPhysical->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("glass_side_window",0xFBDE6187));
	if (pFxInst)
	{
		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecZAlign(fxMat, vPos, vNormal, Vec3V(V_Z_AXIS_WZERO));

		// make the fx matrix relative to the entity
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pPhysical->GetMatrix(), fxMat);

		ptfxAttach::Attach(pFxInst, pPhysical, -1);

		pFxInst->SetOffsetMtx(relFxMat);

		ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pPhysical->GetVelocity()));

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("force", 0x2B56B5F7), force);

		pFxInst->Start();

		if(pPhysical->GetIsTypeVehicle())
		{
			((CVehicle*)pPhysical)->GetVehicleAudioEntity()->TriggerWindowSmashSound(vPos);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxGlassSmash
///////////////////////////////////////////////////////////////////////////////

void			CVfxGlass::TriggerPtFxGlassSmash			(CPhysical* pPhysical, Vec3V_In vPos, Vec3V_In vNormal, float force)
{
	// don't update effect if the entity is invisible
	if (pPhysical && !pPhysical->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("glass_smash",0x2A14DA01));
	if (pFxInst)
	{
		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecZ(fxMat, vPos, vNormal);

		// make the fx matrix relative to the entity
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pPhysical->GetMatrix(), fxMat);

		ptfxAttach::Attach(pFxInst, pPhysical, -1);

		pFxInst->SetOffsetMtx(relFxMat);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("force", 0x2B56B5F7), force);

		pFxInst->Start();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxGlassHitGround
///////////////////////////////////////////////////////////////////////////////

void			CVfxGlass::TriggerPtFxGlassHitGround		(Vec3V_In vPos)
{
	atHashWithStringNotFinal fxHashName = atHashWithStringNotFinal("glass_shards",0xDDC1D669);
	if (ptfxHistory::Check(fxHashName, 0, NULL, vPos, VFXHISTORY_GLASS_HIT_GROUND_DIST))
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(fxHashName);
	if (pFxInst)
	{
		pFxInst->SetBasePos(vPos);
		pFxInst->Start();

		ptfxHistory::Add(fxHashName, 0, NULL, vPos, VFXHISTORY_GLASS_HIT_GROUND_TIME);
	}
}




