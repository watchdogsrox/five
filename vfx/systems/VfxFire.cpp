///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxFire.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	04 Jun 2007
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxFire.h"

// rage

// framework
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "control/replay/replay.h"
#include "control/replay/effects/ParticleFireFxPacket.h"
#include "Peds/Ped.h"
#include "Physics/GtaMaterialManager.h"
#include "Renderer/Lights/Lights.h"
#include "Scene/Entity.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Particles/PtFxDefines.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "Vfx/VfxHelper.h"


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

CVfxFire		g_vfxFire;

BankFloat VFX_FIRE_WRECKED_VEH_PTFX_CAMERA_BIAS = -1.53f;

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxFire
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::Init								(unsigned initMode)
{	
	if (initMode == INIT_AFTER_MAP_LOADED)
	{
		// load in the data from the file
		LoadData();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::Shutdown							(unsigned UNUSED_PARAM(shutdownMode))
{	
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxFire::InitWidgets						()
{	
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Vfx Fire", false);
	{
		pVfxBank->AddTitle("INFO");

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddSlider("Wrecked Vehicle Fire Camera Bias", &VFX_FIRE_WRECKED_VEH_PTFX_CAMERA_BIAS, -10.0f, 10.0f, 0.001f);

	}
	pVfxBank->PopGroup();
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  ProcessPedFire
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::ProcessPedFire					(CFire* pFire, CPed* pPed)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if fire ptfx aren't enabled
	if (pVfxPedInfo->GetFirePtFxEnabled()==false)
	{
		return;
	}

	float burnRatio = pFire->GetLifeRatio();
	Vec4V vLimbBurnRatios = pFire->GetLimbBurnRatios();
	float limbBurnRatios[4];
	limbBurnRatios[0] = vLimbBurnRatios.GetXf();
	limbBurnRatios[1] = vLimbBurnRatios.GetYf();
	limbBurnRatios[2] = vLimbBurnRatios.GetZf();
	limbBurnRatios[3] = vLimbBurnRatios.GetWf();

	for (int i=0; i<pVfxPedInfo->GetFirePtFxNumInfos(); i++)
	{
		const VfxPedFireInfo& vfxPedFireInfo = pVfxPedInfo->GetFirePtFxInfo(i);
		if (vfxPedFireInfo.m_limbId==-1 || burnRatio<limbBurnRatios[vfxPedFireInfo.m_limbId])
		{
			UpdatePtFxFirePedBone(pFire, i, vfxPedFireInfo.m_boneTagA, vfxPedFireInfo.m_boneTagB, vfxPedFireInfo.m_ptFxName, pVfxPedInfo->GetFirePtFxSpeedEvoMin(), pVfxPedInfo->GetFirePtFxSpeedEvoMax());
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxFireMap
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::UpdatePtFxFireMap					(CFire* pFire)
{
	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;
	if (pFire->GetFlag(FIRE_FLAG_IS_SCRIPTED_PETROL_FIRE))
	{
		pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_petrol_script",0x5DD8E7C1), justCreated);
	}
	else
	{
		VfxFireInfo_s* pVfxFireInfo = GetInfo(PGTAMATERIALMGR->GetMtlVfxGroup(pFire->GetMtlId()));
		pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, pVfxFireInfo->ptFxMapFireHashName, justCreated);
	}

	if (pFxInst)
	{
		// this isn't an entity fire - set the position directly
		pFxInst->SetBaseMtx(pFire->GetMatrixOffset());

		// set the time of the effect based on the fire strength
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength", 0xFF1C4627), pFire->GetCurrStrength());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("spread", 0xB2D379B8), pFire->GetMaxStrengthRatio());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fuel", 0x4652DA40), pFire->GetFuelLevel());

		if (pFire->GetFlag(FIRE_FLAG_IS_SCRIPTED_PETROL_FIRE))
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fadein", 0x68CB0025), 1.0f);
		}

		// check if the effect has just been created 
		if (justCreated)
		{
#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld())
			{
				pFxInst->SetCanBeCulled(false);
			}
#endif // GTA_REPLAY

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
} 


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxFirePetrolTrail
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::UpdatePtFxFirePetrolTrail				(CFire* pFire)
{
	// calc the distance to the parent fire
	float distToParent = 0.0f;
	Vec3V vParentPosWld = pFire->GetParentPositionWorld();
	if (IsZeroAll(vParentPosWld)==false)
	{
		distToParent = Dist(pFire->GetParentPositionWorld(), pFire->GetPositionWorld()).Getf();
	}

	// register the fx system
	float clampedDist; 
	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;
	if (pFire->GetEntity())
	{		
		if (pFire->GetEntity()->GetIsTypeVehicle())
		{
			pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_vehicle",0xB54FD790), justCreated);
		}
		else
		{
			pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_object",0x83CC94D0), justCreated);
		}
		clampedDist = 1.0f;
	}
	else if (distToParent<=0.5f)
	{
		pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_petrol_half",0xF9F9E358), justCreated);
		clampedDist = Min(1.0f, distToParent/0.5f);
	}
	else if (distToParent<=1.0f)
	{
		pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_petrol_one",0x206D88DC), justCreated);
		clampedDist = Min(1.0f, distToParent/1.0f);
	}
	else
	{
		pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_petrol_two",0x45708655), justCreated);
		clampedDist = Min(1.0f, distToParent/2.0f);
	}

	if (pFxInst)
	{
		// shrink this to zero as the fire life increases
		float lifeRatio = Min(1.0f, pFire->GetLifeRatio()/FIRE_PETROL_SPREAD_LIFE_RATIO);
		float distEvo = clampedDist * (1.0f - lifeRatio);

		if (pFire->GetEntity())
		{
			if (vfxVerifyf(pFire->GetEntity()->GetIsPhysical(), "petrol trail fire attached to a non-physical entity"))
			{
				CPhysical* pPhysical = static_cast<CPhysical*>(pFire->GetEntity());
				if (pPhysical)
				{
					ptfxAttach::Attach(pFxInst, pFire->GetEntity(), pFire->GetBoneIndex());
					if (justCreated)
					{
						Vec3V vFireOffsetPos = pFire->GetPositionLocal();
						pFxInst->SetOffsetPos(vFireOffsetPos);
					}

					// set the speed evo
					//float speedEvo = CVfxHelper::GetInterpValue(pPhysical->GetVelocity().Mag(), 0.0f, 5.0f);
					//pFxInst->SetEvoValue("speed", distEvo);
				}
			}
		}
		else
		{
			// this isn't an entity fire - set the position directly
			pFxInst->SetBaseMtx(pFire->GetMatrixOffset());
		}

		// set the time of the effect based on the fire strength
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength", 0xFF1C4627), pFire->GetCurrStrength());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("dist", 0xD27AC355), distEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fadein", 0x68CB0025), 1.0f-pFire->GetMaxStrengthRatio());

		// check if the effect has just been created 
		if (justCreated)
		{
#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld())
			{
				pFxInst->SetCanBeCulled(false);
			}
#endif // GTA_REPLAY

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
} 


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxFirePetrolPool
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::UpdatePtFxFirePetrolPool				(CFire* pFire)
{
	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_petrol_pool",0xF8BC1D3B), justCreated);

	if (pFxInst)
	{
		// this isn't an entity fire - set the position directly
		pFxInst->SetBaseMtx(pFire->GetMatrixOffset());

		// set the time of the effect based on the fire strength
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength", 0xFF1C4627), pFire->GetCurrStrength());

		// set a fadein evo
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fadein", 0x68CB0025), 1.0f-pFire->GetMaxStrengthRatio());

		// check if the effect has just been created 
		if (justCreated)
		{
#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld())
			{
				pFxInst->SetCanBeCulled(false);
			}
#endif // GTA_REPLAY

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
} 


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxFireEntity
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::UpdatePtFxFireEntity				(CFire* pFire)
{
	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;

	if (pFire->GetEntity()->GetIsTypeVehicle())
	{
		pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_vehicle",0xB54FD790), justCreated);
	}
	else if (pFire->GetEntity()->GetIsTypeObject())
	{
		pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_object",0x83CC94D0), justCreated);
	}
	else
	{
		vfxAssertf(0, "unsupported entity is on fire");
	}

	if (pFxInst)
	{
		// set position of effect
		ptfxAttach::Attach(pFxInst, pFire->GetEntity(), pFire->GetBoneIndex());
		
		// set the time of the effect based on the fire strength
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength", 0xFF1C4627), pFire->GetCurrStrength());

		// set velocity of entity attached to
		if (pFire->GetEntity()->GetIsPhysical())
		{
			CPhysical* pPhysical = static_cast<CPhysical*>(pFire->GetEntity());
			ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pPhysical->GetVelocity()));
		}

		// check if the effect has just been created 
		if (justCreated)
		{
			// an entity is on fire - point the effect to the matrix to use
			AssertEntityPointerValid(pFire->GetEntity());

#if __ASSERT
			// check what type of entity the fire is attached to
			if (pFire->GetEntity()->GetIsTypeVehicle()==false && pFire->GetEntity()->GetIsTypeObject()==false)
			{
				// unrecognised entity fire - let user know
				vfxAssertf(0, "trying to attach a fire to a non ped");
			}
#endif
			pFxInst->SetOffsetMtx(pFire->GetMatrixOffset());
				
#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld())
			{
				pFxInst->SetCanBeCulled(false);
			}
#endif // GTA_REPLAY

			// it has just been created - start it and set it's position
			pFxInst->Start();

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketFireEntityFx>(CPacketFireEntityFx((size_t)pFire, MAT34V_TO_MATRIX34(pFire->GetMatrixOffset()), pFire->GetCurrStrength(),  pFire->GetBoneIndex()), pFire->GetEntity());
			}
#endif // GTA_REPLAY

		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
		}
#endif
	}
} 


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxFireSteam
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::UpdatePtFxFireSteam						(CFire* pFire)
{
	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_STEAM, false, atHashWithStringNotFinal("fire_extinguish",0xEE8B9E4A), justCreated);

	if (pFxInst)
	{
		// set the time of the effect based on the fire strength
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength", 0xFF1C4627), pFire->GetCurrStrength());

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - set its position
			Vec3V vFirePos = pFire->GetPositionWorld();
			pFxInst->SetBasePos(vFirePos);

#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld())
			{
				pFxInst->SetCanBeCulled(false);
			}
#endif // GTA_REPLAY

			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxFireVehWrecked
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::UpdatePtFxFireVehWrecked			(CFire* pFire)
{
	CVehicle* pVehicle = static_cast<CVehicle*>(pFire->GetEntity());

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;
	
	if (pFire->GetFireType()==FIRETYPE_TIMED_VEH_WRECKED)
	{
		pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, pVfxVehicleInfo->GetWreckedFirePtFxName(), justCreated);
	}
	else if (pFire->GetFireType()==FIRETYPE_TIMED_VEH_WRECKED_2)
	{
		pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, pVfxVehicleInfo->GetWreckedFire2PtFxName(), justCreated);
	}
	else if (pFire->GetFireType()==FIRETYPE_TIMED_VEH_WRECKED_3)
	{
		pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES, false, pVfxVehicleInfo->GetWreckedFire3PtFxName(), justCreated);
	}
		
	if (pFxInst)
	{
		// set up the evolution variables
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fade", 0x6E9C2BB6), pFire->GetCurrStrength());

		ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pVehicle->GetVelocity()));

		ptfxAttach::Attach(pFxInst, pVehicle, pFire->GetBoneIndex());

		// check if the effect has just been created 
		if (justCreated)
		{
			Vec3V vFireOffsetPos = pFire->GetPositionLocal();
			pFxInst->SetOffsetPos(vFireOffsetPos);

#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld())
			{
				pFxInst->SetCanBeCulled(false);
			}
#endif // GTA_REPLAY

			pFxInst->Start();
		}

		pFxInst->SetCameraBias(VFX_FIRE_WRECKED_VEH_PTFX_CAMERA_BIAS);

#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
		}
