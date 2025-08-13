#include "ExplosionInst.h"

// Rage headers
#include "crskeleton/skeleton.h"

// Framework headers
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/vfxutil.h"

// Game headers
#include "camera/CamInterface.h"
#include "Control/GameLogic.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Players/NetworkPlayerMgr.h"
#include "Peds/Ped.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Renderer/Water.h"
#include "Vehicles/Vehicle.h"
#include "Vfx/VfxHelper.h"


FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(phGtaExplosionInst, CExplosionManager::MAX_EXPLOSIONS, 0.63f, atHashString("ExplosionType",0xe4379a4c), sizeof(phGtaExplosionInst));

NETWORK_OPTIMISATIONS()
WEAPON_OPTIMISATIONS()

// Occlusion Cache
#if __BANK
bool CExplosionOcclusionCache::sm_EnableOcclusionCache = true;
bool CExplosionOcclusionCache::sm_EnableLifeTimeOverride = false;
u32 CExplosionOcclusionCache::sm_LifeTimeOverride = 1;
#endif // __BANK
CExplosionOcclusionCache::COcclusionResult* CExplosionOcclusionCache::FindOcclusionResult(rage::phHandle handle)
{
#if __BANK
	if(!sm_EnableOcclusionCache)
	{
		return NULL;
	}
#endif // __BANK
	for(int i = 0; i < m_Cache.GetCount(); ++i)
	{
		if(m_Cache[i].GetHandle() == handle)
		{
			return &m_Cache[i];
		}
	}
	return NULL;
}

CExplosionOcclusionCache::COcclusionResult::Status CExplosionOcclusionCache::FindOcclusionStatus(rage::phHandle handle)
{
	if(COcclusionResult* pOcclusionResult = FindOcclusionResult(handle))
	{
		return pOcclusionResult->GetStatus();
	}
	else
	{
		return COcclusionResult::OUTDATED;
	}		
}

void CExplosionOcclusionCache::UpdateOcclusionStatus(rage::phHandle handle, COcclusionResult::Status status, u32 lifeTime)
{
	if(COcclusionResult* pOcclusionResult = FindOcclusionResult(handle))
	{
		pOcclusionResult->SetStatus(status,lifeTime);
	}
	else if(!m_Cache.IsFull())
	{
		COcclusionResult newOcclusionResult(handle);
		newOcclusionResult.SetStatus(status,lifeTime);
		m_Cache.Push(newOcclusionResult);
	}
}

u32 CExplosionOcclusionCache::ComputeOcclusionResultLifeTime(const CEntity* pEntity, COcclusionResult::Status status)
{
	Assert(status != COcclusionResult::OUTDATED);
#if __BANK
	if(sm_EnableLifeTimeOverride)
	{
		return sm_LifeTimeOverride;
	}
#endif // __BANK

	if(status == COcclusionResult::NOT_OCCLUDED)
	{
		// It isn't noticeable if something is getting hit by an explosion and then stops since it should be flying through the air
		return 20;
	}
	else // OCCLUDED
	{
		// It will look bad if something goes past a doorway as an explosion goes off and doesn't get hit.
		if(pEntity->GetIsTypePed())
		{
			// Be extra wary when occluding peds
			return 5;
		}
		else
		{
			return 10;
		}
	}
}

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------

