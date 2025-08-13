
//std headers
#include <limits>

// Rage headers
#include "grcore/debugdraw.h"
#include "phBound/Bound.h"
#include "phBound/boundbox.h"
#include "phBound/BoundComposite.h"
#include "phbound/BoundSphere.h"
#include "phCore/Segment.h"

// Framework headers
#include "fwmaths/Angle.h"
#include "fwmaths/geomutil.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Event/Events.h"
#include "Network/NetworkInterface.h"
#include "Objects/door.h"
#include "Objects/object.h"
#include "PedGeometryAnalyser.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Physics/GtaArchetype.h"
#include "Physics/GtaInst.h"
#include "Physics/GtaMaterialManager.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "fwscene/search/SearchVolumes.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Combat/Cover/Cover.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/motion/TaskMotionBase.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "vehicles/vehicle.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"

AI_OPTIMISATIONS()

PF_GROUP(PedGeometryAnalyserProbes);
PF_TIMER(TestCapsuleVsComposite, PedGeometryAnalyserProbes);

PF_PAGE(GTA_PedTargetting, "Gta Ped targetting probes");
PF_GROUP(PedTargetting);
PF_LINK(GTA_PedTargetting, PedTargetting);

PF_VALUE_INT(Number_LOS_from_AI, PedTargetting);
PF_VALUE_INT(Number_CanPedTargetPed_calls, PedTargetting);
PF_VALUE_INT(Number_CanPedTargetPoint_calls, PedTargetting);
PF_VALUE_INT(Num_LOS_saved_in_async_cache, PedTargetting);
PF_VALUE_INT(Num_async_probes_rejected, PedTargetting);
PF_VALUE_INT(Num_Cache_full_events, PedTargetting);
PF_VALUE_INT(Num_LOS_not_processed_in_time, PedTargetting);
PF_VALUE_INT(Num_LOS_not_async, PedTargetting);

namespace AIStats
{
	EXT_PF_TIMER(CPedGeometryAnalyser_IsInAir);
}
using namespace AIStats;

// grrr. This macro conflicts under visual studio.
#if defined (MSVC_COMPILER)
#ifdef max
#undef max
#endif //max
#endif //MSVC_COMPILER

#define __PEDGEOM_DEBUG_BOXES	(__DEV && 0)

int gs_iNumWorldProcessLineOfSightUnCached = 0;

void IncPedAiLosCounter(void)
{
	gs_iNumWorldProcessLineOfSightUnCached++;
}

const u32 CEntityBoundAI::ms_iMaxNumBoxes = 178;
const float CEntityBoundAI::ms_fPedHalfHeight = 1.0f;

const u32 CEntityBoundAI::ms_iSideFlags[4] =
{
	1 << FRONT,
	1 << LEFT,
	1 << REAR,
	1 << RIGHT
};

const u32 CEntityBoundAI::ms_iCornerFlags[4] =
{
	ms_iSideFlags[FRONT] | ms_iSideFlags[LEFT],
	ms_iSideFlags[REAR] | ms_iSideFlags[LEFT],
	ms_iSideFlags[REAR] | ms_iSideFlags[RIGHT],
	ms_iSideFlags[FRONT] | ms_iSideFlags[RIGHT]
};

CEntityBoundAI::CEntityBoundAI(const CEntity & entity, const float& fZPlane, const float& fPedRadius,const bool bCalculateTopAndBottomHeight, const Matrix34* pOptionalOverrideMatrix, const s32 iNumSubBoundsToIgnore, const s32 * iSubBoundIndicesToIgnore, const s32 iNumSubBoundsToUseExclusively, const s32 * iSubBoundIndicesToUseExclusively, const float fPedHeight)
{
	Init(entity,fZPlane,fPedRadius,bCalculateTopAndBottomHeight, pOptionalOverrideMatrix, iNumSubBoundsToIgnore, iSubBoundIndicesToIgnore, iNumSubBoundsToUseExclusively, iSubBoundIndicesToUseExclusively, fPedHeight);
}

CEntityBoundAI::CEntityBoundAI(const Vector3 & vEntityMin, const Vector3 & vEntityMax, const Matrix34 & matOrientation, const float& fZPlane, const float& fPedRadius, const bool bCalculateTopAndBottomHeight)
{
	Init(vEntityMin, vEntityMax, matOrientation, fZPlane, fPedRadius, bCalculateTopAndBottomHeight);
}

CEntityBoundAI::CEntityBoundAI(const Vector3 & vEntityMin, const Vector3 & vEntityMax, const Matrix34 & matOrientation, const float& fZPlane, const Vector3& vExpandBox, const bool bCalculateTopAndBottomHeight)
{
	ProjectBoundingBoxOntoZPlane(vEntityMin - vExpandBox, vEntityMax + vExpandBox, matOrientation, fZPlane, bCalculateTopAndBottomHeight);
	ComputeCentre(fZPlane);
	ComputeSphere();
}

void CEntityBoundAI::Init(const CEntity & entity, const float& fZPlane, const float& fPedRadius, const bool bCalculateTopAndBottomHeight, const Matrix34* pOptionalOverrideMatrix, const s32 iNumSubBoundsToIgnore, const s32 * iSubBoundIndicesToIgnore, const s32 iNumSubBoundsToUseExclusively, const s32 * iSubBoundIndicesToUseExclusively, const float fPedHeight)
{
	Assert(entity.GetIsPhysical());
	const CPhysical& physical = static_cast<const CPhysical&>(entity);

	const Mat34V* pMatrixPtr = reinterpret_cast<const Mat34V*>(pOptionalOverrideMatrix);
	if(!pOptionalOverrideMatrix)
	{
		pMatrixPtr = &physical.GetMatrixRef();
	}
	ComputeCorners(entity,fZPlane,fPedRadius,bCalculateTopAndBottomHeight, *pMatrixPtr, iNumSubBoundsToIgnore, iSubBoundIndicesToIgnore, iNumSubBoundsToUseExclusively, iSubBoundIndicesToUseExclusively, fPedHeight);
	ComputeCentre(fZPlane);
	ComputeSphere();

#if __DEBUG
	if(m_sphere.GetRadiusf()==0.0f)
	{
		u32 mi = entity.GetModelIndex();
		CBaseModelInfo * pModelInfo = entity.GetBaseModelInfo();
		printf("ERROR in CEntityBoundAI.  Calculated bounds for entity has zero radius.\nModel Index : %i\n", mi);
		printf("ModelInfo radius : %.2f, min (%.2f, %.2f, %.2f), max (%.2f, %.2f, %.2f)\n",
			pModelInfo->GetBoundingSphereRadius(),
			pModelInfo->GetBoundingBoxMin().x, pModelInfo->GetBoundingBoxMin().y, pModelInfo->GetBoundingBoxMin().z,
			pModelInfo->GetBoundingBoxMax().x, pModelInfo->GetBoundingBoxMax().y, pModelInfo->GetBoundingBoxMax().z
		);
		m_sphere.SetRadius( ScalarV(0.1f) );
		Assert(m_sphere.GetRadiusf()!=0);
	}
#endif

}

bool CEntityBoundAI::Init(const CCarDoor & carDoor, const CVehicle * pParent, const float fZPlane, const float fPedRadius, const bool bCalculateTopAndBottomHeight)
{
	Vector3 vBoundBoxMin, vBoundBoxMax;
	if(!carDoor.GetLocalBoundingBox(pParent, vBoundBoxMin, vBoundBoxMax))
	{
		return false;
	}
	
	Matrix34 localMat, doorMat;
	if( !carDoor.GetLocalMatrix( pParent, localMat) )
	{
		return false;
	}

	doorMat.Dot(localMat, MAT34V_TO_MATRIX34(pParent->GetMatrix()));

	const Vector3 vExpand(fPedRadius, fPedRadius, fPedRadius);
	ProjectBoundingBoxOntoZPlane(vBoundBoxMin - vExpand, vBoundBoxMax + vExpand, doorMat, fZPlane, bCalculateTopAndBottomHeight);
	ComputeCentre(fZPlane);
	ComputeSphere();

	return true;
}

void CEntityBoundAI::Init(const Vector3 & vEntityMin, const Vector3 & vEntityMax, const Matrix34 & matOrientation, const float& fZPlane, const float& fPedRadius, const bool bCalculateTopAndBottomHeight)
{
	const Vector3 vExpand(fPedRadius, fPedRadius, fPedRadius);
	ProjectBoundingBoxOntoZPlane(vEntityMin - vExpand, vEntityMax + vExpand, matOrientation, fZPlane, bCalculateTopAndBottomHeight);
	ComputeCentre(fZPlane);
	ComputeSphere();
}

void CEntityBoundAI::GetCorners(Vector3* corners) const
{
	int i;
	for(i=0;i<4;i++)
	{
		corners[i]=m_corners[i];
	}
}

void CEntityBoundAI::GetSphere(spdSphere& sphere) const
{
	sphere=m_sphere;
}

void CEntityBoundAI::GetPlanes(Vector3* normals, float* ds) const
{
	if(!ds)
	{
		float ds[4];
		ComputeBoundingBoxPlanes(normals,ds);
	}
	else
	{
		ComputeBoundingBoxPlanes(normals,ds);
	}
}
void CEntityBoundAI::GetPlanes(Vector4* planes) const
{
	ComputeBoundingBoxPlanes(planes);
}
void CEntityBoundAI::GetSegmentPlanes(Vector3* normals, float* ds) const
{
	ComputeBoundingBoxPlanes(normals,ds);
}

void CEntityBoundAI::ComputeCentre(const float& fZPlane)
{
	// Original code, before optimization:
	//	m_centre.x = m_centre.y = 0.0f;
	//	m_centre.z = fHeight;
	//
	//	for (s32 i=0; i<4; i++)
	//	{
	//		m_centre.x += m_corners[i].x;
	//		m_centre.y += m_corners[i].y;
	//	}
	//
	//	m_centre.x *= 0.25f;
	//	m_centre.y *= 0.25f; 

	// Load the corners and the height as vectors.
	const Vec3V pt1V = RCC_VEC3V(m_corners[0]);
	const Vec3V pt2V = RCC_VEC3V(m_corners[1]);
	const Vec3V pt3V = RCC_VEC3V(m_corners[2]);
	const Vec3V pt4V = RCC_VEC3V(m_corners[3]);
	const ScalarV heightV = LoadScalar32IntoScalarV(fZPlane);

	// Add up and divide by four to compute the average.
	const Vec3V sumV = Add(Add(pt1V, pt2V), Add(pt3V, pt4V));
	Vec3V centerV = Scale(sumV, ScalarV(V_QUARTER));

	// Set the Z position.
	centerV.SetZ(heightV);

	// Store the result.
	m_centre = VEC3V_TO_VECTOR3(centerV);
}


void CEntityBoundAI::ComputeCorners(const CEntity & entity, const float& fZPlane, const float& fPedRadius, const bool bCalculateTopAndBottomHeight, Mat34V_ConstRef matrix, const s32 iNumSubBoundsToIgnore, const s32 * iSubBoundIndicesToIgnore, const s32 iNumSubBoundsToUseExclusively, const s32 * iSubBoundIndicesToUseExclusively, const float fPedHeight)
{
	Assert(!(iNumSubBoundsToIgnore&&iNumSubBoundsToUseExclusively));

	Vec3V bbminV, bbmaxV;

	Assert(entity.GetIsPhysical());
	CPhysical& physical=(CPhysical&)entity;

	// For frag objects we need to use the bounding box of the frag archetype itself.  This is important
	// because the frag object's GetModelIndex() is the same as the origination object, and therefore
	// of no use in trying to get the dimensions of the entity.
	if(entity.GetType() == ENTITY_TYPE_OBJECT && ((CObject*)&entity)->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
	{
		const phArchetypeDamp * pPhysArch = entity.GetPhysArch();
		phBound * pBound = pPhysArch->GetBound();

		bbminV = pBound->GetBoundingBoxMin();
		bbmaxV = pBound->GetBoundingBoxMax();
	}
	// for entities other than short objects use the graphic bounding-box
	// for short objects use the bounding box of the root bound.
	else if(physical.GetCurrentPhysicsInst()==NULL)	//entity.GetType() != ENTITY_TYPE_OBJECT || physical.GetCurrentPhysicsInst()==NULL) 
	{
	 	// Get min & max of entity
		bbminV = VECTOR3_TO_VEC3V(physical.GetBoundingBoxMin());
    	bbmaxV = VECTOR3_TO_VEC3V(physical.GetBoundingBoxMax());
   	}
	else
	{
		// We used to have these static arrays:
		//	static Vector3 min[ms_iMaxNumBoxes];
		//	static Vector3 max[ms_iMaxNumBoxes];
		//	static float   fMinZ[ms_iMaxNumBoxes];
		//	static float   fMaxZ[ms_iMaxNumBoxes];
		// but now they are allocated on the stack (friendlier for multithreading), and we pack in fMinZ/fMaxZ
		// as the W component in the vector array, for less memory access and better alignment for vector processing.
		Vec4V minBoxAndZArray[ms_iMaxNumBoxes];
		Vec4V maxBoxAndZArray[ms_iMaxNumBoxes];

		int numBoxes = 0;
		ComputeBoxes(entity, physical.GetCurrentPhysicsInst()->GetArchetype()->GetBound(), minBoxAndZArray, maxBoxAndZArray, numBoxes, matrix, iNumSubBoundsToIgnore, iSubBoundIndicesToIgnore, iNumSubBoundsToUseExclusively, iSubBoundIndicesToUseExclusively);

		// Before optimization, we did this:
		//	float ped_base = fHeight - 1.0f;
		//	float ped_top = fHeight + 1.0f;
		// This is equivalent with scalar vectors:
		const ScalarV heightV = LoadScalar32IntoScalarV(fZPlane);
		const ScalarV oneV(fPedHeight);
		const ScalarV pedBaseV = Subtract(heightV, oneV);
		const ScalarV pedTopV = Add(heightV, oneV);

		// Subtract the Z coordinate from the height thresholds, so we don't
		// need to do that within the loop.
		const ScalarV matrixZV = matrix.GetCol3().GetZ();
		const ScalarV localPedBaseV = Subtract(pedBaseV, matrixZV);
		const ScalarV localPedTopV = Subtract(pedTopV, matrixZV);

		// Start off with an invalid bounding box, and start looping over the boxes.
		bbminV = Vec3V(V_FLT_MAX);
		bbmaxV = Vec3V(V_NEG_FLT_MAX);
		BoolV hasValidBoxV(V_FALSE);
	   	for(int i = 0; i < numBoxes; i++) 
		{
			// Get the bound information for box i.
			const Vec4V minBoxAndZV = minBoxAndZArray[i];
			const Vec4V maxBoxAndZV = maxBoxAndZArray[i];

			// We used to check for the Z positions like this:
			//	float fWorldMinZ = mat.d.z + fMinZ[i];
			//	float fWorldMaxZ = mat.d.z + fMaxZ[i];
			//	if(fWorldMaxZ < ped_base)
			//		continue;
			//	if(fWorldMinZ > ped_top)
			//		continue;
			// Now, we use vectors and avoid branching. inRangeV will be true
			// if we are within the range.
			const ScalarV minWorldZV = minBoxAndZV.GetW();
			const ScalarV maxWorldZV = maxBoxAndZV.GetW();
			const BoolV inRangeBaseV = IsGreaterThanOrEqual(maxWorldZV, localPedBaseV);
			const BoolV inRangeTopV = IsLessThanOrEqual(minWorldZV, localPedTopV);
			const BoolV inRangeV = And(inRangeBaseV, inRangeTopV);

			// Next, we mask based on whether we are in range or not. If we are in
			// range, then we will use the bounds of this box. If not, we will keep
			// the same bounds as we had before.
			const Vec3V minBoxV = minBoxAndZV.GetXYZ();
			const Vec3V maxBoxV = maxBoxAndZV.GetXYZ();
			const Vec3V minBoxIfInRangeV = SelectFT(inRangeV, bbminV, minBoxV);
			const Vec3V maxBoxIfInRangeV = SelectFT(inRangeV, bbmaxV, maxBoxV);

			// Update the min and max, possibly using this box.
			bbminV = Min(bbminV, minBoxIfInRangeV);
			bbmaxV = Max(bbmaxV, maxBoxIfInRangeV);

			// OR in to keep track of whether any boxes have been valid.
			hasValidBoxV = Or(hasValidBoxV, inRangeV);
		}

		// If none of the boxes are valid, we will mask in -0.1/0.1:
		Vec3V bbMaxFallbackV(V_FLT_SMALL_1);			// 0.1f
		Vec3V bbMinFallbackV = Negate(bbMaxFallbackV);	// -0.1f
		bbminV = SelectFT(hasValidBoxV, bbMinFallbackV, bbminV);
		bbmaxV = SelectFT(hasValidBoxV, bbMaxFallbackV, bbmaxV);

		// This is how we used to do it:
		//	if(!bHasValidBox)
		//	{
		//		bbmin.x = bbmin.y = bbmin.z = -0.1f;
		//		bbmax.x = bbmax.y = bbmax.z = 0.1f;
		//	}
	}

	// Expand bbox by radius of ped
	// Used to be
	//	bbmin.x -= fPedRadius;
	//	bbmin.y -= fPedRadius;
	//	bbmin.z -= fPedRadius;
	//	bbmax.x += fPedRadius;
	//	bbmax.y += fPedRadius;
	//	bbmax.z += fPedRadius;
	// With vectors:
	const Vec3V pedRadiusV(LoadScalar32IntoScalarV(fPedRadius));
	bbminV = Subtract(bbminV, pedRadiusV);
	bbmaxV = Add(bbmaxV, pedRadiusV);

	ProjectBoundingBoxOntoZPlane(RCC_VECTOR3(bbminV), RCC_VECTOR3(bbmaxV), RCC_MATRIX34(matrix), fZPlane, bCalculateTopAndBottomHeight);
}

void CEntityBoundAI::ProjectBoundingBoxOntoZPlane(const Vector3 & vBoxMinIn, const Vector3 & vBoxMaxIn, const Matrix34 & orientationMat, const float fZPlane, const bool bCalculateTopAndBottomHeight)
{
	// TODO: Would be really nice to vectorize the math here better, didn't have time to do it. It's only
	// called once per entity though, not once per entity and path segment, or once per entity sub-bound.

	//-----------------------------------------------------------
	// Project onto z=fHeight plane
	// NB : Lifted from CEntity::CalculateBBProjection()

	Vector3 vBoxMin = vBoxMinIn;
	Vector3 vBoxMax = vBoxMaxIn;

	float	LengthXProj = (orientationMat.a.x * orientationMat.a.x + orientationMat.a.y * orientationMat.a.y);
	float	LengthYProj = (orientationMat.b.x * orientationMat.b.x + orientationMat.b.y * orientationMat.b.y);
	float	LengthZProj = (orientationMat.c.x * orientationMat.c.x + orientationMat.c.y * orientationMat.c.y);
			
	float	SizeBBX = (vBoxMax.x - vBoxMin.x) * 0.5f;
	float	SizeBBY = (vBoxMax.y - vBoxMin.y) * 0.5f;
	float	SizeBBZ = (vBoxMax.z - vBoxMin.z) * 0.5f;
	
	// Get the centre of the bbox in local space.
	Vector3 vLocalCentre = (vBoxMax + vBoxMin) * 0.5f;
	// Adjust min/max so that centre is at the origin (in case the model is off-centre)
	vBoxMin -= vLocalCentre;
	vBoxMax -= vLocalCentre;
	// Transform to entity's origin
	Vector3 vWorldCentre = orientationMat * vLocalCentre;
	// Find the offset from the matrix's origin to the local-origin in worldspace
	// This offset will be non-zero, if the entity's origin was off-centre
	Vector3 vCentreOffset = vWorldCentre - orientationMat.d;
	// Rotate the offset.  We'll add this onto the final corner points at the end.
	orientationMat.Transform3x3(vLocalCentre);


	// Bounding-box is now at entity's origin, and is symmetrical around this.  The code below relies upon the
	// symmetry to calculate the 2d projection to 4 points.  After this is done, we can move the projected
	// points back by the rotated offset which the bbmin/bbmax was from (0,0,0)
	float	TotalSizeX = LengthXProj * SizeBBX * 2.0f;
	float	TotalSizeY = LengthYProj * SizeBBY * 2.0f;
	float	TotalSizeZ = LengthZProj * SizeBBZ * 2.0f;

	float		OffsetLength1, OffsetLength2, OffsetLength;
	float		OffsetWidth1, OffsetWidth2, OffsetWidth;
	Vector3		MainVec, MainVecPerp;
	Vector3		HigherPoint, LowerPoint;

	if (TotalSizeX > TotalSizeY && TotalSizeX > TotalSizeZ)
	{
		// x-axis is the main one
		MainVec = Vector3(orientationMat.a.x, orientationMat.a.y, 0.0f);

		HigherPoint = vWorldCentre + SizeBBX * MainVec;
		LowerPoint = vWorldCentre - SizeBBX * MainVec;
		MainVec.Normalize();

		OffsetLength1 = SizeBBY * (MainVec.y * orientationMat.b.y   +   MainVec.x * orientationMat.b.x);
		OffsetWidth1 =  SizeBBY * (MainVec.x * orientationMat.b.y   -   MainVec.y * orientationMat.b.x);
		OffsetLength2 = SizeBBZ * (MainVec.y * orientationMat.c.y   +   MainVec.x * orientationMat.c.x);
		OffsetWidth2 =  SizeBBZ * (MainVec.x * orientationMat.c.y   -   MainVec.y * orientationMat.c.x);
	}
	else if (TotalSizeY > TotalSizeZ)
	{
		// y-axis is the main one
		MainVec = Vector3(orientationMat.b.x, orientationMat.b.y, 0.0f);

		HigherPoint = vWorldCentre + SizeBBY * MainVec;
		LowerPoint = vWorldCentre - SizeBBY * MainVec;
		MainVec.Normalize();

		OffsetLength1 = SizeBBX * (MainVec.y * orientationMat.a.y   +   MainVec.x * orientationMat.a.x);
		OffsetWidth1 =  SizeBBX * (MainVec.x * orientationMat.a.y   -   MainVec.y * orientationMat.a.x);
		OffsetLength2 = SizeBBZ * (MainVec.y * orientationMat.c.y   +   MainVec.x * orientationMat.c.x);
		OffsetWidth2 =  SizeBBZ * (MainVec.x * orientationMat.c.y   -   MainVec.y * orientationMat.c.x);
	}
	else
	{
		// z-axis is the main one
		MainVec = Vector3(orientationMat.c.x, orientationMat.c.y, 0.0f);

		HigherPoint = vWorldCentre + SizeBBZ * MainVec;
		LowerPoint = vWorldCentre - SizeBBZ * MainVec;
		MainVec.Normalize();

		OffsetLength1 = SizeBBX * (MainVec.y * orientationMat.a.y   +   MainVec.x * orientationMat.a.x);
		OffsetWidth1 =  SizeBBX * (MainVec.x * orientationMat.a.y   -   MainVec.y * orientationMat.a.x);
		OffsetLength2 = SizeBBY * (MainVec.y * orientationMat.b.y   +   MainVec.x * orientationMat.b.x);
		OffsetWidth2 =  SizeBBY * (MainVec.x * orientationMat.b.y   -   MainVec.y * orientationMat.b.x);
	}

	MainVecPerp = Vector3(MainVec.y, -MainVec.x, 0.0f);

	OffsetLength = ABS(OffsetLength1) + ABS(OffsetLength2);
	OffsetWidth = ABS(OffsetWidth1) + ABS(OffsetWidth2);

	// JB : correct ordering now - front, left, rear & right sides of the entity
	m_corners[0] = HigherPoint + OffsetLength * MainVec - OffsetWidth * MainVecPerp;
	m_corners[1] = LowerPoint - OffsetLength * MainVec - OffsetWidth * MainVecPerp;
	m_corners[2] = LowerPoint - OffsetLength * MainVec + OffsetWidth * MainVecPerp;
	m_corners[3] = HigherPoint + OffsetLength * MainVec + OffsetWidth * MainVecPerp;				

	m_corners[0].z = m_corners[1].z = m_corners[2].z = m_corners[3].z = fZPlane;

	// Find the top-most & bottom-most Z values
	if(bCalculateTopAndBottomHeight)
	{
		const spdAABB worldBox = TransformAABB(RCC_MAT34V(orientationMat), spdAABB(RCC_VEC3V(vBoxMin), RCC_VEC3V(vBoxMax)));

		StoreScalar32FromScalarV(m_fTopZ,		(worldBox.GetMax() + RCC_VEC3V(vCentreOffset)).GetZ());
		StoreScalar32FromScalarV(m_fBottomZ,	(worldBox.GetMin() + RCC_VEC3V(vCentreOffset)).GetZ());
	}
}


//Compute all the boxes stored in a phBound.  If the bound is a composite then query the subbound 
//until we find a box with no children. 
void
CEntityBoundAI::ComputeBoxes(const CEntity& entity, const phBound* pBound, Vec4V* RESTRICT pMinPosAndZ, Vec4V* RESTRICT pMaxPosAndZ, int& numBoxes, Mat34V_ConstRef mWorld, const s32 iNumSubBoundsToIgnore, const s32 * iSubBoundIndicesToIgnore, const s32 iNumSubBoundsToUseExclusively, const s32 * iSubBoundIndicesToUseExclusively) const
{
#if __PEDGEOM_DEBUG_BOXES
	static dev_bool bDebugBoxes = false;
	char txt[64];
#endif

	if(pBound->GetType()==phBound::COMPOSITE)
	{
		phBoundComposite* pBoundComposite=(phBoundComposite*)pBound;
		const int N=pBoundComposite->GetNumBounds();
		int i,n;
		for(i=0;i<N;i++)
		{
			// Possibly skip this sub-bound.
			// This is the only obvious way to ignore components, (eg. doors) as there are
			// no ways of identifying what a bound *is* from the bounds themselves.
			if(iNumSubBoundsToIgnore)
			{
				for(n=0; n<iNumSubBoundsToIgnore; n++)
					if(i == iSubBoundIndicesToIgnore[n])
						break;
				if(n!=iNumSubBoundsToIgnore)
					continue;
			}
			else if(iNumSubBoundsToUseExclusively)
			{
				for(n=0; n<iNumSubBoundsToUseExclusively; n++)
					if(i == iSubBoundIndicesToUseExclusively[n])
						break;
				if(n==iNumSubBoundsToUseExclusively)
					continue;
			}

			const phBound* pSubBound = pBoundComposite->GetBound(i);

#if __PEDGEOM_DEBUG_BOXES
			if(bDebugBoxes)
			{
				if(pSubBound && CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
				{
					formatf(txt, 64, "(%i)", i);

					Vector3 vMid = VEC3V_TO_VECTOR3((pSubBound->GetBoundingBoxMin() + pSubBound->GetBoundingBoxMax()) * ScalarV(V_HALF));
					Matrix34 c = MAT34V_TO_MATRIX34(pBoundComposite->GetCurrentMatrix(i));
					Matrix34 mCombined;
					mCombined.Dot( c, MAT34V_TO_MATRIX34(mWorld) );
					mCombined.Transform(vMid);

					grcDebugDraw::BoxOriented( pSubBound->GetBoundingBoxMin(), pSubBound->GetBoundingBoxMax(), MATRIX34_TO_MAT34V(mCombined), Color_cyan, false);
					grcDebugDraw::Text(vMid, Color_cyan, 0, 0, txt);
				}
			}
#endif
			if(pSubBound)
			{
				Mat34V_ConstRef c = pBoundComposite->GetCurrentMatrix(i);
				ComputeBox(entity, pSubBound, pMinPosAndZ, pMaxPosAndZ, numBoxes, mWorld, c);
			}
		}
	}
	else
	{
#if (__PS3 || __XENON)
		Vector3 min = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMin());
		Vector3 max = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMax());
		//pMin[numBoxes] = min;
		//pMax[numBoxes] = max;
		// compute Z extents with world rotation
		Vector3 size = (max - min) * VEC3_HALF;
		Vector3 centre = (max + min) * VEC3_HALF;
		Vector3 rotz = RCC_MATRIX34(mWorld).c;
		rotz.Abs();
 		Vector3 sizez = rotz.DotV(size);
		Vector3 centrez = __vmaddfp(__vspltw(RCC_MATRIX34(mWorld).a, 2), __vspltw(centre, 0),
						  __vmaddfp(__vspltw(RCC_MATRIX34(mWorld).b, 2), __vspltw(centre, 1),
						   __vmulfp(__vspltw(RCC_MATRIX34(mWorld).c, 2), __vspltw(centre, 2))));
 		Vector3 minz = centrez - sizez;
 		Vector3 maxz = centrez + sizez;

		//__stvewx(minz.xyzw, &fMinZ[numBoxes], 0);
		//__stvewx(maxz.xyzw, &fMaxZ[numBoxes], 0);

		// When we had fMinZ/fMaxZ, we used to store them as above. Now, we store
		// everything in two Vec4V's, which we prepare here:
		Vec4V minPosAndZV(VECTOR3_TO_VEC3V(min).GetIntrin128());
		Vec4V maxPosAndZV(VECTOR3_TO_VEC3V(max).GetIntrin128());
		minPosAndZV.SetW(VECTOR3_TO_VEC3V(minz).GetX());	// The component doesn't matter here, it's splatted.
		maxPosAndZV.SetW(VECTOR3_TO_VEC3V(maxz).GetX());	//

		pMinPosAndZ[numBoxes] = minPosAndZV;
		pMaxPosAndZ[numBoxes] = maxPosAndZV;

		++numBoxes;
#else
		ComputeBox(entity, pBound, pMinPosAndZ, pMaxPosAndZ, numBoxes, mWorld, RCC_MAT34V(M34_IDENTITY));
#endif
	}
}