#endif
	}
} 


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxFireVehWheel
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::UpdatePtFxFireVehWheel			(CFire* pFire)
{
	CVehicle* pVehicle = static_cast<CVehicle*>(pFire->GetEntity());
	CWheel* pWheel = static_cast<CWheel*>(pFire->GetRegOwner());

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

 	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetWheelFirePtFxRangeSqr())
 	{
 		return;
 	}

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pFire->GetRegOwner(), (int)pFire->GetRegOffset(), false, pVfxVehicleInfo->GetWheelFirePtFxName(), justCreated);
	if (pFxInst)
	{
		ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pVehicle->GetVelocity()));

		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetWheelFirePtFxSpeedEvoMin(), pVfxVehicleInfo->GetWheelFirePtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("timer", 0x7E909EC5), pFire->GetCurrStrength());

		ptfxAttach::Attach(pFxInst, pVehicle, pFire->GetBoneIndex());

		if (justCreated)
		{
			pFxInst->SetOffsetPos(pFire->GetMatrixOffset().GetCol3());

			pFxInst->SetUserZoom(pWheel->GetWheelRadius());

#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld())
			{
				pFxInst->SetCanBeCulled(false);
			}
#endif // GTA_REPLAY

			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxFirePetrolTank
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::UpdatePtFxFirePetrolTank			(CFire* pFire)
{
	CVehicle* pVehicle = static_cast<CVehicle*>(pFire->GetEntity());
	vfxAssertf(pVehicle, "Cannot get entity from fire");

	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetPetrolTankFirePtFxRangeSqr())
	{
		return;
	}

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pFire->GetRegOwner(), (int)pFire->GetRegOffset(), true, pVfxVehicleInfo->GetPetrolTankFirePtFxName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pVehicle->GetVelocity()));

		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetPetrolTankFirePtFxSpeedEvoMin(), pVfxVehicleInfo->GetPetrolTankFirePtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// set position
		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		// check if the effect has just been created 
		if (justCreated)
		{	
			// get the petrol tank position relative to the vehicle matrix
			// if this is the right side effect then flip the x offset and axis
			if (pFire->GetRegOffset()>PTFXOFFSET_VEHICLE_FIRE_PETROL_TANK)
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}

