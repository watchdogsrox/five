// 
// ai/spatialarray.cpp 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "ai/spatialarray.h"

#include "math/amath.h"
#include "system/criticalsection.h"
#include "system/memory.h"
#include "system/system.h"
#include "vector/colors.h"
#include "vectormath/classes.h"

#if __DEV
#include "grcore/debugdraw.h"
#endif

#define SA_STATS 0
#define Align16(x) (((x)+15)&~15)

#if SA_STATS

#include "profile/profiler.h"

namespace CSpatialArrayStats
{
	PF_PAGE(SpatialArray, "Spatial Array");
	PF_GROUP(Update);
	PF_LINK(SpatialArray, Update);
	PF_TIMER(Insert, Update);
	PF_TIMER(Remove, Update);
	PF_TIMER(Update, Update);
	PF_TIMER(GetTypeFlags, Update);
	PF_TIMER(SetTypeFlags, Update);
	PF_TIMER(FindClosest3, Update);
	PF_TIMER(FindClosest4, Update);
	PF_TIMER(FindInCylinderXY, Update);
	PF_TIMER(FindInSphere, Update);
	PF_TIMER(FindInSphereOfType, Update);
	PF_TIMER(FindNearSegment, Update);
	PF_TIMER(FindBelowZ, Update);
}

using namespace CSpatialArrayStats;

#define SA_PF_START(x) PF_START(x)
#define SA_PF_STOP(x) PF_STOP(x)
#define SA_PF_FUNC(x) PF_FUNC(x)

#else	// SA_STATS

#define SA_PF_START(x)
#define SA_PF_STOP(x)
#define SA_PF_FUNC(x)

#endif	// SA_STATS

#if SPATIALARRAY64BIT

namespace
{

u64 sNodePtrToU64(const CSpatialArrayNode* nodePtr)
{
#if __64BIT
	return (u64)nodePtr;
#else
	// Could do this to test:
	//	u32 upper = (u32)nodePtr + 1;
	//	u32 lower = (u32)nodePtr;
	//	return ((u64)(upper) << 32) + (u64)lower;
	return (u64)(u32)nodePtr;
#endif
}

}	// anon namespace

#endif	// SPATIALARRAY64BIT


bool CSpatialArray::sm_UseLock = false;