void CEntityBoundAI::ComputeBox(const CEntity& ASSERT_ONLY(entity), const phBound* RESTRICT pBound, Vec4V* RESTRICT pMinPosAndZ, Vec4V* RESTRICT pMaxPosAndZ, int& numBoxes, Mat34V_ConstRef worldMtrx, Mat34V_ConstRef boundMtrx) const
{
	Assertf(pBound->GetType()!=phBound::COMPOSITE, "Composite bound within a composite bound - not allowed.  Model is \"%s\"", entity.GetModelName());
	if(Verifyf(numBoxes<int(ms_iMaxNumBoxes), "CEntityBoundAI::ComputeBox: Too many boxes, can model \"%s\" be made from less sub-bounds?", entity.GetModelName()) )
	{
		const Vec3V localBoxMinV = pBound->GetBoundingBoxMin();
		const Vec3V localBoxMaxV = pBound->GetBoundingBoxMax();

		const Vec3V worldAV = worldMtrx.GetCol0();
		const Vec3V worldBV = worldMtrx.GetCol1();
		const Vec3V worldCV = worldMtrx.GetCol2();

		const Vec3V boundMtrxAV = boundMtrx.GetCol0();
		const Vec3V boundMtrxBV = boundMtrx.GetCol1();
		const Vec3V boundMtrxCV = boundMtrx.GetCol2();
		const Vec3V boundMtrxDV = boundMtrx.GetCol3();

		// The computation of minV and maxV below is equivalent to this unoptimized code:
		//	const Matrix34& c = mBound;
		//	pMin[numBoxes].x=Min(Min(c.a.x*boxMin.x,c.a.x*boxMax.x), Min(Min(c.b.x*boxMin.y,c.b.x*boxMax.y), Min(c.c.x*boxMin.z,c.c.x*boxMax.z)))+c.d.x;
		//	pMin[numBoxes].y=Min(Min(c.a.y*boxMin.x,c.a.y*boxMax.x), Min(Min(c.b.y*boxMin.y,c.b.y*boxMax.y), Min(c.c.y*boxMin.z,c.c.y*boxMax.z)))+c.d.y;
		//	pMin[numBoxes].z=Min(Min(c.a.z*boxMin.x,c.a.z*boxMax.x), Min(Min(c.b.z*boxMin.y,c.b.z*boxMax.y), Min(c.c.z*boxMin.z,c.c.z*boxMax.z)))+c.d.z;
		//	pMax[numBoxes].x=Max(Max(c.a.x*boxMin.x,c.a.x*boxMax.x), Max(Max(c.b.x*boxMin.y,c.b.x*boxMax.y), Max(c.c.x*boxMin.z,c.c.x*boxMax.z)))+c.d.x;
		//	pMax[numBoxes].y=Max(Max(c.a.y*boxMin.x,c.a.y*boxMax.x), Max(Max(c.b.y*boxMin.y,c.b.y*boxMax.y), Max(c.c.y*boxMin.z,c.c.y*boxMax.z)))+c.d.y;
		//	pMax[numBoxes].z=Max(Max(c.a.z*boxMin.x,c.a.z*boxMax.x), Max(Max(c.b.z*boxMin.y,c.b.z*boxMax.y) ,Max(c.c.z*boxMin.z,c.c.z*boxMax.z)))+c.d.z;

		const Vec3V minXV = Scale(boundMtrxAV, localBoxMinV.GetX());
		const Vec3V minYV = Scale(boundMtrxBV, localBoxMinV.GetY());
		const Vec3V minZV = Scale(boundMtrxCV, localBoxMinV.GetZ());
		const Vec3V maxXV = Scale(boundMtrxAV, localBoxMaxV.GetX());
		const Vec3V maxYV = Scale(boundMtrxBV, localBoxMaxV.GetY());
		const Vec3V maxZV = Scale(boundMtrxCV, localBoxMaxV.GetZ());
		const Vec3V minV = Add(Min(Min(minXV, maxXV), Min(Min(minYV, maxYV), Min(minZV, maxZV))), boundMtrxDV);
		const Vec3V maxV = Add(Max(Max(minXV, maxXV), Max(Max(minYV, maxYV), Max(minZV, maxZV))), boundMtrxDV);

		// The computation of fMinZ and fMaxZ used to be done like this:
		//	Matrix34 cm = mWorld;
		//	cm.d.Zero();
		//	cm.Dot(c);
		//	fMinZ[numBoxes] = Min(pMin[numBoxes].z,Min(Min(cm.a.z*boxMin.x,cm.a.z*boxMax.x), Min(Min(cm.b.z*boxMin.y,cm.b.z*boxMax.y), Min(cm.c.z*boxMin.z,cm.c.z*boxMax.z))))+cm.d.z;
		//	fMaxZ[numBoxes] = Max(pMax[numBoxes].z,Max(Max(cm.a.z*boxMin.x,cm.a.z*boxMax.x), Max(Max(cm.b.z*boxMin.y,cm.b.z*boxMax.y), Max(cm.c.z*boxMin.z,cm.c.z*boxMax.z))))+cm.d.z;

		// Now, we first create a vector (sumXYZV) which is equivalent to (cm.a.z, cm.b.z, cm.c.z) above.
		// We don't have to do the full matrix transform.
		const Vec4V temp0V = MergeXY(Vec4V(worldAV), Vec4V(worldCV));
		const Vec4V temp1V = MergeXY(Vec4V(worldBV), Vec4V(worldBV));
		const Vec4V temp2V = MergeZW(Vec4V(worldAV), Vec4V(worldCV));
		const Vec4V temp3V = MergeZW(Vec4V(worldBV), Vec4V(worldBV));
		const Vec3V worldABCXV = MergeXY(temp0V, temp1V).GetXYZ();
		const Vec3V worldABCYV = MergeZW(temp0V, temp1V).GetXYZ();
		const Vec3V worldABCZV = MergeXY(temp2V, temp3V).GetXYZ();
		const Vec3V sumXV = Scale(worldABCXV, boundMtrxAV.GetZ());
		const Vec3V sumXYV = AddScaled(sumXV, worldABCYV, boundMtrxBV.GetZ());
		const Vec3V sumXYZV = AddScaled(sumXYV, worldABCZV, boundMtrxCV.GetZ());

		// Compute the terms cm.a.z*boxMin.x, cm.a.z*boxMax.x, cm.b.z*boxMin.x, etc, using one vector for
		// the minimum and one for the maximum.
		const Vec3V boxMinTimesWorldZV = Scale(sumXYZV, localBoxMinV);
		const Vec3V boxMaxTimesWorldZV = Scale(sumXYZV, localBoxMaxV);

		// Compute the minimum of the two vectors and the maximum of the two vectors.
		// Also, stuff the Z value from minV/maxV in the W component. This part doesn't entirely make sense to me,
		// but comes from how it used to be done here:
		//	fMinZ[numBoxes] = Min(pMin[numBoxes].z, ...
		//	fMaxZ[numBoxes] = Max(pMax[numBoxes].z, ...
		Vec4V boxMinMinWorldZV(Min(boxMinTimesWorldZV, boxMaxTimesWorldZV));
		Vec4V boxMaxMaxWorldZV(Max(boxMinTimesWorldZV, boxMaxTimesWorldZV));
		boxMinMinWorldZV.SetW(minV.GetZ());
		boxMaxMaxWorldZV.SetW(maxV.GetZ());

		// Get the minimum component of each vector, and add the Z value of the D vector of the bound matrix
		// (equivalent to +cm.d.z in the original code, cm.d.z == c.d.z since we cleared out the D vector when
		// doing the matrix transform).
		const ScalarV boxMinWorldWithOffsZV = Add(MinElement(boxMinMinWorldZV), boundMtrxDV.GetZ());
		const ScalarV boxMaxWorldWithOffsZV = Add(MaxElement(boxMaxMaxWorldZV), boundMtrxDV.GetZ());

		// Store the result.
		const int index = numBoxes;
		// Used to do this, but now we squeeze fMinZ and fMaxZ together with the min/max box coordinates:
		//	pMin[index] = VEC3V_TO_VECTOR3(minV);
		//	pMax[index] = VEC3V_TO_VECTOR3(maxV);
		//	StoreScalar32FromScalarV(fMinZ[index], boxMinWorldWithOffsZV);
		//	StoreScalar32FromScalarV(fMaxZ[index], boxMaxWorldWithOffsZV);
		Vec4V minAndZV(minV.GetIntrin128());
		Vec4V maxAndZV(maxV.GetIntrin128());
		minAndZV.SetW(boxMinWorldWithOffsZV);
		maxAndZV.SetW(boxMaxWorldWithOffsZV);
		pMinPosAndZ[index] = minAndZV;
		pMaxPosAndZ[index] = maxAndZV;
		numBoxes = index + 1;
	}
}


void CEntityBoundAI::ComputeSphere()
{
	// TODO: Would be nice to vectorize the math here better:

	float fRadius=0;
    int i;
    for(i=0;i<4;i++)
    {
      const float fRadiusSquared=(m_corners[i]-m_centre).Mag2();
      if(fRadiusSquared>fRadius)
      {
          fRadius=fRadiusSquared;
      }
    }
    fRadius=rage::Sqrtf(fRadius);
    fRadius*=1.1f;
 
    //Set the sphere data.
    m_sphere.Set(RCC_VEC3V(m_centre), ScalarV(fRadius));
}

//Compute an array of 4 planes around the bound such that a point P lies on the ith plane if
//Dot(normals[i],P) +ds[i] = 0.
void CEntityBoundAI::ComputeBoundingBoxPlanes(Vector3* normals, float* ds) const
{
	//Compute the four bounding planes of the box 
	//(the ith plane contains (i-1)th and ith corners).   
	Vector3 v0=m_corners[3];
	int i;
	for(i=0;i<4;i++)
	{
		const Vector3& v1=m_corners[i];
		Vector3 w=v1-v0;
		w.Normalize();
		normals[i]=Vector3(w.y,-w.x,0);
		ds[i]=-DotProduct(v0,normals[i]);
		v0=v1;    
	}
}
void CEntityBoundAI::ComputeBoundingBoxPlanes(Vector4*planes) const
{
	//Compute the four bounding planes of the box 
	//(the ith plane contains (i-1)th and ith corners).   
	Vector3 v0=m_corners[3];
	int i;
	for(i=0;i<4;i++)
	{
		const Vector3& v1=m_corners[i];
		Vector3 w=v1-v0;
		w.Normalize();
		planes[i].x = w.y;
		planes[i].y = -w.x;
		planes[i].z = 0.0f;
		planes[i].w = - planes[i].Dot3(v0);
		v0=v1;    
	}
}
////////////////////////////////////////////////
//Divide the box into four spaces with four planes.
////////////////////////////////////////////////
//
//
//       ----------------------------
//       |¬        left           ¬  |
//       |  ¬                  ¬     |
//       |    ¬             ¬        |
//       |      ¬       ¬            |
//       | front   ¬         rear    |
//       |      ¬       ¬            |
//       |     ¬            ¬        |
//       |   ¬    right        ¬     |
//       | ¬                      ¬  |
//       |¬                          |
//       |----------------------------
	

void CEntityBoundAI::ComputeSegmentPlanes(Vector3* segmentPlaneNormals, float* segmentPlaneDs) const
{
	//Compute the planes bounding each region.
	int i;
	for(i=0;i<4;i++)
	{
	    Vector3 vDir;
	    vDir=m_corners[i]-m_centre;
	    segmentPlaneNormals[i]=Vector3(-vDir.y,vDir.x,0);
		segmentPlaneDs[i]=-DotProduct(segmentPlaneNormals[i],m_corners[i]);
	}
}
void CEntityBoundAI::ComputeSegmentPlanes(Vector2* segmentPlaneNormals, float* segmentPlaneDs) const
{
	//Compute the planes bounding each region.
	int i;
	for(i=0;i<4;i++)
	{
		Vector3 vDir;
		vDir=m_corners[i]-m_centre;
		segmentPlaneNormals[i] = Vector2(-vDir.y,vDir.x);
		segmentPlaneDs[i] = - ((segmentPlaneNormals[i].x*m_corners[i].x) + (segmentPlaneNormals[i].y*m_corners[i].y));
	}
}
//Compute the segment that contains a point.  Note that the point doesn't have to 
//be inside the bound. 
int CEntityBoundAI::ComputePlaneFlags(const Vector3& vPos) const
{
	Vector3 vPlaneNormals[4];
	float fPlaneDists[4];
	GetPlanes(vPlaneNormals, fPlaneDists);

	int iFlags = 0;
	int i;
	for(i=0;i<4;i++)
	{
		const float fDist = DotProduct(vPos, vPlaneNormals[i]) + fPlaneDists[i];
		if(fDist > 0.0f)
		{
			iFlags |= ms_iSideFlags[i];
		}
	}

	return iFlags;
}

//Compute the segment that contains a point.  Note that the point doesn't have to 
//be inside the bound. 
int CEntityBoundAI::ComputeHitSideByPosition(const Vector3& vPos) const
{
	Vector3 segmentPlaneNormals[4];
	float segmentPlaneDs[4];
	ComputeSegmentPlanes(segmentPlaneNormals,segmentPlaneDs);

	int iDir=0;
	int i;
	for(i=0;i<4;i++)
	{
	    const int i0=(i+4-1)%4;
	    const int i1=(i+4+0)%4;
	    
	    const float f0=DotProduct(vPos,segmentPlaneNormals[i0])+segmentPlaneDs[i0];
        const float f1=DotProduct(vPos,segmentPlaneNormals[i1])+segmentPlaneDs[i1];
        
        if((f0>=0)&&(f1<0))		    
	    {
	        iDir=i1;
	        break;
	    }
	}
	Assert(iDir>=0 && iDir<=3);
	
	return iDir;
}

int CEntityBoundAI::ComputeHitSideByVelocity(const Vector3& vVel) const
{
	//Vector3 vInvertedVel;
    //vInvertedVel.FromMultiply(vVel,-1.0f);
	Vector3 vInvertedVel(vVel);
	vInvertedVel*=-1;
    vInvertedVel.Normalize();

	Vector3 normals[4];
	float ds[4];
	ComputeBoundingBoxPlanes(normals,ds);

	//Find the plane normal that is closest to vInvertedVel
	float fBestDot=-1.0;
    int iDir = 0;
    int i;
    for(i=0;i<4;i++)
    {
        const float fDot=DotProduct(vInvertedVel,normals[i]);
        if(fDot>=fBestDot)
        {
            iDir=i;
            fBestDot=fDot;
        }
    }   
    return iDir;
}

//Find the point on the surface of the bounding rectangle that is closest to a point.
//The closest point is either a corner or a point on one of the four lines connecting the 
//corners.
bool CEntityBoundAI::ComputeClosestSurfacePoint(const Vector3& vPos, Vector3& vClosestPoint) const
{
    bool bFoundPoint=false;
    float fMin2=FLT_MAX;//FLT_MAX;
    
	//First try to find a point on an edge that is closest.
    int i;
    for(i=0;i<4;i++)
    {
    	const int i0=(i+0)%4;
    	const int i1=(i+1)%4;
    	const Vector3& v0=m_corners[i0];
    	const Vector3& v1=m_corners[i1];
    	Vector3 w;
    	w=v1-v0;
    	const float l=w.Mag();
    	const float lInverse=1.0f/l;
    	w*=lInverse;
    	
    	//Compute t satisfying (v0+wt-p).w = 0
    	Vector3 pv;
    	pv=vPos-v0;
    	const float t=DotProduct(pv,w);
    	
    	//Make  sure that t lies along the edge v0-v1
    	if(t>=0 && t<=l)
    	{
	    	//vt=v0+w*t
	    	Vector3 vt(w);
	    	vt*=t;
	    	vt+=v0;
	    	
	    	//Compute distance from ped to vt.
	    	Vector3 vDiff;
	    	vDiff=vPos-vt;
	    	const float f2=vDiff.Mag2();
	    	
	    	//Check that it is the closest yet.
	    	if(f2<fMin2)
	    	{
	    		fMin2=f2;
	    		vClosestPoint=vt;
	    		bFoundPoint=true;
	    	}
		}
    }
    
	//Now try to find a corner that is closest.
    if(!bFoundPoint)
    {
     	for(i=0;i<4;i++)
	    {
	    	Vector3 vDiff;
	    	vDiff=m_corners[i]-vPos;
	    	const float f2=vDiff.Mag2();
	    	if(f2<fMin2)
	    	{
	    		fMin2=f2;
	    		vClosestPoint=m_corners[i];
	    		bFoundPoint=true;
	    	}
    	}
    }
    
    return bFoundPoint;	
}

bool CEntityBoundAI::LiesInside(const Vector3& vPos, const float fEps) const
{
	//First do a sphere rejection test.
	if(!m_sphere.ContainsPoint(RCC_VEC3V(vPos)))
	{
		return false;
	}

	//vPos lies inside the bounding sphere.  Test if it lies inside the four bounding planes.
	Vector3 normals[4];
	float ds[4];
	ComputeBoundingBoxPlanes(normals,ds);

	return LiesInside(vPos,normals,ds,4,fEps);
}

bool CEntityBoundAI::LiesInside(const Vector3& vPos, Vector3* normals, float* ds, const int numPlanes, const float fPlaneEps)
{
	//If the point lies on the outside of any plane it must lie outside the bound.  
	//Any point on the inside lies on the inside of all planes.
	int i;
	for(i=0;i<numPlanes;i++)
	{
		const float f=DotProduct(vPos,normals[i])+ds[i];
		if(f > fPlaneEps)
		{
			return false;
		}
	}
	return true;
}

//Test if a sphere lies inside a bound.
bool CEntityBoundAI::LiesInside(const spdSphere& sphere) const
{
	//First do a sphere rejection test.
	if(!m_sphere.IntersectsSphereFlat(sphere))
	{
		return false;
	}

	//Now test if the sphere lies inside the bounding planes.
	Vector3 normals[4];
	float ds[4];
	ComputeBoundingBoxPlanes(normals,ds);

	return LiesInside(sphere,normals,ds,4);
}

bool CEntityBoundAI::LiesInside(const spdSphere& sphere, Vector3* normals, float* ds, const int numPlanes)
{
	//The sphere lies inside the bound if the distance to any plane is less than the sphere radius.
	int i;
	for(i=0;i<numPlanes;i++)
	{
		// [SPHERE-OPTIMISE]
		const float f=DotProduct(normals[i],VEC3V_TO_VECTOR3(sphere.GetCenter()))+ds[i];
		if(f>sphere.GetRadiusf())
		{
			return false;
		}
	}
	return true;
}

void
CEntityBoundAI::MoveOutside(const Vector3 & vIn, Vector3 & vOut, int iHitSide) const
{
	Vector3 normals[4];
	float ds[4];

	Assert(iHitSide>=-1 && iHitSide<4);
	// If no hitside is specified, then use whichever quadrant the input point is in
	if(iHitSide==-1)
	{
		iHitSide = ComputeHitSideByPosition(vIn);
	}

	ComputeBoundingBoxPlanes(normals, ds);

	// Bounding-Box planes are set up to point outwards from the entity
	const float fDist = DotProduct(vIn, normals[iHitSide]) + ds[iHitSide];
	static const float fAmt = 0.05f;

	if(fDist > 0.0f)
	{
		vOut = vIn;
	}
	else
	{
		vOut = vIn + normals[iHitSide] * ((-fDist) + fAmt);
	}
}

#if 0	// Disabled, use the new vectorized function unless there is some problem with it.

bool CEntityBoundAI::TestLineOfSight(const Vector3& vStart, const Vector3& vEnd) const
{
	Vector3 normals[4];
	float ds[4];
	ComputeBoundingBoxPlanes(normals,ds);

	return TestLineOfSight(vStart,vEnd,normals,ds,4);
}

#endif	// 0

bool CEntityBoundAI::TestLineOfSight(Vec3V_In startV, Vec3V_In endV) const
{
	// We want to operate on all sides in parallel, so the first thing we do
	// is to put the X coordinates of all corners in one vector, and the Y coordinates
	// of another, similar to transposing a matrix. It would probably be better
	// to store them like that in memory, which would also save some space since the W
	// component now is unused, and the Z coordinate is the same for all the corners.
	// It's still pretty cheap to do, though.
	const Vec3V corner0V = RCC_VEC3V(m_corners[0]);
	const Vec3V corner1V = RCC_VEC3V(m_corners[1]);
	const Vec3V corner2V = RCC_VEC3V(m_corners[2]);
	const Vec3V corner3V = RCC_VEC3V(m_corners[3]);
	const Vec4V temp0AV = MergeXY(Vec4V(corner0V), Vec4V(corner2V));
	const Vec4V temp1AV = MergeXY(Vec4V(corner1V), Vec4V(corner3V));
	const Vec4V cornersXV = MergeXY(temp0AV, temp1AV);
	const Vec4V cornersYV = MergeZW(temp0AV, temp1AV);

	// Compute the direction of the input segment.
	const Vec3V segDirV = Subtract(endV, startV);

	// Rotate the corner coordinate vectors one step. This is needed since we
	// will be looking at them as edges, effectively the previous and the current corner.
	const Vec4V prevCornersXV = cornersXV.Get<Vec::W, Vec::X, Vec::Y, Vec::Z>();
	const Vec4V prevCornersYV = cornersYV.Get<Vec::W, Vec::X, Vec::Y, Vec::Z>();

	// Compute the delta vector for the edge.
	const Vec4V edgeDeltaXV = Subtract(cornersXV, prevCornersXV);
	const Vec4V edgeDeltaYV = Subtract(cornersYV, prevCornersYV);

	// Get 4D vectors for the start position and delta of the segment.
	const Vec4V startXV(startV.GetX());
	const Vec4V startYV(startV.GetY());
	const Vec4V segDirXV(segDirV.GetX());
	const Vec4V segDirYV(segDirV.GetY());

	// Compute deltas from the starting position of the segment, and the first point on each edge.
	const Vec4V startDeltaXV = Subtract(prevCornersXV, startXV);
	const Vec4V startDeltaYV = Subtract(prevCornersYV, startYV);

	// Basically compute a dot product between the normal of the edge, and the delta between the
	// segment start and the edge. This will be a positive value if the starting point is on the
	// inside of the edge.
	const Vec4V orthDotV = SubtractScaled(Scale(edgeDeltaYV, startDeltaXV), edgeDeltaXV, startDeltaYV);

	// Compute the denominator for finding T values, and the absolute value of it, and check its sign.
	const Vec4V zeroV(V_ZERO);
	const Vec4V denominatorV = SubtractScaled(Scale(segDirXV, edgeDeltaYV), segDirYV, edgeDeltaXV);
	const Vec4V absDenominatorV = Abs(denominatorV);
	const VecBoolV denominatorNonNegV = IsGreaterThanOrEqual(denominatorV, zeroV);

	// Check if the denominators are tiny. That would mean that the segment is parallel to an edge,
	// and we can't trust the T value.
	const VecBoolV denominatorValidV = IsGreaterThanOrEqual(absDenominatorV, Vec4V(V_FLT_SMALL_5));

	// We need to see if the intersection is on each edge, rather than outside, i.e. if the T value
	// along each edge is in the 0..1 range. This would technically entail a division by the denominator,
	// but we can avoid that by testing the pre-division value against the 0..denominator range instead,
	// but only if the denominator is positive - if it's negative, we need to flip it around a bit. Fortunately
	// we can do all that quickly.
	const Vec4V hitT1TimesAbsDenominator = SelectFT(denominatorNonNegV, Negate(orthDotV), orthDotV);
	const VecBoolV hitT1InRangeAV = IsGreaterThanOrEqual(hitT1TimesAbsDenominator, zeroV);
	const VecBoolV hitT1InRangeBV = IsLessThanOrEqual(hitT1TimesAbsDenominator, absDenominatorV);
	const Vec4V hitT2TimesDenominatorV = SubtractScaled(Scale(segDirYV, startDeltaXV), segDirXV, startDeltaYV);
	const Vec4V hitT2TimesAbsDenominatorV = SelectFT(denominatorNonNegV, Negate(hitT2TimesDenominatorV), hitT2TimesDenominatorV);
	const VecBoolV hitT2InRangeAV = IsGreaterThanOrEqual(hitT2TimesAbsDenominatorV, zeroV);
	const VecBoolV hitT2InRangeBV = IsLessThanOrEqual(hitT2TimesAbsDenominatorV, absDenominatorV);

	// Combine the result of these tests along each edge. The first T value has to be in range, the second
	// T value has to be in range, and the denominator has to be valid.
	const VecBoolV hitV = And(And(And(hitT1InRangeAV, hitT1InRangeBV), And(hitT2InRangeAV, hitT2InRangeBV)), denominatorValidV);

	// We need to OR across the components to determine if any of the edges hit.
	const VecBoolV hitEdgesShift1V = hitV.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>();		// Z  W  X  Y
	const VecBoolV hitOr1V = Or(hitV, hitEdgesShift1V);									// XZ YW ZX WY
	const VecBoolV hitEdgesShift2V = hitOr1V.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();		// YW ZX WY XZ
	const BoolV hitAnyEdgeV = Or(hitOr1V, hitEdgesShift2V).GetX();						// XYZW ... ... ...

	// We can intersect the polygon either by the segment intersecting one of the edges, but the other possibility
	// is that the whole segment can be inside the polygon. If so, the starting point must be inside of the polygon.
	// We can test that using the directed distance to the edge plane that we computed earlier. At this point,
	// we must combine the results for the four edges using an AND operator - the starting point is inside the
	// convex polygon only if it's on the inside of each of the edges.
	const VecBoolV insideV = IsGreaterThanOrEqual(orthDotV, zeroV);						// X  Y  Z  W
	const VecBoolV insideShift1V = insideV.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>();		// Z  W  X  Y
	const VecBoolV insideAnd1V = And(insideV, insideShift1V);							// XZ YW ZX WY
	const VecBoolV insideShift2V = insideAnd1V.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();	// YW ZX WY XZ
	const BoolV allInsideV = And(insideAnd1V, insideShift2V).GetX();					// XYZW ... ... ...

	// Combine the edge intersection result with the inside polygon test.
	const BoolV intersectedAnythingV = Or(allInsideV, hitAnyEdgeV);

	// We have line of sight if we did not intersect anything.
	return IsFalse(intersectedAnythingV);
}


bool CEntityBoundAI::TestLineOfSightReturnDistance(Vec3V_In startV, Vec3V_In endV, ScalarV_InOut distIfIsectOut) const
{
	// We want to operate on all sides in parallel, so the first thing we do
	// is to put the X coordinates of all corners in one vector, and the Y coordinates
	// of another, similar to transposing a matrix. It would probably be better
	// to store them like that in memory, which would also save some space since the W
	// component now is unused, and the Z coordinate is the same for all the corners.
	// It's still pretty cheap to do, though.
	const Vec3V corner0V = RCC_VEC3V(m_corners[0]);
	const Vec3V corner1V = RCC_VEC3V(m_corners[1]);
	const Vec3V corner2V = RCC_VEC3V(m_corners[2]);
	const Vec3V corner3V = RCC_VEC3V(m_corners[3]);
	const Vec4V temp0AV = MergeXY(Vec4V(corner0V), Vec4V(corner2V));
	const Vec4V temp1AV = MergeXY(Vec4V(corner1V), Vec4V(corner3V));
	const Vec4V cornersXV = MergeXY(temp0AV, temp1AV);
	const Vec4V cornersYV = MergeZW(temp0AV, temp1AV);

	// Compute the direction of the input segment.
	const Vec3V segDirV = Subtract(endV, startV);

	// Rotate the corner coordinate vectors one step. This is needed since we
	// will be looking at them as edges, effectively the previous and the current corner.
	const Vec4V prevCornersXV = cornersXV.Get<Vec::W, Vec::X, Vec::Y, Vec::Z>();
	const Vec4V prevCornersYV = cornersYV.Get<Vec::W, Vec::X, Vec::Y, Vec::Z>();

	// Compute the delta vector for the edge.
	const Vec4V edgeDeltaXV = Subtract(cornersXV, prevCornersXV);
	const Vec4V edgeDeltaYV = Subtract(cornersYV, prevCornersYV);

	// Get 4D vectors for the start position and delta of the segment.
	const Vec4V startXV(startV.GetX());
	const Vec4V startYV(startV.GetY());
	const Vec4V segDirXV(segDirV.GetX());
	const Vec4V segDirYV(segDirV.GetY());

	// Compute deltas from the starting position of the segment, and the first point on each edge.
	const Vec4V startDeltaXV = Subtract(prevCornersXV, startXV);
	const Vec4V startDeltaYV = Subtract(prevCornersYV, startYV);

	// Basically compute a dot product between the normal of the edge, and the delta between the
	// segment start and the edge. This will be a positive value if the starting point is on the
	// inside of the edge.
	const Vec4V orthDotV = SubtractScaled(Scale(edgeDeltaYV, startDeltaXV), edgeDeltaXV, startDeltaYV);

	// Compute the denominator for finding T values, and the absolute value of it, and check its sign.
	const Vec4V zeroV(V_ZERO);
	const Vec4V denominatorV = SubtractScaled(Scale(segDirXV, edgeDeltaYV), segDirYV, edgeDeltaXV);
	const Vec4V absDenominatorV = Abs(denominatorV);
	const VecBoolV denominatorNonNegV = IsGreaterThanOrEqual(denominatorV, zeroV);

	// Check if the denominators are tiny. That would mean that the segment is parallel to an edge,
	// and we can't trust the T value.
	const VecBoolV denominatorValidV = IsGreaterThanOrEqual(absDenominatorV, Vec4V(V_FLT_SMALL_5));

	// We need to see if the intersection is on each edge, rather than outside, i.e. if the T value
	// along each edge is in the 0..1 range. This would technically entail a division by the denominator,
	// but we can avoid that by testing the pre-division value against the 0..denominator range instead,
	// but only if the denominator is positive - if it's negative, we need to flip it around a bit. Fortunately
	// we can do all that quickly.
	const Vec4V hitT1TimesAbsDenominator = SelectFT(denominatorNonNegV, Negate(orthDotV), orthDotV);
	const VecBoolV hitT1InRangeAV = IsGreaterThanOrEqual(hitT1TimesAbsDenominator, zeroV);
	const VecBoolV hitT1InRangeBV = IsLessThanOrEqual(hitT1TimesAbsDenominator, absDenominatorV);
	const Vec4V hitT2TimesDenominatorV = SubtractScaled(Scale(segDirYV, startDeltaXV), segDirXV, startDeltaYV);
	const Vec4V hitT2TimesAbsDenominatorV = SelectFT(denominatorNonNegV, Negate(hitT2TimesDenominatorV), hitT2TimesDenominatorV);
	const VecBoolV hitT2InRangeAV = IsGreaterThanOrEqual(hitT2TimesAbsDenominatorV, zeroV);
	const VecBoolV hitT2InRangeBV = IsLessThanOrEqual(hitT2TimesAbsDenominatorV, absDenominatorV);

	// Combine the result of these tests along each edge. The first T value has to be in range, the second
	// T value has to be in range, and the denominator has to be valid.
	const VecBoolV hitV = And(And(And(hitT1InRangeAV, hitT1InRangeBV), And(hitT2InRangeAV, hitT2InRangeBV)), denominatorValidV);

	// We need to OR across the components to determine if any of the edges hit.
	const VecBoolV hitEdgesShift1V = hitV.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>();		// Z  W  X  Y
	const VecBoolV hitOr1V = Or(hitV, hitEdgesShift1V);									// XZ YW ZX WY
	const VecBoolV hitEdgesShift2V = hitOr1V.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();		// YW ZX WY XZ
	const BoolV hitAnyEdgeV = Or(hitOr1V, hitEdgesShift2V).GetX();						// XYZW ... ... ...

	// We can intersect the polygon either by the segment intersecting one of the edges, but the other possibility
	// is that the whole segment can be inside the polygon. If so, the starting point must be inside of the polygon.
	// We can test that using the directed distance to the edge plane that we computed earlier. At this point,
	// we must combine the results for the four edges using an AND operator - the starting point is inside the
	// convex polygon only if it's on the inside of each of the edges.
	const VecBoolV insideV = IsGreaterThanOrEqual(orthDotV, zeroV);						// X  Y  Z  W
	const VecBoolV insideShift1V = insideV.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>();		// Z  W  X  Y
	const VecBoolV insideAnd1V = And(insideV, insideShift1V);							// XZ YW ZX WY
	const VecBoolV insideShift2V = insideAnd1V.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();	// YW ZX WY XZ
	const BoolV allInsideV = And(insideAnd1V, insideShift2V).GetX();					// XYZW ... ... ...

	// Combine the edge intersection result with the inside polygon test.
	const BoolV intersectedAnythingV = Or(allInsideV, hitAnyEdgeV);

	// We have line of sight if we did not intersect anything.
	if(IsFalse(intersectedAnythingV))
	{
		return true;
	}

	// Note: it's not clear if it's faster to do the branch above, or just compute it all
	// either way - probably depends on the likelihood of us actually finding an intersection.
	// This function is currently used for path tests and stuff, where it's more common to
	// not have an intersection, so the early branch may be worth it.

	// Perform a division to compute the T value along the input segment, for each intersection
	// with an edge (if one exists).
	// Note: we use InvScaleFast() here, but we intentionally don't use the value to determine
	// if there was an intersection. So, the distance we get out may have some inaccuracy, but
	// the intersection should be identical to the regular TestLineOfSight().
	const Vec4V hitT1V = InvScaleFast(orthDotV, denominatorV);
	// For the invalid hits, we mask in FLT_MAX as the T value.
	const Vec4V maskedHitT1V = SelectFT(hitV, Vec4V(V_FLT_MAX), hitT1V);

	// To compute the closest T value for the edge intersections, we do a similar shift as we did above,
	// but with a Min operator.
	const Vec4V maskedHitT1Shift1V = maskedHitT1V.Get<Vec::Z, Vec::W, Vec::X, Vec::Y>();
	const Vec4V maskedHitT1Min1V = Min(maskedHitT1V, maskedHitT1Shift1V);
	const Vec4V maskedHitT1Shift2V = maskedHitT1Min1V.Get<Vec::Y, Vec::Z, Vec::W, Vec::X>();
	const ScalarV maskedHitT1MinV = Min(maskedHitT1Min1V, maskedHitT1Shift2V).GetX();

	// For the final T value, we use zero if the starting point was found to be inside, otherwise we use
	// the minimum of the edge intersections. This should work since we masked in FLT_MAX for the invalid
	// T values already.
	const ScalarV finalHitTV = SelectFT(allInsideV, maskedHitT1MinV, ScalarV(V_ZERO));

	// Compute the magnitude of the segment, and multiply by the T value.
	// Note that we use MagFast() here, again, assuming that the user doesn't need
	// a totally accurate distance.
	const ScalarV segLenV = MagFast(segDirV);
	const ScalarV distV = Scale(segLenV, finalHitTV);

	// Store the distance.
	distIfIsectOut = distV;

	return false;
}

