#ifndef	__MAPEDITRESTINTERFACE_H_
#define	__MAPEDITRESTINTERFACE_H_

#if __BANK

#include "atl/array.h"
#include "vector/vector2.h"
#include "vector/vector3.h"
#include "renderer/PostScan.h"
#include "scene/Entity.h"
#include "fwscene/stores/mapdatastore.h"

#include "scene/lod/AttributeDebug.h"

class CLodDebugEntityDelete
{
public:

	CLodDebugEntityDelete() : m_hash(0), m_guid(0) {}

	CLodDebugEntityDelete(CEntity* pEntity) 
	{
		if (pEntity)
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			m_hash = pModelInfo->GetModelNameHash();
			m_modelName = pModelInfo->GetModelName();
			m_posn = pEntity->GetTransform().GetPosition();

			strLocalIndex imapIndex = strLocalIndex(pEntity->GetIplIndex());
			if (imapIndex.Get())
			{
				m_imapName = fwMapDataStore::GetStore().GetName(imapIndex);
			}
			else
			{
				m_imapName = "";
			}

			if (!pEntity->GetGuid(m_guid))
				m_guid = 0;
		}
	}

	inline bool operator ==(const CLodDebugEntityDelete& rhs) const
	{
		return m_hash==rhs.m_hash && ( IsEqualAll(m_posn, rhs.m_posn) != 0 );
	}
	inline bool operator !=(const CLodDebugEntityDelete& rhs) const
	{
		return m_hash!=rhs.m_hash || ( IsEqualAll(m_posn, rhs.m_posn) == 0 );
	}

	u32 m_hash;
	Vec3V m_posn;
	u32 m_guid;
	atHashString m_imapName;
	atHashString m_modelName;

	PAR_SIMPLE_PARSABLE;
};

class CLodDebugLodOverride
{
public:

	CLodDebugLodOverride() : m_hash(0), m_distance(0.0f), m_guid(0) {}

	CLodDebugLodOverride(CEntity* pEntity, float fDist)
	{
		if (pEntity)
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			m_hash = pModelInfo->GetModelNameHash();
			m_modelName = pModelInfo->GetModelName();
			m_posn = pEntity->GetTransform().GetPosition();
			m_distance = fDist;

			strLocalIndex imapIndex = strLocalIndex(pEntity->GetIplIndex());
			if (imapIndex.Get())
			{
				m_imapName = fwMapDataStore::GetStore().GetName(imapIndex);
			}
			else
			{
				m_imapName = "";
			}

			if (!pEntity->GetGuid(m_guid))
				m_guid = 0;
		}
	}

	inline bool operator ==(const CLodDebugLodOverride& rhs) const
	{
		return m_hash==rhs.m_hash && ( IsEqualAll(m_posn, rhs.m_posn) != 0 );
	}
	inline bool operator !=(const CLodDebugLodOverride& rhs) const
	{
		return m_hash!=rhs.m_hash || ( IsEqualAll(m_posn, rhs.m_posn) == 0 );
	}

	u32 m_hash;
	Vec3V m_posn;
	float m_distance;
	u32 m_guid;
	atString m_imapName;
	atHashString m_modelName;

	PAR_SIMPLE_PARSABLE;
};

class CAttributeDebugAttribOverride
{
public:

	CAttributeDebugAttribOverride() : m_hash(0), m_guid(0) {}

