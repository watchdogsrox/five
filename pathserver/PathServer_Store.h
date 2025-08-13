#ifndef PATHSERVER_STORE_H
#define PATHSERVER_STORE_H

#include "ai/navmesh/datatypes.h"
#include "ai/navmesh/navmeshstore.h"
#include "scene/DataFileMgr.h"
#include "scene/RegdRefTypes.h"
#include "system/FileMgr.h"

//-----------------------------------------------------------------------------

class aiNavMeshStoreInterfaceGta : public fwNavMeshStoreInterface
{
public:
	virtual void ApplyAreaSwitchesToNewlyLoadedNavMesh(CNavMesh &mesh, aiNavDomain domain) const;
#if __HIERARCHICAL_NODES_ENABLED
	virtual void ApplyAreaSwitchesToNewlyLoadedNavNodes(CHierarchicalNavData &navNodes) const;
#endif
	virtual void FindVehicleForDynamicNavMesh(atHashString iNameHash, CModelInfoNavMeshRef &dynNavInf) const;
	virtual void NavMeshesHaveChanged() const;
	virtual bool IsDefragmentCopyBlocked() const;

	virtual void AdjoinNavmeshesAcrossMapSwap(CNavMesh * pMainMapMesh, CNavMesh * pSwapMesh, eNavMeshEdge iSideOfMainMapMesh) const;
	virtual void RestoreNavmeshesAcrossMapSwap(CNavMesh * pMainMapMesh, CNavMesh * pSwappedBackMesh, eNavMeshEdge iSideOfMainMapMesh) const;

	virtual void ApplyHacksToNewlyLoadedNavMesh(CNavMesh * pNavMesh, aiNavDomain domain) const;
public:
#if __BANK
	static bool ms_bDebugMapSwapUnmatchedAdjacencies;
#endif
};

//-----------------------------------------------------------------------------

template <typename T, typename S> bool ReadIndexMappingFile(aiMeshStore<T,S> &store, const CDataFileMgr::DataFileType dataFileType)
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(dataFileType);
	if(DATAFILEMGR.IsValid(pData))
	{
		FileHandle hFile = CFileMgr::OpenFile(pData->m_filename, "rt");

		Assertf(hFile!=INVALID_FILE_HANDLE, "Couldn't open mapping file \"%s\"", pData->m_filename);
		if(hFile==INVALID_FILE_HANDLE)
			return false;

		char * pLine = NULL;
		u32 iCount = 0, iNumIndices = 0;
		int iIndex;

		pLine = CFileMgr::ReadLine(hFile);

		if(pLine && sscanf(pLine, "NUMENTRIES %i", &iNumIndices))
		{
			Assertf(iNumIndices <= store.GetNumMeshesInAnyLevel(), "aiMeshStore::ReadIndexMappingFile() - Please increase the value of m_iNumMeshesInAnyLevel for \'%s\' image file to at least %i", store.GetModuleName(), iNumIndices);
			Assertf(iNumIndices <= store.GetMaxMeshIndex(), "aiMeshStore::ReadIndexMappingFile() - there are more indices that pool slots!");

			pLine = CFileMgr::ReadLine(hFile);
			while(pLine && sscanf(pLine, "%i", &iIndex))
			{
				Assert(iIndex<store.GetMaxMeshIndex());
				Assert(iCount<iNumIndices);

				if(iCount >= store.GetMaxMeshIndex())
					break;

				store.SetMapping(iIndex, (s16)iCount);	
				iCount++;
				pLine = CFileMgr::ReadLine(hFile);
			}
		}

		CFileMgr::CloseFile(hFile);
		return true;
	}
	
	return false;
}

//-----------------------------------------------------------------------------

class CDynamicNavMeshEntry
{
public:
	CDynamicNavMeshEntry() {
		m_pEntity = NULL;
		m_pNavMesh = NULL;
		m_Matrix.Identity();
		m_bCurrentlyCopyingMatrix = false;
		m_fBoundingRadius = 0.0f;

		for(int p=0; p<6; p++)
		{
			m_vBoundingPlanes[p] = Vector3(0.0f,0.0f,0.0f);
			m_fBoundingPlaneDists[p] = 0.0f;
		}
	}
	~CDynamicNavMeshEntry() { }

	class CEntity* GetEntity() const
	{
		return m_pEntity;
	}

	void SetEntity(CEntity *pNewEntity)
	{
		m_pEntity = pNewEntity;
	}

	Matrix34 m_Matrix;
	Vector3 m_vBoundingPlanes[6];
	float m_fBoundingRadius;
	float m_fBoundingPlaneDists[6];
	CNavMesh * m_pNavMesh;
	u32 m_bCurrentlyCopyingMatrix;

private:

#ifdef GTA_ENGINE
	RegdEnt m_pEntity;
#else
	CEntity * m_pEntity;
#endif

};

//-----------------------------------------------------------------------------

inline bool IsStitchVertSame(const Vector3 & v1, const Vector3 & v2, const float fEpsXYSqr, const float fEpsZ)
{
	if( square(v1.x - v2.x) + square(v1.y - v2.y) < fEpsXYSqr )
	{
		if( Abs( v1.z - v2.z ) < fEpsZ )
		{
			return true;
		}
	}
	return false;
}

inline bool VertexLiesAlongEdge(const Vector3 & vPoint, const eNavMeshEdge iEdge, const float fSharedEdgeCoord)
{
	static const float fEps = 1.0f;

	if(iEdge==eNegX || iEdge==ePosX)
	{
		return IsClose( vPoint.x, fSharedEdgeCoord, fEps );
	}
	else
	{
		return IsClose( vPoint.y, fSharedEdgeCoord, fEps );
	}
}

//-----------------------------------------------------------------------------

#endif // PATHSERVER_STORE_H