// Old code, please don't use in new code unless there is a really good reason.
bool CEntityBoundAI::TestLineOfSightOld(const Vector3 & vStart, const Vector3 & vEnd, Vector3* normals, float* ds, const int numPlanes)
{
	Vector3 vClippedStart = vStart;
	Vector3 vClippedEnd = vEnd;
	float fStartDot, fEndDot, s;
	static const float fEps = 0.0f;
	int p;

	for(p=0; p<numPlanes; p++)
	{
		fStartDot = DotProduct(normals[p], vClippedStart) + ds[p];
		fEndDot = DotProduct(normals[p], vClippedEnd) + ds[p];

		// Both start & end are in front of a plane, so no intersection is possible
		if(fStartDot > fEps && fEndDot > fEps)
		{
			break;
		}
		else if(fStartDot > fEps && fEndDot < -fEps)
		{
			s = fStartDot / (fStartDot - fEndDot);
			vClippedStart = vClippedStart + ((vClippedEnd - vClippedStart) * s);
		}
		else if(fStartDot < -fEps)
		{
			if(fEndDot > fEps)
			{
				s = fStartDot / (fStartDot - fEndDot);
				vClippedEnd = vClippedStart + ((vClippedEnd - vClippedStart) * s);
			}
		}
	}
	
	if(p==4)
		return false;

	return true;
}

//Compute the crossing points v1 and v2 of an unbounded line containing point v in direction w.
//Return false if line doesn't cross the bound.
bool CEntityBoundAI::ComputeCrossingPoints(const Vector3& v, const Vector3& w, Vector3& v1, Vector3& v2) const
{
	MUST_FIX_THIS(gordon - need a replacement for this under rage);
	if(!fwGeom::IntersectSphereRay(m_sphere, v,w,v1,v2))
	{
		return false;
	}

	Vector3 normals[4];
	float ds[4];
	ComputeBoundingBoxPlanes(normals,ds);

	return ComputeCrossingPoints(v,w,normals,ds,4,v1,v2);
}

bool CEntityBoundAI::ComputeCrossingPoints(const Vector3& v, const Vector3& w, Vector3* normals, float* ds, const int numPlanes, Vector3& v1, Vector3& v2)
{
	int i;
	for(i=0;i<numPlanes;i++)
	{
		const float f1=DotProduct(v1,normals[i])+ds[i];
		const float f2=DotProduct(v2,normals[i])+ds[i];
		if(f1>0.001f && f2>0.001f)
		{
			//Both v1 and v2 lie on outside of a plane so line doesn't
			//intersect bounding box.
			return false;
		}
		else if(f1>0.001f)
		{
			//v1 lies on outside of plane i.
			//Push v1 on to the plane by moving along w from v by a distance t.
			const float t=-(ds[i] + DotProduct(normals[i],v))/(DotProduct(normals[i],w));
			//v1.FromMultiply(w,t);
			v1=w*t;
			v1+=v;
		}
		else if(f2>0.001f)
		{
			const float t=-(ds[i] + DotProduct(normals[i],v))/(DotProduct(normals[i],w));
			//v2.FromMultiply(w,t);
			v2=w*t;
			v2+=v;
		}
	}

	// Ensure that crossing points are ordered such that v1 is the first crossing
	if( (v-v1).Mag2() < (v-v2).Mag2() )
	{
		SwapEm(v1, v2);
	}

	return true;
}

#if 0	// Disabled, should use TestLineOfSightReturnDistance() instead for much better performance (but be aware that the return value is flipped).

bool CEntityBoundAI::ComputeDistanceToIntersection(const Vector3& vStart, const Vector3& vEnd, float& fDistToIsect) const
{
	fDistToIsect = 0.0f;

	Vector3 v1,v2;
	Vector3 normals[4];
	float ds[4];
	ComputeBoundingBoxPlanes(normals, ds);
	return ComputeDistanceToIntersection(vStart, vEnd, normals, ds, 4, fDistToIsect);
}

#endif	// 0

bool CEntityBoundAI::ComputeDistanceToIntersection(const Vector3& vStart, const Vector3& vEnd, Vector3* normals, float* ds, const int numPlanes, float& fDistToIsect)
{
	Vector3 v1=vStart;
	Vector3 v2=vEnd;

	const Vector3& v=vStart;
	Vector3 w;
	w=v2-v1;
	w.Normalize();

	int i;
	for(i=0;i<numPlanes;i++)
	{
		const float f1=DotProduct(v1,normals[i])+ds[i];
		const float f2=DotProduct(v2,normals[i])+ds[i];
		if(f1>0.001f && f2>0.001f)
		{
			//Both v1 and v2 lie on outside of a plane so line doesn't
			//intersect bounding box.
			return false;
		}
		else if(f1>0.001f)
		{
			//v1 lies on outside of plane i.
			//Push v1 on to the plane by moving along w from v by a distance t.
			const float t=-(ds[i] + DotProduct(normals[i],v))/(DotProduct(normals[i],w));
			v1=w*t;
			v1+=v;
		}
		else if(f2>0.001f)
		{
			const float t=-(ds[i] + DotProduct(normals[i],v))/(DotProduct(normals[i],w));
			v2=w*t;
			v2+=v;
		}
	}

	fDistToIsect=vStart.Dist(v1);

	return true;
}

#if __BANK
void CEntityBoundAI::fwDebugDraw(Color32 col)
{
	grcDebugDraw::Line(m_corners[0], m_corners[1], col);
	grcDebugDraw::Line(m_corners[1], m_corners[2], col);
	grcDebugDraw::Line(m_corners[2], m_corners[3], col);
	grcDebugDraw::Line(m_corners[3], m_corners[0], col);
}
#endif

///////////////////////////////
//PED ROUTE CALCULATOR
///////////////////////////////


//*****************************************************************************
//	ComputeRouteOutToEdgeOfBound
//	This function calculates a route where the start & end positions are both
//	on the same side (left or right) of the bound, but there is an obstacle in
//	the way - typically a door).  If the target is within the bound, then we'll
//	create a route out to the edge & then back in again to the target.  If the
//	target is not within the bound, then we'll create a route which goes out to
//	the edge, and then to the corner closest to the target.

void CPedGrouteCalculator::ComputeRouteOutToEdgeOfBound(const Vector3 & vStartPos, const Vector3 & vTargetPos, CEntityBoundAI & bound, CPointRoute & route, const int iForceThisSide, CEntity * pEntity, const eHierarchyId iDoorPedIsHeadingFor, const float fSideExtendAmount)
{
	const bool bTargetInsideBound = bound.LiesInside(vTargetPos);

	int iExtendPt = -1;

	if(bTargetInsideBound ||
		((pEntity->GetIsTypeVehicle() && ((CVehicle*)pEntity)->GetVehicleType()==VEHICLE_TYPE_HELI)
			&& (iDoorPedIsHeadingFor==VEH_DOOR_DSIDE_R || iDoorPedIsHeadingFor==VEH_DOOR_PSIDE_R)))
	{
		static const int iMethod = 2;

		// Create a simple route out to the edge of the bound & back
		if(iMethod==0)
		{
			Vector3 vPosOnEdge1,vPosOnEdge2;
			route.Add(vStartPos);
			bound.MoveOutside(vStartPos, vPosOnEdge1, iForceThisSide);
			iExtendPt = route.GetSize();
			route.Add(vPosOnEdge1);
			bound.MoveOutside(vTargetPos, vPosOnEdge2, iForceThisSide);
			route.Add(vPosOnEdge2);
			route.Add(vTargetPos);
		}
		else if(iMethod==1)
		{
			Vector3 vPosOnEdge1,vPosOnEdge2;
			route.Add(vStartPos);
			bound.MoveOutside(vStartPos, vPosOnEdge1, iForceThisSide);
			bound.MoveOutside(vTargetPos, vPosOnEdge2, iForceThisSide);
			iExtendPt = route.GetSize();
			route.Add( (vPosOnEdge1+vPosOnEdge2)*0.5f );
			route.Add(vTargetPos);
		}
		else if(iMethod==2)
		{
			Vector3 vPosOnEdge1,vPosOnEdge2;
			route.Add(vStartPos);
			bound.MoveOutside(vStartPos, vPosOnEdge1, iForceThisSide);
			bound.MoveOutside(vTargetPos, vPosOnEdge2, iForceThisSide);
			
			route.Add(vPosOnEdge1);
			iExtendPt = route.GetSize();
			route.Add((vPosOnEdge1*0.85f) + (vPosOnEdge2*0.15f));
			route.Add(vPosOnEdge2);
			route.Add(vTargetPos);
		}
	}
	else
	{
		// Find which corner is closest to the target
		Vector3 vCorners[4];
		const Vector3 * pClosestCorner = NULL;
		bound.GetCorners(vCorners);
		float fClosest = FLT_MAX;
		for(int i=0; i<4; i++)
		{
			const float fDist2 = (vTargetPos - vCorners[i]).XYMag2();
			if(fDist2 < fClosest)
			{
				fClosest = fDist2;
				pClosestCorner = &vCorners[i];
			}
		}
		Assert(pClosestCorner);

		Vector3 vPosOnEdge1,vPosOnEdge2;
		route.Add(vStartPos);
		bound.MoveOutside(vStartPos, vPosOnEdge1, iForceThisSide);
		iExtendPt = route.GetSize();
		route.Add(vPosOnEdge1);
		route.Add(*pClosestCorner);
	}

	// Optionally extend the walk-round-door route a little further out beyond the side of the vehicle.
	// This is intended to help problems where the ped 'achieves' the first route point before they've
	// actually cleared the door - leaving them stuck behind it & heading for the next route point.
	if(fSideExtendAmount > 0.0f && iExtendPt >= 1)
	{
		Assert(iExtendPt>=1 && iExtendPt<route.GetSize());
		Vector3 vToEdge = route.Get(iExtendPt) - route.Get(iExtendPt-1);
		vToEdge.Normalize();
		vToEdge *= fSideExtendAmount;
		route.Set( iExtendPt, route.Get(iExtendPt) + vToEdge);
	}
}

CPedGrouteCalculator::ERouteRndEntityRet CPedGrouteCalculator::ComputeRouteRoundEntityBoundingBox(
	const CPed& ped,
	CEntity& entity, const Vector3& vTargetPos, const Vector3& vTargetPosForLosTest,
	CPointRoute& route, const u32 iTestFlags, const int iForceDirection, const bool bTestCollisionModel) 
{
	((void)bTestCollisionModel);
	return ComputeRouteRoundEntityBoundingBox(ped, VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()), entity, vTargetPos, vTargetPosForLosTest, route, iTestFlags, iForceDirection);
}

bool IsAnOpenableDoor(const CEntity * pEntity)
{
	return (pEntity->GetIsTypeObject() && ((CObject*)pEntity)->IsADoor() && !((CObject*)pEntity)->IsBaseFlagSet(fwEntity::IS_FIXED));
}

//*********************************************************************************************
//	GetComponentsForBasicVehicleTest
//	This returns a list of the basic components used for testing a vehicle's bounds.
//	It is used as an optimization over the WorldProbe::TestCapsuleAgainstEntity function
//	which is pretty slow.  By testing only a small subset of a vehicle's components we should
//	get a decent speed-up.  This is only possible against cars - helis, etc have various other
//	components which stick out.

int GetComponentsForBasicVehicleTest(CVehicle * pVehicle, const eHierarchyId iDoorPedIsHeadingFor, int * pComponents)
{
	static const u32 iNumComponents = 5;
	static const eHierarchyId iIDs[iNumComponents] =
	{
		VEH_CHASSIS, VEH_DOOR_DSIDE_F, VEH_DOOR_DSIDE_R, VEH_DOOR_PSIDE_F, VEH_DOOR_PSIDE_R
	};

	int iNum = 0;
	for(unsigned int i=0; i<iNumComponents; i++)
	{
		if(iIDs[i]!=iDoorPedIsHeadingFor)
		{
			const int iComponent = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex(iIDs[i]));
			if(iComponent >= 0)
				pComponents[iNum++] = iComponent;
		}
	}
	return iNum;
}

int GetComponentsForBasicHeliTest(CVehicle * pVehicle, const eHierarchyId iDoorPedIsHeadingFor, int * pComponents)
{
	Assert(pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI);

	static const u32 iNumComponents = 7;
	static const eHierarchyId iIDs[iNumComponents] =
	{
		VEH_CHASSIS, VEH_DOOR_DSIDE_F, VEH_DOOR_DSIDE_R, VEH_DOOR_PSIDE_F, VEH_DOOR_PSIDE_R,
		VEH_MISC_E,	// tailplane
		VEH_EXTRA_1 // wing & weapons hardpoints
	};

	int iNum = 0;
	for(unsigned int i=0; i<iNumComponents; i++)
	{
		if(iIDs[i]!=iDoorPedIsHeadingFor)
		{
			const int iComponent = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex(iIDs[i]));
			if(iComponent >= 0)
				pComponents[iNum++] = iComponent;
		}
	}
	return iNum;
}

//*****************************************************************************************
//	IsVehicleSuitableForLowDetailCollisionTest
//	Navigating around vehicles can be fairly costly due to the need for capsule tests
//	which allow us to better navigate around open doors, non-convex geometry, etc.
//	To optimize this I've written some custom collision test functions which work against
//	a composite bound's bounding-boxes.  This is only suitable when we're a little distance
//	away from the vehicle, otherwise we may appear to be within the vehicle's bbox.

bool IsVehicleSuitableForLowDetailCollisionTest(CPed * pPed, const Vector3 & vTargetPos, CVehicle * pVehicle, CEntityBoundAI * pBoundAI)
{
	// Helis are too bloody complicated to do any low-detail tests against!
	if(pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI)
		return false;

	static const float fMinPlanarDist = 0.75f;
	Vector3 pPlaneNormals[4];
	float fPlaneDists[4];
	pBoundAI->GetPlanes(pPlaneNormals, fPlaneDists);
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	if((vPedPos - vTargetPos).Mag2() < 4.0f)
	{
		return false;
	}

	for(int p=0; p<4; p++)
	{
		const float fDist = DotProduct(pPlaneNormals[p], vPedPos) + fPlaneDists[p];
		if(fDist> fMinPlanarDist)
			return true;
	}
	return false;
}

//**************************************************************
//	TestRouteSegAgainstEntity
//	Wrapper for capsule/line test functions.
//	Returns true if hit something, or false if no intersection.

bool DoesRouteSegHitEntity(const Vector3& vStart, const Vector3 &vEnd, const float fRadius, CEntity * pEntity, const bool bUseMultipleCapsules, const bool bBoundingBoxesTest, const eHierarchyId iDoorPedIsHeadingFor)
{
	Assertf(IsLessThanAll(Mag(VECTOR3_TO_VEC3V(vStart)),ScalarV(V_FLT_LARGE_8)),"DoesRouteSegHitEntity has an vStart %f, %f, %f", vStart.x, vStart.y, vStart.z);
	Assertf(IsLessThanAll(Mag(VECTOR3_TO_VEC3V(vEnd)),ScalarV(V_FLT_LARGE_8)),"DoesRouteSegHitEntity has an impractical vEnd %f, %f, %f", vEnd.x, vEnd.y, vEnd.z);

	if(!pEntity || !pEntity->GetCurrentPhysicsInst() || !pEntity->GetCurrentPhysicsInst()->IsInLevel())
		return false;

	static bool bUseBBoxTest = false;
	if(bBoundingBoxesTest || bUseBBoxTest)
	{
		int iComponents[8];
		int iNumComponents = 0;

		if(pEntity->GetType()==ENTITY_TYPE_VEHICLE)
		{
			if(((CVehicle*)pEntity)->GetVehicleType() == VEHICLE_TYPE_CAR)
			{
				iNumComponents = GetComponentsForBasicVehicleTest((CVehicle*)pEntity, iDoorPedIsHeadingFor, iComponents);
			}
			else if(((CVehicle*)pEntity)->GetVehicleType() == VEHICLE_TYPE_HELI)
			{
				iNumComponents = GetComponentsForBasicHeliTest((CVehicle*)pEntity, iDoorPedIsHeadingFor, iComponents);
			}
		}

		bool bHit = CPedGeometryAnalyser::TestCapsuleAgainstComposite(pEntity, vStart, vEnd, fRadius, true, 0, NULL, iNumComponents, iComponents) != NULL;
		return bHit;
	}

	// Test that this entity has a phInst.
	phInst *pTestInst = pEntity->GetFragInst();
	if(pTestInst==NULL) pTestInst = pEntity->GetCurrentPhysicsInst();
	if(pTestInst==NULL) return false;

	WorldProbe::CShapeTestFixedResults<> shapeTestResult;
	bool bHit = false;

	if(bUseMultipleCapsules)
	{
		static const int iNumTests = 3;
		Vector3 vTestOffsets[3] =
		{
			Vector3(0.0f, 0.0f, 1.0f),
			Vector3(0.0f, 0.0f, 0.0f),
			Vector3(0.0f, 0.0f, -0.75f)
		};

		for(int t=0; t<iNumTests; ++t)
		{
			const Vector3 vCpsStart = vStart + vTestOffsets[t];
			const Vector3 vCpsEnd = vEnd + vTestOffsets[t];

			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetResultsStructure(&shapeTestResult);
			capsuleDesc.SetCapsule(vCpsStart, vCpsEnd, fRadius);
			capsuleDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
			capsuleDesc.SetIncludeEntity(pEntity);
			capsuleDesc.SetDoInitialSphereCheck(true);
			capsuleDesc.SetIsDirected(false);

			bHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
			if(bHit) break;
		}
	}
	else
	{
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetResultsStructure(&shapeTestResult);
		capsuleDesc.SetCapsule(vStart, vEnd, fRadius);
		capsuleDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		capsuleDesc.SetIncludeEntity(pEntity);
		capsuleDesc.SetDoInitialSphereCheck(true);
		capsuleDesc.SetIsDirected(true);

		bHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
	}

	//Assert(!bHit || (bHit && shapeTestResult[0].GetHitInst()));

	if(!bHit || shapeTestResult[0].GetHitInst()==0 )
		return false;

	CEntity* pHitEntity = (CEntity*)shapeTestResult[0].GetHitInst()->GetUserData();
	Assert(pHitEntity);

	// Was this an openable door?  (not sure how this could be - vehicle doors are handled differently, see below)
	if(IsAnOpenableDoor(pHitEntity))
		return false;

	if(iDoorPedIsHeadingFor!=VEH_INVALID_ID && pEntity->GetType()==ENTITY_TYPE_VEHICLE)
	{
		// Is this is a vehicle and this capsule test hit the door we are heading towards, then don't count it as an obstruction
		int iDoorParts[MAX_DOOR_COMPONENTS];
		int iNumDoorParts = CCarEnterExit::GetDoorComponents( ((CVehicle*)pEntity), iDoorPedIsHeadingFor, iDoorParts);
		for(int p=0; p<iNumDoorParts; p++)
		{
			if(shapeTestResult[0].GetHitComponent()==iDoorParts[p])
				return false;
		}
		// If we are heading for a front door, and the capsule test hit a rear door - but we are able
		// to push that door closed, then don't count it as an obstacle
		eHierarchyId iDoorBehind = VEH_INVALID_ID;
		if(iDoorPedIsHeadingFor==VEH_DOOR_DSIDE_F)
			iDoorBehind = VEH_DOOR_DSIDE_R;
		else if(iDoorPedIsHeadingFor==VEH_DOOR_PSIDE_F)
			iDoorBehind = VEH_DOOR_PSIDE_R;

		if(iDoorBehind != VEH_INVALID_ID)
		{
			int iNumDoorParts = CCarEnterExit::GetDoorComponents( ((CVehicle*)pEntity), iDoorBehind, iDoorParts);
			for(int p=0; p<iNumDoorParts; p++)
			{
				if(shapeTestResult[0].GetHitComponent()==iDoorParts[p])
				{
					s32 iDoorPart = ((CVehicle*)pEntity)->GetFragInst()->GetComponentFromBoneIndex(((CVehicle*)pEntity)->GetBoneIndex(iDoorBehind) );
					CCarDoor * pDoor = ((CVehicle*)pEntity)->GetDoorFromId(iDoorBehind);
					if(pDoor && !pDoor->GetIsClosed())
					{
						phBound * pVehBound = pEntity->GetFragInst()->GetArchetype()->GetBound();
						Assert(pVehBound && pVehBound->GetType()==phBound::COMPOSITE);
						// Get the door matrix, so we can find the door's position relative to the vehicle
						const Matrix34 & doorMat = RCC_MATRIX34(((phBoundComposite*)pVehBound)->GetCurrentMatrix(iDoorPart));
						Matrix34 invVehicleMat;
						invVehicleMat.Inverse(MAT34V_TO_MATRIX34(pEntity->GetMatrix()));
						// Get ped's position relative to the vehicle.  See if the ped is closer to the front
						// of the vehicle than the door, indicating that it is not an obstacle.
						Vector3 vPedLocalPos;
						invVehicleMat.Transform(vStart, vPedLocalPos);
						if(vPedLocalPos.y < doorMat.d.y + PED_HUMAN_RADIUS)
							return false;
					}
				}
			}
		}
	}

	return (pHitEntity!=NULL);
}



CPedGrouteCalculator::ERouteRndEntityRet CreateClockwiseOrAntiClockwiseRoute(
	const CPed * pPed,
	CEntity * pEntity,
	CEntityBoundAI * pBoundAI,
	const Vector3 & vStartPos,
	const Vector3 & vTargetPos,
	const int iStartHitSide,
	const int iTargetHitSide,
	CPointRoute * pRoute,
	const bool bTestCollisionModel,
	const bool bIsComplicatedBound,
	const bool bUseBoundingBoxTest,
	const eHierarchyId iDoorPedIsHeadingFor,
	const Vector3 * vCorners,
	const Vector3 * vConvexCorners,
	const u32 iLosIncludeFlags,
	const float fLosCapsuleRadius,
	const int iForceDirection)
{

	Assertf(IsLessThanAll(Mag(VECTOR3_TO_VEC3V(vStartPos)),ScalarV(V_FLT_LARGE_8)),"CreateClockwiseOrAntiClockwiseRoute has an vStartPos %f, %f, %f", vStartPos.x, vStartPos.y, vStartPos.z);
	Assertf(IsLessThanAll(Mag(VECTOR3_TO_VEC3V(vTargetPos)),ScalarV(V_FLT_LARGE_8)),"CreateClockwiseOrAntiClockwiseRoute has an impractical vTargetPos %f, %f, %f", vTargetPos.x, vTargetPos.y, vTargetPos.z);

	//********************************************************
	// Create 2 paths round each side of the entity.

	Vector3 vPath1[6];
	Vector3 vPath2[6];
	int iPath1Size = 0;
	int iPath2Size = 0;
	int iSide;

	//***************
	// Clockwise

	vPath1[iPath1Size++] = vStartPos;
	iSide = iStartHitSide;
	while(iSide != iTargetHitSide)
	{
		int iCornerIndex = iSide-1;
		if(iCornerIndex < 0) iCornerIndex = 3;

		if(bTestCollisionModel)
		{
			if(!DoesRouteSegHitEntity(vPath1[iPath1Size-1], vConvexCorners[iCornerIndex], PED_HUMAN_RADIUS, pEntity, bIsComplicatedBound, bUseBoundingBoxTest, iDoorPedIsHeadingFor))
				vPath1[iPath1Size++] = vConvexCorners[iCornerIndex];
			else
				vPath1[iPath1Size++] = vCorners[iCornerIndex];
		}
		else
		{
			vPath1[iPath1Size++] = vCorners[iCornerIndex];
		}

		iSide--;
		if(iSide < 0) iSide = 3;
	}
	if(DoesRouteSegHitEntity(vPath1[iPath1Size-1], vTargetPos, PED_HUMAN_RADIUS, pEntity, bIsComplicatedBound, bUseBoundingBoxTest, iDoorPedIsHeadingFor))
	{
		Vector3 vTgtPosOnEdge;
		pBoundAI->MoveOutside(vTargetPos, vTgtPosOnEdge);
		vPath1[iPath1Size++] = vTgtPosOnEdge;
	}
	vPath1[iPath1Size++] = vTargetPos;


	//***************
	// Anti-Clockwise

	vPath2[iPath2Size++] = vStartPos;
	iSide = iStartHitSide;
	while(iSide != iTargetHitSide)
	{
		int iCornerIndex = iSide;

		if(bTestCollisionModel)
		{
			if(!DoesRouteSegHitEntity(vPath2[iPath2Size-1], vConvexCorners[iCornerIndex], PED_HUMAN_RADIUS, pEntity, bIsComplicatedBound, bUseBoundingBoxTest, iDoorPedIsHeadingFor))
				vPath2[iPath2Size++] = vConvexCorners[iCornerIndex];
			else
				vPath2[iPath2Size++] = vCorners[iCornerIndex];
		}
		else
		{
			vPath2[iPath2Size++] = vCorners[iCornerIndex];
		}

		iSide++;
		if(iSide > 3) iSide = 0;
	}
	if(DoesRouteSegHitEntity(vPath2[iPath2Size-1], vTargetPos, PED_HUMAN_RADIUS, pEntity, bIsComplicatedBound, bUseBoundingBoxTest, iDoorPedIsHeadingFor))
	{
		Vector3 vTgtPosOnEdge;
		pBoundAI->MoveOutside(vTargetPos, vTgtPosOnEdge);
		vPath2[iPath2Size++] = vTgtPosOnEdge;
	}
	vPath2[iPath2Size++] = vTargetPos;


	//****************************************************************************************************************

	bool bIsPath1Clear = true;
	bool bIsPath2Clear = true;
	int iChosenPath=-1;
	CEntity* pHitEnt1 = NULL;
	int i;

	// If a particular direction round the entity is being specified - then skip this bit   
	if(iForceDirection == CPedGrouteCalculator::DIR_UNSPECIFIED)
	{
		WorldProbe::CShapeTestFixedResults<> capsuleResult;

		//Test each path for obstructions.
		Vector3 vCentre(0,0,0);
		int k;
		for(k=0;k<4;k++)
		{
			vCentre+=vCorners[k];
		}
		vCentre*=0.25f;

		int iPath1Block = iPath1Size;

		if(iLosIncludeFlags)
		{
			Vector3 v0=vPath1[0];
			for(i=1;i<iPath1Size;i++)
			{
				const Vector3& v1=vPath1[i];

				INC_PEDAI_LOS_COUNTER;
				WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
				capsuleDesc.SetResultsStructure(&capsuleResult);
				capsuleDesc.SetCapsule(v0, v1, fLosCapsuleRadius);
				capsuleDesc.SetIncludeFlags(iLosIncludeFlags);
				capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
				capsuleDesc.SetIsDirected(true);
				WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

				if(capsuleResult[0].GetHitDetected())
				{
					pHitEnt1 = CPhysics::GetEntityFromInst(capsuleResult[0].GetHitInst());
					if(pHitEnt1 && (pHitEnt1!=pEntity) && (pHitEnt1!=(CPed*)(pPed)) && !IsAnOpenableDoor(pHitEnt1))
					{
						if(pHitEnt1->GetIsTypePed())
						{
							//The entity is a ped.
							if(((CPed*)pHitEnt1)->GetMotionData()->GetIsStill())
							{
								//The hit ped is idle so the path is blocked.
								iPath1Block=i;
								bIsPath1Clear=false; 
								break;  
							}
							else if(DotProduct(v1-v0, VEC3V_TO_VECTOR3(pHitEnt1->GetTransform().GetB())) < 0.0f)
							{
								//The hit ped isn't idle but is walking towards this ped so the path is blocked.
								iPath1Block=i;
								bIsPath1Clear=false;   
								break;
							}
						}
						else
						{
							iPath1Block=i;
							bIsPath1Clear=false;
							break;
						}
					}
					else
					{
						pHitEnt1 = NULL;
					}
				}

				v0=v1;    
			}
		}

		CEntity* pHitEnt2 = NULL;
		int iPath2Block = iPath2Size;

		if(iLosIncludeFlags)
		{
			Vector3 v0=vPath2[0];
			for(i=1;i<iPath2Size;i++)
			{
				const Vector3& v1=vPath2[i];

				INC_PEDAI_LOS_COUNTER;
				capsuleResult.Reset();
				WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
				capsuleDesc.SetResultsStructure(&capsuleResult);
				capsuleDesc.SetCapsule(v0, v1, fLosCapsuleRadius);
				capsuleDesc.SetIncludeFlags(iLosIncludeFlags);
				capsuleDesc.SetExcludeEntity(pEntity);
				capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
				capsuleDesc.SetIsDirected(true);
				WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

				if(capsuleResult[0].GetHitDetected())
				{
					pHitEnt2 = CPhysics::GetEntityFromInst(capsuleResult[0].GetHitInst());
					if(pHitEnt2 && (pHitEnt2!=pEntity) && (pHitEnt2!=(CPed*)(pPed)) && !IsAnOpenableDoor(pHitEnt2))
					{
						if(pHitEnt2->GetIsTypePed())
						{
							//The entity is a ped.
							if(((CPed*)pHitEnt2)->GetMotionData()->GetIsStill())
							{
								//The hit ped is idle so the path is blocked.
								iPath2Block=i;
								bIsPath2Clear=false;   
								break;
							}
							else if(DotProduct(v1-v0, VEC3V_TO_VECTOR3(pHitEnt2->GetTransform().GetB())) < 0.0f)
							{
								//The hit ped isn't idle but is walking towards this ped so the path is blocked.
								iPath2Block=i;
								bIsPath2Clear=false; 
								break;  
							}
						}
						else
						{
							iPath2Block=i;
							bIsPath2Clear=false;
							break;
						}
					}
					else
					{
						pHitEnt2 = NULL;
					}
				}

				v0=v1;    
			}
		}

		//Compute the length of each path.
		float fLength1 = 0;
		for(i=0;i<iPath1Size-1;i++)
		{
			fLength1 += (vPath1[i+1]-vPath1[i+0]).Mag();
		}
		float fLength2=0;
		for(i=0;i<iPath2Size-1;i++)
		{
			fLength2 += (vPath2[i+1]-vPath2[i+0]).Mag();
		}    

		if(bIsPath1Clear && bIsPath2Clear)
		{
			//Both paths are clear so choose the shortest path.
			iChosenPath = fLength1 < fLength2 ? 1 : 2;
		}   
		else if(bIsPath1Clear)
		{
			//Path 1 clear but path 2 blocked.
			iChosenPath = 1;
		}
		else if(bIsPath2Clear)
		{
			//Path 2 clear but path 1 blocked.
			iChosenPath = 2;
		}
		else
		{
			if((1==iPath1Block)&&(iPath2Block>1))
			{
				//Path 1 has a block on the way to the first node
				//so choose path 2 which is clear at least to the first corner.
				iChosenPath = 2;
			}
			else if((iPath1Block>1)&&(1==iPath2Block))
			{
				//Path 2 has a block on the way to the first node
				//so choose path 1 which is clear at least to the first corner.
				iChosenPath = 1;
			}
			else if(pHitEnt1==pHitEnt2)
			{
				//The same entity blocks both paths so choose the shortest path.
				iChosenPath = fLength1 < fLength2 ? 1 : 2;
			}
			else if(!pHitEnt1->IsArchetypeSet() || !pHitEnt2->IsArchetypeSet())
			{
				// Handle the slightly odd case where one of the entities has a valid model index
				iChosenPath = 1;
			}
			else
			{
				//Choose the path with the smallest blockage.
				const float fRadiusBlockage1 = pHitEnt1->GetBoundRadius();
				const float fRadiusBlockage2 = pHitEnt2->GetBoundRadius();
				iChosenPath = fRadiusBlockage1 < fRadiusBlockage2 ? 1 : 2;
			}
		}
	}   

	// If we are forcing a particular direction, then choose which path here..
	else
	{
		Assert(iForceDirection == CPedGrouteCalculator::DIR_ANTICLOCKWISE || iForceDirection == CPedGrouteCalculator::DIR_CLOCKWISE);
		iChosenPath = (iForceDirection == CPedGrouteCalculator::DIR_ANTICLOCKWISE) ? 1 : 2;
	}


	Assert(iChosenPath==1 || iChosenPath==2);

	if(1==iChosenPath)
	{
		i=0;
		while(i<iPath1Size)
		{
			pRoute->Add(vPath1[i]);
			i++;
		}
	}
	else if(2==iChosenPath)
	{
		i=0;
		while(i<iPath2Size)
		{
			pRoute->Add(vPath2[i]);
			i++;
		}
	}

	//********************************************************************************************************
	// See if we can go straight to waypoint 1

	if(iLosIncludeFlags!=0 && !bIsComplicatedBound)
	{
		bool bCanGoStraightTo2ndWpt = false;
		INC_PEDAI_LOS_COUNTER;

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetCapsule(vStartPos, pRoute->Get(1), fLosCapsuleRadius);
		capsuleDesc.SetIncludeFlags(iLosIncludeFlags);
		bCanGoStraightTo2ndWpt = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

		if(bCanGoStraightTo2ndWpt)
		{
			pRoute->Remove(0);
		}

		// And again

		capsuleDesc.SetCapsule(vStartPos, pRoute->Get(1), fLosCapsuleRadius);
		bCanGoStraightTo2ndWpt = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

		if(bCanGoStraightTo2ndWpt)
		{
			pRoute->Remove(0);
		}
	}

	// Return appropriate value
	if(!bIsPath1Clear && !bIsPath2Clear)
	{
		return CPedGrouteCalculator::RouteRndEntityRet_BlockedBothWays;
	}
	else
	{
		return (iChosenPath == 1) ? CPedGrouteCalculator::RouteRndEntityRet_AntiClockwise : CPedGrouteCalculator::RouteRndEntityRet_Clockwise;
	}
}