//			pFxInst->SetOffset(vPetrolTankOffset);
			pFxInst->SetOffsetPos(pFire->GetMatrixOffset().GetCol3());

#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld())
			{
				pFxInst->SetCanBeCulled(false);
			}
#endif // GTA_REPLAY

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
		}
#endif

		// MN - this should be moved to the main fire class so it works like all other fires
//		static const audStringHash petrolTankFireSound("VEHICLE_PETROL_TANK_FIRE");
//		fwUniqueObjId effectUniqueID = fwIdKeyGenerator::Get(pFire->GetRegOwner(), pFire->GetRegOffset());
//		g_AmbientAudioEntity.RegisterEffectAudio(petrolTankFireSound, effectUniqueID, RCC_VECTOR3(vPetrolTankPos), pVehicle);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxFirePedBone
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::UpdatePtFxFirePedBone				(CFire* pFire, s32 id, s32 boneTag1, s32 boneTag2, atHashWithStringNotFinal ptFxHashName, float speedEvoMin, float speedEvoMax)
{
	AssertEntityPointerValid(pFire->GetEntity());

	CPed* pPed = static_cast<CPed*>(pFire->GetEntity());	
	vfxAssertf(pPed, "cannot cast entity to ped");
	s32 boneIndexA = pPed->GetBoneIndexFromBoneTag((eAnimBoneTag)boneTag1);
	s32 boneIndexB = pPed->GetBoneIndexFromBoneTag((eAnimBoneTag)boneTag2);

	if (ptxVerifyf(boneIndexA!=BONETAG_INVALID, "invalid bone index generated from bone tag (A) (%s - %d)", pPed->GetDebugName(), boneTag1))
	{
		if (ptxVerifyf(boneIndexB!=BONETAG_INVALID, "invalid bone index generated from bone tag (B) (%s - %d)", pPed->GetDebugName(), boneTag1))
		{
			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pFire, PTFXOFFSET_FIRE_FLAMES+id, false, ptFxHashName, justCreated);
			if (pFxInst)
			{
				// set position of effect
				Mat34V boneMtxA, boneMtxB;
				CVfxHelper::GetMatrixFromBoneIndex(boneMtxA, pPed, boneIndexA);
				CVfxHelper::GetMatrixFromBoneIndex(boneMtxB, pPed, boneIndexB);

				Vec3V vDir = boneMtxB.GetCol3() - boneMtxA.GetCol3();
				Vec3V vPos = boneMtxA.GetCol3() + (vDir*ScalarV(V_HALF));
				vDir = Normalize(vDir);

				Mat34V fxMat;
				CVfxHelper::CreateMatFromVecYAlign(fxMat, vPos, vDir, Vec3V(V_Z_AXIS_WZERO));

				// make the fx matrix relative to the bone 1
				Mat34V relFxMat;
				CVfxHelper::CreateRelativeMat(relFxMat, boneMtxA, fxMat);

				pFxInst->SetOffsetMtx(relFxMat);	

				// set up the evolution variables
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength", 0xFF1C4627), pFire->GetCurrStrength());

				float speedEvo = CVfxHelper::GetInterpValue(pPed->GetVelocity().Mag(), speedEvoMin, speedEvoMax);
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

				ptfxAttach::Attach(pFxInst, pFire->GetEntity(), boneIndexA);

				// check if the effect has just been created 
				if (justCreated)
				{
#if GTA_REPLAY
					if(CReplayMgr::IsReplayInControlOfWorld())
					{
						pFxInst->SetCanBeCulled(false);
					}
#endif // GTA_REPLAY

					// it has just been created - start it and set it's position
					pFxInst->Start();
				}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
				else
				{
					// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
					vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
				}
#endif
			}
		}
	}
} 


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxPedFireSmoulder
///////////////////////////////////////////////////////////////////////////////

