#ifndef _GLASSPANEMANAGER_H_
#define _GLASSPANEMANAGER_H_

// Rage headers
#include "atl/array.h"
#include "physics/WorldProbe/worldprobe.h"
#include "physics/WorldProbe/shapetestresults.h"
#include "glassPaneSyncing/GlassPaneInfo.h"
#include "glassPaneSyncing/GlassPane.h"

#define DEBUG_RENDER_GLASS_PANE_MANAGER 0

class CGlassPaneManager
{
public:

	struct GlassPaneRegistrationData
	{
		GlassPaneRegistrationData()
		:
			m_object(NULL),
			m_hitPos(VEC3_ZERO),
			m_hitComponent(0)
		{}

		GlassPaneRegistrationData(CObject* object, Vector3 const & hitPos, u8 const hitComponent)
		:
			m_object(object),
			m_hitPos(hitPos),
			m_hitComponent(hitComponent)
		{}

		RegdObj			m_object;
		Vector3			m_hitPos;
		u8				m_hitComponent;
	};

	struct GlassPaneInitData
	{
		GlassPaneInitData(Vector3 const& pos, u32 const geomHash, float const radius, bool const isInside, Vector2 const& hitPosOS, u8 const hitComponent)
		:
			m_pos(pos),
			m_radius(radius),
			m_isInside(isInside),
			m_hitPosOS(hitPosOS),
			m_hitComponent(hitComponent),
			m_geomHash(geomHash)
		{}

		Vector3 m_pos;
		u32		m_geomHash;
		float	m_radius;
		bool	m_isInside;
		Vector2 m_hitPosOS;
		u8		m_hitComponent;
	};


	static const s32 MAX_NUM_GLASS_PANE_OBJECTS				 = 50;	
	static const s32 MAX_NUM_GLASS_PANE_POTENTIALS_PER_FRAME = 20;	

	static void					Init(unsigned initMode);
	static void					Shutdown(unsigned shutdownMode);
	static void					Update();	

	static void					InitPools();
	static void					ShutdownPools();

	static bool					DestroyGlassPane(CGlassPane* glassPane);
	static bool					MigrateGlassPane(CGlassPane const* glassPane);

	static bool					RegisterGlassGeometryObject_OnNetworkOwner(CObject const * object, Vector3 const& hitPos, u8 const hitComponent);
	static CGlassPane const*	RegisterGlassGeometryObject_OnNetworkClone(CObject const * object, GlassPaneInitData const& glassPaneInitData);
	
	static bool					RegisterGlassGeometryObject(CObject const* pObject);
	static bool					UnregisterGlassGeometryObject(CObject const* pObject);

#if __BANK
	static void					DebugPrintRegisteredGlassGeometryObjects();
#endif /* __BANK */

	static bool					IsGeometryObjectRegistered(CObject const * object);
	static bool					IsGeometryRegisteredWithGlassPane(CObject const * object);
	static bool					IsGeometryPendingRegistrationWithGlassPane(CObject const * object);
	static bool					IsGlassPaneRegistered(CGlassPane const* glassPane);

	static CGlassPane const*	GetGlassPaneByIndex(u32 const index);
	static CGlassPane*			AccessGlassPaneByIndex(u32 const index);

	static CGlassPane const*	GetGlassPaneByGeometryHash(u32 const hash);
	static CGlassPane*			AccessGlassPaneByGeometryHash(u32 const hash);

	static CGlassPane const*	GetGlassPaneByObject(CObject const* object);
	static CGlassPane*			AccessGlassPaneByObject(CObject const* object);
	
private:

	static void					ProcessPotentialGlassPanes();

	static CGlassPane*			CreateGlassPane(void);

	static Vector2				ConvertWorldToGlassSpace(CObject const* object, Vector3 const& posWS);

public:

	static u32					GenerateHash(Vector3 const& pos);
	static CObject*				GetGeometryObject(u32 const hash);

private:

#if !__FINAL && DEBUG_RENDER_GLASS_PANE_MANAGER

	static void RenderPanes(void);
	static void RenderGeometry(void);

#endif /* !__FINAL && DEBUG_RENDER_GLASS_PANE_MANAGER */

#if GLASS_PANE_SYNCING_ACTIVE

	static atMap<u32, RegdObj> sm_glassPaneGeometryObjects;
	static atFixedArray<CGlassPane, MAX_NUM_GLASS_PANE_OBJECTS> sm_glassPanes;
	static atFixedArray<GlassPaneRegistrationData, MAX_NUM_GLASS_PANE_POTENTIALS_PER_FRAME> sm_potentialGlassPanes;

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	typedef atMap<u32, RegdObj>::Iterator		geometry_itor;
	typedef atMap<u32, RegdObj>::ConstIterator	geometry_const_itor;

	typedef atFixedArray<GlassPaneRegistrationData, MAX_NUM_GLASS_PANE_POTENTIALS_PER_FRAME>::iterator potential_itor;
	typedef atFixedArray<GlassPaneRegistrationData, MAX_NUM_GLASS_PANE_POTENTIALS_PER_FRAME>::const_iterator const_potential_itor;
};

#endif /* _GLASSPANEMANAGER_H_ */