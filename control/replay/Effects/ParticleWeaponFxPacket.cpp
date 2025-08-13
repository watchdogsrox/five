//=====================================================================================================
// name:		ParticleWeaponFxPacket.cpp
//=====================================================================================================
#include "ParticleWeaponFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "objects/object.h"
#include "modelinfo/WeaponModelInfo.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"
#include "vehicles/vehicle.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxhistory.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "Weapons/Components/WeaponComponentFlashLight.h"
#include "Weapons/Weapon.h"



//////////////////////////////////////////////////////////////////////////
// Projectile Trail Effect (RPG Smoke Trail)
CPacketWeaponProjTrailFx::CPacketWeaponProjTrailFx(u32 uPfxHash, int ptfxOffset, s32 trailBoneID, float trailEvo, Vec3V_In colour)
	: CPacketEvent(PACKETID_WEAPONPROJTRAILFX, sizeof(CPacketWeaponProjTrailFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	m_ptfxOffset		= ptfxOffset;
	m_trailBoneID		= trailBoneID;
	m_trailEvo			= trailEvo;
	m_Colour.Store(colour);
}


//////////////////////////////////////////////////////////////////////////
void CPacketWeaponProjTrailFx::Extract(const CEventInfo<CObject>& eventInfo) const
{
	CObject* pObject = eventInfo.GetEntity();
	if(!pObject)
		return;

	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pObject, m_ptfxOffset, true, m_pfxHash, justCreated);

	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pObject, m_trailBoneID);

		pFxInst->SetCanMoveVeryFast(true);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("trail",0x81528544), m_trailEvo);

		Vec3V colour;
		m_Colour.Load(colour);
		pFxInst->SetColourTint(colour);

		// check if the effect has just been created 
		// or if the entity pointer is no longer valid - the same object could be being re-used (this no longer appears necessary)
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->Start();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Bullet Impact Effect
CPacketWeaponBulletImpactFx::CPacketWeaponBulletImpactFx(u32 pfxHash, Vec3V_In rPos, Vec3V_In rNormal, float lodScale, float scale)
	: CPacketEvent(PACKETID_WEAPONBULLETIMPACTFX, sizeof(CPacketWeaponBulletImpactFx))
	, CPacketEntityInfoStaticAware()
	, m_pfxHash(pfxHash)
	, m_Position(rPos)
	, m_Normal(rNormal)
	, m_lodScale(lodScale)
	, m_scale(scale)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketWeaponBulletImpactFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		Vec3V position, normal;
		m_Position.Load(position);
		m_Normal.Load(normal);

		// calc the world matrix
		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecZ(fxMat, position, normal);

		// set the position of the effect
		if (pEntity && pEntity->GetIsDynamic())
		{
			// make the fx matrix relative to the entity
			Mat34V relFxMat;
			CVfxHelper::CreateRelativeMat(relFxMat, pEntity->GetMatrix(), fxMat);

			ptfxAttach::Attach(pFxInst, pEntity, -1);

			pFxInst->SetOffsetMtx(relFxMat);
		}
		else
		{
			pFxInst->SetBaseMtx(fxMat);
		}

		// if this is a vehicle then override the colour of the effect with that of the vehicle
		if (pEntity && pEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
			CRGBA rgba = CVehicleModelInfo::GetVehicleColours()->GetVehicleColour(pVehicle->GetBodyColour1());
			ptfxWrapper::SetColourTint(pFxInst, rgba.GetRGBA().GetXYZ());
		}

		pFxInst->SetUserZoom(m_scale);
		pFxInst->SetLODScalar(m_lodScale);

		// start the effect
		pFxInst->Start();
	}
}



//////////////////////////////////////////////////////////////////////////
// Weapon Muzzle Flash Effect
CPacketWeaponMuzzleFlashFx::CPacketWeaponMuzzleFlashFx(u32 pfxHash, s32 muzzleBoneIndex, f32 muzzleFlashScale, Vec3V_In muzzleOffset)
	: CPacketEvent(PACKETID_WEAPONMUZZLEFLASHFX, sizeof(CPacketWeaponMuzzleFlashFx))
	, CPacketEntityInfo()
	, m_pfxHash(pfxHash)
	, m_muzzleBoneIndex(muzzleBoneIndex)
	, m_muzzleFlashScale(muzzleFlashScale)
{
	m_muzzleOffset.Store(muzzleOffset);
}