//******************************************************************************************
//	ComputeRouteRoundEntityBoundingBox
//	Calculates the shortest route around an entity's bounding box.
//	This function has become very complicated & inefficient due to the need for testing for
//	car doors, which are attached collision & not individual entities and essentially makes
//	vehicles non-convex.
//	TODO: special-case routes around vehicles and revert this function to a simpler version.


CPedGrouteCalculator::ERouteRndEntityRet CPedGrouteCalculator::ComputeRouteToDoor(
	CVehicle * pVehicle, CPed * pPed, CPointRoute * pRoute,
	const Vector3 & vStartPos, const Vector3 & vTargetPos, const Vector3 & vTargetPosForLosTest,
	const u32 iLosIncludeFlags, const bool bTestCollisionModel, const eHierarchyId iDoorPedIsHeadingFor)
{
	static const float fPlaneTolerance = 0.2f;
	static const float fCapsuleRadius = 0.15f;	// PED_HUMAN_RADIUS
	phIntersection intersection;

	const bool bIsComplicatedBound = (pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI);
	
	Vector3 vCorners[4];
	Vector3 vNormals[4];
	float fPlaneDists[4];
	int i;

	//***************************************************************************
	// First off, get the corners & planes of the entity without adding on the
	// adjustment for the ped's width.  If both the vStartPos & vTargetPos are
	// in front of any one plane then there is no need to compute a route at all,
	// unless the optional test with the collision model returns false.

	CEntityBoundAI boundAI(*pVehicle, pPed->GetTransform().GetPosition().GetZf(), pPed->GetCapsuleInfo()->GetHalfWidth() );
	boundAI.GetCorners(vCorners);
	boundAI.GetPlanes(vNormals, fPlaneDists);

	const bool bUseBoundingBoxes = IsVehicleSuitableForLowDetailCollisionTest(pPed, vTargetPos, pVehicle, &boundAI);

	for(i=0; i<4; i++)
	{
		const float fStartDist = DotProduct(vNormals[i], vStartPos) + fPlaneDists[i];
		const float fTargetDist = DotProduct(vNormals[i], vTargetPos) + fPlaneDists[i];
		if(fStartDist > -fPlaneTolerance && fTargetDist > -fPlaneTolerance)
		{
			if(!bTestCollisionModel || !DoesRouteSegHitEntity(vStartPos, vTargetPosForLosTest, PED_HUMAN_RADIUS, pVehicle, bIsComplicatedBound, bUseBoundingBoxes, iDoorPedIsHeadingFor))
				return RouteRndEntityRet_StraightToTarget;

			// Create a simple route out to the edge of the bound & back
			ComputeRouteOutToEdgeOfBound(vStartPos, vTargetPos, boundAI, *pRoute, i, pVehicle, iDoorPedIsHeadingFor);
			return RouteRndEntityRet_OutToEdge;
		}
	}    

	//****************************************************************************************
	// Calculate the segment planes using proper radius.  If the the vStartPos & vTargetPos
	// are both in the same segment, then again there is no need to compute a route.

	Vector3 vSegmentNormals[4];
	float fSegmentPlaneDists[4];
	boundAI.GetSegmentPlanes(vSegmentNormals, fSegmentPlaneDists);

	int iStartHitSide = boundAI.ComputeHitSideByPosition(vStartPos);
	int iTargetHitSide = boundAI.ComputeHitSideByPosition(vTargetPos);

	if(iStartHitSide==iTargetHitSide && !bIsComplicatedBound)
	{
		if(!bTestCollisionModel || !DoesRouteSegHitEntity(vStartPos, vTargetPosForLosTest, PED_HUMAN_RADIUS, pVehicle, bIsComplicatedBound, bUseBoundingBoxes, iDoorPedIsHeadingFor))
			return RouteRndEntityRet_StraightToTarget;

		// Create a simple route out to the edge of the bound & back
		ComputeRouteOutToEdgeOfBound(vStartPos, vTargetPos, boundAI, *pRoute, iStartHitSide, pVehicle, iDoorPedIsHeadingFor);
		return RouteRndEntityRet_OutToEdge;
	}

	//***************************************************************************
	// Now calculate the corners & planes including the adjustment for the ped's
	// width - and continue with the algorithm proper

	boundAI.GetCorners(vCorners);
	boundAI.GetPlanes(vNormals, fPlaneDists);

//#if __BANK
//	boundAI.fwDebugDraw(Color_magenta);
//#endif

	//*********************************************************************
	// Intersect edge from start to target and see if there is a component 
	// lying inside or if the whole edge lies outside

	Vector3 vPos1;
	Vector3 vPos2;
	bool bLiesOutside=false;

	vPos1=vStartPos;
	vPos2=vTargetPos;
	Vector3 w=vPos2-vPos1;
	w.Normalize();
	Vector3 v=vPos1;

	for(i=0;i<4;i++)
	{
		const float fSide1 = DotProduct(vNormals[i],vPos1) + fPlaneDists[i];
		const int iSide1 = (fSide1 > fPlaneTolerance) ? 1 : (fSide1 < -fPlaneTolerance) ? -1 : 0;

		const float fSide2 = DotProduct(vNormals[i],vPos2) + fPlaneDists[i];
		const int iSide2 = (fSide2 > fPlaneTolerance) ? 1 : (fSide2 < -fPlaneTolerance) ? -1 : 0;

		if((-1!=iSide1)&&(-1!=iSide2))
		{
			//Either 
			//(i)  both points lie outside the plane.
			//(ii) one point lies on the plane and one lies outside the plane.
			//(iii)both points lie on the plane.   
			//The entire edge can be said to lie outside.
			bLiesOutside=true; 

			//Move points on plane just outside the plane 
			if(0==iSide1)
			{
				//Move vPos1 just onto the outside of the plane.   
				vPos1+=vNormals[i]*(-fSide1+fPlaneTolerance);
			}
			if(0==iSide2)
			{
				//Move vPos2 just onto the outside of the plane.,
				vPos2+=vNormals[i]*(-fSide2+fPlaneTolerance);
			}          
		}
		else if((iSide1==-1)&&(iSide2==1))
		{
			const float t =- (DotProduct(vNormals[i],v)+fPlaneDists[i]) / DotProduct(vNormals[i],w);
			vPos2=v+w*t;
		}
		else if((iSide1==1)&&(iSide2==-1))
		{
			const float t =- (DotProduct(vNormals[i],v)+fPlaneDists[i]) / DotProduct(vNormals[i],w);
			vPos1=v+w*t;
		}      
	}


	if(!bLiesOutside)
	{
		spdSphere sphere;
		boundAI.GetSphere(sphere);

		//Intersect the ray from source to target with bounding circle.
		Vector3 w=vTargetPos-vStartPos;
		w.z=0;
		if(w.Mag()==0)
		{
			return RouteRndEntityRet_StraightToTarget;
		}
		w.Normalize();
		Vector3 v=vStartPos;

		if(!fwGeom::IntersectSphereRay(sphere, v,w,vPos1,vPos2))
		{
			return RouteRndEntityRet_StraightToTarget;
		}
	}

	//********************************************************************
	// Find the crossed planes between source and target positions.
	// JB : Start or end points lying on a plane were resulting in the
	// path being marked as clear.  Quite often a ped will call this
	// function, and it's position will be slightly inside the bbox.
	// If within the epsilon (0.2f) the path will now still be blocked.

	int iPlane1=-1;
	int iPlane2=-1;
	v=vPos1;
	w=vPos2-vPos1;
	w.Normalize();

	for(i=0;i<4;i++)
	{
		const float ffSide1=DotProduct(vNormals[i],vPos1) + fPlaneDists[i];
		const float ffSide2=DotProduct(vNormals[i],vPos2) + fPlaneDists[i];

		if((ffSide1==0)&&(ffSide2==0))       
			continue;

		if((ffSide1>=0)&&(ffSide2<=0))
		{
			const float t =- (fPlaneDists[i] + DotProduct(vNormals[i],v)) / DotProduct(vNormals[i],w);
			vPos1=v+w*t;
			iPlane1=i;
		}
		else if((ffSide1<=0)&&(ffSide2>=0))
		{
			const float t =- (fPlaneDists[i] + DotProduct(vNormals[i],v)) / DotProduct(vNormals[i],w);
			vPos2=v+w*t;
			iPlane2=i;
		}
	}

	if(iPlane2<0 || iPlane1<0)
	{
		//There must be a clear path to the target.
		return RouteRndEntityRet_StraightToTarget;
	}

	//**************************************************************
	//	The "bTestCollisionModel" flag indicates that we expect this
	//	bound to be a vehicle which may have its doors open.

	CEntityBoundAI convexBound;
	Vector3 vConvexCorners[4];
	Vector3 vConvexNormals[4];
	float fConvexPlaneDs[4];

	if(bTestCollisionModel)
	{
		if(pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI)
		{
			eHierarchyId iIDsToUse[] = { VEH_CHASSIS };
			int iPartsToUse[1];
			iPartsToUse[0] = pVehicle->GetFragInst()->GetComponentFromBoneIndex( pVehicle->GetBoneIndex(iIDsToUse[0]) );

			convexBound.Set(*pVehicle, pPed->GetTransform().GetPosition().GetZf(), PED_HUMAN_RADIUS, false, NULL, 0, NULL, 1, iPartsToUse);
		}
		else
		{
			static const int NumHeliParts=10;
			int iComponentsToExclude[(MAX_DOOR_COMPONENTS*4)+NumHeliParts];	// NEED TO IGNORE THE DOOR WE ARE ACTUALLY HEADING FOR..
			static const int iDoorIDs[4] = { VEH_DOOR_DSIDE_F, VEH_DOOR_DSIDE_R, VEH_DOOR_PSIDE_F, VEH_DOOR_PSIDE_R };
			for(int d=0; d<4; d++)
			{
				int iIndex = d*MAX_DOOR_COMPONENTS;
				iComponentsToExclude[iIndex] = -1;
				iComponentsToExclude[iIndex+1] = -1;
				CCarEnterExit::GetDoorComponents(pVehicle, (eHierarchyId)iDoorIDs[d], &(iComponentsToExclude[iIndex]));
			}
			// Also exclude extra heli parts
			static const eHierarchyId iHeliPartIDs[NumHeliParts] =
			{
				VEH_MISC_A, VEH_MISC_B, VEH_MISC_C, VEH_MISC_D,
				VEH_MISC_E, VEH_EXTRA_1, HELI_ROTOR_MAIN, HELI_ROTOR_MAIN,
				VEH_WHEEL_LF, VEH_WHEEL_RF
			};
			int iNumComponents = MAX_DOOR_COMPONENTS*4;
			if(pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI)
			{
				for(int c=0; c<NumHeliParts; c++)
				{
					const int iComponent = pVehicle->GetFragInst()->GetComponentFromBoneIndex( pVehicle->GetBoneIndex(iHeliPartIDs[c]) );
					if(iComponent!=-1)
						iComponentsToExclude[iNumComponents++] = iComponent;
				}
			}

			convexBound.Set(*pVehicle, pPed->GetTransform().GetPosition().GetZf(), PED_HUMAN_RADIUS, false, NULL, iNumComponents, iComponentsToExclude);
		}

		
		convexBound.GetCorners(vConvexCorners);
		convexBound.GetPlanes(vConvexNormals, fConvexPlaneDs);

//#if __BANK
//		convexBound.fwDebugDraw(Color_yellow);
//#endif

		static const float fHeliPlaneTolerance = 0.3f;

		for(i=0; i<4; i++)
		{
			const float fStartDist = DotProduct(vConvexNormals[i], vStartPos) + fConvexPlaneDs[i];
			const float fTargetDist = DotProduct(vConvexNormals[i], vTargetPos) + fConvexPlaneDs[i];
			if(fStartDist > -fHeliPlaneTolerance && fTargetDist > -fHeliPlaneTolerance)
			{
				if(!bTestCollisionModel || !DoesRouteSegHitEntity(vStartPos, vTargetPosForLosTest, PED_HUMAN_RADIUS, pVehicle, bIsComplicatedBound, bUseBoundingBoxes, iDoorPedIsHeadingFor))
					return RouteRndEntityRet_StraightToTarget;

				// Create a simple route out to the edge of the bound & back
				ComputeRouteOutToEdgeOfBound(vStartPos, vTargetPos, boundAI, *pRoute, i, pVehicle, iDoorPedIsHeadingFor);
				return RouteRndEntityRet_OutToEdge;
			}
		}    

		// Re-initialise these w/o the doors included
		iStartHitSide = convexBound.ComputeHitSideByPosition(vStartPos);
		iTargetHitSide = convexBound.ComputeHitSideByPosition(vTargetPos);

		if(iStartHitSide==iTargetHitSide)
		{
			if(!DoesRouteSegHitEntity(vStartPos, vTargetPosForLosTest, PED_HUMAN_RADIUS, pVehicle, bIsComplicatedBound, bUseBoundingBoxes, iDoorPedIsHeadingFor))
				return RouteRndEntityRet_StraightToTarget;

			// Create a simple route out to the edge of the bound & back
			ComputeRouteOutToEdgeOfBound(vStartPos, vTargetPos, boundAI, *pRoute, iStartHitSide, pVehicle, iDoorPedIsHeadingFor);
			return RouteRndEntityRet_OutToEdge;
		}
	}

	CPedGrouteCalculator::ERouteRndEntityRet routeRet = CreateClockwiseOrAntiClockwiseRoute(
		pPed,
		pVehicle,
		&boundAI,
		vStartPos,
		vTargetPos,
		iStartHitSide,
		iTargetHitSide,
		pRoute,
		bTestCollisionModel,
		bIsComplicatedBound,
		bUseBoundingBoxes,
		iDoorPedIsHeadingFor,
		vCorners,
		vConvexCorners,
		iLosIncludeFlags,
		fCapsuleRadius,
		0
	);

	return routeRet;
}



//******************************************************************************************
//	ComputeRouteRoundEntityBoundingBox
//	Calculates the shortest route around an entity's bounding box.
//	This function has become very complicated & inefficient due to the need for testing for
//	car doors, which are attached collision & not individual entities and essentially makes
//	vehicles non-convex.
//	TODO: special-case routes around vehicles and revert this function to a simpler version.

CPedGrouteCalculator::ERouteRndEntityRet CPedGrouteCalculator::ComputeRouteRoundEntityBoundingBox(
	const CPed & ped,
	const Vector3 & vStartPos,
	CEntity & entity,
	const Vector3 & vTargetPos,
	const Vector3 & vTargetPosForLosTest,
	CPointRoute & route,
	const u32 iTestFlags,
	const int iForceDirection,
	const bool bTestCollisionModel,
	const int iCollisionComponentID)	// Optional hierarchy ID of what part of the entity we've collided with
{
	//******************************************************************************
	//	The "bTestCollisionModel" flag indicates that we expect this bound to be
	//	a vehicle which may have its doors open.  It means that where we may have
	//	normally been able to early-out (eg. when vStartPos & vTargetPos were both
	//	in the same segment of the bound), we should now verify this with a capsule
	//	test first.
	//	This flag will also cause a 2nd bound to be created excluding the vehicle
	//	doors, and where possible we will use these points instead of the larger
	//	bound which may have been calculated from a vehicle with opened doors.

	static const float fPlaneTolerance = 0.2f;
	static const float fCapsuleRadius = 0.15f;	// PED_HUMAN_RADIUS
	phIntersection intersection;

	const bool bIsComplicatedBound = (entity.GetIsTypeVehicle() && ((CVehicle*)&entity)->GetVehicleType()==VEHICLE_TYPE_HELI);
	static bool bUseBoundingBoxes = false;

	Assert(iForceDirection==DIR_UNSPECIFIED||iForceDirection==DIR_ANTICLOCKWISE||iForceDirection==DIR_CLOCKWISE);
    route.Clear();

    Vector3 corners[4];
    Vector3 normals[4];
    float ds[4];
    int i;
    
	//***************************************************************************
    // First off, get the corners & planes of the entity without adding on the
    // adjustment for the ped's width.  If both the vStartPos & vTargetPos are
    // in front of any one plane then there is no need to compute a route at all,
	// unless the optional test with the collision model returns false.

	Matrix34* pMatOverride = NULL;
	Matrix34 matDoorFullyOpen;
	if (entity.GetIsTypeObject())
	{
		CDoor* pDoor = ((CObject*)&entity)->IsADoor() ? (CDoor*)&entity : NULL;
		if (pDoor)
		{
			CTaskMove* pSimplestMoveTask = ped.GetPedIntelligence()->GetActiveSimplestMovementTask();
			CTaskMoveInterface* pTaskMove = pSimplestMoveTask ? pSimplestMoveTask->GetMoveInterface() : NULL;
			if (pTaskMove && pTaskMove->HasTarget())
			{
				Vector3 vGoToTarget = pSimplestMoveTask->GetTarget();
				Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
				Vector3 vMoveDir = vGoToTarget - vPedPosition;
				vMoveDir.Normalize();
				if (pDoor->GetDoorIsAtLimit(vPedPosition, vMoveDir))
				{
					if (pDoor->CalcOpenFullyOpen(matDoorFullyOpen))
						pMatOverride = &matDoorFullyOpen;
				}
			}
		}
	}

	CEntityBoundAI bound(entity,ped.GetTransform().GetPosition().GetZf(),ped.GetCapsuleInfo()->GetHalfWidth(), false, pMatOverride);
	bound.GetCorners(corners);
	bound.GetPlanes(normals,ds);

   	for(i=0; i<4; i++)
   	{
		const float fStartDist = DotProduct(normals[i], vStartPos) + ds[i];
		const float fTargetDist = DotProduct(normals[i], vTargetPos) + ds[i];
   		if(fStartDist > -fPlaneTolerance && fTargetDist > -fPlaneTolerance)
   		{
			if(!bTestCollisionModel || !DoesRouteSegHitEntity(vStartPos, vTargetPosForLosTest, PED_HUMAN_RADIUS, &entity, bIsComplicatedBound, bUseBoundingBoxes, VEH_INVALID_ID))
	   			return RouteRndEntityRet_StraightToTarget;

			// Create a simple route out to the edge of the bound & back
			ComputeRouteOutToEdgeOfBound(vStartPos, vTargetPos, bound, route, i, &entity);
			return RouteRndEntityRet_OutToEdge;
  		}
   	}    
   	
	//****************************************************************************************
   	// Calculate the segment planes using proper radius.  If the the vStartPos & vTargetPos
   	// are both in the same segment, then again there is no need to compute a route.
   	
    Vector3 seg_normals[4];
    float seg_ds[4];
	bound.GetSegmentPlanes(seg_normals,seg_ds);

	int iStartHitSide = bound.ComputeHitSideByPosition(vStartPos);
	int iTargetHitSide = bound.ComputeHitSideByPosition(vTargetPos);

	if(iStartHitSide==iTargetHitSide && !bIsComplicatedBound)
	{
		if(!bTestCollisionModel || !DoesRouteSegHitEntity(vStartPos, vTargetPosForLosTest, PED_HUMAN_RADIUS, &entity, bIsComplicatedBound, bUseBoundingBoxes, VEH_INVALID_ID))
			return RouteRndEntityRet_StraightToTarget;

		// Create a simple route out to the edge of the bound & back
		static dev_float fExtendOutAmount = 0.33f;
		ComputeRouteOutToEdgeOfBound(vStartPos, vTargetPos, bound, route, iStartHitSide, &entity, VEH_INVALID_ID, fExtendOutAmount);
		return RouteRndEntityRet_OutToEdge;
	}

	//***************************************************************************
	// Now calculate the corners & planes including the adjustment for the ped's
	// width - and continue with the algorithm proper

	bound.GetCorners(corners);
	bound.GetPlanes(normals,ds);

	//*********************************************************************
    // Intersect edge from start to target and see if there is a component 
    // lying inside or if the whole edge lies outside

    Vector3 vPos1;
    Vector3 vPos2;
    bool bLiesOutside=false;
	    
	vPos1=vStartPos;
	vPos2=vTargetPos;
	Vector3 w=vPos2-vPos1;
	w.Normalize();
	Vector3 v=vPos1;

	for(i=0;i<4;i++)
	{
	   const float fSide1 = DotProduct(normals[i],vPos1)+ds[i];
	   const int iSide1 = (fSide1 > fPlaneTolerance) ? 1 : (fSide1 < -fPlaneTolerance) ? -1 : 0;
	   
	   const float fSide2 = DotProduct(normals[i],vPos2)+ds[i];
	   const int iSide2 = (fSide2 > fPlaneTolerance) ? 1 : (fSide2 < -fPlaneTolerance) ? -1 : 0;
	   
	   if((-1!=iSide1)&&(-1!=iSide2))
	   {
		  //Either 
		  //(i)  both points lie outside the plane.
		  //(ii) one point lies on the plane and one lies outside the plane.
		  //(iii)both points lie on the plane.   
		  //The entire edge can be said to lie outside.
		  bLiesOutside=true; 
	      
		  //Move points on plane just outside the plane 
		  if(0==iSide1)
		  {
			  //Move vPos1 just onto the outside of the plane.   
			  vPos1+=normals[i]*(-fSide1+fPlaneTolerance);
		  }
		  if(0==iSide2)
		  {
			 //Move vPos2 just onto the outside of the plane.,
			 vPos2+=normals[i]*(-fSide2+fPlaneTolerance);
		  }          
	   }
	   else if((iSide1==-1)&&(iSide2==1))
	   {
		   const float t=-(DotProduct(normals[i],v)+ds[i])/DotProduct(normals[i],w);
		   vPos2=v+w*t;
	   }
	   else if((iSide1==1)&&(iSide2==-1))
	   {
		   const float t=-(DotProduct(normals[i],v)+ds[i])/DotProduct(normals[i],w);
		   vPos1=v+w*t;
	   }      
	}


	if(!bLiesOutside)
	{
	   spdSphere sphere;
	   bound.GetSphere(sphere);
	 
	   //Intersect the ray from source to target with bounding circle.
	   Vector3 w=vTargetPos-vStartPos;
	   w.z=0;
	   if(w.Mag()==0)
	   {
		   return RouteRndEntityRet_StraightToTarget;
	   }
	   w.Normalize();
	   Vector3 v=vStartPos;

	   if(!fwGeom::IntersectSphereRay(sphere, v,w,vPos1,vPos2))
	   {
		   return RouteRndEntityRet_StraightToTarget;
	   }
	}

	//********************************************************************
	// Find the crossed planes between source and target positions.
	// JB : Start or end points lying on a plane were resulting in the
	// path being marked as clear.  Quite often a ped will call this
	// function, and it's position will be slightly inside the bbox.
	// If within the epsilon (0.2f) the path will now still be blocked.

	int iPlane1=-1;
	int iPlane2=-1;
	v=vPos1;
	w=vPos2-vPos1;
	w.Normalize();

	for(i=0;i<4;i++)
	{
	   const float ffSide1=DotProduct(normals[i],vPos1)+ds[i];
	   const float ffSide2=DotProduct(normals[i],vPos2)+ds[i];
	   
	   if((ffSide1==0)&&(ffSide2==0))       
   		continue;
	   	       
	   if((ffSide1>=0)&&(ffSide2<=0))
	   {
		   const float t=-(ds[i]+DotProduct(normals[i],v))/DotProduct(normals[i],w);
		   vPos1=v+w*t;
		   iPlane1=i;
	   }
	   else if((ffSide1<=0)&&(ffSide2>=0))
	   {
		   const float t=-(ds[i]+DotProduct(normals[i],v))/DotProduct(normals[i],w);
		   vPos2=v+w*t;
		   iPlane2=i;
	   }
	}

	if((iPlane2<0)||(iPlane1<0))
	{
	   //There must be a clear path to the target.
	   return RouteRndEntityRet_StraightToTarget;
	}

	//**************************************************************
	//	The "bTestCollisionModel" flag indicates that we expect this
	//	bound to be a vehicle which may have its doors open.
	
	CEntityBoundAI convexBound;
	Vector3 vConvexCorners[4];
	Vector3 vConvexNormals[4];
	float fConvexPlaneDs[4];

	if(bTestCollisionModel)
	{
		Assert(entity.GetIsTypeVehicle());
		CVehicle * pVehicle = (CVehicle*)&entity;
		int iComponentsToExclude[(MAX_DOOR_COMPONENTS*4)+6];	// NEED TO IGNORE THE DOOR WE ARE ACTUALLY HEADING FOR..
		static const int iDoorIDs[4] = { VEH_DOOR_DSIDE_F, VEH_DOOR_DSIDE_R, VEH_DOOR_PSIDE_F, VEH_DOOR_PSIDE_R };
		for(int d=0; d<4; d++)
		{
			int iIndex = d*MAX_DOOR_COMPONENTS;
			iComponentsToExclude[iIndex] = -1;
			iComponentsToExclude[iIndex+1] = -1;
			CCarEnterExit::GetDoorComponents(pVehicle, (eHierarchyId)iDoorIDs[d], &(iComponentsToExclude[iIndex]));
		}
		// Also exclude extra heli parts
		static const eHierarchyId iHeliPartIDs[6] = { VEH_MISC_A, VEH_MISC_B, VEH_MISC_C, VEH_MISC_D, VEH_MISC_E, VEH_EXTRA_1 };
		int iNumComponents = MAX_DOOR_COMPONENTS*4;
		if(pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI)
		{
			for(int c=0; c<6; c++)
			{
				const int iComponent = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex(iHeliPartIDs[c]));
				if(iComponent!=-1)
					iComponentsToExclude[iNumComponents++] = iComponent;
			}
		}

		convexBound.Set(entity, ped.GetTransform().GetPosition().GetZf(), PED_HUMAN_RADIUS, false, NULL, iNumComponents, iComponentsToExclude);
		//convexBound.Set(entity, ped.GetPosition().z, PED_HUMAN_RADIUS, false, NULL, MAX_DOOR_COMPONENTS*4, iComponentsToExclude);
		convexBound.GetCorners(vConvexCorners);
		convexBound.GetPlanes(vConvexNormals, fConvexPlaneDs);

		for(i=0; i<4; i++)
		{
			const float fStartDist = DotProduct(vConvexNormals[i], vStartPos) + fConvexPlaneDs[i];
			const float fTargetDist = DotProduct(vConvexNormals[i], vTargetPos) + fConvexPlaneDs[i];
			if(fStartDist > -fPlaneTolerance && fTargetDist > -fPlaneTolerance)
			{
				if(!bTestCollisionModel || !DoesRouteSegHitEntity(vStartPos, vTargetPosForLosTest, PED_HUMAN_RADIUS, &entity, bIsComplicatedBound, bUseBoundingBoxes, VEH_INVALID_ID))
					return RouteRndEntityRet_StraightToTarget;

				// Create a simple route out to the edge of the bound & back
				ComputeRouteOutToEdgeOfBound(vStartPos, vTargetPos, bound, route, i, &entity);
				return RouteRndEntityRet_OutToEdge;
			}
		}    

		// Re-initialise these w/o the doors included
		iStartHitSide = convexBound.ComputeHitSideByPosition(vStartPos);
		iTargetHitSide = convexBound.ComputeHitSideByPosition(vTargetPos);

		if(iStartHitSide==iTargetHitSide)
		{
			if(!bIsComplicatedBound)
			{
				if(!DoesRouteSegHitEntity(vStartPos, vTargetPosForLosTest, PED_HUMAN_RADIUS, &entity, bIsComplicatedBound, bUseBoundingBoxes, VEH_INVALID_ID))
					return RouteRndEntityRet_StraightToTarget;

				// Create a simple route out to the edge of the bound & back
				ComputeRouteOutToEdgeOfBound(vStartPos, vTargetPos, bound /*convexBound*/, route, iStartHitSide, &entity);
				return RouteRndEntityRet_OutToEdge;
			}
			else
			{
				// Basically this is just for helis atm
				const int iDoorSide = (iCollisionComponentID==VEH_DOOR_PSIDE_F || iCollisionComponentID==VEH_DOOR_PSIDE_R)  ?
					CEntityBoundAI::RIGHT : (iCollisionComponentID==VEH_DOOR_DSIDE_F || iCollisionComponentID==VEH_DOOR_DSIDE_R) ?
						CEntityBoundAI::LEFT : -1;

				if(iDoorSide != -1)
				{
					// Create a simple route out to the edge of the bound & back, to step around the door
					ComputeRouteOutToEdgeOfBound(vStartPos, vTargetPos, bound /*convexBound*/, route, iDoorSide, &entity);
					return RouteRndEntityRet_OutToEdge;
				}
			}
		}
	}


	CPedGrouteCalculator::ERouteRndEntityRet routeRet = CreateClockwiseOrAntiClockwiseRoute(
		&ped,
		&entity,
		&bound,
		vStartPos,
		vTargetPos,
		iStartHitSide,
		iTargetHitSide,
		&route,
		bTestCollisionModel,
		bIsComplicatedBound,
		bUseBoundingBoxes,
		VEH_INVALID_ID,
		corners,
		vConvexCorners,
		iTestFlags,
		fCapsuleRadius,
		iForceDirection
	);

	// If this is a vehicle, see if the first segment of the route hits any doors.
	// If so then just return a route out to the edge of the vehicle to avoid the door.
	if(bTestCollisionModel && entity.GetType()==ENTITY_TYPE_VEHICLE && route.GetSize()>=2)
	{
		int iDoorComponents[MAX_DOOR_COMPONENTS*4];
		CVehicle * pVehicle = (CVehicle*)&entity;
		static const int iDoorIDs[4] = { VEH_DOOR_DSIDE_F, VEH_DOOR_DSIDE_R, VEH_DOOR_PSIDE_F, VEH_DOOR_PSIDE_R };
		for(int d=0; d<4; d++)
		{
			int iIndex = d*MAX_DOOR_COMPONENTS;
			iDoorComponents[iIndex] = -1;
			iDoorComponents[iIndex+1] = -1;
			CCarEnterExit::GetDoorComponents(pVehicle, (eHierarchyId)iDoorIDs[d], &(iDoorComponents[iIndex]));
		}
		phBound * pHitBound = CPedGeometryAnalyser::TestCapsuleAgainstComposite(&entity, route.Get(0), route.Get(1), PED_HUMAN_RADIUS, false, 0, NULL, MAX_DOOR_COMPONENTS*4, iDoorComponents);
		if(pHitBound)
		{
			route.Clear();

			// Create a simple route out to the edge of the bound & back
			static dev_float fExtendOutAmount = 0.33f;
			//ComputeRouteOutToEdgeOfBound(route.Get(0), route.Get(1), bound, route, iStartHitSide, &entity, VEH_INVALID_ID, fExtendOutAmount);
			ComputeRouteOutToEdgeOfBound(vStartPos, route.Get(1), bound, route, iStartHitSide, &entity, VEH_INVALID_ID, fExtendOutAmount);
			return RouteRndEntityRet_OutToEdge;
		}
	}

	return routeRet;
}