phGtaExplosionInst::phGtaExplosionInst(eExplosionTag ExplosionTag, CEntity *pExplosionOwner, CEntity *pExplodingEntity, CEntity *pIgnoreDamageEntity, CEntity* pAttachEntity, s32 attachBoneTag, const Vector3& vPosWld, const Vector3& vDirWld, float sizeScale, u32 camShakeNameHash, float camShake, float camShakeRollOffScaling, bool makesSound, bool noVfx, bool inAir, u32 vfxTagHash, const CNetFXIdentifier& networkIdentifier, bool bNoDamage, bool bAttachedToVehicle, bool bDetonatingOtherPlayersExplosive, u32 weaponHash, bool bDisableDamagingOwner, fwInteriorLocation intLoc)
:	m_explosionTag(ExplosionTag),
	m_originalExplosionTag(ExplosionTag),
	m_pExplodingEntity(pExplodingEntity),
	m_pExplosionOwner(pExplosionOwner),
	m_pIgnoreDamageEntity(pIgnoreDamageEntity),
	m_pAttachEntity(NULL),
	m_attachBoneIndex(-1),
	m_vPosWld(vPosWld),
	m_vDirWld(vDirWld),
	m_sizeScale(sizeScale),
	m_camShakeNameHash(camShakeNameHash),
	m_fCamShake(camShake),
	m_fCamShakeRollOffScaling(camShakeRollOffScaling),
	m_underwaterDepth(0.0f),
	m_underwaterType(WATERTEST_TYPE_RIVER),
	m_bMakesSound(makesSound),
	m_bNoVfx(noVfx),
	m_bInAir(inAir),
	m_vfxTagHash(vfxTagHash),
	m_networkIdentifier(networkIdentifier),
	m_bNoDamage(bNoDamage),
	m_bAttachedToVehicle(bAttachedToVehicle),
	m_bDetonatingOtherPlayersExplosive(bDetonatingOtherPlayersExplosive),
	m_bNeedsRemoved(false),
	m_weaponHash(weaponHash),
	m_bDisableDamagingOwner(bDisableDamagingOwner),
	m_bReportCrimes(true),
	m_interiorLocation(intLoc),
	m_bIsAttachEntityValid(false)
{
	m_SpawnTimeMS = fwTimer::GetTimeInMilliseconds();

	for( int i = 0; i < MAX_ENTITIES_TO_APPLY_DAMAGE; i++ )
	{
		m_pAlreadyDamagedEntities[i] = NULL;
	}

	Assert(Mag(m_vDirWld).Getf()>=0.997f && Mag(m_vDirWld).Getf()<=1.003f);

	// calculate the local position and direction 
	if (pAttachEntity)
	{
		// we're attached - get the attachment matrix 
		Mat34V vAttachMtx;
		s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(vAttachMtx, pAttachEntity, (eAnimBoneTag)attachBoneTag);

		// check that the matrix is valid
		if (IsZeroAll(vAttachMtx.GetCol0())==false)
		{
			// it is - set up the attachment
			m_pAttachEntity = pAttachEntity;
			m_attachBoneIndex = boneIndex;
			m_bIsAttachEntityValid = true;
			
			// calculate the local values
			Mat34V vInverseAttachMtx;
			InvertTransformOrtho(vInverseAttachMtx, vAttachMtx);
			m_vPosLcl = Transform(vInverseAttachMtx, m_vPosWld);
			m_vDirLcl = Transform3x3(vInverseAttachMtx, m_vDirWld);

			// if the bone index isn't valid the CVfxHelper::GetMatrixFromBoneTag function will return non ortho normalised transform
			// which will meant that the direction might also not be normalised so normalise it
			if( boneIndex == -1 )
			{
				m_vDirLcl = Normalize( m_vDirLcl );
			}
			
			Assertf(Mag(m_vDirLcl).Getf()>=0.997f && Mag(m_vDirLcl).Getf()<=1.003f, "m_vDirLcl: %f, %f, %f | m_vPosLcl: %f, %f, %f | m_vDirWld: %f, %f, %f | m_vPosWld: %f, %f, %f | vAttachMtx.GetCol0(): %f, %f, %f | vInverseAttachMtx.GetCol0(): %f, %f, %f | boneIndex: %i | attachBoneTag: %i | pAttachEntity: %s",
				m_vDirLcl.GetXf(), m_vDirLcl.GetYf(), m_vDirLcl.GetZf(),
				m_vPosLcl.GetXf(), m_vPosLcl.GetYf(), m_vPosLcl.GetZf(),
				m_vDirWld.GetXf(), m_vDirWld.GetYf(), m_vDirWld.GetZf(),
				m_vPosWld.GetXf(), m_vPosWld.GetYf(), m_vPosWld.GetZf(),
				vAttachMtx.GetCol0().GetXf(), vAttachMtx.GetCol0().GetYf(), vAttachMtx.GetCol0().GetZf(),
				vInverseAttachMtx.GetCol0().GetXf(), vInverseAttachMtx.GetCol0().GetYf(), vInverseAttachMtx.GetCol0().GetZf(),
				boneIndex,
				attachBoneTag,
				pAttachEntity->GetDebugName()
				);
		}
	}
	else
	{
		// we're not attached - the local and world values are the same
		m_vPosLcl = m_vPosWld;
		m_vDirLcl = m_vDirWld;
	}

	//Because of audio thread issues, we'll handle this in audExplosionAudioEntity::Update() right before we play the sound
	//m_AudioEntity.Init();

	m_underwaterType = CVfxHelper::GetWaterDepth(m_vPosWld, m_underwaterDepth);

	//Clamp injected size scale, and 
	m_sizeScale = Clamp(m_sizeScale, 0.01f, 1.0f);

	//Apply weapon Area of Effect modifier
	if (const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_weaponHash))
	{
		m_sizeScale *= pWeaponInfo->GetAoEModifier();
	}
}


