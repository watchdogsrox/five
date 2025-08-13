//=====================================================================================================
// name:		ProjectedTexturePacket.h
// description:	Projected texture replay packet.
//=====================================================================================================

#ifndef INC_PROJTEXPACKET_H_
#define INC_PROJTEXPACKET_H_

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Effects/ParticlePacket.h"
#include "control/replay/ReplayEventManager.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "Control/Replay/PacketBasics.h"
#include "phcore/constants.h"
#include "phcore/MaterialMgr.h"
#include "vfx/decals/DecalCallbacks.h"
#include "vfx/systems/VfxTrail.h"

#include "vector/color32.h"

#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS

struct VfxCollInfo_s;
namespace rage
{class Vector3;}


//////////////////////////////////////////////////////////////////////////
class CPacketPedBloodDamage : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedBloodDamage(u16 component, const Vector3& pos, const Vector3& dir, u8 type, u32 bloodHash, bool forceNotStanding, const CPed* pPed, int damageBlitID);

	void Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PEDBLOODDAMAGE, "Validation of CPacketPedBloodDamage Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PEDBLOODDAMAGE;
	}

	bool	NeedsEntitiesForExpiryCheck() const		{ return true; }
	bool 	HasExpired(const CEventInfo<CPed>& info) const;

private:

	u16					m_component;
	CPacketVector<3>	m_position;
	CPacketVector<3>	m_direction;
	u8					m_type;
	u32					m_bloodHash;
	bool				m_forceNotStanding;

	mutable int			m_decalId;
};

//////////////////////////////////////////////////////////////////////////
class CPacketPedBloodDamageScript : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketPedBloodDamageScript(u8 zone, const Vector2 & uv, float rotation, float scale, atHashWithStringBank bloodNameHash, float preAge, int forcedFrame, int damageBlitID);

	void Extract(const CEventInfo<CPed>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CPed>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CPed>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PEDBLOODDAMAGESCRIPT, "Validation of CPacketPedBloodDamageScript Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PEDBLOODDAMAGESCRIPT;
	}

	bool	NeedsEntitiesForExpiryCheck() const		{ return true; }
	bool 	HasExpired(const CEventInfo<CPed>& info) const;

private:
	u8					m_Zone;
	CPacketVector<2>	m_Uv; 
	float				m_Rotation;
	float				m_Scale;
	u32					m_BloodNameHash;
	float				m_PreAge;
	s32					m_ForcedFrame;

	mutable int			m_decalId;
};




//////////////////////////////////////////////////////////////////////////
class CPacketPTexWeaponShot : public CPacketDecalEvent, public CPacketEntityInfoStaticAware<1>
{
public:
	CPacketPTexWeaponShot(const VfxCollInfo_s& collisionInfo, const bool forceMetal, const bool forceWater, const bool glass, const bool firstGlassHit, int decalID, const bool isSnowball);

	void				Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CEventInfo<CEntity>&) const;
	eShouldExtract		ShouldExtract(u32 playbackFlags, const u32 packetFlags, bool isFirstFrame) const;

	bool				NeedsEntitiesForExpiryCheck() const											{ return m_glass; }

	void				PrintXML(FileHandle handle) const	{	CPacketDecalEvent::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PTEXWEAPONSHOT, "Validation of CPacketPTexWeaponShot Failed!, 0x%08X", GetPacketID());
		return CPacketDecalEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_PTEXWEAPONSHOT;
	}

private:

	CPacketVector<3>	m_position;											// the entity space position where the collision occurred
	CPacketVector<3>	m_normal;												// the entity space normal of the collision
	CPacketVector<3>	m_direction;											// the entity space direction of the collision
	Id					m_materialId;												// the material on the entity where the collision occurred
	s32					m_componentId;											// the component on the entity where the collision occurred
	float				m_force;													// the force of the collision
	u8					m_weaponGroup;											// the weapon group that caused the collision (if any)

	bool				m_isBloody		: 1;												// set if there is a nearby headshot so a bloody window smash texture can be applied
	bool				m_isWater		: 1;												// set if the collision is with water so that the water fx group can override the actual one (splashes)
	bool				m_isExitFx		: 1;
	bool 				m_forceMetal	: 1;

	bool 				m_forceWater	: 1;
	bool				m_glass			: 1;
	bool				m_firstGlassHit	 : 1;					
	bool				m_forcedGlassHit : 1;

	// Moving added members to the end of the packet for version compatibility
	s32					m_roomId;												//room ID where the hit occured (for interiors)

	u8					m_isSnowball	: 1;
	u8					m_armouredGlassShotByPedInsideVehicle : 1;
	u8					m_isFMJAmmo		: 1;
	u8					m_unused		: 5;

	float				m_armouredGlassWeaponDamage;
};