//////////////////////////////////////////////////////////////////////////
void CPacketWeaponMuzzleFlashFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	if (pEntity == NULL)
		return;

	// muzzle flash effects	
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pEntity, m_muzzleBoneIndex);

		Vec3V muzzleOffset;
		m_muzzleOffset.Load(muzzleOffset);
		pFxInst->SetOffsetPos(muzzleOffset);

		pFxInst->SetUserZoom(m_muzzleFlashScale);

		pFxInst->Start();
	}
}

//////////////////////////////////////////////////////////////////////////
// Weapon Muzzle Smoke Effect
CPacketWeaponMuzzleSmokeFx::CPacketWeaponMuzzleSmokeFx(u32 pfxHash, s32 muzzleBoneIndex, float smokeLevel, u32 weaponOffset)
	: CPacketEvent(PACKETID_WEAPONMUZZLESMOKEFX, sizeof(CPacketWeaponMuzzleSmokeFx))
	, CPacketEntityInfo()
	, m_pfxHash(pfxHash)
	, m_muzzleBoneIndex(muzzleBoneIndex)
	, m_smokeLevel(smokeLevel)
	, m_weaponOffset(weaponOffset)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketWeaponMuzzleSmokeFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	if (pEntity == NULL)
		return;

	// muzzle flash effects	
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pEntity, m_weaponOffset, false, m_pfxHash, justCreated);
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pEntity, m_muzzleBoneIndex);

		// set evos
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("smoke", 0xB19506B0), m_smokeLevel);

		// check if the effect has just been created 
		if (justCreated)
		{		
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Gun Shell Particle Effect
CPacketWeaponGunShellFx::CPacketWeaponGunShellFx(u32 pfxHash, s32 weaponIndex, s32 gunShellBoneIndex)
	: CPacketEvent(PACKETID_WEAPONGUNSHELLFX, sizeof(CPacketWeaponGunShellFx))
	, CPacketEntityInfo()
	, m_pfxHash(pfxHash)
	, m_weaponIndex(weaponIndex)
	, m_gunShellBoneIndex(gunShellBoneIndex)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketWeaponGunShellFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	if (pEntity == NULL)
		return;

	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pEntity, m_weaponIndex, false, m_pfxHash, justCreated, true);
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pEntity, m_gunShellBoneIndex);

		pFxInst->Start();
	}
}





