//=====================================================================================================
// name:		ProjectedTexturePacket.cpp
//=====================================================================================================

#include "ProjectedTexturePacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "vfx/systems/vfxBlood.h"
#include "vfx/systems/vfxWeapon.h"
#include "vfx/decals/DecalManager.h"
#include "peds/ped.h"
#include "peds/rendering/PedDamage.h"
#include "physics/physics.h"
#include "physics/WorldProbe/WorldProbe.h"
#include "task/Combat/TaskDamageDeath.h"
#include "weapons/WeaponEnums.h"
#include "vector/vector3.h"
#include "vehicles/vehicle.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "control/replay/ReplayExtensions.h"

#include "fwscene/stores/staticboundsstore.h"

//////////////////////////////////////////////////////////////////////////
CPacketPedBloodDamage::CPacketPedBloodDamage(u16 component, const Vector3& pos, const Vector3& dir, u8 type, u32 bloodHash, bool forceNotStanding, const CPed* pPed, int damageBlitID)
	: CPacketEvent(PACKETID_PEDBLOODDAMAGE, sizeof(CPacketPedBloodDamage))
	, CPacketEntityInfo()
	, m_component(component)
	, m_type(type)
	, m_bloodHash(bloodHash)
	, m_forceNotStanding(forceNotStanding)
	, m_decalId(damageBlitID)
{
	Matrix34 matInverse = MAT34V_TO_MATRIX34(pPed->GetMatrix());

	matInverse.Inverse();
	Vector3 objectSpacePos = pos;
	matInverse.Transform(objectSpacePos);
	m_position.Store(objectSpacePos);
	
	Vector3 objectSpaceDirection = dir;
	matInverse.Transform3x3(objectSpaceDirection);
	m_direction.Store(objectSpaceDirection);
}


//////////////////////////////////////////////////////////////////////////
void CPacketPedBloodDamage::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();

	if(pPed == NULL)
		return;

	Matrix34 mat = MAT34V_TO_MATRIX34(pPed->GetTransform().GetMatrix());
	Vector3 objectSpacePos;
	m_position.Load(objectSpacePos);
	mat.Transform(objectSpacePos);
	Vector3 position = objectSpacePos;

	Vector3 objectSpaceDir;
	m_direction.Load(objectSpaceDir);
	mat.Transform3x3(objectSpaceDir);
	Vector3 direction = objectSpaceDir;

	if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD))
	{
		u8 damageId;
		m_decalId = -1;
		if(CPedBloodDamageInfo* pBloodInfo = PEDDAMAGEMANAGER.GetBloodInfoAndDamageId(pPed, m_bloodHash, damageId))
		{
			if( eventInfo.GetIsFirstFrame() )
			{
				//set these timers to be really small so we don't see them fading up for monitored packets.
				pBloodInfo->m_SoakDryingTime = 0.001f;
				pBloodInfo->m_SplatterDryingTime = 0.001f;
				pBloodInfo->m_WoundDryingTime = 0.001f;
			}

			m_decalId = PEDDAMAGEMANAGER.AddBloodDamage(damageId, m_component, position, direction, (ePedDamageTypes)m_type, pBloodInfo, m_forceNotStanding);
		}
	}
	else if(m_decalId != -1)
	{
		u8 damageId = pPed->GetDamageSetID();
		if(damageId != kInvalidPedDamageSet)
		{
			PEDDAMAGEMANAGER.ReplayClearPedBlood(damageId, m_decalId);
		}
		m_decalId = -1;
	}
}

