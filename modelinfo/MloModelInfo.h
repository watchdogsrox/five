//
// filename:	MloModelInfo.h
// description:	Class description of a MLO model type. A MLO (Multi Level Object, see aarong) is an object
//				built up from instances of other objects
//

#ifndef INC_MLOMODELINFO_H_
#define INC_MLOMODELINFO_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "vector/vector3.h"
#include "vector/quaternion.h"
#include "fwscene/mapdata/mapdata.h"
#include "fwscene/world/InteriorLocation.h"

// Game headers
#include "modelinfo/basemodelinfo.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "scene/loader/MapData_Interiors.h"

// --- Defines ------------------------------------------------------------------
#define MAX_ENTITIES_PER_PORTAL		(4)

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CMloInstanceDef;
class CEntityDef;

//class fwEntityDef;
class CMloRoomDef;
class CMloPortalDef;
class CMloEntitySet;

inline u32 SetFlag(u32 flags, u32 bit, bool set) { return (set ? (flags | bit) : (flags & ~bit)); }

class CPortalFlags{
public:
	CPortalFlags() { m_flags = 0; }
	CPortalFlags(u32 val) { m_flags = val; }
	~CPortalFlags() {}

	CPortalFlags& operator=(const CPortalFlags& other) { m_flags = other.m_flags; return *this; }

	void SetIsOneWayPortal(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_ONE_WAY, b); }
	bool GetIsOneWayPortal(void) const {return (m_flags & INTINFO_PORTAL_ONE_WAY) != 0; }

	void SetIsLinkPortal(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_LINK, b); }
	bool GetIsLinkPortal(void) const {return (m_flags & INTINFO_PORTAL_LINK) != 0; }

	void SetIsMirrorPortal(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_MIRROR, b); }
	bool GetIsMirrorPortal(void) const {return (m_flags & INTINFO_PORTAL_MIRROR) != 0; }

	void SetMirrorCanSeeDirectionalLight(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_MIRROR_CAN_SEE_DIRECTIONAL_LIGHT, b); }
	bool GetMirrorCanSeeDirectionalLight(void) const {return (m_flags & INTINFO_PORTAL_MIRROR_CAN_SEE_DIRECTIONAL_LIGHT) != 0;}

	void SetMirrorCanSeeExteriorView(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_MIRROR_CAN_SEE_EXTERIOR_VIEW, b); }
	bool GetMirrorCanSeeExteriorView(void) const {return (m_flags & INTINFO_PORTAL_MIRROR_CAN_SEE_EXTERIOR_VIEW) != 0;}

	void SetIsMirrorUsingPortalTraversal(bool b) { m_flags = SetFlag(m_flags, INTINFO_PORTAL_MIRROR_USING_PORTAL_TRAVERSAL, b); }
	bool GetIsMirrorUsingPortalTraversal(void) const {return (m_flags & INTINFO_PORTAL_MIRROR_USING_PORTAL_TRAVERSAL) != 0;}

	void SetIsMirrorFloor(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_MIRROR_FLOOR, b); }
	bool GetIsMirrorFloor(void) const {return (m_flags & INTINFO_PORTAL_MIRROR_FLOOR) != 0; }

	void SetIsWaterSurface(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_WATER_SURFACE, b); }
	bool GetIsWaterSurface(void) const {return (m_flags & INTINFO_PORTAL_WATER_SURFACE) != 0; }

	void SetIgnoreModifier(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_IGNORE_MODIFIER, b); }
	bool GetIgnoreModifier(void) const {return (m_flags & INTINFO_PORTAL_IGNORE_MODIFIER) != 0; }

	void SetLowLODOnly(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_LOWLODONLY, b); }
	bool GetLowLODOnly(void) const {return (m_flags & INTINFO_PORTAL_LOWLODONLY) != 0; }

	void SetAllowClosing(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_ALLOWCLOSING, b); }
	bool GetAllowClosing(void) const {return (m_flags & INTINFO_PORTAL_ALLOWCLOSING) != 0;}

	void SetUseLightBleed(bool b){ m_flags = SetFlag(m_flags, INTINFO_PORTAL_USE_LIGHT_BLEED, b); }
	bool GetUseLightBleed(void) const {return (m_flags & INTINFO_PORTAL_USE_LIGHT_BLEED) != 0;}

	u32	m_flags;
};


class CMloModelInfo : public CBaseModelInfo
{
public:
	CMloModelInfo();
	virtual ~CMloModelInfo();