bool CPedGrouteCalculator::ComputeBestRouteRoundEntity(
	const Vector3 & vStartPos,
	const Vector3& vTargetPos,
	CEntity & entity,
	CPed & ped,
	CPointRoute & route,
	const u32 iTestFlags,
	const float fExpandWidth,
	const s32 iSubBoundIndexToIgnore)
{
	route.Clear();

	// FOrce task to use navmesh
	if( entity.GetIsTypeVehicle() && ((CVehicle*)&entity)->GetVehicleType() == VEHICLE_TYPE_BOAT )
	{
		return false;
	}

	Vector3 vCorners[4];
	Vector3 vNormals[4];
	float fDists[4];
	int i;

	CEntityBoundAI bound(entity, ped.GetTransform().GetPosition().GetZf(), fExpandWidth, false, NULL, iSubBoundIndexToIgnore);
	bound.GetCorners(vCorners);
	bound.GetPlanes(vNormals, fDists);

	//Intersect edge from start to target and see if there is a component 
	//lying inside or if the whole edge lies outside
	Vector3 vPos1;
	Vector3 vPos2;
	bool bLiesOutside=false;

	vPos1=vStartPos;
	vPos2=vTargetPos;
	Vector3 w=vPos2-vPos1;
	w.Normalize();
	Vector3 v=vPos1;
	const float fTolerance=0.2f;

	for(i=0;i<4;i++)
	{
		const float fSide1 = DotProduct(vNormals[i],vPos1)+fDists[i];
		const int iSide1 = fSide1 > fTolerance ? 1 : fSide1 < -fTolerance ? -1 : 0;

		const float fSide2 = DotProduct(vNormals[i],vPos2)+fDists[i];
		const int iSide2 = fSide2 > fTolerance ? 1 : fSide2 < -fTolerance ? -1 : 0; 

		if((-1!=iSide1)&&(-1!=iSide2))
		{
			//Either 
			//(i)  both points lie outside the plane.
			//(ii) one point lies on the plane and one lies outside the plane.
			//(iii)both points lie on the plane.   
			//The entire edge can be said to lie outside.
			bLiesOutside=true; 

			//Move points on plane just outside the plane 
			if(0==iSide1)
			{
				//Move vPos1 just onto the outside of the plane.   
				vPos1+=vNormals[i]*(-fSide1+fTolerance); // 0.01f
			}
			if(0==iSide2)
			{
				//Move vPos2 just onto the outside of the plane.,
				vPos2+=vNormals[i]*(-fSide2+fTolerance); // 0.01f
			}          
		}
		else if((iSide1==-1)&&(iSide2==1))
		{
			const float t=-(DotProduct(vNormals[i],v)+fDists[i])/DotProduct(vNormals[i],w);
			vPos2=v+w*t;
		}
		else if((iSide1==1)&&(iSide2==-1))
		{
			const float t=-(DotProduct(vNormals[i],v)+fDists[i])/DotProduct(vNormals[i],w);
			vPos1=v+w*t;
		}      
	}

	if(!bLiesOutside)
	{
		spdSphere sphere;
		bound.GetSphere(sphere);

		//Intersect the ray from source to target with bounding circle.
		Vector3 w=vTargetPos-vStartPos;
		w.z=0;
		if(w.Mag2()==0.0f)
		{
			route.Add(vStartPos);
			route.Add(vTargetPos);
			return CPedGeometryAnalyser::TestPointRouteLineOfSight(route, &ped, iTestFlags, &entity);
		}
		w.Normalize();
		Vector3 v=vStartPos;
		if(!fwGeom::IntersectSphereRay(sphere, v,w,vPos1,vPos2))
		{
			//There must be a clear path to the target.
			route.Add(vStartPos);
			route.Add(vTargetPos);
			return CPedGeometryAnalyser::TestPointRouteLineOfSight(route, &ped, iTestFlags, &entity);
		}
	}

	//Find the crossed planes between source and target positions.

	// JB : Start or end points lying on a plane were resulting in the
	// path being marked as clear.  Quite often a ped will call this
	// function, and it's position will be slightly inside the bbox.
	// If within the epsilon (0.2f) the path will now still be blocked.   
	int iPlane1=-1;
	int iPlane2=-1;
	v=vPos1;
	w=vPos2-vPos1;
	w.Normalize();

	for(i=0;i<4;i++)
	{
		const float ffSide1=DotProduct(vNormals[i],vPos1)+fDists[i];
		const float ffSide2=DotProduct(vNormals[i],vPos2)+fDists[i];

		if((ffSide1==0)&&(ffSide2==0))       
			continue;

		if((ffSide1>=0)&&(ffSide2<=0))
		{
			const float t=-(fDists[i]+DotProduct(vNormals[i],v))/DotProduct(vNormals[i],w);
			vPos1=v+w*t;
			iPlane1=i;
		}
		else if((ffSide1<=0)&&(ffSide2>=0))
		{
			const float t=-(fDists[i]+DotProduct(vNormals[i],v))/DotProduct(vNormals[i],w);
			vPos2=v+w*t;
			iPlane2=i;
		}
	}

	if((iPlane2<0)||(iPlane1<0))
	{
		//There must be a clear path to the target.
		route.Add(vStartPos);
		route.Add(vTargetPos);
		return CPedGeometryAnalyser::TestPointRouteLineOfSight(route, &ped, iTestFlags, &entity);
	}

	//Compute the target and start corners.
	const int iTargetCorner1=(iPlane2+4-1)%4;
	const int iTargetCorner2=(iPlane2+4-0)%4;
	const int iStartCorner1=(iPlane1+4-1)%4;
	const int iStartCorner2=(iPlane1+4-0)%4;

	//The two paths are 
	//(i)  start pos -> start corner 1 -> target corner 2 -> target pos
	//(ii) start pos -> start corner 2 -> target corner 1 -> target pos 

	CPointRoute routes[2];

	//*************************
	//	Compute path 1.
	//*************************

	routes[0].Add(vStartPos);
	routes[0].Add(vCorners[iStartCorner1]);

	int iStart1=iStartCorner1;
	while(iStart1!=iTargetCorner2)
	//while(iStart1!=iTargetCorner1)
	{
		iStart1=(iStart1+4-1)%4;
		routes[0].Add(vCorners[iStart1]);
	}
	routes[0].Add(vTargetPos);

	//*************************
	//	Compute path 2
	//*************************

	routes[1].Add(vStartPos);
	routes[1].Add(vCorners[iStartCorner2]);

	int iStart2=iStartCorner2;
	while(iStart2!=iTargetCorner1)
	//while(iStart2!=iTargetCorner2)
	{
		iStart2=(iStart2+4+1)%4;
		routes[1].Add(vCorners[iStart2]);
	}
	routes[1].Add(vTargetPos);

	//**********************************************************
	//	Now check each path for obstructions, starting with the
	//	shortest path.
	//**********************************************************

	int iPathOrdering[2];

	if(routes[0].GetLength() <= routes[1].GetLength())
	{
		iPathOrdering[0] = 0;
		iPathOrdering[1] = 1;
	}
	else
	{
		iPathOrdering[0] = 1;
		iPathOrdering[1] = 0;
	}

	for(i=0; i<2; i++)
	{
		int iPath = iPathOrdering[i];

		if(CPedGeometryAnalyser::TestPointRouteLineOfSight(routes[iPath], &ped, iTestFlags, &entity))
		{
			route.From(routes[iPath]);
			return true;
		}
	}

	// None of the routes are clear
	return false;
}




//************************************************************************************************
//	ComputeRouteRoundSphere.
//	Given a sphere and start/target points, this function calculates a detour point P which will
//	allow a linesegment from start->P->target to completely avoid the sphere.
//	If no detour was actually necessary (eg. the target is in front of the sphere, or the ray
//	from the start to the target doesn't intersect the sphere) then the function returns 0.
//	Otherwise it returns -1 if the detour target was to the left of the sphere, or 1 to the right.
//************************************************************************************************

int CPedGrouteCalculator::ComputeRouteRoundSphere(
	const Vector3 & vPedPos,
	const spdSphere & sphere,
	const Vector3 & vStartPoint,
	const Vector3 & vTarget,
	Vector3 & vNewTarget,
	Vector3 & vDetourTarget,
	const int iSideToPrefer)
{
    Vector3 v1;
    Vector3 v2;

 	const Vector3& v=vPedPos;
    Vector3 w;

    vNewTarget=vTarget;

    //Test if the target is in the sphere.
    //If the target is in the sphere then move the target.
	if(sphere.ContainsPoint(RCC_VEC3V(vTarget)))
    {
        w=vTarget-vStartPoint;
        w.Normalize();
        if(fwGeom::IntersectSphereRay(sphere, v,w,v1,v2))
        {
            vNewTarget=v2;
        }
    }

    //Test if the ped can see the target.
    w=vNewTarget-v;
    w.Normalize();

	if(!fwGeom::IntersectSphereRay(sphere, vNewTarget,w,v1,v2))
    {
        vDetourTarget=vNewTarget;
        return 0;
    }

    //Check if the distance to the target is less than the distance to the 1st intersection point
    //If so then there's no need to compute a route
    Vector3 vVecToTarget = vNewTarget - v;
    Vector3 vVecToFirstIntersection = v1 - v;
    
    if(vVecToTarget.Mag2() < vVecToFirstIntersection.Mag2())
    {
    	vDetourTarget=vNewTarget;
		//phSphere.Release(false);
    	return 0;
   	}
      
    //Ped can't see target.  Compute a detour point.
	if(fwGeom::IntersectSphereRay(sphere, v,w,v1,v2))
    {
        //Find closest point on ray to centre of sphere.
		// [SPHERE-OPTIMISE]
        Vector3 vDiff;
        vDiff=VEC3V_TO_VECTOR3(sphere.GetCenter())-v;
        const float t=DotProduct(vDiff,w);
        Vector3 vPos=w;
        vPos*=t;
        vPos+=v;
        
        //Compute vector from sphere centre to point on ray 
        //and find point on sphere along vector from sphere centre.
        vDiff=vPos-VEC3V_TO_VECTOR3(sphere.GetCenter());
        vDiff.Normalize();

		if(!vDiff.Mag2())
		{
			//Assertf(0, "ComputeRouteRoundSphere - this should never happen.\n");
			//vDiff = ped.GetA();
			// return 0;
			vDiff = CrossProduct(vVecToTarget, Vector3(0.0f,0.0f,1.0f));
			vDiff.Normalize();
		}

		// Find which side we detoured to, left or right.
		Vector3 vCross = CrossProduct(vDiff, w);
		int iAvoidSide;
		if(vCross.z > 0.0f)
			iAvoidSide = 1;
		else
			iAvoidSide = -1;

		// If we have a 'iSideToPrefer' specified, and it differs from our calculated side - then flip the vector
		if(iSideToPrefer && iSideToPrefer != iAvoidSide)
		{
			vDiff *= -1.0f;
			iAvoidSide = -iAvoidSide;
		}

        vDiff*=sphere.GetRadiusf(); 
        vDetourTarget=VEC3V_TO_VECTOR3(sphere.GetCenter())+vDiff;
		return iAvoidSide;
    }
  
    return 0;
}

//*********************************************************************************************************
//*********************************************************************************************************
//
//	PED GEOMETRY ANALYSER
//
//*********************************************************************************************************
//*********************************************************************************************************

dev_float CPedGeometryAnalyser::ms_MaxPedWaterDepthForVisibility = 5.0f;
const float CPedGeometryAnalyser::ms_fClearTargetSearchDistance=5.0f;
float CPedGeometryAnalyser::ms_fWldCallbackPedHeight = 0.0f;
bool CPedGeometryAnalyser::ms_bWldCallbackLosStatus = true;
CEntity * CPedGeometryAnalyser::ms_pEntityPointRouteIsAround = NULL;

int CPedGeometryAnalyser::ms_iNumCollisionPoints;
Vector3 CPedGeometryAnalyser::ms_vecCollisionPoints[CPedGeometryAnalyser::MAX_NUM_COLLISION_POINTS];
u32 CPedGeometryAnalyser::m_nAsyncProbeTimeout = 0;
CCanTargetCache CPedGeometryAnalyser::ms_CanTargetCache;
bool CPedGeometryAnalyser::ms_bProcessCanPedTargetPedAsynchronously = true;
float CPedGeometryAnalyser::ms_fCanTargetCacheMaxPosChangeSqr = 0.5f * 0.5f;
dev_float CPedGeometryAnalyser::ms_fExtendedCombatRange = 150.0f;
dev_u32 CPedGeometryAnalyser::ms_uTimeToUseExtendedCombatRange = 5000;

#if !__FINAL
bool CPedGeometryAnalyser::ms_bDisplayAILosInfo = false;
bool CPedGeometryAnalyser::ms_bDebugCanPedTargetPed = false;
int CPedGeometryAnalyser::ms_iNumLineTestsSavedLastFrame = 0;
int CPedGeometryAnalyser::ms_iNumAsyncLineTestsRejectedLastFrame = 0;
int CPedGeometryAnalyser::ms_iNumCacheFullEventsLastFrame = 0;
int CPedGeometryAnalyser::ms_iNumLineTestsNotProcessedInTime = 0;
int CPedGeometryAnalyser::ms_iNumLineTestsNotAsync = 0;
int CPedGeometryAnalyser::ms_iTotalNumCanPedTargetPointCallsLastFrame = 0;
int CPedGeometryAnalyser::ms_iTotalNumCanPedTargetPedCallsLastFrame = 0;
#endif

void CPedGeometryAnalyser::Init(unsigned /*initMode*/)
{
	
}

void CPedGeometryAnalyser::Shutdown(unsigned /*shutdownMode*/)
{
	ms_CanTargetCache.Clear();
}

bool CPedGeometryAnalyser::GetIsLineOfSightClear(const Vector3& vStart, const Vector3& vTarget, CEntity& entity)
{	
	Assert(entity.GetIsPhysical());
	if(!entity.GetIsPhysical())
	{
		return true;
	}
	else
	{
		phSegment segment;
		segment.Set(vStart,vTarget);
		phSegment segmentLocal;
		Matrix34 m = MAT34V_TO_MATRIX34(entity.GetMatrix());
		segmentLocal.Localize(segment,m);

		const CPhysical& physical=(const CPhysical&)entity;

		if(physical.GetCurrentPhysicsInst())
		{
			//Physical's collision has been streamed in.
			const phBound* pBound=physical.GetCurrentPhysicsInst()->GetArchetype()->GetBound();

			// return !pBound->TestEdge(segmentLocal); DEPRECATED
			phShapeTest<phShapeEdge> shapeTest;
			shapeTest.InitEdge(segmentLocal);
			return !shapeTest.TestOneObject(*pBound);
		}
		else
		{
			//Physical's collision hasn't been streamed in yet but we can get the bounding box extents.
			//Make an obb out of the extents and test the segment against obb.

			CBaseModelInfo* pModelInfo=entity.GetBaseModelInfo();
			const Vector3& bbMax=pModelInfo->GetBoundingBoxMax();
			const Vector3& bbMin=pModelInfo->GetBoundingBoxMin();

			Vector3 normals[6];
			float ds[6];

			normals[0]=Vector3(-1,0,0);
			normals[1]=Vector3(1,0,0);
			normals[2]=Vector3(0,-1,0);
			normals[3]=Vector3(0,1,0);
			normals[4]=Vector3(0,0,-1);
			normals[5]=Vector3(0,0,1);

			ds[0]=bbMin.x;//DotProduct(bbMin,normals[0]);
			ds[1]=-bbMax.x;//DotProduct(bbMax,normals[1]);
			ds[2]=bbMin.y;//DotProduct(bbMin,normals[2]);
			ds[3]=-bbMax.y;//DotProduct(bbMax,normals[3]);
			ds[4]=bbMin.z;//DotProduct(bbMin,normals[4]);
			ds[5]=-bbMax.z;//DotProduct(bbMax,normals[5]);

			float fIntersectionLength=0;
			return CEntityBoundAI::ComputeDistanceToIntersection(segmentLocal.A,segmentLocal.B,normals,ds,6,fIntersectionLength);
		}
	}
}


//Compute if the lineseg intersects any of the entities surrounding the ped
bool
CPedGeometryAnalyser::GetIsLineOfSightClearOfNearbyEntities(const CPed * pPed, const Vector3& vStart, const Vector3& vEnd, bool bVehicles, bool bObjects, bool bPeds)
{
	Assert(bVehicles || bObjects || bPeds);
	CPedIntelligence * pPedAI = pPed->GetPedIntelligence();
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	const CEntity * pEntity;

	if(bVehicles)
	{
		const CEntityScannerIterator vehicleList = pPedAI->GetNearbyVehicles();
		pEntity = vehicleList.GetFirst();
		while(pEntity)
		{
			if(pEntity->IsInPathServer())
			{
				Assert(pEntity->GetIsTypeVehicle());

				CEntityBoundAI bound(*pEntity, vPedPos.z, PED_HUMAN_RADIUS);
				if(!bound.TestLineOfSight(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd)))
					return false;
			}
			pEntity = vehicleList.GetNext();
		}
	}
	if(bObjects)
	{
		const CEntityScannerIterator objectList = pPedAI->GetNearbyObjects();
		pEntity = objectList.GetFirst();
		while(pEntity)
		{
			if(pEntity->IsInPathServer())
			{
				Assert(pEntity->GetIsTypeObject());

				CEntityBoundAI bound(*pEntity, vPedPos.z, PED_HUMAN_RADIUS);
				if(!bound.TestLineOfSight(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd)))
					return false;
			}
			pEntity = objectList.GetNext();
		}
	}
	if(bPeds)
	{
		const CEntityScannerIterator pedList = pPedAI->GetNearbyPeds();
		pEntity = pedList.GetFirst();
		while(pEntity)
		{
			Assert(pEntity->GetIsTypePed());

			CEntityBoundAI bound(*pEntity, vPedPos.z, PED_HUMAN_RADIUS);
			if(!bound.TestLineOfSight(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd)))
				return false;

			pEntity = pedList.GetNext();
		}
	}
	return true;
}

phBound* CPedGeometryAnalyser::TestCapsuleAgainstComposite(
	CEntity* pEntity, const Vector3& vecStart, const Vector3& vecEnd, const float fRadius, const bool bJustTestBoundingBoxes,
	const int iNumPartsToIgnore, int * ppPartsToIgnore, const int iNumPartsToTest, int * ppPartsToTest)
{
	PF_START(TestCapsuleVsComposite);

	phInst* pInst = pEntity->GetFragInst();
	if(pInst==NULL)
		pInst = pEntity->GetCurrentPhysicsInst();

	phBound* pBound = pInst->GetArchetype()->GetBound();
	Matrix34 currMat = RCC_MATRIX34(pInst->GetMatrix());

	phBoundBox boundBox;
	boundBox.Release(false);

	phBound * pHitSubBound = TestCapsuleAgainstCompositeR(pInst, pBound, &currMat, vecStart, vecEnd, fRadius, bJustTestBoundingBoxes, &boundBox, iNumPartsToIgnore, ppPartsToIgnore, iNumPartsToTest, ppPartsToTest);

	PF_STOP(TestCapsuleVsComposite);

	return pHitSubBound;
}

phBound* CPedGeometryAnalyser::TestCapsuleAgainstCompositeR(
	phInst *pInst, phBound* pBound,
	const Matrix34* pCurrMat, const Vector3& vecStart, const Vector3& vecEnd,
	const float fRadius, const bool bJustTestBoundingBoxes, phBoundBox * pBoundBox,
	const int iNumPartsToIgnore, int * ppPartsToIgnore,
	const int iNumPartsToTest, int * ppPartsToTest)
{
#if __BANK
	static bool bDebugMe = false;
#endif

	// recurse with any composite bounds
	if (pBound->GetType() == phBound::COMPOSITE)
	{
		phBoundComposite* pBoundComposite = (phBoundComposite*)pBound;
		for (s32 i=0; i<pBoundComposite->GetNumBounds(); i++)
		{
			if(iNumPartsToIgnore)
			{
				int p;
				for(p=0; p<iNumPartsToIgnore; p++)
					if(ppPartsToIgnore[p]==i)
						break;
				if(p!=iNumPartsToIgnore)
					continue;
			}
			else if(iNumPartsToTest)
			{
				int p;
				for(p=0; p<iNumPartsToTest; p++)
					if(ppPartsToTest[p]==i)
						break;
				if(p==iNumPartsToTest)
					continue;
			}

			// check this bound is still valid (smashable code may have removed it)
			phBound* pChildBound = pBoundComposite->GetBound(i);
			if (pChildBound)
			{
				// calc the new matrix
				Matrix34 newMat = *pCurrMat;
				Matrix34 thisMat = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(i));
				newMat.DotFromLeft(thisMat);	

				if (pChildBound->GetType() == phBound::COMPOSITE)
				{
					// recurse if necessary
					phBound * pHitSubBound = TestCapsuleAgainstCompositeR(pInst, pChildBound, &newMat, vecStart, vecEnd, fRadius, bJustTestBoundingBoxes, pBoundBox);
					if(pHitSubBound)
						return pHitSubBound;
				}
				else
				{
					Matrix34 wldToPartMat;
					wldToPartMat.FastInverse(newMat);
					Vector3 vLocalStart, vLocalEnd;
					wldToPartMat.Transform(vecStart, vLocalStart);
					wldToPartMat.Transform(vecEnd, vLocalEnd);
					const Vector3 vecAxis = vLocalEnd-vLocalStart;
					const float fCapsuleLength = vecAxis.Mag();

					bool bHit;

					phShapeTest<phShapeCapsule> shapeTest;
					const phSegment segment(vLocalStart, vLocalStart + vecAxis * fCapsuleLength);
					shapeTest.InitCapsule(segment,fRadius);

					if(bJustTestBoundingBoxes)
					{
						// test the bound against the capsule
						Vector3 vBBoxSize = VEC3V_TO_VECTOR3(pChildBound->GetBoundingBoxSize());
						pBoundBox->SetBoxSize(RCC_VEC3V(vBBoxSize));
						pBoundBox->SetCentroidOffset(pChildBound->GetCentroidOffset());

						// bHit = pBoundBox->TestCapsule(vLocalStart, vecAxis, fRadius, fCapsuleLength); DEPRECATED
						bHit = shapeTest.TestOneObject(*pBoundBox) != 0;
					}
					else
					{
						// bHit = pChildBound->TestCapsule(vLocalStart, vecAxis, fRadius, fCapsuleLength); DEPRECATED
						bHit = shapeTest.TestOneObject(*pChildBound) != 0;
					}
#if __BANK
					// render the bound
					if(bDebugMe)
					{
						const Vector3 & vMin = VEC3V_TO_VECTOR3(pChildBound->GetBoundingBoxMin());
						const Vector3 & vMax = VEC3V_TO_VECTOR3(pChildBound->GetBoundingBoxMax());
						CDebugScene::Draw3DBoundingBox(vMin, vMax, newMat, bHit ? Color_red : Color_green);
					}
#endif
					if(bHit)
						return pChildBound;
				}
			}
		}
	}

	return NULL;
}

bool CPedGeometryAnalyser::TestPedCapsule(const CPed* pPed, const Matrix34* pMatrix, const CEntity** pExceptions, int nNumExceptions,
										  u32 exceptionOptions, u32 includeFlags, u32 typeFlags, u32 excludeTypeFlags, 
										  CEntity** pFirstHitEntity, s32* pFirstHitComponent,
										  float fCapsuleRadiusMultipier, float fBoundHeading, float fBoundPitch, const Vector3& vBoundOffset, float fStartZOffset)
{
	Assert(pMatrix);
	if(!pMatrix)
		return false;

	//Get biped information back from the ped
	const CBaseCapsuleInfo* pBaseCapsuleInfo = pPed->GetCapsuleInfo();
	if (!pBaseCapsuleInfo)
		return false;

	float fPedCapsuleRadius = pBaseCapsuleInfo->GetHalfWidth();

	// Maybe we always want to do this due to dynamic capsule size
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
		fPedCapsuleRadius = pPed->GetCurrentMainMoverCapsuleRadius();

	fPedCapsuleRadius *= fCapsuleRadiusMultipier;

	phSegment segment;
	phIntersection intersect;

	Matrix34 xTransform = *pMatrix;
	xTransform.RotateLocalZ(fBoundHeading);
	xTransform.RotateLocalX(fBoundPitch);
	xTransform.Translate(vBoundOffset);

	Vector3 vStart = xTransform.d;
	Vector3 vEnd = vStart;

	const CBipedCapsuleInfo *pBipedCapsuleInfo = pBaseCapsuleInfo->GetBipedCapsuleInfo();
	if (pBipedCapsuleInfo)
	{
		vStart += xTransform.c * ((pBipedCapsuleInfo->GetHeadHeight() - fPedCapsuleRadius) + fStartZOffset);
		vEnd += xTransform.c * (pBipedCapsuleInfo->GetCapsuleZOffset() + fPedCapsuleRadius);
	}
	else
	{
		const CQuadrupedCapsuleInfo *pQuadrupedCapsuleInfo = pBaseCapsuleInfo->GetQuadrupedCapsuleInfo();
		if (pQuadrupedCapsuleInfo)
		{
			float fPedHalfCapsuleLength = pQuadrupedCapsuleInfo->GetMainBoundLength() * 0.5f;
			vStart += xTransform.b * fPedHalfCapsuleLength;
			vEnd -= xTransform.b * fPedHalfCapsuleLength;
		}
		else
		{
			const CFishCapsuleInfo *pFishCapsuleInfo = pBaseCapsuleInfo->GetFishCapsuleInfo();
			if (pFishCapsuleInfo)
			{
				float fPedHalfCapsuleLength = pFishCapsuleInfo->GetMainBoundLength() * 0.5f;
				vStart += xTransform.b * fPedHalfCapsuleLength;
				vEnd -= xTransform.b * fPedHalfCapsuleLength;
			}
			return false;
		}
	}

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	capsuleDesc.SetCapsule(vStart, vEnd, fPedCapsuleRadius);
	capsuleDesc.SetIsDirected(false);
	capsuleDesc.SetDoInitialSphereCheck(false);
	capsuleDesc.SetIncludeFlags(includeFlags);
	capsuleDesc.SetExcludeTypeFlags(excludeTypeFlags);
	capsuleDesc.SetTypeFlags(typeFlags);
	capsuleDesc.SetExcludeEntities(pExceptions, nNumExceptions, exceptionOptions);
	
	WorldProbe::CShapeTestFixedResults<> capsuleResult;
	capsuleDesc.SetResultsStructure(&capsuleResult);

	// So... If we are looking for objects but not pickups we need to make sure that we don't get false positive from pickups
	bool bDidCollide = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
	if (bDidCollide)
	{
		bDidCollide = TestIfCollisionTypeIsValid(pPed, includeFlags, capsuleResult);
		if (pFirstHitEntity && capsuleResult.GetResultsReady())
		{
			*pFirstHitEntity = CPhysics::GetEntityFromInst(capsuleResult[0].GetHitInst());
			if (pFirstHitComponent)
			{
				*pFirstHitComponent = capsuleResult[0].GetHitComponent();
			}
		}
	}

	return bDidCollide;
}


//Tests the point route for a clear line-of-sight against the world
bool
CPedGeometryAnalyser::TestPointRouteLineOfSight_PerEntityCallback(CEntity * pEntity, void * pData)
{
	// Can optionally ignore the entity we are walking around
	if(pEntity == ms_pEntityPointRouteIsAround)
		return true;

	if(pEntity->GetIsTypeObject())
	{
		CBaseModelInfo * pModelInfo = pEntity->GetBaseModelInfo();
		if(pModelInfo->GetModelType()==MI_TYPE_WEAPON)
			return true;
	}

	CPointRoute * pRoute = (CPointRoute*)pData;

	int p;
	for(p=1; p<pRoute->GetSize(); p++)
	{
		CEntityBoundAI bound(*pEntity, ms_fWldCallbackPedHeight, PED_HUMAN_RADIUS);
		if(!bound.TestLineOfSight(VECTOR3_TO_VEC3V(pRoute->Get(p)), VECTOR3_TO_VEC3V(pRoute->Get(p-1))))
		{
			ms_bWldCallbackLosStatus = false;
			return false;
		}
	}
	return true;
}