//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------

phGtaExplosionInst::~phGtaExplosionInst()
{
	if( m_pExplosionOwner )
	{
		m_pExplosionOwner = NULL;
	}	
	if( m_pExplodingEntity )
	{
		m_pExplodingEntity = NULL;
	}
	if( m_pIgnoreDamageEntity )
	{
		m_pIgnoreDamageEntity = NULL;
	}
	for( int i = 0; i < MAX_ENTITIES_TO_APPLY_DAMAGE; i++ )
	{
		if( m_pAlreadyDamagedEntities[i] )
		{
			m_pAlreadyDamagedEntities[i] = NULL;
		}
	}
	if( m_pAttachEntity )
	{
		m_pAttachEntity = NULL;
	}

	ptfxReg::UnRegister(this, false);
	g_audExplosionEntity.RemoveExplosion(this);
}


//-------------------------------------------------------------------------
// Update
//-------------------------------------------------------------------------

void phGtaExplosionInst::Update()
{
	// update any attached explosions
	if (m_pAttachEntity)
	{
		// update the attachment matrix
		Mat34V vAttachMtx;
		CVfxHelper::GetMatrixFromBoneIndex(vAttachMtx, m_pAttachEntity, m_attachBoneIndex);

		// check for a zero attach matrix
		if (IsZeroAll(vAttachMtx.GetCol0()))
		{
			// try to fix up the pointers
			bool updatedEntityPtr = false;
			for (s32 i=0; i<vfxUtil::GetNumBrokenFrags(); i++)
			{
				vfxBrokenFragInfo& brokenFragInfo = vfxUtil::GetBrokenFragInfo(i);

				if (brokenFragInfo.pParentEntity==m_pAttachEntity.Get() && brokenFragInfo.pChildEntity!=NULL)
				{
					// replace with the child
					CEntity* pChildEntity = static_cast<CEntity*>(static_cast<fwEntity*>(brokenFragInfo.pChildEntity));
					CVfxHelper::GetMatrixFromBoneIndex(vAttachMtx, pChildEntity, (eAnimBoneTag)m_attachBoneIndex);

					// return if we don't have a valid matrix
					if (IsZeroAll(vAttachMtx.GetCol0()))
					{
						continue;
					}

					m_pAttachEntity = pChildEntity;

					updatedEntityPtr = true;
				}
			}

			if (!updatedEntityPtr)
			{
				// can't find a replacement entity - we need to remove this explosion
				m_bNeedsRemoved = true;
			}
		}

		// check for deleted drawable
		if (m_pAttachEntity->GetDrawable()==NULL)
		{
			// the attached drawable is missing - we need to remove this explosion
			m_bNeedsRemoved = true;
		}

		// update the world position and direction
		m_vPosWld = Transform(vAttachMtx, m_vPosLcl);
		m_vDirWld = Transform3x3(vAttachMtx, m_vDirLcl);
	}
	else if (m_bIsAttachEntityValid)
	{
		// the attach entity has been deleted - remove the explosion
		m_bNeedsRemoved = true;
	}

	m_underwaterType = CVfxHelper::GetWaterDepth(m_vPosWld, m_underwaterDepth);
}


//-------------------------------------------------------------------------
// GetPhysicalType
//-------------------------------------------------------------------------

