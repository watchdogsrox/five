#ifndef ASYNCPROBES_H
#define ASYNCPROBES_H

#include "vector/vector3.h"
#include "scene/Entity.h"
#include "physics/WorldProbe/worldprobe.h"
#include "audiosoundtypes/sound.h"
//*********************************************************************
// audCachedLos and audCachedLosResults
// These classes handle issuing of environment probes asynchrously

#define AUD_LOS_FAILSAFE_LIFETIME	2000

// @OCC TODO - Do we want the multiple intersection / material occlusion based metrics for outside occlusion ( good for chainlink fences etc. )
#define AUD_MAX_TARGETTING_INTERSECTIONS 6


class audCachedLos
{
public:

	audCachedLos() : m_hShapeTest(1){};
	void AddProbe(const Vector3 & vStart, const Vector3 & vEnd, CEntity * pExcludeEntity, const u32 iIncludeFlags, bool bForceImmediate=false);
	void GetProbeResult();
	bool Clear();

	Vector3 m_vStart;
	Vector3 m_vEnd;
	Vector3 m_vIntersectPos;
	Vector3 m_vIntersectNormal;

	WorldProbe::CShapeTestResults m_hShapeTest;
	CEntity * m_pExcludeEntity;
	phInst* m_Instance;
	phMaterialMgr::Id m_MaterialId;
	u32 m_iIncludeFlags;
	s32 m_iLifeTime;	// failsafe so this gets cleared if something goes wrong

	// Whether this structure contains valid probe info
	u32 m_bContainsProbe	:1;
	// Whether this structure contains results
	u32 m_bHasProbeResults	:1;
	// Whether this structure has been processed
	u32 m_bHasBeenProcessed	:1;
	// Whether this probe his something (only if m_bHasProbeResults is set)
	u32 m_bHitSomething		:1;
	u16 m_Component;
};

class audCachedLosOneResult
{
public:

	audCachedLosOneResult();
	bool Clear();
	bool IsComplete();
	void GetProbeResults(); 

	Vector3 m_GroundNormal;
	Vector3 m_GroundPosition;
	audCachedLos m_MainLineTest;
};

class audCachedMultipleTestResults 
{
public:
	audCachedMultipleTestResults();
	virtual ~audCachedMultipleTestResults();
	virtual void ClearAll() = 0;
	virtual bool AreAllComplete() = 0;
	virtual bool NewTestComplete() = 0;
	virtual void GetAllProbeResults() = 0;

	Vector3 m_ResultNormal;
	Vector3 m_ResultPosition;
	s32 m_CurrentIndex;
	bool m_HasFoundTheSolution;
};


class audCachedLosMultipleResults
{
public:

	enum
	{
		NumMainLOS		= 3,
		NumMidLOS		= 4,
		NumNearVertLOS	= 2,
	};

	audCachedLosMultipleResults();
	void ClearAll();
	bool AreAllComplete();
	void GetAllProbeResults();

	s32 m_CurrentIndex;

	audCachedLos m_MainLos[NumMainLOS];
	audCachedLos m_MidLos[NumMidLOS];
	audCachedLos m_NearVerticalLos[NumNearVertLOS];
};

class audOcclusionIntersections
{
public:
	audOcclusionIntersections();
	void ClearAll();

	const Vector3& GetIntersectionPosition(u32 intersectionIndex) { return m_Position[intersectionIndex]; }
	atRangeArray<Vector3, AUD_MAX_TARGETTING_INTERSECTIONS> m_Position;

	float GetMaterialFactor(u32 intersectionIndex) { return m_MaterialFactor[intersectionIndex]; }
	atRangeArray<float, AUD_MAX_TARGETTING_INTERSECTIONS> m_MaterialFactor;

	int numIntersections;

#if __BANK
	int GetMaterialID(u32 intersectionIndex) { return m_MaterialID[intersectionIndex]; }
	atRangeArray<int, AUD_MAX_TARGETTING_INTERSECTIONS> m_MaterialID;
#endif
};

#endif