#ifndef EXPORT_COLLISION_H
#define EXPORT_COLLISION_H

#include "PathServer\NavGenParam.h"
#include "fwscene\stores\boxstreamersearch.h"
#include "fwutil\PtrList.h"
#include "spatialdata\aabb.h"

class CEntity;
class CPhysical;

namespace rage
{
	class phArchetype;
}

#if NAVMESH_EXPORT

class CNavMeshDataExporterInterface
{
public:
	virtual ~CNavMeshDataExporterInterface() {}

	virtual bool EntityIntersectsCurrentNavMesh(CEntity* pEntity) const = 0;
	virtual bool ExtentsIntersectCurrentNavMesh(Vec3V_In vMin, Vec3V_In vMax) const = 0;
	virtual bool GetCurrentNavMeshExtents(rage::spdAABB & outAABB) const = 0;
	virtual bool IsAlreadyInPhysicsLevel(CPhysical* pPhysical, phArchetype* pArchetype = NULL) const = 0;
	virtual void AppendToActiveBuildingsArray(atArray<CEntity*>& entityArray) = 0;
	virtual void AddSearches(atArray<fwBoxStreamerSearch>& searchList) = 0;
};

class CNavMeshDataExporter
{
public:
	enum eExportMode
	{
		eNotExporting,
		eWaitingToStartExport,
		eMapExport
	};

	static bool WillExportCollision();

	static bool IsExportingCollision() { return m_eExportMode == eMapExport; }

	static bool EntityIntersectsCurrentNavMesh(CEntity* pEntity)
	{	return GetActiveExporter()->EntityIntersectsCurrentNavMesh(pEntity);	}

	static bool ExtentsIntersectCurrentNavMesh(Vec3V_In vMin, Vec3V_In vMax)
	{	return GetActiveExporter()->ExtentsIntersectCurrentNavMesh(vMin, vMax);	}

	static bool GetCurrentNavMeshExtents(rage::spdAABB & outAABB)
	{	return GetActiveExporter()->GetCurrentNavMeshExtents(outAABB); }

	static bool IsAlreadyInPhysicsLevel(CPhysical* pPhysical, phArchetype* pArchetype = NULL)
	{	return GetActiveExporter()->IsAlreadyInPhysicsLevel(pPhysical, pArchetype);	}

	static eExportMode GetExportMode()
	{	return m_eExportMode;	}

	static void SetExportMode(eExportMode eMode)
	{	m_eExportMode = eMode;	}

	static CNavMeshDataExporterInterface *GetActiveExporter()
	{	return sm_ActiveExporter;	}

	static void SetActiveExporter(CNavMeshDataExporterInterface *pExporter)
	{	sm_ActiveExporter = pExporter;	}

	static void AddSearches(atArray<fwBoxStreamerSearch>& searchList)
	{
		GetActiveExporter()->AddSearches(searchList);
	}
#if __DEV
	static bool IsNamedModel(CEntity * pEntity);
#endif

protected:
	// The current export mode (main map, or interiors)
	static eExportMode m_eExportMode;

	static CNavMeshDataExporterInterface*	sm_ActiveExporter;
};


#endif	// NAVMESH_EXPORT

#endif	// EXPORT_COLLISION_H
