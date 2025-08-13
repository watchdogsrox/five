///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxProjectile.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	12 Nov 2009
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxProjectile.h"

// rage
#include "physics/simulator.h"

// framework
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/ptfx/ptfxwrapper.h"

// game
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/Aim/FirstPersonShooterCamera.h"
#include "control/replay/replay.h"
#include "control/replay/Effects/ParticleWeaponFxPacket.h"
#include "control/replay/Effects/ParticleFireFxPacket.h"
#include "ModelInfo/WeaponModelInfo.h"
#include "Objects/Object.h"
#include "Peds/Ped.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/VfxHelper.h"
#include "Weapons/Projectiles/Projectile.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxProjectile		g_vfxProjectile;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxProjectile
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CVfxProjectile::Init							(unsigned UNUSED_PARAM(initMode))
{	
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CVfxProjectile::Shutdown						(unsigned UNUSED_PARAM(shutdownMode))
{	
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxProjectile::InitWidgets						()
{	
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  ProcessPtFxProjProximity
///////////////////////////////////////////////////////////////////////////////

void			CVfxProjectile::ProcessPtFxProjProximity(CProjectile* pProjectile, bool isProximityAcvtive)
{
	if (isProximityAcvtive)
	{
		if (pProjectile->GetInfo()->GetProximityFxHashName())
		{
			// register the fx system
			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pProjectile, PTFXOFFSET_PROJECTILE_PROXIMITY, true, pProjectile->GetInfo()->GetProximityFxHashName(), justCreated);
			if (pFxInst)
			{
				ptfxAttach::Attach(pFxInst, pProjectile, -1);

				// check if the effect has just been created 
				if (justCreated)
				{
					// it has just been created - start it and set its position
					pFxInst->Start();
				}
			}
		}
	}
	else
	{
		RemovePtFxProjProximity(pProjectile);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  ProcessPtFxProjProximity
///////////////////////////////////////////////////////////////////////////////

void			CVfxProjectile::RemovePtFxProjProximity(CProjectile* pProjectile)
{
	ptfxRegInst* pRegFxInst = ptfxReg::Find(pProjectile, PTFXOFFSET_PROJECTILE_PROXIMITY);
	if (pRegFxInst)
	{
		if (pRegFxInst->m_pFxInst)
		{
			pRegFxInst->m_pFxInst->Finish(false);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  ProcessPtFxProjFuse
///////////////////////////////////////////////////////////////////////////////

void			CVfxProjectile::ProcessPtFxProjFuse		(CProjectile* pProjectile, bool isPrimed)
{	
	UpdatePtFxProjFuse(pProjectile, isPrimed, false);
	UpdatePtFxProjFuse(pProjectile, isPrimed, true);
}




///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxProjFuse
///////////////////////////////////////////////////////////////////////////////

void			CVfxProjectile::UpdatePtFxProjFuse		(CProjectile* pProjectile, bool isPrimed, bool isFirstPersonPass)
{	
	// don't process if the projectile is invisible
	if (!pProjectile->GetIsVisible())
	{
		return;
	}

	// don't process if the projectile isn't in range
	if (CVfxHelper::GetDistSqrToCamera(pProjectile->GetTransform().GetPosition())>VFXRANGE_WPN_MOLOTOV_FLAME_SQR)
	{
		return;
	}

	// check if the first person camera is active for the ped holding this projectile
	bool isFirstPersonCameraActive = false;
#if FPS_MODE_SUPPORTED
	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	CEntity* pProjectileOwner = pProjectile->GetOwner();
	isFirstPersonCameraActive = pCamera && pCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && 
		pProjectileOwner && pProjectileOwner->GetIsTypePed() && static_cast<const CPed*>(pProjectileOwner)->IsLocalPlayer();
#endif

	// deal with the first person camera being inactive
	if (!isFirstPersonCameraActive)
	{
		// remove any first person specific effects
		RemovePtFxProjFuse(pProjectile, false, true);

		// return if this is the first person pass
		if (isFirstPersonPass)
		{
			return;
		}
	}
	else
	{
		// remove any non first person specific effects
		RemovePtFxProjFuse(pProjectile, true, false);
	}


	// get the name of the effect to play
	atHashWithStringNotFinal fuseEffectHashName;
	if (isFirstPersonPass)
	{
		if (isPrimed)
		{
			fuseEffectHashName = pProjectile->GetInfo()->GetPrimedFirstPersonFxHashName()!=0 ? 
								 pProjectile->GetInfo()->GetPrimedFirstPersonFxHashName() : 
								 pProjectile->GetInfo()->GetPrimedFxHashName();
		}
		else
		{
			fuseEffectHashName = pProjectile->GetInfo()->GetFuseFirstPersonFxHashName()!=0 ? 
								 pProjectile->GetInfo()->GetFuseFirstPersonFxHashName() : 
								 pProjectile->GetInfo()->GetFuseFxHashName();
		}
	}
	else
	{
		fuseEffectHashName = isPrimed ? 
							 pProjectile->GetInfo()->GetPrimedFxHashName() : 
							 pProjectile->GetInfo()->GetFuseFxHashName();
	}

	if (fuseEffectHashName==0)
	{
		return;
	}

	// get the effect offset
	int classFxOffset = PTFXOFFSET_PROJECTILE_FUSE;
	if (isFirstPersonCameraActive)
	{
		if (isFirstPersonPass)
		{
			classFxOffset = PTFXOFFSET_PROJECTILE_FUSE_ALT_FP;
		}
		else
		{
			classFxOffset = PTFXOFFSET_PROJECTILE_FUSE_ALT_NORM;
		}
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pProjectile, classFxOffset, true, fuseEffectHashName, justCreated);
	if (pFxInst)
	{
		CWeaponModelInfo* pModelInfo = static_cast<CWeaponModelInfo*>(pProjectile->GetBaseModelInfo());
		s32 trailBoneId = pModelInfo->GetBoneIndex(WEAPON_VFX_PROJTRAIL);

		if (isFirstPersonPass)
		{
			// the first person pass plays the alternative effect, attached to the normal skeleton bone, only rendered in the first person view
			ptfxAttach::Attach(pFxInst, pProjectile, trailBoneId);
			pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW);
		}
		else 
		{
			// the normal (non-first person) pass plays the normal effect
			if (isFirstPersonCameraActive)
			{
				// if the first person camera is active we want to attach to the alternative skeleton and only render in the non first person view 
				ptfxAttach::Attach(pFxInst, pProjectile, trailBoneId, PTFXATTACHMODE_FULL, true);
				pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW);
			}
			else
			{
				// if the first person camera isn't active we want to attach to the normal skeleton and render at all times
				ptfxAttach::Attach(pFxInst, pProjectile, trailBoneId);
				pFxInst->ClearFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW);
			}
		}

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->Start();
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx(CPacketFireProjectileFx(isPrimed, isFirstPersonCameraActive, isFirstPersonPass, fuseEffectHashName), pProjectile);
		}
#endif // GTA_REPLAY

	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemovePtFxProjFuse
///////////////////////////////////////////////////////////////////////////////

void			CVfxProjectile::RemovePtFxProjFuse		(CProjectile* pProjectile, bool removeNormalPtFx, bool removeAltPtFx)
{	
	// when the projectile becomes active we need to make sure the fuse effect is removed
	// this is because the inactive projectile gets deleted and a new active projectile is created
	// so the fuse effect gets left behind in its last position to play out
	// instead we just want to remove it by setting it as finished

	// find the fx system
	if (removeNormalPtFx)
	{
		ptfxRegInst* pRegFxInst = ptfxReg::Find(pProjectile, PTFXOFFSET_PROJECTILE_FUSE);
		if (pRegFxInst)
		{
			if (pRegFxInst->m_pFxInst)
			{
				pRegFxInst->m_pFxInst->Finish(false);
			}
		}
	}

	// find the fx system
	if (removeAltPtFx)
	{
		ptfxRegInst* pRegFxInst = ptfxReg::Find(pProjectile, PTFXOFFSET_PROJECTILE_FUSE_ALT_FP);
		if (pRegFxInst)
		{
			if (pRegFxInst->m_pFxInst)
			{
				pRegFxInst->m_pFxInst->Finish(false);
			}
		}

		pRegFxInst = ptfxReg::Find(pProjectile, PTFXOFFSET_PROJECTILE_FUSE_ALT_NORM);
		if (pRegFxInst)
		{
			if (pRegFxInst->m_pFxInst)
			{
				pRegFxInst->m_pFxInst->Finish(false);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  GetWeaponProjTintColour
///////////////////////////////////////////////////////////////////////////////

Vec3V_Out		CVfxProjectile::GetWeaponProjTintColour(u8 weaponTintIndex)
{
	Vec3V vTintRGB;
	if (weaponTintIndex==0)
	{
		// red
		vTintRGB = Vec3V(255.0f/255.0f, 127.0f/255.0f, 128.0f/255.0f);
	}
	else if (weaponTintIndex==1)
	{
		// green
		vTintRGB = Vec3V(75.0f/255.0f, 80.0f/255.0f, 55.0f/255.0f);
	}
	else if (weaponTintIndex==2)
	{
		// gold
		vTintRGB = Vec3V(212.0f/255.0f, 142.0f/255.0f, 31.0f/255.0f);
	}
	else if (weaponTintIndex==3)
	{
		// pink
		vTintRGB = Vec3V(247.0f/255.0f, 31.0f/255.0f, 132.0f/255.0f);
	}
	else if (weaponTintIndex==4)
	{
		// tan
		vTintRGB = Vec3V(120.0f/255.0f, 101.0f/255.0f, 66.0f/255.0f);
	}
	else if (weaponTintIndex==5)
	{
		// blue
		vTintRGB = Vec3V(27.0f/255.0f, 53.0f/255.0f, 80.0f/255.0f);
	}
	else if (weaponTintIndex==6)
	{
		// orange
		vTintRGB = Vec3V(180.0f/255.0f, 63.0f/255.0f, 35.0f/255.0f);
	}
	else if (weaponTintIndex==7)
	{
		// platinum
		vTintRGB = Vec3V(111.0f/255.0f, 137.0f/255.0f, 145.0f/255.0f);
	}

	return vTintRGB;
}

///////////////////////////////////////////////////////////////////////////////
//  GetAltWeaponProjTintColour
///////////////////////////////////////////////////////////////////////////////

Vec3V_Out		CVfxProjectile::GetAltWeaponProjTintColour(u8 weaponTintIndex)
{
	Vec3V vTintRGB = Vec3V(255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f);

	if (weaponTintIndex == 0)
	{
		// blue tint (yellow rings)
		vTintRGB = Vec3V(255.0f / 255.0f, 252.0f / 255.0f, 75.0f / 255.0f);
	}
	else if (weaponTintIndex == 1)
	{
		// purple tint (orange rings)
		vTintRGB = Vec3V(255.0f / 255.0f, 130.0f / 255.0f, 30.0f / 255.0f);
	}
	else if (weaponTintIndex == 2)
	{
		// green tint (blue rings)
		vTintRGB = Vec3V(75.0f / 255.0f, 228.0f / 255.0f, 255.0f / 255.0f);
	}
	else if (weaponTintIndex == 3)
	{
		// orange tint (red rings)
		vTintRGB = Vec3V(255.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f);
	}
	else if (weaponTintIndex == 4)
	{
		// pink tint (cream rings)
		vTintRGB = Vec3V(255.0f / 255.0f, 255.0f / 255.0f, 169.0f / 255.0f);
	}
	else if (weaponTintIndex == 5)
	{
		// gold tint (gold rings)
		vTintRGB = Vec3V(208.0f / 255.0f, 203.0f / 255.0f, 173.0f / 255.0f);
	}
	else if (weaponTintIndex == 6)
	{
		// platinum tint (white rings)
		vTintRGB = Vec3V(255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f);
	}

	return vTintRGB;
}

///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxProjTrail
///////////////////////////////////////////////////////////////////////////////

void			CVfxProjectile::UpdatePtFxProjTrail		(CProjectile* pProjectile, float trailEvo, u8 weaponTintIndex)
{	
	if (CVfxHelper::GetDistSqrToCamera(pProjectile->GetTransform().GetPosition())>VFXRANGE_WPN_ROCKET_TRAIL_SQR)
	{
		return;
	}

	// get info about the effect
	int classFxOffset;
	atHashWithStringNotFinal trailEffectHashName;
	if (pProjectile->GetIsInWater())
	{
		classFxOffset = PTFXOFFSET_PROJECTILE_TRAIL_UNDERWATER;
		trailEffectHashName = pProjectile->GetInfo()->GetTrailFxUnderWaterHashName();
	}
	else
	{
		classFxOffset = PTFXOFFSET_PROJECTILE_TRAIL;
		trailEffectHashName = pProjectile->GetInfo()->GetTrailFxHashName();
	}

	if (trailEffectHashName==0)
	{
		return;
	}

	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pProjectile, classFxOffset, true, trailEffectHashName, justCreated);

	if (pFxInst)
	{
		CWeaponModelInfo* pModelInfo = static_cast<CWeaponModelInfo*>(pProjectile->GetBaseModelInfo());
		s32 trailBoneId = pModelInfo->GetBoneIndex(WEAPON_VFX_PROJTRAIL);

		ptfxAttach::Attach(pFxInst, pProjectile, trailBoneId);

		pFxInst->SetCanMoveVeryFast(true);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("trail", 0x81528544), trailEvo);

		Vec3V colour = pProjectile->GetInfo()->GetFxUseAlternateTintColor() ? GetAltWeaponProjTintColour(weaponTintIndex) : GetWeaponProjTintColour(weaponTintIndex);

		if(RenderPhaseSeeThrough::GetState())
			colour = Vec3V(V_ONE);

		pFxInst->SetColourTint(colour);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->Start();
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWeaponProjTrailFx>(
				CPacketWeaponProjTrailFx(trailEffectHashName,
					classFxOffset,
					trailBoneId,
					trailEvo,
					colour),
				pProjectile);
		}
#endif // GTA_REPLAY
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxProjGround
///////////////////////////////////////////////////////////////////////////////

void			CVfxProjectile::UpdatePtFxProjGround		(CProjectile* pProjectile, WorldProbe::CShapeTestHitPoint& shapeTestResult)
{
	if (CVfxHelper::GetDistSqrToCamera(pProjectile->GetTransform().GetPosition())>VFXRANGE_WPN_ROCKET_TRAIL_SQR)
	{
		return;
	}

	atHashWithStringNotFinal fxHashName = u32(0);
	VfxDisturbanceType_e vfxDisturbanceType = PGTAMATERIALMGR->GetMtlVfxDisturbanceType(shapeTestResult.GetHitMaterialId());
	if (vfxDisturbanceType==VFX_DISTURBANCE_DEFAULT)
	{
		fxHashName = pProjectile->GetInfo()->GetDisturbFxDefaultHashName();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_SAND)
	{
		fxHashName = pProjectile->GetInfo()->GetDisturbFxSandHashName();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_DIRT)
	{
		fxHashName = pProjectile->GetInfo()->GetDisturbFxDirtHashName();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_WATER)
	{
		fxHashName = pProjectile->GetInfo()->GetDisturbFxWaterHashName();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_FOLIAGE)
	{
		fxHashName = pProjectile->GetInfo()->GetDisturbFxFoliageHashName();
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pProjectile, PTFXOFFSET_PROJECTILE_GROUND, true, fxHashName, justCreated);

	if (pFxInst)
	{
		pFxInst->SetBasePos(RCC_VEC3V(shapeTestResult.GetHitPosition()));

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("height", 0x52FDF336), shapeTestResult.GetHitTValue());

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->Start();
		}
	}
}