//////////////////////////////////////////////////////////////////////////
class CPacketAddScriptDecal : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketAddScriptDecal(const Vector3& pos, const Vector3& dir, const Vector3& side, Color32 color, int renderSettingId, float width, float height, float totalLife, DecalType_e decalType, bool isDynamic, int decalID);
	void				Extract(CTrackedEventInfo<tTrackedDecalType>& eventInfo) const;
	ePreloadResult		Preload(const CTrackedEventInfo<tTrackedDecalType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CTrackedEventInfo<tTrackedDecalType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CTrackedEventInfo<tTrackedDecalType>&) const;
	bool				Setup(bool)										{	return true;	}

	bool				ShouldStartTracking() const						{	return true;	}

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SCRIPTDECALADD, "Validation of CPacketAddScriptDecal Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SCRIPTDECALADD;
	}

private:

	CPacketVector<3>	m_pos;
	CPacketVector<3>	m_dir;
	CPacketVector<3>	m_side;
	Color32				m_color;
	int					m_renderSettingId;
	float				m_width;
	float				m_height;
	float				m_totalLife;
	DecalType_e			m_decalType;
	bool				m_isDynamic		: 1;
	bool								: 7;

	mutable int			m_decalId;
};


//////////////////////////////////////////////////////////////////////////
class CPacketRemoveScriptDecal : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketRemoveScriptDecal();
	void				Extract(CTrackedEventInfo<tTrackedDecalType>& eventInfo) const;
	ePreloadResult		Preload(const CTrackedEventInfo<tTrackedDecalType>&) const			{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CTrackedEventInfo<tTrackedDecalType>&) const			{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CTrackedEventInfo<tTrackedDecalType>&) const		{	return true;	}
	bool				Setup(bool)										{	return false;	}

	bool				ShouldEndTracking() const						{	return true;	}

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SCRIPTDECALREMOVE, "Validation of CPacketAddScriptDecal Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SCRIPTDECALREMOVE;
	}
};

//=====================================================================================================
class CPacketPTexFootPrint : public CPacketDecalEvent, public CPacketEntityInfo<0>
{
public:
	CPacketPTexFootPrint(s32 settingIndex, s32 settingCount, float width, float length,	Color32 col, Id	mtlId, const Vector3& footStepPos, const Vector3& footStepUp, const Vector3& footStepSide, bool isLeft, float decalLife);

	void Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<void>&) const	
	{
		replayDebugf1("********************************************");
		replayDebugf1("*********Preload CPacketPTexFootPrint...************");
		replayDebugf1("********************************************");
		 return PRELOAD_SUCCESSFUL; 
	}
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PTEXFOOTPRINT, "Validation of CPacketPTexFootPrint Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PTEXFOOTPRINT;
	}

protected:

	s32					m_decalRenderSettingIndex;
	s32					m_decalRenderSettingCount;
	float				m_decalWidth;
	float				m_decalLength;
	Color32				m_col; 
	Id					m_mtlId;
	CPacketVector3		m_vFootStepPos; 
	CPacketVector3		m_vFootStepUp;
	CPacketVector3 		m_vFootStepSide; 
	bool				m_isLeft; 
	float				m_decalLife;
};

//////////////////////////////////////////////////////////////////////////
class CPacketPTexMtlBang : public CPacketDecalEvent, public CPacketEntityInfoStaticAware<1>
{
public:
	CPacketPTexMtlBang(const VfxCollInfo_s& vfxCollInfo, phMaterialMgr::Id tlIdB, CEntity* pEntity, int decalId, eReplayPacketId packetId = PACKETID_PTEXMTLBANG);