//////////////////////////////////////////////////////////////////////////
CPacketWeaponBulletTraceFx::CPacketWeaponBulletTraceFx(u32 uPfxHash, const Vector3& startPos, const Vector3& traceDir, float traceLength)
	: CPacketEvent(PACKETID_WEAPONBULLETTRACEFX, sizeof(CPacketWeaponBulletTraceFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
	, m_traceDir(traceDir)
	, m_startPos(startPos)
	, m_traceLen(traceLength)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketWeaponBulletTraceFx::Extract(const CEventInfo<void>&) const
{
	// play any required effect
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecZ(fxMat, m_startPos, m_traceDir);

		// set the position
		pFxInst->SetBaseMtx(fxMat);

		// set the far clamp
		pFxInst->SetOverrideFarClipDist(m_traceLen);

		// start the effect
		pFxInst->Start();
	}
}


//////////////////////////////////////////////////////////////////////////
tPacketVersion CPacketWeaponBulletTraceFx2_V1 = 1;
CPacketWeaponBulletTraceFx2::CPacketWeaponBulletTraceFx2(u32 uPfxHash, const Vector3& traceDir, float traceLength, const Vec3V& colour)
	: CPacketEvent(PACKETID_WEAPONBULLETTRACEFX2, sizeof(CPacketWeaponBulletTraceFx2), CPacketWeaponBulletTraceFx2_V1)
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
	, m_traceDir(traceDir)
	, m_traceLen(traceLength)
	, m_colour(colour)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketWeaponBulletTraceFx2::Extract(const CEventInfo<CObject>& eventInfo) const
{
	// play any required effect
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		CObject* pObject = eventInfo.GetEntity();
		if(!pObject) return;
		
		CWeapon* pWeapon = pObject->GetWeapon();
		if(!pWeapon) return;

		Vector3 vMuzzlePosition(Vector3::ZeroType);
		const Matrix34& weaponMatrix = RCC_MATRIX34(pObject->GetMatrixRef());
		pWeapon->GetMuzzlePosition(weaponMatrix, vMuzzlePosition);


		// calc the world matrix
 		Mat34V fxMat;
 		CVfxHelper::CreateMatFromVecZ(fxMat, VECTOR3_TO_VEC3V(vMuzzlePosition), m_traceDir);
 
 		// set the position
 		pFxInst->SetBaseMtx(fxMat);
 
 		// set the far clamp
 		pFxInst->SetOverrideFarClipDist(m_traceLen);

		// Add the colour that was recorded
		if(GetPacketVersion() >= CPacketWeaponBulletTraceFx2_V1)
		{
			Vec3V colour;
			m_colour.Load(colour);

			if(RenderPhaseSeeThrough::GetState())
				colour = Vec3V(V_ONE);

			pFxInst->SetColourTint(colour);
		}
 
 		// start the effect
 		pFxInst->Start();
	}
}



//========================================================================================================================
CPacketWeaponExplosionWaterFx::CPacketWeaponExplosionWaterFx(u32 uPfxHash, const Vector3& rPos, float fDepth, float fRiverEvo)
	: CPacketEvent(PACKETID_WEAPONEXPLOSIONWATERFX, sizeof(CPacketWeaponExplosionWaterFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	m_Position.Store(rPos);

	m_depth = fDepth;
	m_riverEvo = fRiverEvo;
}

//========================================================================================================================
void CPacketWeaponExplosionWaterFx::Extract(const CEventInfo<void>&) const
{
	// play any required effect
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		Vector3 pos;
		m_Position.Load(pos);

		// calc the world matrix
		Mat34V fxMat;
		Vec3V vNormal(V_Z_AXIS_WZERO);
		CVfxHelper::CreateMatFromVecZ(fxMat, VECTOR3_TO_VEC3V(pos), vNormal);

		// set the position
		pFxInst->SetBaseMtx(fxMat);

		// set any evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth",0xbca972d6), m_depth);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("river", 0xBA8B3109), m_riverEvo);

		// start the effect
		pFxInst->Start();
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: WEAPON EXPLOS WATER FX: \n");
#endif
	}
}



//////////////////////////////////////////////////////////////////////////
CPacketWeaponExplosionFx_Old::CPacketWeaponExplosionFx_Old(u32 uPfxHash, const Matrix34& rMatrix, s32 boneIndex, float fScale)
	: CPacketEvent(PACKETID_WEAPONEXPLOSIONFX_OLD, sizeof(CPacketWeaponExplosionFx_Old))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	m_posAndQuat.StoreMatrix(rMatrix);
	m_boneIndex = boneIndex;
	m_scale = fScale;
}

//////////////////////////////////////////////////////////////////////////
void CPacketWeaponExplosionFx_Old::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		Matrix34 matrix;
		m_posAndQuat.LoadMatrix(matrix);

		if (pEntity)
		{
			ptfxAttach::Attach(pFxInst, pEntity, m_boneIndex);
			pFxInst->SetOffsetMtx(MATRIX34_TO_MAT34V(matrix));
		}
		else
		{
			pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(matrix));
		}

		pFxInst->SetUserZoom(m_scale);

		pFxInst->Start();
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: WEAPON EXPLOS FX: \n");
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketWeaponExplosionFx_Old::Print() const
{
	CPacketEvent::Print();

	replayDebugf1("BoneIndex: %d", m_boneIndex);
	replayDebugf1("Scale: %f", m_scale);
}

