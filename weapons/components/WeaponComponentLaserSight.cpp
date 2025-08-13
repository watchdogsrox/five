//
// weapons/weaponcomponentlasersight.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponentLaserSight.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Weapons/WeaponManager.h"
#include "Weapons/Helpers/WeaponHelpers.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentLaserSightInfo::CWeaponComponentLaserSightInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentLaserSightInfo::~CWeaponComponentLaserSightInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentLaserSight::CWeaponComponentLaserSight()
: m_iLaserSightBoneIdx(-1)
, m_fOffset(0.0f)
, m_fOffsetTarget(PI)
, m_pShapeTest(NULL)
, m_pvOldProbeEnd(NULL)
, m_bHaveOldProbeResults(false)
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentLaserSight::CWeaponComponentLaserSight(const CWeaponComponentLaserSightInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable)
: superclass(pInfo, pWeapon, pDrawable)
, m_iLaserSightBoneIdx(-1)
, m_fOffset(0.0f)
, m_fOffsetTarget(PI)
, m_pShapeTest(NULL)
, m_pvOldProbeEnd(NULL)
, m_bHaveOldProbeResults(false)
{
	if(GetDrawable())
	{
		m_iLaserSightBoneIdx = GetInfo()->GetLaserSightBone().GetBoneIndex(GetDrawable()->GetModelIndex());
		if(!weaponVerifyf(m_iLaserSightBoneIdx == -1 || m_iLaserSightBoneIdx < (s32)GetDrawable()->GetSkeleton()->GetBoneCount(), "Invalid m_iLaserSightBoneIdx bone index %d. Should be between 0 and %d", m_iLaserSightBoneIdx, GetDrawable()->GetSkeleton()->GetBoneCount()-1))
			m_iLaserSightBoneIdx = -1;
	}

	m_pShapeTest = rage_new WorldProbe::CShapeTestSingleResult;

	m_pvOldProbeEnd = rage_new Vector3;
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentLaserSight::~CWeaponComponentLaserSight()
{
	delete m_pvOldProbeEnd;
	m_pvOldProbeEnd = NULL;

	delete m_pShapeTest;
	m_pShapeTest = NULL;
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentLaserSight::Process(CEntity* pFiringEntity)
{
	if(!pFiringEntity || !pFiringEntity->GetIsTypePed() || !static_cast<CPed*>(pFiringEntity)->IsPlayer())
	{
		static dev_float APPROACH_RATE = 0.6f;
		if(Approach(m_fOffset, m_fOffsetTarget, APPROACH_RATE, fwTimer::GetTimeStep()))
		{
			m_fOffsetTarget = -m_fOffsetTarget;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentLaserSight::ProcessPostPreRender(CEntity* pFiringEntity)
{
	if(CWeaponManager::ShouldRenderLaserSight() && GetWeapon())
	{
		if(GetDrawable())
		{
			if(m_iLaserSightBoneIdx != -1)
			{
				// Get the bone matrix for the laser sight
				Matrix34 laserSightMatrix;
				GetDrawable()->GetGlobalMtx(m_iLaserSightBoneIdx, laserSightMatrix);

				// Get the weapon range
				f32 fRange = GetWeapon()->GetRange();

				// Determine the firing vector
				Vector3 vStart(laserSightMatrix.d);
				Vector3 vEnd(laserSightMatrix.d + laserSightMatrix.a * fRange);
				
				// If the firer is a ped, look up the task firing vector
				bool isAiming = false;

				// async for all but the player
				bool bAsync = true;
				if(pFiringEntity && pFiringEntity->GetIsTypePed())
				{
					CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);

					// always sync for local players
					bAsync = !pFiringPed->IsLocalPlayer();

					// Do not render if reset flag is set
					if( pFiringPed->GetPedResetFlag( CPED_RESET_FLAG_DisableWeaponLaserSight ) )
						return;

					CTaskAimGun* pAimGunTask = static_cast<CTaskAimGun*>(pFiringPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
					if(!pAimGunTask)
					{
						pAimGunTask = static_cast<CTaskAimGun*>(pFiringPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
					}

					if(!pAimGunTask)
					{
						pAimGunTask = static_cast<CTaskAimGun*>(pFiringPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_BLIND_FIRE));
					}

					if(pAimGunTask)
					{
						pAimGunTask->CalculateFiringVector(pFiringPed, vStart, vEnd);

						if (pAimGunTask->GetIsReadyToFire() && !pAimGunTask->GetIsWeaponBlocked())
						{
							isAiming = true;
						}
					}
				}

				if(isAiming)
				{
					if(bAsync) 
					{
						Vector3 vProbeEnd;
						if(GetEndPoint(vProbeEnd))
						{
							//DrawLaserSight(laserSightMatrix, vProbeEnd);
						}
					}
					// order here matters as we wipe out the probe results before we submit; 
					// async needs to read results before they are wiped; 
					// its possible that our async hasn't yet finished, in which case we'll invalidate the probe before launching another
					SubmitProbe(pFiringEntity, vStart, vEnd, bAsync);
					if(!bAsync)
					{
						GetEndPoint(vEnd);
						//DrawLaserSight(laserSightMatrix, vEnd);
						m_bHaveOldProbeResults = false;
					}
				}
				else
				{
					m_bHaveOldProbeResults = false;
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponComponentLaserSight::GetEndPoint(Vector3& oEnd)
{
	bool hasEndPoint = false;

	Assertf(m_pShapeTest != NULL, "NULL Shapetest in CWeaponComponentLaserSight::GetEndPoint");

	if(m_pShapeTest->GetResultsReady())
	{
		if( m_pShapeTest->GetNumHits() > 0u )
		{
			oEnd = (*m_pShapeTest)[0].GetHitPosition();
			*m_pvOldProbeEnd = oEnd;
			m_bHaveOldProbeResults = true;

			m_pShapeTest->Reset();
			hasEndPoint = true;
		}
	}
	else if(m_bHaveOldProbeResults)
	{
		// use previous probe results
		oEnd = *m_pvOldProbeEnd;
		hasEndPoint = true;
	}

	return hasEndPoint;
}

////////////////////////////////////////////////////////////////////////////////
/*
void CWeaponComponentLaserSight::DrawLaserSight(Matrix34& inLaserSightMatrix,Vector3& inEnd)
{
	Vector3 vLaserDirection = inEnd - inLaserSightMatrix.d;
	vLaserDirection.NormalizeSafe();

	static dev_float ANGLE_TOLERANCE = 0.99625f;
	float fAngle = inLaserSightMatrix.a.Dot(vLaserDirection);
	if(fAngle > ANGLE_TOLERANCE)
	{
		// Render laser sight fx
		Vec3V vDir(inEnd - inLaserSightMatrix.d);
		Vec3V vZero = Vec3V(V_ZERO);
		vDir = NormalizeSafe(vDir, vZero);
		ScalarV vError = ScalarVFromF32(0.05f);
		if(IsCloseAll(vDir, vZero, vError) == false)
		{
			g_decalMan.RegisterLaserSight(RCC_VEC3V(inEnd), vDir);
		}

		g_vfxWeapon.UpdatePtFxLaserSight(GetWeapon(), RCC_VEC3V(inLaserSightMatrix.d), RCC_VEC3V(inEnd));

		// Render corona
		g_coronas.Register(RCC_VEC3V(inLaserSightMatrix.d), 
			GetInfo()->GetCoronaSize(), 
			Color_red, 
			GetInfo()->GetCoronaIntensity(),
			0.0f, 
			RCC_VEC3V(inLaserSightMatrix.a),
			1.0f,
			CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
		    CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
			CORONA_DONT_REFLECT | CORONA_HAS_DIRECTION);
	}
}						
*/
////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentLaserSight::SubmitProbe(CEntity* pFiringEntity, Vector3& inStart, Vector3& inEnd, bool inAsync)
{
	const s32 EXCLUSION_MAX = 2 + CWeaponComponentPoint::MAX_WEAPON_COMPONENTS;
	const CEntity* exclusionList[EXCLUSION_MAX];
	s32 iExclusions = 0;

	if(GetWeapon()->GetEntity())
	{
		exclusionList[iExclusions++] = GetWeapon()->GetEntity();
		const CWeapon::Components& components = GetWeapon()->GetComponents();
		for(s32 i = 0; i < components.GetCount(); i++)
		{
			exclusionList[iExclusions++] = components[i]->GetDrawable();
		}
	}

	if(pFiringEntity)
	{
		exclusionList[iExclusions++] = pFiringEntity;
	}

	if(!pFiringEntity || !pFiringEntity->GetIsTypePed() || !static_cast<CPed*>(pFiringEntity)->IsPlayer())
	{
		static const f32 ONE_OVER_PI = 1.f/PI;
		Vector3 vOffset(m_fOffset*ONE_OVER_PI, 0.f, Sinf(m_fOffset) * (m_fOffsetTarget > 0.f ? 1.f : -1.f));

		static dev_float SCALE = 0.25f;
		vOffset *= SCALE;
		if(pFiringEntity)
		{
			MAT34V_TO_MATRIX34(pFiringEntity->GetMatrix()).Transform3x3(vOffset);
		}

		// Apply the offset
		inEnd += vOffset;
	}

	//Create a new probe
	Assertf(inAsync || !m_pShapeTest->GetResultsWaitingOrReady(), "CWeaponComponentLaserSight::ProcessPostPreRender already has a valid shape test handle in AddProbe");

	m_pShapeTest->Reset();

	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(m_pShapeTest);
	//@TODO there isn't async support for Tyre tests!
	if(!inAsync)
	{
		probeDesc.SetOptions(WorldProbe::LOS_TEST_VEH_TYRES);
	}
	probeDesc.SetStartAndEnd(inStart, inEnd);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_WEAPON_TYPES);
	probeDesc.SetExcludeEntities(exclusionList, iExclusions);
	probeDesc.SetContext(WorldProbe::LOS_Weapon);

	// Determine if we've hit anything
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, inAsync ? WorldProbe::PERFORM_ASYNCHRONOUS_TEST : WorldProbe::PERFORM_SYNCHRONOUS_TEST);
}

////////////////////////////////////////////////////////////////////////////////