//**************************************************************************************
//	TestPointRouteLineOfSight
//	Tests whether a point route is free of obstructions.  This calls the physics
//	level's intersection functions for the world geometry, but uses the CEntityBoundAI
//	class for all intersection with vehicles/objects/peds..
//	Optionally the 'pRouteEntity' pointer can be supplied, which will prevent that
//	entity from being checked for LOS intersections.
//**************************************************************************************
bool
CPedGeometryAnalyser::TestPointRouteLineOfSight(CPointRoute & pointRoute, CPed * pPed, u32 iTestFlags, CEntity * pRouteEntity)
{
	int p;

	if((iTestFlags & ArchetypeFlags::GTA_ALL_MAP_TYPES)!=0)
	{
		for(p=1; p<pointRoute.GetSize(); p++)
		{
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(pointRoute.Get(p-1), pointRoute.Get(p));
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeDesc.SetExcludeEntity(pRouteEntity);
			probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
				return false;
		}
	}

	ms_bWldCallbackLosStatus = true;
	ms_pEntityPointRouteIsAround = pRouteEntity;

	static u32 iAllFlags = ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_PED_TYPE;

	if((iTestFlags & iAllFlags)!=0)
	{
		ms_fWldCallbackPedHeight = pPed->GetTransform().GetPosition().GetZf();

		Vector3 vMin, vMax;
		pointRoute.GetMinMax(vMin, vMax);
		// Expand min/max a bit to approximate ped's size & height..
		vMin.x -= PED_HUMAN_RADIUS;
		vMin.y -= PED_HUMAN_RADIUS;
		vMin.z -= 1.5f;
		vMax.x += PED_HUMAN_RADIUS;
		vMax.y += PED_HUMAN_RADIUS;
		vMax.z += 1.5f;
		spdAABB box(RCC_VEC3V(vMin), RCC_VEC3V(vMax));

		s32 typeFlags = 0;

		if ((iTestFlags & ArchetypeFlags::GTA_VEHICLE_TYPE)!=0) { typeFlags |= ENTITY_TYPE_MASK_VEHICLE; }
		if ((iTestFlags & ArchetypeFlags::GTA_PED_TYPE)!=0) { typeFlags |= ENTITY_TYPE_MASK_PED; }
		if ((iTestFlags & ArchetypeFlags::GTA_OBJECT_TYPE)!=0) { typeFlags |= ENTITY_TYPE_MASK_OBJECT; }

		fwIsBoxIntersectingApprox searchBox(box);
		CGameWorld::ForAllEntitiesIntersecting(
			&searchBox,
			TestPointRouteLineOfSight_PerEntityCallback,
			&pointRoute,
			typeFlags, SEARCH_LOCATION_EXTERIORS,
			SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_PEDS
		);  // hmm, might want to check inside too though?
	}

	return ms_bWldCallbackLosStatus;
}

void CPedGeometryAnalyser::ComputeMoveDirToAvoidEntity(const CPed& ped, CEntity& entity, Vector3& vMoveDir)
{
  //Compute the corners and normals of the vehicle.
    Vector3 normals[4];
    float ds[4];

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	CEntityBoundAI bound(entity,vPedPosition.z,ped.GetCapsuleInfo()->GetHalfWidth());
	bound.GetPlanes(normals,ds);
    
    //Find the direction the ped has to move in order to avoid the car  
    //If the ped is nearest the rhs(lhs) then move further right(left)
	const float fLeftDist=DotProduct(normals[CEntityBoundAI::LEFT],vPedPosition)+ds[CEntityBoundAI::LEFT];
    const float fRightDist=DotProduct(normals[CEntityBoundAI::RIGHT],vPedPosition)+ds[CEntityBoundAI::RIGHT];
    if(fLeftDist>0)
    {
        vMoveDir=normals[CEntityBoundAI::LEFT];
    }
    else if(fRightDist>0)
    {
        vMoveDir=normals[CEntityBoundAI::RIGHT];
    }
    else
    {
        if(fLeftDist>fRightDist)
        {
            vMoveDir=normals[CEntityBoundAI::LEFT];
        }
        else
        {
            vMoveDir=normals[CEntityBoundAI::RIGHT];
        }
    }
}

//Test whether the ped's movement is brushing up against the side of an entity & does not constitute a collision/potential-collision
bool
CPedGeometryAnalyser::IsBrushingContact(const CPed& ped, CEntity& entity, Vector3& vNormalisedMoveDir)
{
	static const float fCloseDist = 0.2f;
	static const float fBrushingAngleEps = 0.087f;	// 5 degs
	Vector3 vPlaneNormals[4];
	float fPlaneDists[4];
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	CEntityBoundAI boundAI(entity, vPedPos.z, PED_HUMAN_RADIUS);

	int iHitSide = boundAI.ComputeHitSideByPosition(vPedPos);

	boundAI.GetPlanes(vPlaneNormals, fPlaneDists);
	float fDist = DotProduct(vPedPos, vPlaneNormals[iHitSide]) + fPlaneDists[iHitSide];

	// Ped is too far away from closest edge for there to be any contact
	if(fDist > fCloseDist)
		return false;

	float fDot = DotProduct(vNormalisedMoveDir, vPlaneNormals[iHitSide]);
	if(fDot > -fBrushingAngleEps)
		return true;

	return false;
}

// JB : the contact normal returned for collisions between peds and buildings is not the surface
// normal of the geometry we've hit!  It is the vector from the collision point, to the center of
// the collision sphere which has been hit - usually the bottom one (colSphere[0]).  Therefore for
// low walls this may have a higher z component than expected.

bool CPedGeometryAnalyser::CanPedJumpObstacle(const CPed& ped, const CEntity& UNUSED_PARAM(entity), const Vector3& vContactNormal, const Vector3& UNUSED_PARAM(vContactPos))
{
	Vector3 vecWaistPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	Vector3 vecTestAheadPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetB());	// peds should always be vertical

    //Always want to be able to jump out of swimming pools
	if (PGTAMATERIALMGR->GetMtlVfxGroup(ped.GetPackedGroundMaterialId())==VFXGROUP_LIQUID_WATER)
	{
		return true;
    }
 
	// JB
	if(vContactNormal.z > 0.17f)	// 10deg upwards
	{
		// if collision is nearly flat, we shouldn't need to jump this
		if(vContactNormal.z > 0.9f)
			return false;

		MUST_FIX_THIS(sandy - collision models have changed with rage);

		const float fPedRadius = ped.GetCapsuleInfo()->GetHalfWidth();
		
		// try and work out the exact height of the collision
		vecWaistPos.z += (-0.2f - fPedRadius*vContactNormal.z);
		
		float fHorizontalNormalMag = vContactNormal.XYMag();
		// and for collisions with relatively flat planes, extend the line check
		if(vContactNormal.z > 0.5f)
		{
			vecTestAheadPos.x = -vContactNormal.x;
			vecTestAheadPos.y = -vContactNormal.y;
			vecTestAheadPos.z = 0.0f;
			vecTestAheadPos /= fHorizontalNormalMag;
			vecTestAheadPos += vecTestAheadPos*fHorizontalNormalMag*fPedRadius;
			vecTestAheadPos *= MIN(4.0f, 2.0f/fHorizontalNormalMag);
		}
		else
		{
			// move collision test forward by where the collision occured in x,y plane
			vecTestAheadPos += vecTestAheadPos*fHorizontalNormalMag*fPedRadius;
		}
	}
	else
	{
		if(!CPedGroups::IsInPlayersGroup(&ped))
		{
			// if colliding against a nearly vertical wall, use a slightly lower height
			vecWaistPos.z -= 0.15f;
		}
	}
	
	//Do a test from the ped's waist to a metre in front of the ped at waist height.
	INC_PEDAI_LOS_COUNTER;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vecWaistPos, vecWaistPos+vecTestAheadPos);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);

	if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		//Make sure the ped isn't going to throw themselves off a cliff	
        //Compute the ground height after the jump.
        Vector3 vLandTestPos = vecWaistPos + (vecTestAheadPos * 3.0f);
	    bool bGroundFound=false;
		const float fSecondSurfaceInterp=0.0f;

		const float fGroundZ=WorldProbe::FindGroundZFor3DCoord(fSecondSurfaceInterp,vLandTestPos.x,vLandTestPos.y,vLandTestPos.z,&bGroundFound);

		const float fDiff=vLandTestPos.z-fGroundZ;
    	
    	//If ground has been found and it isn't too far down then jump.
    	if(bGroundFound && (fDiff<3.0f))
    	{
    	    return true;
        }
	}

	return false;
}

bool CPedGeometryAnalyser::CanPedJumpObstacle(const CPed& ped, const CEntity& UNUSED_PARAM(entity))
{
    const Vector3 v1=VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	Vector3 v2=v1+VEC3V_TO_VECTOR3(ped.GetTransform().GetB());
    INC_PEDAI_LOS_COUNTER;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(v1, v2);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	return (!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc));
}



void CCanTargetCacheEntry::Clear()
{
	m_pFromPed = m_pToPed = NULL;
	m_hShapeTestHandle.Reset();
	m_iTimeIssuedMs = 0;
	m_bLosExists = false;
	m_bSuccesfulResultQueried = false;
}

//******************************************************************************
//	RetrieveAsyncResults
//	Goes through the array of TCanTargetQuery's and retrieves the Los results
//	from the CWorldProbeAsync system.  These results will then be used when
//	CanPedSeePed() and similar functions are called.

void CPedGeometryAnalyser::Process()
{
	ms_CanTargetCache.ClearExpiredEntries();
	ms_CanTargetCache.GetLosResults();

#if !__FINAL
	PF_SET(Number_LOS_from_AI, gs_iNumWorldProcessLineOfSightUnCached);
	PF_SET(Number_CanPedTargetPed_calls, ms_iTotalNumCanPedTargetPedCallsLastFrame);
	PF_SET(Number_CanPedTargetPoint_calls, ms_iTotalNumCanPedTargetPointCallsLastFrame);
	PF_SET(Num_LOS_saved_in_async_cache, ms_iNumLineTestsSavedLastFrame);
	PF_SET(Num_async_probes_rejected, ms_iNumAsyncLineTestsRejectedLastFrame);
	PF_SET(Num_Cache_full_events, ms_iNumCacheFullEventsLastFrame);
	PF_SET(Num_LOS_not_processed_in_time, ms_iNumLineTestsNotProcessedInTime);
	PF_SET(Num_LOS_not_async, ms_iNumLineTestsNotAsync);
#endif //!__FINAL

#if DEBUG_DRAW

	//********************************************************************************
	//	Debug stuff

	if(ms_bDebugCanPedTargetPed)
	{
		ms_CanTargetCache.Debug();
	}

	if(ms_bDisplayAILosInfo)
	{
		grcDebugDraw::AddDebugOutput(" ");
		grcDebugDraw::AddDebugOutput("PedGeometryAnalyser - LOS stats from last frame");
		grcDebugDraw::AddDebugOutput(" ");
		grcDebugDraw::AddDebugOutput("Number LOS from AI : %i", gs_iNumWorldProcessLineOfSightUnCached);
		grcDebugDraw::AddDebugOutput("Number CanPedTargetPed() calls : %i", ms_iTotalNumCanPedTargetPedCallsLastFrame);
		grcDebugDraw::AddDebugOutput("Number CanPedTargetPoint() calls : %i", ms_iTotalNumCanPedTargetPointCallsLastFrame);
		grcDebugDraw::AddDebugOutput("CanPedTargetPed() - num LOS saved via async : %i", ms_iNumLineTestsSavedLastFrame);
		grcDebugDraw::AddDebugOutput("CanPedTargetPed() - num LOS unable to async : %i", ms_iNumAsyncLineTestsRejectedLastFrame);
		grcDebugDraw::AddDebugOutput("CanPedTargetPed() - num LOS not processed in time : %i", ms_iNumLineTestsNotProcessedInTime);
		grcDebugDraw::AddDebugOutput("CPedAcquaintanceScanner - num LOS *not* async : %i", ms_iNumLineTestsNotAsync);
		grcDebugDraw::AddDebugOutput(" ");
		grcDebugDraw::AddDebugOutput(" ");
	}

	ms_iNumLineTestsSavedLastFrame = 0;
	ms_iNumAsyncLineTestsRejectedLastFrame = 0;
	ms_iNumCacheFullEventsLastFrame = 0;
	ms_iTotalNumCanPedTargetPointCallsLastFrame = 0;
	ms_iTotalNumCanPedTargetPedCallsLastFrame = 0;
	ms_iNumLineTestsNotProcessedInTime = 0;
	gs_iNumWorldProcessLineOfSightUnCached = 0;
	ms_iNumLineTestsNotAsync = 0;

	//
	//********************************************************************************
#endif
}


void CCanTargetCacheEntry::Init(CPed * pFromPed, CPed * pToPed)
{
	Assert(pFromPed && pToPed);

	m_pFromPed = pFromPed;
	m_pToPed = pToPed;

	m_iTimeIssuedMs = fwTimer::GetTimeInMilliseconds();
	m_vFromPoint = VEC3V_TO_VECTOR3(pFromPed->GetTransform().GetPosition());
	m_vToPoint = VEC3V_TO_VECTOR3(pToPed->GetTransform().GetPosition());
	m_vBlockedPosition.Zero();

	m_hShapeTestHandle.Reset();
}

CCanTargetCache::CCanTargetCache()
{

}

// Remove old Los results which are out of date.
// TODO: store the from/to targetting positions & check against them (requires calculating these up-front above)
// as a way of ensuring more accurate results.
void CCanTargetCache::ClearExpiredEntries()
{
	u32 iCurrTime = fwTimer::GetTimeInMilliseconds();
	for(int r=0; r<CAN_TARGET_CACHE_SIZE; r++)
	{
		if(m_Cache[r].m_pFromPed && m_Cache[r].m_pToPed)
		{
			u32 iDeltaMs = iCurrTime - m_Cache[r].m_iTimeIssuedMs;
			if(iDeltaMs > CAN_TARGET_CACHE_TIMEOUT)
			{
				m_Cache[r].Clear();
			}
		}
		else
		{
			m_Cache[r].Clear();
		}
	}
}

void CCanTargetCache::Clear()
{
	for(int r=0; r<CAN_TARGET_CACHE_SIZE; r++)
	{
		m_Cache[r].Clear();
	}
}

void CCanTargetCache::GetLosResults()
{
	// Handle obtaining the result from the CWorldProbeAsync system
	for(int r=0; r<CAN_TARGET_CACHE_SIZE; r++)
	{
		if(m_Cache[r].m_hShapeTestHandle.GetResultsWaitingOrReady())
		{
			bool bReady = m_Cache[r].m_hShapeTestHandle.GetResultsReady();

// #if __ASSERT
// 			static bool DEBUG_RENDER_RENDER = false;
// 			static bool DEBUG_ASYNC_LOS = false;
// 			static bool ADD_FAILED_PROBE_TO_MEASURING_TOOL = false;
// 			if( DEBUG_ASYNC_LOS && (( eLosResult==CWorldProbeAsync::EClear ) || ( eLosResult==CWorldProbeAsync::EBlocked ) ) &&
// 				m_Cache[r].m_pFromPed && m_Cache[r].m_pToPed)
// 			{
// 				if(DEBUG_RENDER_RENDER)
// 				{
// 					grcDebugDraw::Line(m_Cache[r].m_pFromPed->GetPosition(), m_Cache[r].m_vFromPoint, Color_red, Color_red);
// 					grcDebugDraw::Line(m_Cache[r].m_vFromPoint, m_Cache[r].m_vToPoint, Color_orange, Color_orange);
// 					grcDebugDraw::Line(m_Cache[r].m_vToPoint, m_Cache[r].m_pToPed->GetPosition(), Color_green, Color_green);
// 				}
// 				CEntity* pException = NULL;
// 				if( m_Cache[r].m_pToPed->m_pMyVehicle && m_Cache[r].m_pToPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
// 				{
// 					pException = m_Cache[r].m_pToPed->m_pMyVehicle;
// 				}
// 				else if( m_Cache[r].m_pFromPed->m_pMyVehicle && m_Cache[r].m_pFromPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
// 				{
// 					pException = m_Cache[r].m_pFromPed->m_pMyVehicle;
// 				}
// 				const int nFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
// 				const int nOptions = WorldProbe::LOS_IGNORE_NO_COLLISION | WorldProbe::LOS_IGNORE_SEE_THRU | WorldProbe::LOS_IGNORE_SHOOT_THRU;
// 				bool bhasLos = WorldProbe::GetIsLineOfSightClear(m_Cache[r].m_vFromPoint, m_Cache[r].m_vToPoint, pException, nFlags, nOptions, WorldProbe::LOS_GeneralAI);
// 				if( (( !bhasLos && eLosResult==CWorldProbeAsync::EClear ) || ( bhasLos && eLosResult==CWorldProbeAsync::EBlocked ) ) )
// 				{
// 					if( ADD_FAILED_PROBE_TO_MEASURING_TOOL )
// 					{
// 						CPhysics::g_vClickedPos[0] = m_Cache[r].m_vFromPoint;
// 						CPhysics::g_vClickedNormal[0] = ZAXIS;
// 						CPhysics::g_pPointers[0] = NULL;
// 						CPhysics::g_bHasPosition[0] = true;
// 						sprintf(CPhysics::ms_Pos1, "%.2f, %.2f, %.2f", CPhysics::g_vClickedPos[0].x, CPhysics::g_vClickedPos[0].y, CPhysics::g_vClickedPos[0].z);
// 						sprintf(CPhysics::ms_Ptr1, "%p", CPhysics::g_pPointers[0]);
// 						sprintf(CPhysics::ms_Normal1, "%.2f, %.2f, %.2f", CPhysics::g_vClickedNormal[0].x, CPhysics::g_vClickedNormal[0].y, CPhysics::g_vClickedNormal[0].z);
// 
// 						CPhysics::g_vClickedPos[1] = m_Cache[r].m_vToPoint;
// 						CPhysics::g_vClickedNormal[1] = ZAXIS;
// 						CPhysics::g_pPointers[1] = NULL;
// 						CPhysics::g_bHasPosition[1] = true;
// 						sprintf(CPhysics::ms_Pos2, "%.2f, %.2f, %.2f", CPhysics::g_vClickedPos[1].x, CPhysics::g_vClickedPos[1].y, CPhysics::g_vClickedPos[1].z);
// 						sprintf(CPhysics::ms_Ptr2, "%p", CPhysics::g_pPointers[1]);
// 						sprintf(CPhysics::ms_Normal2, "%.2f, %.2f, %.2f", CPhysics::g_vClickedNormal[1].x, CPhysics::g_vClickedNormal[1].y, CPhysics::g_vClickedNormal[1].z);
// 					}
// 				}
// 			}
// #endif

			if(bReady)
			{
				bool bBlocked = m_Cache[r].m_hShapeTestHandle.GetNumHits() > 0u;

				if(bBlocked)
				{
					m_Cache[r].m_vBlockedPosition = m_Cache[r].m_hShapeTestHandle[0].GetHitPosition();
				}

				m_Cache[r].m_bLosExists = !bBlocked;
				m_Cache[r].m_hShapeTestHandle.Reset();
			}
		}
	}
}

#if DEBUG_DRAW
void CCanTargetCache::Debug()
{
	grcDebugDraw::AddDebugOutput("CCanTargetCache cache contents.");

	for(int r=0; r<CAN_TARGET_CACHE_SIZE; r++)
	{
		if(!m_Cache[r].m_pFromPed || !m_Cache[r].m_pToPed)
		{
			grcDebugDraw::AddDebugOutput("--------");
		}
		else
		{
			grcDebugDraw::AddDebugOutput("%i)  h:0x%x (Ped1:0x%p -> Ped2:0x%p) AgeMs:%i Los:%s",
				r,
				&m_Cache[r].m_hShapeTestHandle,
				m_Cache[r].m_pFromPed.Get(),
				m_Cache[r].m_pToPed.Get(),
				fwTimer::GetTimeInMilliseconds() - m_Cache[r].m_iTimeIssuedMs,
				m_Cache[r].m_bLosExists ? "Clear" : "Blocked"
			);
		}
	}
}
#endif

ECanTargetResult CCanTargetCache::GetCachedResult(const CPed * pTargeter, const CPed * pTarget, CCanTargetCacheEntry ** pNextFreeCacheEntry, const float fMaxPosDeltaSqr, const u32 iMaxTimeDeltaMs, Vector3 * pProbeStartPosition, Vector3 * pIntersectionIfLosBlocked, bool bReturnOldestIfFull)
{
	Assert(pNextFreeCacheEntry || !bReturnOldestIfFull);	// No point saying that we want the oldest one to be returned if there is no place to store it.

	const u32 iCurrTime = fwTimer::GetTimeInMilliseconds();
	CCanTargetCacheEntry * pFirstFree = NULL;
	CCanTargetCacheEntry * pOldestInUse = NULL;
	u32 iOldestTimeMs = 0;

	for(int q=0; q<CAN_TARGET_CACHE_SIZE; q++)
	{
		//********************************************************************************************
		// If we have an entry in the cache which has already been processed then use those results.
		// These results persists for a short amount of time (2 secs) so will still be reasonably valid.
		// TODO : store the precise start & end targeting positions & see if these are still within some delta.

		if(m_Cache[q].m_pFromPed == pTargeter && m_Cache[q].m_pToPed == pTarget)
		{
			// Is there a valid asynchronous shape test associated with this cache entry?
			if(!m_Cache[q].m_hShapeTestHandle.GetResultsWaitingOrReady())
			{
				// There is no asynchronous shape test in progress for this combination of peds if we get in here.

				const Vector3 vFromPedPos = VEC3V_TO_VECTOR3(pTargeter->GetTransform().GetPosition());
				const Vector3 vToPedPos = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());
				const Vector3 vFromPosDelta = (vFromPedPos - m_Cache[q].m_vFromPoint);
				const Vector3 vToPosDelta = (vToPedPos - m_Cache[q].m_vToPoint);

				//********************************************************************************************
				// IF we have found the entry for these peds but the positions have changed too much to use
				// or too much time has passed for us to use this slot, then Clear out the slot, maybe set
				// this as the pFirstFreeSlot, and then break out of this loop.

				const bool bPosDeltaExceeded = (vFromPosDelta.Mag2() > fMaxPosDeltaSqr) || (vToPosDelta.Mag2() > fMaxPosDeltaSqr);
				const u32 iDeltaTime = iCurrTime - m_Cache[q].m_iTimeIssuedMs;

				if( (iDeltaTime < iMaxTimeDeltaMs && !bPosDeltaExceeded ) || !m_Cache[q].m_bSuccesfulResultQueried)
				{
					const ECanTargetResult canTarget = (m_Cache[q].m_bLosExists) ? ECanTarget : ECanNotTarget;

					// Optionally store the position of the intersection, if the Los was blocked
					if(!m_Cache[q].m_bLosExists && pIntersectionIfLosBlocked)
						*pIntersectionIfLosBlocked = m_Cache[q].m_vBlockedPosition;

					// Optionally store the original start position
					if(pProbeStartPosition)
						*pProbeStartPosition = m_Cache[q].m_vFromPoint;

					#if !__FINAL
					CPedGeometryAnalyser::ms_iNumLineTestsSavedLastFrame++;
					#endif

					m_Cache[q].m_bSuccesfulResultQueried = true;

					// If the positional delta was exceeded for this query, then force it to be re-requested.
					if(bPosDeltaExceeded)
						m_Cache[q].m_iTimeIssuedMs = 0;

// 					static bool DEBUG_ASYNC_LOS = false;
// 					static bool ADD_FAILED_PROBE_TO_MEASURING_TOOL = false;
// 					if( m_Cache[q].m_bLosExists && DEBUG_ASYNC_LOS )
// 					{
// 						CEntity* pException = NULL;
// 						if( pTarget->m_pMyVehicle && pTarget->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
// 						{
// 							pException = pTarget->m_pMyVehicle;
// 						}
// 						else if( pTargeter->m_pMyVehicle && pTargeter->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
// 						{
// 							pException = pTargeter->m_pMyVehicle;
// 						}
// 						const int nFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
// 						const int nOptions = WorldProbe::LOS_IGNORE_NO_COLLISION | WorldProbe::LOS_IGNORE_SEE_THRU | WorldProbe::LOS_IGNORE_SHOOT_THRU;
// 						if( !WorldProbe::GetIsLineOfSightClear(m_Cache[q].m_vFromPoint, m_Cache[q].m_vToPoint, pException, nFlags, nOptions, WorldProbe::LOS_GeneralAI) )
// 						{
// 							if( ADD_FAILED_PROBE_TO_MEASURING_TOOL )
// 							{
// 								CPhysics::g_vClickedPos[0] = m_Cache[q].m_vFromPoint;
// 								CPhysics::g_vClickedNormal[0] = ZAXIS;
// 								CPhysics::g_pPointers[0] = NULL;
// 								CPhysics::g_bHasPosition[0] = true;
// 								sprintf(CPhysics::ms_Pos1, "%.2f, %.2f, %.2f", CPhysics::g_vClickedPos[0].x, CPhysics::g_vClickedPos[0].y, CPhysics::g_vClickedPos[0].z);
// 								sprintf(CPhysics::ms_Ptr1, "%p", CPhysics::g_pPointers[0]);
// 								sprintf(CPhysics::ms_Normal1, "%.2f, %.2f, %.2f", CPhysics::g_vClickedNormal[0].x, CPhysics::g_vClickedNormal[0].y, CPhysics::g_vClickedNormal[0].z);
// 
// 								CPhysics::g_vClickedPos[1] = m_Cache[q].m_vToPoint;
// 								CPhysics::g_vClickedNormal[1] = ZAXIS;
// 								CPhysics::g_pPointers[1] = NULL;
// 								CPhysics::g_bHasPosition[1] = true;
// 								sprintf(CPhysics::ms_Pos2, "%.2f, %.2f, %.2f", CPhysics::g_vClickedPos[1].x, CPhysics::g_vClickedPos[1].y, CPhysics::g_vClickedPos[1].z);
// 								sprintf(CPhysics::ms_Ptr2, "%p", CPhysics::g_pPointers[1]);
// 								sprintf(CPhysics::ms_Normal2, "%.2f, %.2f, %.2f", CPhysics::g_vClickedNormal[1].x, CPhysics::g_vClickedNormal[1].y, CPhysics::g_vClickedNormal[1].z);
// 							}
// 						}
// 					}
// 					else if( !m_Cache[q].m_bLosExists && DEBUG_ASYNC_LOS )
// 					{
// 						CEntity* pException = NULL;
// 						if( pTarget->m_pMyVehicle && pTarget->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
// 						{
// 							pException = pTarget->m_pMyVehicle;
// 						}
// 						else if( pTargeter->m_pMyVehicle && pTargeter->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
// 						{
// 							pException = pTargeter->m_pMyVehicle;
// 						}
// 						const int nFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
// 						const int nOptions = WorldProbe::LOS_IGNORE_NO_COLLISION | WorldProbe::LOS_IGNORE_SEE_THRU | WorldProbe::LOS_IGNORE_SHOOT_THRU;
// 						Assert(!WorldProbe::GetIsLineOfSightClear(m_Cache[q].m_vFromPoint, m_Cache[q].m_vToPoint, pException, nFlags, nOptions, WorldProbe::LOS_GeneralAI));
// 						if( WorldProbe::GetIsLineOfSightClear(m_Cache[q].m_vFromPoint, m_Cache[q].m_vToPoint, pException, nFlags, nOptions, WorldProbe::LOS_GeneralAI) )
// 						{
// 							if( ADD_FAILED_PROBE_TO_MEASURING_TOOL )
// 							{
// 								CPhysics::g_vClickedPos[0] = m_Cache[q].m_vFromPoint;
// 								CPhysics::g_vClickedNormal[0] = ZAXIS;
// 								CPhysics::g_pPointers[0] = NULL;
// 								CPhysics::g_bHasPosition[0] = true;
// 								sprintf(CPhysics::ms_Pos1, "%.2f, %.2f, %.2f", CPhysics::g_vClickedPos[0].x, CPhysics::g_vClickedPos[0].y, CPhysics::g_vClickedPos[0].z);
// 								sprintf(CPhysics::ms_Ptr1, "%p", CPhysics::g_pPointers[0]);
// 								sprintf(CPhysics::ms_Normal1, "%.2f, %.2f, %.2f", CPhysics::g_vClickedNormal[0].x, CPhysics::g_vClickedNormal[0].y, CPhysics::g_vClickedNormal[0].z);
// 
// 								CPhysics::g_vClickedPos[1] = m_Cache[q].m_vToPoint;
// 								CPhysics::g_vClickedNormal[1] = ZAXIS;
// 								CPhysics::g_pPointers[1] = NULL;
// 								CPhysics::g_bHasPosition[1] = true;
// 								sprintf(CPhysics::ms_Pos2, "%.2f, %.2f, %.2f", CPhysics::g_vClickedPos[1].x, CPhysics::g_vClickedPos[1].y, CPhysics::g_vClickedPos[1].z);
// 								sprintf(CPhysics::ms_Ptr2, "%p", CPhysics::g_pPointers[1]);
// 								sprintf(CPhysics::ms_Normal2, "%.2f, %.2f, %.2f", CPhysics::g_vClickedNormal[1].x, CPhysics::g_vClickedNormal[1].y, CPhysics::g_vClickedNormal[1].z);
// 							}
// 						}
// 					}

					return canTarget;
				}
				else
				{
					// We've decided that this slot is no longer valid either because it is too old or because the peds involved have
					// moved too far since it was first created. We will return ENotSureYet below.
					m_Cache[q].Clear();

					if(!pFirstFree)
						pFirstFree = &m_Cache[q];
					break;
				}
			}
			else
			{
				//**********************************************************************************************
				// If we have an entry for these two peds, which still has a m_hShapeTestHandle then this means
				// the request is there but has not yet been processed.  We do not need to add it again.

				return ENotSureYet;
			}
		}
		// Take a note of the first empty slot we find.
		if(!pFirstFree)
		{
			if(!m_Cache[q].m_pFromPed && !m_Cache[q].m_pToPed)
			{
				pFirstFree = &m_Cache[q];
			}
			else if(bReturnOldestIfFull)
			{
				// In this case, we haven't yet found a free one, and we are considering recycling the oldest
				// entry if we end up being full. Below, we check if this entry is older than anything we've
				// found so far.
				u32 iDeltaMs = iCurrTime - m_Cache[q].m_iTimeIssuedMs;
				if(iDeltaMs > iOldestTimeMs)
				{
					// If we are not waiting for an asynchronous probe to finish, and if we have retrieved the probe,
					// results, keep track of it as the oldest one so far.
					if(m_Cache[q].m_bSuccesfulResultQueried && !m_Cache[q].m_hShapeTestHandle.GetResultsWaitingOrReady())
					{
						pOldestInUse = &m_Cache[q];
						iOldestTimeMs = iDeltaMs;
					}
				}
			}
		}
	}

	if(pNextFreeCacheEntry)
	{
		// If we didn't find a free entry but found an old one to recycle (which should
		// imply that bReturnOldestIfFull was set), clear out that entry and return that
		// as if it had been free.
		if(!pFirstFree && pOldestInUse)
		{
			pOldestInUse->Clear();
			pFirstFree = pOldestInUse;
		}

		*pNextFreeCacheEntry = pFirstFree;
	}

	return ENotSureYet;
}

// PURPOSE: Clear a specific cached result so the targeting system doesn't rely on this old result
void CCanTargetCache::ClearCachedResult(const CPed* pTargeter, const CPed* pTarget)
{
	for(int q = 0; q < CAN_TARGET_CACHE_SIZE; q++)
	{
		if(m_Cache[q].m_pFromPed == pTargeter && m_Cache[q].m_pToPed == pTarget)
		{
			m_Cache[q].Clear();
			break;
		}
	}
}

// PURPOSE: If we have been waiting too long on an asynchronous probe, kill it.
void CCanTargetCache::CancelAsyncProbe(const CPed* pTargeter, const CPed* pTarget)
{
	// Find the corresponding cache entry.
	for(int q = 0; q < CAN_TARGET_CACHE_SIZE; q++)
	{
		if(m_Cache[q].m_pFromPed == pTargeter && m_Cache[q].m_pToPed == pTarget)
		{
			// Abort the asynchronous probe related to this shape test handle.
			m_Cache[q].m_hShapeTestHandle.Reset();
			break;
		}
	}
}