	virtual void Init();
	virtual void InitArchetypeFromDefinition(strLocalIndex mapTypeDefIndex, fwArchetypeDef* definition, bool bHasAssetReferences);

	virtual fwEntity* CreateEntity();
	
	virtual void Shutdown();

	void SetStartInstance(u32 start) {m_MLOInstanceBlockOffset = start; }		// offset into MLOinstances storage for this .ityp file
	void SetMapTypeDefIndex(s32 index) { m_mapFileDefIndex = index; }
	void SetNumMLOInstances(u32 num) { m_numMLOInstances = num; }

	s32 GetMapTypeDefIndex() const { return m_mapFileDefIndex; }
	u32 GetNumInstances() const { return(m_numMLOInstances); }

	void RequestObjectsInRoom(const char* pRoomName, u32 flags);
	bool HaveObjectsInRoomLoaded(const char* pRoomName);
	void SetMissionDoesntRequireRoom(const char* pRoomName);

	u32				GetMLOFlags() const		{ return(m_mloFlags); }
	u32				GetRoomFlags(u32 roomIdx) const;
	CPortalFlags	GetPortalFlags(u32 roomIdx) const;
	void			GetPortalData(u32 portalIdx, u32& roomIdx1, u32& roomIdx2, CPortalFlags& flags) const;
	void			GetRoomData(u32 roomIdx, atHashWithStringNotFinal& timeCycleName, atHashWithStringNotFinal& secondaryTimeCycleName, float& blend) const;
	void			GetRoomTimeCycleNames(u32 roomIdx, atHashWithStringNotFinal& timeCycleName, atHashWithStringNotFinal& secondaryTimeCycleName) const;
	inline float	GetRoomBlend(u32 roomIdx) const
	{
		modelinfoAssert(m_pRooms); 
		modelinfoAssert(roomIdx < m_pRooms->GetCount());

		return ((*m_pRooms)[roomIdx]).m_blend;
	}

#if __BANK
	const char* GetRoomName(fwInteriorLocation location) const;
#endif // __BANK

	s32	GetInteriorPortalIdx(u32 roomIdx, u32 roomPortalIdx) const;


	const atArray< fwEntityDef* >&	GetEntities(void)	{ return(*m_pEntities); }
	const atArray< CMloRoomDef >&	GetRooms(void) const{ return(*m_pRooms); }
	const atArray< CMloPortalDef >&	GetPortals(void) const{ return(*m_pPortals); }
	const atArray< CMloEntitySet >& GetEntitySets(void) { return(*m_pEntitySets); }
	atArray< CMloEntitySet >&		GetEntitySetsNC(void) { return(*m_pEntitySets); }
	const atArray< CMloTimeCycleModifier >& GetTimeCycleModifiers(void)	{ return(*m_pTimeCycleModifiers); }

	bool GetIsStreamType(void) const { return(m_bIsStreamType); }
	void SetIsStreamType(bool val) { m_bIsStreamType = val; }

	void GetAudioSettings(u32 roomIdx, const InteriorSettings*& audioSettings, const InteriorRoom*& audioRoom) const;
	void SetAudioSettings(u32 roomIdx, const InteriorSettings* audioSettings, const InteriorRoom* audioRoom);

	float	GetBoundingPortalRadius(void) const		{ return(m_boundingPortalRadius); }
protected:


	u32 m_MLOInstanceBlockOffset;
	u32 m_numMLOInstances;
	s32	m_mapFileDefIndex;

	u32		m_mloFlags;

	float	m_boundingPortalRadius;

	bool						m_bIsStreamType;

	//new stuff
	atArray< fwEntityDef*>		*m_pEntities;
	atArray< CMloRoomDef>       *m_pRooms;
	atArray< CMloPortalDef>		*m_pPortals;
	atArray< CMloEntitySet>		*m_pEntitySets;
	atArray< CMloTimeCycleModifier>	*m_pTimeCycleModifiers;

	class CInteriorAudioSettings{
	public:
		CInteriorAudioSettings() : 
			m_audioSettings(NULL), 
			m_audioRoom(NULL) 
			{}
		virtual ~CInteriorAudioSettings() {}

		const InteriorSettings*		m_audioSettings;
		const InteriorRoom*			m_audioRoom;
	};
	atArray< CInteriorAudioSettings >	m_cachedAudioSettings;
};

// --- Globals ------------------------------------------------------------------



#endif // !INC_MLOMODELINFO_H_