void			CVfxFire::TriggerPtFxPedFireSmoulder					(CPed* pPed)
{
	// don't do this for player peds
	if (pPed->IsPlayer())
	{
		return;
	}

	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if smoulder ptfx aren't enabled
	if (pVfxPedInfo->GetSmoulderPtFxEnabled()==false)
	{
		return;
	}

	eAnimBoneTag boneTagA = pVfxPedInfo->GetSmoulderPtFxBoneTagA();
	eAnimBoneTag boneTagB = pVfxPedInfo->GetSmoulderPtFxBoneTagB();

	const s32 boneIndexA = pPed->GetBoneIndexFromBoneTag(boneTagA);
	const s32 boneIndexB = pPed->GetBoneIndexFromBoneTag(boneTagB);

	if (ptxVerifyf(boneIndexA!=BONETAG_INVALID, "invalid bone index generated from bone tag (A) (%s - %d)", pPed->GetDebugName(), boneTagA))
	{
		if (ptxVerifyf(boneIndexB!=BONETAG_INVALID, "invalid bone index generated from bone tag (B) (%s - %d)", pPed->GetDebugName(), boneTagB))
		{
			// check that there is a camera in range
			Vec3V vFxPos = pPed->GetTransform().GetPosition();
			if (CVfxHelper::GetDistSqrToCamera(vFxPos)<pVfxPedInfo->GetSmoulderPtFxRangeSqr())
			{
				// register the fx system
				ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxPedInfo->GetSmoulderPtFxName());
				if (pFxInst)
				{
					// set position of effect
					Mat34V boneMtxA, boneMtxB;
					CVfxHelper::GetMatrixFromBoneIndex(boneMtxA, pPed, boneIndexA);
					CVfxHelper::GetMatrixFromBoneIndex(boneMtxB, pPed, boneIndexB);

					Vec3V vDir = boneMtxB.GetCol3() - boneMtxA.GetCol3();
					Vec3V vPos = boneMtxA.GetCol3() + (vDir*ScalarV(V_HALF));
					vDir = Normalize(vDir);

					Mat34V fxMat;
					CVfxHelper::CreateMatFromVecYAlign(fxMat, vPos, vDir, Vec3V(V_Z_AXIS_WZERO));

					// make the fx matrix relative to the bone 1
					Mat34V relFxMat;
					CVfxHelper::CreateRelativeMat(relFxMat, boneMtxA, fxMat);

					ptfxAttach::Attach(pFxInst, pPed, boneIndexA);

					pFxInst->SetOffsetMtx(relFxMat);

#if GTA_REPLAY
					if(CReplayMgr::IsReplayInControlOfWorld())
					{
						pFxInst->SetCanBeCulled(false);
					}
#endif // GTA_REPLAY

					pFxInst->Start();
				}
			}	
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxFire::LoadData					()
{
	// initialise the data
	for (s32 i=0; i<NUM_VFX_GROUPS; i++)
	{
		m_vfxFireInfos[i].ptFxMapFireHashName = (u32)0;
	}

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::FIREFX_FILE);
	while (DATAFILEMGR.IsValid(pData))
	{
		// read in the data
		fiStream* stream = ASSET.Open(pData->m_filename, "dat", true);
		vfxAssertf(stream, "Cannot open data file");

		if (stream)
		{
#if CHECK_VFXGROUP_DATA
			bool vfxGroupVerify[NUM_VFX_GROUPS];

			for (s32 i=0; i<NUM_VFX_GROUPS; i++)
			{
				vfxGroupVerify[i] = false;
			}
#endif
			// initialise the tokens
			fiAsciiTokenizer token;
			token.Init("fireFx", stream);

			// read in the data
			char charBuff[128];
			s32 phase = -1;	

			// read in the version
			m_version = token.GetFloat();

			while (1)
			{
				token.GetToken(charBuff, 128);

				// check for commented lines
				if (charBuff[0]=='#')
				{
					token.SkipToEndOfLine();
					continue;
				}

				// check for change of phase
				else if (stricmp(charBuff, "FIREFX_INFO_START")==0)
				{
					phase = 0;
					continue;
				}
				else if (stricmp(charBuff, "FIREFX_INFO_END")==0)
				{
					break;
				}

				if (phase==0)
				{
					VfxGroup_e vfxGroup = phMaterialGta::FindVfxGroup(charBuff);
					if (vfxGroup==VFXGROUP_UNDEFINED)
					{
#if CHECK_VFXGROUP_DATA
						vfxAssertf(0, "Invalid vfx group found in firefx.dat %s", charBuff);
#endif
						token.SkipToEndOfLine();
					}	
					else
					{
#if CHECK_VFXGROUP_DATA
						vfxAssertf(vfxGroupVerify[vfxGroup]==false, "Duplicate vfxgroup data found in firefx.dat for %s", charBuff);
						vfxGroupVerify[vfxGroup]=true;
#endif

						// fire settings
						m_vfxFireInfos[vfxGroup].flammability = token.GetFloat();
						m_vfxFireInfos[vfxGroup].burnTime = token.GetFloat();
						m_vfxFireInfos[vfxGroup].burnStrength = token.GetFloat();

						// map fire fx system
						token.GetToken(charBuff, 128);		
						if (stricmp(charBuff, "-")==0)
						{	
							m_vfxFireInfos[vfxGroup].ptFxMapFireHashName = (u32)0;					
						}
						else
						{
							m_vfxFireInfos[vfxGroup].ptFxMapFireHashName = atHashWithStringNotFinal(charBuff);
						}
					}
				}
			}

			stream->Close();

#if CHECK_VFXGROUP_DATA
			for (s32 i=0; i<NUM_VFX_GROUPS; i++)
			{
				vfxAssertf(vfxGroupVerify[i]==true, "Missing vfxgroup data found in firefx.dat for %s", g_fxGroupsList[i]);
			}
#endif
		}

		// get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
	}
}