ePhysicalType phGtaExplosionInst::GetPhysicalType()
{
	eExplosionTag expTag = GetExplosionTag();
	if (CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).directedWidth>0.0f)
	{
		return PT_DIRECTED;
	}
	else
	{
		return PT_SPHERE;
	}
}


//-------------------------------------------------------------------------
// GetWeaponGroup
//-------------------------------------------------------------------------

eWeaponEffectGroup phGtaExplosionInst::GetWeaponGroup()
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_weaponHash);
	if(pWeaponInfo)
	{
		return pWeaponInfo->GetEffectGroup();
	}

	return WEAPON_EFFECT_GROUP_EXPLOSION;
}


//-------------------------------------------------------------------------
// GetDamageType
//-------------------------------------------------------------------------

eDamageType phGtaExplosionInst::GetDamageType()
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_weaponHash);
	if(pWeaponInfo)
	{
		return pWeaponInfo->GetDamageType();
	}

	return DAMAGE_TYPE_NONE;
}


//--------------------------------------------------------------------------
// ShouldApplyContinuousDamage
//--------------------------------------------------------------------------
bool phGtaExplosionInst::ShouldApplyContinuousDamage()
{
	return CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(GetExplosionTag()).bAppliesContinuousDamage;
}


//--------------------------------------------------------------------------
// ShouldPostProcessCollisionsWithNoForce
//--------------------------------------------------------------------------
bool phGtaExplosionInst::ShouldPostProcessCollisionsWithNoForce()
{
	return CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(GetExplosionTag()).bPostProcessCollisionsWithNoForce;
}


//--------------------------------------------------------------------------
// ShouldDamageVehicles
//--------------------------------------------------------------------------
bool phGtaExplosionInst::ShouldDamageVehicles()
{
	return CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(GetExplosionTag()).bDamageVehicles;
}


//--------------------------------------------------------------------------
// ShouldDamageObjects
//--------------------------------------------------------------------------
bool phGtaExplosionInst::ShouldDamageObjects()
{
	return CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(GetExplosionTag()).bDamageObjects;
}

//--------------------------------------------------------------------------
// ShouldEnforceDamagedEntityList
//--------------------------------------------------------------------------
bool phGtaExplosionInst::ShouldEnforceDamagedEntityList()
{
	return CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(GetExplosionTag()).bEnforceDamagedEntityList;
}

//--------------------------------------------------------------------------
// CalculateExplosionDamage
//--------------------------------------------------------------------------
float phGtaExplosionInst::CalculateExplosionDamage(Vec3V_In vHitPosition, bool bIsTypePed)
{
	const CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(GetExplosionTag());
	
	float fDamageAtCentre = expTagData.damageAtCentre;
	float fDamageAtEdge = expTagData.bUseDistanceDamageCalc ? expTagData.damageAtEdge : expTagData.damageAtCentre;

	Vec3V vExpPosWld = GetPosWld();

	// B*1895048: If explosion Z is slightly above or below hit Z for peds (origin), flatten distance check for a more accurate result
	Vec3V vHitPositionAdjusted = vHitPosition;
	if (expTagData.bUseDistanceDamageCalc && bIsTypePed)
	{
		float fExpZ = vExpPosWld.GetZf();
		float fHitZ = vHitPosition.GetZf();
		TUNE_GROUP_FLOAT(EXPLOSIVE_TUNE, FLATTEN_DISTANCE_CHECK_PED_HEIGHT, 1.3f, 0.0f, 2.0f, 0.01f);
		if (fExpZ >= (fHitZ - FLATTEN_DISTANCE_CHECK_PED_HEIGHT) && fExpZ <= (fHitZ + FLATTEN_DISTANCE_CHECK_PED_HEIGHT))
		{
			vHitPositionAdjusted.SetZ(fExpZ);
		}
	}

	float fDistToCentre = Dist(vExpPosWld, vHitPositionAdjusted).Getf();
	float fScaledMidRadius = expTagData.midRadius * m_sizeScale;
	float fScaledEndRadius = expTagData.endRadius * m_sizeScale;
	float fDamage = 0.0f;

	// We need greater control over damage distance for Orbital Cannon, use three points instead of two	
	if ((expTagData.damageAtMid > 0.0f) && (fScaledMidRadius > 0.0f))
	{
		Assert(fScaledMidRadius < fScaledEndRadius);
		Assert(expTagData.damageAtMid >= fDamageAtEdge && expTagData.damageAtMid <= fDamageAtCentre);

		if (fDistToCentre <= fScaledMidRadius)
		{
			float fPercentage = fDistToCentre / fScaledMidRadius;
			fDamage = Lerp(fPercentage, expTagData.damageAtCentre, expTagData.damageAtMid);
		}
		else
		{
			float fPercentage = MIN(1.0f, (fDistToCentre - fScaledMidRadius) / (fScaledEndRadius - fScaledMidRadius));
			fDamage = Lerp(fPercentage, expTagData.damageAtMid, expTagData.damageAtEdge);
		}
	}
	else
	{
		const float fDamagePercentage = 1.0f - MIN(1.0f, fDistToCentre / fScaledEndRadius);
		fDamage = (((fDamageAtCentre - fDamageAtEdge)* fDamagePercentage) + fDamageAtEdge);
	}
	
	if (expTagData.bAppliesContinuousDamage)
	{
		const float fBaseFrameRateForContinuousDamage = 30.0f;
		const int fBaseTimeSlicesForContinuousDamage = 2;

		float fDamageOverOneSecond = fDamage * fBaseFrameRateForContinuousDamage * fBaseTimeSlicesForContinuousDamage;
		fDamage = fDamageOverOneSecond * (fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices());
	}

	const CItemInfo* pItemInfo = CWeaponInfoManager::GetInfo(GetWeaponHash());
	if(pItemInfo && pItemInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
	{
		const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pItemInfo);
		{
			fDamage *= pWeaponInfo->GetWeaponDamageModifier();
		}
	}

	return fDamage;
}