	CAttributeDebugAttribOverride(CEntity* pEntity, TweakerAttributeContainer &attribs, bool bStreamingPriorityLow, int priority)
	{
		if (pEntity)
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			m_hash = pModelInfo->GetModelNameHash();
			m_modelName = pModelInfo->GetModelName();
			m_posn = pEntity->GetTransform().GetPosition();

			m_bDontCastShadows = attribs.bDontCastShadows;
			m_bDontRenderInShadows = attribs.bDontRenderInShadows;
			m_bDontRenderInReflections = attribs.bDontRenderInReflections;
			m_bOnlyRenderInReflections = attribs.bOnlyRenderInReflections;
			m_bDontRenderInWaterReflections = attribs.bDontRenderInWaterReflections;
			m_bOnlyRenderInWaterReflections = attribs.bOnlyRenderInWaterReflections;
			m_bDontRenderInMirrorReflections = attribs.bDontRenderInMirrorReflections;
			m_bOnlyRenderInMirrorReflections = attribs.bOnlyRenderInMirrorReflections;

			m_bStreamingPriorityLow = bStreamingPriorityLow;
			m_iPriority = priority;

			strLocalIndex imapIndex = strLocalIndex(pEntity->GetIplIndex());
			if (imapIndex.Get())
			{
				m_imapName = fwMapDataStore::GetStore().GetName(imapIndex);
			}
			else
			{
				m_imapName = "";
			}

			if (!pEntity->GetGuid(m_guid))
				m_guid = 0;
		}
	}

	inline bool operator ==(const CAttributeDebugAttribOverride& rhs) const
	{
		return m_hash==rhs.m_hash && ( IsEqualAll(m_posn, rhs.m_posn) != 0 );
	}
	inline bool operator !=(const CAttributeDebugAttribOverride& rhs) const
	{
		return m_hash!=rhs.m_hash || ( IsEqualAll(m_posn, rhs.m_posn) == 0 );
	}

	u32		m_hash;
	Vec3V	m_posn;
	bool	m_bDontCastShadows;
	bool	m_bDontRenderInShadows;
	bool	m_bDontRenderInReflections;
	bool	m_bOnlyRenderInReflections;
	bool	m_bDontRenderInWaterReflections;
	bool	m_bOnlyRenderInWaterReflections;
	bool	m_bDontRenderInMirrorReflections;
	bool	m_bOnlyRenderInMirrorReflections;
	bool	m_bStreamingPriorityLow;
	int		m_iPriority;
	u32		m_guid;
	atString m_imapName;
	atHashString m_modelName;

	PAR_SIMPLE_PARSABLE;
};

class CEntityMatrixOverride
{
public:

	CEntityMatrixOverride() : m_hash(0), m_guid(0) {}

	CEntityMatrixOverride(CEntity* pEntity)
	{
		if (pEntity)
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			m_hash = pModelInfo->GetModelNameHash();
			m_modelName = pModelInfo->GetModelName();
			m_matrix = pEntity->GetMatrix();

			strLocalIndex imapIndex = strLocalIndex(pEntity->GetIplIndex());
			if (imapIndex.Get())
			{
				m_imapName = fwMapDataStore::GetStore().GetName(imapIndex);
			}
			else
			{
				m_imapName = "";
			}

			if (!pEntity->GetGuid(m_guid))
				m_guid = 0;
		}
	}

	inline bool operator ==(const CEntityMatrixOverride& rhs) const
	{
		return m_hash==rhs.m_hash && m_guid==rhs.m_guid;
	}
	inline bool operator !=(const CEntityMatrixOverride& rhs) const
	{
		return m_hash!=rhs.m_hash || m_guid!=rhs.m_guid;
	}

	u32 m_hash;
	u32 m_guid;
	atString m_imapName;
	atHashString m_modelName;
	Mat34V m_matrix;

	PAR_SIMPLE_PARSABLE;
};

class COutputGameCameraState
{
public:
	Vec3V m_vPos;
	Vec3V m_vFront;
	Vec3V m_vUp;
	float m_fov;

	PAR_SIMPLE_PARSABLE;
};

class CMapEditRESTInterface
{
public:
	CMapEditRESTInterface() : m_bInitialised(false) {}
	void	Init();
	void	InitIfRequired();
	void	Update();

	atArray<CLodDebugLodOverride> m_distanceOverride;
	atArray<CLodDebugLodOverride> m_childDistanceOverride;
	atArray<CLodDebugEntityDelete> m_mapEntityDeleteList;
	atArray<CAttributeDebugAttribOverride> m_attributeOverride;
	atArray<CEntityMatrixOverride> m_entityMatrixOverride;
	COutputGameCameraState m_gameCamera;

	PAR_SIMPLE_PARSABLE;

private:
	bool m_bInitialised;
};

extern	CMapEditRESTInterface g_MapEditRESTInterface;


#endif	//__BANK

#endif	//__MAPEDITRESTINTERFACE_H_
