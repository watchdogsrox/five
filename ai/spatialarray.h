// 
// ai/spatialarray.h 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AI_SPATIALARRAY_H
#define AI_SPATIALARRAY_H

#ifndef SYSTEM_CRITICALSECTION_H
#include "system/criticalsection.h"
#endif

#include "ai/spatialarraynode.h"

#include "vectormath/vec4v.h"

// PURPOSE:	If this is set, CSpatialArray is compiled with full 64 bit support for
//			the pointers. This is probably what you want on a 64 bit system if you need
//			to support the full address range, but it may also be useful to temporarily
//			enable it on 32 bit systems to test that code path.
#define SPATIALARRAY64BIT (__64BIT)

#if !SPATIALARRAY64BIT
typedef u32 CSpatialArrayNodeAddr;
#endif

//------------------------------------------------------------------------------
// CSpatialArray
class CSpatialArray
{
public:
	enum
	{
#if SPATIALARRAY64BIT
		kSizePerNode = 3*sizeof(float) + 2*sizeof(u32) + sizeof(u32)
#else
		kSizePerNode = 3*sizeof(float) + sizeof(CSpatialArrayNodeAddr) + sizeof(u32)
#endif
	};

	explicit CSpatialArray(void *buffer, int maxObj);

	void Reset();

	void Insert(CSpatialArrayNode &node, u32 typeFlags = 0, bool forceInsert = false);
	void Remove(CSpatialArrayNode &node);
	void Update(CSpatialArrayNode &node, float posX, float posY, float posZ);
	void UpdateWithTypeFlags(CSpatialArrayNode &node, float posX, float posY, float posZ, u32 flagsToChange, u32 flagValues);

	void GetPosition(const CSpatialArrayNode &node, Vec3V_Ref posOut) const;

	void SetTypeFlags(CSpatialArrayNode &node, u32 flagsToChange, u32 flagValues);

	u32 GetTypeFlags(const CSpatialArrayNode &node) const;

	int FindClosest3(Vec3V_In centerV, CSpatialArrayNode **found, int maxFound,
			const u32 &typeFlagsToCareAbout, const u32 &typeFlagValues,
			const CSpatialArrayNode* &excl1, const CSpatialArrayNode* &excl2,
			float maxDist) const;

	int FindClosest4(Vec3V_In centerV, CSpatialArrayNode **found, int maxFound,
			const u32 &typeFlagsToCareAbout, const u32 &typeFlagValues,
			const CSpatialArrayNode* &excl1, const CSpatialArrayNode* &excl2,
			float maxDist) const;

	struct FindResult
	{
		CSpatialArrayNode*	m_Node;
		float				m_DistanceSq;
	};

	int FindInCylinderXY(Vec2V_In centerXYV, ScalarV_In radiusV, CSpatialArrayNode **found, int maxFound) const;

	int FindInSphere(Vec3V_In centerV, ScalarV_In radiusV, FindResult *found, int maxFound) const;

	int FindInSphere(Vec3V_In centerV, float radius, FindResult *found, int maxFound) const;

	int FindInSphereOfType(Vec3V_In centerV, ScalarV_In radiusV, CSpatialArrayNode **found,
			int maxFound, const u32 &typeFlagsToCareAbout, const u32 &typeFlagValues) const;

	int FindInSphereOfType(Vec3V_In centerV, float radius, CSpatialArrayNode **found,
			int maxFound, const u32 &typeFlagsToCareAbout, const u32 &typeFlagValues) const;

	int FindBelowZ(ScalarV_In scalar_thresholdZV, CSpatialArrayNode **found, int maxFound) const;

	int FindNearSegment(Vec3V_In segPos1V, Vec3V_In segPos2V, const float& distSegToObjCenter,
			CSpatialArrayNode** found, int maxFound) const;

	bool IsFull() const			{ return m_NumObj >= m_MaxObj; }
	bool IsEmpty() const		{ return m_NumObj == 0; }

	const float* GetPosXArray() const { return m_PosXArray; }
	const float* GetPosYArray() const { return m_PosYArray; }
	const float* GetPosZArray() const { return m_PosZArray; }
	const u32* GetTypeFlagArray() const { return m_TypeFlagArray; }

	CSpatialArrayNode* GetNodePointer(int index) const
	{
		Assert(index >= 0 && index < m_NumObj);
#if SPATIALARRAY64BIT
		const u32 nodeLower = m_NodeArrayLower[index];
		const u32 nodeUpper = m_NodeArrayUpper[index];
		return NodePtrFromUpperLower(nodeUpper, nodeLower);
#else
		return (CSpatialArrayNode*)m_NodeArray[index];
#endif
	}

	int GetNumObjects() const	{ return m_NumObj; }

	static CSpatialArrayNode* NodePtrFromUpperLower(u32 upper, u32 lower)
	{
		u64 addr = (((u64)upper) << 32) + (u64)lower;
#if __64BIT
		return (CSpatialArrayNode*)addr;
#else
		// Could do this to test:
		//	Assert(upper == lower + 1 || (upper == 0 && lower == 0));
		return (CSpatialArrayNode*)(u32)addr;
#endif
	}

	static void SetUseLock(const bool useLock) { sm_UseLock = useLock; }
	static bool GetUseLock() { return sm_UseLock; }

#if __DEV
	void DebugDraw() const;
#endif

protected:
	static const int kMaxObjForTempBuffer = 1024;

#if SPATIALARRAY64BIT
	static int CreateCompactNodePointerArray(const u32* foundArrayUpper, const u32* foundArrayLower, int numObj, CSpatialArrayNode** found, int maxFound);
#else
	static int CreateCompactNodePointerArray(const CSpatialArrayNodeAddr* foundArray, int numObj, CSpatialArrayNode** found, int maxFound);
#endif

	mutable sysCriticalSectionToken		m_Lock;

	float*					m_PosXArray;
	float*					m_PosYArray;
	float*					m_PosZArray;

#if SPATIALARRAY64BIT
	u32*					m_NodeArrayUpper;
	u32*					m_NodeArrayLower;
#else
	CSpatialArrayNodeAddr*	m_NodeArray;
#endif
	u32*					m_TypeFlagArray;

	int						m_MaxObj;
	int						m_NumObj;

	static bool				sm_UseLock;
};

//------------------------------------------------------------------------------
// CSpatialArrayFixed

template<int _maxObj> class CSpatialArrayFixed : public CSpatialArray
{
public:
	explicit CSpatialArrayFixed<_maxObj>()
			: CSpatialArray((void*)m_Buff, _maxObj)
	{
	}

	Vec4V m_Buff[CSpatialArray::kSizePerNode*_maxObj/sizeof(Vec4V)];
};

//------------------------------------------------------------------------------

#endif	// AI_SPATIALARRAY_H

/* End of file ai/spatialarray.h */
