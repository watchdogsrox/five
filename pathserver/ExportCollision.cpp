#include "PathServer\ExportCollision.h"

#if NAVMESH_EXPORT

#include "scene\Entity.h"

CNavMeshDataExporterInterface* CNavMeshDataExporter::sm_ActiveExporter = NULL;

CNavMeshDataExporter::eExportMode CNavMeshDataExporter::m_eExportMode = CNavMeshDataExporter::eNotExporting;

bool CNavMeshDataExporter::WillExportCollision()
{
	if(m_eExportMode == eMapExport || m_eExportMode == eWaitingToStartExport)
	{
		return true;
	}
	return false;
}

#if __DEV
bool CNavMeshDataExporter::IsNamedModel(CEntity * pEntity)
{
	static const char * pTargetName = "Prop_BoxPile_10A";
	static Vector3 vTargetPos(-610.3f, -1628.0f, 32.9f);
	static float fEps = 2.0f;

	if(pEntity && pEntity->IsArchetypeSet())
	{
		const char * pEntityName = pEntity->GetModelName();

		if(stricmp(pEntityName, pTargetName)==0)
		{
			Vector3 vEntityPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
			if(vEntityPos.IsClose(vTargetPos, fEps))
			{
				return true;
			}
		}
	}
	return false;
}
#endif

#endif	// NAVMESH_EXPORT