dev_float TARGET_VEHICLE_DISTANCE = 80.0f;
dev_float TARGET_IN_HELI_MULTIPLIER = 2.0f;
#define MAX_TARGETTING_INTERSECTIONS 16

// This function will take a ped, a target position and our current position to test from and adjust the test position
// based on cover information.
void CPedGeometryAnalyser::AdjustPositionForCover(const CPed& ped, const Vector3& vTarget, Vector3& vPositionToAdjust)
{
	if(!ped.GetCoverPoint())
	{
		return;
	}

	Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	Vector3 vCoverPos;
	Vector3 vVantagePos;
	Vector3 vToTarget = vTarget - vPedPosition;
	vToTarget.z = 0.0f;
	vToTarget.Normalize();
	if(CCover::FindCoordinatesCoverPoint(ped.GetCoverPoint(), &ped, vToTarget, vCoverPos, &vVantagePos))
	{
		CTask* pCoverTask = ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER);

		if (pCoverTask  ||
			( ( ( vPedPosition - vVantagePos ).XYMag2() < rage::square(2.0f)) && 
			(!ped.GetMotionData()->GetIsStill() || ped.GetPedResetFlag(CPED_RESET_FLAG_IsEnteringCover)) ) )
		{
			if( pCoverTask )
			{
				bool bIsFacingLeft = static_cast<CTaskInCover*>(pCoverTask)->IsFacingLeft();

				bool bCloseToEdge = false;
				if(ped.IsLocalPlayer())
				{
					CTaskCover *pTaskCover = static_cast<CTaskCover*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
					if(pTaskCover && pTaskCover->IsCoverFlagSet(CTaskCover::CF_CloseToPossibleCorner))
					{
						bCloseToEdge = true;
					}
				}
				
				// Add on any offset, keeping the z value always the same as the vantage pos
				vCoverPos.z = vVantagePos.z;
				Vector3 vCoverToVantage = vVantagePos - vCoverPos;
				Vector3 vCoverDir = VEC3V_TO_VECTOR3(ped.GetCoverPoint()->GetCoverDirectionVector(&RCC_VEC3V(vToTarget)));
				if( vCoverToVantage.Mag2() > rage::square(0.1f) )
				{
					float fDot;
					float fModifier = CTaskInCover::GetCoverOutModifier( bIsFacingLeft, vCoverDir, vToTarget, fDot, true, false, bCloseToEdge );
					vVantagePos = vCoverPos + (vCoverToVantage*fModifier);
				}
			}

			vVantagePos += ped.GetLocalOffsetToCoverPoint();

			//				float fOriginalZ = vPositionToAdjust.z;
			vPositionToAdjust = vVantagePos;// + Vector3(0.0f,0.0f,1.75f);
			//				vPositionToAdjust.z = fOriginalZ;
		}
	}
}

// If collision exist, check with this function if it is a valid one
bool CPedGeometryAnalyser::TestIfCollisionTypeIsValid(const CPed* pPed, u32 includeFlags, const WorldProbe::CShapeTestResults& results)
{
	bool bIsValid = true;

	// Test if we collided only with pickups when we did not want to
	// - i.e. filter out the pickup results if testing against object type only
	if ((includeFlags & (ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_PICKUP_TYPE)) == ArchetypeFlags::GTA_OBJECT_TYPE)
	{
		for (int i = 0; i < results.GetNumHits(); ++i)
		{
			bIsValid = false;

			// Not sure if we should allow null here or not, maybe just assert on null?
			const CEntity* pEntity = CPhysics::GetEntityFromInst(results[i].GetHitInst());
			if (!pEntity || !pEntity->GetIsTypeObject())
			{
				// some collisions are ignored in MP (eg players in passive mode or concealed vehicles)				
				if (pPed && pEntity && NetworkInterface::AreInteractionsDisabledInMP(*pPed, *pEntity))
				{
					continue;
				}
				else
				{
					bIsValid = true;
					break;
				}
			}

			const CObject* pObject = (CObject*)pEntity;
			//assert(dynamic_cast<CObject>(pEntity));
			if (!pObject->m_nObjectFlags.bIsPickUp)
			{
				bIsValid = true;
				break;
			}

			// Only return false if all of the collision results have hit pickups
		}
	}

	// Add possibly more tests here
    if (bIsValid && pPed->GetIsInVehicle())
	{
		for (int i = 0; i < results.GetNumHits(); ++i)
		{
			const CEntity* pEntity = CPhysics::GetEntityFromInst(results[i].GetHitInst());
			if(pEntity && pEntity->GetIsTypeVehicle())
			{
				const CVehicle* pHitVeh = static_cast<const CVehicle*>(pEntity);
				if (pHitVeh && pHitVeh == pPed->GetMyVehicle())
				{
					bIsValid = false;

					eHierarchyId eDoorId = CVehicle::GetDoorHierachyIdFromImpact(*pPed->GetMyVehicle(), results[i].GetHitComponent());
					if (eDoorId == VEH_INVALID_ID)
					{
						bIsValid = true;
						break;
					}
				}
			}
		}
	}

	return bIsValid;
}

dev_float DISTANCE_SPOTTED_IN_UNKNOWN_VEHICLE = 20.0f;
dev_float DISTANCE_SPOTTED_IN_BUSH = 20.0f;

bool CPedGeometryAnalyser::CanPedTargetPoint(const CPed& ped, const Vector3& vTarget, s32 iTargetOptions, const CEntity** const ppExcludeEnts, const int nNumExcludeEnts, CCanTargetCacheEntry * pCacheEntry, Vector3 * pProbeStartPosition, Vector3* pvIntersectionPosition, const CPed *pTargeteePed)
{
#if !__FINAL
	ms_iTotalNumCanPedTargetPointCallsLastFrame++;
#endif

    //Test that the target is near and in front of the ped.
    const Vector3 vPedDir=VEC3V_TO_VECTOR3(ped.GetTransform().GetB());
    Vector3 vDiff;

	//Test if there is a clear line of sight from the ped's eye to the target.
	Vector3 vEye(ped.GetBonePositionCached(BONETAG_HEAD));

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());

	if( iTargetOptions & TargetOption_UseMeleeTargetPosition )
	{
		vEye = ped.GetBonePositionCached(BONETAG_SPINE1);
		Vector3 vTargetGroundPos = ped.GetGroundPos();

		//! If we have a valid ground, move position closer to the floor, so we don't target over small objects.
		if(ped.GetIsStanding() && vTargetGroundPos.z > PED_GROUNDPOS_RESET_Z)
		{
			vEye.z = vTargetGroundPos.z + ((vEye.z - vTargetGroundPos.z) * 0.75f);
		}
	}
	else
	{
		// Take the X and Y components from the peds position, to ensure its safely in the peds capsule and out of collision
		vEye.x = vPedPosition.x;
		vEye.y = vPedPosition.y;
		
		bool bOverrideEye = false;
		// If the players aim or follow cam is active, use that for the targeting position
		if( ped.IsLocalPlayer()	&& ((iTargetOptions & TargetOption_UseCameraPosIfOnScreen) FPS_MODE_SUPPORTED_ONLY(|| camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() ) ) )
		{
			if( FPS_MODE_SUPPORTED_ONLY(camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() ||)
				camInterface::GetGameplayDirector().IsThirdPersonAiming() || 
				camInterface::GetGameplayDirector().IsFollowingPed() ||
				(camInterface::GetGameplayDirector().IsFollowingVehicle() && ped.GetMyVehicle() && camInterface::GetGameplayDirector().GetFollowVehicle() == ped.GetMyVehicle()))
			{
				vEye = camInterface::GetGameplayDirector().GetFrame().GetPosition();
				bOverrideEye = true;
			}
		}

		// If the target is in cover, work out the vantage location for the purposes of a LOS test
		if( ped.GetCoverPoint() && !bOverrideEye)
		{
			AdjustPositionForCover(ped, vTarget, vEye);
#if __DEV
			TUNE_BOOL(DEBUG_SHOW_TARGETING_COVER_ADJUST, false);
			if(ped.IsLocalPlayer() && DEBUG_SHOW_TARGETING_COVER_ADJUST)
			{
				CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(vEye), 0.1f, Color_red, 2000);
			}
#endif
		}   
	}
#if __DEV
	static dev_bool DRAW_ALL_DEBUG_TARGET_LINES = false;
	static dev_bool DRAW_PLAYER_DEBUG_TARGET_LINES = false;
	TUNE_BOOL(DEBUG_CAN_PED_TARGET_POINT, false);
	if( ( ped.IsLocalPlayer() && DRAW_PLAYER_DEBUG_TARGET_LINES ) || DRAW_ALL_DEBUG_TARGET_LINES )
	{
		grcDebugDraw::Line( vEye, vTarget, Color_blue );
	}
#endif
	if(pCacheEntry)
	{
		pCacheEntry->m_vFromPoint	= vEye;
		pCacheEntry->m_vToPoint		= vTarget;
	}

	vDiff=vTarget - vPedPosition;

    if((iTargetOptions & TargetOption_doDirectionalTest) && (DotProduct(vDiff,vPedDir)<0))
    {
        return false;
    }

	float fSeeingRange = ped.GetPedIntelligence()->GetPedPerception().GetSeeingRange();
	if(iTargetOptions & TargetOption_TargetIsInSubmergedSub)
	{
		fSeeingRange = MIN(fSeeingRange, DISTANCE_SPOTTED_IN_UNKNOWN_VEHICLE);
	}
	else
	{
		if( ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && (iTargetOptions & TargetOption_TargetInVehicle) )
			fSeeingRange = MAX( fSeeingRange, TARGET_VEHICLE_DISTANCE);

		if(iTargetOptions & TargetOption_TargetIsWantedAndInHeli)
		{
			fSeeingRange = MAX(fSeeingRange, ped.GetPedIntelligence()->GetPedPerception().GetSeeingRange() * TARGET_IN_HELI_MULTIPLIER);
		}

		if(iTargetOptions & TargetOption_UseExtendedCombatRange)
		{
			fSeeingRange = MAX(fSeeingRange, ms_fExtendedCombatRange);
		}
	}

    if(!ped.IsLocalPlayer() && vDiff.Mag2() > rage::square( fSeeingRange ) )
    {
        return false;
    }

	int nIncludeFlags = ArchetypeFlags::GTA_ALL_SHAPETEST_TYPES;

	int nFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE;

	if(!(iTargetOptions & TargetOption_IgnoreVehicles))
		nFlags |= ArchetypeFlags::GTA_VEHICLE_TYPE;

	if(!(iTargetOptions & TargetOption_IgnoreSmoke))		
	{
		nFlags |= ArchetypeFlags::GTA_SMOKE_TYPE;
		nIncludeFlags |= ArchetypeFlags::GTA_SMOKE_INCLUDE_TYPES;
	}

	if((iTargetOptions & TargetOption_TestForPotentialPedTargets))
	{
		nFlags |= ArchetypeFlags::GTA_PED_TYPE;
	}

	int nOptions = WorldProbe::LOS_IGNORE_NO_COLLISION | WorldProbe::LOS_IGNORE_SEE_THRU; /* | WorldProbe::LOS_IGNORE_SHOOT_THRU;*/

	if(ped.GetPedResetFlag(CPED_RESET_FLAG_DisableSeeThroughChecksWhenTargeting))
	{
		nOptions = 0;
	}

	if(pCacheEntry)
	{
		// We were assigned a free cache entry back in GetCachedResult(). That means there is space for us to do a new
		// asynchronous probe.

		WorldProbe::CShapeTestProbeDesc probeData;
		probeData.SetResultsStructure(&(pCacheEntry->m_hShapeTestHandle));
		probeData.SetStartAndEnd(vEye,vTarget);
		probeData.SetContext(WorldProbe::EPedTargetting);
		probeData.SetExcludeEntities(ppExcludeEnts, nNumExcludeEnts);
		probeData.SetIncludeFlags(nFlags);
		probeData.SetOptions(nOptions);
		probeData.SetIsDirected(true);
		WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

		if(pCacheEntry->m_hShapeTestHandle.GetResultsStatus() == WorldProbe::SUBMISSION_FAILED)
		{
			#if !__FINAL
			ms_iNumAsyncLineTestsRejectedLastFrame++;
			#endif

			// Fall through to the standard LOS tests below.
		}
		else
		{
			// Asynchronous probe is in flight.
			m_nAsyncProbeTimeout = fwTimer::GetTimeInMilliseconds();
			return true;
		}
	}

	// Perform a synchronous test if we were unable to do an asynchronous one.

	if( pProbeStartPosition )
	{
		*pProbeStartPosition = vEye;
	}

	{
		INC_PEDAI_LOS_COUNTER;
		WorldProbe::CShapeTestHitPoint probeIsects[MAX_TARGETTING_INTERSECTIONS];
		WorldProbe::CShapeTestResults probeResults(probeIsects, MAX_TARGETTING_INTERSECTIONS);
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetTypeFlags(nIncludeFlags);
		probeDesc.SetStartAndEnd(vEye, vTarget);
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetIncludeFlags(nFlags);
		probeDesc.SetOptions(nOptions);
		probeDesc.SetExcludeEntities(ppExcludeEnts, nNumExcludeEnts);
		probeDesc.SetContext(WorldProbe::EPedTargetting);

		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
		const s32 iNumIntersections = probeResults.GetNumHits();

		if( iNumIntersections == 0 )
		{
#if __DEV
			if (DEBUG_CAN_PED_TARGET_POINT)
				CTask::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(probeDesc.GetStart()), VECTOR3_TO_VEC3V(probeDesc.GetEnd()), Color_green, 2000);
#endif
			return true;
		}

		for( s32 i = 0; i < MAX_TARGETTING_INTERSECTIONS; i++ )
		{
			// Can have gammy intersections with no instance
			if( !probeResults[i].GetHitDetected() )
				continue;

			CEntity* pHitEntity = CPhysics::GetEntityFromInst(probeResults[i].GetHitInst());
			if (pHitEntity)
			{
				//! HACK - REMOVE post V.
				//! url:bugstar:1597655. Can't fixup map data, so we need to identify this part of the map
				//! as it hasn't been marked as see-through. 
				//! Nothing you can say will make me feel any worse than I do right now.
				if(NetworkInterface::IsGameInProgress() && 
					pHitEntity->GetCurrentPhysicsInst() && 
					pHitEntity->GetCurrentPhysicsInst()->GetArchetype() && 
					pHitEntity->GetCurrentPhysicsInst()->GetArchetype()->GetFilename())
				{
					static u32 nFuckingAwfulMapLookUpHash = atStringHash("cs1_13_2");

					u32 nArchHash = atStringHash(pHitEntity->GetCurrentPhysicsInst()->GetArchetype()->GetFilename());
					if(nArchHash == nFuckingAwfulMapLookUpHash)
					{
						//! The part of the map we care about is marked as no cover, so just use that.
						bool bSeeThrough = PGTAMATERIALMGR->GetIsNotCover(probeResults[i].GetMaterialId());
						if(bSeeThrough)
						{
							continue;
						}
					}
				}
				//! HACK - REMOVE post V.

				if( (iTargetOptions & TargetOption_TestForPotentialPedTargets) && pHitEntity->GetIsTypePed() )
				{ 
					//! if ped isn't in our targeting array, then we can skip this.
					bool bHitOtherTarget = false;
					for(int nTarget = 0; nTarget < CPedTargetEvaluator::GetNumPotentialTargets(); ++nTarget)
					{
						if( (CPedTargetEvaluator::GetPotentialTarget(nTarget).pEntity == pHitEntity) && 
							!CPedTargetEvaluator::IsDownedPedThreat(static_cast<CPed*>(pHitEntity), true ) )
						{
							bHitOtherTarget = true;
							break;
						}
					}
					
					if(!bHitOtherTarget)
					{
						continue;
					}
				}

				bool bTargetingDownedPed = pTargeteePed && CPedTargetEvaluator::IsDownedPedThreat(pTargeteePed);

				//! Don't lock on to downed peds through vehicles. Just prevent lock on in this case.
				if( !bTargetingDownedPed && 
					(iTargetOptions & TargetOption_IgnoreLowVehicles) && 
					pHitEntity->GetIsTypeVehicle() )
				{
					CVehicle *pHitVehicle = static_cast<CVehicle*>(pHitEntity);
					spdAABB tempBox;
					const spdAABB &bbox = pHitEntity->GetBoundBox(tempBox);

					//! If we hit a vehicle door, extend low vehicle height as the bounding box is not really representative here.
					static dev_float s_fDefaultLowVehicleHeight = 2.0f;
					static dev_float s_fOpenDoorLowVehicleHeight = 3.0f;
					static dev_float s_fOpenDoorRatio = 0.5f;

					const CCarDoor* pDoor = CCarEnterExit::GetCollidedDoorFromCollisionComponent(pHitVehicle, probeResults[i].GetComponent());

					float fLowVehicleHeight = (pDoor && (pDoor->GetDoorRatio() > s_fOpenDoorRatio)) ? s_fOpenDoorLowVehicleHeight : s_fDefaultLowVehicleHeight;

					if( IsLessThanOrEqualAll((bbox.GetMax() - bbox.GetMin()).GetZ(), ScalarV(fLowVehicleHeight)) )
					{
						bool bBreakOut = false;
						//! Indicate to target scoring that we have hit a low vehicle.
						if(pTargeteePed)
						{
							for(int nTarget = 0; nTarget < CPedTargetEvaluator::GetNumPotentialTargets(); ++nTarget)
							{
								if(CPedTargetEvaluator::GetPotentialTarget(nTarget).pEntity == pTargeteePed)
								{
									CPedTargetEvaluator::GetPotentialTarget(nTarget).bTargetBehindLowVehicle = true;
									bBreakOut = true;
								}
							}
						}

						if(bBreakOut)
						{
							break;
						}

						continue;
					}
				}
			}

#if __DEV
			if (DEBUG_CAN_PED_TARGET_POINT)
			{
				CTask::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(probeDesc.GetStart()), VECTOR3_TO_VEC3V(probeResults[i].GetHitPosition()), Color_red, 2000);
				CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(probeResults[i].GetHitPosition()), 0.025f, Color_red, 2000);
			}
#endif 

			if( pvIntersectionPosition )
				*pvIntersectionPosition = probeResults[i].GetHitPosition();

			return false;
		}

#if __DEV
		if (DEBUG_CAN_PED_TARGET_POINT)
			CTask::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(probeDesc.GetStart()), VECTOR3_TO_VEC3V(probeDesc.GetEnd()), Color_green, 2000);
#endif
		return true;
	}
}


ECanTargetResult
CPedGeometryAnalyser::CanPedTargetPedAsync(CPed & targetter, CPed &  targettee, s32 iTargetOptions, const bool bDontIssueAsyncProbe, const float fMaxPositionDeltaSqr, const u32 iMaxTimeDeltaMs, Vector3 * pProbeStartPosition, Vector3 * pIntersectionIfLosBlocked)
{
	// Firstly, check to see if we have a result for these 2 peds from last frame
	CCanTargetCacheEntry * pEmptyCacheEntry = NULL;

	// As long as we are considering issuing probes, allow old cache entries to get recycled
	// if there are no unused ones.
	const bool bReturnOldestIfFull = !bDontIssueAsyncProbe;

	ECanTargetResult eCanTarget = ms_CanTargetCache.GetCachedResult(&targetter, &targettee, &pEmptyCacheEntry, fMaxPositionDeltaSqr, iMaxTimeDeltaMs, pProbeStartPosition, pIntersectionIfLosBlocked, bReturnOldestIfFull);

	if(eCanTarget==ECanTarget || eCanTarget==ECanNotTarget)
		return eCanTarget;

	if(bDontIssueAsyncProbe)
		return ENotSureYet;

	//**************************************************************************************************************
	// If we didn't get a pFirstFreeSlot pointer, then it means there is no space in the 'm_CanTargetQueries' cache.
	// For this to happen there needs to be a very large number of these queries happening at this point in time.
	// It is not a problem, as we can just revert to the old method of doing the test immediately.

	if(!pEmptyCacheEntry)
	{
		#if !__FINAL
		ms_iNumCacheFullEventsLastFrame++;
		#endif

		// Perform a synchronous targeting query from targetter->targettee.
		bool bLos = CanPedTargetPed(targetter, targettee, iTargetOptions, NULL);
		return bLos ? ECanTarget : ECanNotTarget;
	}

	// We didn't have a result for these two peds. Create a new query and pass it into the CanPedTargetPed function.
	pEmptyCacheEntry->Init(&targetter, &targettee);

	// Try to create an asynchronous targeting query from targetter->targettee.
	bool bLos = CanPedTargetPed(targetter, targettee, iTargetOptions, pEmptyCacheEntry, pProbeStartPosition, pIntersectionIfLosBlocked);

	// If we have no cache entry, then just return the bLos. There was probably no room to store the ped<->ped visibility above.
	if(!pEmptyCacheEntry)
	{
		return bLos ? ECanTarget : ECanNotTarget;
	}
	// If we have a cache entry, set up with the correct peds, but with no handle - then there was either no room to issue
	// the request with the CWorldProbeAsync system; or else we were able to early-out w/o needing to issue the request.
	// Either way bLos is valid, and we can cache the result.
	else if(pEmptyCacheEntry && pEmptyCacheEntry->MatchesPeds(&targetter, &targettee) && !pEmptyCacheEntry->m_hShapeTestHandle.GetResultsWaitingOrReady())
	{
		pEmptyCacheEntry->m_bLosExists = bLos;
		return bLos ? ECanTarget : ECanNotTarget;
	}

	// In all other cases an async request has been issued, and we need to wait upon the result
	return ENotSureYet;
}

//*********************************************************************************
//	GetModifiedTargetPositions
//	Given 2 peds and some targeting params, this returns the from & to positions
//	which should be used for the line-test between them.

void
CPedGeometryAnalyser::GetModifiedTargetPosition(const CPed & targetter, 
												const CPed & targettee, 
												const bool bUseDirectionTest,
												const bool bIgnoreTargetsCover, 
												const bool bUseTargetableDistance, 
												const bool bUseCoverVantagePoint, 
												const bool bUseMeleePosition,
												Vector3 & vTargetPos)
{
	((void)bUseDirectionTest);

	const Vector3 vTargetterPosition = VEC3V_TO_VECTOR3(targetter.GetTransform().GetPosition());
	const Vector3 vTargetteePosition = VEC3V_TO_VECTOR3(targettee.GetTransform().GetPosition());

	if(bUseMeleePosition)
	{
		vTargetPos = targettee.GetBonePositionCached(BONETAG_SPINE1);
		Vector3 vTargetGroundPos = targettee.GetGroundPos();
		
		//! If we have a valid ground, move position closer to the floor, so we don't target over small objects.
		if(targettee.GetIsStanding() && vTargetGroundPos.z > PED_GROUNDPOS_RESET_Z)
		{
			Vector3 vTargetGroundPos = targettee.GetGroundPos();
			vTargetPos.z = vTargetGroundPos.z + ((vTargetPos.z - vTargetGroundPos.z) * 0.75f);
		}
	}
	else
	{
		vTargetPos = targettee.GetBonePositionCached(BONETAG_HEAD);
		if(!targettee.GetIsInCover())
		{
			vTargetPos.x = vTargetteePosition.x;
			vTargetPos.y = vTargetteePosition.y;
		}
	}

// 	// Use the same position for los tests as for lockon
// 	targettee.GetLockOnTargetAimAtPos(vTargetPos);
	
	bool bUseCoverPos = bUseCoverVantagePoint && targetter.GetPedIntelligence()->GetTargetting()->AreTargetsWhereaboutsKnown();

	// Stop peds within a really close proximity using the cover position, they need a direct LOS at this range
	if( !targetter.IsLocalPlayer() && vTargetterPosition.Dist2(vTargetteePosition) < rage::square(2.0f) )
	{
		bUseCoverPos = false;
	}

	// If the target is in cover, work out the vantage location for the purposes of a LOS test
	if(!bIgnoreTargetsCover )
	{
		Vector3 vCoverPos;
		Vector3 vVantagePos;
		Vector3 vCoverDirection(Vector3::ZeroType);
		if(CCover::FindCoverCoordinatesForPed(targettee, vTargetterPosition - vTargetteePosition, vCoverPos, &vVantagePos, &vCoverDirection))
		{
			bool bInCover = targettee.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER);

			if(bUseCoverPos)
			{
				if( bInCover || ( ( ( vTargetteePosition - vVantagePos ).XYMag() < 2.0f ) && targettee.GetMotionData()->GetIsStill() ) )
				{
					vTargetPos = vVantagePos;
				}
			}
			else
			{
				if (bInCover && !targettee.GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover) && vCoverDirection.Mag2() > 0.01f)
				{
					vTargetPos += (-vCoverDirection * CPlayerPedTargeting::ms_Tunables.m_CoverDirectionOffsetForInCoverTarget);
				}
			}
		}
	}

	if (bUseTargetableDistance)
	{
		CEntity::GetEntityModifiedTargetPosition(targettee, vTargetterPosition, vTargetPos);
	}
}


bool
CPedGeometryAnalyser::CanPedTargetPed(const CPed& targetter, 
									  const CPed& targettee,
									  s32 iTargetOptions, 
									  CCanTargetCacheEntry * pEmptyCacheEntry, 
									  Vector3 * pProbeStartPosition, 
									  Vector3* pvIntersectionPosition)
{
#if !__FINAL
	ms_iTotalNumCanPedTargetPedCallsLastFrame++;
#endif

	Vector3 vTargetPos;
	const bool bDoDirectionalTest = (iTargetOptions&TargetOption_doDirectionalTest) != 0;
	const bool bIgnoreTargetsCover = (iTargetOptions&TargetOption_IgnoreTargetsCover) != 0;
	const bool bUseTargetableDistance = (iTargetOptions&TargetOption_UseTargetableDistance) != 0;
	const bool bUseCoverVantagePoint = (iTargetOptions&TargetOption_UseCoverVantagePoint) != 0;
	const bool bUseMeleePos = (iTargetOptions&TargetOption_UseMeleeTargetPosition) != 0;
	const bool bTestForPedTargets = (iTargetOptions&TargetOption_TestForPotentialPedTargets) != 0;

 	GetModifiedTargetPosition(targetter, 
		targettee, 
		bDoDirectionalTest, 
		bIgnoreTargetsCover, 
		bUseTargetableDistance, 
		bUseCoverVantagePoint,
		bUseMeleePos,
		vTargetPos);

	const CEntity* pException = NULL;
	CVehicle* pTargeteeVehicle = targettee.GetMyVehicle();

	// First check if the targetee is in a vehicle
	if( pTargeteeVehicle != NULL && targettee.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
	{
		iTargetOptions |= TargetOption_TargetInVehicle;
		
		// If the person doing the targeting is AI and also in a vehicle then we'll ignore vehicles
		if( targetter.GetMyVehicle() != NULL && targetter.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
		{
			if( !targetter.IsAPlayerPed() )
			{
				iTargetOptions |= TargetOption_IgnoreVehicles;
			}
		}
		else
		{
			// If the person doing the targeting is not in a vehicle then we'll ignore the targtee vehicle
			pException = pTargeteeVehicle;
		}

		if(targettee.IsAPlayerPed())
		{
			// Set some options based on if we are in a submerged sub or in a heli while wanted
			if(pTargeteeVehicle->InheritsFromSubmarine() || (pTargeteeVehicle->InheritsFromSubmarineCar() && pTargeteeVehicle->IsInSubmarineMode()))
			{
				if(pTargeteeVehicle->m_Buoyancy.GetStatus() == FULLY_IN_WATER)
				{
					iTargetOptions |= TargetOption_TargetIsInSubmergedSub;
				}
			}
			else if(pTargeteeVehicle->InheritsFromHeli())
			{
				CWanted* pWanted = targettee.GetPlayerWanted();
				if(pWanted && pWanted->GetWantedLevel() != WANTED_CLEAN)
				{
					iTargetOptions |= TargetOption_TargetIsWantedAndInHeli;
				}
			}
		}
	}

	CTaskMotionBase *pMotionTask = targetter.GetCurrentMotionTask();
	bool bUnderWater = pMotionTask && pMotionTask->IsUnderWater();

	// Can't see submerged peds (unless underwater of course).
	if( !bUnderWater && 
		!targetter.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanSeeUnderwaterPeds) &&
		CPedGeometryAnalyser::IsPedInWaterAtDepth(targettee, CPedGeometryAnalyser::ms_MaxPedWaterDepthForVisibility) )
	{
		return false;
	}

	// Don't use the camera position as the source if the player is assisted-aiming, as this mode does not fire through the camera.
	if( targetter.IsLocalPlayer() && targettee.GetIsVisibleInSomeViewportThisFrame() && !targetter.GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) )
	{
		iTargetOptions |= TargetOption_UseCameraPosIfOnScreen;
	}

	// Force the seeing range to be higher if we know the target's whereabouts
	CPedTargetting* pPedTargeting = targetter.GetPedIntelligence()->GetTargetting(false);
	if(pPedTargeting)
	{
		// Ensure the target info is valid, that the target has been hostile towards us recently and that we know their position
		const CSingleTargetInfo* pTargetInfo = pPedTargeting->FindTarget(&targettee);
		if( pTargetInfo && pTargetInfo->GetTimeOfLastHostileAction() > 0 &&
			CTimeHelpers::GetTimeSince(pTargetInfo->GetTimeOfLastHostileAction()) < ms_uTimeToUseExtendedCombatRange && pTargetInfo->AreWhereaboutsKnown() )
		{
			iTargetOptions |= TargetOption_UseExtendedCombatRange;
		}
	}

	const Vector3 vTargetteePosition = VEC3V_TO_VECTOR3(targettee.GetTransform().GetPosition());
	// can't target if the targetter is in a chopper and the targetee is in a tunnel underground ( no collision over tunnel).
	if( !targetter.IsAPlayerPed() && targetter.GetVehiclePedInside() && targetter.GetVehiclePedInside()->GetVehicleType() == VEHICLE_TYPE_HELI )
	{
		if( targettee.GetIsInInterior() && vTargetteePosition.z < 0.0f )
		{
			return false;
		}
	}

	// Field of view perception
	if( iTargetOptions & TargetOption_UseFOVPerception )
	{
		fwFlags8 uFlags = (targetter.IsLawEnforcementPed() && targettee.IsPlayer() &&
			(targettee.GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)) ? CPedPerception::FOV_CanUseRearView : 0;
		if( !targetter.GetPedIntelligence()->GetPedPerception().ComputeFOVVisibility(&targettee, &vTargetPos, uFlags) )
		{
			return false;
		}
	}

	static const s32 iMaxNumExceptions = 5 + CNetworkGhostCollisions::MAX_GHOST_COLLISIONS;
	const CEntity* ppExceptions[iMaxNumExceptions] = { NULL };

	s32 iNumExceptions = 0;
	ppExceptions[iNumExceptions++] = pException;

	if(!bIgnoreTargetsCover && targettee.IsLocalPlayer() && targettee.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER) )
	{
		if( targettee.GetCoverPoint() )
		{
			CEntity* pCoverEntity = CPlayerInfo::ms_DynamicCoverHelper.GetCoverEntryEntity();
			if (pCoverEntity && pCoverEntity->GetIsTypeVehicle())
			{
				ppExceptions[iNumExceptions++] = pCoverEntity;
			}
		}
	}

	if ( iTargetOptions & TargetOption_IgnoreVehicleThisPedIsIn && (iNumExceptions==0 || (targetter.GetMyVehicle() && targetter.GetMyVehicle()->IsTurretSeat(targetter.GetAttachCarSeatIndex()))) )
	{
		ppExceptions[iNumExceptions++] = targetter.GetMyVehicle();
	}


	if (bTestForPedTargets)
	{
		ppExceptions[iNumExceptions++] = &targetter;
		ppExceptions[iNumExceptions++] = &targettee;
	}

	// ignore any ghost entities that are currently intersecting the targettee. This is to avoid the exploit where players can stand inside ghost (passive) vehicles and not get shot by
	// the cops
	if (NetworkInterface::IsGameInProgress() && targettee.IsAPlayerPed())
	{
		const CPhysical* ghostCollisions[CNetworkGhostCollisions::MAX_GHOST_COLLISIONS];
		u32 numGhostCollisions = 0;

		CNetworkGhostCollisions::GetAllGhostCollisions(targettee, ghostCollisions, numGhostCollisions);

		for (u32 i=0; i<numGhostCollisions; i++)
		{
			ppExceptions[iNumExceptions++] = ghostCollisions[i];
		}
	}

	bool bReturn = CanPedTargetPoint(targetter, vTargetPos, iTargetOptions, ppExceptions, iNumExceptions, pEmptyCacheEntry, pProbeStartPosition, pvIntersectionPosition, &targettee );
	
	//! If targeting through camera in cover, do another test from cover vantage point to see if we can see target ped.
	//! B*1425949.
	if( !bReturn &&
		targetter.IsLocalPlayer() && 
		targetter.GetCoverPoint() && 
		FPS_MODE_SUPPORTED_ONLY(!targetter.IsFirstPersonShooterModeEnabledForPlayer(false) &&)
		(iTargetOptions & TargetOption_UseCameraPosIfOnScreen) &&
		!(iTargetOptions & TargetOption_UseMeleeTargetPosition) )
	{
		iTargetOptions &= ~TargetOption_UseCameraPosIfOnScreen;
	
		bReturn = CanPedTargetPoint(targetter, vTargetPos, iTargetOptions, ppExceptions, iNumExceptions, pEmptyCacheEntry, pProbeStartPosition, pvIntersectionPosition, &targettee );
	
		//! Inform targeting that we couldn't see this ped from cover, so that we can prrioritise peds we can see.
		if(bReturn && targetter.IsLocalPlayer())
		{
			for(int nTarget = 0; nTarget < CPedTargetEvaluator::GetNumPotentialTargets(); ++nTarget)
			{
				if(CPedTargetEvaluator::GetPotentialTarget(nTarget).pEntity == &targettee)
				{
					CPedTargetEvaluator::GetPotentialTarget(nTarget).bCantSeeFromCover = true;
					break;
				}
			}
		}
	}

	if( pEmptyCacheEntry )
	{
		pEmptyCacheEntry->m_vFromPoint = VEC3V_TO_VECTOR3(targetter.GetTransform().GetPosition());
		pEmptyCacheEntry->m_vToPoint = vTargetteePosition;
	}

	return bReturn;
}