#define SPATIALARRAYTHREADLOCK \
	sysCriticalSection cs(m_Lock, sm_UseLock); \
	if (!sm_UseLock) \
	{ \
		Assertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CSpatialArray used from another thread than the main thread, this may not be safe."); \
	}

//------------------------------------------------------------------------------
// CSpatialArray

CSpatialArray::CSpatialArray(void *buffer, int maxObj)
		: m_MaxObj(maxObj)
		, m_NumObj(0)
{
	// The use of vector selection for addresses wouldn't work right
	// away on 64 bit pointers. It should still be possible to vectorize
	// it efficiently with 128 bit vectors, but we would need one high
	// and one low vector register for four pointers or something.
	// That work hasn't been done, so generate some errors if this is about
	// to happen. /FF
#if !SPATIALARRAY64BIT
#if __64BIT
	// To not prevent 64 bit applications to build, this is just a run time error
	// (it's very possible that the application compiles with this class without
	// actually using it). /FF
	Errorf("CSpatialArray was not designed for 64 bit pointers, probably won't work correctly right now.");
#else
	// If we are not running in 64 bit mode, I wouldn't expect pointers to be
	// anything but 32 bits, so if they are, we may as well catch it at compile time. /FF
	CompileTimeAssert(sizeof(void*) == 4);
#endif
#endif	// !SPATIALARRAY64BIT

	Assert((maxObj & 3) == 0);

	float *posXArray = (float*)buffer;
	float *posYArray = posXArray + maxObj;
	float *posZArray = posYArray + maxObj;
	
	Assert(Align16((size_t)posXArray));
	Assert(Align16((size_t)posYArray));
	Assert(Align16((size_t)posZArray));
	
#if SPATIALARRAY64BIT
	u32 *nodesUpper = (u32*)(posZArray + maxObj);
	u32 *nodesLower = nodesUpper + maxObj;
	u32 *typeFlagArray = (u32*)(nodesLower + maxObj);
#else
	CSpatialArrayNodeAddr *nodes = (CSpatialArrayNodeAddr*)(posZArray + maxObj);
	u32 *typeFlagArray = (u32*)(nodes + maxObj);
#endif

	Assert(((size_t)posXArray & 0xf) == 0);
	Assert(((size_t)posYArray & 0xf) == 0);
	Assert(((size_t)posZArray & 0xf) == 0);
#if SPATIALARRAY64BIT
	Assert(((size_t)nodesUpper & 0xf) == 0);
	Assert(((size_t)nodesLower & 0xf) == 0);
#else
	Assert(((size_t)nodes & 0xf) == 0);
#endif
	Assert(((size_t)typeFlagArray & 0xf) == 0);

	m_PosXArray = posXArray;
	m_PosYArray = posYArray;
	m_PosZArray = posZArray;
#if SPATIALARRAY64BIT
	m_NodeArrayUpper = nodesUpper;
	m_NodeArrayLower = nodesLower;
#else
	m_NodeArray = nodes;
#endif
	m_TypeFlagArray = typeFlagArray;

	// For some of the vector operations to work at the end of the array,
	// we make sure to keep the node pointers to NULL. /FF
#if SPATIALARRAY64BIT
	sysMemSet((void*)nodesUpper, 0, sizeof(u32)*maxObj);
	sysMemSet((void*)nodesLower, 0, sizeof(u32)*maxObj);
#else
	sysMemSet((void*)nodes, 0, sizeof(CSpatialArrayNodeAddr)*maxObj);
#endif



	Assert(kMaxObjForTempBuffer >= maxObj);
	Assert(maxObj*sizeof(float) <= 0xffff);
}

void CSpatialArray::Reset()
{
	SPATIALARRAYTHREADLOCK;
	m_NumObj = 0;
}

void CSpatialArray::Insert(CSpatialArrayNode &node, u32 typeFlags, bool forceInsert)
{
	SA_PF_FUNC(Insert);

	if(forceInsert || Verifyf(node.m_Offs == CSpatialArrayNode::kOffsInvalid, "Tried to insert a spatial array node that's already inserted."))
	{
		SPATIALARRAYTHREADLOCK;

		const int numObj = m_NumObj;
		const int maxObj = m_MaxObj;

		if(Verifyf(numObj < maxObj, "Out of space in spatial array."))
		{
			const unsigned int offs = numObj*sizeof(float);
			const int newNumObj = numObj + 1;

#if SPATIALARRAY64BIT
			u32 *addedNodePtrUpper = (u32*)((char*)m_NodeArrayUpper + offs);
			u32 *addedNodePtrLower = (u32*)((char*)m_NodeArrayLower + offs);
#else
			CSpatialArrayNodeAddr *addedNodePtr = (CSpatialArrayNodeAddr*)((char*)m_NodeArray + offs);
#endif
			u32 *typeFlagPtr = (u32*)((char*)m_TypeFlagArray + offs);

#if SPATIALARRAY64BIT
			u64 nodePtr = sNodePtrToU64(&node);
			*addedNodePtrLower = (u32)nodePtr;
			*addedNodePtrUpper = (u32)(nodePtr >> 32);
#else
			*addedNodePtr = ptrdiff_t_to_int((ptrdiff_t)&node);		// Catch truncation on x64 builds
#endif
			node.m_Offs = (u16)offs;
			m_NumObj = newNumObj;
			*typeFlagPtr = typeFlags;
		}
		else
		{
			node.m_Offs = CSpatialArrayNode::kOffsInvalid;
		}
	}
}


void CSpatialArray::Remove(CSpatialArrayNode &node)
{
	SA_PF_FUNC(Remove);

	if(Verifyf(node.m_Offs != CSpatialArrayNode::kOffsInvalid, "Removing spatial array node not in array."))
	{
		SPATIALARRAYTHREADLOCK;

		const int oldNumObj = m_NumObj;
		const int newNumObj = oldNumObj - 1;

		const unsigned int removedOffs = node.m_Offs;
#if SPATIALARRAY64BIT
		u32* nodesLower = m_NodeArrayLower;
		u32* nodesUpper = m_NodeArrayUpper;
		u32 oldLastNodeLower = nodesLower[newNumObj];
		u32 oldLastNodeUpper = nodesUpper[newNumObj];
		CSpatialArrayNode* oldLastNodePtr = NodePtrFromUpperLower(oldLastNodeUpper, oldLastNodeLower);

		u32* removedNodePtrLower = (u32*)((char*)nodesLower + removedOffs);
		u32* removedNodePtrUpper = (u32*)((char*)nodesUpper + removedOffs);
#else
		CSpatialArrayNodeAddr* nodes = m_NodeArray;
		CSpatialArrayNodeAddr oldLastNode = nodes[newNumObj];
		CSpatialArrayNode* oldLastNodePtr = (CSpatialArrayNode*)oldLastNode;

		CSpatialArrayNodeAddr *removedNodePtr = (CSpatialArrayNodeAddr*)((char*)nodes + removedOffs);
#endif

		float *posXArray = m_PosXArray;
		float *posYArray = m_PosYArray;
		float *posZArray = m_PosZArray;
		u32 *typeFlagArray = m_TypeFlagArray;

		const unsigned int oldOffs = oldLastNodePtr->m_Offs;

		float *posXPtr = (float*)((char*)posXArray + removedOffs);
		float *posYPtr = (float*)((char*)posYArray + removedOffs);
		float *posZPtr = (float*)((char*)posZArray + removedOffs);
		u32 *typeFlagPtr = (u32*)((char*)typeFlagArray + removedOffs);

		float *posXPtrOld = (float*)((char*)posXArray + oldOffs);
		float *posYPtrOld = (float*)((char*)posYArray + oldOffs);
		float *posZPtrOld = (float*)((char*)posZArray + oldOffs);
		u32 *typeFlagArrayOld = (u32*)((char*)typeFlagArray + oldOffs);

		*posXPtr = *posXPtrOld;
		*posYPtr = *posYPtrOld;
		*posZPtr = *posZPtrOld;
		*typeFlagPtr = *typeFlagArrayOld;

#if SPATIALARRAY64BIT
		*removedNodePtrLower = oldLastNodeLower;
		*removedNodePtrUpper = oldLastNodeUpper;
#else
		*removedNodePtr = oldLastNode;
#endif

		oldLastNodePtr->m_Offs = (u16)removedOffs;
		node.m_Offs = CSpatialArrayNode::kOffsInvalid;
		m_NumObj = newNumObj;

		// For some of the vector operations to work properly at the end
		// of the array, we make sure to clear out the node pointer at
		// the previous end of the array. /FF
#if SPATIALARRAY64BIT
		nodesUpper[newNumObj] = 0;
		nodesLower[newNumObj] = 0;
#else
		nodes[newNumObj] = 0;
#endif
	}
}


void CSpatialArray::Update(CSpatialArrayNode &node, float posX, float posY, float posZ)
{
	SA_PF_FUNC(Update);

	if(Verifyf(node.m_Offs != CSpatialArrayNode::kOffsInvalid, "Tried to update position of invalid spatial array node."))
	{
		SPATIALARRAYTHREADLOCK;

		const unsigned int offs = node.m_Offs;
		float *posXArray = m_PosXArray;
		float *posYArray = m_PosYArray;
		float *posZArray = m_PosZArray;

		float *posXPtr = (float*)((char*)posXArray + offs);
		float *posYPtr = (float*)((char*)posYArray + offs);
		float *posZPtr = (float*)((char*)posZArray + offs);

		*posXPtr = posX;
		*posYPtr = posY;
		*posZPtr = posZ;
	}
}


void CSpatialArray::UpdateWithTypeFlags(CSpatialArrayNode &node, float posX, float posY, float posZ, u32 flagsToChange, u32 flagValues)
{
	SA_PF_FUNC(Update);

	if(Verifyf(node.m_Offs != CSpatialArrayNode::kOffsInvalid, "Tried to update position of invalid spatial array node."))
	{
		SPATIALARRAYTHREADLOCK;

		const unsigned int offs = node.m_Offs;
		float *posXArray = m_PosXArray;
		float *posYArray = m_PosYArray;
		float *posZArray = m_PosZArray;

		float *posXPtr = (float*)((char*)posXArray + offs);
		float *posYPtr = (float*)((char*)posYArray + offs);
		float *posZPtr = (float*)((char*)posZArray + offs);
		u32 *flagPtr = (u32*)((char*)m_TypeFlagArray + offs);

		const u32 oldFlags = *flagPtr;
		const u32 newFlags = (oldFlags & ~flagsToChange) | flagValues;

		*posXPtr = posX;
		*posYPtr = posY;
		*posZPtr = posZ;
		*flagPtr = newFlags;
	}
}


void CSpatialArray::GetPosition(const CSpatialArrayNode &node, Vec3V_Ref posOut) const
{
	// Not sure:
	//	SPATIALARRAYTHREADLOCK;

	const unsigned int offs = node.m_Offs;
	const float *posXArray = m_PosXArray;
	const float *posYArray = m_PosYArray;
	const float *posZArray = m_PosZArray;

	const float *posXPtr = (const float*)((char*)posXArray + offs);
	const float *posYPtr = (const float*)((char*)posYArray + offs);
	const float *posZPtr = (const float*)((char*)posZArray + offs);

	posOut.SetXf(*posXPtr);
	posOut.SetYf(*posYPtr);
	posOut.SetZf(*posZPtr);
}



void CSpatialArray::SetTypeFlags(CSpatialArrayNode &node, u32 flagsToChange, u32 flagValues)
{
	SA_PF_FUNC(SetTypeFlags);	// Maybe not really accurate. /FF

	// If this fails, there are values set in flagValues that are not in flagsToChange,
	// which we are probably better off if the user could avoid, so we don't have to
	// spend time on masking them here. /FF
	Assert((flagValues & ~flagsToChange) == 0);

	if(Verifyf(node.m_Offs != CSpatialArrayNode::kOffsInvalid, "Tried to set type flags of invalid spatial array node."))
	{
		SPATIALARRAYTHREADLOCK;

		const unsigned int offs = node.m_Offs;
		u32 *flagPtr = (u32*)((char*)m_TypeFlagArray + offs);

		const u32 oldFlags = *flagPtr;
		const u32 newFlags = (oldFlags & ~flagsToChange) | flagValues;

		*flagPtr = newFlags;
	}
}


u32 CSpatialArray::GetTypeFlags(const CSpatialArrayNode &node) const
{
	SA_PF_FUNC(GetTypeFlags);	// Maybe not really accurate. /FF

	u32 r;

	if(Verifyf(node.m_Offs != CSpatialArrayNode::kOffsInvalid, "Tried to get type flags of invalid spatial array node."))
	{
		SPATIALARRAYTHREADLOCK;

		const unsigned int offs = node.m_Offs;
		const u32 *flagPtr = (u32*)((char*)m_TypeFlagArray + offs);
		r = *flagPtr;
	}
	else
	{
		r = 0;
	}

	return r;
}

#if SPATIALARRAY64BIT
static int sPickFromSortedArrays(const Vec4V *closestND2V,
		const Vec4V *closestNNodesUpperV,
		const Vec4V *closestNNodesLowerV,
		float maxDist, CSpatialArrayNode **found,
		int numToPick)
#else
static int sPickFromSortedArrays(const Vec4V *closestND2V, const Vec4V *closestNNodesV,
		float maxDist, CSpatialArrayNode **found,
		int numToPick)
#endif
{
	const float maxDistSq = square(Min(maxDist, LARGE_FLOAT));	// Make sure we don't square FLT_MAX. /FF

	int numFound = 0;

	// We now basically have four sorted arrays of length N in memory,
	// and we will do comparisons between these arrays to find the closest
	// three over all. We do the comparisons using u32's, making use of the
	// fact that positive IEEE754 floating point numbers preserve the numerical
	// order when interpreted as integeres. That way, we avoid floating point
	// branches. /FF
	int k1 = 0, k2 = 1, k3 = 2, k4 = 3;
	u32 d1 = ((u32*)closestND2V)[k1];
	u32 d2 = ((u32*)closestND2V)[k2];
	u32 d3 = ((u32*)closestND2V)[k3];
	u32 d4 = ((u32*)closestND2V)[k4];
	for(int i = 0; i < numToPick; i++)
	{
		// Note: should be set in all code paths below. /FF
		int closestIndex;

		if(d1 < d2)
		{
			if(d1 < d3)
			{
				if(d1 < d4)
				{
					// d1 smallest
					closestIndex = k1;
					k1 += 4;
					d1 = ((u32*)closestND2V)[k1];
				}
				else
				{
					// d4 smallest
					closestIndex = k4;
					k4 += 4;
					d4 = ((u32*)closestND2V)[k4];
				}
			}
			else
			{
				if(d3 < d4)
				{
					// d3 smallest
					closestIndex = k3;
					k3 += 4;
					d3 = ((u32*)closestND2V)[k3];
				}
				else
				{
					// d4 smallest
					closestIndex = k4;
					k4 += 4;
					d4 = ((u32*)closestND2V)[k4];
				}
			}
		}
		else
		{
			if(d2 < d3)
			{
				if(d2 < d4)
				{
					// d2 smallest
					closestIndex = k2;
					k2 += 4;
					d2 = ((u32*)closestND2V)[k2];
				}
				else
				{
					// d4 smallest
					closestIndex = k4;
					k4 += 4;
					d4 = ((u32*)closestND2V)[k4];
				}
			}
			else
			{
				if(d3 < d4)
				{
					// d3 smallest
					closestIndex = k3;
					k3 += 4;
					d3 = ((u32*)closestND2V)[k3];
				}
				else
				{
					// d4 smallest
					closestIndex = k4;
					k4 += 4;
					d4 = ((u32*)closestND2V)[k4];
				}
			}
		}

#if SPATIALARRAY64BIT
		CSpatialArrayNode* closest = CSpatialArray::NodePtrFromUpperLower(
				((u32*)closestNNodesUpperV)[closestIndex],
				((u32*)closestNNodesLowerV)[closestIndex]
				);
#else
		CSpatialArrayNode* closest = (CSpatialArrayNode*)(((CSpatialArrayNodeAddr*)closestNNodesV)[closestIndex]);
#endif
		if(closest)
		{
			const float distSq = ((float*)closestND2V)[closestIndex];
			if(distSq <= maxDistSq)
			{
				found[numFound++] = closest;
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	// Note: if it's useful, we could potentially let the code above extract
	// more close objects by continuing to operate on the arrays. It wouldn't
	// be perfectly accurate beyond the first three, but they would still be
	// objects closer than many others. /FF

	return numFound;
}


int CSpatialArray::FindClosest3(Vec3V_In centerV,
		CSpatialArrayNode **found, int ASSERT_ONLY(maxFound),
		const u32 &typeFlagsToCareAbout, const u32 &typeFlagValues,
		const CSpatialArrayNode* &excl1, const CSpatialArrayNode* &excl2,
		float maxDist) const
{
	SA_PF_FUNC(FindClosest3);

	Assert(maxFound >= 3);

	// If this fails, there are values set in flagValues that are not in flagsToChange,
	// which we are probably better off if the user could avoid, so we don't have to
	// spend time on masking them here. /FF
	Assert((typeFlagValues & ~typeFlagsToCareAbout) == 0);

	SPATIALARRAYTHREADLOCK;

	// Load the type flag stuff into vector registers. Note that we intentionally
	// pass in these by reference, requiring the user to put them in memory, because
	// if they were passed in in general purpose registers, we would need to store
	// them to memory and load them back anyway. Could pass them in in vector
	// registers, of course, but that's probably not worth the trouble. /FF
	const Vec4V typeFlagsToCareAboutV = Vec4V(LoadScalar32IntoScalarV(typeFlagsToCareAbout));
	const Vec4V typeFlagValuesV = Vec4V(LoadScalar32IntoScalarV(typeFlagValues));

#if SPATIALARRAY64BIT

	// Get the exclusion pointers into vector registers. This way
	// of doing it is probably sub-optimal: we should be able to
	// read straight from excl1/excl2 (references to caller's memory)
	// into vector registers like we do in the 32 bit case, but to do so
	// we would have to be really careful to avoid endianness issues.

	u64 excl1Ptr = sNodePtrToU64(excl1);
	u64 excl2Ptr = sNodePtrToU64(excl2);

	ScalarV excl1LowerSV, excl1UpperSV;
	ScalarV excl2LowerSV, excl2UpperSV;
	excl1UpperSV.Seti((u32)(excl1Ptr >> 32));
	excl2UpperSV.Seti((u32)(excl2Ptr >> 32));
	excl1LowerSV.Seti((u32)excl1Ptr);
	excl2LowerSV.Seti((u32)excl2Ptr);

	const Vec4V excl1LowerV = Vec4V(excl1LowerSV);
	const Vec4V excl2LowerV = Vec4V(excl2LowerSV);
	const Vec4V excl1UpperV = Vec4V(excl1UpperSV);
	const Vec4V excl2UpperV = Vec4V(excl2UpperSV);

#else
	const Vec4V excl1V = Vec4V(LoadScalar32IntoScalarV(*(u32*)&excl1));
	const Vec4V excl2V = Vec4V(LoadScalar32IntoScalarV(*(u32*)&excl2));
#endif

	const Vec4V* RESTRICT objXPtr = (const Vec4V*)m_PosXArray;
	const Vec4V* RESTRICT objYPtr = (const Vec4V*)m_PosYArray;
	const Vec4V* RESTRICT objZPtr = (const Vec4V*)m_PosZArray;
#if SPATIALARRAY64BIT
	const Vec4V* RESTRICT nodesPtrLower = (const Vec4V*)m_NodeArrayLower;
	const Vec4V* RESTRICT nodesPtrUpper = (const Vec4V*)m_NodeArrayUpper;
#else
	const Vec4V* RESTRICT nodesPtr = (const Vec4V*)m_NodeArray;
#endif
	const Vec4V* RESTRICT typeFlagPtr = (const Vec4V*)m_TypeFlagArray;

	const Vec4V centerxV(SplatX(centerV));
	const Vec4V centeryV(SplatY(centerV));
	const Vec4V centerzV(SplatZ(centerV));

	const Vec4V zeroV(V_ZERO);
	const Vec4V maxDistV(V_FLT_MAX);

	const int numObj = m_NumObj;

	// These are used to keep track of the three closest objects
	// for each of the components in the vector registers. /FF
#if SPATIALARRAY64BIT
	Vec4V close1NodesLowerV(V_ZERO);
	Vec4V close2NodesLowerV(V_ZERO);
	Vec4V close3NodesLowerV(V_ZERO);
	Vec4V close1NodesUpperV(V_ZERO);
	Vec4V close2NodesUpperV(V_ZERO);
	Vec4V close3NodesUpperV(V_ZERO);
#else
	Vec4V close1NodesV(V_ZERO);
	Vec4V close2NodesV(V_ZERO);
	Vec4V close3NodesV(V_ZERO);
#endif

	// These are the squared distances for the objects in
	// close[1/2/3]NodesV. /FF
	Vec4V close1D2V(V_FLT_MAX);
	Vec4V close2D2V(V_FLT_MAX);
	Vec4V close3D2V(V_FLT_MAX);

	for(int i = 0; i < numObj; i += 4)
	{
		// Load from the arrays to the vector registers. /FF
		const Vec4V xxV = *objXPtr;
		const Vec4V yyV = *objYPtr;
		const Vec4V zzV = *objZPtr;
#if SPATIALARRAY64BIT
		const Vec4V nodesUpperV = *nodesPtrUpper;
		const Vec4V nodesLowerV = *nodesPtrLower;
#else
		const Vec4V nodesV = *nodesPtr;
#endif
		const Vec4V objTypeFlagsV = *typeFlagPtr;

		// Compute the squared distance to the center. /FF
		const Vec4V dxV = Subtract(xxV, centerxV);
		const Vec4V dyV = Subtract(yyV, centeryV);
		const Vec4V dzV = Subtract(zzV, centerzV);
		const Vec4V dx2V = Scale(dxV, dxV);
		const Vec4V dxy2V = AddScaled(dx2V, dyV, dyV);
		const Vec4V d2BeforeMaskV = AddScaled(dxy2V, dzV, dzV);

		const Vec4V objTypeFlagsCaredAboutV = And(objTypeFlagsV, typeFlagsToCareAboutV);

		// This is needed to deal properly with the end of the array if the number
		// of objects is not aligned with 4. The node pointers beyond the end will
		// be NULL, and here we create a mask where 0x0000 means that the node was
		// within range (pointer not NULL) while 0xffff indicates a value past the
		// end of the array. /FF
#if SPATIALARRAY64BIT
		const VecBoolV selectNodePtrZeroV = IsEqualInt(Or(nodesLowerV, nodesUpperV), zeroV);	// Ptr is NULL only if both halves are 0.
#else
		const VecBoolV selectNodePtrZeroV = IsEqualInt(nodesV, zeroV);
#endif

		// Match the type flags. 0xffff in this mask indicates that
		// (objTypeFlags & typeFlagsToCareAbout) == typeFlagValues
		// i.e. the bits we care about have the values we are looking for. /FF
		const VecBoolV selectTypeFlagMatchV = IsEqualInt(objTypeFlagsCaredAboutV, typeFlagValuesV);

#if SPATIALARRAY64BIT
		// Check for matches on the upper and lower halves of the exclusion addresses.
		const VecBoolV selectNodePtrExcl1LowerV = IsEqualInt(nodesLowerV, excl1LowerV);
		const VecBoolV selectNodePtrExcl2LowerV = IsEqualInt(nodesLowerV, excl2LowerV);
		const VecBoolV selectNodePtrExcl1UpperV = IsEqualInt(nodesUpperV, excl1UpperV);
		const VecBoolV selectNodePtrExcl2UpperV = IsEqualInt(nodesUpperV, excl2UpperV);

		// Combine the upper/lower halves together: both halves have to match.
		const VecBoolV selectNodePtrExcl1V = And(selectNodePtrExcl1LowerV, selectNodePtrExcl1UpperV);
		const VecBoolV selectNodePtrExcl2V = And(selectNodePtrExcl2LowerV, selectNodePtrExcl2UpperV);
#else
		const VecBoolV selectNodePtrExcl1V = IsEqualInt(nodesV, excl1V);
		const VecBoolV selectNodePtrExcl2V = IsEqualInt(nodesV, excl2V);
#endif
		// We have a couple of vectors now that are 0xffff on a mismatch,
		// instead of 0x0000 on a match. We OR them and NOT them so that
		// we get a mask that's 0xffff when they all match. /FF
		const VecBoolV selectNodePtrV = InvertBits(Or(Or(selectNodePtrExcl1V, selectNodePtrExcl2V),
				selectNodePtrZeroV));

		// To allow use of an element, we require both that it's not past the end
		// of the array or otherwise an ineligible node pointer, and that the type flags match. 
		const VecBoolV combinedSelect = And(selectNodePtrV, selectTypeFlagMatchV);

		// Now, select between the true measured distances and FLT_MAX, depending on whether
		// these objects fit the acceptance criteria above. If FLT_MAX is selected here,
		// it won't be closer than objects we have previously found, so the objects being
		// looked at now won't be chosen. /FF
		const Vec4V d2V = SelectFT(combinedSelect, maxDistV, d2BeforeMaskV);

		// Compare the squared distance of these objects vs. the squared distances
		// of the closest objects found so far. /FF
		const VecBoolV selectCloserThan1V = IsLessThan(d2V, close1D2V);
		const VecBoolV selectCloserThan2V = IsLessThan(d2V, close2D2V);
		const VecBoolV selectCloserThan3V = IsLessThan(d2V, close3D2V);

		// Compute some temporary vectors for the logic of how to move the elements.
		// For example, temp2D2V is used for the squared distance of the 2nd closest
		// object. If we are going to replace that element, it would either be replaced
		// by the current distance (if the new object is closest than the old 2nd closest,
		// but not closer than the #1 closest one) or by the old distance for the #1 closest
		// one (if that's going to get replaced). /FF
		const Vec4V temp2D2V = SelectFT(selectCloserThan1V, d2V, close1D2V);
		const Vec4V temp3D2V = SelectFT(selectCloserThan2V, d2V, close2D2V);
#if SPATIALARRAY64BIT
		const Vec4V temp2NodesUpperV = SelectFT(selectCloserThan1V, nodesUpperV, close1NodesUpperV);
		const Vec4V temp3NodesUpperV = SelectFT(selectCloserThan2V, nodesUpperV, close2NodesUpperV);
		const Vec4V temp2NodesLowerV = SelectFT(selectCloserThan1V, nodesLowerV, close1NodesLowerV);
		const Vec4V temp3NodesLowerV = SelectFT(selectCloserThan2V, nodesLowerV, close2NodesLowerV);
#else
		const Vec4V temp2NodesV = SelectFT(selectCloserThan1V, nodesV, close1NodesV);
		const Vec4V temp3NodesV = SelectFT(selectCloserThan2V, nodesV, close2NodesV);
#endif

		// Finally, compute the new first, second, and third closest objects found
		// so far. /FF
		close3D2V = SelectFT(selectCloserThan3V, close3D2V, temp3D2V);
		close2D2V = SelectFT(selectCloserThan2V, close2D2V, temp2D2V);
		close1D2V = SelectFT(selectCloserThan1V, close1D2V, d2V);
#if SPATIALARRAY64BIT
		close3NodesUpperV = SelectFT(selectCloserThan3V, close3NodesUpperV, temp3NodesUpperV);
		close2NodesUpperV = SelectFT(selectCloserThan2V, close2NodesUpperV, temp2NodesUpperV);
		close1NodesUpperV = SelectFT(selectCloserThan1V, close1NodesUpperV, nodesUpperV);

		close3NodesLowerV = SelectFT(selectCloserThan3V, close3NodesLowerV, temp3NodesLowerV);
		close2NodesLowerV = SelectFT(selectCloserThan2V, close2NodesLowerV, temp2NodesLowerV);
		close1NodesLowerV = SelectFT(selectCloserThan1V, close1NodesLowerV, nodesLowerV);
#else
		close3NodesV = SelectFT(selectCloserThan3V, close3NodesV, temp3NodesV);
		close2NodesV = SelectFT(selectCloserThan2V, close2NodesV, temp2NodesV);
		close1NodesV = SelectFT(selectCloserThan1V, close1NodesV, nodesV);
#endif

		// Move on in the arrays. /FF
		objXPtr++;
		objYPtr++;
		objZPtr++;
#if SPATIALARRAY64BIT
		nodesPtrUpper++;
		nodesPtrLower++;
#else
		nodesPtr++;
#endif
		typeFlagPtr++;
	}

	// Store out the squared distances and pointers to memory. /FF
	Vec4V closest3D2V[3];
	closest3D2V[0] = close1D2V;
	closest3D2V[1] = close2D2V;
	closest3D2V[2] = close3D2V;

#if SPATIALARRAY64BIT
	Vec4V closest3NodesUpperV[3];
	closest3NodesUpperV[0] = close1NodesUpperV;
	closest3NodesUpperV[1] = close2NodesUpperV;
	closest3NodesUpperV[2] = close3NodesUpperV;

	Vec4V closest3NodesLowerV[3];
	closest3NodesLowerV[0] = close1NodesLowerV;
	closest3NodesLowerV[1] = close2NodesLowerV;
	closest3NodesLowerV[2] = close3NodesLowerV;

	return sPickFromSortedArrays(closest3D2V, closest3NodesUpperV, closest3NodesLowerV, maxDist, found, 3);
#else
	Vec4V closest3NodesV[3];
	closest3NodesV[0] = close1NodesV;
	closest3NodesV[1] = close2NodesV;
	closest3NodesV[2] = close3NodesV;

	return sPickFromSortedArrays(closest3D2V, closest3NodesV, maxDist, found, 3);
#endif
}


int CSpatialArray::FindClosest4(Vec3V_In centerV,
		CSpatialArrayNode **found, int ASSERT_ONLY(maxFound),
		const u32 &typeFlagsToCareAbout, const u32 &typeFlagValues,
		const CSpatialArrayNode* &excl1, const CSpatialArrayNode* &excl2,
		float maxDist) const
{
	SA_PF_FUNC(FindClosest4);

	Assert(maxFound >= 4);

	// If this fails, there are values set in flagValues that are not in flagsToChange,
	// which we are probably better off if the user could avoid, so we don't have to
	// spend time on masking them here. /FF
	Assert((typeFlagValues & ~typeFlagsToCareAbout) == 0);

	SPATIALARRAYTHREADLOCK;

	// Load the type flag stuff into vector registers. Note that we intentionally
	// pass in these by reference, requiring the user to put them in memory, because
	// if they were passed in in general purpose registers, we would need to store
	// them to memory and load them back anyway. Could pass them in in vector
	// registers, of course, but that's probably not worth the trouble. /FF
	const Vec4V typeFlagsToCareAboutV = Vec4V(LoadScalar32IntoScalarV(typeFlagsToCareAbout));
	const Vec4V typeFlagValuesV = Vec4V(LoadScalar32IntoScalarV(typeFlagValues));

#if SPATIALARRAY64BIT

	// Get the exclusion pointers into vector registers. This way
	// of doing it is probably sub-optimal: we should be able to
	// read straight from excl1/excl2 (references to caller's memory)
	// into vector registers like we do in the 32 bit case, but to do so
	// we would have to be really careful to avoid endianness issues.

	u64 excl1Ptr = sNodePtrToU64(excl1);
	u64 excl2Ptr = sNodePtrToU64(excl2);

	ScalarV excl1LowerSV, excl1UpperSV;
	ScalarV excl2LowerSV, excl2UpperSV;
	excl1UpperSV.Seti((u32)(excl1Ptr >> 32));
	excl2UpperSV.Seti((u32)(excl2Ptr >> 32));
	excl1LowerSV.Seti((u32)excl1Ptr);
	excl2LowerSV.Seti((u32)excl2Ptr);

	const Vec4V excl1LowerV = Vec4V(excl1LowerSV);
	const Vec4V excl2LowerV = Vec4V(excl2LowerSV);
	const Vec4V excl1UpperV = Vec4V(excl1UpperSV);
	const Vec4V excl2UpperV = Vec4V(excl2UpperSV);

#else
	const Vec4V excl1V = Vec4V(LoadScalar32IntoScalarV(*(u32*)&excl1));
	const Vec4V excl2V = Vec4V(LoadScalar32IntoScalarV(*(u32*)&excl2));
#endif

	const Vec4V* RESTRICT objXPtr = (const Vec4V*)m_PosXArray;
	const Vec4V* RESTRICT objYPtr = (const Vec4V*)m_PosYArray;
	const Vec4V* RESTRICT objZPtr = (const Vec4V*)m_PosZArray;
#if SPATIALARRAY64BIT
	const Vec4V* RESTRICT nodesPtrLower = (const Vec4V*)m_NodeArrayLower;
	const Vec4V* RESTRICT nodesPtrUpper = (const Vec4V*)m_NodeArrayUpper;
#else
	const Vec4V* RESTRICT nodesPtr = (const Vec4V*)m_NodeArray;
#endif
	const Vec4V* RESTRICT typeFlagPtr = (const Vec4V*)m_TypeFlagArray;

	const Vec4V centerxV(SplatX(centerV));
	const Vec4V centeryV(SplatY(centerV));
	const Vec4V centerzV(SplatZ(centerV));

	const Vec4V zeroV(V_ZERO);
	const Vec4V maxDistV(V_FLT_MAX);

	const int numObj = m_NumObj;

	// These are used to keep track of the three closest objects
	// for each of the components in the vector registers. /FF
#if SPATIALARRAY64BIT
	Vec4V close1NodesLowerV(V_ZERO);
	Vec4V close2NodesLowerV(V_ZERO);
	Vec4V close3NodesLowerV(V_ZERO);
	Vec4V close4NodesLowerV(V_ZERO);
	Vec4V close1NodesUpperV(V_ZERO);
	Vec4V close2NodesUpperV(V_ZERO);
	Vec4V close3NodesUpperV(V_ZERO);
	Vec4V close4NodesUpperV(V_ZERO);
#else
	Vec4V close1NodesV(V_ZERO);
	Vec4V close2NodesV(V_ZERO);
	Vec4V close3NodesV(V_ZERO);
	Vec4V close4NodesV(V_ZERO);
#endif

	// These are the squared distances for the objects in
	// close[1/2/3/4]NodesV. /FF
	Vec4V close1D2V(V_FLT_MAX);
	Vec4V close2D2V(V_FLT_MAX);
	Vec4V close3D2V(V_FLT_MAX);
	Vec4V close4D2V(V_FLT_MAX);

	for(int i = 0; i < numObj; i += 4)
	{
		// Load from the arrays to the vector registers. /FF
		const Vec4V xxV = *objXPtr;
		const Vec4V yyV = *objYPtr;
		const Vec4V zzV = *objZPtr;
#if SPATIALARRAY64BIT
		const Vec4V nodesUpperV = *nodesPtrUpper;
		const Vec4V nodesLowerV = *nodesPtrLower;
#else
		const Vec4V nodesV = *nodesPtr;
#endif
		const Vec4V objTypeFlagsV = *typeFlagPtr;

		// Compute the squared distance to the center. /FF
		const Vec4V dxV = Subtract(xxV, centerxV);
		const Vec4V dyV = Subtract(yyV, centeryV);
		const Vec4V dzV = Subtract(zzV, centerzV);
		const Vec4V dx2V = Scale(dxV, dxV);
		const Vec4V dxy2V = AddScaled(dx2V, dyV, dyV);
		const Vec4V d2BeforeMaskV = AddScaled(dxy2V, dzV, dzV);

		const Vec4V objTypeFlagsCaredAboutV = And(objTypeFlagsV, typeFlagsToCareAboutV);

		// This is needed to deal properly with the end of the array if the number
		// of objects is not aligned with 4. The node pointers beyond the end will
		// be NULL, and here we create a mask where 0x0000 means that the node was
		// within range (pointer not NULL) while 0xffff indicates a value past the
		// end of the array. /FF
#if SPATIALARRAY64BIT
		const VecBoolV selectNodePtrZeroV = IsEqualInt(Or(nodesLowerV, nodesUpperV), zeroV);	// Ptr is NULL only if both halves are 0.
#else
		const VecBoolV selectNodePtrZeroV = IsEqualInt(nodesV, zeroV);
#endif

		// Match the type flags. 0xffff in this mask indicates that
		// (objTypeFlags & typeFlagsToCareAbout) == typeFlagValues
		// i.e. the bits we care about have the values we are looking for. /FF
		const VecBoolV selectTypeFlagMatchV = IsEqualInt(objTypeFlagsCaredAboutV, typeFlagValuesV);

#if SPATIALARRAY64BIT
		// Check for matches on the upper and lower halves of the exclusion addresses.
		const VecBoolV selectNodePtrExcl1LowerV = IsEqualInt(nodesLowerV, excl1LowerV);
		const VecBoolV selectNodePtrExcl2LowerV = IsEqualInt(nodesLowerV, excl2LowerV);
		const VecBoolV selectNodePtrExcl1UpperV = IsEqualInt(nodesUpperV, excl1UpperV);
		const VecBoolV selectNodePtrExcl2UpperV = IsEqualInt(nodesUpperV, excl2UpperV);

		// Combine the upper/lower halves together: both halves have to match.
		const VecBoolV selectNodePtrExcl1V = And(selectNodePtrExcl1LowerV, selectNodePtrExcl1UpperV);
		const VecBoolV selectNodePtrExcl2V = And(selectNodePtrExcl2LowerV, selectNodePtrExcl2UpperV);
#else
		const VecBoolV selectNodePtrExcl1V = IsEqualInt(nodesV, excl1V);
		const VecBoolV selectNodePtrExcl2V = IsEqualInt(nodesV, excl2V);
#endif
		// We have a couple of vectors now that are 0xffff on a mismatch,
		// instead of 0x0000 on a match. We OR them and NOT them so that
		// we get a mask that's 0xffff when they all match. /FF
		const VecBoolV selectNodePtrV = InvertBits(Or(Or(selectNodePtrExcl1V, selectNodePtrExcl2V),
				selectNodePtrZeroV));

		// To allow use of an element, we require both that it's not past the end
		// of the array or otherwise an ineligible node pointer, and that the type flags match. 
		const VecBoolV combinedSelect = And(selectNodePtrV, selectTypeFlagMatchV);

		// Now, select between the true measured distances and FLT_MAX, depending on whether
		// these objects fit the acceptance criteria above. If FLT_MAX is selected here,
		// it won't be closer than objects we have previously found, so the objects being
		// looked at now won't be chosen. /FF
		const Vec4V d2V = SelectFT(combinedSelect, maxDistV, d2BeforeMaskV);

		// Compare the squared distance of these objects vs. the squared distances
		// of the closest objects found so far. /FF
		const VecBoolV selectCloserThan1V = IsLessThan(d2V, close1D2V);
		const VecBoolV selectCloserThan2V = IsLessThan(d2V, close2D2V);
		const VecBoolV selectCloserThan3V = IsLessThan(d2V, close3D2V);
		const VecBoolV selectCloserThan4V = IsLessThan(d2V, close4D2V);

		// Compute some temporary vectors for the logic of how to move the elements.
		// For example, temp2D2V is used for the squared distance of the 2nd closest
		// object. If we are going to replace that element, it would either be replaced
		// by the current distance (if the new object is closest than the old 2nd closest,
		// but not closer than the #1 closest one) or by the old distance for the #1 closest
		// one (if that's going to get replaced). /FF
		const Vec4V temp2D2V = SelectFT(selectCloserThan1V, d2V, close1D2V);
		const Vec4V temp3D2V = SelectFT(selectCloserThan2V, d2V, close2D2V);
		const Vec4V temp4D2V = SelectFT(selectCloserThan3V, d2V, close3D2V);
#if SPATIALARRAY64BIT
		const Vec4V temp2NodesUpperV = SelectFT(selectCloserThan1V, nodesUpperV, close1NodesUpperV);
		const Vec4V temp3NodesUpperV = SelectFT(selectCloserThan2V, nodesUpperV, close2NodesUpperV);
		const Vec4V temp4NodesUpperV = SelectFT(selectCloserThan3V, nodesUpperV, close3NodesUpperV);
		const Vec4V temp2NodesLowerV = SelectFT(selectCloserThan1V, nodesLowerV, close1NodesLowerV);
		const Vec4V temp3NodesLowerV = SelectFT(selectCloserThan2V, nodesLowerV, close2NodesLowerV);
		const Vec4V temp4NodesLowerV = SelectFT(selectCloserThan3V, nodesLowerV, close3NodesLowerV);
#else
		const Vec4V temp2NodesV = SelectFT(selectCloserThan1V, nodesV, close1NodesV);
		const Vec4V temp3NodesV = SelectFT(selectCloserThan2V, nodesV, close2NodesV);
		const Vec4V temp4NodesV = SelectFT(selectCloserThan3V, nodesV, close3NodesV);
#endif

		// Finally, compute the new first, second, and third closest objects found
		// so far. /FF
		close4D2V = SelectFT(selectCloserThan4V, close4D2V, temp4D2V);
		close3D2V = SelectFT(selectCloserThan3V, close3D2V, temp3D2V);
		close2D2V = SelectFT(selectCloserThan2V, close2D2V, temp2D2V);
		close1D2V = SelectFT(selectCloserThan1V, close1D2V, d2V);
#if SPATIALARRAY64BIT
		close4NodesUpperV = SelectFT(selectCloserThan4V, close4NodesUpperV, temp4NodesUpperV);
		close3NodesUpperV = SelectFT(selectCloserThan3V, close3NodesUpperV, temp3NodesUpperV);
		close2NodesUpperV = SelectFT(selectCloserThan2V, close2NodesUpperV, temp2NodesUpperV);
		close1NodesUpperV = SelectFT(selectCloserThan1V, close1NodesUpperV, nodesUpperV);

		close4NodesLowerV = SelectFT(selectCloserThan4V, close4NodesLowerV, temp4NodesLowerV);
		close3NodesLowerV = SelectFT(selectCloserThan3V, close3NodesLowerV, temp3NodesLowerV);
		close2NodesLowerV = SelectFT(selectCloserThan2V, close2NodesLowerV, temp2NodesLowerV);
		close1NodesLowerV = SelectFT(selectCloserThan1V, close1NodesLowerV, nodesLowerV);
#else
		close4NodesV = SelectFT(selectCloserThan4V, close4NodesV, temp4NodesV);
		close3NodesV = SelectFT(selectCloserThan3V, close3NodesV, temp3NodesV);
		close2NodesV = SelectFT(selectCloserThan2V, close2NodesV, temp2NodesV);
		close1NodesV = SelectFT(selectCloserThan1V, close1NodesV, nodesV);
#endif

		// Move on in the arrays. /FF
		objXPtr++;
		objYPtr++;
		objZPtr++;
#if SPATIALARRAY64BIT
		nodesPtrUpper++;
		nodesPtrLower++;
#else
		nodesPtr++;
#endif
		typeFlagPtr++;
	}

	// Store out the squared distances and pointers to memory. /FF
	Vec4V closest4D2V[4];
	closest4D2V[0] = close1D2V;
	closest4D2V[1] = close2D2V;
	closest4D2V[2] = close3D2V;
	closest4D2V[3] = close4D2V;

#if SPATIALARRAY64BIT
	Vec4V closest4NodesUpperV[4];
	closest4NodesUpperV[0] = close1NodesUpperV;
	closest4NodesUpperV[1] = close2NodesUpperV;
	closest4NodesUpperV[2] = close3NodesUpperV;
	closest4NodesUpperV[3] = close4NodesUpperV;

	Vec4V closest4NodesLowerV[4];
	closest4NodesLowerV[0] = close1NodesLowerV;
	closest4NodesLowerV[1] = close2NodesLowerV;
	closest4NodesLowerV[2] = close3NodesLowerV;
	closest4NodesLowerV[3] = close4NodesLowerV;

	return sPickFromSortedArrays(closest4D2V, closest4NodesUpperV, closest4NodesLowerV, maxDist, found, 4);
#else
	Vec4V closest4NodesV[4];
	closest4NodesV[0] = close1NodesV;
	closest4NodesV[1] = close2NodesV;
	closest4NodesV[2] = close3NodesV;
	closest4NodesV[3] = close4NodesV;

	return sPickFromSortedArrays(closest4D2V, closest4NodesV, maxDist, found, 4);
#endif
}


int CSpatialArray::FindInSphere(Vec3V_In centerV, ScalarV_In radiusV,
		FindResult *found, int maxFound) const
{
	SA_PF_FUNC(FindInSphere);

	SPATIALARRAYTHREADLOCK;

	// TODO: Probably operate on 8 objects instead of 4, to keep vector pipeline busy.
	// TODO: Maybe use cache prefetch and/or clear instructions.
	// TODO: Add some protection about assumption of 32 bit pointers, etc. /FF

	const Vec4V radius2V(Scale(radiusV, radiusV));

	const int numObj = m_NumObj;

	const Vec4V* RESTRICT objXPtr = (const Vec4V*)m_PosXArray;
	const Vec4V* RESTRICT objYPtr = (const Vec4V*)m_PosYArray;
	const Vec4V* RESTRICT objZPtr = (const Vec4V*)m_PosZArray;

	const Vec4V centerxV(SplatX(centerV));
	const Vec4V centeryV(SplatY(centerV));
	const Vec4V centerzV(SplatZ(centerV));

#if SPATIALARRAY64BIT
	const static int tempArraySize = (kMaxObjForTempBuffer*sizeof(u32))/sizeof(Vec4V);
	Vec4V foundArrayBuffUpper[tempArraySize];
	Vec4V foundArrayBuffLower[tempArraySize];
	u32* RESTRICT foundArrayUpper = (u32*)foundArrayBuffUpper;
	u32* RESTRICT foundArrayLower = (u32*)foundArrayBuffLower;

	// Make really sure they got aligned properly. /FF
	Assertf((((size_t)foundArrayUpper) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArrayUpper);
	Assertf((((size_t)foundArrayLower) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArrayLower);

	u32* RESTRICT foundArrayPtrUpper = foundArrayUpper;
	u32* RESTRICT foundArrayPtrLower = foundArrayLower;
#else
	// Reserve a vector-aligned array on the stack. /FF
	// Note: I believe this would work too:  ALIGNAS(16) CSpatialArrayNode* foundArrayBuff[kMaxObjForTempBuffer] ;
	const static int tempArraySize = (kMaxObjForTempBuffer*sizeof(CSpatialArrayNodeAddr))/sizeof(Vec4V);
	Vec4V foundArrayBuff[ tempArraySize ];
	CSpatialArrayNodeAddr* RESTRICT foundArray = (CSpatialArrayNodeAddr*)foundArrayBuff;

	// Make really sure it got aligned properly. /FF
	Assertf((((size_t)foundArray) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArray);

	CSpatialArrayNodeAddr* RESTRICT foundArrayPtr = foundArray;
#endif

	Vec4V distanceArray[ kMaxObjForTempBuffer ];

	int numfound = 0;
	Vec4V* RESTRICT distanceArrayPtr = distanceArray;
	for(int i = 0; i < numObj; i += 4)
	{
		const Vec4V xxV = *objXPtr;
		const Vec4V yyV = *objYPtr;
		const Vec4V zzV = *objZPtr;
#if SPATIALARRAY64BIT
		const Vec4V nodesUpperV = *(Vec4V*)&m_NodeArrayUpper[i];
		const Vec4V nodesLowerV = *(Vec4V*)&m_NodeArrayLower[i];
#else
		const Vec4V nodesV = *(Vec4V*)&m_NodeArray[i];
#endif

		const Vec4V dxV = Subtract(xxV, centerxV);
		const Vec4V dyV = Subtract(yyV, centeryV);
		const Vec4V dzV = Subtract(zzV, centerzV);

		const Vec4V dx2V = Scale(dxV, dxV);
		const Vec4V dxy2V = AddScaled(dx2V, dyV, dyV);
		const Vec4V d2V = AddScaled(dxy2V, dzV, dzV);

		const Vec4V selectWithinSphereV(IsLessThan(d2V, radius2V));

#if SPATIALARRAY64BIT
		const Vec4V nodesWithinSphereUpperV = And(selectWithinSphereV, nodesUpperV);
		const Vec4V nodesWithinSphereLowerV = And(selectWithinSphereV, nodesLowerV);
#else
		const Vec4V nodesWithinSphereV = And(selectWithinSphereV, nodesV);
#endif

		objXPtr++;
		objYPtr++;
		objZPtr++;

		Vec4V* RESTRICT oldDistancePtr = distanceArrayPtr;

#if SPATIALARRAY64BIT
		u32* RESTRICT oldFoundArrayPtrUpper = foundArrayPtrUpper;
		foundArrayPtrUpper += 4;

		u32* RESTRICT oldFoundArrayPtrLower = foundArrayPtrLower;
		foundArrayPtrLower += 4;

		*(Vec4V*)oldFoundArrayPtrLower = nodesWithinSphereLowerV;
		*(Vec4V*)oldFoundArrayPtrUpper = nodesWithinSphereUpperV;
#else
		CSpatialArrayNodeAddr* RESTRICT oldFoundArrayPtr = foundArrayPtr;
		foundArrayPtr += 4;

		*(Vec4V*)oldFoundArrayPtr = nodesWithinSphereV;
#endif
		distanceArrayPtr ++;
		*oldDistancePtr = d2V;
	}

	float* floatDistanceArray = reinterpret_cast<float*>(distanceArray);
	for(int i = 0; i < numObj; i++)
	{
#if SPATIALARRAY64BIT
		CSpatialArrayNode* addr = NodePtrFromUpperLower(foundArrayUpper[i], foundArrayLower[i]);
#else
		CSpatialArrayNode* addr = (CSpatialArrayNode*)foundArray[i];
#endif
		if(addr)
		{
			found[numfound].m_Node = addr;
			found[numfound].m_DistanceSq = floatDistanceArray[i];
			numfound++;
			if(numfound >= maxFound)
			{
				break;
			}
		}
	}

	return numfound;
}


int CSpatialArray::FindInCylinderXY(Vec2V_In centerXYV, ScalarV_In radiusV,
		CSpatialArrayNode **found, int maxFound) const
{
	SA_PF_FUNC(FindInCylinderXY);

	SPATIALARRAYTHREADLOCK;

	const Vec4V radius2V(Scale(radiusV, radiusV));

	const int numObj = m_NumObj;

	const Vec4V* RESTRICT objXPtr = (const Vec4V*)m_PosXArray;
	const Vec4V* RESTRICT objYPtr = (const Vec4V*)m_PosYArray;

	const Vec4V centerxV(SplatX(centerXYV));
	const Vec4V centeryV(SplatY(centerXYV));

#if SPATIALARRAY64BIT
	const static int tempArraySize = (kMaxObjForTempBuffer*sizeof(u32))/sizeof(Vec4V);
	Vec4V foundArrayBuffUpper[tempArraySize];
	Vec4V foundArrayBuffLower[tempArraySize];
	u32* RESTRICT foundArrayUpper = (u32*)foundArrayBuffUpper;
	u32* RESTRICT foundArrayLower = (u32*)foundArrayBuffLower;

	// Make really sure they got aligned properly. /FF
	Assertf((((size_t)foundArrayUpper) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArrayUpper);
	Assertf((((size_t)foundArrayLower) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArrayLower);

	u32* RESTRICT foundArrayPtrUpper = foundArrayUpper;
	u32* RESTRICT foundArrayPtrLower = foundArrayLower;
#else
	// Reserve a vector-aligned array on the stack. /FF
	// Note: I believe this would work too:  ALIGNAS(16) CSpatialArrayNode* foundArrayBuff[kMaxObjForTempBuffer] ;
	const static int tempArraySize = (kMaxObjForTempBuffer*sizeof(CSpatialArrayNodeAddr))/sizeof(Vec4V);
	Vec4V foundArrayBuff[ tempArraySize ];
	CSpatialArrayNodeAddr* RESTRICT foundArray = (CSpatialArrayNodeAddr*)foundArrayBuff;

	// Make really sure it got aligned properly. /FF
	Assertf((((size_t)foundArray) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArray);

	CSpatialArrayNodeAddr* RESTRICT foundArrayPtr = foundArray;
#endif

	for(int i = 0; i < numObj; i += 4)
	{
		const Vec4V xxV = *objXPtr;
		const Vec4V yyV = *objYPtr;
#if SPATIALARRAY64BIT
		const Vec4V nodesUpperV = *(Vec4V*)&m_NodeArrayUpper[i];
		const Vec4V nodesLowerV = *(Vec4V*)&m_NodeArrayLower[i];
#else
		const Vec4V nodesV = *(Vec4V*)&m_NodeArray[i];
#endif

		const Vec4V dxV = Subtract(xxV, centerxV);
		const Vec4V dyV = Subtract(yyV, centeryV);

		const Vec4V dx2V = Scale(dxV, dxV);
		const Vec4V d2V = AddScaled(dx2V, dyV, dyV);

		const Vec4V selectWithinCylV(IsLessThan(d2V, radius2V));

#if SPATIALARRAY64BIT
		const Vec4V nodesWithinCylUpperV = And(selectWithinCylV, nodesUpperV);
		const Vec4V nodesWithinCylLowerV = And(selectWithinCylV, nodesLowerV);
#else
		const Vec4V nodesWithinCylV = And(selectWithinCylV, nodesV);
#endif

		objXPtr++;
		objYPtr++;

#if SPATIALARRAY64BIT
		u32* RESTRICT oldFoundArrayPtrUpper = foundArrayPtrUpper;
		foundArrayPtrUpper += 4;

		u32* RESTRICT oldFoundArrayPtrLower = foundArrayPtrLower;
		foundArrayPtrLower += 4;

		*(Vec4V*)oldFoundArrayPtrLower = nodesWithinCylLowerV;
		*(Vec4V*)oldFoundArrayPtrUpper = nodesWithinCylUpperV;
#else
		CSpatialArrayNodeAddr* RESTRICT oldFoundArrayPtr = foundArrayPtr;
		foundArrayPtr += 4;

		*(Vec4V*)oldFoundArrayPtr = nodesWithinCylV;
#endif
	}

#if SPATIALARRAY64BIT
	return CreateCompactNodePointerArray(foundArrayUpper, foundArrayLower, numObj, found, maxFound);
#else
	return CreateCompactNodePointerArray(foundArray, numObj, found, maxFound);
#endif
}


int CSpatialArray::FindInSphere(Vec3V_In centerV, float radius, FindResult *found,
		int maxFound) const
{
	const ScalarV radiusV(LoadScalar32IntoScalarV(radius));
	return FindInSphere(centerV, radiusV, found, maxFound);
}


int CSpatialArray::FindInSphereOfType(Vec3V_In centerV, ScalarV_In radiusV,
		CSpatialArrayNode **found, int maxFound,
		const u32 &typeFlagsToCareAbout, const u32 &typeFlagValues) const
{
	SA_PF_FUNC(FindInSphereOfType);

	// If this fails, there are values set in flagValues that are not in flagsToChange,
	// which we are probably better off if the user could avoid, so we don't have to
	// spend time on masking them here. /FF
	Assert((typeFlagValues & ~typeFlagsToCareAbout) == 0);

	SPATIALARRAYTHREADLOCK;

	// TODO: Probably operate on 8 objects instead of 4, to keep vector pipeline busy.
	// TODO: Maybe use cache prefetch and/or clear instructions.
	// TODO: Add some protection about assumption of 32 bit pointers, etc. /FF

	const Vec4V radius2V(Scale(radiusV, radiusV));

	const int numObj = m_NumObj;

	// Load the type flag stuff into vector registers. Note that we intentionally
	// pass in these by reference, requiring the user to put them in memory, because
	// if they were passed in in general purpose registers, we would need to store
	// them to memory and load them back anyway. Could pass them in in vector
	// registers, of course, but that's probably not worth the trouble. /FF
	const Vec4V typeFlagsToCareAboutV = Vec4V(LoadScalar32IntoScalarV(typeFlagsToCareAbout));
	const Vec4V typeFlagValuesV = Vec4V(LoadScalar32IntoScalarV(typeFlagValues));

	const Vec4V* RESTRICT objXPtr = (const Vec4V*)m_PosXArray;
	const Vec4V* RESTRICT objYPtr = (const Vec4V*)m_PosYArray;
	const Vec4V* RESTRICT objZPtr = (const Vec4V*)m_PosZArray;
	const Vec4V* RESTRICT typeFlagPtr = (const Vec4V*)m_TypeFlagArray;

	const Vec4V centerxV(SplatX(centerV));
	const Vec4V centeryV(SplatY(centerV));
	const Vec4V centerzV(SplatZ(centerV));

#if SPATIALARRAY64BIT
	const static int tempArraySize = (kMaxObjForTempBuffer*sizeof(u32))/sizeof(Vec4V);
	Vec4V foundArrayBuffUpper[tempArraySize];
	Vec4V foundArrayBuffLower[tempArraySize];
	u32* RESTRICT foundArrayUpper = (u32*)foundArrayBuffUpper;
	u32* RESTRICT foundArrayLower = (u32*)foundArrayBuffLower;

	// Make really sure they got aligned properly. /FF
	Assertf((((size_t)foundArrayUpper) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArrayUpper);
	Assertf((((size_t)foundArrayLower) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArrayLower);

	u32* RESTRICT foundArrayPtrUpper = foundArrayUpper;
	u32* RESTRICT foundArrayPtrLower = foundArrayLower;
#else
	// Reserve a vector-aligned array on the stack. /FF
	// Note: I believe this would work too:  ALIGNAS(16) CSpatialArrayNode* foundArrayBuff[kMaxObjForTempBuffer] ;
	const static int tempArraySize = (kMaxObjForTempBuffer*sizeof(CSpatialArrayNodeAddr))/sizeof(Vec4V);
	Vec4V foundArrayBuff[ tempArraySize ];
	CSpatialArrayNodeAddr* RESTRICT foundArray = (CSpatialArrayNodeAddr*)foundArrayBuff;

	// Make really sure it got aligned properly. /FF
	Assertf((((size_t)foundArray) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArray);

	CSpatialArrayNodeAddr* RESTRICT foundArrayPtr = foundArray;
#endif

	for(int i = 0; i < numObj; i += 4)
	{
		const Vec4V xxV = *objXPtr;
		const Vec4V yyV = *objYPtr;
		const Vec4V zzV = *objZPtr;
#if SPATIALARRAY64BIT
		const Vec4V nodesUpperV = *(Vec4V*)&m_NodeArrayUpper[i];
		const Vec4V nodesLowerV = *(Vec4V*)&m_NodeArrayLower[i];
#else
		const Vec4V nodesV = *(Vec4V*)&m_NodeArray[i];
#endif
		const Vec4V objTypeFlagsV = *typeFlagPtr;

		const Vec4V dxV = Subtract(xxV, centerxV);
		const Vec4V dyV = Subtract(yyV, centeryV);
		const Vec4V dzV = Subtract(zzV, centerzV);

		const Vec4V dx2V = Scale(dxV, dxV);
		const Vec4V dxy2V = AddScaled(dx2V, dyV, dyV);
		const Vec4V d2V = AddScaled(dxy2V, dzV, dzV);

		const VecBoolV selectWithinSphereV = IsLessThan(d2V, radius2V);

		// Get the type flags and filter out the ones we don't care about.
		const Vec4V objTypeFlagsCaredAboutV = And(objTypeFlagsV, typeFlagsToCareAboutV);

		// See if the remaining ones have the values we want. If so, selectTypeFlagMatchV
		// should be all 0xffffffff.
		const VecBoolV selectTypeFlagMatchV = IsEqualInt(objTypeFlagsCaredAboutV, typeFlagValuesV);

		// Compute the mask for matching both the type and being within the sphere.
		const VecBoolV selectMatch = And(selectWithinSphereV, selectTypeFlagMatchV);

		// Mask out the pointers for the nodes that didn't match.
#if SPATIALARRAY64BIT
		const Vec4V matchingNodesUpperV = And((Vec4V)selectMatch, nodesUpperV);
		const Vec4V matchingNodesLowerV = And((Vec4V)selectMatch, nodesLowerV);
#else
		const Vec4V matchingNodesV = And((Vec4V)selectMatch, nodesV);
#endif

		objXPtr++;
		objYPtr++;
		objZPtr++;
		typeFlagPtr++;

#if SPATIALARRAY64BIT
		u32* RESTRICT oldFoundArrayPtrUpper = foundArrayPtrUpper;
		foundArrayPtrUpper += 4;

		u32* RESTRICT oldFoundArrayPtrLower = foundArrayPtrLower;
		foundArrayPtrLower += 4;

		*(Vec4V*)oldFoundArrayPtrLower = matchingNodesLowerV;
		*(Vec4V*)oldFoundArrayPtrUpper = matchingNodesUpperV;
#else
		CSpatialArrayNodeAddr* RESTRICT oldFoundArrayPtr = foundArrayPtr;
		foundArrayPtr += 4;

		*(Vec4V*)oldFoundArrayPtr = matchingNodesV;
#endif

	}

#if SPATIALARRAY64BIT
	return CreateCompactNodePointerArray(foundArrayUpper, foundArrayLower, numObj, found, maxFound);
#else
	return CreateCompactNodePointerArray(foundArray, numObj, found, maxFound);
#endif
}


int CSpatialArray::FindBelowZ(ScalarV_In scalar_thresholdZV,
		CSpatialArrayNode **found, int maxFound) const
{
	SA_PF_FUNC(FindBelowZ);

	SPATIALARRAYTHREADLOCK;

	// TODO: Probably operate on 8 objects instead of 4, to keep vector pipeline busy.
	// TODO: Maybe use cache prefetch and/or clear instructions.
	// TODO: Add some protection about assumption of 32 bit pointers, etc. /FF

	const int numObj = m_NumObj;

	const Vec4V* RESTRICT objZPtr = (const Vec4V*)m_PosZArray;

#if SPATIALARRAY64BIT
	const static int tempArraySize = (kMaxObjForTempBuffer*sizeof(u32))/sizeof(Vec4V);
	Vec4V foundArrayBuffUpper[tempArraySize];
	Vec4V foundArrayBuffLower[tempArraySize];
	u32* RESTRICT foundArrayUpper = (u32*)foundArrayBuffUpper;
	u32* RESTRICT foundArrayLower = (u32*)foundArrayBuffLower;

	// Make really sure they got aligned properly. /FF
	Assertf((((size_t)foundArrayUpper) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArrayUpper);
	Assertf((((size_t)foundArrayLower) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArrayLower);

	u32* RESTRICT foundArrayPtrUpper = foundArrayUpper;
	u32* RESTRICT foundArrayPtrLower = foundArrayLower;
#else
	// Reserve a vector-aligned array on the stack. /FF
	// Note: I believe this would work too:  ALIGNAS(16) CSpatialArrayNode* foundArrayBuff[kMaxObjForTempBuffer] ;
	const static int tempArraySize = (kMaxObjForTempBuffer*sizeof(CSpatialArrayNodeAddr))/sizeof(Vec4V);
	Vec4V foundArrayBuff[ tempArraySize ];
	CSpatialArrayNodeAddr* RESTRICT foundArray = (CSpatialArrayNodeAddr*)foundArrayBuff;

	// Make really sure it got aligned properly. /FF
	Assertf((((size_t)foundArray) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArray);

	CSpatialArrayNodeAddr* RESTRICT foundArrayPtr = foundArray;
#endif

	const Vec4V thresholdZV(scalar_thresholdZV);

	for(int i = 0; i < numObj; i += 4)
	{
		const Vec4V zzV = *objZPtr;
#if SPATIALARRAY64BIT
		const Vec4V nodesUpperV = *(Vec4V*)&m_NodeArrayUpper[i];
		const Vec4V nodesLowerV = *(Vec4V*)&m_NodeArrayLower[i];
#else
		const Vec4V nodesV = *(Vec4V*)&m_NodeArray[i];
#endif
		const VecBoolV selectMatch = IsLessThan(zzV, thresholdZV);

		// Mask out the pointers for the nodes that didn't match.
#if SPATIALARRAY64BIT
		const Vec4V matchingNodesUpperV = And((Vec4V)selectMatch, nodesUpperV);
		const Vec4V matchingNodesLowerV = And((Vec4V)selectMatch, nodesLowerV);
#else
		const Vec4V matchingNodesV = And((Vec4V)selectMatch, nodesV);
#endif

		objZPtr++;

#if SPATIALARRAY64BIT
		u32* RESTRICT oldFoundArrayPtrUpper = foundArrayPtrUpper;
		foundArrayPtrUpper += 4;

		u32* RESTRICT oldFoundArrayPtrLower = foundArrayPtrLower;
		foundArrayPtrLower += 4;

		*(Vec4V*)oldFoundArrayPtrLower = matchingNodesLowerV;
		*(Vec4V*)oldFoundArrayPtrUpper = matchingNodesUpperV;
#else
		CSpatialArrayNodeAddr* RESTRICT oldFoundArrayPtr = foundArrayPtr;
		foundArrayPtr += 4;

		*(Vec4V*)oldFoundArrayPtr = matchingNodesV;
#endif

	}

#if SPATIALARRAY64BIT
	return CreateCompactNodePointerArray(foundArrayUpper, foundArrayLower, numObj, found, maxFound);
#else
	return CreateCompactNodePointerArray(foundArray, numObj, found, maxFound);
#endif
}


int CSpatialArray::FindInSphereOfType(Vec3V_In centerV, float radius,
		CSpatialArrayNode **found,
		int maxFound, const u32 &typeFlagsToCareAbout, const u32 &typeFlagValues) const
{
	const ScalarV radiusV(LoadScalar32IntoScalarV(radius));
	return FindInSphereOfType(centerV, radiusV, found, maxFound, typeFlagsToCareAbout, typeFlagValues);
}


int CSpatialArray::FindNearSegment(Vec3V_In segPos1V, Vec3V_In segPos2V, const float& distSegToObjCenter, CSpatialArrayNode** found, int maxFound) const
{
	SA_PF_FUNC(FindNearSegment);

	SPATIALARRAYTHREADLOCK;

	const ScalarV thresholdDistV = LoadScalar32IntoScalarV(distSegToObjCenter);
	const Vec4V thresholdDistSqV = Vec4V(Scale(thresholdDistV, thresholdDistV));

	const int numObj = m_NumObj;

	const Vec4V* RESTRICT objXPtr = (const Vec4V*)m_PosXArray;
	const Vec4V* RESTRICT objYPtr = (const Vec4V*)m_PosYArray;
	const Vec4V* RESTRICT objZPtr = (const Vec4V*)m_PosZArray;

#if SPATIALARRAY64BIT
	const static int tempArraySize = (kMaxObjForTempBuffer*sizeof(u32))/sizeof(Vec4V);
	Vec4V foundArrayBuffUpper[tempArraySize];
	Vec4V foundArrayBuffLower[tempArraySize];
	u32* RESTRICT foundArrayUpper = (u32*)foundArrayBuffUpper;
	u32* RESTRICT foundArrayLower = (u32*)foundArrayBuffLower;

	// Make really sure they got aligned properly. /FF
	Assertf((((size_t)foundArrayUpper) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArrayUpper);
	Assertf((((size_t)foundArrayLower) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArrayLower);

	u32* RESTRICT foundArrayPtrUpper = foundArrayUpper;
	u32* RESTRICT foundArrayPtrLower = foundArrayLower;
#else
	// Reserve a vector-aligned array on the stack. /FF
	// Note: I believe this would work too:  ALIGNAS(16) CSpatialArrayNode* foundArrayBuff[kMaxObjForTempBuffer] ;
	const static int tempArraySize = (kMaxObjForTempBuffer*sizeof(CSpatialArrayNodeAddr))/sizeof(Vec4V);
	Vec4V foundArrayBuff[ tempArraySize ];
	CSpatialArrayNodeAddr* RESTRICT foundArray = (CSpatialArrayNodeAddr*)foundArrayBuff;

	// Make really sure it got aligned properly. /FF
	Assertf((((size_t)foundArray) & 0xf) == 0, "Got address %p, expected 16 byte alignment.", foundArray);

	CSpatialArrayNodeAddr* RESTRICT foundArrayPtr = foundArray;
#endif

	const Vec3V segPos1To2V = Subtract(segPos2V, segPos1V);
	const Vec4V point1XV(segPos1V.GetX());
	const Vec4V point1YV(segPos1V.GetY());
	const Vec4V point1ZV(segPos1V.GetZ());

	const Vec4V deltaXV(segPos1To2V.GetX());
	const Vec4V deltaYV(segPos1To2V.GetY());
	const Vec4V deltaZV(segPos1To2V.GetZ());

	const Vec4V zeroV(V_ZERO);
	const Vec4V oneV(V_ONE);

	for(int i = 0; i < numObj; i += 4)
	{
		const Vec4V xxV = *objXPtr;
		const Vec4V yyV = *objYPtr;
		const Vec4V zzV = *objZPtr;

#if SPATIALARRAY64BIT
		const Vec4V nodesUpperV = *(Vec4V*)&m_NodeArrayUpper[i];
		const Vec4V nodesLowerV = *(Vec4V*)&m_NodeArrayLower[i];
#else
		const Vec4V nodesV = *(Vec4V*)&m_NodeArray[i];
#endif

		// Here, we will compute the T values of the closest points on the segment,
		// for the four points. It's more or less done with the same operations
		// as in geomTValues::FindTValueSegToOriginV(), and it's even more similar
		// to sFindTValueSegToPoint() in 'TaskNavBase.cpp'.

		const Vec4V ptXV = Subtract(xxV, point1XV);
		const Vec4V ptYV = Subtract(yyV, point1YV);
		const Vec4V ptZV = Subtract(zzV, point1ZV);

		const Vec4V oneDotXV = Scale(deltaXV, ptXV);
		const Vec4V oneDotXYV = AddScaled(oneDotXV, deltaYV, ptYV);
		const Vec4V oneDotV = AddScaled(oneDotXYV, deltaZV, ptZV);

		const Vec4V bothDotXV = Scale(deltaXV, deltaXV);
		const Vec4V bothDotXYV = AddScaled(bothDotXV, deltaYV, deltaYV);
		const Vec4V bothDotV = AddScaled(bothDotXYV, deltaZV, deltaZV);

		const Vec4V tOnInfLineV = InvScaleFast(oneDotV, bothDotV);
		const VecBoolV tMaxMaskV = IsGreaterThanOrEqual(tOnInfLineV, oneV);
		const Vec4V tClampedMaxV = SelectFT(tMaxMaskV, tOnInfLineV, oneV);
		const VecBoolV tMinMaskV = IsGreaterThan(oneDotV, zeroV);
		const Vec4V tClampedV = And(tClampedMaxV, Vec4V(tMinMaskV));

		// Next, compute the X, Y, and Z coordinates of the closest points
		// to each of the four objects.
		const Vec4V closestPtXV = AddScaled(point1XV, deltaXV, tClampedV);
		const Vec4V closestPtYV = AddScaled(point1YV, deltaYV, tClampedV);
		const Vec4V closestPtZV = AddScaled(point1ZV, deltaZV, tClampedV);

		// Compute the squared distance to each of these.
		const Vec4V ptToClosestXV = Subtract(closestPtXV, xxV);
		const Vec4V ptToClosestYV = Subtract(closestPtYV, yyV);
		const Vec4V ptToClosestZV = Subtract(closestPtZV, zzV);
		const Vec4V distSqXV = Scale(ptToClosestXV, ptToClosestXV);
		const Vec4V distSqXYV = AddScaled(distSqXV, ptToClosestYV, ptToClosestYV);
		const Vec4V distSqV = AddScaled(distSqXYV, ptToClosestZV, ptToClosestZV);

		// Compute a mask for which objects are close enough, and AND that with
		// the node addresses.
		const Vec4V selectNearSegV(IsLessThan(distSqV, thresholdDistSqV));
#if SPATIALARRAY64BIT
		const Vec4V nodesWithinSphereUpperV = And(selectNearSegV, nodesUpperV);
		const Vec4V nodesWithinSphereLowerV = And(selectNearSegV, nodesLowerV);
#else
		const Vec4V nodesWithinSphereV = And(selectNearSegV, nodesV);
#endif

		// Advance to the next four objects.
		objXPtr++;
		objYPtr++;
		objZPtr++;
#if SPATIALARRAY64BIT
		u32* RESTRICT oldFoundArrayPtrUpper = foundArrayPtrUpper;
		foundArrayPtrUpper += 4;

		u32* RESTRICT oldFoundArrayPtrLower = foundArrayPtrLower;
		foundArrayPtrLower += 4;

		*(Vec4V*)oldFoundArrayPtrLower = nodesWithinSphereLowerV;
		*(Vec4V*)oldFoundArrayPtrUpper = nodesWithinSphereUpperV;
#else
		CSpatialArrayNodeAddr* RESTRICT oldFoundArrayPtr = foundArrayPtr;
		foundArrayPtr += 4;

		*(Vec4V*)oldFoundArrayPtr = nodesWithinSphereV;
#endif
	}

	// Create a compact array of pointers to return to the caller.
#if SPATIALARRAY64BIT
	return CreateCompactNodePointerArray(foundArrayUpper, foundArrayLower, numObj, found, maxFound);
#else
	return CreateCompactNodePointerArray(foundArray, numObj, found, maxFound);
#endif
}

#if __DEV

void CSpatialArray::DebugDraw() const
{
	SPATIALARRAYTHREADLOCK;

	Matrix34 mtrx;
	mtrx.Identity();

	const int numObj = m_NumObj;
	for(int i = 0; i < numObj; i++)
	{
		mtrx.d.x = m_PosXArray[i];
		mtrx.d.y = m_PosYArray[i];
		mtrx.d.z = m_PosZArray[i];
		grcDebugDraw::Axis(mtrx, 1.0f);

		char buf[16];
		formatf(buf, "%04x", m_TypeFlagArray[i]);

		grcDebugDraw::Text(mtrx.d, Color_white,	buf);
	}
}

#endif	// __DEV

#if SPATIALARRAY64BIT
int CSpatialArray::CreateCompactNodePointerArray(const u32* foundArrayUpper, const u32* foundArrayLower, int numObj, CSpatialArrayNode** found, int maxFound)
#else
int CSpatialArray::CreateCompactNodePointerArray(const CSpatialArrayNodeAddr* foundArray, int numObj, CSpatialArrayNode** found, int maxFound)
#endif
{
	int numfound = 0;
	for(int i = 0; i < numObj; i++)
	{
#if SPATIALARRAY64BIT
		CSpatialArrayNode* addr = NodePtrFromUpperLower(foundArrayUpper[i], foundArrayLower[i]);
#else
		CSpatialArrayNode* addr = (CSpatialArrayNode*)foundArray[i];
#endif
		if(addr)
		{
			found[numfound] = addr;
			numfound++;
			if(numfound >= maxFound)
			{
				break;
			}
		}
	}

	return numfound;
}

//------------------------------------------------------------------------------

/* End of file sagcore/spatialarray.cpp */