	void Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PTEXMTLBANG, "Validation of CPacketPTexMtlBang Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_PTEXMTLBANG;
	}

private:
	CPacketVector<3>				m_vPositionWld;
	CPacketVector<3>				m_vNormalWld;

	phMaterialMgr::Id				m_MaterialId;
	phMaterialMgr::Id				m_tlIdB;

	s32								m_ComponentId;
	float							m_Force;
};

//////////////////////////////////////////////////////////////////////////
class CPacketPTexMtlBang_PlayerVeh : public CPacketPTexMtlBang
{
public:
	CPacketPTexMtlBang_PlayerVeh(const VfxCollInfo_s& vfxCollInfo, phMaterialMgr::Id tlIdB, CEntity* pEntity, int decalId);

	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PTEXMTLBANG_PLAYERVEH, "Validation of CPacketPTexMtlBang_PlayerVeh Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_PTEXMTLBANG_PLAYERVEH;
	}

private:
};

//////////////////////////////////////////////////////////////////////////
class CPacketPTexMtlScrape : public CPacketDecalEvent, public CPacketEntityInfoStaticAware<1>
{
public:
	CPacketPTexMtlScrape(CEntity* pEntity, s32 renderSettingsIndex, Vec3V_In vPtA, Vec3V_InOut vPtB, Vec3V_In vNormal, float width, Color32 col, float life, u8 mtlId, float maxOverlayRadius, float duplicateRejectDist, int decalId, eReplayPacketId packetId = PACKETID_PTEXMTLSCRAPE);

	void Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PTEXMTLSCRAPE, "Validation of CPacketPTexMtlScrape Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_PTEXMTLSCRAPE;
	}

private:
	CPacketVector<3>				m_PtA;
	CPacketVector<3>				m_PtB;
	CPacketVector<3>				m_Normal;

	float							m_width;	
	float							m_life;
	float							m_maxOverlayRadius;
	float							m_duplicateRejectDist;

	Color32							m_col;
	s32								m_renderSettingsIndex;

	u8								m_mtlId;	
};

//For the players vehicle have a new packet thats high priority monitored so it wont get removed if the buffer gets full
class CPacketPTexMtlScrape_PlayerVeh : public CPacketPTexMtlScrape
{
public:
	CPacketPTexMtlScrape_PlayerVeh(CEntity* pEntity, s32 renderSettingsIndex, Vec3V_In vPtA, Vec3V_InOut vPtB, Vec3V_In vNormal, float width, Color32 col, float life, u8 mtlId, float maxOverlayRadius, float duplicateRejectDist, int decalId);

	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PTEXMTLSCRAPE_PLAYERVEH, "Validation of CPacketPTexMtlScrape Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_PTEXMTLSCRAPE_PLAYERVEH;
	}

private:

};

class CPacketDecalRemove : public CPacketEvent, public CPacketEntityInfoStaticAware<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketDecalRemove(int boneIndex);

	void Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DECALSREMOVE, "Validation of CPacketDecalRemove Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_DECALSREMOVE;
	}

protected:

	int		m_boneIndex;
};


class CPacketDisableDecalRendering : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketDisableDecalRendering();

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DISABLE_DECAL_RENDERING, "Validation of CPacketDisableDecalRendering Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DISABLE_DECAL_RENDERING;
	}
};




class CPacketAddSnowballDecal : public CPacketDecalEvent, public CPacketEntityInfo<0>
{
public:
	CPacketAddSnowballDecal(const Vec3V& worldPos, const Vec3V& worldDir, const Vec3V& side, int decalId);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketDecalEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ADDSNOWBALLDECAL, "Validation of CPacketAddSnowballDecal Failed!, 0x%08X", GetPacketID());
		return CPacketDecalEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_ADDSNOWBALLDECAL;
	}
private:
	CPacketVector3	m_worldPos;
	CPacketVector3	m_worldDir;
	CPacketVector3	m_side;
};







#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#endif // GTA_REPLAY

#endif // INC_PROJTEXPACKET_H_
