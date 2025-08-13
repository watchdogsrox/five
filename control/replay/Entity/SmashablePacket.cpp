//========================================================================================================================
// name:		SmashablePacket.cpp
// description:	Smashable replay packet.
//========================================================================================================================

#include "control/replay/Entity/SmashablePacket.h"

#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "vehicles/vehicle.h"


//////////////////////////////////////////////////////////////////////////
CPacketSmashableExplosion::CPacketSmashableExplosion(CEntity* pEntity, Vec3V& rPos, float fRadius, float fForce, bool bRemoveDetachedPolys)
	: CPacketEvent(PACKETID_SMASHABLEEXPLOSION, sizeof(CPacketSmashableExplosion))
	, CPacketEntityInfo()
{
	REPLAY_CHECK(pEntity != NULL, NO_RETURN_VALUE, "Entity cannot be NULL in CPacketSmashableExplosion");

	// History event
	m_uModelIndex = (u16)pEntity->GetModelIndex();
	m_fRadius = fRadius;
	m_fForce = fForce;
	m_bRemoveDetachedPolys = bRemoveDetachedPolys;

	m_Position.Store(VEC3V_TO_VECTOR3(rPos));
}


//////////////////////////////////////////////////////////////////////////
void CPacketSmashableExplosion::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	REPLAY_CHECK(pEntity != NULL, NO_RETURN_VALUE, "Entity cannot be NULL in CPacketSmashableExplosion");
	if (pEntity)
	{
		Vector3 oPosition;
		m_Position.Load(oPosition);

		VfxExpInfo_s expInfo;
		expInfo.regdEnt = pEntity;
		expInfo.vPositionWld = VECTOR3_TO_VEC3V(oPosition);
		expInfo.radius = m_fRadius;
		expInfo.force = m_fForce;
		expInfo.flags = VfxExpInfo_s::EXP_FORCE_DETACH | (m_bRemoveDetachedPolys ? VfxExpInfo_s::EXP_REMOVE_DETATCHED : 0);

		g_vehicleGlassMan.StoreExplosion(expInfo);
	}
}




//////////////////////////////////////////////////////////////////////////
CPacketSmashableCollision::CPacketSmashableCollision(CEntity* pEntity, Vec3V& rPos, Vec3V& rNormal, Vec3V& rDirection, u32 uMaterialID, s32 uCompID, u8 uWeaponGrp, float fForce, bool bIsBloody, bool bIsWater, bool bIsHeli)
	: CPacketEvent(PACKETID_SMASHABLECOLLISION, sizeof(CPacketSmashableCollision))
	, CPacketEntityInfo()
{
	//todo4five temp as these are causing issues in the final builds
	(void) bIsHeli;

	REPLAY_CHECK(pEntity != NULL, NO_RETURN_VALUE, "Entity cannot be NULL in CPacketSmashableCollision");

	// History event
	m_uModelIndex = (u16)pEntity->GetModelIndex();
	m_fForce = fForce;

	m_bIsBloody = bIsBloody;
	m_bIsWater = bIsWater;

	replayAssert(uCompID < 0xFFFF);
	m_uCompID = (u16)uCompID;
	m_uWeaponGrp = uWeaponGrp;
	m_uMaterialID = uMaterialID;

	if (pEntity->GetIsTypeVehicle() && ((CVehicle*)pEntity)->GetVehicleType() == VEHICLE_TYPE_HELI)
	{
		replayAssert(bIsHeli);
	}

	m_Position.Store(VEC3V_TO_VECTOR3(rPos));
	m_normal.Store(VEC3V_TO_VECTOR3(rNormal));
	m_direction.Store(VEC3V_TO_VECTOR3(rDirection));
}


//////////////////////////////////////////////////////////////////////////
void CPacketSmashableCollision::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	REPLAY_CHECK(pEntity != NULL, NO_RETURN_VALUE, "Entity cannot be NULL in CPacketSmashableCollision");
	if (pEntity)
	{
		Vector3 oPosition, oNormal, oDirection;
		m_Position.Load(oPosition);
		m_normal.Load(oNormal);
		m_direction.Load(oDirection);

		VfxCollInfo_s collInfo;
		collInfo.regdEnt = pEntity;
		collInfo.vPositionWld = VECTOR3_TO_VEC3V(oPosition);
		collInfo.vNormalWld = VECTOR3_TO_VEC3V(oNormal);
		collInfo.vDirectionWld = VECTOR3_TO_VEC3V(oDirection);
		collInfo.materialId = m_uMaterialID;
		collInfo.componentId = m_uCompID;

		replayAssert(collInfo.vNormalWld.GetXf()!=0.0f || collInfo.vNormalWld.GetYf()!=0.0f || collInfo.vNormalWld.GetZf()!=0.0f);

		collInfo.weaponGroup = (eWeaponEffectGroup)m_uWeaponGrp;
		collInfo.force = m_fForce;
		collInfo.isBloody = m_bIsBloody;
		collInfo.isWater = m_bIsWater;
		collInfo.isExitFx = false;
		collInfo.noPtFx = false;
		collInfo.noPedDamage = false;
		collInfo.noDecal = false; 
		collInfo.isSnowball = false;

		// try to smash the object
		g_vehicleGlassMan.StoreCollision(collInfo);
	}
}

#endif // GTA_REPLAY