bool
CPedGeometryAnalyser::IsPedInUnknownCar( const CPed& ped )
{
	if( !ped.IsPlayer() ||
		!ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
	{
		return false;
	}

	CVehicle* pMyVehicle = ped.GetMyVehicle();
	if( !pMyVehicle )
	{
		return false;
	}

	if (pMyVehicle->m_bDisableWantedConesResponse)
	{
		return true;
	}
	
	// Check for submerged subs before checking for our vehicle being wanted
	if( (pMyVehicle->InheritsFromSubmarine() || (pMyVehicle->InheritsFromSubmarineCar() && pMyVehicle->IsInSubmarineMode())) &&
		pMyVehicle->m_Buoyancy.GetStatus() == FULLY_IN_WATER )
	{
		return true;
	}

	if(	pMyVehicle->m_nVehicleFlags.bWanted )
	{
		return false;
	}

	return true;
}


bool CPedGeometryAnalyser::IsPedInBush( const CPed& ped )
{
	return ped.GetPedResetFlag( CPED_RESET_FLAG_InContactWithBIGFoliage );
}

bool CPedGeometryAnalyser::IsPedInWaterAtDepth( const CPed& ped, float fDepth )
{
	// If the other ped is under water a certain depth then we heavily restrict the distance
	if( !ped.GetIsInVehicle() && ped.GetIsInWater() &&
		ped.m_Buoyancy.GetAbsWaterLevel() - ped.GetTransform().GetPosition().GetZf() > fDepth )
	{
		return true;
	}

	return false;
}

bool
CPedGeometryAnalyser::CanPedSeePedInUnknownCar( const CPed& targetter, const CPed& targettee)
{
	Assert(targettee.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && targettee.GetMyVehicle());

	CVehicle* pMyVehicle = targettee.GetMyVehicle();

	if (pMyVehicle && pMyVehicle->m_bDisableWantedConesResponse)
	{
		return false;
	}

	// We should not be able to see fully submerged submarines
	if( (pMyVehicle->InheritsFromSubmarine() || (pMyVehicle->InheritsFromSubmarineCar() && pMyVehicle->IsInSubmarineMode())) &&
		pMyVehicle->m_Buoyancy.GetStatus() == FULLY_IN_WATER )
	{
		return false;
	}

	// Now that we have verified the vehicle isn't a fully submerged sub, we can assert on the wanted status of the vehicle
	Assert(!pMyVehicle->m_nVehicleFlags.bWanted);

	Vector3 vFromTargetToPed = VEC3V_TO_VECTOR3(targetter.GetTransform().GetPosition() - targettee.GetTransform().GetPosition());
	if( vFromTargetToPed.Mag2() < rage::square( DISTANCE_SPOTTED_IN_UNKNOWN_VEHICLE ) )
	{
		return true;
	}
	return false;
}


bool CPedGeometryAnalyser::CanPedSeePedInBush( const CPed& targetter, const CPed& targettee )
{
	Assert(targettee.GetPedResetFlag( CPED_RESET_FLAG_InContactWithBIGFoliage ) );
	Vector3 vFromTargetToPed = VEC3V_TO_VECTOR3(targetter.GetTransform().GetPosition() - targettee.GetTransform().GetPosition());
	if( vFromTargetToPed.Mag2() < rage::square( DISTANCE_SPOTTED_IN_BUSH ) )
	{
		return true;
	}
	return false;
}

float CPedGeometryAnalyser::GetDistanceSpottedInUnknownVehicle()
{
	return DISTANCE_SPOTTED_IN_UNKNOWN_VEHICLE;
}
float CPedGeometryAnalyser::GetDistanceSpottedInBush()
{
	return DISTANCE_SPOTTED_IN_BUSH;
}

bool CPedGeometryAnalyser::IsInAir(const CPed& ped)
{
	PF_FUNC(CPedGeometryAnalyser_IsInAir);

	// if NOT driving and NOT swimming in water
	if((ped.GetPedResetFlag(CPED_RESET_FLAG_CanAbortExitForInAirEvent) || !ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) && (!ped.GetPedIntelligence()->GetTaskActive() 
	|| (!ped.GetPedIntelligence()->IsPedSwimming()
	&&  !ped.GetCurrentMotionTask()->CanFlyThroughAir() //ped.GetPedIntelligence()->GetTaskActiveSimplest()->GetTaskType()!=CTaskTypes::TASK_SIMPLE_PLAYER_ON_SKIS
	&&  !ped.GetPedResetFlag( CPED_RESET_FLAG_IsClimbing )
	&&  !ped.GetPedResetFlag( CPED_RESET_FLAG_SuppressInAirEvent )
	&&	!ped.GetPedResetFlag( CPED_RESET_FLAG_IsRappelling ))))
	{
		// if we are not falling down, no need to do shape tests
		static const float minFallingSpeed = -1.f;
		if(ped.GetVelocity().z > minFallingSpeed)
			return false;

		static dev_float s_fLandThreshold = 0.5f;
		static dev_float s_fLandRadius = 0.25f;
		bool bGetGround = false;
		
		//! If we are on a train, choose less aggressive fall params to keep from falling too easily between gaps.
		if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_RidingTrain))
		{
			Vector3 vRemainingDistance(Vector3::ZeroType);
			float fLandOut = PED_GROUNDPOS_RESET_Z;
			bGetGround = CTaskJump::IsPedNearGroundIncRadius(&ped, s_fLandThreshold, s_fLandRadius, vRemainingDistance, fLandOut);
		}
		else
		{
			Vector3 vStart = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
			Vector3 vEnd = vStart - Vector3(0.0f, 0.0f, ped.GetCapsuleInfo()->GetGroundToRootOffset() + 0.5f);

			INC_PEDAI_LOS_COUNTER;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vStart, vEnd);
			probeDesc.SetResultsStructure(NULL);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
			probeDesc.SetExcludeEntity(NULL);

			bGetGround = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
			if(!bGetGround && !ped.GetPedResetFlag( CPED_RESET_FLAG_IsJumping ))
			{	
				vStart.z -= ped.GetCapsuleInfo()->GetGroundToRootOffset();
				WorldProbe::CShapeTestSphereDesc sphereDesc;
				sphereDesc.SetSphere(vStart, 0.15f);
				sphereDesc.SetResultsStructure(NULL);
				sphereDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
				sphereDesc.SetExcludeEntity(&ped);
				if(WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
				{
					bGetGround = true;
				}
			}
		}
		return !bGetGround;
	}
	else
	{
		//Ped in vehicle so can't be in free-fall.
		return false;
	}
}

const CPed* CPedGeometryAnalyser::GetNearestPed(const Vector3& vPos, const CPed* pException )
{
	CPed* pNearestPed=0;
	float fNearestDist2=FLT_MAX;//FLT_MAX;
	CPed::Pool* pPedPool = CPed::GetPool();
	s32 i=pPedPool->GetSize();	
	while(i--)
	{
		CPed* pPed = pPedPool->GetSlot(i);
		if(pPed && ( pPed != pException ) )
		{
			Vector3 vDiff=vPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const float fDist2=vDiff.Mag2();
			if(fDist2<fNearestDist2)
			{
				fNearestDist2=fDist2;
				pNearestPed=pPed;
			}
		}
	}
	return pNearestPed;
}

CRouteToDoorCalculator::CRouteToDoorCalculator(CPed * pPed, const Vector3 & vStartPos, const Vector3 & vEndPos, CVehicle * pVehicle, CPointRoute * pOutRoute, const eHierarchyId iDoorPedIsHeadingFor)
{
	m_vStartPos = vStartPos;
	m_vEndPos = vEndPos;
	m_pPed = pPed;
	m_pVehicle = pVehicle;
	m_pRoute = pOutRoute;
	m_iDoorPedIsHeadingFor = iDoorPedIsHeadingFor;

	// Make a list of which components of this vehicle are a part of its doors.
	m_iNumDoorComponentsToExclude = 0;
	static const int iDoorIDs[4] = { VEH_DOOR_DSIDE_F, VEH_DOOR_DSIDE_R, VEH_DOOR_PSIDE_F, VEH_DOOR_PSIDE_R };
	for(int d=0; d<4; d++)
	{
		const int iIndex = d*MAX_DOOR_COMPONENTS;
		m_iDoorComponentsToExclude[iIndex] = -1;
		m_iDoorComponentsToExclude[iIndex+1] = -1;
		CCarEnterExit::GetDoorComponents(pVehicle, (eHierarchyId)iDoorIDs[d], &(m_iDoorComponentsToExclude[iIndex]));
		m_iNumDoorComponentsToExclude += MAX_DOOR_COMPONENTS;
	}

	m_AiBound.Set(*m_pVehicle, m_pVehicle->GetTransform().GetPosition().GetZf(), PED_HUMAN_RADIUS, false, NULL, m_iNumDoorComponentsToExclude, m_iDoorComponentsToExclude);
}
CRouteToDoorCalculator::~CRouteToDoorCalculator()
{

}

void CRouteToDoorCalculator::GetStartingCornersFromSideBitFlags(const u32 iStartingSideBitflags, int & iStartingCornerClockwise, int & iStartingCornerAntiClockwise)
{
	Assertf(iStartingSideBitflags, "GetStartingCornersFromSideBitFlags() - iStartingSideBitflags cannot be zero");

	// When along the side/end of the vehicle, use the immediately adjacent corners
	if(iStartingSideBitflags == CEntityBoundAI::ms_iSideFlags[CEntityBoundAI::FRONT])
	{
		iStartingCornerClockwise = CEntityBoundAI::FRONT_RIGHT;
		iStartingCornerAntiClockwise = CEntityBoundAI::FRONT_LEFT;
	}
	else if(iStartingSideBitflags == CEntityBoundAI::ms_iSideFlags[CEntityBoundAI::LEFT])
	{
		iStartingCornerClockwise = CEntityBoundAI::FRONT_LEFT;
		iStartingCornerAntiClockwise = CEntityBoundAI::REAR_LEFT;
	}
	else if(iStartingSideBitflags ==  CEntityBoundAI::ms_iSideFlags[CEntityBoundAI::REAR])
	{
		iStartingCornerClockwise = CEntityBoundAI::REAR_LEFT;
		iStartingCornerAntiClockwise = CEntityBoundAI::REAR_RIGHT;
	}
	else if(iStartingSideBitflags == CEntityBoundAI::ms_iSideFlags[CEntityBoundAI::RIGHT])
	{
		iStartingCornerClockwise = CEntityBoundAI::REAR_RIGHT;
		iStartingCornerAntiClockwise = CEntityBoundAI::FRONT_RIGHT;
	}

	// When in a corner beyond two edge-planes, use the corners adjacent to the closest corner
	else if(iStartingSideBitflags == CEntityBoundAI::ms_iCornerFlags[CEntityBoundAI::FRONT_LEFT])
	{
		iStartingCornerClockwise = CEntityBoundAI::REAR_LEFT;
		iStartingCornerAntiClockwise = CEntityBoundAI::FRONT_RIGHT;
	}
	else if(iStartingSideBitflags == CEntityBoundAI::ms_iCornerFlags[CEntityBoundAI::REAR_LEFT])
	{
		iStartingCornerClockwise = CEntityBoundAI::FRONT_LEFT;
		iStartingCornerAntiClockwise = CEntityBoundAI::REAR_RIGHT;
	}
	else if(iStartingSideBitflags == CEntityBoundAI::ms_iCornerFlags[CEntityBoundAI::REAR_RIGHT])
	{
		iStartingCornerClockwise = CEntityBoundAI::REAR_LEFT;
		iStartingCornerAntiClockwise = CEntityBoundAI::FRONT_RIGHT;
	}
	else if(iStartingSideBitflags == CEntityBoundAI::ms_iCornerFlags[CEntityBoundAI::FRONT_RIGHT])
	{
		iStartingCornerClockwise = CEntityBoundAI::REAR_RIGHT;
		iStartingCornerAntiClockwise = CEntityBoundAI::FRONT_LEFT;
	}
	else
	{
		Assertf(false, "GetStartingCornersFromSideBitFlags() - iStartingSideBitflags should be at most 2 edgeplanes.");
		iStartingCornerClockwise = CEntityBoundAI::REAR_RIGHT;
		iStartingCornerAntiClockwise = CEntityBoundAI::FRONT_LEFT;
	}
}

bool CRouteToDoorCalculator::TestRouteForObstruction(CPointRoute * pRoute, float & fObstructionDistance, CEntity ** ppBlockingEntity)
{
	static const float fCapsuleRadius = PED_HUMAN_RADIUS - 0.1f;
	static const u32 iLosFlags = (ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
	static const float fStillMbrSqr = 0.1f * 0.1f;

	WorldProbe::CShapeTestFixedResults<> capsuleResult;

	float fDistSoFar = 0.0f;
	fObstructionDistance = 0.0f;
	*ppBlockingEntity = NULL;

	int i;
	for(i=0; i<pRoute->GetSize()-1; i++)
	{
		const Vector3 & v1 = pRoute->Get(i);
		const Vector3 & v2 = pRoute->Get(i+1);

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleResult.Reset();
		capsuleDesc.SetResultsStructure(&capsuleResult);
		capsuleDesc.SetCapsule(v1, v2, fCapsuleRadius);
		capsuleDesc.SetIncludeFlags(iLosFlags);
		capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
		capsuleDesc.SetExcludeEntity(m_pVehicle);
		capsuleDesc.SetIsDirected(true);
		WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

		const Vector3 v1_to_v2 = v2 - v1;

		// If this line-segment hit something, then see whether it is ignorable or not.
		if(capsuleResult[0].GetHitDetected())
		{
			CEntity * pHitEntity = CPhysics::GetEntityFromInst(capsuleResult[0].GetHitInst());
			Assert(pHitEntity && pHitEntity!=m_pVehicle && pHitEntity!=m_pPed);

			if(pHitEntity && !IsAnOpenableDoor(pHitEntity))
			{
				if(pHitEntity->GetIsTypePed())
				{
					Vector3 vMoveVec;
					((CPed*)pHitEntity)->GetCurrentMotionTask()->GetMoveVector(vMoveVec);

					// If the ped is stationary, OR moving against the direction of travel
					if(vMoveVec.Mag2() < fStillMbrSqr || DotProduct(v1_to_v2, vMoveVec) < 0.0f)
					{
						fObstructionDistance = (fDistSoFar + (capsuleResult[0].GetHitPosition()-v1).Mag());
						*ppBlockingEntity = pHitEntity;
						break;
					}
				}
				else
				{
					fObstructionDistance = (fDistSoFar + (capsuleResult[0].GetHitPosition()-v1).Mag());
					*ppBlockingEntity = pHitEntity;
					break;
				}
			}
		}

		fDistSoFar += v1_to_v2.Mag();
	}

	return (fObstructionDistance != 0.0f);
}

bool CRouteToDoorCalculator::CalculateRouteToDoor()
{
	int iStartSideFlags = m_AiBound.ComputePlaneFlags(m_vStartPos);
	if(!iStartSideFlags)
	{
		m_AiBound.MoveOutside(m_vStartPos, m_vStartPos);
		iStartSideFlags = m_AiBound.ComputePlaneFlags(m_vStartPos);
		Assertf(iStartSideFlags, "m_vStartPos failed to move outside bound.");
	}

	int iTargetSideFlags = m_AiBound.ComputePlaneFlags(m_vEndPos);
	if(!iTargetSideFlags)
	{
		m_AiBound.MoveOutside(m_vEndPos, m_vEndPos);
		iTargetSideFlags = m_AiBound.ComputePlaneFlags(m_vEndPos);
		Assertf(iTargetSideFlags, "m_vEndPos failed to move outside bound.");
	}

	const bool bNeedsToPostProcessRoute = true;
	

	//********************************************************************************
	// If iStartSide & iEndSide are the same then the ped can just walk to the target
	// unless there is an open door or other component blocking the way.

	if(iStartSideFlags & iTargetSideFlags)
	{
		m_pRoute->Add(m_vStartPos);
		m_pRoute->Add(m_vEndPos);
		m_iRouteSideFlags[0] = 0;	//iStartSideFlags;
		m_iRouteSideFlags[1] = 0;	//iTargetSideFlags;

		if(bNeedsToPostProcessRoute)
			PostProcessToAvoidComponents();

		return true;
	}

	//****************************************************************************************
	// Even if iStartSide & iEndSide are not the same it may be possible to just walk to the
	// target if the hit-sides are adjacent, and there is no corner in the way (and unless
	// there is an open door or other component blocking the way.

	Vector3 vEdgePlaneNormals[4];
	float fEdgePlaneDists[4];
	m_AiBound.GetPlanes(vEdgePlaneNormals, fEdgePlaneDists);

	// Note: this could probably use the new TestLineOfSight(), and then we wouldn't need
	// the planes above for anything - looks like just the standard planes from corners.

	if(m_AiBound.TestLineOfSightOld(m_vStartPos, m_vEndPos, vEdgePlaneNormals, fEdgePlaneDists, 4))
	{
		m_pRoute->Add(m_vStartPos);
		m_pRoute->Add(m_vEndPos);
		m_iRouteSideFlags[0] = 0;	//iStartSideFlags;
		m_iRouteSideFlags[1] = 0;	//iTargetSideFlags;

		if(bNeedsToPostProcessRoute)
			PostProcessToAvoidComponents();

		return true;
	}

	//********************************************************
	// Find which corner is the closest to the start position

	Vector3 vCorners[4];
	m_AiBound.GetCorners(vCorners);

	//*******************************************************************************************
	// Create two routes, each starting with the closest corner - and one going clockwise round
	// the vehicle whilst the other goes anticlockwise.

	int iCurrentCornerFlags;
	int iStartCornerClockwise, iStartCornerAntiClockwise;
	GetStartingCornersFromSideBitFlags(iStartSideFlags, iStartCornerClockwise, iStartCornerAntiClockwise);

	//*******************
	// Clockwise route

	CPointRoute routeClockwise;
	int iClockwiseRouteSideFlags[CPointRoute::MAX_NUM_ROUTE_ELEMENTS];

	iClockwiseRouteSideFlags[routeClockwise.GetSize()] = iStartSideFlags;
	routeClockwise.Add(m_vStartPos);
	
	int iStartCorner = iStartCornerClockwise;
	iCurrentCornerFlags = CEntityBoundAI::ms_iCornerFlags[iStartCorner];
	iClockwiseRouteSideFlags[routeClockwise.GetSize()] = iCurrentCornerFlags;
	routeClockwise.Add(vCorners[iStartCorner]);
	
	do 
	{
		// Decrement is clockwise
		iStartCorner--;
		if(iStartCorner < 0)
			iStartCorner += 4;

		if((iCurrentCornerFlags & iTargetSideFlags)!=0)
			break;

		iCurrentCornerFlags = CEntityBoundAI::ms_iCornerFlags[iStartCorner];
		iClockwiseRouteSideFlags[routeClockwise.GetSize()] = iCurrentCornerFlags;
		routeClockwise.Add(vCorners[iStartCorner]);

	} while((iCurrentCornerFlags & iTargetSideFlags)==0);

	iClockwiseRouteSideFlags[routeClockwise.GetSize()] = iTargetSideFlags;
	routeClockwise.Add(m_vEndPos);

	//**********************
	// Anticlockwise route

	CPointRoute routeAntiClockwise;
	int iAntiClockwiseRouteSideFlags[CPointRoute::MAX_NUM_ROUTE_ELEMENTS];

	iAntiClockwiseRouteSideFlags[routeAntiClockwise.GetSize()] = iStartSideFlags;
	routeAntiClockwise.Add(m_vStartPos);

	iStartCorner = iStartCornerAntiClockwise;
	iCurrentCornerFlags = CEntityBoundAI::ms_iCornerFlags[iStartCorner];
	iAntiClockwiseRouteSideFlags[routeAntiClockwise.GetSize()] = iCurrentCornerFlags;
	routeAntiClockwise.Add(vCorners[iStartCorner]);

	do 
	{
		// Increment is anticlockwise
		iStartCorner++;
		if(iStartCorner >= 4)
			iStartCorner -= 4;

		if((iCurrentCornerFlags & iTargetSideFlags)!=0)
			break;

		iCurrentCornerFlags = CEntityBoundAI::ms_iCornerFlags[iStartCorner];
		iAntiClockwiseRouteSideFlags[routeAntiClockwise.GetSize()] = iCurrentCornerFlags;
		routeAntiClockwise.Add(vCorners[iStartCorner]);

	} while((iCurrentCornerFlags & iTargetSideFlags)==0);

	iAntiClockwiseRouteSideFlags[routeAntiClockwise.GetSize()] = iTargetSideFlags;
	routeAntiClockwise.Add(m_vEndPos);

	//***************************************************************************
	// Clone m_pRoute from whichever route was unblocked and was least distance

	float fClockwiseObstructionDistance = 0.0f;
	CEntity * pClockwiseObstructionEntity = NULL;
	const bool bClockwiseRouteBlocked = TestRouteForObstruction(&routeClockwise, fClockwiseObstructionDistance, &pClockwiseObstructionEntity);

	float fAntiClockwiseObstructionDistance = 0.0f;
	CEntity * pAntiClockwiseObstructionEntity = NULL;
	const bool bAntiClockwiseRouteBlocked = TestRouteForObstruction(&routeAntiClockwise, fAntiClockwiseObstructionDistance, &pAntiClockwiseObstructionEntity);

	CPointRoute * pRouteToChoose = NULL;

	// Neither routes are blocked
	if(!bClockwiseRouteBlocked && !bAntiClockwiseRouteBlocked)
	{
		const float fClockwiseLength = routeClockwise.GetLength();
		const float fAntiClockwiseLength = routeAntiClockwise.GetLength();
		pRouteToChoose = (fClockwiseLength < fAntiClockwiseLength) ? &routeClockwise : &routeAntiClockwise;
	}
	// One or the other is blocked
	else if(!(bClockwiseRouteBlocked && bAntiClockwiseRouteBlocked))
	{
		pRouteToChoose = bAntiClockwiseRouteBlocked ? &routeClockwise : &routeAntiClockwise;
	}
	// Both are blocked, choose whichever route has the longest distance until the obstruction is reached
	else
	{
		pRouteToChoose = (fClockwiseObstructionDistance > fAntiClockwiseObstructionDistance) ? &routeClockwise : &routeAntiClockwise;
	}

	//**********************************************
	// Copy the final chosen route into m_pRoute

	Assert(pRouteToChoose);
	m_pRoute->From(*pRouteToChoose);

	int * pRouteSideFlags = (pRouteToChoose==&routeClockwise) ? &iClockwiseRouteSideFlags[0] : &iAntiClockwiseRouteSideFlags[0];
	for(s32 i=0; i<pRouteToChoose->GetSize(); i++)
		m_iRouteSideFlags[i] = pRouteSideFlags[i];

	if(bNeedsToPostProcessRoute)
		PostProcessToAvoidComponents();

	return true;
}

void CRouteToDoorCalculator::PostProcessToAvoidComponents()
{
	static const bool bUseMultipleCapsules = false;
	static const bool bUseOnlyBoundingBoxes = false;

	//*********************************************************************************
	// Make a CEntityBoundAI with all the non-convex components included.
	// If our calculated root hits any doors, etc, along the side of the vehicle then
	// we will walk out to the edge of this 2nd bound for the duration of that edge.

	CEntityBoundAI boundWithComponents;
	boundWithComponents.Set(*m_pVehicle, m_pVehicle->GetTransform().GetPosition().GetZf(), PED_HUMAN_RADIUS, false, NULL);
	Vector3 vExpandedCorners[4];
	boundWithComponents.GetCorners(vExpandedCorners);

	int i;
	for(i=1; i<m_pRoute->GetSize()-2; i++)
	{
		const u32 iVertFlags = m_iRouteSideFlags[i];
		const u32 iNextVertFlags = m_iRouteSideFlags[i+1];

		const Vector3 & vPos = m_pRoute->Get(i);
		const Vector3 & vNextPos = m_pRoute->Get(i+1);

		const bool bHitVehicle = DoesRouteSegHitEntity(vPos, vNextPos, PED_HUMAN_RADIUS, m_pVehicle, bUseMultipleCapsules, bUseOnlyBoundingBoxes, m_iDoorPedIsHeadingFor);

		if(bHitVehicle)
		{
			//***********************************************************************************
			// The line from vLastPos to vPos hit the vehicle.
			// To avoid the obstruction, we shall replace these corner points with the corner
			// vertices from the larger CEntityBoundAI which includes all components.

			const int iVertCornerIndex = CEntityBoundAI::GetCornerIndexFromBitset(iVertFlags);
			if(iVertCornerIndex != -1)
			{
				m_pRoute->Set(i, vExpandedCorners[iVertCornerIndex]);
			}
			const int iNextVertCornerIndex = CEntityBoundAI::GetCornerIndexFromBitset(iNextVertFlags);
			if(iNextVertCornerIndex != -1)
			{
				m_pRoute->Set(i+1, vExpandedCorners[iVertCornerIndex]);
			}
		}
	}

	//********************************************************************************************
	// Now check the end line segment, points (m_pRoute->GetSize()-2) to (m_pRoute->GetSize()-1).
	// By doing the *end* of the route first, we don't have to worry about inserting items into
	// the m_iRouteSideFlags array - the start of the route will still be ok even if we've fucked
	// about with the end of it.

	const Vector3 & vEndVert0 = m_pRoute->Get( m_pRoute->GetSize()-2 );
	const Vector3 & vEndVert1 = m_pRoute->Get( m_pRoute->GetSize()-1 );

	const bool bEndSegHits = DoesRouteSegHitEntity(vEndVert0, vEndVert1, PED_HUMAN_RADIUS, m_pVehicle, bUseMultipleCapsules, bUseOnlyBoundingBoxes, m_iDoorPedIsHeadingFor);

	if(bEndSegHits)
	{
		//***********************************************************************
		// if vEndVert0 is a corner vertex, replace it with an expanded corner

		const u32 iEndVert0Flags = m_iRouteSideFlags[ m_pRoute->GetSize()-2 ];
		const int iVertCornerIndex = CEntityBoundAI::GetCornerIndexFromBitset(iEndVert0Flags);
		if(iVertCornerIndex != -1)
		{
			m_pRoute->Set(m_pRoute->GetSize()-2, vExpandedCorners[iVertCornerIndex]);
		}

		//*********************************************************************************************************
		// if vEndVert1 lies within the expanded volume - then insert a new vertex between vEndVert0 and vEndVert1
		// If the vEndVert1 was along the side of the original bound, then the new vertex should be the vEndVert1,
		// but moved outside of the expanded bound along the normal of the appropriate side plane.

		if(boundWithComponents.LiesInside(vEndVert1))
		{
			const u32 iPlaneFlags = m_AiBound.ComputePlaneFlags(vEndVert1);
			const s32 iSide = CEntityBoundAI::GetSideIndexFromBitset(iPlaneFlags);
			if(iSide != -1)
			{
				Vector3 vNewEndVert;
				boundWithComponents.MoveOutside(vEndVert1, vNewEndVert, iSide);
				m_pRoute->Insert(m_pRoute->GetSize()-1, vNewEndVert);
			}
		}
	}

	//*********************************************************************
	// Now check the start line segment, points (0) to (1).

	if(m_pRoute->GetSize() > 2)
	{
		const Vector3 & vStartVert0 = m_pRoute->Get( 0 );
		const Vector3 & vStartVert1 = m_pRoute->Get( 1 );

		const bool bStartSegHits = DoesRouteSegHitEntity(vStartVert0, vStartVert1, PED_HUMAN_RADIUS, m_pVehicle, bUseMultipleCapsules, bUseOnlyBoundingBoxes, m_iDoorPedIsHeadingFor);

		if(bStartSegHits)
		{
			//***********************************************************************
			// if vStartVert1 is a corner vertex, replace it with an expanded corner

			const u32 iStartVert1Flags = m_iRouteSideFlags[ 1 ];
			const int iVertCornerIndex = CEntityBoundAI::GetCornerIndexFromBitset(iStartVert1Flags);
			if(iVertCornerIndex != -1)
			{
				m_pRoute->Set( 1, vExpandedCorners[iVertCornerIndex]);
			}

			//***************************************************************************************************************
			// if vStartVert0 lies within the expanded volume - then insert a new vertex between vStartVert0 and vStartVert1
			// If the vStartVert0 was along the side of the original bound, then the new vertex should be the vStartVert0,
			// but moved outside of the expanded bound along the normal of the appropriate side plane.

			if(boundWithComponents.LiesInside(vStartVert0))
			{
				const u32 iPlaneFlags = m_AiBound.ComputePlaneFlags(vStartVert0);
				const s32 iSide = CEntityBoundAI::GetSideIndexFromBitset(iPlaneFlags);
				if(iSide != -1)
				{
					Vector3 vNewStartVert;
					boundWithComponents.MoveOutside(vStartVert0, vNewStartVert, iSide);
					m_pRoute->Insert(1, vNewStartVert);
				}
			}
		}
	}
}







