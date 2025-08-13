#ifndef _GLASSPANE_H_
#define _GLASSPANE_H_

//---

#include "glassPaneSyncing/GlassPaneInfo.h"
#include "fwnet/NetGameObjectBase.h"
#include "Network/Objects/Entities/NetObjGame.h"
#include "network/Objects/Entities/NetObjGlassPane.h"

//---

class CObject;

//---

class CGlassPane : public netGameObjectBase
{
	friend class CGlassPaneManager;

private:

	// if we don't have the geometry in memory we use the proxy data set on creation instead...
	struct GlassPaneProxy
	{
		GlassPaneProxy()
		:
			m_pos(VEC3_ZERO),
			m_radius(0.0f),
			m_isInside(false),
			m_geomHash(0)
		{}

		void Reset(void)
		{
			m_pos.Zero();
			m_geomHash	= 0;
			m_radius	= 0.0f;
			m_isInside	= false;
		}

		u32		m_geomHash;	
		Vector3 m_pos;
		float	m_radius;
		bool	m_isInside;
	};

public:

	// The maximum number of glass panes
	static const s32 MAX_PANES = 13;

#if GLASS_PANE_SYNCING_ACTIVE
	FW_REGISTER_CLASS_POOL(CGlassPane);
#endif /* GLASS_PANE_SYNCING_ACTIVE */

	CGlassPane();
	
	~CGlassPane();

	void					SetGeometry(CObject const* object);
	inline CObject const*	GetGeometry(void) const						{ return m_geometry;		}

	Vector3::Return			GetDummyPosition(void)			const;
	bool					GetIsInInterior(void)			const;
	float					GetRadius(void)					const;
	u32						GetGeometryHash(void)			const;

	inline bool				IsReserved(void) const						{ return m_reserved;		}
	inline void				SetReserved(bool const reserved)			{ m_reserved = reserved;	}
	void					Reset(void);

	Vector2 const&			GetHitPositionOS(void) const				{ return m_hitPosOS;		}
	u8						GetHitComponent(void) const					{ return m_hitComponent;	}

	inline void				SetHitPositionOS(Vector2 const& posOS)		{ m_hitPosOS = posOS;		}
	inline void				SetHitComponent(u8 const comp)				{ m_hitComponent = comp;	}

	void					ApplyDamage(void);

protected:

	inline void				SetProxyGeometryHash(u32 const hash)			{ m_geometryProxy.m_geomHash	= hash;		}
	inline void				SetProxyPosition(Vector3 const& pos)			{ m_geometryProxy.m_pos			= pos;		}
	inline void				SetProxyIsInside(bool const isInside)			{ m_geometryProxy.m_isInside	= isInside;	}
	inline void				SetProxyRadius(float const radius)				{ m_geometryProxy.m_radius		= radius;	}

	// Not allowed to copy construct or assign
	CGlassPane(const CGlassPane &);
	CGlassPane &operator=(const CGlassPane &);

private:

	// Purely used for debug rendering - we only deal 
	// with the dummy position of an object (i.e windows can't move)....
	Vector3::Return			GetGeometryPosition(void)		const;

	// declare as a network object
	NETWORK_OBJECT_PTR_DECL_EX(CNetObjGame);
	NETWORK_OBJECT_TYPE_DECL( CNetObjGlassPane, NET_OBJ_TYPE_GLASS_PANE );

	RegdObj					m_geometry;
	GlassPaneProxy			m_geometryProxy;
	Vector2					m_hitPosOS;
	u8						m_hitComponent;
	bool					m_reserved;
};

//---

#endif /* _GLASSPANE_H_ */