bool CPacketPedBloodDamage::HasExpired(const CEventInfo<CPed>& info) const
{
	const CPed* pPed = info.GetEntity();
	if( pPed )
	{
		u8 damageId = pPed->GetDamageSetID();
		if(damageId != kInvalidPedDamageSet)
		{
			return !PEDDAMAGEMANAGER.ReplayIsBlitIdValid(damageId, m_decalId);
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
CPacketPedBloodDamageScript::CPacketPedBloodDamageScript(u8 zone, const Vector2 & uv, float rotation, float scale, atHashWithStringBank bloodNameHash, float preAge, int forcedFrame, int damageBlitID)
	: CPacketEvent(PACKETID_PEDBLOODDAMAGESCRIPT, sizeof(CPacketPedBloodDamageScript))
	, CPacketEntityInfo()
	, m_Zone(zone)
	, m_Rotation(rotation)
	, m_Scale(scale)
	, m_BloodNameHash(bloodNameHash)
	, m_PreAge(preAge)
	, m_ForcedFrame(forcedFrame)
	, m_decalId(damageBlitID)
{
	m_Uv.Store(uv);
}


//////////////////////////////////////////////////////////////////////////
void CPacketPedBloodDamageScript::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();

	if(pPed == NULL)
		return;

	if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD))
	{
		u8 damageId;
		m_decalId = -1;
		if(PEDDAMAGEMANAGER.GetBloodInfoAndDamageId(pPed, m_BloodNameHash, damageId))
		{
			Vector2 uv;
			m_Uv.Load(uv);
			m_decalId = PEDDAMAGEMANAGER.AddPedBlood(pPed, static_cast<ePedDamageZones>(m_Zone), uv, m_Rotation, 
													 m_Scale, m_BloodNameHash, false, m_PreAge, m_ForcedFrame, false, 255);
		}
	}
	else if(m_decalId != -1)
	{
		u8 damageId = pPed->GetDamageSetID();
		if(damageId != kInvalidPedDamageSet)
		{
			PEDDAMAGEMANAGER.ClearPedBlood(damageId);
		}
		m_decalId = -1;
	}
}