//-------------------------------------------------------------------------
// AddDamagedEntity
//-------------------------------------------------------------------------

bool phGtaExplosionInst::AddDamagedEntity(CEntity* pEntity)
{
	for( int i = 0; i < MAX_ENTITIES_TO_APPLY_DAMAGE; i++ )
	{
		if( !m_pAlreadyDamagedEntities[i] )
		{
			m_pAlreadyDamagedEntities[i] = pEntity;
			return true;
		}
	}
	//Warningf( "Maximum entities damaged\n" );
	return false;
}


//-------------------------------------------------------------------------
// RemoveDamagedEntity
//-------------------------------------------------------------------------

void	phGtaExplosionInst::RemoveDamagedEntity(CEntity* pEntity)
{
	for( int i = 0; i < MAX_ENTITIES_TO_APPLY_DAMAGE; i++ )
	{
		if( m_pAlreadyDamagedEntities[i] == pEntity )
		{
			m_pAlreadyDamagedEntities[i] = NULL;
		}
	}
}


//-------------------------------------------------------------------------
// IsEntityAlreadyDamaged
//-------------------------------------------------------------------------

bool	phGtaExplosionInst::IsEntityAlreadyDamaged(CEntity* pEntity)
{
	for( int i = 0; i < MAX_ENTITIES_TO_APPLY_DAMAGE; i++ )
	{
		if( m_pAlreadyDamagedEntities[i] == pEntity )
		{
			return true;
		}
	}
	return false;
}


//-------------------------------------------------------------------------
// CreateAudio
//-------------------------------------------------------------------------

void phGtaExplosionInst::CreateAudio()							
{
	g_audExplosionEntity.PlayExplosion(this);
}


//-------------------------------------------------------------------------
// CreateCameraShake
//-------------------------------------------------------------------------

void phGtaExplosionInst::CreateCameraShake()
{
	float shakeMult = m_underwaterDepth>0.0f ? 0.5f : 1.0f;

	camInterface::ShakeCameraExplosion(m_camShakeNameHash, m_fCamShake*shakeMult, m_fCamShakeRollOffScaling, RCC_VECTOR3(m_vPosWld));
}


//-------------------------------------------------------------------------
// CanActivate
//-------------------------------------------------------------------------

bool phGtaExplosionInst::CanActivate() const
{
	// explosions with a network owner are dormant until they are verified
	//return (!m_networkIdentifier.IsValid());

	// explosions are immediately activated in MP now
	return true;
}