//////////////////////////////////////////////////////////////////////////
CPacketWeaponExplosionFx::CPacketWeaponExplosionFx(u32 uPfxHash, const Matrix34& rMatrix, s32 boneIndex, float fScale)
	: CPacketEvent(PACKETID_WEAPONEXPLOSIONFX, sizeof(CPacketWeaponExplosionFx))
	, CPacketEntityInfoStaticAware()
	, m_pfxHash(uPfxHash)
{
	m_posAndQuat.StoreMatrix(rMatrix);
	m_boneIndex = boneIndex;
	m_scale = fScale;
}

//////////////////////////////////////////////////////////////////////////
void CPacketWeaponExplosionFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		Matrix34 matrix;
		m_posAndQuat.LoadMatrix(matrix);

		if (pEntity)
		{
			ptfxAttach::Attach(pFxInst, pEntity, m_boneIndex);
			pFxInst->SetOffsetMtx(MATRIX34_TO_MAT34V(matrix));
		}
		else
		{
			pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(matrix));
		}

		pFxInst->SetUserZoom(m_scale);

		pFxInst->Start();
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: WEAPON EXPLOS FX: \n");
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketWeaponExplosionFx::Print() const
{
	CPacketEvent::Print();

	replayDebugf1("BoneIndex: %d", m_boneIndex);
	replayDebugf1("Scale: %f", m_scale);
}


//////////////////////////////////////////////////////////////////////////
// Volumetric Effect
CPacketVolumetricFx::CPacketVolumetricFx(u32 pfxHash, s32 muzzleBoneIndex, u32 weaponOffset)
	: CPacketEvent(PACKETID_WEAPONVOLUMETRICFX, sizeof(CPacketVolumetricFx))
	, CPacketEntityInfoStaticAware()
	, m_pfxHash(pfxHash)
	, m_muzzleBoneIndex(muzzleBoneIndex)
	, m_weaponOffset(weaponOffset)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketVolumetricFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();

	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pEntity, m_weaponOffset, false, m_pfxHash, justCreated);
	if (pFxInst)
	{
		// set position
		ptfxAttach::Attach(pFxInst, pEntity, m_muzzleBoneIndex);

		// check if the effect has just been created 
		if (justCreated)
		{		
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketWeaponFlashLight::CPacketWeaponFlashLight(bool active, u8 lod)
	: CPacketEvent(PACKETID_WEAPONFLASHLIGHT, sizeof(CPacketWeaponFlashLight))
	, CPacketEntityInfo()
	, m_Active(active)
	, m_lod(lod)
{
}

//////////////////////////////////////////////////////////////////////////
void CPacketWeaponFlashLight::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	if (pEntity && pEntity->GetIsTypeObject())
	{
		CObject* pObject = static_cast<CObject*>(pEntity);
		if(pObject && pObject->GetWeapon())
		{
			CWeaponComponentFlashLight* pFlashLight = pObject->GetWeapon()->GetFlashLightComponent();
			if(pFlashLight)
			{
				// Set if the light was active
				pFlashLight->SetActive(m_Active);

				// Override the lod for the flash light, ideally the light could be added to the  CWeaponComponentFlashLight::flashlightList as normal
				// but that requires the ped/pedtype and to replicate the existing logic in CWeaponComponentFlashLight::Process. 
				pFlashLight->OverrideLod(m_lod);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketWeaponThermalVision::CPacketWeaponThermalVision(bool active)
	: CPacketEvent(PACKETID_WEAPON_THERMAL_VISION, sizeof(CPacketWeaponThermalVision))
	, CPacketEntityInfo()
{
	SetPadBool(0, active);
}

//////////////////////////////////////////////////////////////////////////
void CPacketWeaponThermalVision::Extract(const CEventInfo<void>& eventInfo) const
{
	bool active = GetPadBool(0);
	if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK))
	{
		active = !active;
	}

	RenderPhaseSeeThrough::SetState(active);
}

bool CPacketWeaponThermalVision::HasExpired(const CEventInfoBase&) const
{
	return !RenderPhaseSeeThrough::GetState();
}


#endif // GTA_REPLAY