bool CPacketPedBloodDamageScript::HasExpired(const CEventInfo<CPed>& info) const
{
	const CPed* pPed = info.GetEntity();
	if( pPed )
	{
		u8 damageId = pPed->GetDamageSetID();
		if(damageId != kInvalidPedDamageSet)
		{
			return !PEDDAMAGEMANAGER.ReplayIsBlitIdValid(damageId, m_decalId);
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Store information for a bullet impact decal
tPacketVersion CPacketPTexWeaponShot_V1 = 1;
tPacketVersion CPacketPTexWeaponShot_V2 = 2;
tPacketVersion CPacketPTexWeaponShot_V3 = 3;
tPacketVersion CPacketPTexWeaponShot_V4 = 4;
tPacketVersion CPacketPTexWeaponShot_V5 = 5;
CPacketPTexWeaponShot::CPacketPTexWeaponShot(const VfxCollInfo_s& collisionInfo, const bool forceMetal, const bool forceWater, const bool glass, const bool firstGlassHit, int decalID, const bool isSnowball)
	: CPacketDecalEvent(decalID, PACKETID_PTEXWEAPONSHOT, sizeof(CPacketPTexWeaponShot), CPacketPTexWeaponShot_V5)
	, CPacketEntityInfoStaticAware()
{
	Matrix34 matInverse = MAT34V_TO_MATRIX34(collisionInfo.regdEnt->GetMatrix());
	matInverse.Inverse();

	Vector3 objectSpacePos = RCC_VECTOR3(collisionInfo.vPositionWld);
	matInverse.Transform(objectSpacePos);
	Vector3 objectSpaceNormal = RCC_VECTOR3(collisionInfo.vNormalWld);
	matInverse.Transform3x3(objectSpaceNormal);
	Vector3 objectSpaceDirection = RCC_VECTOR3(collisionInfo.vDirectionWld);
	matInverse.Transform3x3(objectSpaceDirection);

	m_position.Store(objectSpacePos);
	m_normal.Store(objectSpaceNormal);
	m_direction.Store(objectSpaceDirection);
	m_materialId	= collisionInfo.materialId;
	m_componentId	= collisionInfo.componentId;
	m_force			= collisionInfo.force;
	m_roomId		= collisionInfo.roomId;
	m_weaponGroup	= static_cast<u8>(collisionInfo.weaponGroup);

	m_isBloody		= collisionInfo.isBloody;
	m_isWater		= collisionInfo.isWater;
	m_isExitFx		= collisionInfo.isExitFx;

	m_armouredGlassShotByPedInsideVehicle = collisionInfo.armouredGlassShotByPedInsideVehicle;
	m_armouredGlassWeaponDamage = collisionInfo.armouredGlassWeaponDamage;
	m_isFMJAmmo		= collisionInfo.isFMJAmmo;

	m_forceMetal	= forceMetal;
	m_forceWater	= forceWater;
	m_glass			= glass;
	m_firstGlassHit = firstGlassHit;

	//this was a special forced hit, usually cause by breaking into a locked vehicle or closing a vehicle door when it was already broken
	//record it in a flag so we can reset these to the correct value in the extract
	m_forcedGlassHit = IsEqualAll(collisionInfo.vPositionWld, Vec3V(V_FLT_MAX)) == 1 &&	IsEqualAll(collisionInfo.vDirectionWld, Vec3V(V_Z_AXIS_WZERO)) == 1;

	if(m_glass && collisionInfo.regdEnt && collisionInfo.regdEnt->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(collisionInfo.regdEnt.Get());
		ReplayVehicleExtension::SetVehicleAsDamaged(pVehicle);
	}

	m_isSnowball	= isSnowball;
	m_unused		= 0;
}


//////////////////////////////////////////////////////////////////////////
void CPacketPTexWeaponShot::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CPacketDecalEvent::CanPlayBack(eventInfo.GetPlaybackFlags()))
	{
		if(m_glass && m_firstGlassHit)
		{
			CEntity* pEntity = eventInfo.GetEntity();
			if(pEntity && pEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = (CVehicle*)pEntity;

				//loop around all the windows to find the one were after and reset it
				for(int nId = VEH_FIRST_WINDOW; nId <= VEH_LAST_WINDOW; nId++)
				{	
					const int windowBoneId = pVehicle->GetBoneIndex((eHierarchyId)nId);
					s32 iWindowComponent = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(windowBoneId);

					if(iWindowComponent != -1 && iWindowComponent == m_componentId 
						&& pVehicle->IsBrokenFlagSet(iWindowComponent)) //make sure the window is actually broken before we fix it
					{
						pVehicle->FixWindow((eHierarchyId)nId);
						pVehicle->RollWindowUp((eHierarchyId)nId);
					}
				}
			}
			else if(pEntity && pEntity->GetIsTypeObject())
			{
				// If it's an object go through the vehicle glass manager to remove the glass.
				CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
				g_vehicleGlassMan.RemoveComponent(pPhysical, m_componentId);
				pPhysical->ClearBrokenFlag(m_componentId);
				pPhysical->ClearHiddenFlag(m_componentId);
			}
		}
		return;
	}

	CEntity* pEntity = eventInfo.GetEntity();

	Vector3 worldSpacePos;
	m_position.Load(worldSpacePos);
	Vector3 worldSpaceNormal;
	m_normal.Load(worldSpaceNormal);
	Vector3 worldSpaceDir;
	m_direction.Load(worldSpaceDir);
	
	if(GetPacketVersion() >= CPacketPTexWeaponShot_V1)
	{	// Old version of this packet stored these in world space...
		// We need to store in object space though to apply decals to entities
		// in different positions.
		Matrix34 mat = MAT34V_TO_MATRIX34(pEntity->GetTransform().GetMatrix());
		mat.Transform(worldSpacePos);
		mat.Transform3x3(worldSpaceNormal);
		mat.Transform3x3(worldSpaceDir);
	}

	VfxCollInfo_s vfxCollInfo;
	vfxCollInfo.vPositionWld	= VECTOR3_TO_VEC3V(worldSpacePos);
	vfxCollInfo.vNormalWld		= VECTOR3_TO_VEC3V(worldSpaceNormal);
	vfxCollInfo.vDirectionWld	= VECTOR3_TO_VEC3V(worldSpaceDir);
	vfxCollInfo.regdEnt			= pEntity;
	vfxCollInfo.materialId		= m_materialId;
	vfxCollInfo.componentId		= m_componentId;
	vfxCollInfo.roomId			= m_roomId;
	vfxCollInfo.force			= m_force;
	vfxCollInfo.weaponGroup		= static_cast<eWeaponEffectGroup>(m_weaponGroup);
	vfxCollInfo.isBloody		= m_isBloody;
	vfxCollInfo.isWater			= m_isWater;
	vfxCollInfo.isExitFx		= m_isExitFx;
	vfxCollInfo.isSnowball		= false;
	if(GetPacketVersion() >= CPacketPTexWeaponShot_V3)
		vfxCollInfo.isSnowball	= m_isSnowball;
	if(GetPacketVersion() >= CPacketPTexWeaponShot_V4)
	{
		vfxCollInfo.armouredGlassShotByPedInsideVehicle = m_armouredGlassShotByPedInsideVehicle;
		vfxCollInfo.armouredGlassWeaponDamage = m_armouredGlassWeaponDamage;
	}
	vfxCollInfo.isFMJAmmo = false;
	if(GetPacketVersion() >= CPacketPTexWeaponShot_V5)
		vfxCollInfo.isFMJAmmo = m_isFMJAmmo;

	if(m_glass)
	{
		if(GetPacketVersion() >= CPacketPTexWeaponShot_V2)
		{
			//restore the setting that are usually set for a forced hit
			if(m_forcedGlassHit)
			{
				vfxCollInfo.vPositionWld = Vec3V(V_FLT_MAX);
				vfxCollInfo.vDirectionWld = Vec3V(V_Z_AXIS_WZERO);
			}
		}

		if(pEntity->GetIsTypeObject())
		{
			fragInst* pFragInst = pEntity->GetFragInst();
			if (pFragInst)
			{
				// If it's an object then the glass bone may have been zero'd out.  In this case the glass
				// is placed from the bounds so make sure they're set up.
				pFragInst->PoseBoundsFromSkeleton(true, false);
			}
		}

		g_vehicleGlassMan.StoreCollision(vfxCollInfo);

		return;
	}


	VfxGroup_e vfxGroup;
	vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(vfxCollInfo.materialId);

	VfxWeaponInfo_s* pFxWeaponInfo = g_vfxWeapon.GetInfo(vfxGroup, vfxCollInfo.weaponGroup, vfxCollInfo.isBloody);
	if(m_forceMetal)
		pFxWeaponInfo = g_vfxWeapon.GetInfo(VFXGROUP_CAR_METAL, vfxCollInfo.weaponGroup);
	else if(m_forceWater)
		pFxWeaponInfo = g_vfxWeapon.GetInfo(VFXGROUP_LIQUID_WATER, vfxCollInfo.weaponGroup);

	VfxWeaponInfo_s fxWeaponInfo = *pFxWeaponInfo;
	if( eventInfo.GetIsFirstFrame() || IsInitialStatePacket() || eventInfo.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP))
		fxWeaponInfo.fadeInTime = 0.0f;

	m_decalId = g_decalMan.AddWeaponImpact(fxWeaponInfo, vfxCollInfo);
}


//////////////////////////////////////////////////////////////////////////
bool CPacketPTexWeaponShot::HasExpired(const CEventInfo<CEntity>& info) const
{
	if(m_glass)
	{
		if(info.GetEntity() && info.GetEntity()->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<CVehicle*>(info.GetEntity());
			return ReplayVehicleExtension::IsVehicleUndamaged(pVehicle);
		}
		else
		{
			//we don't have a valid vehicle anymore, so we can just discard this packet
			return true;
		}
	}
	else
	{
		return CPacketDecalEvent::HasExpired(info);
	}
}

//////////////////////////////////////////////////////////////////////////
eShouldExtract CPacketPTexWeaponShot::ShouldExtract(u32 playbackFlags, const u32 packetFlags, bool isFirstFrame) const
{
	//glass isn't really a decal so we should use the same extract logic, otherwise it will fail sometimes due to
	//the number of decals per update being reached and then bad thing(tm) occur
	if(m_glass)
	{
		return CPacketEvent::ShouldExtract(playbackFlags, packetFlags, isFirstFrame);
	}

	return CPacketDecalEvent::ShouldExtract(playbackFlags, packetFlags, isFirstFrame); 
}

//////////////////////////////////////////////////////////////////////////
CPacketAddScriptDecal::CPacketAddScriptDecal(const Vector3& pos, const Vector3& dir, const Vector3& side, Color32 color, int renderSettingId, float width, float height, float totalLife, DecalType_e decalType, bool isDynamic, int decalID)
	: CPacketEventTracked(PACKETID_SCRIPTDECALADD, sizeof(CPacketAddScriptDecal))
	, CPacketEntityInfo()
	, m_pos(pos)
	, m_dir(dir)
	, m_side(side)
	, m_color(color)
	, m_renderSettingId(renderSettingId)
	, m_width(width)
	, m_height(height)
	, m_totalLife(totalLife)
	, m_decalType(decalType)
	, m_isDynamic(isDynamic)
	, m_decalId(decalID)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketAddScriptDecal::Extract(CTrackedEventInfo<tTrackedDecalType>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		if(eventInfo.m_pEffect[0] > 0)
		{
			if(g_decalMan.IsAlive(eventInfo.m_pEffect[0]))
			{
				g_decalMan.Remove(eventInfo.m_pEffect[0]);
			}

			eventInfo.m_pEffect[0] = 0;
		}
		return;
	}

	s32 decalRenderSettingIndex;
	s32 decalRenderSettingCount;
	g_decalMan.FindRenderSettingInfo(m_renderSettingId, decalRenderSettingIndex, decalRenderSettingCount);	

	Vector3 pos;
	Vector3 dir;
	Vector3 side;

	m_pos.Load(pos);
	m_dir.Load(dir);
	m_side.Load(side);

	float startLife = 0.0f;
	if( eventInfo.GetIsFirstFrame() )
		startLife = DECAL_INST_INV_FADE_TIME_UNITS_HZ;

	eventInfo.m_pEffect[0] = g_decalMan.AddGeneric(
		m_decalType,
		decalRenderSettingIndex,
		decalRenderSettingCount,
		VECTOR3_TO_VEC3V((Vector3)pos),
		VECTOR3_TO_VEC3V((Vector3)dir),
		VECTOR3_TO_VEC3V((Vector3)side),
		m_width,
		m_height,
		0.0f,
		m_totalLife,
		0.0f,
		1.0f,
		1.0f,
		0.0f,
		m_color,
		false,
		false,
		0.0f,
		false,
		m_isDynamic,
		startLife );
}


//////////////////////////////////////////////////////////////////////////
bool CPacketAddScriptDecal::HasExpired(const CTrackedEventInfo<tTrackedDecalType>&) const
{
	return !g_decalMan.IsAlive(m_decalId);
}


//////////////////////////////////////////////////////////////////////////
CPacketRemoveScriptDecal::CPacketRemoveScriptDecal()
	: CPacketEventTracked(PACKETID_SCRIPTDECALREMOVE, sizeof(CPacketAddScriptDecal))
	, CPacketEntityInfo()
{
	SetEndTracking();
}

//////////////////////////////////////////////////////////////////////////
void CPacketRemoveScriptDecal::Extract(CTrackedEventInfo<tTrackedDecalType>& eventInfo) const
{
	if(eventInfo.m_pEffect[0] > 0)
	{
		if(g_decalMan.IsAlive(eventInfo.m_pEffect[0]))
		{
			g_decalMan.Remove(eventInfo.m_pEffect[0]);
		}

		eventInfo.m_pEffect[0] = 0;
		return;
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketPTexFootPrint::CPacketPTexFootPrint(s32 settingIndex, s32 settingCount, float width, float length,	Color32 col, Id	mtlId, const Vector3& footStepPos, const Vector3& footStepUp, const Vector3& footStepSide, bool isLeft, float decalLife)
	: CPacketDecalEvent(0, PACKETID_PTEXFOOTPRINT, sizeof(CPacketPTexFootPrint))
{
	m_decalRenderSettingIndex	= settingIndex;
	m_decalRenderSettingCount	= settingCount;
	m_decalWidth				= width;
	m_decalLength				= length;
	m_col						= col;
	m_mtlId						= mtlId;
	m_vFootStepPos.Store(footStepPos);
	m_vFootStepUp.Store(footStepUp);
	m_vFootStepSide.Store(footStepSide);
	m_isLeft					= isLeft;
	m_decalLife					= decalLife;
}


//////////////////////////////////////////////////////////////////////////
void CPacketPTexFootPrint::Extract(const CEventInfo<void>& eventInfo) const
{
	if(!CPacketDecalEvent::CanPlayBack(eventInfo.GetPlaybackFlags()))
	{
		return;
	}

	Vector3 position;
	m_vFootStepPos.Load(position);
	Vector3 up;
	m_vFootStepUp.Load(up);
	Vector3 side;
	m_vFootStepSide.Load(side);

	m_decalId = g_decalMan.AddFootprint(m_decalRenderSettingIndex, 
		m_decalRenderSettingCount, 
		m_decalWidth, 
		m_decalLength, 
		m_col,  
		m_mtlId, 
		VECTOR3_TO_VEC3V(position), 
		VECTOR3_TO_VEC3V(up), 
		VECTOR3_TO_VEC3V(side), 
		m_isLeft, 
		m_decalLife);
}

//////////////////////////////////////////////////////////////////////////
CPacketPTexMtlBang::CPacketPTexMtlBang(const VfxCollInfo_s& vfxCollInfo, phMaterialMgr::Id tlIdB, CEntity* pEntity, int decalId, eReplayPacketId packetId)
	: CPacketDecalEvent(decalId, packetId, sizeof(CPacketPTexMtlBang)),
	CPacketEntityInfoStaticAware()
{
	Matrix34 matInverse = MAT34V_TO_MATRIX34(pEntity->GetMatrix());

	matInverse.Inverse();
	Vector3 objectSpacePos = VEC3V_TO_VECTOR3(vfxCollInfo.vPositionWld);
	matInverse.Transform(objectSpacePos);
	m_vPositionWld.Store(objectSpacePos);

	Vector3 objectSpaceColNormal = VEC3V_TO_VECTOR3(vfxCollInfo.vNormalWld);
	matInverse.Transform3x3(objectSpaceColNormal);
	m_vNormalWld.Store(objectSpaceColNormal);

	m_MaterialId = vfxCollInfo.materialId;
	m_ComponentId = vfxCollInfo.componentId;
	m_Force =	vfxCollInfo.force;

	m_tlIdB = tlIdB;
}

//========================================================================================================================
void CPacketPTexMtlBang::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CPacketDecalEvent::CanPlayBack(eventInfo.GetPlaybackFlags()))
	{
		return;
	}

	CEntity* pEntity = eventInfo.GetEntity(0);

	VfxCollInfo_s vfxCollInfo;
	vfxCollInfo.regdEnt = pEntity;

	Matrix34 mat = MAT34V_TO_MATRIX34(pEntity->GetTransform().GetMatrix());
	Vector3 objectSpacePos;
	m_vPositionWld.Load(objectSpacePos);
	mat.Transform(objectSpacePos);
	vfxCollInfo.vPositionWld = VECTOR3_TO_VEC3V(objectSpacePos);

	Vector3 objectSpaceCollNormal;
	m_vNormalWld.Load(objectSpaceCollNormal);
	mat.Transform3x3(objectSpaceCollNormal);
	vfxCollInfo.vNormalWld = VECTOR3_TO_VEC3V(objectSpaceCollNormal);


	vfxCollInfo.materialId = m_MaterialId;
	vfxCollInfo.roomId = 0;
	vfxCollInfo.componentId = m_ComponentId;
	vfxCollInfo.force = m_Force;
	vfxCollInfo.vDirectionWld	= Vec3V(V_Z_AXIS_WZERO);
	vfxCollInfo.weaponGroup		= WEAPON_EFFECT_GROUP_PUNCH_KICK;		// shouldn't matter what this is set to at this point
	vfxCollInfo.isBloody		= false;
	vfxCollInfo.isWater			= false;
	vfxCollInfo.isExitFx		= false;
	vfxCollInfo.noPtFx			= false;
	vfxCollInfo.noPedDamage		= false;
	vfxCollInfo.noDecal			= false;
	vfxCollInfo.isSnowball		= false;
	vfxCollInfo.isFMJAmmo		= false;

	VfxMaterialInfo_s* pFxMaterialInfo = g_vfxMaterial.GetBangInfo(PGTAMATERIALMGR->GetMtlVfxGroup(m_MaterialId), PGTAMATERIALMGR->GetMtlVfxGroup(m_tlIdB));

	if( pFxMaterialInfo == NULL )
	{
		return;
	}

	float waterDepth;
	if (CVfxHelper::GetWaterDepth(vfxCollInfo.vPositionWld, waterDepth, pEntity))
	{
		if (waterDepth>0.0f)
		{
			return;
		}
	}

	m_decalId = g_decalMan.AddBang(*pFxMaterialInfo, vfxCollInfo);
}

CPacketPTexMtlBang_PlayerVeh::CPacketPTexMtlBang_PlayerVeh(const VfxCollInfo_s& vfxCollInfo, phMaterialMgr::Id tlIdB, CEntity* pEntity, int decalId)
	: CPacketPTexMtlBang(vfxCollInfo, tlIdB, pEntity, decalId, PACKETID_PTEXMTLBANG_PLAYERVEH)
{
}

//////////////////////////////////////////////////////////////////////////
CPacketPTexMtlScrape::CPacketPTexMtlScrape(CEntity* pEntity, s32 renderSettingsIndex, Vec3V_In vPtA, Vec3V_InOut vPtB, Vec3V_In vNormal, float width, Color32 col, float life, u8 mtlId, float maxOverlayRadius, float duplicateRejectDist, int decalId, eReplayPacketId packetId)
	: CPacketDecalEvent(decalId, packetId, sizeof(CPacketPTexMtlScrape)),
	CPacketEntityInfoStaticAware()
{

	Matrix34 matInverse = MAT34V_TO_MATRIX34(pEntity->GetMatrix());

	matInverse.Inverse();
	Vector3 objectSpacePosA = VEC3V_TO_VECTOR3(vPtA);
	matInverse.Transform(objectSpacePosA);
	m_PtA.Store(objectSpacePosA);

	Vector3 objectSpacePosB = VEC3V_TO_VECTOR3(vPtB);
	matInverse.Transform(objectSpacePosB);
	m_PtB.Store(objectSpacePosB);

	Vector3 objectSpaceNormal = VEC3V_TO_VECTOR3(vNormal);
	matInverse.Transform3x3(objectSpaceNormal);
	m_Normal.Store(objectSpaceNormal);

	m_renderSettingsIndex = renderSettingsIndex;
	m_width = width;
	m_col = col;
	m_life = life;
	m_mtlId = mtlId;
	m_maxOverlayRadius = maxOverlayRadius;
	m_duplicateRejectDist = duplicateRejectDist;
}

//========================================================================================================================
void CPacketPTexMtlScrape::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity(0);	

	if( !pEntity->GetIsTypeVehicle())
		return;	

	if(!CPacketDecalEvent::CanPlayBack(eventInfo.GetPlaybackFlags()))
	{
		return;
	}
	
	Matrix34 mat = MAT34V_TO_MATRIX34(pEntity->GetTransform().GetMatrix());
	Vector3 objectSpacePosA;
	m_PtA.Load(objectSpacePosA);
	mat.Transform(objectSpacePosA);
	Vec3V ptA = VECTOR3_TO_VEC3V(objectSpacePosA);

	Vector3 objectSpacePosB;
	m_PtB.Load(objectSpacePosB);
	mat.Transform(objectSpacePosB);
	Vec3V ptB = VECTOR3_TO_VEC3V(objectSpacePosB);

	Vector3 objectSpaceNormal;
	m_Normal.Load(objectSpaceNormal);
	mat.Transform3x3(objectSpaceNormal);
	Vec3V normal = VECTOR3_TO_VEC3V(objectSpaceNormal);

	//If this is the first frame then dont fade up the decals.
	bool fadeInDecals = !eventInfo.GetIsFirstFrame();

	m_decalId = g_decalMan.AddScuff(pEntity, m_renderSettingsIndex, ptA, ptB, normal, 
		m_width, m_col, m_life, m_mtlId, 
		m_maxOverlayRadius, m_duplicateRejectDist, fadeInDecals);
}

//////////////////////////////////////////////////////////////////////////
CPacketPTexMtlScrape_PlayerVeh::CPacketPTexMtlScrape_PlayerVeh(CEntity* pEntity, s32 renderSettingsIndex, Vec3V_In vPtA, Vec3V_InOut vPtB, Vec3V_In vNormal, float width, Color32 col, float life, u8 mtlId, float maxOverlayRadius, float duplicateRejectDist, int decalId)
	: CPacketPTexMtlScrape(pEntity, renderSettingsIndex, vPtA, vPtB, vNormal, width, col, life, mtlId, maxOverlayRadius, duplicateRejectDist, decalId, PACKETID_PTEXMTLSCRAPE_PLAYERVEH)
{
}

/////////////////////////////////////////////////////

CPacketDecalRemove::CPacketDecalRemove(int boneIndex)
	: CPacketEvent(PACKETID_DECALSREMOVE, sizeof(CPacketDecalRemove))
	, CPacketEntityInfoStaticAware()
{
	m_boneIndex = boneIndex;
}


void CPacketDecalRemove::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity(0);
	if(pEntity)
		g_decalMan.Remove(pEntity, m_boneIndex);
}

//////////////////////////////////////////////////////////////////////////
CPacketDisableDecalRendering::CPacketDisableDecalRendering()
	: CPacketEvent(PACKETID_DISABLE_DECAL_RENDERING, sizeof(CPacketDisableDecalRendering))
	, CPacketEntityInfo()
{
}

//////////////////////////////////////////////////////////////////////////
void CPacketDisableDecalRendering::Extract(const CEventInfo<void>&) const
{
	g_decalMan.SetDisableRenderingThisFrame();
}


CPacketAddSnowballDecal::CPacketAddSnowballDecal(const Vec3V& worldPos, const Vec3V& worldDir, const Vec3V& side, int decalId)
	: CPacketDecalEvent(decalId, PACKETID_ADDSNOWBALLDECAL, sizeof(CPacketAddSnowballDecal))
	, CPacketEntityInfo()
{
	m_worldPos.Store(VEC3V_TO_VECTOR3(worldPos));
	m_worldDir.Store(VEC3V_TO_VECTOR3(worldDir));
	m_side.Store(VEC3V_TO_VECTOR3(side));
}


void CPacketAddSnowballDecal::Extract(const CEventInfo<void>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		if(g_decalMan.IsAlive(m_decalId))
		{
			g_decalMan.Remove(m_decalId);
		}
		return;
	}

	if(!CPacketDecalEvent::CanPlayBack(eventInfo.GetPlaybackFlags()))
	{
		return;
	}

	Vector3 worldPos, worldDir, side;
	m_worldPos.Load(worldPos);
	m_worldDir.Load(worldDir);
	m_side.Load(side);

	s32 decalRenderSettingIndex;
	s32 decalRenderSettingCount;
	g_decalMan.FindRenderSettingInfo(DECALID_SNOWBALL, decalRenderSettingIndex, decalRenderSettingCount);

	m_decalId = g_decalMan.AddGeneric(DECALTYPE_GENERIC_COMPLEX_COLN, 
		decalRenderSettingIndex, decalRenderSettingCount, 
		VECTOR3_TO_VEC3V(worldPos), -VECTOR3_TO_VEC3V(worldDir), VECTOR3_TO_VEC3V(side), 
		1.0f, 1.0f, 0.3f, 
		-1.0f, 0.0f, 
		1.0f, 1.0f, 0.0f, 
		Color32(1.0f, 1.0f, 1.0f, 1.0f), 
		false, false, 
		0.0f, 
		false, false
		REPLAY_ONLY(, 0.0f));
}


#endif // GTA_REPLAY
