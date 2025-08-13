 
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    Cover.cpp
// PURPOSE : Everything having to do with the taking cover and finding places to do it.
// AUTHOR :  Obbe
// CREATED : 19-2-04
//
/////////////////////////////////////////////////////////////////////////////////

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwmaths/vector.h"
#include "fwscript/scriptguid.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "Control/Replay/Replay.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/debugscene.h"
#include "ModelInfo/Vehiclemodelinfo.h"
#include "Objects/CoverTuning.h"
#include "Objects/Object.h"
#include "Pathserver/Pathserver.h"
#include "PedGroup/Formation.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "peds/ped.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Scene/Building.h"
#include "scene/world/GameWorld.h"
#include "script/script_channel.h"
#include "script/script_itemsets.h"
#include "Task/Combat/TaskCombat.h"
#include "Task\Combat\Cover\Cover.h"
#include "Task/Combat/Cover/coverfilter.h"
#include "Task/Combat/Cover\coversupport.h"
#include "Task\Combat\Cover\TaskCover.h"
#include "Task/Default/TaskPlayer.h"
#include "Task\Movement\TaskMoveWithinAttackWindow.h"
#include "Vehicles/Automobile.h"
#include "Vfx/Misc/Fire.h"

//rage
#include "fragment/instance.h" //required to get def of class fragInst
#include "profile/profiler.h"

namespace AICombatStats
{
	EXT_PF_TIMER(CoverUpdateTotal);
	EXT_PF_TIMER(CoverUpdateArea);
	EXT_PF_TIMER(CoverUpdateMap);
	EXT_PF_TIMER(CoverUpdateMoveAndRemove);
	EXT_PF_TIMER(CoverUpdateObjects);
	EXT_PF_TIMER(CoverUpdateStatus);
	EXT_PF_TIMER(CoverUpdateVehicles);
	EXT_PF_TIMER(CalculateVantagePointForCoverPoint);
	EXT_PF_TIMER(FindClosestCoverPoint);
	EXT_PF_TIMER(FindClosestCoverPointWithCB);
	EXT_PF_TIMER(FindCoverPointInArc);
	EXT_PF_TIMER(FindCoverPoints);
	EXT_PF_TIMER(FindCoverPointsWithinRange);
}
using namespace AICombatStats;

AI_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

#if AI_OPTIMISATIONS_OFF && __DEV
#define DEBUG_GRIDCELLS		// this double-checks that coverpoints exist in their correct gridcells
#endif

atBitSet				CCover::m_UsedCoverPoints;
CCoverPoint				CCover::m_aPoints[NUM_COVERPOINTS];
s32						CCover::m_NumPoints						= 0;
s16						CCover::m_aMinFreeSlot[CP_NumPools];
fwPtrListDoubleLink		CCover::m_ListOfProcessedBuildings;
s32 					CCover::LastTimeRecorded				= 0;
s32 					CCover::SlowestTimeRecorded				= 0;

const u32			CCover::ms_iAddFromMapOperationFrequencyMS = 150;
const u32			CCover::ms_iAddFromObjectsOperationFrequencyMS = 33;
const u32			CCover::ms_iAddFromVehiclesOperationFrequencyMS = 33;

// We want to check for coverpoint removal at least once per second.
// This means that depending on the total number of coverpoints, we have the following formula:
// ms_iCoverPointRemovalFrequency * NUM_COVERPOINTS = ms_iNumRemovalChecksPerFrame * desiredUpdateRatePerCoverPointMS
// units are milliseconds and coverpoints
const u32			CCover::ms_iCoverPointRemovalFrequency	= 40; // 40 * 2624 = 105 * 1000
const u32			CCover::ms_iNumRemovalChecksPerFrame = 105; // 40 * 2624 = 105 * 1000

u32					CCover::ms_iLastRemovalStopIndex = 0;

u32					CCover::ms_iNextAddCoverPointsFromMapTime	= 0;
u32					CCover::ms_iNextAddCoverPointsFromObjectsTime	= 0;
u32					CCover::ms_iNextAddCoverPointsFromVehiclesTime	= 0;
u32					CCover::ms_iLastTimeCoverPointsRemoved	= 0;
bool					CCover::ms_bForceUpdateAll				= true;

#if !__FINAL && !__PPU
float					CCover::ms_fMsecsTakenToRemoveCoverPoints	= 0;
float					CCover::ms_fMsecsTakenToAddFromMap			= 0;
float					CCover::ms_fMsecsTakenToAddFromVehicles		= 0;
float					CCover::ms_fMsecsTakenToAddFromObjects		= 0;
sysPerformanceTimer	*	CCover::ms_pPerfTimer						= NULL;
#endif

#if __BANK
bool					CCover::ms_bUseDebugCameraAsCoverOrigin		= false;
#endif

const float				CCover::ms_fMaxSpeedSqrForCarToBeCover	= (4.0f * 4.0f);
const float				CCover::ms_fMaxSpeedSqrForCarToBeLowCover	= (4.0f * 4.0f);
const float				CCover::ms_fMaxDistSqrCoverPointCanMove = (0.125f * 0.125f);
const float				CCover::ms_fMaxSpeedSqrForObjectToBeCover = (2.0f * 2.0f);


const float				CCover::ms_fMaxCoverPointDistance	= 75.0f;
const float				CCover::ms_fMaxCoverPointDistanceSqr	= (ms_fMaxCoverPointDistance * ms_fMaxCoverPointDistance);

// Variables used to search for cover points around peds and form bounding boxes
CCover::eBoundingAreaOp  CCover::ms_eBoundingAreaOperation		= CCover::ENoBoundingAreaOp;
const float				CCover::ms_fMaxPointDistanceSearchXY	= 15.0f;
const float				CCover::ms_fMaxPointDistanceSearchZ		= 5.0f;
const float				CCover::ms_fMinPointDistanceSearchXY	= 5.0f;
const float				CCover::ms_fMinPointDistanceSearchZ		= 5.0f;
float					CCover::ms_fCoverPointDistanceSearchXY	= CCover::ms_fMaxPointDistanceSearchXY;
float					CCover::ms_fCoverPointDistanceSearchZ	= CCover::ms_fMaxPointDistanceSearchZ;

const float				CCover::ms_fMaxCoverBoundingBoxXY		= 25.0f;
const float				CCover::ms_fMaxCoverBoundingBoxZ		= 10.0f;
const float				CCover::ms_fMinCoverBoundingBoxXY		= 5.0f;
const float				CCover::ms_fMinCoverBoundingBoxZ		= 5.0f;
float					CCover::ms_fCurrentCoverBoundingBoxXY	= CCover::ms_fMaxCoverBoundingBoxXY;
float					CCover::ms_fCurrentCoverBoundingBoxZ	= CCover::ms_fMaxCoverBoundingBoxZ;

const float				CCover::ms_fStepSizeForDecreasingBoundingBoxes	= 5.0f;
const float				CCover::ms_fStepSizeForIncreasingBoundingBoxes	= 1.0f;

int						CCover::ms_iCurrentNumMapAndDynamicCoverPts		= 0;
int						CCover::ms_iCurrentNumScriptedCoverPts	= 0;

s32						CCover::ms_iLastStatusCheckIndex		= 0;
s32						CCover::ms_iNumStatusUpdatesPerFrame	= 15;

u32						CCover::ms_iLastAddFromObjectStopIndex		= 0;
dev_u32					CCover::ms_iNumAddFromObjectChecksPerFrame	= 32;
dev_u32					CCover::ms_iNumActualAddsFromObjectsPerFrame = 8;

u32						CCover::ms_iLastAddFromVehicleStopIndex		= 0;
dev_u32					CCover::ms_iNumAddFromVehicleChecksPerFrame	= 32;
dev_u32					CCover::ms_iNumActualAddsFromVehiclesPerFrame = 8;

dev_u32					CCover::ms_iNumActualAddsFromMapPerFrame = 32;

const float				CCover::ms_fMinDistBetweenCoverPoints		= 0.5f;
const float				CCover::ms_fMinDistSqrBetweenCoverPoints	= CCover::ms_fMinDistBetweenCoverPoints * CCover::ms_fMinDistBetweenCoverPoints;

const float				CCover::ms_fMinDistBetweenObjectCoverPoints		= 0.05f;
const float				CCover::ms_fMinDistSqrBetweenObjectCoverPoints	= CCover::ms_fMinDistBetweenObjectCoverPoints * CCover::ms_fMinDistBetweenObjectCoverPoints;

const float				CCoverPointsGridCell::ms_fSizeOfGrid		= 32.0f;
static const s32		iNumGridsInMaxRadius						= (s32) ((CCover::ms_fMaxCoverPointDistance / CCoverPointsGridCell::ms_fSizeOfGrid) + 0.5f) + 1; // round up & then add one
const int				CCoverPointsContainer::ms_iMaxNumberOfGrids = (iNumGridsInMaxRadius*2)+(iNumGridsInMaxRadius*2)+(iNumGridsInMaxRadius*2);
atArray<CCoverPointsGridCell*> CCoverPointsContainer::ms_CoverPointGrids;

// Cover bounding area static variables
CCoverBoundingArea	CCover::ms_boundingAreas[MAX_BOUNDING_AREAS];
u32				CCover::ms_iNumberAreas = 0;

// Areas set by script to block the generation of coverpoints
atFixedArray<CCoverBlockingBoundingArea, CCover::MAX_BOUNDING_AREAS>	CCover::ms_blockingBoundingAreas;
CCoverBlockingBoundingArea  CCover::ms_hardCodedBlockingBoundingAreas[MAX_HARD_CODED_AREAS];
u32						CCover::ms_iNumberHardCodedBlockingAreas = 0;

dev_float		ZUPFOROBJECTTOBEUPRIGHT					= 0.9f;
dev_float		COVERPOINT_LOS_Z_ELEVATION				= 1.65f;

Vector3				CCover::ms_vPlayerCoverExtents( ms_fCoverPointDistanceSearchXY, ms_fCoverPointDistanceSearchXY, ms_fCoverPointDistanceSearchZ );
Vector3				CCover::ms_vAiCoverExtents( ms_fCoverPointDistanceSearchXY, ms_fCoverPointDistanceSearchXY, ms_fCoverPointDistanceSearchZ);
Vector3				CCover::ms_vMaxCoverExtents( ms_fMaxCoverBoundingBoxXY, ms_fMaxCoverBoundingBoxXY, ms_fMaxCoverBoundingBoxZ);

INSTANTIATE_RTTI_CLASS(CScriptedCoverPoint,0x5C79CB42);

CScriptedCoverPoint::CScriptedCoverPoint(s32 iScriptIndex)
: m_iScriptIndex(iScriptIndex)
{
}

CScriptedCoverPoint::~CScriptedCoverPoint()
{

}

//-------------------------------------------------------------------------
// Simple struct to define the type and size of the individual allocated pools of
// cover, used to split up the main cover array into separate pools
//-------------------------------------------------------------------------
SCoverPoolSize CCover::m_aCoverPoolSizes[CP_NumPools] =
{
	{ CP_NavmeshAndDynamicPoints, NUM_NAVMESH_COVERPOINTS  },
	{ CP_SciptedPoint, NUM_SCRIPTED_COVERPOINTS }
};

SCachedScriptedCoverArea CCover::m_sCachedScriptedCoverArea;

const Vec3V CCoverPointsGridCell::ms_vPadMin(-SMALL_FLOAT, -SMALL_FLOAT, -SMALL_FLOAT);

// Note: the +1 on Z is here because in FindCoverPointInArc(), we actually add 1 before doing the test
// against the arc, so we need to make the box large enough to account for that. We don't do the same on
// the minimum vector, because not all users of the box may want to include that offset.
const Vec3V CCoverPointsGridCell::ms_vPadMax(SMALL_FLOAT, SMALL_FLOAT, SMALL_FLOAT + 1.0f);


void CCoverPointsGridCell::InitCellAroundPos(Vec3V_ConstRef pos)
{
	// If there is some point in this cell when we initialize it, it's quite
	// likely that something went wrong.
	Assert(!m_pFirstCoverPoint);

	float fX = (pos.GetXf() / CCoverPointsGridCell::ms_fSizeOfGrid);
	float fY = (pos.GetYf() / CCoverPointsGridCell::ms_fSizeOfGrid);
	float fZ = (pos.GetZf() / CCoverPointsGridCell::ms_fSizeOfGrid);

	s32 iX = (fX >= 0.0f) ? ((s32)fX) : ((s32)(fX-1.0f));
	s32 iY = (fY >= 0.0f) ? ((s32)fY) : ((s32)(fY-1.0f));
	s32 iZ = (fZ >= 0.0f) ? ((s32)fZ) : ((s32)(fZ-1.0f));

	const float x = ((float) iX) * CCoverPointsGridCell::ms_fSizeOfGrid;
	const float y = ((float) iY) * CCoverPointsGridCell::ms_fSizeOfGrid;
	const float z = ((float) iZ) * CCoverPointsGridCell::ms_fSizeOfGrid;
	m_vBoundingBoxForCellMin = Vec3V(x, y, z);

	const Vec3V deltaV = Vec3V(ScalarV(CCoverPointsGridCell::ms_fSizeOfGrid));
	m_vBoundingBoxForCellMax = Add(m_vBoundingBoxForCellMin, deltaV);

	Assert(IsGreaterThanOrEqualAll(pos, m_vBoundingBoxForCellMin));
	Assert(IsLessThanOrEqualAll(pos, m_vBoundingBoxForCellMax));
}


void CCoverPointsGridCell::ClearBoundingBoxForContents()
{
	m_vBoundingBoxForContentsMin.ZeroComponents();
	m_vBoundingBoxForContentsMax.ZeroComponents();
	m_bBoundingBoxForContentsExists = false;
}


void CCoverPointsGridCell::RecomputeBoundingBoxForContents()
{
	// If we have any points, loop over them, find all positions, and update the min
	// and max vectors.
	if(m_pFirstCoverPoint)
	{
		Vec3V bndMaxV(V_NEG_FLT_MAX);
		Vec3V bndMinV(V_FLT_MAX);

		for(const CCoverPoint* pt = m_pFirstCoverPoint; pt; pt = pt->m_pNextCoverPoint)
		{
			Vec3V posV;
			pt->GetCoverPointPosition(RC_VECTOR3(posV));

			bndMaxV = Max(bndMaxV, posV);
			bndMinV = Min(bndMinV, posV);
		}

		// Include some extra padding, and store the result.
		m_vBoundingBoxForContentsMax = Add(bndMaxV, ms_vPadMax);
		m_vBoundingBoxForContentsMin = Add(bndMinV, ms_vPadMin);
		m_bBoundingBoxForContentsExists = true;
	}
	else
	{
		// No points, no box.
		ClearBoundingBoxForContents();
	}
}


void CCoverPointsGridCell::GrowBoundingBoxForAddedPoint(Vec3V_In posV)
{
	// Compute a miniature bounding box by adding the padding space to the position.
	const Vec3V posMinV = Add(posV, ms_vPadMin);
	const Vec3V posMaxV = Add(posV, ms_vPadMax);

	// Merge in with the bounding box around the rest of the contents, if any.
	if(m_bBoundingBoxForContentsExists)
	{
		m_vBoundingBoxForContentsMin = Min(posMinV, m_vBoundingBoxForContentsMin);
		m_vBoundingBoxForContentsMax = Max(posMaxV, m_vBoundingBoxForContentsMax);
	}
	else
	{
		// No box before, make one for this point.
		m_vBoundingBoxForContentsMin = posMinV;
		m_vBoundingBoxForContentsMax = posMaxV;
		m_bBoundingBoxForContentsExists = true;
	}
}


void
CCoverPointsContainer::Init(void)
{
	Clear();
}

void
CCoverPointsContainer::Shutdown(void)
{
	Clear();
}

void
CCoverPointsContainer::Clear(void)
{
	for(int i=0; i<ms_CoverPointGrids.GetCount(); i++)
	{
		CCoverPointsGridCell * pGridCell = ms_CoverPointGrids[i];
		Assert(pGridCell);
		delete pGridCell;
	}

	ms_CoverPointGrids.Reset();
	ms_CoverPointGrids.Reserve(ms_iMaxNumberOfGrids);
}

//**********************************************************************************************
//	CheckForOverlapAndGetGridCell
//
//	This function fulfills two purposes :
//	1)	It checks all the other gridcells intersecting with the vWorldPos of the new coverpoint,
//		and decides whether there will be conflict (if this is a scripted coverpoint then any
//		existing ones will be removed to make way for it.
//	2)	If the coverpoint can be added without conflict, it returns the gridcell in which the
//		coverpoint should be placed.
//**********************************************************************************************

CCoverPointsGridCell *
CCoverPointsContainer::CheckIfNoOverlapAndGetGridCell(const Vector3 & vWorldPos, CCoverPoint::eCoverType Type, CEntity *pEntity, CCoverPoint*& pOverlappingCoverPoint, bool bAssertOnOverlap )
{
	Vector3 vExtraSize(CCover::ms_fMinDistBetweenCoverPoints, CCover::ms_fMinDistBetweenCoverPoints, CCover::ms_fMinDistBetweenCoverPoints);
	Vector3 vCoverPointMins = vWorldPos - vExtraSize;
	Vector3 vCoverPointMaxs = vWorldPos + vExtraSize;

	Vector3 vExistingGlobal;

	CCoverPointsGridCell * pGridCell;
	CCoverPointsGridCell * pOwnerGridCell = NULL;
	CCoverPointsGridCell * pFirstEmptyGridCell = NULL;

	pOverlappingCoverPoint = NULL;

	for(int i=0; i<ms_CoverPointGrids.GetCount(); i++)
	{
		pGridCell = ms_CoverPointGrids[i];
		Assert(pGridCell);

		// Keep a record of the first unused gridcell we come across
		if(!pFirstEmptyGridCell && !pGridCell->m_pFirstCoverPoint)
			pFirstEmptyGridCell = pGridCell;

		// Is there any overlap between the bbox of the gridcell, and the expanded bbox of the cover position ?
		if(!pGridCell->DoesAABBOverlapCell(VECTOR3_TO_VEC3V(vCoverPointMins), VECTOR3_TO_VEC3V(vCoverPointMaxs)))
		{
			continue;
		}

		// If the cover vertex position is actually within this grid, then make a note of the grid
		if(pGridCell->IsPointWithinCellExclMax(VECTOR3_TO_VEC3V(vWorldPos)))
		{
			pOwnerGridCell = pGridCell;
		}

		// Now we'll check whether there are any conflict with any of the coverpoints in this gridcell
		CCoverPoint * pCoverPoint = pGridCell->m_pFirstCoverPoint;
		while(pCoverPoint)
		{
			const bool bBothPointsMapOrScript = ( pCoverPoint->GetType() == CCoverPoint::COVTYPE_SCRIPTED || pCoverPoint->GetType() == CCoverPoint::COVTYPE_POINTONMAP ) &&
												( Type == CCoverPoint::COVTYPE_SCRIPTED || Type == CCoverPoint::COVTYPE_POINTONMAP );
			// Only check scripted coverpoints and map coverpoints for overlap
			// Or identical other points
			if( !( Type == pCoverPoint->GetType() && pEntity == pCoverPoint->m_pEntity ) &&
				 !bBothPointsMapOrScript )
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			// Check distance
			if(!pCoverPoint->GetCoverPointPosition(vExistingGlobal))
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			// work out how close the coverpoints can be together dependant on the 
			float fMinDistSqr = CCover::ms_fMinDistSqrBetweenCoverPoints;
			if( pCoverPoint->GetType() == CCoverPoint::COVTYPE_BIG_OBJECT ||
				pCoverPoint->GetType() == CCoverPoint::COVTYPE_VEHICLE )
			{
				fMinDistSqr = CCover::ms_fMinDistSqrBetweenObjectCoverPoints;
			}

			if((vWorldPos - vExistingGlobal).Mag2() >= fMinDistSqr)
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			// Points could overlap, decide what to do depending on types
			// Scripted override all but scripted, so remove the existing point
			if( Type == CCoverPoint::COVTYPE_SCRIPTED &&
				(pCoverPoint->GetType() == CCoverPoint::COVTYPE_POINTONMAP || pCoverPoint->GetFlag(CCoverPoint::COVFLAGS_WaitingForCleanup)) )
			{
				CCoverPoint * pCoverPointToRemove = pCoverPoint;
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				CCover::RemoveCoverPoint(pCoverPointToRemove);
				continue;
			}

			if (bAssertOnOverlap)
			{
				Assertf( Type != CCoverPoint::COVTYPE_SCRIPTED || pCoverPoint->GetType() != CCoverPoint::COVTYPE_SCRIPTED, "%s: Scripted cover point being added ontop of another scripted coverpoint!", CTheScripts::GetCurrentScriptNameAndProgramCounter() );
			}

			pOverlappingCoverPoint = pCoverPoint;

			// IFF we've reached here the points must overlap one another, so return NULL
			return NULL;
		}
	}

	if(!pOwnerGridCell)
	{
		// Can we reuse this empty grid cell ?
		if(pFirstEmptyGridCell)
		{
			pOwnerGridCell = pFirstEmptyGridCell;
		}
		// Otherwise we'll have to allocate a new grid cell
		else
		{
			pOwnerGridCell = rage_new CCoverPointsGridCell();

			if(ms_CoverPointGrids.GetCount() >= ms_CoverPointGrids.GetCapacity())
			{
				ms_CoverPointGrids.Grow() = pOwnerGridCell;
			}
			else
			{
				ms_CoverPointGrids.Append() = pOwnerGridCell;
			}
		}

		// Set up this grid cell with the appropriate min/max for this cover point
		pOwnerGridCell->InitCellAroundPos(VECTOR3_TO_VEC3V(vWorldPos));

#ifdef DEBUG_GRIDCELLS
		if(!pOwnerGridCell->IsPointWithinCellExclMax(VECTOR3_TO_VEC3V(vWorldPos)))
		{
			printf("CCoverPointsContainer::CheckIfNoOverlapAndGetGridCell()\n");
			printf("CoverPoint position was outside of pOwnerGridCell after the new grid was initialised.\n");
			printf("vWorldPos = (%.2f, %.2f, %.2f)\n", vWorldPos.x, vWorldPos.y, vWorldPos.z);
			printf("Grid Mins = (%.2f, %.2f, %.2f), Grid Maxs = (%.2f, %.2f, %.2f)\n",
				pOwnerGridCell->m_vBoundingBoxForCellMin.GetXf(), pOwnerGridCell->m_vBoundingBoxForCellMin.GetYf(), pOwnerGridCell->m_vBoundingBoxForCellMin.GetZf(),
				pOwnerGridCell->m_vBoundingBoxForCellMax.GetXf(), pOwnerGridCell->m_vBoundingBoxForCellMax.GetYf(), pOwnerGridCell->m_vBoundingBoxForCellMax.GetZf());

			// Disabled this, it's not easily accessible now when InitCellAroundPos() does the math.
			//	printf("iX = %i, iY = %i, iZ = %i\n", iX, iY, iZ);

			Warningf("Error adding cover-point to grid cell.  PLEASE INCLUDE DEBUG SPEW FROM THE CONSOLE WINDOW WITH BUG REPORT.");
		}
#endif
	}

	return pOwnerGridCell;
}

CCoverPointsGridCell *
CCoverPointsContainer::MoveCoverPointToAnotherGridCellIfRequired(CCoverPoint * pCoverPoint)
{
	Assert(pCoverPoint->m_pOwningGridCell);

	Vector3 vCoverPointPos;
	pCoverPoint->GetCoverPointPosition(vCoverPointPos);

	Assert(pCoverPoint->GetCoverPointPosition(vCoverPointPos));

	CCoverPointsGridCell* pOldCell = pCoverPoint->m_pOwningGridCell;
	if(!pOldCell->IsPointWithinCellExclMax(VECTOR3_TO_VEC3V(vCoverPointPos)))
	{
		CCoverPointsGridCell * pNewGridCell = GetGridCellForCoverPoint(vCoverPointPos);
		Assert(pNewGridCell);

		if(pCoverPoint->m_pNextCoverPoint)
			pCoverPoint->m_pNextCoverPoint->m_pPrevCoverPoint = pCoverPoint->m_pPrevCoverPoint;
		if(pCoverPoint->m_pPrevCoverPoint)
			pCoverPoint->m_pPrevCoverPoint->m_pNextCoverPoint = pCoverPoint->m_pNextCoverPoint;
		Assert(pCoverPoint->m_pOwningGridCell);
		if(pOldCell && pOldCell->m_pFirstCoverPoint == pCoverPoint)
		{
			pOldCell->m_pFirstCoverPoint = pCoverPoint->m_pNextCoverPoint;

			// Clear the old's cell's bounding box around the contents, if this point was
			// the last one in the cell.
			if(!pCoverPoint->m_pNextCoverPoint)
			{
				pOldCell->ClearBoundingBoxForContents();
			}
		}

		pCoverPoint->m_pOwningGridCell = pNewGridCell;
		pCoverPoint->m_pPrevCoverPoint = NULL;
		pCoverPoint->m_pNextCoverPoint = pNewGridCell->m_pFirstCoverPoint;
		pNewGridCell->m_pFirstCoverPoint = pCoverPoint;
		if(pCoverPoint->m_pNextCoverPoint)
			pCoverPoint->m_pNextCoverPoint->m_pPrevCoverPoint = pCoverPoint;

		// Expand the bounding box in the cell we moved to.
		pNewGridCell->GrowBoundingBoxForAddedPoint(VECTOR3_TO_VEC3V(vCoverPointPos));

		// Note: we could recompute the bounding box of the cell we moved from, or
		// come up with a way to make it as dirty to recompute later (in case multiple points move).
		// Not sure that it's worth the effort, though. For now, all we do is clear the bounding
		// box if the cell is now empty (see above).
	}

	return pCoverPoint->m_pOwningGridCell;
}


CCoverPointsGridCell *
CCoverPointsContainer::GetGridCellForCoverPoint(const Vector3 & vPosition)
{
	CCoverPointsGridCell * pGridCell;
	CCoverPointsGridCell * pFirstEmptyGridCell = NULL;

	const Vec3V posV = RCC_VEC3V(vPosition);

	for(int i=0; i<ms_CoverPointGrids.GetCount(); i++)
	{
		pGridCell = ms_CoverPointGrids[i];
		Assert(pGridCell);

		// Keep track of the first unused grid cell we might find
		if(!pFirstEmptyGridCell && !pGridCell->m_pFirstCoverPoint)
			pFirstEmptyGridCell = pGridCell;

		// Does this grid cell contain the position of the cover point?
		if(!pGridCell->IsPointWithinCellExclMax(posV))
		{
			continue;
		}

		return pGridCell;
	}

	CCoverPointsGridCell * pNewGridCell;

	// Can we reuse this empty grid cell ?
	if(pFirstEmptyGridCell)
	{
		pNewGridCell = pFirstEmptyGridCell;
	}
	// Otherwise we'll have to allocate a new grid cell
	else
	{
		pNewGridCell = rage_new CCoverPointsGridCell();

		if(ms_CoverPointGrids.GetCount() >= ms_CoverPointGrids.GetCapacity())
		{
			ms_CoverPointGrids.Grow() = pNewGridCell;
		}
		else
		{
			ms_CoverPointGrids.Append() = pNewGridCell;
		}
	}

	// Set up this grid cell with the appropriate min/max for this cover point
	// Note, before sharing this code with CheckIfNoOverlapAndGetGridCell(),
	// the rounding was done differently here, probably incorrectly:
	//	s32 iX = (s32) (vPosition.x / CCoverPointsGridCell::ms_fSizeOfGrid);
	//	s32 iY = (s32) (vPosition.y / CCoverPointsGridCell::ms_fSizeOfGrid);
	//	s32 iZ = (s32) (vPosition.z / CCoverPointsGridCell::ms_fSizeOfGrid);
	pNewGridCell->InitCellAroundPos(VECTOR3_TO_VEC3V(vPosition));

	return pNewGridCell;
}

bank_float CCoverSearchWithinRangeData::ms_fDefaultMaxSearchDist		= 5.0f;
bank_float CCoverSearchWithinRangeData::ms_fDefaultMinToThreatDist	= 5.0f;
bank_float CCoverSearchWithinRangeData::ms_fDefaultMaxToThreatDist	= 10.0f;


CCoverSearchWithinRangeData::CCoverSearchWithinRangeData()
: m_iSearchFlags(0)
, m_vRouteStartPos(Vector3::ZeroType)
, m_vSearchPos(Vector3::ZeroType)
, m_vAvoidancePos(Vector3::ZeroType)
, m_fMaxSearchDist(ms_fDefaultMaxSearchDist)
, m_fMinDistToAvoidancePos(ms_fDefaultMinToThreatDist)
, m_fMaxDistToAvoidancePos(ms_fDefaultMaxToThreatDist)
, m_apExceptions(NULL)
, m_iNumExceptions(0)
{

}

CCoverSearchWithinRangeData::CCoverSearchWithinRangeData(const CCoverSearchWithinRangeData& other)
: m_iSearchFlags(other.m_iSearchFlags)
, m_vRouteStartPos(other.m_vRouteStartPos)
, m_vSearchPos(other.m_vSearchPos)
, m_vAvoidancePos(other.m_vAvoidancePos)
, m_fMaxSearchDist(other.m_fMaxSearchDist)
, m_fMinDistToAvoidancePos(other.m_fMinDistToAvoidancePos)
, m_fMaxDistToAvoidancePos(other.m_fMaxDistToAvoidancePos)
, m_apExceptions(other.m_apExceptions)
, m_iNumExceptions(other.m_iNumExceptions)
{

}

CCoverSearchWithinRangeData::~CCoverSearchWithinRangeData()
{

}

CCoverSearchWithinRangeData& CCoverSearchWithinRangeData::operator=(const CCoverSearchWithinRangeData& src)
{
	From(src);
	return *this;
}


void CCoverSearchWithinRangeData::From(const CCoverSearchWithinRangeData& src)
{
	m_pPed				= src.m_pPed;
	m_iSearchFlags		= src.m_iSearchFlags;
	m_vSearchPos		= src.m_vSearchPos;
	m_vRouteStartPos	= src.m_vRouteStartPos;
	m_vAvoidancePos		= src.m_vAvoidancePos;
	m_fMaxSearchDist	= src.m_fMaxSearchDist;
	m_fMinDistToAvoidancePos	= src.m_fMinDistToAvoidancePos;
	m_fMaxDistToAvoidancePos	= src.m_fMaxDistToAvoidancePos;
	m_apExceptions		= src.m_apExceptions;
	m_iNumExceptions	= src.m_iNumExceptions;
}


//////////////////////////////////////////////////////
// FUNCTION: Init
// PURPOSE:  Initialises the cover points.
//////////////////////////////////////////////////////

void CCover::Init()
{
	m_NumPoints = 0;
	m_ListOfProcessedBuildings.Flush();

	m_UsedCoverPoints.Init(NUM_COVERPOINTS);
	m_UsedCoverPoints.Reset();

	// Initialize min free slots
	m_aMinFreeSlot[CP_NavmeshAndDynamicPoints] = 0; // front of the array
	m_aMinFreeSlot[CP_SciptedPoint] = m_aCoverPoolSizes[CP_NavmeshAndDynamicPoints].m_iSize; // end of first partition begins second partition

	memset(m_aPoints, 0, sizeof(CCoverPoint) * NUM_COVERPOINTS);

	ms_iNextAddCoverPointsFromMapTime = 0;
	ms_iNextAddCoverPointsFromObjectsTime = 0;
	ms_iNextAddCoverPointsFromVehiclesTime = 0;
	
	ms_iLastAddFromObjectStopIndex = 0;
	ms_iLastAddFromVehicleStopIndex = 0;
	ms_iLastRemovalStopIndex = 0;
	ms_iLastTimeCoverPointsRemoved	= 0;

	ms_iLastStatusCheckIndex = 0;

	ms_bForceUpdateAll				= true;

	ms_iNumberHardCodedBlockingAreas = 0;
	AddHardCodedCoverBlockingArea(Vector3(322.32f, 2878.75f, 42.10f), Vector3(323.78f, 2881.05f, 45.44f));
	AddHardCodedCoverBlockingArea(Vector3(-693.91f, -620.38f, 28.56f), Vector3(-692.95f, -619.19f, 32.67f));
	AddHardCodedCoverBlockingArea(Vector3(228.5f, -2006.0, 18.0f), Vector3(229.0f, -2007.0f, 19.0f));

#if !__FINAL && !__PPU
	ms_pPerfTimer = rage_new sysPerformanceTimer("CCover::Update() timer");
#endif

	ms_blockingBoundingAreas.Reset();

	// Allocate/initialise the coverpoints grid-cells
	CCoverPointsContainer::Init();
}

void CCover::InitSession(unsigned iMode)
{
	if(iMode == INIT_SESSION)
	{
		ms_iNextAddCoverPointsFromMapTime = 0;
		ms_iNextAddCoverPointsFromObjectsTime = 0;
		ms_iNextAddCoverPointsFromVehiclesTime = 0;
	}
}


#if __BANK
void CCover::InitWidgets()
{
	bkBank* pOptimisationsBank = BANKMGR.FindBank("Optimization");
	if(pOptimisationsBank)
		pOptimisationsBank->AddToggle("Use debug camera as cover origin", &CCover::ms_bUseDebugCameraAsCoverOrigin);

	// NB: All other cover widgets created via CCoverDebug
}
#endif

void CCover::Shutdown(void)
{
	m_ListOfProcessedBuildings.Flush();

	CCoverPointsContainer::Shutdown();

#if !__FINAL && !__PPU
	if(ms_pPerfTimer)
	{
		delete ms_pPerfTimer;
		ms_pPerfTimer = NULL;
	}
#endif

	m_UsedCoverPoints.Kill();
}

//#define SMALL_OBJECT_COVER_SIZE (1.0f)

bank_float DISTANCE_FROM_COVER_TO_OBJECT_HIGH = (0.30f);
bank_float DISTANCE_FROM_COVER_TO_OBJECT_LOW = (0.30f);
bank_float DISTANCE_FROM_COVER_TO_OBJECT_CORNER = (0.4f);

bank_float OBJECT_HIGH_WIDTH_FOR_SINGLE_END_CP = (0.8f);
bank_float OBJECT_WIDTH_FOR_SINGLE_END_CP = (1.0f);
bank_float MIN_OBJECT_WIDTH_FOR_SINGLE_END_CP = (0.35f);
bank_float MIN_OBJECT_HEIGHT_FOR_COVER = (0.65f);

bank_float OBJECT_WIDTH_FOR_CENTER_CP = (3.5f);

//-------------------------------------------------------------------------
// helper function for adding cover points from objects
//-------------------------------------------------------------------------
bool CCover::AddCoverPointFromObject( CObject* pObj )
{
	CoverOrientation eNewOrientation = CO_Invalid;

	if ( //pObj->GetC().z > 0.95f &&
		(pObj->GetVelocity().Mag2() < ms_fMaxSpeedSqrForObjectToBeCover) )
	{
		if (((CObject*)pObj)->CanBeUsedToTakeCoverBehind())
		{
			if( IsPointWithinValidArea( VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition()), CCoverPoint::COVTYPE_OBJECT ) )
			{
				eNewOrientation = CCachedObjectCoverManager::GetUpAxis(pObj);
				if( eNewOrientation == CO_XUp || eNewOrientation == CO_XDown )
				{
					const float	BoundingBoxX = pObj->GetBoundingBoxMax().x - pObj->GetBoundingBoxMin().x;
					if( ABS(BoundingBoxX) < MIN_OBJECT_COVER_BOUNDING_BOX_HEIGHT )
					{
						eNewOrientation = CO_Invalid;
					}
				}
				else if( eNewOrientation == CO_YUp || eNewOrientation == CO_YDown )
				{
					const float	BoundingBoxY = pObj->GetBoundingBoxMax().y - pObj->GetBoundingBoxMin().y;
					if( ABS(BoundingBoxY) < MIN_OBJECT_COVER_BOUNDING_BOX_HEIGHT )
					{
						eNewOrientation = CO_Invalid;
					}
				}
				// @TODO: Why not check if Z oriented and Z height is too small?
			}
		}
	}

	// Decide whether to try to add cover
	bool bShouldAddCover = false;

	// Check if the orientation has changed
	const CoverOrientation ePrevOrientation = pObj->GetCoverOrientation();
	if( eNewOrientation != ePrevOrientation )
	{
		bShouldAddCover = true;
	}

	// Check if the object had cover removed due to obstructions and time has expired
	if( pObj->HasExpiredObjectCoverObstructed() )
	{
		// Clear all of the flags obstructing cover
		pObj->ClearAllCachedObjectCoverObstructed();

		// Only add new coverpoints if there is no ped using cover on this entity
		// Otherwise they'll be forced out of cover when RemoveCoverPointsForThisEntity() is called below
		if(!IsAnyPedUsingCoverOnThisEntity(pObj))
			bShouldAddCover = true;
	}

	// If object needs cover
	if( bShouldAddCover )
	{
		// If previous orientation was invalid or dirty there is nothing to remove
		if(ePrevOrientation != CO_Invalid && ePrevOrientation != CO_Dirty)
		{
			// otherwise we will have to recompute, so remove existing
			RemoveCoverPointsForThisEntity(pObj);
		}

		pObj->SetCoverOrientation(eNewOrientation);

		if( eNewOrientation != CO_Invalid )
		{
			if(!CCachedObjectCoverManager::AddCoverpointsForPhysical( pObj ))
			{
				// If we get here, we were not entirely successful in adding cover.
				// Setting the cover orientation to "dirty" means that we will try again
				// at the next opportunity. A specific case that came up was that
				// GetCurrentPhysicsInst() returned NULL, presumably because something had
				// not been streamed in or initialized yet for this object.
				pObj->SetCoverOrientation(CO_Dirty);

				// remove cover until we try again
				RemoveCoverPointsForThisEntity(pObj);
				
				// did not add cover
				return false;
			}
			// we did add cover successfully
			return true;
		}
	}

	// did not add cover
	return false;
}

//-------------------------------------------------------------------------
// helper function for adding cover points from vehicles
//-------------------------------------------------------------------------
bool CCover::AddCoverPointFromVehicle( CVehicle* pVeh )
{
#if __DEV
	// Do not commit enabled!
	static bool s_bDebugAddCoverPointFromVehicleOutput = false;
	static CVehicle* s_pDebugVehicle = NULL;
#endif // __DEV

	// Check if the vehicle velocity is too high to be used as cover
	const float fVehicleSpeedSq = pVeh->GetVelocity().Mag2();
	//const float fMaxSpeedLimit = coverHeight == CCoverPoint::COVHEIGHT_TOOHIGH ? ms_fMaxSpeedSqrForCarToBeCover : ms_fMaxSpeedSqrForCarToBeLowCover;
	if( fVehicleSpeedSq > ms_fMaxSpeedSqrForCarToBeLowCover )
	{
		RemoveCoverPointsForVehicle(pVeh);

		// bail out as long as velocity remains high
		return false;
	}

	// If vehicle does not qualify for cover generation
	if ( !pVeh->InheritsFromAutomobile() ||
		pVeh->GetIsAircraft() ||
		!pVeh->m_nVehicleFlags.bDoesProvideCover )
	{
		RemoveCoverPointsForVehicle(pVeh);

		// no more to be done here
		return false;
	}

	// Get the vehicle matrix orientation
	CoverOrientation eCurrentOrientation = CCachedObjectCoverManager::GetUpAxis(pVeh);

	// If the orientation is invalid
	if( eCurrentOrientation == CO_Invalid )
	{
		RemoveCoverPointsForVehicle(pVeh);

		// mark orientation invalid
		pVeh->SetCoverOrientation(CO_Invalid);

		// no more to be done here
		return false;
	}

	// Get vehicle position
	const Vector3 vVehPosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());

	// If not in a valid area
	if( !IsPointWithinValidArea( vVehPosition, CCoverPoint::COVTYPE_VEHICLE ) )
	{
		RemoveCoverPointsForVehicle(pVeh);

		// no more to be done here
		return false;
	}

	//
	// At this point vehicle should generate cover: proceed
	//

	// Decide if we need to clear all existing cover. (default false)
	bool bClearExistingCover = false;

	// If the orientation has changed
	if( !bClearExistingCover && eCurrentOrientation != pVeh->GetCoverOrientation() )
	{
#if __DEV
		if(s_bDebugAddCoverPointFromVehicleOutput && pVeh == s_pDebugVehicle)
		{
			aiDebugf2("CCover::AddCoverPointFromVehicle(%p %s) needs recalculation: orientation changed from %d to %d",pVeh,pVeh->GetDebugName(),pVeh->GetCoverOrientation(), eCurrentOrientation);
		}
#endif // __DEV

		// mark for clear
		bClearExistingCover = true;
	}
			
	// If the position has changed
	if( !bClearExistingCover && vVehPosition.Dist2(pVeh->GetLastCoverPosition()) > 1.0f )
	{
#if __DEV
		if(s_bDebugAddCoverPointFromVehicleOutput && pVeh == s_pDebugVehicle)
		{
			aiDebugf2("CCover::AddCoverPointFromVehicle(%p %s) needs recalculation: position changed from [%.1f, %.1f, %.1f] to [%.1f,%.1f,%.1f]",
				pVeh,pVeh->GetDebugName(),pVeh->GetLastCoverPosition().x,pVeh->GetLastCoverPosition().y,pVeh->GetLastCoverPosition().z,vVehPosition.x,vVehPosition.y, vVehPosition.z);
		}
#endif // __DEV

		// mark for clear
		bClearExistingCover = true;
	}

	// Get vehicle forward direction
	const Vector3 vVehForward = VEC3V_TO_VECTOR3(pVeh->GetVehicleForwardDirection());

	// If the forward direction has changed
	if( !bClearExistingCover )
	{
		const float fAngleDelta = vVehForward.Angle(pVeh->GetLastCoverForwardDirection());
		const float fMaxDeltaDegrees = 4.5f;
		if( fAngleDelta > DtoR * fMaxDeltaDegrees )
		{
#if __DEV
			if(s_bDebugAddCoverPointFromVehicleOutput && pVeh == s_pDebugVehicle)
			{
				aiDebugf2("CCover::AddCoverPointFromVehicle(%p %s) needs recalculation: fwd dir changed %.2f degrees (max allowed:%.2f)", pVeh, pVeh->GetDebugName(), RtoD * fAngleDelta, fMaxDeltaDegrees);
			}
#endif // __DEV

			// mark for clear
			bClearExistingCover = true;
		}
	}

	// If any door has changed
	if( !bClearExistingCover )
	{
		if( ComputeHasAnyVehicleDoorChangedForCover(pVeh) )
		{
#if __DEV
			if(s_bDebugAddCoverPointFromVehicleOutput && pVeh == s_pDebugVehicle)
			{
				aiDebugf2("CCover::AddCoverPointFromVehicle(%p %s) needs recalculation: a door has changed", pVeh, pVeh->GetDebugName());
			}
#endif // __DEV
			
			// mark for clear
			bClearExistingCover = true;
		}
	}

	// If we need to clear all existing cover
	if( bClearExistingCover )
	{
		RemoveCoverPointsForVehicle(pVeh);
	}
	// otherwise, if vehicle already generated cover
	else if( pVeh->GetHasGeneratedCover() )
	{
		// no work to do here, exit
		return false;
	}

	// Keep track if all cover points were added (default to false)
	bool bCoverAddedSuccessfully = false;

	// If vehicle is upright
	if( CCachedObjectCoverManager::GetUpAxis(pVeh) == CO_ZUp )
	{
		// apply custom vehicle coverpoints
		bCoverAddedSuccessfully = HelperAddCoverForUprightVehicle(pVeh);
	}
	else // non-upright
	{
		// treat as a general object
		bCoverAddedSuccessfully = HelperAddCoverForNonUprightVehicle(pVeh);
	}

	// Record cover generation tracking values on the vehicle
	pVeh->SetCoverOrientation(eCurrentOrientation);
	pVeh->SetLastCoverPosition(vVehPosition);
	pVeh->SetLastCoverForwardDirection(vVehForward);

	// Record cover as successfully generated, unless some cover failed to be added.
	// This can happen if the cover pool is full right now.
	// Keep trying to generate cover for this vehicle until all cover is added.
	pVeh->SetHasGeneratedCover(bCoverAddedSuccessfully);

	// report whether cover was successfully generated
	return bCoverAddedSuccessfully;
}

bool CCover::HelperAddCoverForUprightVehicle( CVehicle* pVeh )
{
	// Keep track if any cover points fail to be added (default to false)
	bool bFailedToAddCoverPoint = false;

	// Get local handle to the vehicle model info
	CVehicleModelInfo* pModelInfo = pVeh->GetVehicleModelInfo();

	// Get the object cover tuning, which may be the default
	const CCoverTuning& coverTuning = CCoverTuningManager::GetInstance().GetTuningForModel(pModelInfo->GetHashKey());

	// Set conditions according to tuning settings.
	bool bSkipDoors = (coverTuning.m_Flags & CCoverTuning::NoCoverVehicleDoors) > 0;

	// Draws the results of the cover height check performed at load time.
	//pModelInfo->DebugDrawCollisionModel( pVeh->GetMatrix() );

	// loop through all doors, unless skipping
	for( s32 iDoor = 0; iDoor < MAX_NUM_COVER_DOORS && !bSkipDoors; iDoor++ )
	{
		// Get door ID from iterator index
		eHierarchyId doorID = HelperMapIndexToHierarchyID(iDoor);

		// skip if the door ID is invalid
		if(doorID == VEH_INVALID_ID)
		{
			continue;
		}

		// if the vehicle only has two doors
		if(pVeh->GetNumDoors() <= 2)
		{
			// skip rear doors, there aren't any
			if( doorID == VEH_DOOR_DSIDE_R || doorID == VEH_DOOR_PSIDE_R )
			{
				continue;
			}
		}

		// Get bone index for the current door, if any
		s32 doorBoneIdx = pVeh->GetBoneIndex(doorID);
		if(doorBoneIdx == -1)
		{
			// no bone index on this vehicle for the given door ID
			continue;
		}

		// get a handle to the current door, if any
		CCarDoor* pDoor = pVeh->GetDoorFromId(doorID);
		if( pDoor == NULL )
		{
			// no door object for this vehicle for the given door ID
			continue;
		}

		// store the door intact status and door ratio to detect cover point recalculation need
		pVeh->SetLastCoverDoorIntactStatus(iDoor, pDoor->GetIsIntact(pVeh));
		pVeh->SetLastCoverDoorRatio(iDoor, pDoor->GetDoorRatio());

		// if any door is intact and considered closed
		if( pDoor->GetIsIntact(pVeh) && pDoor->GetDoorRatio() < MIN_RATIO_FOR_OPEN_DOOR_COVER )
		{
			// check if the vehicle height is low enough to shoot over
			// The height map indices for the middle of the car are 1 and 4
			if( pModelInfo->GetHeightMapHeight(1) < HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES && pModelInfo->GetHeightMapHeight(4) < HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES )
			{
				// Try to place a closed door cover point facing over the body of the vehicle
				s16 iCoverIndex = HelperAddCoverPointClosedDoor(pVeh, doorID, doorBoneIdx);

				// If the cover was not successfully placed
				if( iCoverIndex == INVALID_COVER_POINT_INDEX )
				{
					// mark the failure so that we can keep trying
					bFailedToAddCoverPoint = true;
				}
			}
		}
		// otherwise, if this is a forward door
		else if( doorID == VEH_DOOR_DSIDE_F || doorID == VEH_DOOR_PSIDE_F )
		{
			// if the door is intact and considered open
			if( pDoor->GetIsIntact(pVeh) && pDoor->GetDoorRatio() >= MIN_RATIO_FOR_OPEN_DOOR_COVER )
			{
				// Try to place an open door cover point
				s16 iCoverIndex = HelperAddCoverPointOpenForwardDoor(pVeh, doorID, doorBoneIdx);

				// If the cover was not successfully placed
				if( iCoverIndex == INVALID_COVER_POINT_INDEX )
				{
					// mark the failure so that we can keep trying
					bFailedToAddCoverPoint = true;
				}
			}
		}
	}// end loop through doors

	// Generate points according to model info
	for( s32 i = 0; i < pModelInfo->GetNumCoverPoints(); i++ )
	{
		CCoverPointInfo *				pCoverPointInfo	= pModelInfo->GetCoverPoint(i);
		const CCoverPoint::eCoverUsage	coverUsage		= (CCoverPoint::eCoverUsage) pCoverPointInfo->m_iUsage;
		const CCoverPoint::eCoverHeight	coverHeight		= (CCoverPoint::eCoverHeight)pCoverPointInfo->m_iCoverHeight;
		const CCoverPoint::eCoverArc	coverArc		= (CCoverPoint::eCoverArc)pCoverPointInfo->m_iCoverArc;
		const u8 						iLocalDir		= pCoverPointInfo->m_iLocalDir;
		const bool						bIsPriority		= false;

		// Try to place model info cover point
		s16 iCoverIndex = AddCoverPoint(CCover::CP_NavmeshAndDynamicPoints, CCoverPoint::COVTYPE_VEHICLE, pVeh, &pCoverPointInfo->m_vLocalOffset, coverHeight, coverUsage, iLocalDir, coverArc, bIsPriority );

		// If the cover was not successfully placed
		if( iCoverIndex == INVALID_COVER_POINT_INDEX )
		{
			// mark the failure so that we can keep trying
			bFailedToAddCoverPoint = true;
		}
	}

	// report whether cover was successfully generated
	return !bFailedToAddCoverPoint;
}

bool CCover::HelperAddCoverForNonUprightVehicle( CVehicle* pVeh )
{
	// clear any object obstruction flags, as this is only called when the physical is disturbed
	CPhysical* pPhysical = (CPhysical*)pVeh;
	pPhysical->ClearAllCachedObjectCoverObstructed();

	// treat the vehicle as a general dynamic object
	return CCachedObjectCoverManager::AddCoverpointsForPhysical( pVeh );
}

/* static inline */
void CCover::RemoveCoverPointsForVehicle( CVehicle* pVeh )
{
	// remove any previous cover points
	if( pVeh && pVeh->GetHasGeneratedCover() )
	{
		RemoveCoverPointsForThisEntity(pVeh);
		pVeh->SetHasGeneratedCover(false);
	}
}

/* static inline */
s16 CCover::HelperAddCoverPointOpenForwardDoor( CVehicle* pVeh, eHierarchyId doorID, int doorBoneIndex )
{
	aiAssert(doorID == VEH_DOOR_DSIDE_F || doorID == VEH_DOOR_PSIDE_F);

	// Open door configuration
	const CCoverPoint::eCoverUsage	coverUsage		= CCoverPoint::COVUSE_WALLTOBOTH;
	const CCoverPoint::eCoverHeight	coverHeight		= CCoverPoint::COVHEIGHT_HIGH;
	const CCoverPoint::eCoverArc	coverArc		= CCoverPoint::COVARC_90;
	const u8	 					iLocalDir		= 0; // forward
	const bool						bIsPriority		= false;

	// Work out a local position for the open door cover points
	Vector3 vDoorPos = pVeh->GetLocalMtx(doorBoneIndex).d;
	static float YDIST_FROM_LATCH = 1.1f;
	static float XDIST_FROM_LATCH = 0.5f;
	vDoorPos.y -= YDIST_FROM_LATCH;
	vDoorPos.x += (doorID==VEH_DOOR_DSIDE_F?-XDIST_FROM_LATCH:XDIST_FROM_LATCH);
	vDoorPos.z = pVeh->GetVehicleModelInfo()->GetBoundingBoxMin().z;

	// Add in the cover point
	s16 iCoverIndex = AddCoverPoint(CCover::CP_NavmeshAndDynamicPoints, CCoverPoint::COVTYPE_VEHICLE, pVeh, &vDoorPos, coverHeight, coverUsage, iLocalDir, coverArc, bIsPriority, doorID);

	// If a cover point was added
	if( iCoverIndex != INVALID_COVER_POINT_INDEX )
	{
		//Grab the cover point.
		CCoverPoint* pCoverPoint = CCover::FindCoverPointWithIndex(iCoverIndex);
		if(pCoverPoint)
		{
			//Set the transient flag.
			pCoverPoint->SetFlag(CCoverPoint::COVFLAGS_Transient);
		}
	}

	return iCoverIndex;
}

/* static inline */
s16 CCover::HelperAddCoverPointClosedDoor( CVehicle* pVeh, eHierarchyId doorID, int doorBoneIndex )
{
	// Closed door configuration
	// local direction is dir * ((2 * PI)/COVER_POINT_DIR_RANGE) (see FindVectorFromDir)
	static u8 iLocalDirR = (u8)rage::round(COVER_POINT_DIR_RANGE * 0.75f);// 270 degrees from forward
	static u8 iLocalDirL = (u8)rage::round(COVER_POINT_DIR_RANGE * 0.25f);// 90 degrees from forward
	const CCoverPoint::eCoverUsage	coverUsage		= CCoverPoint::COVUSE_WALLTOBOTH;
	const CCoverPoint::eCoverHeight	coverHeight		= CCoverPoint::COVHEIGHT_HIGH;
	const CCoverPoint::eCoverArc	coverArc		= CCoverPoint::COVARC_120;
	const bool						bIsPriority		= false;
	      u8	 					iLocalDir		= iLocalDirR; // default to face right over vehicle body
	// if door is on passenger side (right side)
	if( doorID == VEH_DOOR_PSIDE_F || doorID == VEH_DOOR_PSIDE_R )
	{
		// cover will face left over vehicle body
		iLocalDir = iLocalDirL;
	}

	// Work out a local position for the closed door cover point
	Vector3 vDoorPos = pVeh->GetLocalMtx(doorBoneIndex).d;
	static float YDIST_FROM_LATCH = 0.5f;
	static float XDIST_FROM_LATCH = 0.4f;
	vDoorPos.y -= YDIST_FROM_LATCH;
	vDoorPos.z = pVeh->GetVehicleModelInfo()->GetBoundingBoxMin().z;
	// if door is on passenger side (positive local x)
	if( doorID == VEH_DOOR_PSIDE_F || doorID == VEH_DOOR_PSIDE_R )
	{
		vDoorPos.x += XDIST_FROM_LATCH;
	}
	else // driver side (negative local x)
	{
		vDoorPos.x += -XDIST_FROM_LATCH;
	}

	// Add in the cover point
	s16 iCoverIndex = AddCoverPoint(CCover::CP_NavmeshAndDynamicPoints, CCoverPoint::COVTYPE_VEHICLE, pVeh, &vDoorPos, coverHeight, coverUsage, iLocalDir, coverArc, bIsPriority);

	return iCoverIndex;
}

/* static inline */
bool CCover::ComputeHasAnyVehicleDoorChangedForCover( CVehicle* pVeh )
{
	// If a door has changed intact status or ratio (amount of open/closed)
	for(int iDoor=0; iDoor < MAX_NUM_COVER_DOORS; iDoor++)
	{
		if( ComputeHasDoorChangedForCover(pVeh, iDoor) )
		{
			// change detected
			return true;
		}
	}

	// default no change
	return false;
}

bool CCover::ComputeHasDoorChangedForCover( CVehicle* pVeh, int iDoorIndex )
{
	// Get door ID from iterator index
	eHierarchyId doorID = HelperMapIndexToHierarchyID(iDoorIndex);

	// skip if doorID is invalid
	if( doorID == VEH_INVALID_ID )
	{
		return false;
	}

	// if the vehicle only has two doors
	if(pVeh->GetNumDoors() <= 2)
	{
		// no change for non-existent rear doors
		if( doorID == VEH_DOOR_DSIDE_R || doorID == VEH_DOOR_PSIDE_R )
		{
			return false;
		}
	}

	// get a handle to the current door, if any
	CCarDoor* pDoor = pVeh->GetDoorFromId(doorID);
	if( pDoor == NULL )
	{
		// no door object for this vehicle for the given door ID
		return false;
	}

	// check for a door intactness change
	if( pDoor->GetIsIntact(pVeh) != pVeh->GetLastCoverDoorIntactStatus(iDoorIndex) )
	{
		return true;
	}

	// get stored door ratio
	const float fLastCoverDoorRatio = pVeh->GetLastCoverDoorRatio(iDoorIndex);

	// check for a previously closed door being opened
	if( fLastCoverDoorRatio < MIN_RATIO_FOR_OPEN_DOOR_COVER && pDoor->GetDoorRatio() >= MIN_RATIO_FOR_OPEN_DOOR_COVER )
	{
		return true;
	}

	// check for a previously open door being closed
	if( fLastCoverDoorRatio >= MIN_RATIO_FOR_OPEN_DOOR_COVER && pDoor->GetDoorRatio() < MIN_RATIO_FOR_OPEN_DOOR_COVER )
	{
		return true;
	}

	// default no change
	return false;
}

/* static inline */
eHierarchyId CCover::HelperMapIndexToHierarchyID(int index)
{
	if( index == 0 )
	{
		// driver side front
		return VEH_DOOR_DSIDE_F;
	}
	if( index == 1 )
	{
		// driver side rear
		return VEH_DOOR_DSIDE_R;
	}
	if( index == 2 )
	{
		// passenger side front
		return VEH_DOOR_PSIDE_F;
	}
	if( index == 3 )
	{
		// passenger side rear
		return VEH_DOOR_PSIDE_R;
	}
	
	Assertf(index >= 0 && index < 4, "Only 4 doors are supported for coverpoint generation, invalid index %d", index);
	return VEH_INVALID_ID;
}

//////////////////////////////////////////////////////
// FUNCTION: Update
// PURPOSE:  Called once a frame. Tidys up the coverpoints that aren't valid anymore.
//////////////////////////////////////////////////////

void CCover::Update()
{
	// Don't update CCover when the game is paused
	if(fwTimer::IsGamePaused())
		return;

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif

	if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning()) return;		// Don't mess with the coverpoints if a replay is going on.

	PF_FUNC(CoverUpdateTotal);

	PF_START(CoverUpdateArea);

	//****************************************
	// Update the valid cover bounding areas

	if(ms_eBoundingAreaOperation==EReduceBoundingArea)
	{
		bool bCouldReduce = ReduceBoundingAreaSize(ms_fStepSizeForDecreasingBoundingBoxes);
		if(bCouldReduce)
			ms_bForceUpdateAll = true;
	}
	else
	{
		// Have the cover search extents been reduced at all ?
		if(ms_fCurrentCoverBoundingBoxXY < ms_fMaxCoverBoundingBoxXY ||
			ms_fCurrentCoverBoundingBoxZ < ms_fMaxCoverBoundingBoxZ ||
			ms_fCoverPointDistanceSearchXY < ms_fMaxPointDistanceSearchXY ||
			ms_fCoverPointDistanceSearchZ < ms_fMaxPointDistanceSearchZ)
		{
			// If none of the pools are overcomited anymore, then we can maybe extend them again
			// static const float fOverComitRatio = 0.75f;
			if(!IsCoverPoolOverComitted(CP_NavmeshAndDynamicPoints, 0.75f))
			{
				// Expand the bounding areas within which we create cover-points
				bool bCouldIncrease = IncreaseBoundingAreaSize(ms_fStepSizeForIncreasingBoundingBoxes);
				if(bCouldIncrease)
					ms_bForceUpdateAll = true;
			}
		}
	}

	ms_eBoundingAreaOperation = ENoBoundingAreaOp;

	GenerateCoverAreaBoundingBoxes();

	PF_STOP(CoverUpdateArea);

	//*********************************************************************************
	// Figure out which operations we're going to do this frame.
	// We will periodically perform operations, unless
	// the "ms_bForceUpdateAll" flag has been set - in which case we will perform all
	// processing steps this frame (eg. for use when the player has been teleported).
	//*********************************************************************************

	// Get the current time
	u32 iTimeMs = fwTimer::GetTimeInMilliseconds();

	// We will perform remove operations periodically
	bool bRemoveCoverpoints = false;
	u32 iDeltaRemovalTime = iTimeMs - ms_iLastTimeCoverPointsRemoved;
	if(iDeltaRemovalTime > ms_iCoverPointRemovalFrequency)
	{
		bRemoveCoverpoints = true;
		ms_iLastTimeCoverPointsRemoved = iTimeMs;
	}

	// We will perform one type of coverpoint add operation at a time
	// according to their scheduled times, with an ordered check as follows
	bool bUpdateCoverpoints = false;
	eCoverUpdateOp updateOp = ENoOperation;
	if( iTimeMs >= ms_iNextAddCoverPointsFromMapTime 
		&& ms_iNextAddCoverPointsFromMapTime < ms_iNextAddCoverPointsFromVehiclesTime 
		&& ms_iNextAddCoverPointsFromMapTime < ms_iNextAddCoverPointsFromObjectsTime )
	{
		bUpdateCoverpoints = true;
		updateOp = EAddFromMap;
		ms_iNextAddCoverPointsFromMapTime = iTimeMs + ms_iAddFromMapOperationFrequencyMS;
	}
	else if( iTimeMs >= ms_iNextAddCoverPointsFromVehiclesTime 
		&& ms_iNextAddCoverPointsFromVehiclesTime < ms_iNextAddCoverPointsFromObjectsTime )
	{
		bUpdateCoverpoints = true;
		updateOp = EAddFromVehicles;
		// NOTE: intentionally do not set ms_iNextAddCoverPointsFromVehiclesTime here
	}
	else if( iTimeMs >= ms_iNextAddCoverPointsFromObjectsTime )
	{
		bUpdateCoverpoints = true;
		updateOp = EAddFromObjects;
		// NOTE: intentionally do not set ms_iNextAddCoverPointsFromObjectsTime here
	}
	if(!bUpdateCoverpoints && !bRemoveCoverpoints && !ms_bForceUpdateAll)
	{
		return;
	}

	//************************************************************************
	// Evict coverpoints which are out of range, or otherwise invalidated
	//************************************************************************

	PF_START(CoverUpdateMoveAndRemove);
	if(bRemoveCoverpoints || ms_bForceUpdateAll)
	{
#if !__FINAL && !__PPU
		ms_pPerfTimer->Reset();
		ms_pPerfTimer->Start();
#endif

		RemoveAndMoveCoverPoints();

#if !__FINAL && !__PPU
		ms_pPerfTimer->Stop();
		ms_fMsecsTakenToRemoveCoverPoints = (float)ms_pPerfTimer->GetTimeMS();
#endif

	}
	PF_STOP(CoverUpdateMoveAndRemove);


	//***********************************
	// Add coverpoints from vehicles
	//***********************************

	if(updateOp == EAddFromVehicles || ms_bForceUpdateAll)
	{
		PF_AUTO_PUSH_TIMEBAR("CoverUpdateVehicles");
#if !__FINAL && !__PPU
		ms_pPerfTimer->Reset();
		ms_pPerfTimer->Start();
#endif
		PF_START(CoverUpdateVehicles);

		CVehicle::Pool *VehiclePool = CVehicle::GetPool();
		CVehicle* pVeh;
		const u32 NUM_VEHICLES = (u32) VehiclePool->GetSize();

		// start at the index we left off on and haven't checked yet
		u32 iStartIndex = ms_iLastAddFromVehicleStopIndex;

		// stop at the index according to the number of checks per operation, wrapping around if necessary
		Assert(ms_iNumAddFromVehicleChecksPerFrame > 0);
		Assert(ms_iNumAddFromVehicleChecksPerFrame <= NUM_VEHICLES);
		u32 iStopIndex = ms_iLastAddFromVehicleStopIndex + ms_iNumAddFromVehicleChecksPerFrame;
		if( iStopIndex >= NUM_VEHICLES )
		{
			iStopIndex -= NUM_VEHICLES;
		}
		
		// Count the number of successful adds as these are more expensive
		int numVehiclesCoverAdded = 0;

		// Traverse the object list from the start index to the stop index, wrapping around if necessary
		u32 VehIndex = iStartIndex;
		for(; VehIndex != iStopIndex; VehIndex == NUM_VEHICLES-1 ? VehIndex = 0 : VehIndex++)
		{
			pVeh = VehiclePool->GetSlot(VehIndex);

			if( pVeh )
			{
				if( AddCoverPointFromVehicle( pVeh ) )
				{
					numVehiclesCoverAdded++;
				}
			}

			// if we have reached the limit of more expensive adds this frame
			if( numVehiclesCoverAdded >= ms_iNumActualAddsFromVehiclesPerFrame )
			{
				break;
			}
		}

		// mark the last vehicle not checked
		ms_iLastAddFromVehicleStopIndex = VehIndex;

		// If we were not limited this frame
		if( numVehiclesCoverAdded < ms_iNumActualAddsFromVehiclesPerFrame )
		{
			// set the next operation time
			ms_iNextAddCoverPointsFromVehiclesTime = iTimeMs + ms_iAddFromVehiclesOperationFrequencyMS;
		}

		PF_STOP(CoverUpdateVehicles);

#if !__FINAL && !__PPU
		ms_pPerfTimer->Stop();
		ms_fMsecsTakenToAddFromVehicles = (float)ms_pPerfTimer->GetTimeMS();
#endif
	}

	//***********************************
	// Add coverpoints from objects
	//***********************************

	if(updateOp == EAddFromObjects || ms_bForceUpdateAll)
	{
		PF_AUTO_PUSH_TIMEBAR("CoverUpdateObjects");
#if !__FINAL && !__PPU
		ms_pPerfTimer->Reset();
		ms_pPerfTimer->Start();
#endif
		PF_START(CoverUpdateObjects);

		// We will allow the peds to hide behind certain objects. Trees, fire hydrants etc
		CObject::Pool *ObjectPool = CObject::GetPool();
		const u32 NUM_OBJECTS = (u32) ObjectPool->GetSize();

		// start at the index we left off on and haven't checked yet
		u32 iStartIndex = ms_iLastAddFromObjectStopIndex;

		// stop at the index according to the number of checks per operation, wrapping around if necessary
		Assert(ms_iNumAddFromObjectChecksPerFrame > 0);
		Assert(ms_iNumAddFromObjectChecksPerFrame <= NUM_OBJECTS);
		u32 iStopIndex = ms_iLastAddFromObjectStopIndex + ms_iNumAddFromObjectChecksPerFrame;
		if( iStopIndex >= NUM_OBJECTS )
		{
			iStopIndex -= NUM_OBJECTS;
		}

		// Count the number of successful adds as these are more expensive
		int numObjectsCoverAdded = 0;

		// Traverse the object list from the start index to the stop index, wrapping around if necessary
		u32 ObjIndex = iStartIndex;
		for(; ObjIndex != iStopIndex; ObjIndex == NUM_OBJECTS-1 ? ObjIndex = 0 : ObjIndex++)
		{
			// Get the object in this slot
			CObject* pObj = ObjectPool->GetSlot(ObjIndex);

			if( pObj )
			{
				CBaseModelInfo * pModelInfo = pObj->GetBaseModelInfo();
				bool bCreatedByScript = (pObj->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT);
				bool bAlreadyHasCoverInNavMesh = (pModelInfo->GetIsFixed() || pModelInfo->GetIsFixedForNavigation()) && !bCreatedByScript;

				// Don't add coverpoints for objects which don't provide cover
				// Don't add coverpoints for objects which are fixed or fixed-for-navigation - their coverpoints will be in the navmesh
				if((!pModelInfo->GetDoesNotProvideAICover())
					&& (!bAlreadyHasCoverInNavMesh)
					&& (!pModelInfo->GetIsLadder())
					&& (!pModelInfo->GetUsesDoorPhysics()))
				{
					if( AddCoverPointFromObject(pObj) )
					{
						numObjectsCoverAdded++;
					}
				}
			}

			// if we have reached the limit of more expensive adds this frame
			if( numObjectsCoverAdded >= ms_iNumActualAddsFromObjectsPerFrame )
			{
				break;
			}
		}

		// mark the last index not checked
		ms_iLastAddFromObjectStopIndex = ObjIndex;

		// if we did not hit the add limit this operation
		if( numObjectsCoverAdded < ms_iNumActualAddsFromObjectsPerFrame )
		{
			// mark the next update time
			ms_iNextAddCoverPointsFromObjectsTime = iTimeMs + ms_iAddFromObjectsOperationFrequencyMS;
		}

		PF_STOP(CoverUpdateObjects);

#if !__FINAL && !__PPU
		ms_pPerfTimer->Stop();
		ms_fMsecsTakenToAddFromObjects = (float)ms_pPerfTimer->GetTimeMS();
#endif
	}

	//***********************************
	// Add coverpoints from map
	//***********************************

	if(updateOp == EAddFromMap || ms_bForceUpdateAll)
	{
		PF_AUTO_PUSH_TIMEBAR("CoverUpdateMap");
#if !__FINAL && !__PPU
		ms_pPerfTimer->Reset();
		ms_pPerfTimer->Start();
#endif

		PF_START(CoverUpdateMap);
		CPathServer::AddCoverPoints(CGameWorld::FindLocalPlayerCoors(), ms_fMaxCoverPointDistance, ms_iNumActualAddsFromMapPerFrame);
		PF_STOP(CoverUpdateMap);

#if !__FINAL && !__PPU
		ms_pPerfTimer->Stop();
		ms_fMsecsTakenToAddFromMap = (float)ms_pPerfTimer->GetTimeMS();
#endif
	}

	PF_START(CoverUpdateStatus);
	// Update the status of a number of coverpoints per update
	for (s32 C = 0; C < CCover::ms_iNumStatusUpdatesPerFrame; C++)
	{
		++ms_iLastStatusCheckIndex;
		if( ms_iLastStatusCheckIndex >= NUM_COVERPOINTS )
		{
			ms_iLastStatusCheckIndex -= NUM_COVERPOINTS;
		}

		// Reset the cover point status (if it's not valid then it's up to the using code to use the helper to check it)
		m_aPoints[ms_iLastStatusCheckIndex].ResetCoverPointStatus();
	}
	PF_STOP(CoverUpdateStatus);

	// Clear the force update all flag
	ms_bForceUpdateAll = false;

	return;
}


void
CCover::RemoveAndMoveCoverPoints()
{
	// start at the index we left off on and haven't checked yet
	u32 iStartIndex = ms_iLastRemovalStopIndex;
	if( iStartIndex >=  NUM_COVERPOINTS )
	{
		iStartIndex -= NUM_COVERPOINTS;
	}

	// stop at the index according to the number of checks per operation, wrapping around if necessary
	Assert(ms_iNumRemovalChecksPerFrame > 0);
	Assert(ms_iNumRemovalChecksPerFrame <= NUM_COVERPOINTS);
	u32 iStopIndex = (ms_iLastRemovalStopIndex + ms_iNumRemovalChecksPerFrame);
	if( iStopIndex >=  NUM_COVERPOINTS )
	{
		iStopIndex -= NUM_COVERPOINTS;
	}

	// mark the last index we are not going to check
	ms_iLastRemovalStopIndex = iStopIndex;

	// Traverse the cover list from the start index to the end index, wrapping around if necessary
	for(u32 C = iStartIndex; C != iStopIndex; C == NUM_COVERPOINTS-1 ? C = 0 : C++)
	{
		if(!m_UsedCoverPoints.IsSet(C))
		{
			Assert(m_aPoints[C].GetType()==CCoverPoint::COVTYPE_NONE);
			continue;
		}

		switch(m_aPoints[C].GetType())
		{
			case CCoverPoint::COVTYPE_VEHICLE:
			{
				const float fMaxSpeedLimit = m_aPoints[C].GetHeight() == CCoverPoint::COVHEIGHT_TOOHIGH ? ms_fMaxSpeedSqrForCarToBeCover : ms_fMaxSpeedSqrForCarToBeLowCover;

				bool bRemoved = false;
				if(!m_aPoints[C].m_pEntity)
				{	
					// We've lost the thing to hide behind.
					RemoveCoverPoint(&m_aPoints[C]);
					bRemoved = true;
				}
				// Only remove if out of range if not in use
				else if(!m_aPoints[C].IsOccupied() &&
					!IsPointWithinValidArea(VEC3V_TO_VECTOR3(m_aPoints[C].m_pEntity->GetTransform().GetPosition()), m_aPoints[C].GetType() ) )
				{	
					SetEntityCoverOrientationDirty(&m_aPoints[C]);
					RemoveCoverPoint(&m_aPoints[C]);
					bRemoved = true;
				}
				else if( ((CPhysical *)(m_aPoints[C].m_pEntity.Get()))->GetVelocity().Mag2() > fMaxSpeedLimit )
				{
					SetEntityCoverOrientationDirty(&m_aPoints[C]);
					RemoveCoverPoint(&m_aPoints[C]);
					bRemoved = true;
				}
				else if( CCachedObjectCoverManager::GetUpAxis(((CPhysical *)m_aPoints[C].m_pEntity.Get())) == CO_Invalid )
				{
					SetEntityCoverOrientationDirty(&m_aPoints[C]);
					RemoveCoverPoint(&m_aPoints[C]);
					bRemoved = true;
				}

				Assert(m_NumPoints >= 0);
				// Let's check that this coverpoint is still within the gridcell it inhabits.
				// The vehicle may have moved, and caused the coverpoint to leave the cell.
				if(!bRemoved)
				{
					CCoverPointsContainer::MoveCoverPointToAnotherGridCellIfRequired(&m_aPoints[C]);
				}
				break;
			}	
			case CCoverPoint::COVTYPE_BIG_OBJECT:
			case CCoverPoint::COVTYPE_OBJECT:
			{
				CObject* pObj = (CObject*)m_aPoints[C].m_pEntity.Get();
				bool bRemoved = false;
				if(!pObj)
				{	
					// We've lost the thing to hide behind.
					RemoveCoverPoint(&m_aPoints[C]);
					bRemoved = true;
				}
				// Only remove if out of range if not in use
				else if(!m_aPoints[C].IsOccupied() &&
					!IsPointWithinValidArea(VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition()), m_aPoints[C].GetType() ))
				{
					// If the object is now out of range, the cover point has gone
					SetEntityCoverOrientationDirty(&m_aPoints[C]);
					RemoveCoverPoint(&m_aPoints[C]);
					bRemoved = true;
				}
				else if( CCachedObjectCoverManager::GetUpAxis(pObj) == CO_Invalid )
				{
					SetEntityCoverOrientationDirty(&m_aPoints[C]);
					RemoveCoverPoint(&m_aPoints[C]);
					bRemoved = true;
				}
				else if(pObj->GetVelocity().Mag2() >= ms_fMaxSpeedSqrForObjectToBeCover )
				{
					SetEntityCoverOrientationDirty(&m_aPoints[C]);
					RemoveCoverPoint(&m_aPoints[C]);
					bRemoved = true;
				}
				Assert(m_NumPoints >= 0);
				// Let's check that this coverpoint is still within the gridcell it inhabits.
				// The object may have moved, and caused the coverpoint to leave the cell.
				if(!bRemoved)
				{
					CCoverPointsContainer::MoveCoverPointToAnotherGridCellIfRequired(&m_aPoints[C]);
				}
				// Check if the cover position is blocked
				if( !bRemoved && (m_aPoints[C].GetStatus() & CCoverPoint::COVSTATUS_PositionBlocked) > 0 )
				{
					// Get the physical entity
					CPhysical* pPhysical = (CPhysical*)m_aPoints[C].m_pEntity.Get();
					// Get the stored index of the cached object cover definition
					int cachedObjectCoverIndex = m_aPoints[C].GetCachedObjectCoverIndex();
					// Validate physical handle and index range
					if( pPhysical && cachedObjectCoverIndex >= 0 && cachedObjectCoverIndex < CCachedObjectCover::MAX_COVERPOINTS_PER_OBJECT )
					{
						// mark the physical so that we do not re-add a coverpoint
						// for the corresponding object cover definition index
						pPhysical->SetCachedObjectCoverObstructed(cachedObjectCoverIndex);
					}

					// remove this cover point
					RemoveCoverPoint(&m_aPoints[C]);
					bRemoved = true;
				}
				// Check for broken cover object
				if( !bRemoved && pObj && pObj->GetFragInst() && pObj->GetFragInst()->GetPartBroken() )
				{
					// Check if the new broken piece cannot be used as cover
					if( !pObj->CanBeUsedToTakeCoverBehind() )
					{
						// remove this cover point
						RemoveCoverPoint(&m_aPoints[C]);
						bRemoved = true;
					}
				}
				break;
			}
			case CCoverPoint::COVTYPE_POINTONMAP:
			// Only remove if not in use
			if( !m_aPoints[C].IsOccupied() )
			{
				// Out of range
				Vector3 vCoors;
				m_aPoints[C].GetCoverPointPosition(vCoors);
				if( !IsPointWithinValidArea( vCoors, m_aPoints[C].GetType() ) )
				{
					RemoveCoverPoint(&m_aPoints[C]);
					Assert(m_NumPoints >= 0);
				}
			}
			break;

		case CCoverPoint::COVTYPE_SCRIPTED:
			// If waiting to be cleaned up and not used, then we can remove it
			if( !m_aPoints[C].IsOccupied() && m_aPoints[C].GetFlag(CCoverPoint::COVFLAGS_WaitingForCleanup) )
			{
				RemoveCoverPoint(&m_aPoints[C]);
			}
			break;
		default:
			Assert(0);
			break;		
		}
	}
}


//////////////////////////////////////////////////////
// FUNCTION: SetEntityCoverOrientationDirty
// PURPOSE:  Checks if cover point has a qualifying entity
//			 and marks the entity for cover analysis
//////////////////////////////////////////////////////

void CCover::SetEntityCoverOrientationDirty(CCoverPoint* pPoint)
{
	// If coverpoint has a physical entity, set cover orientation to CO_Dirty so that the object
	// will be considered for runtime cover generation in AddCoverPointFromObject or AddCoverPointFromVehicle in the future
	if( pPoint && pPoint->m_pEntity && pPoint->m_pEntity->GetIsPhysical() )
	{
		if(pPoint->m_pEntity->GetIsTypeObject())
		{
			static_cast<CObject*>(pPoint->m_pEntity.Get())->SetCoverOrientation(CO_Dirty);
		}
		if(pPoint->m_pEntity->GetIsTypeVehicle())
		{
			static_cast<CVehicle*>(pPoint->m_pEntity.Get())->SetCoverOrientation(CO_Dirty);
		}
	}
}


//////////////////////////////////////////////////////
// FUNCTION: AdjustMapCoverPoint
// PURPOSE:  Adjusts the arc and direction of the cover point 
//			 depending on the direction covered
//////////////////////////////////////////////////////

void CCover::AdjustMapCoverPoint( CCoverPoint* pPoint )
{
	if( pPoint->GetType() != CCoverPoint::COVTYPE_POINTONMAP )
	{
		return;
	}

	// If shots can be fired over this cover point, no need to adjust the direction
	if( pPoint->GetHeight() != CCoverPoint::COVHEIGHT_TOOHIGH && !pPoint->GetFlag(CCoverPoint::COVFLAGS_IsLowCorner) )
	{
		return;
	}

	// Wall to the left, protected arc is CCW from forward
	if( pPoint->GetUsage() == CCoverPoint::COVUSE_WALLTOLEFT )
	{
		if( pPoint->GetArc() == CCoverPoint::COVARC_45 )
		{
			pPoint->SetArc( CCoverPoint::COVARC_0TO22 );
		}
		else if( pPoint->GetArc() == CCoverPoint::COVARC_90 )
		{
			pPoint->SetArc( CCoverPoint::COVARC_0TO45 );
		}
		else if( pPoint->GetArc() == CCoverPoint::COVARC_120 )
		{
			pPoint->SetArc( CCoverPoint::COVARC_0TO60 );
		}
	}
	// Wall to the right, protected arc is CW from forward
	else if( pPoint->GetUsage() == CCoverPoint::COVUSE_WALLTORIGHT )
	{
		if( pPoint->GetArc() == CCoverPoint::COVARC_45 )
		{
			pPoint->SetArc( CCoverPoint::COVARC_338TO0 );
		}
		else if( pPoint->GetArc() == CCoverPoint::COVARC_90 )
		{
			pPoint->SetArc( CCoverPoint::COVARC_315TO0 );
		}
		else if( pPoint->GetArc() == CCoverPoint::COVARC_120 )
		{
			pPoint->SetArc( CCoverPoint::COVARC_300TO0 );
		}
	}
	else if( pPoint->GetUsage() == CCoverPoint::COVUSE_WALLTOBOTH )
	{
		pPoint->SetArc( CCoverPoint::COVARC_180 );
	}
}


//-------------------------------------------------------------------------
// Calculates the cover pool type extents
//-------------------------------------------------------------------------
bool CCover::CalculateCoverPoolExtents( CoverPoolType CoverPool, s16 &iPoolMin, s16 &iPoolMax )
{
	// Initialize out parameters
	iPoolMin = 0;
	iPoolMax = NUM_COVERPOINTS;

	// First find the range for the pool this coverpoint has been allocated in
	for( s16 iPool = 0; iPool < CP_NumPools; iPool++ )
	{
		// Once the pool is found, continue to find the maximum size (the min pool value is now correct)
		// Note if it is the last pool in the array this will be the last run through but this is OK
		// because iPoolMax is already set to the maximum array bound
		if( m_aCoverPoolSizes[iPool].m_iCoverPoolType == CoverPool )
		{
			iPoolMax = iPoolMin + m_aCoverPoolSizes[iPool].m_iSize;
			return true;
		}
		// Continue to search for the pool remembering the current allocation
		else
		{
			iPoolMin = iPoolMin + m_aCoverPoolSizes[iPool].m_iSize;
		}
	}
	return false;
}

//-------------------------------------------------------------------------
// Fetch the next available slot for the given cover pool
//-------------------------------------------------------------------------
bool CCover::GetNextAvailableSlot( CoverPoolType CoverPool, s16& out_iAvailableSlot )
{
	if( AssertVerify(IsCoverPoolTypeValid(CoverPool)) )
	{
		// Check if the pool is full
		if( m_aMinFreeSlot[CoverPool] < 0)
		{
			// no available slot
			return false;
		}
		else
		{
			// Use the stored min free slot for the cover pool
			Assert( m_UsedCoverPoints.IsClear(m_aMinFreeSlot[CoverPool]) );
			out_iAvailableSlot = m_aMinFreeSlot[CoverPool];
			return true;
		}
	}

	// no available slot
	return false;
}

//-------------------------------------------------------------------------
// Modify static data to account for new slot in use
//-------------------------------------------------------------------------
void CCover::SetSlotInUse( CoverPoolType CoverPool, s16 iSlotInUse )
{
	// Mark the bit for the new slot in the bitset.
	Assert(m_UsedCoverPoints.IsClear(iSlotInUse));
	m_UsedCoverPoints.Set(iSlotInUse, true);

	// Increment the static count
	m_NumPoints++;
	Assert(m_NumPoints <= NUM_COVERPOINTS);

	// Keep a track of how many of each type of coverpoint are in existence
	switch(CoverPool)
	{
	case CP_NavmeshAndDynamicPoints:
		ms_iCurrentNumMapAndDynamicCoverPts++;
		Assert(ms_iCurrentNumMapAndDynamicCoverPts <= NUM_NAVMESH_COVERPOINTS);
		break;
	case CP_SciptedPoint:
		ms_iCurrentNumScriptedCoverPts++;
		Assert(ms_iCurrentNumScriptedCoverPts <= NUM_SCRIPTED_COVERPOINTS);
		break;
	default:
		Assert(0);
	}

	// Check for a valid CoverPool type
	// Check that we are adding at the current minimum free slot
	if( AssertVerify(IsCoverPoolTypeValid(CoverPool)) && AssertVerify(m_aMinFreeSlot[CoverPool] == iSlotInUse) )
	{
		s16 iPoolMin = 0;
		s16 iPoolMax = NUM_COVERPOINTS;
		if( AssertVerify(CalculateCoverPoolExtents(CoverPool, iPoolMin, iPoolMax)) )
		{
			// Start at the next higher slot index
			s16 nextFreeSlotInPool = iSlotInUse + 1;
			Assert(nextFreeSlotInPool > iPoolMin);

			// Increment the index until we find an available slot
			while(nextFreeSlotInPool < iPoolMax && m_UsedCoverPoints.IsSet(nextFreeSlotInPool) )
			{
				nextFreeSlotInPool++;
			}

			// Check if we have run out of slots for this pool
			if( nextFreeSlotInPool >= iPoolMax )
			{
				// Set the min free slot to a negative value to indicate full pool
				m_aMinFreeSlot[CoverPool] = -1;

				// Get out of this method, no more to be done here
				return;
			}

			// set the min free slot to the available slot we found
			m_aMinFreeSlot[CoverPool] = nextFreeSlotInPool;
		}
	}
}

//-------------------------------------------------------------------------
// Modify static data to account for new available slot
//-------------------------------------------------------------------------
void CCover::SetSlotAvailable( CoverPoolType CoverPool, s16 iAvailableSlot )
{
	// Clear the bit for the given coverpoint in the bitset
	Assert(m_UsedCoverPoints.IsSet(iAvailableSlot));
	m_UsedCoverPoints.Clear(iAvailableSlot);

	// Decrement the static count
	m_NumPoints--;
	Assert(m_NumPoints >= 0);

	// Keep a track of how many of each type of coverpoint are in existence
	switch(CoverPool)
	{
	case CP_NavmeshAndDynamicPoints:
		ms_iCurrentNumMapAndDynamicCoverPts--;
		Assert(ms_iCurrentNumMapAndDynamicCoverPts >= 0);
		break;
	case CP_SciptedPoint:
		ms_iCurrentNumScriptedCoverPts--;
		Assert(ms_iCurrentNumScriptedCoverPts >= 0);
		break;
	default:
		Assert(0);
	}

	if( AssertVerify(IsCoverPoolTypeValid(CoverPool)) )
	{
		// If pool is full
		if( m_aMinFreeSlot[CoverPool] < 0 )
		{
			// Set the new minimum free slot
			m_aMinFreeSlot[CoverPool] = iAvailableSlot;
		}
		// If freeing a slot index that is less than the current minimum
		else if( iAvailableSlot < m_aMinFreeSlot[CoverPool] )
		{
			// Set the new minimum free slot
			m_aMinFreeSlot[CoverPool] = iAvailableSlot;
		}
#if __DEV
		// Validate that the slot being made available is within the indices for the given pool
		s16 iPoolMin = 0;
		s16 iPoolMax = NUM_COVERPOINTS;
		if( AssertVerify(CalculateCoverPoolExtents(CoverPool, iPoolMin, iPoolMax)) )
		{
			Assert(iAvailableSlot >= iPoolMin);
			Assert(iAvailableSlot < iPoolMax);
		}
#endif // __DEV
	}
}


//////////////////////////////////////////////////////
// FUNCTION: AddCoverPoint
// PURPOSE:  If this guy doesn't exist yet we'll add it to the list.
//////////////////////////////////////////////////////

s16
CCover::AddCoverPoint(CoverPoolType CoverPool, CCoverPoint::eCoverType Type, CEntity *pEntity, Vector3 *pCoors, CCoverPoint::eCoverHeight Height, CCoverPoint::eCoverUsage Usage, u8 CoverDir, CCoverPoint::eCoverArc CoverArc, bool bIsPriority, eHierarchyId eVehHierarchyId, s16* piOverlappingCoverpoint )
{
	Assert(Type != CCoverPoint::COVTYPE_NONE);
	Assert(pCoors);

	// Check if the coverpoint limit has already been reached
	if (m_NumPoints >= NUM_COVERPOINTS)
	{
		Assertf(CoverPool!=CP_SciptedPoint, "Scripted cover-point failed to be added at position (%.1f, %.1f, %.1f)", pCoors->x, pCoors->y, pCoors->z);
		return INVALID_COVER_POINT_INDEX;	// List is already full.
	}

	// Check if the specified cover pool type is invalid
	if( !IsCoverPoolTypeValid(CoverPool) )
	{
		Assertf( false, "Cover point added to unknown pool type!" );
		Assertf(CoverPool!=CP_SciptedPoint, "Scripted cover-point failed to be added at position (%.1f, %.1f, %.1f)", pCoors->x, pCoors->y, pCoors->z);
		return INVALID_COVER_POINT_INDEX;
	}

	// Get the position of this coverpoint in worldspace
	Vector3 vWorldPos;
	if(Type == CCoverPoint::COVTYPE_VEHICLE || Type == CCoverPoint::COVTYPE_OBJECT || Type == CCoverPoint::COVTYPE_BIG_OBJECT)
	{
		Assert(pEntity);
		vWorldPos = *pCoors;
		CCachedObjectCoverManager::TransformLocalToWorld(vWorldPos, (CPhysical*)pEntity);
	}
	else
	{
		vWorldPos = *pCoors;
	}
	CCoverPoint* pOverlappingCoverPoint = NULL;
	CCoverPointsGridCell * pCoverPointsGridCell = CCoverPointsContainer::CheckIfNoOverlapAndGetGridCell(vWorldPos, Type, pEntity, pOverlappingCoverPoint);
	if(!pCoverPointsGridCell)
	{
		// If the point overlaps, return the point if found
		if( pOverlappingCoverPoint && piOverlappingCoverpoint)
		{
			*piOverlappingCoverpoint = (s16) ( pOverlappingCoverPoint - m_aPoints );
			Assert(pOverlappingCoverPoint == &m_aPoints[*piOverlappingCoverpoint] );
		}
		Assertf(CoverPool!=CP_SciptedPoint, "Scripted cover-point failed to be added at position (%.1f, %.1f, %.1f)", pCoors->x, pCoors->y, pCoors->z);
		return INVALID_COVER_POINT_INDEX;
	}

	// Find the first empty slot in the correct pool.
	s16	NewSlot = 0;
	bool bSlotFound = GetNextAvailableSlot(CoverPool, NewSlot);

	// Check if a free slot was not found
	if( bSlotFound == FALSE )
	{
		// COVER POOL FULL

		// If this isn't the scripted pool (which is up to level designers to manage), then see if we can reduce the
		// bounding areas and force an update which will free up some space.  The next time this coverpoint gets
		// attempted to be added it may succeed.
		if(CoverPool!=CP_SciptedPoint)
		{
			// Set this flag, so that bounding areas will be reduced at the next Update()
			ms_eBoundingAreaOperation = EReduceBoundingArea;
		}
		else
		{
			Warningf("Ran out of space for adding scripted coverpoints, only %i are allowed.", NUM_SCRIPTED_COVERPOINTS);
			Assertf(CoverPool!=CP_SciptedPoint, "Scripted cover-point failed to be added at position (%.1f, %.1f, %.1f)", pCoors->x, pCoors->y, pCoors->z);
		}

		return INVALID_COVER_POINT_INDEX;
	}

	Assert(NewSlot < NUM_COVERPOINTS);
	Assert(m_aPoints[NewSlot].GetType() == CCoverPoint::COVTYPE_NONE);
	Assert(Type != CCoverPoint::COVTYPE_NONE);

	m_aPoints[NewSlot].SetType( Type );
	m_aPoints[NewSlot].SetDirection( CoverDir );
	m_aPoints[NewSlot].SetHeight( Height );
	m_aPoints[NewSlot].SetUsage( Usage );
	m_aPoints[NewSlot].SetArc( CoverArc );
	m_aPoints[NewSlot].ResetCoverPointStatus();
	m_aPoints[NewSlot].ResetCoverFlags();
	m_aPoints[NewSlot].SetIsThin(false);
	m_aPoints[NewSlot].SetVehicleHierarchyId(eVehHierarchyId);

	// Mark qualifying cover as low corner
	if( IsCoverLowCorner(Height,Usage) )
	{
		m_aPoints[NewSlot].SetFlag(CCoverPoint::COVFLAGS_IsLowCorner);
	}

	// Adjust this map cover point to give a more accurate direction depending on the usage
	AdjustMapCoverPoint(&m_aPoints[NewSlot]);

	// Clear the list of peds using the new coverpoint
	for (s32 C = 0; C < MAX_PEDS_PER_COVER_POINT; C++)
	{
		m_aPoints[NewSlot].m_pPedsUsingThisCover[C] = NULL;
	}

	// Set the entity the new coverpoint is associated with, if any
	m_aPoints[NewSlot].m_pEntity = pEntity;

	// Set the specified position
	if(pCoors)
	{
		m_aPoints[NewSlot].SetPosition(*pCoors);
	}

	//********************************************************************
	// Add the new coverpoint to the grid cell within which it resides.
	// We can just add it to the start of the list.
	//********************************************************************

	m_aPoints[NewSlot].m_pOwningGridCell = pCoverPointsGridCell;

	m_aPoints[NewSlot].m_pNextCoverPoint = pCoverPointsGridCell->m_pFirstCoverPoint;
	m_aPoints[NewSlot].m_pPrevCoverPoint = NULL;
	pCoverPointsGridCell->m_pFirstCoverPoint = &m_aPoints[NewSlot];
	if(m_aPoints[NewSlot].m_pNextCoverPoint)
		m_aPoints[NewSlot].m_pNextCoverPoint->m_pPrevCoverPoint = &m_aPoints[NewSlot];

	// Use the world space position of this point and make sure the bounding box of the cell
	// we added to gets extended, if needed.
	pCoverPointsGridCell->GrowBoundingBoxForAddedPoint(VECTOR3_TO_VEC3V(vWorldPos));

	if (bIsPriority)
	{
		m_aPoints[NewSlot].SetFlag(CCoverPoint::COVFLAGS_IsPriority);
	}

	// Update static data to reflect new slot in use
	SetSlotInUse(CoverPool, NewSlot);

	return NewSlot;
}


//////////////////////////////////////////////////////
// FUNCTION: RemoveCoverPointsForThisEntity
// PURPOSE:  Any cover points that belong to this entity are removed.
//////////////////////////////////////////////////////

void CCover::RemoveCoverPointsForThisEntity(CEntity *pEntity)
{
	for (s32 C = 0; C < NUM_COVERPOINTS; C++)
	{
		if(m_UsedCoverPoints.IsSet(C))
		{
			Assert(m_aPoints[C].GetType() != CCoverPoint::COVTYPE_NONE);
			if (pEntity == m_aPoints[C].m_pEntity)
			{
				RemoveCoverPoint(&m_aPoints[C]);
				Assert(m_NumPoints >= 0);
			}
		}
	}
}

//////////////////////////////////////////////////////
// FUNCTION: RemoveCoverPointWithIndex
// PURPOSE: Used by script functions, removed the cover point
//			with the given index
//////////////////////////////////////////////////////
void CCover::RemoveCoverPointWithIndex(s32 iIndex)
{
	Assert( iIndex >= 0 && iIndex < NUM_COVERPOINTS );

	if(m_UsedCoverPoints.IsSet(iIndex))
	{
		Assert(m_aPoints[iIndex].GetType() != CCoverPoint::COVTYPE_NONE);
		RemoveCoverPoint( &m_aPoints[iIndex] );
		Assert(m_NumPoints >= 0);
	}
}

//////////////////////////////////////////////////////
// FUNCTION: RemoveCoverPoint
// PURPOSE: removes the given cover point
//////////////////////////////////////////////////////
void CCover::RemoveCoverPoint(CCoverPoint *pPoint)
{
	Assert( pPoint );
	Assert( pPoint->GetType() != CCoverPoint::COVTYPE_NONE);

	//*******************************************************************************
	// Unlink this coverpoint from the grid-cell's coverpoints list which it is in.
	//*******************************************************************************

	// the previous ptr
	if(pPoint->m_pPrevCoverPoint)
		pPoint->m_pPrevCoverPoint->m_pNextCoverPoint = pPoint->m_pNextCoverPoint;
	// the next ptr
	if(pPoint->m_pNextCoverPoint)
		pPoint->m_pNextCoverPoint->m_pPrevCoverPoint = pPoint->m_pPrevCoverPoint;
	// the list head ptr in the cell itself
	Assert(pPoint->m_pOwningGridCell);
	if(pPoint->m_pOwningGridCell && pPoint->m_pOwningGridCell->m_pFirstCoverPoint == pPoint)
	{
		pPoint->m_pOwningGridCell->m_pFirstCoverPoint = pPoint->m_pNextCoverPoint;

		// If this was the last cover point, clear the box.
		if(!pPoint->m_pNextCoverPoint)
		{
			pPoint->m_pOwningGridCell->ClearBoundingBoxForContents();
		}
	}

	// Note: we could try to shrink the bounding box of the cell here, but not sure
	// if it's worth it.

	// Don't remove invalid coverpoints
	if( pPoint->GetType() == CCoverPoint::COVTYPE_NONE )
	{
#if __ASSERT
		const u32 iIndex = ptrdiff_t_to_int(pPoint - &m_aPoints[0]);
		Assert(m_UsedCoverPoints.IsClear(iIndex));
#endif
		return;
	}

	CoverPoolType CoverPool = GetCoverPoolType(pPoint->GetType());

	if (pPoint->m_pEntity.Get() != NULL) 
	{
		// Inform the object it is no longer used by cover
		// Flag no longer used
		//if( pPoint->GetType() == CCoverPoint::COVTYPE_OBJECT )
		//{
		//	static_cast<CObject*>(pPoint->m_pEntity)->SetUsedByCoverPoint(false);
		//}	
		pPoint->m_pEntity = NULL;
	}

	// Inform any peds using it that the cover point has been removed.
	for( s32 i = 0; i < MAX_PEDS_PER_COVER_POINT; i++ )
	{
		if( pPoint->m_pPedsUsingThisCover[i] )
		{
			// The ped release cover point will also unref and remove the ped from this point
			pPoint->m_pPedsUsingThisCover[i]->ReleaseCoverPoint();

			// Should be automatically wiped by the above function
			if( pPoint->m_pPedsUsingThisCover[i] )
			{
				pPoint->m_pPedsUsingThisCover[i] = NULL;
			}
		}
	}

	pPoint->SetCachedObjectCoverIndex(-1);

	pPoint->SetTypeNone();
	pPoint->m_pEntity = NULL;

	// Compute the index from the pointer offset in the cover array
	Assert(ptrdiff_t_to_int(pPoint - &m_aPoints[0]) < SHRT_MAX);
	const s16 iCoverIndex = (s16)(ptrdiff_t_to_int(pPoint - &m_aPoints[0]));
	if (aiVerifyf(iCoverIndex >= 0 && iCoverIndex < NUM_COVERPOINTS, "iCoverIndex wasn't within range %i", iCoverIndex))
	{
		SetSlotAvailable(CoverPool, iCoverIndex);
	}
}

#define MAX_SEARCH_COVER_POINTS 48

void CCover::PossiblyAddCoverPointToArray(sCoverPoint *paCoverPoints, CCoverPoint *pCoverPoint, const float &fScore, float &fWorstScore, s32 &iWorstIndex, s32 &iNumCoverPoints, const s32 &iNumCoverPointsToFind, bool bIsHighScoreBetter)
{
	bool bShouldAddCoverPoint = bIsHighScoreBetter ? fScore > fWorstScore : fScore < fWorstScore;
	if(bShouldAddCoverPoint)
	{
		aiAssertf(iWorstIndex >= 0 && iWorstIndex < iNumCoverPointsToFind, "Out of range array access on aCoverPoints!");
		paCoverPoints[iWorstIndex].fScore = fScore;
		paCoverPoints[iWorstIndex].pCoverPoint = pCoverPoint;

		// Increment the number of cover points found
		iNumCoverPoints = MIN(iNumCoverPoints + 1, iNumCoverPointsToFind);

		// If we haven't filled out the array yet then the we can just use the new cover points found index as the worst
		if(iNumCoverPoints < iNumCoverPointsToFind)
		{
			fWorstScore = bIsHighScoreBetter ? -LARGE_FLOAT : LARGE_FLOAT;
			iWorstIndex = iNumCoverPoints;
		}
		else
		{
			fWorstScore = bIsHighScoreBetter ? LARGE_FLOAT : -LARGE_FLOAT;
			iWorstIndex = -1;

			for(int j = 0; j < iNumCoverPointsToFind; j++)
			{	
				if( (!bIsHighScoreBetter && paCoverPoints[j].fScore > fWorstScore) ||
					(bIsHighScoreBetter && paCoverPoints[j].fScore < fWorstScore))
				{	
					iWorstIndex = j;
					fWorstScore = paCoverPoints[j].fScore;
				}
			}

			aiAssertf(iWorstIndex >= 0 && iWorstIndex < iNumCoverPointsToFind, "Could not find new worst index in aCoverPoints!");
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////
// FUNCTION: IsAnyPedUsingCoverOnThisObject
// PURPOSE:  Returns true if any ped is using a coverpoint on this object/vehicle
//////////////////////////////////////////////////////////////////////////////////

bool CCover::IsAnyPedUsingCoverOnThisEntity(CEntity *pEntity)
{
	for (s32 C = 0; C < NUM_COVERPOINTS; C++)
	{
		if(m_UsedCoverPoints.IsSet(C))
		{
			if(pEntity == m_aPoints[C].m_pEntity && (m_aPoints[C].GetType() == CCoverPoint::COVTYPE_OBJECT || m_aPoints[C].GetType() == CCoverPoint::COVTYPE_BIG_OBJECT || m_aPoints[C].GetType() == CCoverPoint::COVTYPE_VEHICLE))
			{
				for(s32 P = 0; P < MAX_PEDS_PER_COVER_POINT; P++)
				{
					if( m_aPoints[C].m_pPedsUsingThisCover[P].Get() != NULL )
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}


//////////////////////////////////////////////////////
// FUNCTION: FindCoverPoints
// PURPOSE:  This function finds the best cover points for a ped to use for taking cover.
//			 Either to attack or to run away and take cover.
//////////////////////////////////////////////////////

static float MAXDISTTOCOVERPOINT = (20.0f);		// Peds don't look for points further than this away

s32 CCover::FindCoverPoints(CPed* pPed, const Vector3& vSearchStartPos, const Vector3 &TargetCoors, const fwFlags32& iSearchFlags, CCoverPoint** paCoverpoints, s32 iNumCoverPointsToFind, CCoverPoint** paExceptions,
											  int iNumExceptions, const eCoverSearchType eSearchType, const float fMinDistance, const float fMaxDistance, CoverPointFilterConstructFn pFilterConstructFn, void* pData, const CPed* pTargetPed)
{
	PF_FUNC(FindCoverPoints);

	CCoverPointFilterInstance filter;
	const CCoverPointFilter* pFilter = filter.Construct(pFilterConstructFn, pData);

	aiAssertf(iNumCoverPointsToFind > 0 && iNumCoverPointsToFind <= MAX_SEARCH_COVER_POINTS, 
		"CCover::FindCoverPoints - iNumCoverPointsToFind must be greater than zero and less than %d!", MAX_SEARCH_COVER_POINTS);

	sCoverPoint aCoverPoints[MAX_SEARCH_COVER_POINTS];
	float fWorstScore = LARGE_FLOAT;
	s32 iWorstIndex = 0;
	s32	iNumCoverPoints = 0;

	Vector3			PointCoors;					// The current coverpoint's position

	// Initialise params used to score the cover points
	CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
	CDefensiveArea* pDefensiveAreaForCoverSearch = pPed->GetPedIntelligence()->GetDefensiveAreaForCoverSearch();

	sScoreCoverPointParams scoreCoverParams(pPed,
											pTargetPed,
											vSearchStartPos,
											TargetCoors,
											PointCoors,
											iSearchFlags,
											paExceptions,
											iNumExceptions,
											eSearchType,
											square(fMinDistance),
											square(fMaxDistance),
											vSearchStartPos.Dist2(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())),
											pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatOptimalCoverDistance),
											pPed->GetCoverPoint() != NULL,
											pPedTargeting && pPedTargeting->GetLosStatus(pTargetPed, false) != Los_blocked,
											pDefensiveAreaForCoverSearch && pDefensiveAreaForCoverSearch->IsActive(),
											pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableCoverArcAdjustments));

	// If ped is setup to use preferred cover and they have a valid list, search here first
	s32 iPreferredCoverGuid = pPed->GetPedIntelligence()->GetCombatBehaviour().GetPreferredCoverGuid();
	if (iPreferredCoverGuid != -1)
	{
		fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(iPreferredCoverGuid);
		if (pBase)
		{
			aiAssertf(pBase->GetIsClassId(CItemSet::GetStaticClassId()), "Guid not an item set");

			CItemSet* pItemSet = static_cast<CItemSet*>(pBase);
	
			s32 iNumItems = pItemSet->GetSetSize();
			if (iNumItems > 0)
			{
				for (s32 i=0; i<iNumItems; i++)
				{
					aiAssertf(pItemSet->GetEntity(i)->GetIsClassId(CScriptedCoverPoint::GetStaticClassId()), "Item %i Not a scripted coverpoint", i);

					CScriptedCoverPoint* pScriptedCoverPoint = static_cast<CScriptedCoverPoint*>(pItemSet->GetEntity(i));
					if (pScriptedCoverPoint)
					{
						CCoverPoint* pCoverPoint = FindCoverPointWithIndex(pScriptedCoverPoint->GetScriptIndex());

						if (pCoverPoint)
						{
							// Update the cover point to score and score it
							scoreCoverParams.pCoverPoint = pCoverPoint;
							scoreCoverParams.fScore = LARGE_FLOAT;
							if(ScoreCoverPoint(scoreCoverParams, pFilter))
							{
								PossiblyAddCoverPointToArray(&aCoverPoints[0], pCoverPoint, scoreCoverParams.fScore, fWorstScore, iWorstIndex, iNumCoverPoints, iNumCoverPointsToFind, false);
							}
						}		
					}
				}
			}
		}
	}

	if(iNumCoverPoints == 0)
	{
		// Haven't got or found a preferred cover point search the rest of the coverpoints
		const Vec3V sphereCenterV = VECTOR3_TO_VEC3V(vSearchStartPos);
		const ScalarV sphereRadiusV(fMaxDistance);
		CCoverPointsGridCell * pGridCell;

		for(int i=0; i<CCoverPointsContainer::ms_CoverPointGrids.GetCount(); i++)
		{
			pGridCell = CCoverPointsContainer::ms_CoverPointGrids[i];
			Assert(pGridCell);

			// Is there any overlap between the bbox of the gridcell contents, and the sphere we are looking within?
			if(!pGridCell->DoesSphereOverlapContents(sphereCenterV, sphereRadiusV))
			{
				continue;
			}

			// Check the remaining cover points
			CCoverPoint * pCoverPoint = pGridCell->m_pFirstCoverPoint;
			while(pCoverPoint)
			{
				// Update the cover point to score and score it
				scoreCoverParams.pCoverPoint = pCoverPoint;
				scoreCoverParams.fScore = LARGE_FLOAT;
				if(ScoreCoverPoint(scoreCoverParams, pFilter))
				{
					PossiblyAddCoverPointToArray(&aCoverPoints[0], pCoverPoint, scoreCoverParams.fScore, fWorstScore, iWorstIndex, iNumCoverPoints, iNumCoverPointsToFind, false);
				}

				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
			}
		}
	}

	// If we found some cover points then sort them and fill out the passed in array
	if(iNumCoverPoints)
	{
		std::sort(&aCoverPoints[0], &aCoverPoints[0] + iNumCoverPoints, sCoverPoint::SortAscending);

		for(int i = 0; i < iNumCoverPoints; i++)
		{
			paCoverpoints[i] = aCoverPoints[i].pCoverPoint;
		}
	}

	return iNumCoverPoints;
}


//-------------------------------------------------------------------------
// Finds the closest cover point to the given position
//-------------------------------------------------------------------------
CCoverPoint* CCover::FindClosestCoverPoint(const CPed* pPed, const Vector3& vPos, const Vector3* pvTarget, const eCoverSearchType eSearchType, CCoverPoint::eCoverType eCoverType, CEntity* pEntityConstraint, const bool bCoverPointMustBeFree )
{
	PF_FUNC(FindClosestCoverPoint);
	//@@: location CCOVER_FINDCLOSEST_COVER_POINT
	const Vec3V sphereCenterV = VECTOR3_TO_VEC3V(vPos);
	const ScalarV sphereRadiusV(MAXDISTTOCOVERPOINT);

	Vector3	PointCoors;
	CCoverPoint*	pBestCoverPoint = NULL;
	float	fBestScore = 99999.9f;
	float	Distance_PointToPos;

	CCoverPointsGridCell * pGridCell;

	for(int i=0; i<CCoverPointsContainer::ms_CoverPointGrids.GetCount(); i++)
	{
		pGridCell = CCoverPointsContainer::ms_CoverPointGrids[i];
		Assert(pGridCell);

		// Is there any overlap between the bbox of the gridcell contents, and the sphere we are looking within ?
		if(!pGridCell->DoesSphereOverlapContents(sphereCenterV, sphereRadiusV))
		{
			continue;
		}

		// Now we'll check whether there are any conflict with any of the coverpoints in this gridcell
		CCoverPoint * pCoverPoint = pGridCell->m_pFirstCoverPoint;
		while(pCoverPoint)
		{
			
			if( RemoveCoverPointIfEntityLost(pCoverPoint) )
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}
		
			if (!bCoverPointMustBeFree || pCoverPoint->CanAccomodateAnotherPed())
			{
				// Get the points coordinates
				pCoverPoint->GetCoverPointPosition(PointCoors);

				// If the point is currently dangerous ignore
				if( pCoverPoint->IsDangerous() )
				{
					pCoverPoint = pCoverPoint->m_pNextCoverPoint;
					continue;
				}

				// If the point is thin, ignore by default, only the player can use it
				if( pCoverPoint->GetIsThin() )
				{
					pCoverPoint = pCoverPoint->m_pNextCoverPoint;
					continue;
				}

				// Check the type constraints:
				if( eCoverType != CCoverPoint::COVTYPE_NONE && pCoverPoint->GetType() != eCoverType )
				{
					pCoverPoint = pCoverPoint->m_pNextCoverPoint;
					continue;
				}

				// Check the entity constraint
				if( pEntityConstraint != NULL && pCoverPoint->m_pEntity != pEntityConstraint )
				{
					pCoverPoint = pCoverPoint->m_pNextCoverPoint;
					continue;
				}
				
				// If the point is close to the player, ignore it
				if ( pPed && CCoverPoint::IsCloseToPlayer(*pPed, VECTOR3_TO_VEC3V(PointCoors)) )
				{
					pCoverPoint = pCoverPoint->m_pNextCoverPoint;
					continue;
				}

				// We have found an active coverpoint that hasn't been taken by anyone yet.
				if (ABS(vPos.z - PointCoors.z) < 4.0f)
				{		// Resonably close in z
					Distance_PointToPos = ((Vector3)(vPos - PointCoors)).Mag();

					if (Distance_PointToPos < MAXDISTTOCOVERPOINT)
					{		
						float fScore = Distance_PointToPos;

						// Check that the cover point provides cover to the target passed
						if( pvTarget )
						{
							const bool bProvidesCover = DoesCoverPointProvideCover( pCoverPoint, pCoverPoint->GetArc(), *pvTarget, PointCoors );

							if( eSearchType == CS_MUST_PROVIDE_COVER &&
								!bProvidesCover )
							{
								pCoverPoint = pCoverPoint->m_pNextCoverPoint;
								continue;
							}
							if( eSearchType == CS_PREFER_PROVIDE_COVER &&
								!bProvidesCover )
							{
								// Increase the distance of cover points that do not provide cover to make peds less likely to
								// use them if valid coverpoints are available.
								fScore *= 2.0f;
								fScore += 5.0f;
							}
						}

						if (fScore < fBestScore )
						{
							fBestScore = fScore;
							pBestCoverPoint = pCoverPoint;
						}
					}
				}
			}
			pCoverPoint = pCoverPoint->m_pNextCoverPoint;
		}
	}
	
	if (pBestCoverPoint)
	{
		return pBestCoverPoint;
	}
	return NULL;
}


//-------------------------------------------------------------------------
// Finds the closest cover point to the given position
//-------------------------------------------------------------------------
CCoverPoint* CCover::FindClosestCoverPointWithCB( const CPed* pPed, const Vector3& vPos, float fMaxDistanceFromPed, const Vector3* pvTarget, const eCoverSearchType eSearchType, CoverPointScoringFn pScoringFn, CoverPointValidityFn pValidityFn, void* pData )
{
	PF_FUNC(FindClosestCoverPointWithCB);

	const Vec3V sphereCenterV = VECTOR3_TO_VEC3V(vPos);
	const ScalarV sphereRadiusV(fMaxDistanceFromPed);

	Vector3	PointCoors;
	CCoverPoint*	pBestCoverPoint = NULL;
	float	fBestScore = CTaskCover::INVALID_COVER_SCORE;
	float	Distance_PointToPos;
	float	Distance_PointToPosSq;

	CCoverPointsGridCell * pGridCell;

	for(int i=0; i<CCoverPointsContainer::ms_CoverPointGrids.GetCount(); i++)
	{
		pGridCell = CCoverPointsContainer::ms_CoverPointGrids[i];
		Assert(pGridCell);

		// Is there any overlap between the bbox of the points in the gridcell, and the sphere we are interested in finding cover within?
		if(!pGridCell->DoesSphereOverlapContents(sphereCenterV, sphereRadiusV))
		{
			continue;
		}

		// Now we'll check whether there are any conflict with any of the coverpoints in this gridcell
		CCoverPoint * pCoverPoint = pGridCell->m_pFirstCoverPoint;
		while(pCoverPoint)
		{
			if( RemoveCoverPointIfEntityLost(pCoverPoint) )
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			if (pCoverPoint->CanAccomodateAnotherPed())
			{
				// Get the points coordinates
				pCoverPoint->GetCoverPointPosition(PointCoors);

				// If the point is currently dangerous ignore
				if( pCoverPoint->IsDangerous() )
				{
					pCoverPoint = pCoverPoint->m_pNextCoverPoint;
					continue;
				}
				
				// If the point is close to the player, ignore it
				if ( pPed && CCoverPoint::IsCloseToPlayer(*pPed, VECTOR3_TO_VEC3V(PointCoors)) )
				{
					pCoverPoint = pCoverPoint->m_pNextCoverPoint;
					continue;
				}

				// If it's a point within a player only blocking area and we're the player, ignore this one
				if (pPed->IsLocalPlayer() && !CCover::IsPointWithinValidArea(PointCoors, pCoverPoint->GetType(), true))
				{
					pCoverPoint = pCoverPoint->m_pNextCoverPoint;
					continue;
				}

				// We have found an active coverpoint that hasn't been taken by anyone yet.
				if (ABS(vPos.z - PointCoors.z) < 4.0f)
				{		// Resonably close in z
					Distance_PointToPosSq = ((Vector3)(vPos - PointCoors)).Mag2();

					if (Distance_PointToPosSq < rage::square(fMaxDistanceFromPed))
					{
						Distance_PointToPos = sqrtf(Distance_PointToPosSq);
						float fScore = 0.0f;
						
						if( pValidityFn != NULL && !pValidityFn( pCoverPoint, PointCoors, pData ) )
						{
							pCoverPoint = pCoverPoint->m_pNextCoverPoint;
							continue;
						}
						// Check the scoring callback function
						if( pScoringFn != NULL )
						{
							fScore = pScoringFn( pCoverPoint, PointCoors, pData );
						}
						else
						{
							fScore = Distance_PointToPos;
						}
						
						if( pvTarget )
						{
							const bool bProvidesCover = DoesCoverPointProvideCover( pCoverPoint, pCoverPoint->GetArc(), *pvTarget, PointCoors );

							if( eSearchType == CS_MUST_PROVIDE_COVER &&
								!bProvidesCover )
							{
								pCoverPoint = pCoverPoint->m_pNextCoverPoint;
								continue;
							}
							if( eSearchType == CS_PREFER_PROVIDE_COVER &&
								!bProvidesCover )
							{
								// Increase the distance of cover points that do not provide cover to make peds less likely to
								// use them if valid coverpoints are available.
								fScore *= 2.0f;
								fScore += 5.0f;
							}
						}

						if (fScore < fBestScore )
						{
							fBestScore = fScore;
							pBestCoverPoint = pCoverPoint;

							if (pPed->IsLocalPlayer() && pScoringFn == CTaskPlayerOnFoot::StaticCoverpointScoringCB)
							{
								PlayerCoverSearchData* pCoverSearchData = (PlayerCoverSearchData*)pData;
								pCoverSearchData->m_BestScore = fBestScore;
							}
						}
					}
				}
			}
			pCoverPoint = pCoverPoint->m_pNextCoverPoint;
		}
	}

	if (pBestCoverPoint)
	{
		return pBestCoverPoint;
	}
	return NULL;
}

bool CCover::FindCoverPointsWithinRange(CCoverPoint** paCoverpoints, s32& iNumCoverPoints, const CCoverSearchWithinRangeData& searchData)
{
	PF_FUNC(FindCoverPointsWithinRange);

	const CPed*	pPed = searchData.GetPed();
	s32	iNumCoverPointsIn	= iNumCoverPoints;
	float	fMaxSearchDist		= searchData.GetMaxSearchDist();
	Vector3 vSearchPos			= searchData.GetSearchPos();
	Vector3 vThreatPos			= searchData.GetThreatPos();
	float	fMinDistToThreat	= searchData.GetMinDistToThreat();
	float	fMaxDistToThreat	= searchData.GetMaxDistToThreat();
	CCoverPoint**	apExceptions = searchData.GetExceptions();
	s32   iNumExceptions		= searchData.GetNumExceptions();

	Vector3 vExtraSize(fMaxSearchDist, fMaxSearchDist, fMaxSearchDist);
	Vector3 vCoverPointMins = vSearchPos - vExtraSize;
	Vector3 vCoverPointMaxs = vSearchPos + vExtraSize;
	Vector3	vPointCoors;

	float	fDistance_PointToPosSq;
	float	fDistance_PointToThreatSq;

	CCoverPointsGridCell* pGridCell;

	for (int i=0; i<CCoverPointsContainer::ms_CoverPointGrids.GetCount(); i++)
	{
		pGridCell = CCoverPointsContainer::ms_CoverPointGrids[i];
		aiAssert(pGridCell);

		// Is there any overlap between the bbox of the gridcell, and the expanded bbox of the cover position ?
		if(!pGridCell->DoesAABBOverlapCell(VECTOR3_TO_VEC3V(vCoverPointMins), VECTOR3_TO_VEC3V(vCoverPointMaxs)))
		{
			continue;
		}

		// Now we'll check whether there are any conflict with any of the coverpoints in this gridcell
		CCoverPoint * pCoverPoint = pGridCell->m_pFirstCoverPoint;
		while (pCoverPoint && iNumCoverPoints < 64)
		{
			if (RemoveCoverPointIfEntityLost(pCoverPoint))
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			if (pCoverPoint->CanAccomodateAnotherPed())
			{
				// If the point is currently dangerous ignore
				if (pCoverPoint->IsDangerous())
				{
					pCoverPoint = pCoverPoint->m_pNextCoverPoint;
					continue;
				}

				// Get the points coordinates
				pCoverPoint->GetCoverPointPosition(vPointCoors);
				
				// If the point is close to the player, ignore it
				if ( pPed && CCoverPoint::IsCloseToPlayer(*pPed, VECTOR3_TO_VEC3V(vPointCoors)) )
				{
					pCoverPoint = pCoverPoint->m_pNextCoverPoint;
					continue;
				}

				// We have found an active coverpoint that hasn't been taken by anyone yet.
				if (ABS(vSearchPos.z - vPointCoors.z) < 4.0f)
				{		
					// Reasonably close in z (Might want to change this if we want to search for sniping positions etc)
			
					bool bPassedAllTests = true;

					// Perform exception test
					if (searchData.IsFlagSet(CCoverSearchFlags::CSD_DoExceptionCheck) && aiVerifyf(apExceptions, "Exception array was null"))
					{
						for (int i=0; i<iNumExceptions; i++)
						{
							if (apExceptions[i] == pCoverPoint)
							{
								bPassedAllTests = false;
								break;
							}
						}
					}

					// Perform search tests based on flags set in the search data
					if (bPassedAllTests && searchData.IsFlagSet(CCoverSearchFlags::CSD_DoRangeCheck))
					{
						fDistance_PointToPosSq = (vSearchPos - vPointCoors).Mag2();
						
						// If the cover point is too far, discount this one
						if (fDistance_PointToPosSq > rage::square(fMaxSearchDist))
						{
							bPassedAllTests = false;
						}
					}

					if (bPassedAllTests && searchData.IsFlagSet(CCoverSearchFlags::CSD_DoAvoidancePosCheck))
					{
						fDistance_PointToThreatSq = (vThreatPos - vPointCoors).Mag2();

						// If the cover point is too far or too close to the threat, discount this one
						if ((fDistance_PointToThreatSq > rage::square(fMaxDistToThreat)) ||
							(fDistance_PointToThreatSq < rage::square(fMinDistToThreat)))
						{
							bPassedAllTests = false;
						}
					}

					if (bPassedAllTests && searchData.IsFlagSet(CCoverSearchFlags::CSD_DoDirectionCheck))
					{
						Vector3 vAlteredCoverCoors = vPointCoors;
						vAlteredCoverCoors.z = 0.0f;
						Vector3 vAlteredThreatPos = vThreatPos;
						vAlteredThreatPos.z = 0.0f;

						Vector3 vToTarget = vAlteredThreatPos - vAlteredCoverCoors;
						vToTarget.Normalize();

						Vector3 vCoverDirection = VEC3V_TO_VECTOR3(pCoverPoint->GetCoverDirectionVector(&RCC_VEC3V(vToTarget)));
						vCoverDirection.z = 0.0f;
						vCoverDirection.Normalize();

						float fCoverTolerance = QUARTER_PI; 

						if (acos(vCoverDirection.Dot(vToTarget)) > fCoverTolerance)
						{
							bPassedAllTests = false;
						}
					}

					// Cover point passed, add to list
					if (bPassedAllTests)
					{
						paCoverpoints[iNumCoverPoints++] = pCoverPoint;
					}
				}
			}
			pCoverPoint = pCoverPoint->m_pNextCoverPoint;
		}
	}

	// Cover points found
	if (iNumCoverPointsIn != iNumCoverPoints)
	{
		return true;
	}

	return false;
}

bool CCover::ScoreCoverPoint(sScoreCoverPointParams& scoringParams, const CCoverPointFilter* pFilter)
{
	float	Distance_PointToSearchStartSq;

	if( RemoveCoverPointIfEntityLost(scoringParams.pCoverPoint) )
	{
		return false;
	}

	// return if this cover point is one of the exceptions
	for( s32 i = 0; i < scoringParams.iNumExceptions; i++ )
	{
		if( scoringParams.paExceptions[i] == scoringParams.pCoverPoint )
		{
			return false;
		}
	}

	// If the point is waiting to be cleaned up then don't use it
	if( scoringParams.pCoverPoint->GetFlag(CCoverPoint::COVFLAGS_WaitingForCleanup) )
	{
		return false;
	}

	if (scoringParams.pCoverPoint->CanAccomodateAnotherPed())
	{
		// If the point is currently dangerous ignore
		if( scoringParams.pCoverPoint->IsDangerous() )
		{
			return false;
		}

		// If the point is thin, ignore by default, only the player can use it
		if( scoringParams.pCoverPoint->GetIsThin() )
		{
			return false;
		}

		// If it isn't allowed to be the players vehicle and is, skip it
		if( scoringParams.iSearchFlags.IsFlagSet(SF_CheckForTargetsInVehicle) && !CheckForTargetsInVehicle(scoringParams.pCoverPoint, scoringParams.pPed) )
		{
			return false;
		}

		// Get the points coordinates
		scoringParams.pCoverPoint->GetCoverPointPosition(scoringParams.PointCoors);

		// Skip the point if the filter's validity function returns false
		if( pFilter && !pFilter->CheckValidity( *scoringParams.pCoverPoint, RCC_VEC3V(scoringParams.PointCoors) ) )
		{
			return false;
		}

		// If the point is close to the player, ignore it
		if ( CCoverPoint::IsCloseToPlayer(*scoringParams.pPed, VECTOR3_TO_VEC3V(scoringParams.PointCoors)) )
		{
			return false;
		}

		CCoverPoint::eCoverArc coverArcToUse = scoringParams.bDisableCoverAdjustments ? 
											   scoringParams.pCoverPoint->GetArc() :
											   scoringParams.pCoverPoint->GetArcToUse(scoringParams.bHasLosToTarget, scoringParams.bIsUsingDefensiveArea);

		const bool bProvidesCover = DoesCoverPointProvideCoverFromTargets( scoringParams.pPed, scoringParams.TargetCoors, scoringParams.pCoverPoint,
																		   coverArcToUse, scoringParams.PointCoors, scoringParams.pTargetPed );

		// Points on the map only work in a specific direction.
		if( scoringParams.eSearchType != CS_MUST_PROVIDE_COVER || bProvidesCover )
		{
			// We have found an active coverpoint that hasn't been taken by anyone yet.
			Distance_PointToSearchStartSq = ((Vector3)(scoringParams.vSearchStartPos - scoringParams.PointCoors)).Mag2();

			// Is this point is close enough to be considered ?
			if (Distance_PointToSearchStartSq >= scoringParams.fMinDistanceSq &&
				Distance_PointToSearchStartSq < scoringParams.fMaxDistanceSq)
			{
				if(scoringParams.bHasCoverPoint)
				{
					// For now we will reject any cover point that is further than we are from the search position if we already have cover
					if( Distance_PointToSearchStartSq > scoringParams.fPedToSearchStartDistanceSq )
					{
						return false;
					}

					// If we are already within the optimal range then there isn't a need to go any closer
					if( scoringParams.bHasOptimalCoverDistance && scoringParams.bIsPedWithinOptimalCoverDistance && 
						Distance_PointToSearchStartSq < scoringParams.fOptimalCoverDistanceSq )
					{
						return false;
					}
				}
				
				if( scoringParams.bHasCoverPoint || scoringParams.bHasOptimalCoverDistance )
				{
					// More likely the closer in distance it is to ourselves
					scoringParams.fScore = scoringParams.PointCoors.Dist2(VEC3V_TO_VECTOR3(scoringParams.pPed->GetTransform().GetPosition()));
				}
				else
				{
					// More like the closer it is to the search start position
					scoringParams.fScore = Distance_PointToSearchStartSq;
				}

				// Alter the score if the coverpoint does not provide cover.
				if( scoringParams.eSearchType == CS_PREFER_PROVIDE_COVER &&
					!bProvidesCover )
				{
					scoringParams.fScore *= 2.0f;
					scoringParams.fScore += 5.0f;
				}

				return true;
			}
		}
	}

	return false;
}

bool CCover::CheckForTargetsInVehicle(CCoverPoint *pCoverPoint, const CPed* pPed)
{
	if (pPed && pCoverPoint && pCoverPoint->GetType() == CCoverPoint::COVTYPE_VEHICLE)
	{
		CEntity* pEntity = static_cast<CEntity*>(pCoverPoint->m_pEntity.Get());

		if (pEntity && pEntity->GetIsTypeVehicle())
		{
			CPedTargetting* pTargeting = pPed->GetPedIntelligence()->GetTargetting();
			if (pTargeting)
			{
				const CEntity* pCurrentTarget = static_cast<const CEntity*>(pTargeting->GetCurrentTarget());

				if (pCurrentTarget && pCurrentTarget->GetIsTypePed())
				{
					const CPed* pTargetPed = static_cast<const CPed*>(pCurrentTarget);
					const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
					const CPed* pDriver = pVehicle->GetSeatManager()->GetDriver();

					// If vehicle contains current target or the ped searching for cover isn't friendly with the driver,
					// don't allow ped to use this cover point
					if (pVehicle->ContainsPed(pTargetPed) || (pDriver && !pPed->GetPedIntelligence()->IsFriendlyWith(*pDriver)))
					{
						return false;
					}
				}
			}
		}
	}
	return true;
}

//-------------------------------------------------------------------------
// PURPOSE: Returns a random coverpoint in the given arc
// PARAMS:	current position
//					direction
//					maximum angle
//-------------------------------------------------------------------------
s32 CCover::FindCoverPointInArc(const CPed* pPed, Vector3 * pvOut, const Vector3& vPos, const Vector3& vDirection, const float fArc, const float fMinDistance, const float fMaxDistance,
								const fwFlags32& iSearchFlags, const Vector3 & vTarget, CCoverPoint** paCoverpoints, s32 iNumCoverPointsToFind, const eCoverSearchType eSearchType,
								CCoverPoint** paExceptions, int iNumExceptions, CoverPointFilterConstructFn pFilterConstructFn, void* pData, const CPed* pTargetPed)
{
	PF_FUNC(FindCoverPointInArc);

	CCoverPointFilterInstance filter;
	const CCoverPointFilter* pFilter = filter.Construct(pFilterConstructFn, pData);

	aiAssertf(iNumCoverPointsToFind > 0 && iNumCoverPointsToFind <= MAX_SEARCH_COVER_POINTS, 
		"CCover::FindCoverPointInArc - iNumCoverPointsToFind must be greater than zero and less than %d!", MAX_SEARCH_COVER_POINTS);

	Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	float Distance_PedToPosSq = vPedPosition.Dist2(vPos);

	bool bPedHasCoverPoint = pPed->GetCoverPoint() != NULL;
	bool bIsRandomSearch = iSearchFlags.IsFlagSet(SF_Random);
	sCoverPoint aCoverPoints[MAX_SEARCH_COVER_POINTS];

	Vector3	PointCoors;
	float fTotalProbability = 0.0f;
	float Distance_PointToPos;
	float fWorstScore = -LARGE_FLOAT;
	float afCoverPointProbabilities[MAX_SEARCH_COVER_POINTS];
	s32 iWorstIndex = 0;
	s32	iNumCoverPoints = 0;

	float fOptimalCoverDistance = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatOptimalCoverDistance);
	float fOptimalCoverDistanceSq = square(fOptimalCoverDistance);
	float fOptimalWithExtraDistSq = square(fOptimalCoverDistance + EXTRA_COVER_RANGE);
	bool bHasOptimalCoverDistance = fOptimalCoverDistance > 0.0f;
	bool bIsPedWithinOptimalRange = Distance_PedToPosSq < fOptimalWithExtraDistSq;

	CCoverPointsGridCell * pGridCell;

	// Make sure we have a normalized direction:
	Assert(vDirection.Mag2() >= square(0.99f) && vDirection.Mag2() <= 1.01f);

	// If we have an arc angle of 90 degrees or less, we can use a hemisphere test to reject
	// whole grid cells with points that couldn't possibly intersect the arc.
	// Otherwise, we'll use a sphere. Note that it would be possible to write test functions
	// for arbitrary cones, but right now, we don't seem to use this function with anything else
	// than 90 or 180 degree arcs.
	const bool doHemisphereTest = (fArc <= PI*0.5f + SMALL_FLOAT);

	const Vec3V sphereCenterV = VECTOR3_TO_VEC3V(vPos);
	const ScalarV sphereRadiusV(fMaxDistance);

	for(int i=0; i<CCoverPointsContainer::ms_CoverPointGrids.GetCount(); i++)
	{
		if( bIsRandomSearch && iNumCoverPoints >= MAX_SEARCH_COVER_POINTS )
			break;

		pGridCell = CCoverPointsContainer::ms_CoverPointGrids[i];
		Assert(pGridCell);

		// Is there any overlap between the bbox of the gridcell contents, and the sphere or hemisphere covering the arc?
		if(doHemisphereTest)
		{
			if(!pGridCell->DoesHemisphereOverlapContents(sphereCenterV, sphereRadiusV,
					VECTOR3_TO_VEC3V(vDirection)))
			{
				continue;
			}
		}
		else
		{
			if(!pGridCell->DoesSphereOverlapContents(sphereCenterV, sphereRadiusV))
			{
				continue;
			}
		}

		CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
		bool bHasLosToTarget = (pPedTargeting && pPedTargeting->GetLosStatus(pTargetPed, false) != Los_blocked);
		bool bUsingDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveAreaForCoverSearch() && pPed->GetPedIntelligence()->GetDefensiveAreaForCoverSearch()->IsActive();
		bool bDisableCoverArcAdjustments = pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableCoverArcAdjustments);

		// Now we'll check whether there are any conflict with any of the coverpoints in this gridcell
		CCoverPoint * pCoverPoint = pGridCell->m_pFirstCoverPoint;
		while(pCoverPoint)
		{
			bool bSkipPoint = false;
			// Check it is not one of the exceptions
			for( s32 i = 0; i < iNumExceptions; i++ )
			{
				if( paExceptions[i] == pCoverPoint )
				{
					bSkipPoint = true;
					break;
				}
			}

			// Skip the point if instructed
			if( bSkipPoint )
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			if( RemoveCoverPointIfEntityLost(pCoverPoint) )
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			// If the point is waiting to be cleaned up then don't use it
			if( pCoverPoint->GetFlag(CCoverPoint::COVFLAGS_WaitingForCleanup) )
			{
				return false;
			}

			// If it isn't allowed to be occupied and is, skip it
			if( !iSearchFlags.IsFlagSet(SF_CanBeOccupied) && pCoverPoint->IsOccupied())
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			// If the point is currently dangerous continue
			if( pCoverPoint->IsDangerous() )
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			// If the point is thin, ignore by default, only the player can use it
			if(pCoverPoint->GetIsThin() )
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			// If it isn't allowed to be the players vehicle and is, skip it
			if( iSearchFlags.IsFlagSet(SF_CheckForTargetsInVehicle) && !CheckForTargetsInVehicle(pCoverPoint, pPed) )
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			pCoverPoint->GetCoverPointPosition(PointCoors);
			PointCoors.z += 1.0f;

			// If a validity callback function has been provided, use this to check if this coverpoint is valid
			if( pFilter && !pFilter->CheckValidity(*pCoverPoint, RCC_VEC3V(PointCoors)))
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			// If the point is close to the player, ignore it
			if ( CCoverPoint::IsCloseToPlayer(*pPed, VECTOR3_TO_VEC3V(PointCoors)) )
			{
				pCoverPoint = pCoverPoint->m_pNextCoverPoint;
				continue;
			}

			// make sure the cover point isn't too high or low
			{	
				Vector3 vFromPosToPoint = PointCoors - vPos;
				Distance_PointToPos = vFromPosToPoint.Mag();

				// Check it is within the distance constraints
				if ( Distance_PointToPos < fMaxDistance &&
					Distance_PointToPos > fMinDistance )
				{
					if( bPedHasCoverPoint )
					{
						float Distance_PointToPosSq = square(Distance_PointToPos);

						// For now we will reject any cover point that is further than we are from the search position if we already have cover
						if( Distance_PointToPosSq > Distance_PedToPosSq )
						{
							pCoverPoint = pCoverPoint->m_pNextCoverPoint;
							continue;
						}

						// If we are already within the optimal range then there isn't a need to go any closer
						if( bHasOptimalCoverDistance && bIsPedWithinOptimalRange && 
							Distance_PointToPosSq < fOptimalCoverDistanceSq )
						{
							pCoverPoint = pCoverPoint->m_pNextCoverPoint;
							continue;
						}
					}

					vFromPosToPoint.Normalize();
					const float fAngleDifference = vFromPosToPoint.Angle( vDirection );

					// Check the angular constraints
					if( ABS( fAngleDifference ) < fArc )
					{
						// Work out the weighting of this point
						float fWeight = 0.0f;

						if(bPedHasCoverPoint || bHasOptimalCoverDistance)
						{
							// More likely the closer in distance it is to ourselves
							fWeight += 1.0f - ( vPedPosition.Dist(PointCoors) / fMaxDistance );
						}
						else
						{
							// More likely the closer in distance it is to the search position
							fWeight += 1.0f - ( (Distance_PointToPos - fMinDistance) / (fMaxDistance - fMinDistance) );
						}

						// More likely the closer in angle
						fWeight += 1.0f - ( fAngleDifference / fArc );

						CCoverPoint::eCoverArc coverArcToUse = bDisableCoverArcAdjustments ? pCoverPoint->GetArc() : pCoverPoint->GetArcToUse(bHasLosToTarget, bUsingDefensiveArea);
						const bool bProvidesCover = DoesCoverPointProvideCoverFromTargets( pPed, vTarget, pCoverPoint, coverArcToUse, PointCoors, pTargetPed );

						// Ignore points that don't provide cover if the search criteria specifies that it has to
						if( eSearchType == CS_MUST_PROVIDE_COVER &&
							!bProvidesCover )
						{
							pCoverPoint = pCoverPoint->m_pNextCoverPoint;
							continue;
						}

						// If the search prefers points that provide cover, reduce the change of picking one
						// that does now.
						if( eSearchType == CS_PREFER_PROVIDE_COVER &&
							!bProvidesCover )
						{
							fWeight *= 0.5f;
						}

						// If we're not performing a random search
						if( !bIsRandomSearch )
						{
							// Add in the point if possible
							PossiblyAddCoverPointToArray(&aCoverPoints[0], pCoverPoint, fWeight, fWorstScore, iWorstIndex, iNumCoverPoints, iNumCoverPointsToFind, true);
						}
						else
						{
							// Add the cover point to the list
							aCoverPoints[iNumCoverPoints].pCoverPoint = pCoverPoint;
							afCoverPointProbabilities[iNumCoverPoints] = fTotalProbability + fWeight;
							fTotalProbability = afCoverPointProbabilities[iNumCoverPoints];

							// Increment the number of cover points found
							++iNumCoverPoints;

							if( iNumCoverPoints >= MAX_SEARCH_COVER_POINTS )
								break;
						}

						//grcDebugDraw::Line( vPos, PointCoors, Color32(0.00f,0.00f,fWeight/2.0f), Color32(0.00f,0.00f,fWeight/2.0f) );
					}
				}
			}

			pCoverPoint = pCoverPoint->m_pNextCoverPoint;
		}
	}

	// No valid cover points, return 0
	if( iNumCoverPoints == 0 )
	{
		return 0;
	}

	// Choose from the generated list randomly
	if( bIsRandomSearch )
	{
		aiAssertf(iNumCoverPointsToFind == 1, "CCover::FindCoverPointInArc - Random search should only request one cover point!");

		// Randomly pick a cover point depending on it's weighting
		const float fRand = fwRandom::GetRandomNumberInRange( 0.0f, fTotalProbability );

		for( s32 i = 0; i < iNumCoverPoints; i++ )
		{
			if( fRand <= afCoverPointProbabilities[i] )
			{
				if( pvOut )
				{
					aCoverPoints[i].pCoverPoint->GetCoverPointPosition(*pvOut);
				}

				paCoverpoints[0] = aCoverPoints[i].pCoverPoint;
				break;
			}
		}
	}
	else
	{
		std::sort(&aCoverPoints[0], &aCoverPoints[0] + iNumCoverPoints, sCoverPoint::SortDescending);
		
		for(int i = 0; i < iNumCoverPoints; i++)
		{
			paCoverpoints[i] = aCoverPoints[i].pCoverPoint;
		}
	}

	return iNumCoverPoints;
}

void CCover::CalculateCoverArcDirections(const CCoverPoint& rCoverPoint, Vector3& vCoverArcDir1, Vector3& vCoverArcDir2)
{
	//Get the cover direction.
	Vector3 vDir = VEC3V_TO_VECTOR3(rCoverPoint.GetCoverDirectionVector(NULL));
	vCoverArcDir1 = vDir;
	vCoverArcDir2 = vDir;
	
	//Note: These rotations are actually opposite from what you would expect,
	//		due to the way RotateZ works.
	
	//Check the cover arc.
	switch(rCoverPoint.GetArc())
	{
		case CCoverPoint::COVARC_180:
		{
			vCoverArcDir1.RotateZ( 90.0f * DtoR);
			vCoverArcDir2.RotateZ(-90.0f * DtoR);
			break;
		}
		case CCoverPoint::COVARC_120:
		{
			vCoverArcDir1.RotateZ( 60.0f * DtoR);
			vCoverArcDir2.RotateZ(-60.0f * DtoR);
			break;
		}
		case CCoverPoint::COVARC_90:
		{
			vCoverArcDir1.RotateZ( 45.0f * DtoR);
			vCoverArcDir2.RotateZ(-45.0f * DtoR);
			break;
		}
		case CCoverPoint::COVARC_45:
		{
			vCoverArcDir1.RotateZ( 22.5f * DtoR);
			vCoverArcDir2.RotateZ(-22.5f * DtoR);
			break;
		}
		case CCoverPoint::COVARC_0TO60:
		{
			vCoverArcDir2.RotateZ(-60.0f * DtoR);
			break;
		}
		case CCoverPoint::COVARC_300TO0:
		{
			vCoverArcDir1.RotateZ( 60.0f * DtoR);
			break;
		}
		case CCoverPoint::COVARC_0TO45:
		{
			vCoverArcDir2.RotateZ(-45.0f * DtoR);
			break;
		}
		case CCoverPoint::COVARC_315TO0:
		{
			vCoverArcDir1.RotateZ( 45.0f * DtoR);
			break;
		}
		case CCoverPoint::COVARC_0TO22:
		{
			vCoverArcDir2.RotateZ(-22.5f * DtoR);
			break;
		}
		case CCoverPoint::COVARC_338TO0:
		{
			vCoverArcDir1.RotateZ( 22.5f * DtoR);
			break;
		}
		default:
		{
			aiAssertf(false, "The cover arc is invalid: %d.", rCoverPoint.GetArc());
			break;
		}
	}
}

//////////////////////////////////////////////////////
// FUNCTION: CalculateVantagePointForCoverPoint
// PURPOSE:  Given a pointer to a coverpoint this function will return the coordinates.
//			 of the vantage point for the cover point, i.e. where the ped will lean to
//			 when shooting
//////////////////////////////////////////////////////
void CCover::CalculateVantagePointForCoverPoint( CCoverPoint *pCoverPoint, const Vector3& vDirection, Vector3 &vCoverCoors, Vector3& vVantagePointCoors, bool bIgnoreProvidesCoverCheck )
{
	PF_FUNC(CalculateVantagePointForCoverPoint);

	vVantagePointCoors = vCoverCoors;
	if( pCoverPoint->GetType() != CCoverPoint::COVTYPE_POINTONMAP &&
		pCoverPoint->GetType() != CCoverPoint::COVTYPE_SCRIPTED &&
		pCoverPoint->GetType() != CCoverPoint::COVTYPE_VEHICLE &&
		pCoverPoint->GetType() != CCoverPoint::COVTYPE_BIG_OBJECT&&
		pCoverPoint->GetType() != CCoverPoint::COVTYPE_OBJECT )
	{
		// Thats it all non-map and non-vehicle vantage points are in the same location
		return;
	}

	// A low cover point's vantage point is in the same place as the original cover point
	if( pCoverPoint->GetUsage() == CCoverPoint::COVUSE_WALLTOBOTH )
	{
		//! If local player is close to an edge, use step out position as vantage point.
		if(pCoverPoint->m_pPedsUsingThisCover[0] && 
			pCoverPoint->m_pPedsUsingThisCover[0]->IsLocalPlayer() && pCoverPoint->GetHeight()==CCoverPoint::COVHEIGHT_TOOHIGH)
		{
			CTaskCover *pTaskCover = static_cast<CTaskCover*>(pCoverPoint->m_pPedsUsingThisCover[0]->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
			if(pTaskCover && pTaskCover->IsCoverFlagSet(CTaskCover::CF_CloseToPossibleCorner))
			{
				bool bFaceLeft = pTaskCover->IsCoverFlagSet(CTaskCover::CF_FacingLeft);
				vVantagePointCoors = CTaskAimGunFromCoverIntro::ComputeHighCoverStepOutPosition(bFaceLeft, *CGameWorld::FindLocalPlayer(), *pCoverPoint);	
				return;
			}
		}
		
		vVantagePointCoors.z += COVERPOINT_LOS_Z_ELEVATION;
		return;
	}

	if( !bIgnoreProvidesCoverCheck && !DoesCoverPointProvideCoverFromDirection(pCoverPoint, pCoverPoint->GetArc(), vDirection ) )
	{
		vVantagePointCoors.z += COVERPOINT_LOS_Z_ELEVATION;
		return;
	}


	// If the coverpoint allows firing both to the left and right, calculate the best cover point depending on
	// the direction toward the target
	CCoverPoint::eCoverUsage coverUsage = pCoverPoint->GetUsage();
	if( coverUsage == CCoverPoint::COVUSE_WALLTONEITHER )
	{
		//Vector3 vRight;
		//Vector3 vCoverDirection;

		// If the cover is from any direction, the vantage point can be on either side
		// So work out which the ped is closer to
		if( pCoverPoint->GetDirection() == COVER_POINT_ANY_DIR )
		{
			//vCoverDirection = vDirection;
			coverUsage = CCoverPoint::COVUSE_WALLTOLEFT;//fwRandom::GetRandomTrueFalse() ? CCoverPoint::COVUSE_WALLTOLEFT : CCoverPoint::COVUSE_WALLTORIGHT;
		}
		else
		{
			const Vector3 vCoverDirection = VEC3V_TO_VECTOR3(pCoverPoint->GetCoverDirectionVector(&RCC_VEC3V(vDirection)));
			Vector3 vRight;
			vRight.Cross( vCoverDirection, Vector3( 0.0f, 0.0f, 1.0f ) );
			vRight.NormalizeFast();
			float fDot = vRight.Dot( vDirection );
			const float fDotLimit = 0.0f;

			// Override the cover usage depending on whether the vantage point should be to
			// the right or left
			if( fDot <= -fDotLimit )
			{
				coverUsage = CCoverPoint::COVUSE_WALLTORIGHT;
			}
			else if( fDot >= fDotLimit )
			{
				coverUsage = CCoverPoint::COVUSE_WALLTOLEFT;
			}		
		}
	}

	// If cover should have a step out position as vantage point
	if( (pCoverPoint->GetFlag(CCoverPoint::COVFLAGS_IsLowCorner) || pCoverPoint->GetHeight() == CCoverPoint::COVHEIGHT_TOOHIGH) &&
		(coverUsage == CCoverPoint::COVUSE_WALLTOLEFT || coverUsage == CCoverPoint::COVUSE_WALLTORIGHT) )
	{
		const bool bFaceLeft = coverUsage == CCoverPoint::COVUSE_WALLTOLEFT ? false : true;
		// Use the step out position as the vantage
		vVantagePointCoors = CTaskAimGunFromCoverIntro::ComputeHighCoverStepOutPosition(bFaceLeft, *CGameWorld::FindLocalPlayer(), *pCoverPoint);
		vVantagePointCoors.z += (COVERPOINT_LOS_Z_ELEVATION - 1.0f);
	}
	else // otherwise just elevate for popover
	{
		vVantagePointCoors.z += COVERPOINT_LOS_Z_ELEVATION;
	}
}

//////////////////////////////////////////////////////
// FUNCTION: CalculateVantagePointFromPosition
// PURPOSE:  Calculates the nearest vantage point from
//			 the cover point
//////////////////////////////////////////////////////
void CCover::CalculateVantagePointFromPosition( CCoverPoint *pCoverPoint, const Vector3& vPosition, const Vector3& vDirection, Vector3 &vCoverCoors, Vector3& vVantagePointCoors )
{
	vVantagePointCoors = vCoverCoors;
	if( pCoverPoint->GetType() != CCoverPoint::COVTYPE_POINTONMAP &&
		pCoverPoint->GetType() != CCoverPoint::COVTYPE_SCRIPTED &&
		pCoverPoint->GetType() != CCoverPoint::COVTYPE_VEHICLE &&
		pCoverPoint->GetType() != CCoverPoint::COVTYPE_BIG_OBJECT&&
		pCoverPoint->GetType() != CCoverPoint::COVTYPE_OBJECT )
	{
		// Thats it all non-map and non-vehicle vantage points are in the same location
		return;
	}

	// A low cover point's vantage point is in the same place as the original cover point
	if( pCoverPoint->GetUsage() == CCoverPoint::COVUSE_WALLTOBOTH )
	{
		vVantagePointCoors.z += COVERPOINT_LOS_Z_ELEVATION;
		return;
	}

	// If the coverpoint allows firing both to the left and right, calculate teh best cover point depending on
	// the direction toward the target
	CCoverPoint::eCoverUsage coverUsage = pCoverPoint->GetUsage();
	if( coverUsage == CCoverPoint::COVUSE_WALLTONEITHER )
	{
		//Vector3 vRight;
		//Vector3 vCoverDirection;

		// If the cover is from any direction, the vantage point can be on either side
		// So work out which the ped is closer too
		if( pCoverPoint->GetDirection() == COVER_POINT_ANY_DIR )
		{
			//vCoverDirection = vDirection;
			coverUsage = CCoverPoint::COVUSE_WALLTOLEFT;//fwRandom::GetRandomTrueFalse() ? CCoverPoint::COVUSE_WALLTOLEFT : CCoverPoint::COVUSE_WALLTORIGHT;
		}
		else
		{
			const Vector3 vCoverDirection = VEC3V_TO_VECTOR3(pCoverPoint->GetCoverDirectionVector(&RCC_VEC3V(vDirection)));
			Vector3 vRight;
			vRight.Cross( vCoverDirection, Vector3( 0.0f, 0.0f, 1.0f ) );
			vRight.NormalizeFast();
			Vector3 vCover;
			pCoverPoint->GetCoverPointPosition(vCover);
			Vector3 vFromPedToCover = vCover - vPosition;
			vFromPedToCover.NormalizeFast();
			float fDot = vRight.Dot( vFromPedToCover );

			// Override the cover usage depending on whether the vantage point should be to
			// the right or left
			if( fDot <= 0.0f )
			{
				coverUsage = CCoverPoint::COVUSE_WALLTOLEFT;
			}
			else if( fDot > 0.0f )
			{
				coverUsage = CCoverPoint::COVUSE_WALLTORIGHT;
			}		
		}
	}

	// A low cover point's vantage point is in the same place as the original cover point
	if( coverUsage == CCoverPoint::COVUSE_WALLTOLEFT || coverUsage == CCoverPoint::COVUSE_WALLTORIGHT )
	{
		Vector3 vVantageDir = VEC3V_TO_VECTOR3(pCoverPoint->GetCoverDirectionVector(&RCC_VEC3V(vDirection)));
		// Work out the correct vantage point by rotating the direction a quater of a full rotation left or right
		if( coverUsage == CCoverPoint::COVUSE_WALLTOLEFT )
		{
			vVantageDir.RotateZ(-HALF_PI);
		}
		else
		{
			vVantageDir.RotateZ(HALF_PI);
		}

		// Add on the vantage direction to the coors
		float fDot;
		float fAdditionalCoverMovement = CTaskInCover::GetCoverOutModifier(coverUsage == CCoverPoint::COVUSE_WALLTOLEFT, VEC3V_TO_VECTOR3(pCoverPoint->GetCoverDirectionVector(&RCC_VEC3V(vDirection))), vDirection, fDot);
#if DEBUG_DRAW
		BANK_ONLY(fAdditionalCoverMovement *= CCoverDebug::ms_Tunables.m_CoverModifier;)
#endif
		vVantagePointCoors += vVantageDir * fAdditionalCoverMovement;
		vVantagePointCoors.z += COVERPOINT_LOS_Z_ELEVATION;
	}
}

#define IN_COVER_DISTANCE_FROM_OBJECT (0.35f)
#define IN_COVER_DISTANCE_FROM_CIRCULUAR_OBJECT (0.55f)

//////////////////////////////////////////////////////
// FUNCTION: FindCoordinatesCoverPointNotReserved
// PURPOSE:  Given a pointer to a coverpoint this function will return the coordinates.
//			 If the cover point is an object or vehicle the function will calculate
//			 an appropriate coordinate to hide from the opponent.
//////////////////////////////////////////////////////

bool CCover::FindCoordinatesCoverPointNotReserved(CCoverPoint *pCoverPoint, const Vector3 &vDirection, Vector3 &ResultCoors, Vector3 *pVantagePointCoors)
{
	switch(pCoverPoint->GetType())
	{
		case CCoverPoint::COVTYPE_NONE:
			Assert(false);
			break;
			
		case CCoverPoint::COVTYPE_OBJECT:
			{
				CObject *pObj = (CObject *)pCoverPoint->m_pEntity.Get();
				if (pObj)
				{
					Assert( pObj->GetBaseModelInfo() );

					const float	Radius = IN_COVER_DISTANCE_FROM_OBJECT;
					Vector3	Diff = vDirection;//TargetCoors - pObj->GetPosition();
					Diff.z = 0.0f;
					Diff.Normalize();

					// Bounds default to model bounds
					Vector3 vBoundsMin = pObj->GetBoundingBoxMin();
					Vector3 vBoundsMax = pObj->GetBoundingBoxMax();

					CCachedObjectCoverManager::GetCorrectedBoundsForObject(pObj, vBoundsMin, vBoundsMax);

					// Calculate the elliptical position around the object
					const Vector3 vecObjRight(VEC3V_TO_VECTOR3(pObj->GetTransform().GetA()));
					const Vector3 vecObjForward(VEC3V_TO_VECTOR3(pObj->GetTransform().GetB()));
					const float fX = DotProduct(vecObjRight, Diff) * Max(vBoundsMax.x, fabsf(vBoundsMin.x));
					const float fY = DotProduct(vecObjForward, Diff) * Max(vBoundsMax.y, fabsf(vBoundsMin.y));
					const Vector3 vOffset = ( vecObjRight * fX ) + ( vecObjForward * fY );

					ResultCoors = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition()) - vOffset - (Diff * Radius);
					if( pVantagePointCoors )
					{
						CalculateVantagePointForCoverPoint( pCoverPoint, vDirection, ResultCoors, *pVantagePointCoors );
					}
					return true;
				}
			}
			break;

		case CCoverPoint::COVTYPE_BIG_OBJECT:
		case CCoverPoint::COVTYPE_VEHICLE:
			{
				if (pCoverPoint->m_pEntity.Get())
				{
					// Take the local cover offset from the ped if a valid pointer
					Vector3 vOffset(0.0f, 0.0f, 0.0f);
					pCoverPoint->GetCoverPointPosition(ResultCoors, &vOffset);
					if( pVantagePointCoors )
					{
						//*pVantagePointCoors = ResultCoors;
						//pVantagePointCoors->z += COVERPOINT_LOS_Z_ELEVATION;
						// Calculate the vantage point
						CalculateVantagePointForCoverPoint( pCoverPoint, vDirection, ResultCoors, *pVantagePointCoors );
					}
					return true;
				}
			}
			break;

		case CCoverPoint::COVTYPE_POINTONMAP:
		case CCoverPoint::COVTYPE_SCRIPTED:
			{
				// Take the local cover offset from the ped if a valid pointer
				Vector3 vOffset(0.0f, 0.0f, 0.0f);
				pCoverPoint->GetCoverPointPosition(ResultCoors, &vOffset);

				// Move the position to the vantage until we have a new set of clips
				// for firing round corners
	/*			if( pCoverPoint->GetHeight() == CCoverPoint::COVHEIGHT_TOOHIGH )
				{
					Vector3 vVantage;
					CalculateVantagePointForCoverPoint( pCoverPoint, vDirection, ResultCoors, vVantage );
					ResultCoors = vVantage;
				}*/

				// If no vantage vector passed, no need to calculate the firing position
				if( !pVantagePointCoors )
				{
					return true;
				}

				// Calculate the vantage point
				CalculateVantagePointForCoverPoint( pCoverPoint, vDirection, ResultCoors, *pVantagePointCoors );
			}
			return true;
	}

	return false;
}

bool CCover::GetPedsCoverHeight(const CPed& rPed, CCoverPoint::eCoverHeight& coverPointHeight)
{
	if (rPed.GetCoverPoint())
	{
		coverPointHeight = rPed.GetCoverPoint()->GetHeight();
		return true;
	}
	else if (rPed.IsNetworkClone())
	{
		const CTaskCover* pCoverTask = static_cast<const CTaskCover*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
		if (pCoverTask)
		{
			return pCoverTask->GetNetCoverPointHeight(rPed, coverPointHeight);
		}
	}
	return false;
}

bool CCover::FindCoverDirectionForPed(const CPed& rPed, Vector3& vDirection, const Vector3& vToTarget)
{
	if (rPed.GetCoverPoint())
	{
		vDirection = VEC3V_TO_VECTOR3(rPed.GetCoverPoint()->GetCoverDirectionVector(&RCC_VEC3V(vToTarget)));
		return true;
	}
	else if (rPed.IsNetworkClone())
	{
		const CTaskCover* pCoverTask = static_cast<const CTaskCover*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
		if (pCoverTask)
		{
			return pCoverTask->GetNetCoverPointDirection(rPed, vDirection);
		}
	}
	return false;
}

bool CCover::FindCoverCoordinatesForPed(const CPed& rPed, const Vector3& vDirection, Vector3& vCoverCoords, Vector3* pVantagePointCoors, Vector3 *pCoverDirection)
{
	if (rPed.GetCoverPoint())
	{
		if(pCoverDirection)
		{
			*pCoverDirection = VEC3V_TO_VECTOR3(rPed.GetCoverPoint()->GetCoverDirectionVector());
		}
		return FindCoordinatesCoverPoint(rPed.GetCoverPoint(), &rPed, vDirection, vCoverCoords, pVantagePointCoors);
	}
	else if (rPed.IsNetworkClone())
	{
		const CTaskCover* pCoverTask = static_cast<const CTaskCover*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
		if (pCoverTask)
		{
			if (pCoverTask->GetNetCoverPointPosition(rPed, vCoverCoords))
			{
				if (pVantagePointCoors)
				{
					Vector3 vCoverDirection;
					if (pCoverTask->GetNetCoverPointDirection(rPed, vCoverDirection))
					{
						if(pCoverDirection)
						{
							*pCoverDirection = vCoverDirection;
						}

						CCoverPoint::eCoverUsage coverPointUsage;
						if (pCoverTask->GetNetCoverPointUsage(rPed, coverPointUsage))
						{
							CCoverPoint tempCoverPoint;
							tempCoverPoint.SetType(CCoverPoint::COVTYPE_POINTONMAP);
							tempCoverPoint.SetWorldPosition(vCoverCoords);
							tempCoverPoint.SetWorldDirection(CCover::FindDirFromVector(vCoverDirection));
							tempCoverPoint.SetUsage(coverPointUsage);
							CalculateVantagePointForCoverPoint(&tempCoverPoint, vDirection, vCoverCoords, *pVantagePointCoors, true);
						}
					}
				}
				return true;
			}
		}
	}
	return false;
}

bool CCover::FindCoordinatesCoverPoint(CCoverPoint *pCoverPoint, const CPed *pPed, const Vector3 &vDirection, Vector3 &ResultCoors, Vector3 *pVantagePointCoors )
{
	Assert(pCoverPoint);

	bool	bOK = false;
	for (s32 C = 0; C < MAX_PEDS_PER_COVER_POINT; C++)
	{
		if (pCoverPoint->m_pPedsUsingThisCover[C] == pPed)
		{
			bOK = true;
		}
	
	}
	if (!bOK)
	{
		return false;
	}

	return FindCoordinatesCoverPointNotReserved(pCoverPoint, vDirection, ResultCoors, pVantagePointCoors);
}


//////////////////////////////////////////////////////
// FUNCTION: Render
// PURPOSE:
//////////////////////////////////////////////////////

#if !__FINAL
void CCover::Render()
{
#if DEBUG_DRAW
	char debugText[256];

	// Render the cover area bounding boxes
	RenderCoverAreaBoundingBoxes();

	if (CCoverDebug::ms_Tunables.m_BoundingAreas.m_RenderCells)
	{
		RenderCoverCells();
	}

	if (!CCoverDebug::ms_Tunables.m_RenderCoverPoints) return;

	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);

	// Are to any cover point that provides cover from the player
	CPed* pFocusPed = NULL;
	if(pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEntity);
	}

	CPed* pPedToCheck =  pFocusPed ?  pFocusPed : CGameWorld::FindLocalPlayer();

	// LEGEND!
	static Vec2V svStartPos(50.0f,75.0f);
	Vec2V vStartPos = svStartPos;
	static s32 iVerticalSpace = 10;

	u32 iNoTexts = 0;

	if (CCoverDebug::ms_Tunables.m_RenderCoverPointHelpText)
	{
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_grey, "==================================================================");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_grey, "                 COVER TYPE                                       ");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_grey, "==================================================================");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);

		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_blue, "Auto Generated Cover Point (Bottom is position)");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_orange, "Script Generated Cover Point");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_purple, "Player Prioritised Cover Point");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_green, "Vantage Point");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_white, "Cover Direction");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);

		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_grey, "==================================================================");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);

		char szText[256];
		formatf(szText, "COVER STATUS TO FOCUS PED WITH ADDRESS 0x%X (DEFAULT PLAYER)", pPedToCheck);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_grey, szText);
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_grey, "==================================================================");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);

		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_SkyBlue, "Provides cover and has clear vantage to focus ped");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_red, "Provides cover, but no line of sight to focus ped");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_black, "Cover position is blocked by something");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, Color_pink, "Doesn't provide cover to focus ped");
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
		vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);
	}

	sprintf(debugText, "Num map/dynamic coverpoints : %i/%i", ms_iCurrentNumMapAndDynamicCoverPts, NUM_NAVMESH_COVERPOINTS);
	grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, (ms_iCurrentNumMapAndDynamicCoverPts>=NUM_NAVMESH_COVERPOINTS)?Color_red:Color_SkyBlue, debugText);
	vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);

	sprintf(debugText, "Num script coverpoints : %i/%i", ms_iCurrentNumScriptedCoverPts, NUM_SCRIPTED_COVERPOINTS);
	grcDebugDraw::Text(vStartPos, DD_ePCS_Pixels, (ms_iCurrentNumScriptedCoverPts>=NUM_SCRIPTED_COVERPOINTS)?Color_red:Color_SkyBlue, debugText);
	vStartPos.SetYf(svStartPos.GetYf() + ++iNoTexts*iVerticalSpace);

	int iNumMapCoverPoints = 0;
	int iNumObjectCoverPoints = 0;
	int iNumVehicleCoverPoints = 0;
	int iNumScriptedCoverPoints = 0;

	bool bIsFocusPedInCombat = pFocusEntity && pFocusEntity->GetIsTypePed() && static_cast<CPed*>(pFocusEntity)->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT);
	CEntity* pTargetEntity = bIsFocusPedInCombat ? static_cast<CPed*>(pFocusEntity)->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_COMBAT) : NULL;

	for (s32 C = 0; C < NUM_COVERPOINTS; C++)
	{
		if(!m_UsedCoverPoints.IsSet(C))
		{
			Assert(m_aPoints[C].GetType() == CCoverPoint::COVTYPE_NONE);
			continue;
		}
		Assert(m_aPoints[C].GetType() != CCoverPoint::COVTYPE_NONE);

		if( RemoveCoverPointIfEntityLost(&m_aPoints[C]) )
		{
			continue;
		}

		for (s32 i = 0; i < MAX_PEDS_PER_COVER_POINT; i++)
		{
			if( m_aPoints[C].m_pPedsUsingThisCover[i].Get() != NULL )
			{
				Assert( m_aPoints[C].m_pPedsUsingThisCover[i]->GetCoverPoint() == &m_aPoints[C] );
			}
		}

		// Only render coverpoints in the focus areas defensive area or attack window
		if( ((CPedDebugVisualiserMenu::IsVisualisingDefensiveAreas() && CPedDebugVisualiserMenu::IsDisplayingOnlyForFocus()) || 
			CPedDebugVisualiserMenu::IsVisualisingAttackWindowForFocusPed()) &&
			pFocusEntity)
		{
			if(!pFocusEntity->GetIsTypePed())
			{
				continue;
			}

			CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
			if(pFocusPed->GetPedIntelligence()->GetDefensiveArea()->IsActive())
			{
				if( CPedDebugVisualiserMenu::IsVisualisingDefensiveAreas())
				{
					Vector3 vCoverPos;
					m_aPoints[C].GetCoverPointPosition( vCoverPos );
					if( !pFocusPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(vCoverPos ) )
					{
						continue;
					}
				}
			}
			else if(CPedDebugVisualiserMenu::IsVisualisingAttackWindowForFocusPed() &&
					bIsFocusPedInCombat && pTargetEntity && pTargetEntity->GetIsTypePed())
			{
				Vector3 vCoverPos;
				m_aPoints[C].GetCoverPointPosition( vCoverPos );
				if(CTaskMoveWithinAttackWindow::IsPositionOutsideAttackWindow(pFocusPed, static_cast<CPed*>(pTargetEntity), RCC_VEC3V(vCoverPos), true))
				{
					continue;
				}
			}
		}

		Vector3	DrawCoors;

		switch (m_aPoints[C].GetType())
		{
			case CCoverPoint::COVTYPE_OBJECT:
				if (m_aPoints[C].m_pEntity)
				{
					Vector3 vCoverDir = Vector3(0.0f, 1.0f, 0.0f);
					DrawCoors = VEC3V_TO_VECTOR3(m_aPoints[C].m_pEntity->GetTransform().GetPosition());
					DrawCoors.z += m_aPoints[C].m_pEntity->GetBoundingBoxMax().z;

					if (m_aPoints[C].GetFlag(CCoverPoint::COVFLAGS_IsPriority))
					{
						grcDebugDraw::Sphere(DrawCoors + Vector3(0.0f,0.0f,COVERPOINT_LOS_Z_ELEVATION - 0.05f), 0.025f, Color_purple);	
					}

					grcDebugDraw::Line(DrawCoors, DrawCoors + Vector3(0.0f, 0.0f, 2.0f), Color_blue);
					// Also draw a little indicator for the direction. (points in the direction of the cover (wall etc))
					grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()), Color_white);
					
					if(CCoverDebug::ms_Tunables.m_RenderCoverPointArcs)
					{
						Vector3 vCoverArcDir1;
						Vector3 vCoverArcDir2;
						CalculateCoverArcDirections(m_aPoints[C], vCoverArcDir1, vCoverArcDir2);
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + vCoverArcDir1, Color_orange, Color_orange);
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + vCoverArcDir2, Color_orange, Color_orange);
					}
					
					// Also draw an indicator showing the position of the cover points vantage location
					Vector3 vVantageCoors;
					CalculateVantagePointForCoverPoint( &m_aPoints[C], vCoverDir, DrawCoors, vVantageCoors );
					grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), vVantageCoors, Color_green);

					if( CCoverDebug::ms_Tunables.m_RenderCoverPointAddresses )
					{
						char szAddress[128];
						sprintf(szAddress, "CP(%p) E(%p)", &m_aPoints[C], m_aPoints[C].m_pEntity.Get());
						grcDebugDraw::Text(DrawCoors, Color_white, 0, 0, szAddress);
					}

					if( CCoverDebug::ms_Tunables.m_RenderCoverPointTypes )
					{
						char szType[128];
						formatf(szType, "Type: OBJECT");
						grcDebugDraw::Text(DrawCoors, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight(), szType);
					}

					// If possible to fire either side, render both sides
					if( m_aPoints[C].GetUsage() == CCoverPoint::COVUSE_WALLTONEITHER )
					{
						Vector3 vVantageCoors2 = vVantageCoors - DrawCoors;
						vVantageCoors2 = DrawCoors - vVantageCoors2;
						vVantageCoors2.z = vVantageCoors.z;
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), vVantageCoors2, Color_green);
					}
					iNumObjectCoverPoints++;
				}
				break;
			case CCoverPoint::COVTYPE_BIG_OBJECT:
				if (m_aPoints[C].m_pEntity)
				{
					static bool sbDisplayLocalVec = false;
					Vector3 vCoverDir = VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector(NULL));
					if( sbDisplayLocalVec )
					{
						vCoverDir = VEC3V_TO_VECTOR3(m_aPoints[C].m_pEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_aPoints[C].GetLocalDirectionVector())));
					}
					m_aPoints[C].GetCoverPointPosition(DrawCoors);

					if (m_aPoints[C].GetFlag(CCoverPoint::COVFLAGS_IsPriority))
					{
						grcDebugDraw::Sphere(DrawCoors + Vector3(0.0f,0.0f,COVERPOINT_LOS_Z_ELEVATION - 0.05f), 0.025f, Color_purple);	
					}

					grcDebugDraw::Line(DrawCoors, DrawCoors + Vector3(0.0f, 0.0f, 1.0f), Color_blue);
					// Also draw a little indicator for the direction. (points in the direction of the cover (wall etc))
					grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()), Color_white);
					
					if(CCoverDebug::ms_Tunables.m_RenderCoverPointArcs)
					{
						Vector3 vCoverArcDir1;
						Vector3 vCoverArcDir2;
						CalculateCoverArcDirections(m_aPoints[C], vCoverArcDir1, vCoverArcDir2);
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + vCoverArcDir1, Color_orange, Color_orange);
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + vCoverArcDir2, Color_orange, Color_orange);
					}

					if(CCoverDebug::ms_Tunables.m_RenderCoverPointHeightRulers)
					{
						// Set the measuring stick just behind the cover position at object min height
						Vector3 vMeasureStickA = DrawCoors - (0.05f * VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()));
						Vector3 vMeasureStickB = vMeasureStickA;

						switch(m_aPoints[C].GetHeight())
						{
						case CCoverPoint::COVHEIGHT_LOW:
							vMeasureStickB.z = DrawCoors.z + LOW_COVER_MAX_HEIGHT;
							grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vMeasureStickA), VECTOR3_TO_VEC3V(vMeasureStickB), 0.01f, Color_blue, Color_green);
							break;
						case CCoverPoint::COVHEIGHT_HIGH:
							vMeasureStickB.z = DrawCoors.z + HIGH_COVER_MAX_HEIGHT;
							grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vMeasureStickA), VECTOR3_TO_VEC3V(vMeasureStickB), 0.01f, Color_blue, Color_yellow);
							break;
						default:
						case CCoverPoint::COVHEIGHT_TOOHIGH:
							vMeasureStickB.z = DrawCoors.z + HIGH_COVER_MAX_HEIGHT + 0.4f;
							grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vMeasureStickA), VECTOR3_TO_VEC3V(vMeasureStickB), 0.01f, Color_blue, Color_red);
							break;
						}
					}

					if(CCoverDebug::ms_Tunables.m_RenderCoverPointUsage)
					{
						// Set the draw position relative to the cover position
						Vector3 vDrawPosition = DrawCoors - (0.05f * VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()));
						vDrawPosition.z = DrawCoors.z + LOW_COVER_MAX_HEIGHT / 2.0f;

						switch(m_aPoints[C].GetUsage())
						{
							case CCoverPoint::COVUSE_WALLTOLEFT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTOLEFT");
								break;
							case CCoverPoint::COVUSE_WALLTORIGHT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTORIGHT");
								break;
							case CCoverPoint::COVUSE_WALLTOBOTH:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTOBOTH");
								break;
							case CCoverPoint::COVUSE_WALLTONEITHER:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTONEITHER");
								break;
							case CCoverPoint::COVUSE_WALLTOLEFTANDLOWRIGHT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTOLEFTANDLOWRIGHT");
								break;
							case CCoverPoint::COVUSE_WALLTORIGHTANDLOWLEFT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTORIGHTANDLOWLEFT");
								break;
							default:
								break;
						}
					}

					if(CCoverDebug::ms_Tunables.m_RenderCoverPointLowCorners)
					{
						if(m_aPoints[C].GetFlag(CCoverPoint::COVFLAGS_IsLowCorner))
						{
							// Set the draw position relative to the cover position
							Vector3 vDrawPosition = DrawCoors - (0.05f * VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()));
							vDrawPosition.z = DrawCoors.z + LOW_COVER_MAX_HEIGHT;
							grcDebugDraw::Sphere(vDrawPosition, 0.05f, Color_red);
						}
					}
					
					// Also draw an indicator showing the position of the cover points vantage location
					Vector3 vVantageCoors;
					CalculateVantagePointForCoverPoint( &m_aPoints[C], vCoverDir, DrawCoors, vVantageCoors );
					grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), vVantageCoors, Color_green);
				
					grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), VEC3V_TO_VECTOR3(m_aPoints[C].m_pEntity->GetTransform().GetPosition()), Color_white);

					if( CCoverDebug::ms_Tunables.m_RenderCoverPointAddresses )
					{
						char szAddress[128];
						sprintf(szAddress, "CP(%p) E(%p)", &m_aPoints[C], m_aPoints[C].m_pEntity.Get());
						grcDebugDraw::Text(DrawCoors, Color_white, 0, 0, szAddress);
					}

					if( CCoverDebug::ms_Tunables.m_RenderCoverPointTypes )
					{
						char szType[128];
						formatf(szType, "Type: BIG OBJECT");
						grcDebugDraw::Text(DrawCoors, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight(), szType);
					}

					// If possible to fire either side, render both sides
					if( m_aPoints[C].GetUsage() == CCoverPoint::COVUSE_WALLTONEITHER )
					{
						Vector3 vVantageCoors2 = vVantageCoors - DrawCoors;
						vVantageCoors2 = DrawCoors - vVantageCoors2;
						vVantageCoors2.z = vVantageCoors.z;
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), vVantageCoors2, Color_green);
					}

					iNumObjectCoverPoints++;
				}
				break;
			case CCoverPoint::COVTYPE_VEHICLE:
				if (m_aPoints[C].m_pEntity)
				{
					Vector3 vCoverDir = VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector(NULL));
					m_aPoints[C].GetCoverPointPosition(DrawCoors);

					if (m_aPoints[C].GetFlag(CCoverPoint::COVFLAGS_IsPriority))
					{
						grcDebugDraw::Sphere(DrawCoors + Vector3(0.0f,0.0f,COVERPOINT_LOS_Z_ELEVATION - 0.05f), 0.025f, Color_purple);	
					}

					grcDebugDraw::Line(DrawCoors, DrawCoors + Vector3(0.0f, 0.0f, 1.0f), Color_blue);
					// Also draw a little indicator for the direction. (points in the direction of the cover (wall etc))
					grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()), Color_white);
					
					if(CCoverDebug::ms_Tunables.m_RenderCoverPointArcs)
					{
						Vector3 vCoverArcDir1;
						Vector3 vCoverArcDir2;
						CalculateCoverArcDirections(m_aPoints[C], vCoverArcDir1, vCoverArcDir2);
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + vCoverArcDir1, Color_orange, Color_orange);
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + vCoverArcDir2, Color_orange, Color_orange);
					}

					if(CCoverDebug::ms_Tunables.m_RenderCoverPointHeightRulers)
					{
						// Set the measuring stick just behind the cover position at object min height
						Vector3 vMeasureStickA = DrawCoors - (0.05f * VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()));
						Vector3 vMeasureStickB = vMeasureStickA;

						switch(m_aPoints[C].GetHeight())
						{
						case CCoverPoint::COVHEIGHT_LOW:
							vMeasureStickB.z = DrawCoors.z + LOW_COVER_MAX_HEIGHT;
							grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vMeasureStickA), VECTOR3_TO_VEC3V(vMeasureStickB), 0.01f, Color_blue, Color_green);
							break;
						case CCoverPoint::COVHEIGHT_HIGH:
							vMeasureStickB.z = DrawCoors.z + HIGH_COVER_MAX_HEIGHT;
							grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vMeasureStickA), VECTOR3_TO_VEC3V(vMeasureStickB), 0.01f, Color_blue, Color_yellow);
							break;
						default:
						case CCoverPoint::COVHEIGHT_TOOHIGH:
							vMeasureStickB.z = DrawCoors.z + HIGH_COVER_MAX_HEIGHT + 0.4f;
							grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vMeasureStickA), VECTOR3_TO_VEC3V(vMeasureStickB), 0.01f, Color_blue, Color_red);
							break;
						}
					}

					if(CCoverDebug::ms_Tunables.m_RenderCoverPointUsage)
					{
						// Set the draw position relative to the cover position
						Vector3 vDrawPosition = DrawCoors - (0.05f * VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()));
						vDrawPosition.z = DrawCoors.z + LOW_COVER_MAX_HEIGHT / 2.0f;

						switch(m_aPoints[C].GetUsage())
						{
							case CCoverPoint::COVUSE_WALLTOLEFT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTOLEFT");
								break;
							case CCoverPoint::COVUSE_WALLTORIGHT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTORIGHT");
								break;
							case CCoverPoint::COVUSE_WALLTOBOTH:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTOBOTH");
								break;
							case CCoverPoint::COVUSE_WALLTONEITHER:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTONEITHER");
								break;
							case CCoverPoint::COVUSE_WALLTOLEFTANDLOWRIGHT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTOLEFTANDLOWRIGHT");
								break;
							case CCoverPoint::COVUSE_WALLTORIGHTANDLOWLEFT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTORIGHTANDLOWLEFT");
								break;
							default:
								break;
						}
					}

					if(CCoverDebug::ms_Tunables.m_RenderCoverPointLowCorners)
					{
						if(m_aPoints[C].GetFlag(CCoverPoint::COVFLAGS_IsLowCorner))
						{
							// Set the draw position relative to the cover position
							Vector3 vDrawPosition = DrawCoors - (0.05f * VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()));
							vDrawPosition.z = DrawCoors.z + LOW_COVER_MAX_HEIGHT;
							grcDebugDraw::Sphere(vDrawPosition, 0.05f, Color_red);
						}
					}
					
					// Also draw an indicator showing the position of the cover points vantage location
					Vector3 vVantageCoors;
					CalculateVantagePointForCoverPoint( &m_aPoints[C], vCoverDir, DrawCoors, vVantageCoors );
					grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), vVantageCoors, Color_green);
					if( CCoverDebug::ms_Tunables.m_RenderCoverPointAddresses )
					{
						char szAddress[128];
						sprintf(szAddress, "CP(%p) E(%p)", &m_aPoints[C], m_aPoints[C].m_pEntity.Get());
						grcDebugDraw::Text(DrawCoors, Color_white, 0, 0, szAddress);
					}

					if( CCoverDebug::ms_Tunables.m_RenderCoverPointTypes )
					{
						char szType[128];
						formatf(szType, "Type: VEHICLE");
						grcDebugDraw::Text(DrawCoors, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight(), szType);
					}

					char szText[128];
					switch (m_aPoints[C].GetHeight())
					{
						case CCoverPoint::COVHEIGHT_LOW: formatf(szText, "Height: COVHEIGHT_LOW"); break;
						case CCoverPoint::COVHEIGHT_HIGH: formatf(szText, "Height: COVHEIGHT_HIGH"); break;
						case CCoverPoint::COVHEIGHT_TOOHIGH: formatf(szText, "Height: COVHEIGHT_TOOHIGH"); break;
					}
					grcDebugDraw::Text(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), Color_blue, szText);

					if (m_aPoints[C].GetVehicleHierarchyId() != VEH_INVALID_ID)
					{
						formatf(szText, "VEHICLE HIERARCHY ID : %i", m_aPoints[C].GetVehicleHierarchyId());
						grcDebugDraw::Text(DrawCoors + Vector3(0.0f, 0.0f, 0.8f), Color_red, szText);
					}
					
					iNumVehicleCoverPoints++;
				}
				break;
			case CCoverPoint::COVTYPE_POINTONMAP:
			case CCoverPoint::COVTYPE_SCRIPTED:
				{
					Vector3 vCoverDir = VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector(NULL));
					m_aPoints[C].GetCoverPointPosition(DrawCoors);

					if (m_aPoints[C].GetFlag(CCoverPoint::COVFLAGS_IsPriority))
					{
						grcDebugDraw::Sphere(DrawCoors + Vector3(0.0f,0.0f,COVERPOINT_LOS_Z_ELEVATION - 0.05f), 0.025f, Color_purple);	
					}

					if( m_aPoints[C].GetType() == CCoverPoint::COVTYPE_SCRIPTED )
					{
						grcDebugDraw::Line(DrawCoors, DrawCoors + Vector3(0.0f, 0.0f, 1.0f), Color_orange, Color_orange);
					}
					else
					{
						grcDebugDraw::Line(DrawCoors, DrawCoors + Vector3(0.0f, 0.0f, 1.0f), Color_blue, Color_blue);
					}

					// Also draw a little indicator for the direction. (points in the direction of the cover (wall etc))
					grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + VEC3V_TO_VECTOR3(FindVectorFromDir(m_aPoints[C].GetDirection())), Color_white, Color_white);
					
					if(CCoverDebug::ms_Tunables.m_RenderCoverPointArcs)
					{
						Vector3 vCoverArcDir1;
						Vector3 vCoverArcDir2;
						CalculateCoverArcDirections(m_aPoints[C], vCoverArcDir1, vCoverArcDir2);
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + vCoverArcDir1, Color_orange, Color_orange);
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), DrawCoors + Vector3(0.0f, 0.0f, 1.0f) + vCoverArcDir2, Color_orange, Color_orange);
					}

					if(CCoverDebug::ms_Tunables.m_RenderCoverPointHeightRulers)
					{
						// Set the measuring stick just behind the cover position at object min height
						Vector3 vMeasureStickA = DrawCoors - (0.05f * VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()));
						Vector3 vMeasureStickB = vMeasureStickA;

						switch(m_aPoints[C].GetHeight())
						{
						case CCoverPoint::COVHEIGHT_LOW:
							vMeasureStickB.z = DrawCoors.z + LOW_COVER_MAX_HEIGHT;
							grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vMeasureStickA), VECTOR3_TO_VEC3V(vMeasureStickB), 0.01f, Color_blue, Color_green);
							break;
						case CCoverPoint::COVHEIGHT_HIGH:
							vMeasureStickB.z = DrawCoors.z + HIGH_COVER_MAX_HEIGHT;
							grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vMeasureStickA), VECTOR3_TO_VEC3V(vMeasureStickB), 0.01f, Color_blue, Color_yellow);
							break;
						default:
						case CCoverPoint::COVHEIGHT_TOOHIGH:
							vMeasureStickB.z = DrawCoors.z + HIGH_COVER_MAX_HEIGHT + 0.4f;
							grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vMeasureStickA), VECTOR3_TO_VEC3V(vMeasureStickB), 0.01f, Color_blue, Color_red);
							break;
						}
					}

					if(CCoverDebug::ms_Tunables.m_RenderCoverPointUsage)
					{
						// Set the draw position relative to the cover position
						Vector3 vDrawPosition = DrawCoors - (0.05f * VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()));
						vDrawPosition.z = DrawCoors.z + LOW_COVER_MAX_HEIGHT / 2.0f;

						switch(m_aPoints[C].GetUsage())
						{
							case CCoverPoint::COVUSE_WALLTOLEFT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTOLEFT");
								break;
							case CCoverPoint::COVUSE_WALLTORIGHT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTORIGHT");
								break;
							case CCoverPoint::COVUSE_WALLTOBOTH:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTOBOTH");
								break;
							case CCoverPoint::COVUSE_WALLTONEITHER:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTONEITHER");
								break;
							case CCoverPoint::COVUSE_WALLTOLEFTANDLOWRIGHT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTOLEFTANDLOWRIGHT");
								break;
							case CCoverPoint::COVUSE_WALLTORIGHTANDLOWLEFT:
								grcDebugDraw::Text(vDrawPosition, Color_green, 0, 0, "WALLTORIGHTANDLOWLEFT");
								break;
							default:
								break;
						}
					}

					if(CCoverDebug::ms_Tunables.m_RenderCoverPointLowCorners)
					{
						if(m_aPoints[C].GetFlag(CCoverPoint::COVFLAGS_IsLowCorner))
						{
							// Set the draw position relative to the cover position
							Vector3 vDrawPosition = DrawCoors - (0.05f * VEC3V_TO_VECTOR3(m_aPoints[C].GetCoverDirectionVector()));
							vDrawPosition.z = DrawCoors.z + LOW_COVER_MAX_HEIGHT;
							grcDebugDraw::Sphere(vDrawPosition, 0.05f, Color_red);
						}
					}

					// Also draw an indicator showing the position of the cover points vantage location
					Vector3 vVantageCoors;
					CalculateVantagePointForCoverPoint( &m_aPoints[C], vCoverDir, DrawCoors, vVantageCoors );
					grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), vVantageCoors, Color_green, Color_green);
					if( CCoverDebug::ms_Tunables.m_RenderCoverPointAddresses )
					{
						char szAddress[128];
						sprintf(szAddress, "CP(%p)", &m_aPoints[C]);
						grcDebugDraw::Text(DrawCoors, Color_white, 0, 0, szAddress);
					}

					if( CCoverDebug::ms_Tunables.m_RenderCoverPointTypes )
					{
						char szType[128];
						formatf(szType, "Type: MAP (or SCRIPT)");
						grcDebugDraw::Text(DrawCoors, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight(), szType);
					}

					// If possible to fire either side, render both sides
					if( m_aPoints[C].GetUsage() == CCoverPoint::COVUSE_WALLTONEITHER )
					{
						Vector3 vVantageCoors2 = vVantageCoors - DrawCoors;
						vVantageCoors2 = DrawCoors - vVantageCoors2;
						vVantageCoors2.z = vVantageCoors.z;
						grcDebugDraw::Line(DrawCoors + Vector3(0.0f, 0.0f, 1.0f), vVantageCoors2, Color_green, Color_green);
					}
					iNumMapCoverPoints++;
				}
				break;
			case CCoverPoint::COVTYPE_NONE:
				Assert(false);
				break;
		}

		if( pPedToCheck )
		{
			Vector3 vPosition;
			Vector3 vVantagePos;
			m_aPoints[C].GetCoverPointPosition(vPosition);
			const Vector3 vPedToCheckPosition = VEC3V_TO_VECTOR3(pPedToCheck->GetTransform().GetPosition());
			Vector3 vPosToCheck = vPedToCheckPosition;

			// If the ped is in cover, work out the vantage location for the purposes of a LOS test
			if( pPedToCheck->GetCoverPoint() )
			{
				Vector3 vCoverPos;
				if(CCover::FindCoordinatesCoverPoint(pPedToCheck->GetCoverPoint(), pPedToCheck, vPosition - vPedToCheckPosition, vCoverPos, &vVantagePos))
				{
					vPosToCheck = vVantagePos;// + Vector3(0.0f,0.0f,1.75f);
				}
			}

			// Prevent z fighting
			DrawCoors += Vector3(0.0f, 0.0f, COVERPOINT_LOS_Z_ELEVATION);

			// Visualise test pos
			grcDebugDraw::Sphere( vPosToCheck, 0.05f, Color_red );

			// Visualise the status of the coverpoint, black if the coverpoint is blocked, yellow line, if its direction is blocked, blue line if clear,
			// red if not clear
			if( m_aPoints[C].GetStatus() & CCoverPoint::COVSTATUS_PositionBlocked )
			{
				grcDebugDraw::Line( DrawCoors, DrawCoors + Vector3(0.0f, 0.0f, 5.0f), Color_black, Color_black);
			}
			else if( m_aPoints[C].GetStatus() & CCoverPoint::COVSTATUS_DirectionBlocked )
			{
				grcDebugDraw::Line( DrawCoors, DrawCoors + Vector3(0.0f, 0.0f, 5.0f), Color_yellow, Color_yellow);
			}
			else if( DoesCoverPointProvideCover(&m_aPoints[C], m_aPoints[C].GetArc(), vPosToCheck, vPosition))
			{
				if(CCover::FindCoordinatesCoverPoint(&m_aPoints[C], NULL, vPosToCheck - vPosition, vPosition, &vVantagePos))
				{
					WorldProbe::CShapeTestProbeDesc probeDesc;
					probeDesc.SetStartAndEnd(vVantagePos, vPosToCheck);
					probeDesc.SetExcludeEntity(pPedToCheck->GetVehiclePedInside());
					probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
					probeDesc.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU);
					probeDesc.SetContext(WorldProbe::LOS_CombatAI);
					if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
					{
						grcDebugDraw::Line( DrawCoors, DrawCoors + Vector3(0.0f, 0.0f, 5.0f), Color_SkyBlue);
						grcDebugDraw::Line( vVantagePos, vVantagePos + Vector3(0.0f, 0.0f, 5.0f), Color_SkyBlue);
					}
					else
					{
						grcDebugDraw::Line( DrawCoors, DrawCoors + Vector3(0.0f, 0.0f, 5.0f), Color_red);
						grcDebugDraw::Line( vVantagePos, vVantagePos + Vector3(0.0f, 0.0f, 5.0f), Color_red);
					}
				}
			}
			else
			{
				grcDebugDraw::Sphere( DrawCoors, 0.025f, Color_pink);	
			}
		}

		// Also draw a line from the coverpoint to the ped that's using it at the moment.
		for (s32 i = 0; i < MAX_PEDS_PER_COVER_POINT; i++)
		{
			if (m_aPoints[C].GetType() != CCoverPoint::COVTYPE_NONE && m_aPoints[C].m_pPedsUsingThisCover[i])
			{
				const Vector3 vPedUsingThisCoverPosition = VEC3V_TO_VECTOR3(m_aPoints[C].m_pPedsUsingThisCover[i]->GetTransform().GetPosition());
				grcDebugDraw::Line(DrawCoors, vPedUsingThisCoverPosition, Color32(0x00, 0x00, 0xff, 0xff));
				Vector3 vCoverPosition;
				m_aPoints[C].GetCoverPointPosition(vCoverPosition, &m_aPoints[C].m_pPedsUsingThisCover[i]->GetLocalOffsetToCoverPoint() );
				grcDebugDraw::Line(vCoverPosition, vPedUsingThisCoverPosition, Color32(0xff, 0x00, 0x00, 0xff));
			}
		}
	}

	// Add some debug text displaying the total num coverpoints, and how many of each type.
	sprintf(debugText, "NumCoverPoints : %i/%i (Map : %i, Object : %i, Vehicle : %i, Scripted : %i) NumGrids : %i",
		iNumMapCoverPoints + iNumObjectCoverPoints + iNumVehicleCoverPoints + iNumScriptedCoverPoints,
		NUM_COVERPOINTS,
		iNumMapCoverPoints,
		iNumObjectCoverPoints,
		iNumVehicleCoverPoints,
		iNumScriptedCoverPoints,
		CCoverPointsContainer::GetNumGrids()
	);

	grcDebugDraw::AddDebugOutput(debugText);

#if !__FINAL && !__PPU
	sprintf(debugText, "Timings in Millisecs (Removal : %.2f, AddVehicles : %.2f, AddObjects : %.2f, AddFromMap : %.2f)",
		ms_fMsecsTakenToRemoveCoverPoints,
		ms_fMsecsTakenToAddFromVehicles,
		ms_fMsecsTakenToAddFromObjects,
		ms_fMsecsTakenToAddFromMap
	);

	grcDebugDraw::AddDebugOutput(debugText);
#endif
	
#endif
}
#endif	// !__FINAL

//////////////////////////////////////////////////////
// FUNCTION: RemoveCoverPointIfEntityLost
// RETURNS: true if the coverpiont was removed
//////////////////////////////////////////////////////

bool CCover::RemoveCoverPointIfEntityLost(CCoverPoint *pPoint)
{
	switch (pPoint->GetType())
	{
		case CCoverPoint::COVTYPE_OBJECT:
		case CCoverPoint::COVTYPE_VEHICLE:
		case CCoverPoint::COVTYPE_BIG_OBJECT:
			if (!pPoint->m_pEntity)
			{
				RemoveCoverPoint(pPoint);
				Assert(m_NumPoints >= 0);
				return true;
			}
			break;
		case CCoverPoint::COVTYPE_POINTONMAP:	// Entity pointer will always be NULL if the coverpoints have come from the navmesh
		case CCoverPoint::COVTYPE_SCRIPTED:
			break;
		case CCoverPoint::COVTYPE_NONE:
		default:
			Assert(false);
			break;
	}
	return false;
}

// PURPOSE:	Look-up table used by CCover::DoesCoverPointProvideCoverFromDirection().
// NOTES:	The X component should be the square of the cosine of the half-angle of the arc.
//			The Y component should be the cosine of the arc center direction relative to the cover direction.
//			The Z component should be the sine of the arc center direction relative to the cover direction.
//			The W component is just the negative of the Z component.
static const Vec4V s_CoverArcs[] =
{
	Vec4V(square(cosf(90.0f*DtoR)), 1.0f, 0.0f, 0.0f),													// COVARC_180
	Vec4V(square(cosf(60.0f*DtoR)), 1.0f, 0.0f, 0.0f),													// COVARC_120
	Vec4V(square(cosf(45.0f*DtoR)), 1.0f, 0.0f, 0.0f),													// COVARC_90
	Vec4V(square(cosf(22.5f*DtoR)), 1.0f, 0.0f, 0.0f),													// COVARC_45
	Vec4V(square(cosf(37.5f*DtoR)), cosf( 22.5f*DtoR),	sinf(	 22.5f*DtoR	),	-sinf( 22.5f*DtoR) ),	// COVARC_0TO60  (actually  -15 to 60)
	Vec4V(square(cosf(37.5f*DtoR)), cosf(-22.5f*DtoR),	sinf(	-22.5f*DtoR	),	-sinf(-22.5f*DtoR) ),	// COVARC_300TO0 (actually  300 to 15)
	Vec4V(square(cosf(30.0f*DtoR)), cosf( 15.0f*DtoR),	sinf(	 15.0f*DtoR	),	-sinf( 15.0f*DtoR) ),	// COVARC_0TO45  (actually  -15 to 45)
	Vec4V(square(cosf(30.0f*DtoR)), cosf(-15.0f*DtoR),	sinf(	-15.0f*DtoR	),	-sinf(-15.0f*DtoR) ),	// COVARC_315TO0 (actually  315 to 15)
	Vec4V(square(cosf(18.75f*DtoR)),cosf(  3.75f*DtoR),	sinf(	  3.75f*DtoR),	-sinf(  3.75f*DtoR) ),	// COVARC_0TO22  (actually  -15 to 22)
	Vec4V(square(cosf(18.75f*DtoR)),cosf( -3.75f*DtoR),	sinf(	 -3.75f*DtoR),	-sinf( -3.75f*DtoR) ),	// COVARC_338TO0 (actually  338 to 15)
};

// If any of these fail, it just means that s_CoverArcs[] above needs to be updated to match the new COVARC_... enums.
CompileTimeAssert(CCoverPoint::COVARC_180 == 0);
CompileTimeAssert(CCoverPoint::COVARC_120 == 1);
CompileTimeAssert(CCoverPoint::COVARC_90 == 2);
CompileTimeAssert(CCoverPoint::COVARC_45 == 3);
CompileTimeAssert(CCoverPoint::COVARC_0TO60 == 4);
CompileTimeAssert(CCoverPoint::COVARC_300TO0 == 5);
CompileTimeAssert(CCoverPoint::COVARC_0TO45 == 6);
CompileTimeAssert(CCoverPoint::COVARC_315TO0 == 7);
CompileTimeAssert(CCoverPoint::COVARC_0TO22 == 8);
CompileTimeAssert(CCoverPoint::COVARC_338TO0 == 9);

//////////////////////////////////////////////////////
// FUNCTION: DoesCoverPointProvideCoverFromDirection
// PURPOSE:
//////////////////////////////////////////////////////

bool CCover::DoesCoverPointProvideCoverFromDirection(CCoverPoint *pPoint, CCoverPoint::eCoverArc arcToUse, const Vector3& TargetDirection)
{
	if (!pPoint)
	{
		return false;
	}

	const CCoverPoint::eCoverType pointType = pPoint->GetType();
	if (pointType == CCoverPoint::COVTYPE_NONE)
	{
		return false;
	}

	if (RemoveCoverPointIfEntityLost(pPoint))
	{
		return false;
	}

	// Before, we used to compare if we were one of a number of other types, but all it did
	// in the end was to return true in the COVTYPE_OBJECT case. Now, we just check for that,
	// and assert on the rest.
	if(pointType == CCoverPoint::COVTYPE_OBJECT)
	{
		return true;
	}
	Assert(pointType == CCoverPoint::COVTYPE_POINTONMAP ||
		pointType == CCoverPoint::COVTYPE_SCRIPTED ||
		pointType == CCoverPoint::COVTYPE_VEHICLE ||
		pointType == CCoverPoint::COVTYPE_BIG_OBJECT);

	// Compute the cover direction.
	const Vec3V coverDirV = pPoint->GetCoverDirectionVector();

	// Load a proper vector variable for the target direction.
	const Vec3V targetDirV = RCC_VEC3V(TargetDirection);

	// It looks like the direction now always comes from FindVectorFromDir() in the end,
	// and if attached to an object, it's just rotated around the Z axis.
	// So, it should always be flat and normalized, which helps performance.
	Assert(coverDirV.GetZf() == 0.0f);
	Assert(fabsf(square(coverDirV.GetXf()) + square(coverDirV.GetYf()) - 1.0f) <= 0.01f);

	// Get the arc type, and use that to load up parameters describing its arc into a vector register.
	Assert(arcToUse >= 0 && arcToUse < NELEM(s_CoverArcs));
	const Vec4V fromCoverArcTable = s_CoverArcs[arcToUse];

	const ScalarV zeroV(V_ZERO);

	// Extract the parameters we need from this vector.
	const ScalarV cosAngleSqV = fromCoverArcTable.GetX();
	const ScalarV rotXV = fromCoverArcTable.GetY();
	const ScalarV rotYV = fromCoverArcTable.GetZ();
	const ScalarV negRotYV = fromCoverArcTable.GetW();

	// Next, we will compute the center direction of the arc, by rotating the
	// cover direction. First, we build a vector that contains (X, Y, X, Y)
	// from the cover direction:
	const ScalarV coverDirXV = coverDirV.GetX();
	const ScalarV coverDirYV = coverDirV.GetY();
	const Vec4V coverDirForRotsV = MergeXY(Vec4V(coverDirXV), Vec4V(coverDirYV));

	// Build a vector containing (X, Y, -Y, X) of the cos/sin rotation parameters.
	const Vec4V rotVecV(rotXV, rotYV, negRotYV, rotXV);

	// We now multiply each component of this with the corresponding components of coverDirForRotsV,
	// forming the terms of the dot products that we need to do the rotation.
	const Vec4V arcCenterRotV = Scale(coverDirForRotsV, rotVecV);

	// We now shift the elements of this vector over by one, and add them together.
	// The X and Z components of this vector are now the X and Y coordinates of the arc center direction.
	const Vec4V arcCenterRot2V = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(arcCenterRotV, arcCenterRotV);
	const Vec4V arcCenterSumV = Add(arcCenterRotV, arcCenterRot2V);

	// Form a vector that's just (X, X, Y, Y) of the target direction.
	const Vec4V targetDirDotTerm1V = MergeXY(targetDirV, targetDirV);

	// Form another vector out of the rotated arc center, and the target direction.
	const Vec4V targetDirDotTerm2V(arcCenterSumV.GetX(), targetDirV.GetX(), arcCenterSumV.GetZ(), targetDirV.GetY());

	// Multiply the components of this vector, shift it over one step, and add together. This
	// computes another two 2D dot products (between the arc center and the target direction, and between
	// the target direction and itself).
	const Vec4V targetDirDotTermsV = Scale(targetDirDotTerm1V, targetDirDotTerm2V);
	const Vec4V targetDirDotTermsRotatedV = GetFromTwo<Vec::Z1, Vec::W1, Vec::X2, Vec::Y2>(targetDirDotTermsV, targetDirDotTermsV);
	const Vec4V targetDirDotAddedV = Add(targetDirDotTermsV, targetDirDotTermsRotatedV);
	const ScalarV dotV = targetDirDotAddedV.GetX();
	//const ScalarV targetDirMagXYSqV = targetDirDotAddedV.GetY();

	// We actually need the square of the target direction's dot product with the arc center,
	// and also the target direction's dot product times the square of the cosine of the angle.
	// We can do these in a single multiplication:
	const Vec4V dotAndTargetDirMulV = MergeXY(targetDirDotAddedV, Vec4V(cosAngleSqV));
	const Vec4V dotAndTargetDirMultipliedV = Scale(targetDirDotAddedV, dotAndTargetDirMulV);
	const ScalarV dotSqV = dotAndTargetDirMultipliedV.GetX();
	const ScalarV arcThresholdV = dotAndTargetDirMultipliedV.GetY();

	// Finally, we basically want to compute this condition:
	//	(dot >= 0.0f) && (square(dot) >= cosAngleSq*targetDirMagXYSq);
	// We can do both of these comparisons as one, by merging together the
	// left side of both inequalities in one vector, and the right side
	// in another.
	const Vec4V dotAndDotSqV = MergeXY(Vec4V(dotV), Vec4V(dotSqV));
	const Vec4V thresholdsV = MergeXY(Vec4V(zeroV), Vec4V(arcThresholdV));

	// Do the final comparison.
	return IsGreaterThanOrEqualAll(dotAndDotSqV, thresholdsV) != 0;
}

#if 0 // Unoptimized version, for reference

bool CCover::DoesCoverPointProvideCoverFromDirection(CCoverPoint *pPoint, const Vector3& TargetDirection)
{
	if (!pPoint || pPoint->GetType() == CCoverPoint::COVTYPE_NONE)
	{
		return false;
	}

	if (RemoveCoverPointIfEntityLost(pPoint))
	{
		return false;
	}

	if ( pPoint->GetType() != CCoverPoint::COVTYPE_POINTONMAP &&
		pPoint->GetType() != CCoverPoint::COVTYPE_SCRIPTED &&
		pPoint->GetType() != CCoverPoint::COVTYPE_VEHICLE &&
		pPoint->GetType() != CCoverPoint::COVTYPE_BIG_OBJECT )
	{
		return true;
	}

	switch( pPoint->GetArc() )
	{
	case CCoverPoint::COVARC_180:
		{
			// Points on the map only work in a specific direction.
			if ( DotProduct(pPoint->GetCoverDirectionVector(), TargetDirection) >= 0.0f)
			{
				return true;
			}
		}
		break;
	case CCoverPoint::COVARC_120:
		{
			Vector3 vTargetDirection = TargetDirection;
			vTargetDirection.z = 0.0f;
			vTargetDirection.Normalize();
			const float fDot = DotProduct(pPoint->GetCoverDirectionVector(), vTargetDirection);
			if ( fDot >= 0.5f)
			{
				return true;
			}
		}
		break;
	case CCoverPoint::COVARC_90:
		{
			Vector3 vTargetDirection = TargetDirection;
			vTargetDirection.z = 0.0f;
			vTargetDirection.Normalize();
			if ( DotProduct(pPoint->GetCoverDirectionVector(), vTargetDirection) >= 0.707f)
			{
				return true;
			}
		}
		break;
	case CCoverPoint::COVARC_45:
		{
			Vector3 vTargetDirection = TargetDirection;
			vTargetDirection.z = 0.0f;
			vTargetDirection.Normalize();
			if ( DotProduct(pPoint->GetCoverDirectionVector(), vTargetDirection) >= 0.924f)
			{
				return true;
			}
		}
		break;
	case CCoverPoint::COVARC_0TO60: // Cover forward to 60 degrees counter-clockwise
		{
			Vector3 vTargetDirection = TargetDirection;
			vTargetDirection.z = 0.0f;
			vTargetDirection.Normalize();
			if ( DotProduct(pPoint->GetCoverDirectionVector(), vTargetDirection) >= 0.5f)
			{
				Vector3 vRight;
				vRight.Cross(Vector3(0.0f, 0.0f, 1.0f), pPoint->GetCoverDirectionVector());
				// allow 15 degrees beyond zero forward
				if( vRight.Dot( vTargetDirection ) <= 0.25f )
				{
					return true;
				}
			}
		}
		break;
	case CCoverPoint::COVARC_300TO0:  // Cover forward to 60 degrees clockwise
		{
			Vector3 vTargetDirection = TargetDirection;
			vTargetDirection.z = 0.0f;
			vTargetDirection.Normalize();
			if ( DotProduct(pPoint->GetCoverDirectionVector(), vTargetDirection) >= 0.5f)
			{
				Vector3 vRight;
				vRight.Cross(Vector3(0.0f, 0.0f, 1.0f), pPoint->GetCoverDirectionVector());
				// allow 15 degrees beyond zero forward
				if( vRight.Dot( vTargetDirection ) >= -0.25f )
				{
					return true;
				}
			}
		}	
		break;
	case CCoverPoint::COVARC_0TO45: // Cover forward to 45 degrees counter-clockwise
		{
			Vector3 vTargetDirection = TargetDirection;
			vTargetDirection.z = 0.0f;
			vTargetDirection.Normalize();
			if ( DotProduct(pPoint->GetCoverDirectionVector(), vTargetDirection) >= 0.707f)
			{
				Vector3 vRight;
				vRight.Cross(Vector3(0.0f, 0.0f, 1.0f), pPoint->GetCoverDirectionVector());
				// allow 15 degrees beyond zero forward
				if( vRight.Dot( vTargetDirection ) <= 0.25f )
				{
					return true;
				}
			}
		}
		break;
	case CCoverPoint::COVARC_315TO0: // Cover forward to 45 degrees clockwise
		{
			Vector3 vTargetDirection = TargetDirection;
			vTargetDirection.z = 0.0f;
			vTargetDirection.Normalize();
			if ( DotProduct(pPoint->GetCoverDirectionVector(), vTargetDirection) >= 0.707f)
			{
				Vector3 vRight;
				vRight.Cross(Vector3(0.0f, 0.0f, 1.0f), pPoint->GetCoverDirectionVector());
				// allow 15 degrees beyond zero forward
				if( vRight.Dot( vTargetDirection ) >= -0.25f )
				{
					return true;
				}
			}
		}	
		break;
	case CCoverPoint::COVARC_0TO22: // Cover forward to 22.5 degrees counter-clockwise
		{
			Vector3 vTargetDirection = TargetDirection;
			vTargetDirection.z = 0.0f;
			vTargetDirection.Normalize();
			if ( DotProduct(pPoint->GetCoverDirectionVector(), vTargetDirection) >= 0.924f)
			{
				Vector3 vRight;
				vRight.Cross(Vector3(0.0f, 0.0f, 1.0f), pPoint->GetCoverDirectionVector());
				// allow 15 degrees beyond zero forward
				if( vRight.Dot( vTargetDirection ) <= 0.25f )
				{
					return true;
				}
			}
		}
		break;
	case CCoverPoint::COVARC_338TO0: // Cover forward to 22.5 degrees clockwise
		{
			Vector3 vTargetDirection = TargetDirection;
			vTargetDirection.z = 0.0f;
			vTargetDirection.Normalize();
			if ( DotProduct(pPoint->GetCoverDirectionVector(), vTargetDirection) >= 0.924f)
			{
				Vector3 vRight;
				vRight.Cross(Vector3(0.0f, 0.0f, 1.0f), pPoint->GetCoverDirectionVector());
				// allow 15 degrees beyond zero forward
				if( vRight.Dot( vTargetDirection ) >= -0.25f )
				{
					return true;
				}
			}
		}	
		break;
	}
	return false;
}

#endif	// 0 - Unoptimized version, for reference


//////////////////////////////////////////////////////
// FUNCTION: DoesCoverPointProvideCover
// PURPOSE:
//////////////////////////////////////////////////////

bool CCover::DoesCoverPointProvideCover(CCoverPoint *pPoint, CCoverPoint::eCoverArc arcToUse, const Vector3& TargetCoors, const Vector3 & vCoverPos)
{
	return DoesCoverPointProvideCoverFromDirection( pPoint, arcToUse, TargetCoors - vCoverPos );
}

//////////////////////////////////////////////////////
// PURPOSE: To check if a cover point provides cover against targets in our targeting
//////////////////////////////////////////////////////
bool CCover::DoesCoverPointProvideCoverFromTargets(const CPed* pPed, const Vector3& vTargetPos, CCoverPoint *pPoint, CCoverPoint::eCoverArc arcToUse,
												   const Vector3 &vCoverPos, const CEntity* pTargetEntity)
{
	// First check against our primary target position
	if(!DoesCoverPointProvideCoverFromDirection(pPoint, arcToUse, vTargetPos - vCoverPos))
	{
		return false;
	}

	// If we don't have a ped for some reason then just consider this cover point safe because we can't test any other targets
	if(!pPed)
	{
		return true;
	}

	bool bNetworkGameInProgress = NetworkInterface::IsGameInProgress();

	// If we have a targeting we want to test against any player peds in the targeting
	CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
	if(pPedTargeting)
	{
		const int numTargets = pPedTargeting->GetNumActiveTargets();
		for(int i = 0; i < numTargets; i++)
		{
			// Make sure the entity exists and it isn't our passed in target as we've already checked against it
			const CEntity* pEntity = pPedTargeting->GetTargetAtIndex(i);
			if(pEntity && pEntity != pTargetEntity)
			{
				// Check if this entity is a player ped
				if(pEntity->GetIsTypePed() && static_cast<const CPed*>(pEntity)->IsPlayer())
				{
					Vec3V vEntityPosition = pEntity->GetTransform().GetPosition();

					// Special checks if we are in a network game
					if(bNetworkGameInProgress)
					{
						// Don't bother checking this player if we can't see them
						LosStatus targetLos = pPedTargeting->GetLosStatus(pEntity, false);
						if(targetLos == Los_blocked || targetLos == Los_unchecked)
						{
							continue;
						}

						// Don't check the player if they are too far away
						static float fMaxPlayerDistanceSq = square(30.0f);
						ScalarV scDistSq = DistSquared(VECTOR3_TO_VEC3V(vCoverPos), vEntityPosition);
						ScalarV scMaxDistSq = ScalarVFromF32(fMaxPlayerDistanceSq);
						if(IsGreaterThanAll(scDistSq, scMaxDistSq))
						{
							continue;
						}
						
					}

					// If the cover isn't safe from this ped then return
					if(!DoesCoverPointProvideCoverFromDirection(pPoint, arcToUse, VEC3V_TO_VECTOR3(vEntityPosition) - vCoverPos))
					{
						return false;
					}
				}
			}
		}
	}

	// None of our tests against our targets failed so that means this cover is safe
	return true;
}

//////////////////////////////////////////////////////
// FUNCTION: GenerateCoverAreaBoundingBox
// PURPOSE:  Examines all currently existing peds interested in cover and 
//			 generates a bounding box to encompass them
//////////////////////////////////////////////////////
void CCover::GenerateCoverAreaBoundingBoxes()
{
	ms_iNumberAreas = 0;

	// First add the initial bounding box from the player
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if( !pPed )
	{
		return;
	}

	Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vPlayerCoverExtents = ms_vPlayerCoverExtents;

#if __BANK
	// Allow vPlayerPos to be overriden by the camera position; so that QA can easily test for cover
	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	if(ms_bUseDebugCameraAsCoverOrigin)
	{
		CCoverDebug::ms_Tunables.m_RenderCoverPoints = true;
		if(debugDirector.IsFreeCamActive())
		{
			vPlayerPos = debugDirector.GetFreeCamFrame().GetPosition();
			vPlayerCoverExtents = ms_vMaxCoverExtents;
		}
	}
#endif

	// Set up the first bounding box to contain the ped and a default radius around
	ms_boundingAreas[0].init(vPlayerPos, ms_vPlayerCoverExtents );
	ms_iNumberAreas = 1;

	// Loop through all other peds and include them if they are interested in cover
	CPed::Pool* pPedPool = CPed::GetPool();
	s32 i=pPedPool->GetSize();
	while(i--)
	{
		pPed = pPedPool->GetSlot(i);
		if(pPed && !pPed->IsInjured() && (pPed->GetPedResetFlag( CPED_RESET_FLAG_SearchingForCover )||pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ForcePedLoadCover )) )
		{
			Vector3 vCoverSearchCenter = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			// If the ped is running a task which would like to search for cover further ahead,
			// adjust the cover search position from the task to bring in coverpoints about that position
			CTask * pTaskActive = pPed->GetPedIntelligence()->GetTaskActive();
			if(AssertVerify(pTaskActive))
			{
				pTaskActive->AdjustCoverSearchPosition(vCoverSearchCenter);
			}

			Vector3 vCoverExtents = ms_vAiCoverExtents;
			// IF the ped is assigned a defensive area, generate cover around this rather than
			// around the ped
			CDefensiveArea * pDefArea = pPed->GetPedIntelligence()->GetDefensiveAreaForCoverSearch();
			Assert(pDefArea);
			if( pDefArea && pDefArea->IsActive() )
			{
				float fRadius;
				pDefArea->GetCentreAndMaxRadius(vCoverSearchCenter, fRadius);
				fRadius = rage::Sqrtf(fRadius);
				vCoverExtents.x = MAX( ms_vAiCoverExtents.x, fRadius );
				vCoverExtents.y = MAX( ms_vAiCoverExtents.y, fRadius );
				Vector3 vStart, vEnd;
				pDefArea->Get(vStart, vEnd, fRadius );
				if( pDefArea->GetType() == CDefensiveArea::AT_AngledArea )
				{
					vCoverExtents.z = MAX( ms_vAiCoverExtents.z, ABS( vStart.z - vEnd.z ) );
				}
				else
				{
					vCoverExtents.z = MAX( ms_vAiCoverExtents.z, ABS( fRadius * 2.0f ) );
				}
			}
			// else if we are running combat we should use the attack window values
			else if(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_WillAdvance &&
				pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
			{
				// Make sure we have a target entity as we need a center position
				CEntity* pTargetEntity = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_COMBAT);
				if(pTargetEntity)
				{
					float fWindowMin = 0.0f;
					float fWindowMax = 0.0f;
					CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(pPed, fWindowMin, fWindowMax, true);
					vCoverSearchCenter = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetPosition());
					vCoverExtents.x = MAX( ms_vAiCoverExtents.x, fWindowMax );
					vCoverExtents.y = MAX( ms_vAiCoverExtents.y, fWindowMax );
					vCoverExtents.z = MAX( ms_vAiCoverExtents.z, fWindowMax );
				}
			}

			// Add the main cover area for the ped
			if (AddNewCoverArea(vCoverSearchCenter, vCoverExtents))
			{
				continue;
			}
		}
	}

	// Process the cached scripted cover area
	if (!m_sCachedScriptedCoverArea.m_vCoverAreaPosition.IsZero() && ms_iNumberAreas < MAX_BOUNDING_AREAS)
	{
		ms_boundingAreas[ms_iNumberAreas].init(m_sCachedScriptedCoverArea.m_vCoverAreaPosition, m_sCachedScriptedCoverArea.m_vCoverAreaExtents);
		++ms_iNumberAreas;

		// Reset these values to ensure script have to call this every frame
		m_sCachedScriptedCoverArea.Reset();
	}
}

bool CCover::AddNewCoverArea(const Vector3& vCoverSearchCenter, const Vector3& vCoverExtents)
{
	// This ped is interested in cover, first try to include the ped in an existing pool
	for (u32 j = 0; j < ms_iNumberAreas; j++)
	{
		if (ms_boundingAreas[j].ExpandToInclude(vCoverSearchCenter, vCoverExtents, ms_vMaxCoverExtents))
		{
			return true;
		}
	}

	// The ped cannot be included in any existing boxes, try and create a new one to include them if we can
	if (ms_iNumberAreas == MAX_BOUNDING_AREAS)
	{
		//Assertf( NULL, "Ped cannot be included in cover search" );
		//Warningf( "Too many peds too spread out, some will not have cover generated nearby!\n" );
		return false;
	}

	// Create a new box
	ms_boundingAreas[ms_iNumberAreas].init(vCoverSearchCenter, vCoverExtents);
	++ms_iNumberAreas;

	return true;
}

bool CCover::AddNewScriptedCoverArea(const Vector3& vCoverAreaCenter, const float fRadius )
{
	if (ms_iNumberAreas == MAX_BOUNDING_AREAS)
	{
		Warningf("[AddNewScriptedCoverArea] Too many areas already, cannot add a new one\n" );
		return false;
	}

	m_sCachedScriptedCoverArea.m_vCoverAreaPosition = vCoverAreaCenter;

	m_sCachedScriptedCoverArea.m_vCoverAreaExtents.x = MAX(ms_vAiCoverExtents.x, fRadius);
	m_sCachedScriptedCoverArea.m_vCoverAreaExtents.y = MAX(ms_vAiCoverExtents.y, fRadius);
	m_sCachedScriptedCoverArea.m_vCoverAreaExtents.z = MAX(ms_vAiCoverExtents.z, fRadius);

	return true;
}

s32 CCover::CopyBoundingAreas(CCoverBoundingArea * pAreasBuffer)
{
	Vector3 vMin, vMax;
	for(u32 i = 0; i < ms_iNumberAreas; i++)
	{
		ms_boundingAreas[i].GetExtents(vMin, vMax);
		pAreasBuffer[i].SetMin(vMin);
		pAreasBuffer[i].SetMax(vMax);
	}

	return ms_iNumberAreas;
}

void CCover::RecalculateCurrentCoverExtents()
{
	ms_vPlayerCoverExtents = Vector3( ms_fCoverPointDistanceSearchXY, ms_fCoverPointDistanceSearchXY, ms_fCoverPointDistanceSearchZ );
	ms_vAiCoverExtents = Vector3( ms_fCoverPointDistanceSearchXY, ms_fCoverPointDistanceSearchXY, ms_fCoverPointDistanceSearchZ);
	ms_vMaxCoverExtents = Vector3( ms_fCurrentCoverBoundingBoxXY, ms_fCurrentCoverBoundingBoxXY, ms_fCurrentCoverBoundingBoxZ);
}

bool
CCover::ReduceBoundingAreaSize(const float fDecrement)
{
	const bool bCanReduce =
		(ms_fCoverPointDistanceSearchXY > ms_fMinPointDistanceSearchXY) ||
		(ms_fCoverPointDistanceSearchZ > ms_fMinPointDistanceSearchZ) ||
		(ms_fCurrentCoverBoundingBoxXY > ms_fMinCoverBoundingBoxXY) ||
		(ms_fCurrentCoverBoundingBoxZ > ms_fMinCoverBoundingBoxZ);

	if(!bCanReduce)
		return false;

	ms_fCoverPointDistanceSearchXY -= fDecrement;
	ms_fCoverPointDistanceSearchXY = Max(ms_fCoverPointDistanceSearchXY, ms_fMinPointDistanceSearchXY);
	ms_fCoverPointDistanceSearchZ -= fDecrement;
	ms_fCoverPointDistanceSearchZ = Max(ms_fCoverPointDistanceSearchZ, ms_fMinPointDistanceSearchZ);

	ms_fCurrentCoverBoundingBoxXY -= fDecrement;
	ms_fCurrentCoverBoundingBoxXY = Max(ms_fCurrentCoverBoundingBoxXY, ms_fMinCoverBoundingBoxXY);
	ms_fCurrentCoverBoundingBoxZ -= fDecrement;
	ms_fCurrentCoverBoundingBoxZ = Max(ms_fCurrentCoverBoundingBoxZ, ms_fMinCoverBoundingBoxZ);

#if AI_OPTIMISATIONS_OFF
	Displayf("CCover - cover pool is overcomitted, reducing bounding areas by %.1f\n", ms_fStepSizeForDecreasingBoundingBoxes);
	Displayf("AI/Plyr Extents = %.1f (min: %.1f, max: %.1f).\nCombined Extents = %.1f (min: %.1f, max: %.1f).\n",
		ms_fCoverPointDistanceSearchXY, ms_fMinPointDistanceSearchXY, ms_fMaxPointDistanceSearchXY,
		ms_fCurrentCoverBoundingBoxXY, ms_fMinCoverBoundingBoxXY, ms_fMaxCoverBoundingBoxXY
	);
#endif

	RecalculateCurrentCoverExtents();

	return true;
}

bool
CCover::IncreaseBoundingAreaSize(const float fDecrement)
{
	const bool bCanIncrease =
		(ms_fCoverPointDistanceSearchXY < ms_fMaxPointDistanceSearchXY) ||
		(ms_fCoverPointDistanceSearchZ < ms_fMaxPointDistanceSearchZ) ||
		(ms_fCurrentCoverBoundingBoxXY < ms_fMaxCoverBoundingBoxXY) ||
		(ms_fCurrentCoverBoundingBoxZ < ms_fMaxCoverBoundingBoxZ);

	if(!bCanIncrease)
		return false;

	ms_fCoverPointDistanceSearchXY += fDecrement;
	ms_fCoverPointDistanceSearchXY = Min(ms_fCoverPointDistanceSearchXY, ms_fMaxPointDistanceSearchXY);
	ms_fCoverPointDistanceSearchZ += fDecrement;
	ms_fCoverPointDistanceSearchZ = Min(ms_fCoverPointDistanceSearchZ, ms_fMaxPointDistanceSearchZ);

	ms_fCurrentCoverBoundingBoxXY += fDecrement;
	ms_fCurrentCoverBoundingBoxXY = Min(ms_fCurrentCoverBoundingBoxXY, ms_fMaxCoverBoundingBoxXY);
	ms_fCurrentCoverBoundingBoxZ += fDecrement;
	ms_fCurrentCoverBoundingBoxZ = Min(ms_fCurrentCoverBoundingBoxZ, ms_fMaxCoverBoundingBoxZ);

#if AI_OPTIMISATIONS_OFF
	Displayf("CCover - cover pool is less than 75%% comitted, increasing bounding areas by %.1f\n", ms_fStepSizeForIncreasingBoundingBoxes);
	Displayf("AI/Plyr Extents = %.1f (min: %.1f, max: %.1f).\nCombined Extents = %.1f (min: %.1f, max: %.1f).\n",
		ms_fCoverPointDistanceSearchXY, ms_fMinPointDistanceSearchXY, ms_fMaxPointDistanceSearchXY,
		ms_fCurrentCoverBoundingBoxXY, ms_fMinCoverBoundingBoxXY, ms_fMaxCoverBoundingBoxXY
		);
#endif

	RecalculateCurrentCoverExtents();

	return true;
}

//**********************************************************************************************
//	Returns whether any pool is comitted over the given ratio

bool
CCover::IsCoverPoolOverComitted(const int iPool, const float fRatio)
{
	Assert(fRatio >= 0.0f && fRatio <= 1.0f);

	switch(iPool)
	{
		case CP_NavmeshAndDynamicPoints:
		{
			float fRatioMapCoverPts = ((float)ms_iCurrentNumMapAndDynamicCoverPts) / ((float)NUM_NAVMESH_COVERPOINTS);
			if(fRatioMapCoverPts > fRatio)
				return true;
			break;
		}
		case CP_SciptedPoint:
		{
			float fRatioScriptedCoverPts = ((float)ms_iCurrentNumScriptedCoverPts) / ((float)NUM_SCRIPTED_COVERPOINTS);
			if(fRatioScriptedCoverPts > fRatio)
				return true;
			break;
		}
		default:
			Assert(0);
	}
	return false;
}


//////////////////////////////////////////////////////
// FUNCTION: IsPointWithinValidArea
// PURPOSE:  Returns true if the given point is in a given cover point area
//////////////////////////////////////////////////////
bool CCover::IsPointWithinValidArea( const Vector3& vPoint, const CCoverPoint::eCoverType coverType, bool bIsPlayer )
{
	// Hard coded blocking bounds because new nav meshes are not an option
#if __BANK
	TUNE_GROUP_BOOL(COVER_TUNE, CHECK_HARD_CODED_BOUNDS, true);
	if (CHECK_HARD_CODED_BOUNDS && !bIsPlayer)
#else // __BANK
	if (!bIsPlayer)
#endif // __BANK
	
	{
		for (u32 i = 0; i<MAX_HARD_CODED_AREAS; i++)
		{
			if (ms_hardCodedBlockingBoundingAreas[i].ContainsPoint(vPoint))
			{
				return false;
			}
		}
	}

	// block points within blocking area
	for( u32 i = 0; i < ms_blockingBoundingAreas.GetCount(); i++ )
	{
		if( ( ms_blockingBoundingAreas[i].GetBlocksMap() && coverType == CCoverPoint::COVTYPE_POINTONMAP ) ||
			( ms_blockingBoundingAreas[i].GetBlocksObjects() && coverType == CCoverPoint::COVTYPE_OBJECT ) ||
			( ms_blockingBoundingAreas[i].GetBlocksObjects() && coverType == CCoverPoint::COVTYPE_BIG_OBJECT ) ||
			( ms_blockingBoundingAreas[i].GetBlocksVehicles() && coverType == CCoverPoint::COVTYPE_VEHICLE ) ||
			( ms_blockingBoundingAreas[i].GetBlocksPlayer() && bIsPlayer ))
		{
			if( ms_blockingBoundingAreas[i].ContainsPoint(vPoint) )
			{
				return false;
			}
		}
	}

	// Render all present bounding boxes
	for( u32 i = 0; i < ms_iNumberAreas; i++ )
	{
		if( ms_boundingAreas[i].ContainsPoint(vPoint) )
		{
			return true;
		}
	}
	return false;
}


#if !__FINAL
//////////////////////////////////////////////////////
// FUNCTION: RenderCoverAreaBoundingBoxes
// PURPOSE:  Renders the bounding boxes
//////////////////////////////////////////////////////
void CCover::RenderCoverAreaBoundingBoxes()
{
#if DEBUG_DRAW
	// Render all present bounding boxes
	if (CCoverDebug::ms_Tunables.m_BoundingAreas.m_RenderBoundingAreas)
	{
		// Blue
		Color32 color( 0.0f, 0.0f, 1.0f, 0.3f );
		for( u32 i = 0; i < ms_iNumberAreas; i++ )
		{
			Vector3 vMin, vMax;
			ms_boundingAreas[i].GetExtents(vMin, vMax);
			RenderBoundingBox(vMin, vMax, color);
		}
	}

	// Render all present blocking boxes
	if (CCoverDebug::ms_Tunables.m_BoundingAreas.m_RenderBlockingAreas)
	{
		// Red
		Color32 color( 1.0f, 0.0f, 0.0f, 0.3f );
		for( u32 i = 0; i < ms_blockingBoundingAreas.GetCount(); i++ )
		{
			Vector3 vMin, vMax;
			ms_blockingBoundingAreas[i].GetExtents(vMin, vMax);
			RenderBoundingBox(vMin, vMax, color);

			const Vector3 vDrawPosition = (vMin + vMax) * 0.5f;
			s32 iNoTexts = 0;
			char szText[128];
			if (ms_blockingBoundingAreas[i].GetBlocksMap())
			{
				formatf(szText, "Blocks Map Cover");
				grcDebugDraw::Text(RCC_VEC3V(vDrawPosition), Color_yellow, 0, 10 * iNoTexts++, szText);
			}
			if (ms_blockingBoundingAreas[i].GetBlocksVehicles())
			{
				formatf(szText, "Blocks Vehicle Cover");
				grcDebugDraw::Text(RCC_VEC3V(vDrawPosition), Color_blue, 0, 10 * iNoTexts++, szText);
			}
			if (ms_blockingBoundingAreas[i].GetBlocksObjects())
			{
				formatf(szText, "Blocks Object Cover");
				grcDebugDraw::Text(RCC_VEC3V(vDrawPosition), Color_brown, 0, 10 * iNoTexts++, szText);
			}
			if (ms_blockingBoundingAreas[i].GetBlocksPlayer())
			{
				formatf(szText, "Blocks Player Cover");
				grcDebugDraw::Text(RCC_VEC3V(vDrawPosition), Color_green, 0, 10 * iNoTexts++, szText);
			}
		}

		for( u32 i = 0; i < ms_iNumberHardCodedBlockingAreas; i++ )
		{
			Vector3 vMin, vMax;
			ms_hardCodedBlockingBoundingAreas[i].GetExtents(vMin, vMax);
			RenderBoundingBox(vMin, vMax, Color_green);
		}
	}
#endif
}

void CCover::RenderBoundingBox(const Vector3& DEBUG_DRAW_ONLY(vMin), const Vector3& DEBUG_DRAW_ONLY(vMax), Color32 DEBUG_DRAW_ONLY(color))
{
#if DEBUG_DRAW
	// top
	grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMin.x, vMax.y, vMin.z ), Vec3V( vMax.x, vMax.y, vMin.z), color );
	grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMin.z ), RCC_VEC3V(vMin), Vec3V( vMax.x, vMax.y, vMin.z), color );
	grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMin.z ), RCC_VEC3V(vMin), Vec3V( vMax.x, vMax.y, vMin.z), color );
	grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMax.x, vMin.y, vMin.z ), Vec3V( vMax.x, vMax.y, vMin.z), color );
	// bottom
	grcDebugDraw::Poly( Vec3V( vMin.x, vMin.y, vMax.z), Vec3V( vMin.x, vMax.y, vMax.z ), RCC_VEC3V(vMax), color );
	grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMax.z ), Vec3V( vMin.x, vMin.y, vMax.z), RCC_VEC3V(vMax), color );
	grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMax.z ), Vec3V( vMin.x, vMin.y, vMax.z), RCC_VEC3V(vMax), color );
	grcDebugDraw::Poly( Vec3V( vMin.x, vMin.y, vMax.z), Vec3V( vMax.x, vMin.y, vMax.z ), RCC_VEC3V(vMax), color );

	//left
	grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMin.z ), RCC_VEC3V(vMin), Vec3V( vMax.x, vMin.y, vMax.z), color );
	grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMin.x, vMin.y, vMax.z ), Vec3V( vMax.x, vMin.y, vMax.z), color );
	grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMax.x, vMin.y, vMin.z ), Vec3V( vMax.x, vMin.y, vMax.z), color );
	grcDebugDraw::Poly( Vec3V( vMin.x, vMin.y, vMax.z ), RCC_VEC3V(vMin), Vec3V( vMax.x, vMin.y, vMax.z), color );
	//right
	grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMin.z), Vec3V( vMax.x, vMax.y, vMin.z ), RCC_VEC3V(vMax), color );
	grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMax.z ), Vec3V( vMin.x, vMax.y, vMin.z), RCC_VEC3V(vMax), color );
	grcDebugDraw::Poly( Vec3V( vMax.x, vMax.y, vMin.z ), Vec3V( vMin.x, vMax.y, vMin.z), RCC_VEC3V(vMax), color );
	grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMin.z), Vec3V( vMin.x, vMax.y, vMax.z ), RCC_VEC3V(vMax), color );

	//front
	grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMin.x, vMax.y, vMin.z ), Vec3V( vMin.x, vMax.y, vMax.z), color );
	grcDebugDraw::Poly( Vec3V( vMin.x, vMin.y, vMax.z ), RCC_VEC3V(vMin), Vec3V( vMin.x, vMax.y, vMax.z), color );
	grcDebugDraw::Poly( Vec3V( vMin.x, vMax.y, vMin.z ), RCC_VEC3V(vMin), Vec3V( vMin.x, vMax.y, vMax.z), color );
	grcDebugDraw::Poly( RCC_VEC3V(vMin), Vec3V( vMin.x, vMin.y, vMax.z ), Vec3V( vMin.x, vMax.y, vMax.z), color );
	//back
	grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMin.z), Vec3V( vMax.x, vMax.y, vMin.z ), RCC_VEC3V(vMax), color );
	grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMax.z ), Vec3V( vMax.x, vMin.y, vMin.z), RCC_VEC3V(vMax), color );
	grcDebugDraw::Poly( Vec3V( vMax.x, vMax.y, vMin.z ), Vec3V( vMax.x, vMin.y, vMin.z), RCC_VEC3V(vMax), color );
	grcDebugDraw::Poly( Vec3V( vMax.x, vMin.y, vMin.z), Vec3V( vMax.x, vMin.y, vMax.z ), RCC_VEC3V(vMax), color );
#endif
}

void CCover::RenderCoverCells()
{
#if DEBUG_DRAW
	for(int i = 0; i < CCoverPointsContainer::ms_CoverPointGrids.GetCount(); i++)
	{
		const CCoverPointsGridCell* pGridCell = CCoverPointsContainer::ms_CoverPointGrids[i];
		grcDebugDraw::BoxAxisAligned(pGridCell->m_vBoundingBoxForCellMin, pGridCell->m_vBoundingBoxForCellMax, Color_black, false);

		if(pGridCell->m_bBoundingBoxForContentsExists)
		{
			const Vec3V bndMinV = pGridCell->m_vBoundingBoxForContentsMin;
			const Vec3V bndMaxV = pGridCell->m_vBoundingBoxForContentsMax;
			grcDebugDraw::BoxAxisAligned(bndMinV, bndMaxV, Color_yellow, false);
			grcDebugDraw::BoxAxisAligned(pGridCell->m_vBoundingBoxForCellMin, pGridCell->m_vBoundingBoxForCellMax, Color32(0.65f, 0.16f, 0.16f, 0.25f), true);
		}
	}
#endif	// DEBUG_DRAW
}

#endif // !__FINAL



//////////////////////////////////////////////////////
// FUNCTION: UpdateStatus
// PURPOSE:  Updates the status flags for this coverpoint.
//			 checks to see if it is obstructed by anything.
// NOTE:	Commented out on 1/11/13. Replaced by an "on demand" check in tactical analysis and cover finder - CR
//////////////////////////////////////////////////////

//void CCoverPoint::UpdateStatus()
//{
//	// Clear the status by default
//	m_CoverPointStatus = 0;
//
//	// Get the cover position, bail if invalid
//	Vector3 vCoverPosition;
//	if( !GetCoverPointPosition(vCoverPosition) )
//	{
//		return;
//	}
//
//	dev_float COVER_TEST_LOW_HEIGHT = 0.35f;
//	dev_float COVER_TEST_HIGH_HEIGHT = 1.5f;
//	dev_float COVER_TEST_CAPSULE_RADIUS = 0.2f;
//	dev_float VANTAGE_TEST_HEIGHT = 0.9f;
//	dev_float VANTAGE_TEST_CAPSULE_RADIUS = 0.35f;
//	dev_float SURFACE_TEST_ABOVE_HEIGHT = 0.75f;
//	dev_float SURFACE_TEST_BELOW_HEIGHT = 0.75f;
//	dev_float SURFACE_TEST_BELOW_HEIGHT_SCRIPTED = 1.50f; // scripters are placing points by reading ped coords, so probe down must be large to compensate
//	dev_float SURFACE_TEST_HEIGHT_TOLERANCE = 0.50f;
//
//#if __DEV 
//	
//	// Debug rendering:
//
//	// This bool controls whether we render or not
//	TUNE_GROUP_BOOL(COVER_DEBUG, RENDER_COVERPOINT_BLOCKING_TESTS, false);
//	
//	// Declare capsule test results to store for rendering
//	const u8 MAX_COVER_INTERSECTIONS = 20;
//	WorldProbe::CShapeTestFixedResults<MAX_COVER_INTERSECTIONS> coverCapsuleResults;
//	WorldProbe::CShapeTestFixedResults<MAX_COVER_INTERSECTIONS> vantageCapsuleResults;
//
//#endif // __DEV
//
//	// If the coverpoint was not generated with the navmesh
//	// and is currently marked as clear
//	float probedGroundHeight = 0.0f;
//	bool bHasProbedGroundHeight = false;
//	if( m_Type != COVTYPE_POINTONMAP /*&& (m_CoverPointStatus & COVSTATUS_Clear) > 0*/ )
//	{
//		// Check for a support surface beneath the coverpoint
//		bool bCoverHasValidSupportSurface = true;
//
//		// Use a simple line probe down from just above to just below cover position.
//		float fProbeDownDistance = SURFACE_TEST_BELOW_HEIGHT;
//		if( m_Type == COVTYPE_SCRIPTED )
//		{
//			fProbeDownDistance = SURFACE_TEST_BELOW_HEIGHT_SCRIPTED;
//		}
//		WorldProbe::CShapeTestProbeDesc probeDesc;
//		probeDesc.SetStartAndEnd(vCoverPosition + Vector3(0.0f,0.0f,SURFACE_TEST_ABOVE_HEIGHT), vCoverPosition + Vector3(0.0f,0.0f,-fProbeDownDistance));
//		// for the ground testing, include objects, vehicles, and map types
//		s32 iGroundCheckTypeFlags = ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_ALL_MAP_TYPES;
//		probeDesc.SetIncludeFlags(iGroundCheckTypeFlags);
//		probeDesc.SetExcludeEntity(m_pEntity);
//		// looking for the first hit, if any
//		const u8 SUPPORT_INTERSECTIONS = 1;
//		WorldProbe::CShapeTestFixedResults<SUPPORT_INTERSECTIONS> probeResults;
//		probeDesc.SetResultsStructure(&probeResults);
//
//		// Is there surface at the cover position?
//		bool bCoverPositionSurface = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
//		if( bCoverPositionSurface )
//		{
//			// Check if the surface Z height is too different from the coverpoint height to be valid
//			probedGroundHeight = probeResults[0].GetHitPosition().z;
//			bHasProbedGroundHeight = true;
//			if( ABS(probedGroundHeight - vCoverPosition.z) > SURFACE_TEST_HEIGHT_TOLERANCE )
//			{
//				// @TEMP HACK while we decide what to do about vehicles on slopes all blocking from this test (dmorales)
//				//bCoverHasValidSupportSurface = false;
//			}
//		}
//		else // nothing was hit in the test
//		{
//			// @TEMP HACK while we decide what to do about vehicles on slopes all blocking from this test (dmorales)
//			//bCoverHasValidSupportSurface = false;
//		}
//
//		// If the cover does not have valid surface
//		if( !bCoverHasValidSupportSurface )
//		{
//			// consider this coverpoint position blocked
//			m_CoverPointStatus |= COVSTATUS_PositionBlocked;
//#if __DEV
//			bool bRenderDebugSphere = RENDER_COVERPOINT_BLOCKING_TESTS;
//			// If the coverpoint was created by script, warn the scripts
//			if( m_Type == COVTYPE_SCRIPTED )
//			{
//				bRenderDebugSphere = true;
//
//				if( bCoverPositionSurface )
//				{
//					scriptAssertf(0, "SCRIPTED cover point 0x%X at <<%.2f,%.2f,%.2f>> is floating above surface (Z height should be: %.2f)", this, vCoverPosition.x, vCoverPosition.y, vCoverPosition.z, probedGroundHeight);
//				}
//				// Scripted points can be created before the map finishes loading in, so don't spam as the point may be fine once collision loads (dmorales)
//				//else
//				//{
//				//scriptAssertf(0, "SCRIPTED cover point 0x%X at <<%.2f,%.2f,%.2f>> is hovering in space (probe down %.2f from cover hit nothing!)", this, vCoverPosition.x, vCoverPosition.y, vCoverPosition.z, fProbeDownDistance);
//				//}
//			}
//			if( bRenderDebugSphere )
//			{
//				CTask::ms_debugDraw.AddSphere(RCC_VEC3V(vCoverPosition), SURFACE_TEST_HEIGHT_TOLERANCE, Color_red, 1000, 0, false);
//			}
//#endif // __DEV
//			return;
//		}
//	}
//
//	// If this is corner cover with a step out position, perform additional tests for clearance
//	bool bDoCornerCoverTests = ( m_Usage == COVUSE_WALLTOLEFT || m_Usage == COVUSE_WALLTORIGHT );
//
//	// Define the cover position capsule start and end points
//	Vector3 vCoverCapsuleStart = vCoverPosition;
//	Vector3 vCoverCapsuleEnd = vCoverPosition;
//	if( bHasProbedGroundHeight )
//	{
//		vCoverCapsuleStart.z = probedGroundHeight + COVER_TEST_LOW_HEIGHT;
//		vCoverCapsuleEnd.z = probedGroundHeight + COVER_TEST_HIGH_HEIGHT;
//	}
//	else
//	{
//		vCoverCapsuleStart.z += COVER_TEST_LOW_HEIGHT;
//		vCoverCapsuleEnd.z += COVER_TEST_HIGH_HEIGHT;
//	}
//
//	INC_PEDAI_LOS_COUNTER;
//
//	// Define the types to test for
//	s32 iCoverCapsuleTypeFlags = ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE;
//	// Map collision points only check against objects and vehicles, the point is already assumed to be correct in the world
//	if( m_Type != COVTYPE_POINTONMAP )
//	{
//		iCoverCapsuleTypeFlags |= ArchetypeFlags::GTA_ALL_MAP_TYPES;
//	}
//
//	// Set up the cover capsule desc
//	WorldProbe::CShapeTestCapsuleDesc coverCapsuleDesc;
//	coverCapsuleDesc.SetCapsule(vCoverCapsuleStart, vCoverCapsuleEnd, COVER_TEST_CAPSULE_RADIUS);
//	coverCapsuleDesc.SetIncludeFlags(iCoverCapsuleTypeFlags); 
//	coverCapsuleDesc.SetExcludeEntity(m_pEntity);
//#if __DEV
//	// set the results to render or report at cover position
//	coverCapsuleDesc.SetResultsStructure(&coverCapsuleResults);
//#endif // __DEV
//
//	// Set up the vantage position capsule if appropriate
//	WorldProbe::CShapeTestCapsuleDesc vantageCapsuleDesc;
//	Vector3 vVantagePosition, vVantageCapsuleStart, vVantageCapsuleEnd;
//	if( bDoCornerCoverTests )
//	{
//		// Set up the probe from the cover position to the vantage position
//		Vector3 vCoverDirection = VEC3V_TO_VECTOR3(GetCoverDirectionVector(NULL));
//		CCover::CalculateVantagePointForCoverPoint(this, vCoverDirection, vCoverPosition, vVantagePosition);
//		Vector3 vVantageDir = vVantagePosition - vCoverPosition;
//		vVantageDir.z = 0.0f;
//		vVantageDir.NormalizeSafe();
//		vVantageCapsuleStart = vCoverPosition;
//		vVantageCapsuleStart += (vVantageDir * VANTAGE_TEST_CAPSULE_RADIUS);
//		vVantageCapsuleEnd = vVantagePosition;
//		if( bHasProbedGroundHeight )
//		{
//			vVantageCapsuleStart.z = probedGroundHeight + VANTAGE_TEST_HEIGHT;
//			vVantageCapsuleEnd.z = probedGroundHeight + VANTAGE_TEST_HEIGHT;
//		}
//		else
//		{
//			vVantageCapsuleStart.z = vCoverPosition.z + VANTAGE_TEST_HEIGHT;
//			vVantageCapsuleEnd.z = vCoverPosition.z + VANTAGE_TEST_HEIGHT;
//		}
//		vantageCapsuleDesc.SetCapsule(vVantageCapsuleStart, vVantageCapsuleEnd, VANTAGE_TEST_CAPSULE_RADIUS);
//		// for the vantage testing, include objects, vehicles, and map types
//		s32 iVantageCapsuleTypeFlags = ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_ALL_MAP_TYPES;
//		vantageCapsuleDesc.SetIncludeFlags(iVantageCapsuleTypeFlags);
//		// exclude the cover user, if any
//		vantageCapsuleDesc.SetExcludeEntity(m_pEntity);
//#if __DEV
//		// set the vantage test results to render or report
//		vantageCapsuleDesc.SetResultsStructure(&vantageCapsuleResults);
//#endif // __DEV
//	}
//
//	// Is the cover position blocked?
//	bool bCoverPositionBlocked = WorldProbe::GetShapeTestManager()->SubmitTest(coverCapsuleDesc);
//
//	// If the cover position is clear and we are doing corner tests, is the vantage position blocked?
//	bool bVantagePositionBlocked = false;
//	if( !bCoverPositionBlocked && bDoCornerCoverTests )
//	{
//		bVantagePositionBlocked = WorldProbe::GetShapeTestManager()->SubmitTest(vantageCapsuleDesc);
//	}
//
//	// If the cover is blocked in some way
//	if(bCoverPositionBlocked || bVantagePositionBlocked)
//	{
//#if __DEV
//		// Check if this is a scripted cover point blocked by geometry
//		bool bIsScriptPointBlockedByMapGeometry = false;
//
//		// If the cover position is obstructed, warn the scripts
//		if ( bCoverPositionBlocked && m_Type == COVTYPE_SCRIPTED )
//		{
//			for (int i = 0; i < coverCapsuleResults.GetNumHits(); ++i)
//			{
//				const CEntity* pEntity = CPhysics::GetEntityFromInst(coverCapsuleResults[i].GetHitInst());
//				if (pEntity && (pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeMLO()) )
//				{
//					bIsScriptPointBlockedByMapGeometry = true;
//
//
//					scriptAssertf(0, "SCRIPTED cover point 0x%X at <<%.2f,%.2f,%.2f>> is being blocked by map geometry", this, vCoverPosition.x, vCoverPosition.y, vCoverPosition.z);
//				}
//			}
//		}
//
//		// If the cover vantage position is obstructed, warn the scripts
//		if ( bVantagePositionBlocked && m_Type == COVTYPE_SCRIPTED )
//		{
//			for (int i = 0; i < vantageCapsuleResults.GetNumHits(); ++i)
//			{
//				const CEntity* pEntity = CPhysics::GetEntityFromInst(vantageCapsuleResults[i].GetHitInst());
//				if (pEntity && (pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeMLO()) )
//				{
//					bIsScriptPointBlockedByMapGeometry = true;
//					scriptAssertf(0, "SCRIPTED cover point 0x%X at <<%.2f,%.2f,%.2f>> has vantage position blocked by map geometry", this, vCoverPosition.x, vCoverPosition.y, vCoverPosition.z);
//				}
//			}
//		}
//
//		// If this is a script point blocked by geometry
//		// OR we are debug rendering the tests
//		if (bIsScriptPointBlockedByMapGeometry || RENDER_COVERPOINT_BLOCKING_TESTS)
//		{
//			if( bCoverPositionBlocked )
//			{
//				CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vCoverCapsuleStart), RCC_VEC3V(vCoverCapsuleEnd), COVER_TEST_CAPSULE_RADIUS, Color_red, 1000, 0, false);
//			}
//
//			if( bVantagePositionBlocked )
//			{
//				CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vVantageCapsuleStart), RCC_VEC3V(vVantageCapsuleEnd), VANTAGE_TEST_CAPSULE_RADIUS, Color_red, 1000, 0, false);
//			}
//		}
//#endif // __DEV
//
//		// cover is blocked by something
//		m_CoverPointStatus |= COVSTATUS_PositionBlocked;
//	}
//	else
//	{
//		// cover is clear
//		m_CoverPointStatus |= COVSTATUS_Clear;
//
//#if __DEV
//		if (RENDER_COVERPOINT_BLOCKING_TESTS)
//		{
//			CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vCoverCapsuleStart), RCC_VEC3V(vCoverCapsuleEnd), COVER_TEST_CAPSULE_RADIUS, Color_green, 1000, 0, false);
//
//			if( bDoCornerCoverTests )
//			{
//				CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vVantageCapsuleStart), RCC_VEC3V(vVantageCapsuleEnd), VANTAGE_TEST_CAPSULE_RADIUS, Color_green, 1000, 0, false);
//			}
//		}
//#endif // __DEV
//	}
//}



//////////////////////////////////////////////////////
// FUNCTION: ReserveCoverPointForPed
// PURPOSE:  Srores this ped in the appropriate field so that the cover point is reserved for it.
//////////////////////////////////////////////////////

void CCoverPoint::ReserveCoverPointForPed(CPed *pPed)
{
	for (s32 C = 0; C < MAX_PEDS_PER_COVER_POINT; C++)
	{
		if (m_pPedsUsingThisCover[C] == pPed)
		{
			return;
		}
		if (m_pPedsUsingThisCover[C].Get() == NULL)
		{
			m_pPedsUsingThisCover[C] = pPed;

			// By default, peds are flagged as using a cover point they reserve
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UsingCoverPoint, true );
			return;
		}
	}
	Assert(0);
}

//////////////////////////////////////////////////////
// FUNCTION: ReleaseCoverPointForPed
// PURPOSE:  Stores this ped in the appropriate field so that the cover point is reserved for it.
//////////////////////////////////////////////////////

void CCoverPoint::ReleaseCoverPointForPed(CPed *pPed)
{
	for (s32 C = 0; C < MAX_PEDS_PER_COVER_POINT; C++)
	{
		if (m_pPedsUsingThisCover[C] == pPed)
		{
			m_pPedsUsingThisCover[C] = NULL;
		}
	}


}
//////////////////////////////////////////////////////
// FUNCTION: GetCoverDirectionVector
// PURPOSE:  Gets the directional vector from this cover point
//////////////////////////////////////////////////////
Vec3V_Out CCoverPoint::GetCoverDirectionVector( const Vec3V* pvTargetDir ) const
{
	switch( GetType() )
	{
	case COVTYPE_POINTONMAP:
	case COVTYPE_SCRIPTED:
		return CCover::FindVectorFromDir(m_CoverDirection);
	case COVTYPE_VEHICLE:
	case COVTYPE_BIG_OBJECT:
		if( m_pEntity )
		{
			const Vec3V localDirectionV = CCover::FindVectorFromDir(m_CoverDirection);
			const Vec3V worldDirectionV = CCachedObjectCoverManager::RotateLocalZToWorld(localDirectionV, *(CPhysical*)m_pEntity.Get());
			return worldDirectionV;
		}

		break;
	case COVTYPE_OBJECT:
		if( pvTargetDir && m_pEntity )
		{
			// TODO: should check how this gets used, if it's already always normalized, etc.
			return Normalize(*pvTargetDir);
		}
		Assertf(0, "There should be no more COVTYPE_OBJECT's in the game! Entity %s", m_pEntity ? m_pEntity->GetDebugName() : "NULL" );
		break;
	default:
		Assertf(0, "Invalid Cover Type : %i", GetType());
		break;
	}
	return Vec3V(V_Y_AXIS_WZERO);
}

//////////////////////////////////////////////////////
// FUNCTION: GetLocalDirectionVector
// PURPOSE:  Gets the local directional vector from this cover point
//////////////////////////////////////////////////////
Vector3 CCoverPoint::GetLocalDirectionVector()
{
	switch( GetType() )
	{
	case COVTYPE_POINTONMAP:
	case COVTYPE_SCRIPTED:
		return VEC3V_TO_VECTOR3(CCover::FindVectorFromDir(m_CoverDirection));
	case COVTYPE_VEHICLE:
	case COVTYPE_BIG_OBJECT:
		if( m_pEntity )
		{
			//Vector3 vDirection = CCover::FindVectorFromDir(m_CoverDirection);
			//Vector3 vConvertedDirection = vDirection;

			Vector3 vConvertedDirection = VEC3V_TO_VECTOR3(CCover::FindVectorFromDir(m_CoverDirection));

			CoverOrientation orientation = CCachedObjectCoverManager::GetUpAxis( (CPhysical*)m_pEntity.Get() );
			if( orientation == CO_XDown )
			{
				vConvertedDirection.RotateY(PI);
			}
			else if( orientation == CO_XUp )
			{
				vConvertedDirection.RotateY(-PI);
			}
			else if( orientation == CO_YDown )
			{
				vConvertedDirection.RotateX(PI);
			}
			else if( orientation == CO_YUp )
			{
				vConvertedDirection.RotateX(-PI);
			}

			return vConvertedDirection;
		}

		break;
	default:
		Assertf(0,"Can't be used on COVTYPE_OBJECT!");
		break;
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}
void CCoverPoint::GetLocalPosition(Vector3 & vCoverPos) const
{
	vCoverPos.x = m_CoorsX;
	vCoverPos.y = m_CoorsY;
	vCoverPos.z = m_CoorsZ;

	switch( GetType() )
	{
	case COVTYPE_VEHICLE:
	case COVTYPE_BIG_OBJECT:
		if( m_pEntity )
		{
			Vec3V vGlobalPos;
			GetCoverPointPosition(RC_VECTOR3(vGlobalPos), NULL);
			vCoverPos = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().UnTransform(vGlobalPos));
		}

		break;
	default:
		break;
	}
}


//////////////////////////////////////////////////////
// FUNCTION: GetCoverPointPosition
// PURPOSE:  Gets the position vector from this cover point
//////////////////////////////////////////////////////

bool
CCoverPoint::GetCoverPointPosition( Vector3 & vCoverPos, const Vector3* pLocalOffset ) const
{
	switch(m_Type)
	{
		case COVTYPE_POINTONMAP:
		case COVTYPE_SCRIPTED:
		{
			vCoverPos.x = m_CoorsX;
			vCoverPos.y = m_CoorsY;
			vCoverPos.z = m_CoorsZ;
			if( pLocalOffset )
			{
				vCoverPos += *pLocalOffset;
			}
			break;
		}
		case COVTYPE_VEHICLE:
		case COVTYPE_BIG_OBJECT:
		{
			if(m_pEntity)
			{
				// Initialize the coverpoint coordinates using the member values in local space
				Vector3 vCoors(m_CoorsX, m_CoorsY, m_CoorsZ);

				// Check for a local offset passed in
				if( pLocalOffset )
				{
					// Add the offset to the local cover coordinates
					vCoors += *pLocalOffset;
				}

				// Do a direct transform
				m_pEntity->TransformIntoWorldSpace(vCoverPos, vCoors);
			}
			else
			{
				Warningf("CCover::GetCoverPointPosition() - vehicle ptr is NULL");
#if __DEV
				float fNan; MakeNan(fNan);
				vCoverPos = Vector3(fNan, fNan, fNan);
#endif
				return false;
			}
			break;
		}
		case CCoverPoint::COVTYPE_OBJECT:
		{
			if(m_pEntity)
			{
				Vector3 vCoors(m_CoorsX, m_CoorsY, m_CoorsZ);
				m_pEntity->TransformIntoWorldSpace(vCoverPos, vCoors);

				//vCoverPos = m_pEntity->GetPosition() + Vector3(m_CoorsX, m_CoorsY, m_CoorsZ);
			}
			else
			{
				Warningf("CCover::GetCoverPointPosition() - object ptr is NULL");
#if __DEV
				float fNan; MakeNan(fNan);
				vCoverPos = Vector3(fNan, fNan, fNan);
#endif
				return false;
			}
			break;
		}
		default:
		{
			Displayf("CCover::GetCoverPointPosition() - coverpoint type was %i\n", m_Type);
			Assertf(0, "CCover::GetCoverPointPosition() - unknown coverpoint type");
			//vCoverPos = Vector3(0.0f, 0.0f, 0.0f);
			return false;
		}
	}

	return true;
}

Vec3V_Out CCoverPoint::GetCoverPointPosition() const
{
	Vector3 vTemp(0.0f,0.0f,0.0f);
	GetCoverPointPosition(vTemp);
	return VECTOR3_TO_VEC3V(vTemp);
}

void CCoverPoint::SetWorldDirection(const u8 dir)
{
	switch (m_Type)
	{
	case COVTYPE_VEHICLE:
	case COVTYPE_BIG_OBJECT:
		{
			Vector3 vDirection = VEC3V_TO_VECTOR3(CCover::FindVectorFromDir(dir));
			CCachedObjectCoverManager::RotateWorldToLocalZ(vDirection, static_cast<CPhysical*>(m_pEntity.Get()));
			m_CoverDirection = CCover::FindDirFromVector(vDirection);
		}
		break;

	default:
		m_CoverDirection = dir;
	}
}

void CCoverPoint::SetWorldPosition(const Vector3& vCoverWorldPos)
{
	switch (m_Type)
	{
	case COVTYPE_VEHICLE:
	case COVTYPE_BIG_OBJECT:
		{
			Vector3 vCoverPos = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vCoverWorldPos)));
			m_CoorsX = vCoverPos.x;
			m_CoorsY = vCoverPos.y;
			m_CoorsZ = vCoverPos.z;
		}
		break;

	default:
		m_CoorsX = vCoverWorldPos.x;
		m_CoorsY = vCoverWorldPos.y;
		m_CoorsZ = vCoverWorldPos.z;
	}
}

/*
bool CCoverPoint::UntransformPosition( Vector3 &  ) const
{
	if( m_pEntity )
	{Assert(0);
		return false;
	}
	return false;
}
*/

bool CCoverPoint::TransformPosition( Vector3 & vPos ) const
{
	if( m_pEntity )
	{
		CCachedObjectCoverManager::TransformLocalToWorld(vPos, (CPhysical*)m_pEntity.Get());
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////
// FUNCTION: Returns if the cover point is currently dangerous e.g a car on fire
// PURPOSE:  To ignore dangerous cover points or get peds to flee from them
//////////////////////////////////////////////////////
bool CCoverPoint::IsDangerous( void ) const
{
	switch(m_Type)
	{
	case COVTYPE_VEHICLE:
		if( m_pEntity && m_pEntity->GetType() == ENTITY_TYPE_VEHICLE )
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(m_pEntity.Get());
			if(pVehicle->IsOnFire())
			{
				return true;
			}
		}
		break;
	default:
		break;
	}
	return false;
}

bool CCoverPoint::IsEdgeCoverPoint(bool bFacingLeft) const
{
	return (m_Usage == CCoverPoint::COVUSE_WALLTONEITHER ||
			(!bFacingLeft && m_Usage == CCoverPoint::COVUSE_WALLTOLEFT) ||
			(bFacingLeft && m_Usage == CCoverPoint::COVUSE_WALLTORIGHT)) ? true : false;
}

bool CCoverPoint::IsEdgeCoverPoint() const
{
	return (m_Usage == CCoverPoint::COVUSE_WALLTOLEFT || m_Usage == CCoverPoint::COVUSE_WALLTORIGHT);
}

bool CCoverPoint::IsEdgeOrDoubleEdgeCoverPoint() const
{
	return (m_Usage == CCoverPoint::COVUSE_WALLTOLEFT || m_Usage == CCoverPoint::COVUSE_WALLTORIGHT || m_Usage == CCoverPoint::COVUSE_WALLTONEITHER);
}

CCoverPoint::eCoverArc CCoverPoint::GetArcToUse(const CPed& ped, const CEntity* pTarget) const
{
	if(!pTarget)
	{
		return (eCoverArc)m_CoverArc;
	}

	CPedIntelligence* pPedIntelligence = ped.GetPedIntelligence();
	if( !pPedIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableCoverArcAdjustments) &&
		pPedIntelligence->GetDefensiveArea() && pPedIntelligence->GetDefensiveArea()->IsActive() )
	{
		CPedTargetting* pPedTargeting = pPedIntelligence->GetTargetting(false);
		if(pPedTargeting && pPedTargeting->GetLosStatus(pTarget, false) == Los_blocked)
		{
			return GetArcToUse(false, true);
		}
	}

	return (eCoverArc)m_CoverArc;
}

CCoverPoint::eCoverArc CCoverPoint::GetArcToUse(bool bHasLosToTarget, bool bIsUsingDefensiveArea) const
{
	if(bHasLosToTarget || !bIsUsingDefensiveArea)
	{
		return (eCoverArc)m_CoverArc;
	}

	switch(m_CoverArc)
	{
		case CCoverPoint::COVARC_300TO0:
		case CCoverPoint::COVARC_315TO0:
		case CCoverPoint::COVARC_0TO45:
		case CCoverPoint::COVARC_0TO60:
		{
			return CCoverPoint::COVARC_120;
		}
		default:
			return (eCoverArc)m_CoverArc;
	}
}

bool CCoverPoint::IsCloseToPlayer(const CPed& ped) const
{
	//Get the cover point position.
	Vec3V vCoverPointPosition;
	GetCoverPointPosition(RC_VECTOR3(vCoverPointPosition));
	return IsCloseToPlayer(ped, vCoverPointPosition);
}

bool CCoverPoint::IsCloseToPlayer(const CPed& ped, Vec3V_In vCoverPointPosition)
{
	//The local player doesn't need to check if they are too close to themselves
	if(ped.IsLocalPlayer())
	{
		return false;
	}
	
	//Grab the player position.
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	Vec3V vPlayerPosition = pPlayer->GetTransform().GetPosition();
	
	//Ensure the height difference is within the threshold.
	ScalarV scHeightDifference = Abs(Subtract(vCoverPointPosition.GetZ(), vPlayerPosition.GetZ()));
	ScalarV scMaxHeightDifference = ScalarV(V_FIVE);
	if(IsGreaterThanAll(scHeightDifference, scMaxHeightDifference))
	{
		return false;
	}
	
	//Ensure the XY distance is within the threshold.
	Vec3V vCoverPointPositionXY = vCoverPointPosition;
	vCoverPointPositionXY.SetZ(ScalarV(V_ZERO));
	Vec3V vPlayerPositionXY = vPlayerPosition;
	vPlayerPositionXY.SetZ(ScalarV(V_ZERO));
	ScalarV scDistSq = DistSquared(vCoverPointPositionXY, vPlayerPositionXY);
	ScalarV scMaxDistSq = ScalarV(V_ONE);
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}
	
	return true;
}

//-------------------------------------------------------------------------
// Returns the cover point height enum from the given floating point height
//-------------------------------------------------------------------------
CCoverPoint::eCoverHeight CCover::FindCoverPointHeight( const float fCoverHeight, s32 iType )
{
	float iHighCoverHeight = iType == CCoverPoint::COVTYPE_VEHICLE ?  HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES : HIGH_COVER_MAX_HEIGHT;

	CCoverPoint::eCoverHeight iCoverHeight;
	// Calculate the enumerated height from the height of collision
	if( fCoverHeight < LOW_COVER_MAX_HEIGHT )
	{
		iCoverHeight	= CCoverPoint::COVHEIGHT_LOW;
	}
	else if( fCoverHeight < iHighCoverHeight )
	{
		iCoverHeight	= CCoverPoint::COVHEIGHT_HIGH;
	}
	else
	{
		iCoverHeight	= CCoverPoint::COVHEIGHT_TOOHIGH;
	}

	return iCoverHeight;
}

//-------------------------------------------------------------------------
// Returns the cover point height enum from the given floating point height
//-------------------------------------------------------------------------
CCoverPoint::eCoverUsage CCover::GetCoverPointUsageFromWidth( const float fCoverWidth )
{
	// Calculate the enumerated height from the height of collision
	if( fCoverWidth < ( MAX_COVER_SIDEWAYS_REACH * 2.0f ) )
	{
		return CCoverPoint::COVUSE_WALLTONEITHER;
	}
	else
	{
		return CCoverPoint::COVUSE_WALLTOBOTH;
	}
}

//-------------------------------------------------------------------------
// Determine whether a cover point is a low corner
//-------------------------------------------------------------------------
bool CCover::IsCoverLowCorner( const CCoverPoint::eCoverHeight eHeight, const CCoverPoint::eCoverUsage eUsage)
{
	// if the height is either LOW or HIGH and usage is WALLTOLEFT/RIGHT
	return (eHeight < CCoverPoint::COVHEIGHT_TOOHIGH && (eUsage == CCoverPoint::COVUSE_WALLTOLEFT || eUsage == CCoverPoint::COVUSE_WALLTORIGHT));
}


CCoverPoint::CCoverPoint()
{
	m_pEntity = NULL;
	for (s32 C = 0; C < MAX_PEDS_PER_COVER_POINT; C++)
		m_pPedsUsingThisCover[C] = NULL;
	m_pPrevCoverPoint = NULL;
	m_pNextCoverPoint = NULL;
	m_pOwningGridCell = NULL;
	
	m_Flags = 0;
	m_iCachedObjectCoverIndex = -1;
}

void CCoverPoint::Copy(const CCoverPoint& other)
{
	m_pEntity = other.m_pEntity;
	for (s32 C = 0; C < MAX_PEDS_PER_COVER_POINT; C++)
		m_pPedsUsingThisCover[C] = NULL;
	m_pPrevCoverPoint = NULL;
	m_pNextCoverPoint = NULL;
	m_pOwningGridCell = NULL;
	
	m_Type = other.m_Type;			
	m_Height = other.m_Height;			
	m_Usage	= other.m_Usage;			
	m_CoverArc = other.m_CoverArc;		
	m_CoverPointStatus = other.m_CoverPointStatus;
	m_CoverDirection = other.m_CoverDirection;		
	m_Flags = other.m_Flags;
	m_CoorsX = other.m_CoorsX;
	m_CoorsY = other.m_CoorsY;
	m_CoorsZ = other.m_CoorsZ;
}

bool CCoverPoint::IsCoverPointMoving( float fVelocityThreshhold )
{
	if( m_pEntity && m_pEntity->GetIsPhysical() )
	{
		if( static_cast<CPhysical*>(m_pEntity.Get())->GetVelocity().Mag2() > rage::square(fVelocityThreshhold) )
		{
			return true;
		}
	}
	return false;
}

bool CCoverPoint::PedClosestToFacingLeft(const CPed& rPed) const
{
	const Vector3 vCoverDirection = VEC3V_TO_VECTOR3(GetCoverDirectionVector());
	const Vector3 vPedForward = VEC3V_TO_VECTOR3(rPed.GetTransform().GetB());
	return vPedForward.CrossZ(vCoverDirection) < 0.0f ? true : false;
}

//-------------------------------------------------------------------------
// Bounding area constructor
//-------------------------------------------------------------------------
CCoverBoundingArea::CCoverBoundingArea()
{
}

//-------------------------------------------------------------------------
// Bounding area destructor
//-------------------------------------------------------------------------
CCoverBoundingArea::~CCoverBoundingArea()
{

}

//-------------------------------------------------------------------------
// initialises the bounding box with the center and extents specified
//-------------------------------------------------------------------------
void CCoverBoundingArea::init( const Vector3& vCentre, const Vector3& vExtents )
{
	m_vBoundingMin = vCentre - vExtents;
	m_vBoundingMax = vCentre + vExtents;
}

//-------------------------------------------------------------------------
// initialises the bounding box with the center and extents specified
//-------------------------------------------------------------------------
void CCoverBoundingArea::initFromMinMax( const Vector3& vMin, const Vector3& vMax )
{
	m_vBoundingMin = vMin;
	m_vBoundingMax = vMax;
}

//-------------------------------------------------------------------------
// Attempts to expand the bounding box to include the area specified, 
// but keeping within the maximum extents, returns true if successful
//-------------------------------------------------------------------------
bool CCoverBoundingArea::ExpandToInclude( const Vector3& vCentre, const Vector3& vExtents, const Vector3& vMaxExtents )
{
	// Check the expansion to make sure it won't violate the maximum extents

	// Work out the new box to be included
	const Vector3 vBoxBoundingMin = vCentre - vExtents;
	const Vector3 vBoxBoundingMax = vCentre + vExtents;

	// Work out the new extents
	Vector3 vNewBoundingBoxMin( MIN( vBoxBoundingMin.x, m_vBoundingMin.x ),
		MIN( vBoxBoundingMin.y, m_vBoundingMin.y ),
		MIN( vBoxBoundingMin.z, m_vBoundingMin.z ) );

	Vector3 vNewBoundingBoxMax( MAX( vBoxBoundingMax.x, m_vBoundingMax.x ),
		MAX( vBoxBoundingMax.y, m_vBoundingMax.y ),
		MAX( vBoxBoundingMax.z, m_vBoundingMax.z ) );

	// If the areas are already very similar, include this coverpoint
	if( ( m_vBoundingMin.Dist2(vNewBoundingBoxMin) < 1.0f ) && 
		( m_vBoundingMax.Dist2(vNewBoundingBoxMax) < 1.0f  ) )
	{
		// Same size (approximately) as existing area, allow it to be expanded
	}
	// If any of these extents violate the maximum extents, return failure to expand
	else if( ( ( vNewBoundingBoxMax.x - vNewBoundingBoxMin.x ) > ( vMaxExtents.x * 2.0f ) ) ||
		( ( vNewBoundingBoxMax.y - vNewBoundingBoxMin.y ) > ( vMaxExtents.y * 2.0f ) ) ||
		( ( vNewBoundingBoxMax.z - vNewBoundingBoxMin.z ) > ( vMaxExtents.z * 2.0f ) ) )
	{
		return false;
	}

	// New expansion doesn't violate maximum extents, record new extents
	m_vBoundingMin = vNewBoundingBoxMin;
	m_vBoundingMax = vNewBoundingBoxMax;

	return true;
}

//-------------------------------------------------------------------------
// Setup the cover point
//-------------------------------------------------------------------------
void CCoverPointInfo::init( Vector3& vOffset, u8 iDir, float fCoverHeight, s32 iUsage, s32 iCoverArc, s32 iType )
{
	m_vLocalOffset	= vOffset;
	m_iCoverArc		= iCoverArc;
	m_iUsage		= iUsage;
	m_iLocalDir		= iDir;
	m_bThin			= false;

	// Calculate the enumerated height from the height of collision
	m_iCoverHeight = CCover::FindCoverPointHeight( fCoverHeight, iType );

	// Compute whether this is a low corner
	m_bLowCorner = CCover::IsCoverLowCorner( (CCoverPoint::eCoverHeight)m_iCoverHeight, (CCoverPoint::eCoverUsage)m_iUsage );

	// If a point can't be fired over the top of, but can be used to fire round a corner,
	// limit the angles so it can only be used in one direction
	if( m_iCoverHeight == CCoverPoint::COVHEIGHT_TOOHIGH || m_bLowCorner )
	{
		AdjustArcForCorner();
	}
}
//-------------------------------------------------------------------------
// Setup the cover point
//-------------------------------------------------------------------------
void CCoverPointInfo::init( Vector3& vOffset, float fDir, float fCoverHeight, s32 iUsage, s32 iCoverArc, s32 iType )
{
	init( vOffset, 
		 static_cast<u8>(( DtoR * fDir) * (COVER_POINT_DIR_RANGE / (2.0f * PI))), 
		 fCoverHeight, 
		 iUsage, 
		 iCoverArc, iType );
}

//-------------------------------------------------------------------------
// Modify cover arc for corner cover
//-------------------------------------------------------------------------
void CCoverPointInfo::AdjustArcForCorner()
{
	if( m_iUsage == CCoverPoint::COVUSE_WALLTOLEFT )
	{
		if( m_iCoverArc == CCoverPoint::COVARC_45 )
		{
			m_iCoverArc = CCoverPoint::COVARC_0TO22;
		}
		else if( m_iCoverArc == CCoverPoint::COVARC_90 )
		{
			m_iCoverArc = CCoverPoint::COVARC_0TO45;
		}
		else if( m_iCoverArc == CCoverPoint::COVARC_120 )
		{
			m_iCoverArc = CCoverPoint::COVARC_0TO60;
		}
	}
	else if( m_iUsage == CCoverPoint::COVUSE_WALLTORIGHT )
	{
		if( m_iCoverArc == CCoverPoint::COVARC_45 )
		{
			m_iCoverArc = CCoverPoint::COVARC_338TO0;
		}
		else if( m_iCoverArc == CCoverPoint::COVARC_90 )
		{
			m_iCoverArc = CCoverPoint::COVARC_315TO0;
		}
		else if( m_iCoverArc == CCoverPoint::COVARC_120 )
		{
			m_iCoverArc = CCoverPoint::COVARC_300TO0;
		}
	}
	else if( m_iUsage == CCoverPoint::COVUSE_WALLTOBOTH )
	{
		m_iCoverArc = CCoverPoint::COVARC_180;
	}
}

//-------------------------------------------------------------------------
// OBJECT COVERPOINT CACHING
//-------------------------------------------------------------------------
CCachedObjectCover	CCachedObjectCoverManager::m_aCachedCoverPoints[MAX_CACHED_OBJECT_COVER_POINTS];
s32				CCachedObjectCoverManager::m_iNextFreeCachedObject = 0;


//-------------------------------------------------------------------------
// Adds coverpoints for an object of the type cached by this instance
//-------------------------------------------------------------------------
bool CCachedObjectCover::AddCoverPoints( CPhysical* pPhysical )
{
	bool bAllPointsAdded = true;

	for( s32 i = 0; i < m_iCoverPoints; i++ )
	{
		// Do not re-add cover for the current index
		// if it has been marked as blocked on the physical
		if( pPhysical->IsCachedObjectCoverObstructed(i) )
		{
			continue;
		}

		CCoverPointInfo *				pCoverPointInfo	= &m_aCoverPoints[i];
		const CCoverPoint::eCoverUsage	coverUsage		= (CCoverPoint::eCoverUsage) pCoverPointInfo->m_iUsage;
		const CCoverPoint::eCoverHeight	coverHeight		= (CCoverPoint::eCoverHeight)pCoverPointInfo->m_iCoverHeight;
		const CCoverPoint::eCoverArc	coverArc		= (CCoverPoint::eCoverArc)pCoverPointInfo->m_iCoverArc;
		const u8 						iLocalDir		= pCoverPointInfo->m_iLocalDir;
		Vector3							vLocalOffset	= pCoverPointInfo->m_vLocalOffset;

		// Default to big object
		CCoverPoint::eCoverType eType = CCoverPoint::COVTYPE_BIG_OBJECT;
		// If a single point and member flag is not set, type is OBJECT
		if( m_iCoverPoints == 1 && !m_bForceTypeObject )
		{
			eType = CCoverPoint::COVTYPE_OBJECT;
		}

		// Get the index of the coverpoint if it already exists for the given coverpoint info
		s16 iExistingCoverIndex = INVALID_COVER_POINT_INDEX;

		// not a priority
		bool bIsPriority = false;

		// not a door ID
		eHierarchyId doorID = VEH_INVALID_ID;

		// Try to add the cover point
		s16 iCoverIndex = CCover::AddCoverPoint(CCover::CP_NavmeshAndDynamicPoints, eType, pPhysical, &vLocalOffset, coverHeight, coverUsage, iLocalDir, coverArc, bIsPriority, doorID, &iExistingCoverIndex );

		// If add failed due to existing coverpoint, use that index
		if( iCoverIndex == INVALID_COVER_POINT_INDEX && iExistingCoverIndex != INVALID_COVER_POINT_INDEX )
		{
			iCoverIndex = iExistingCoverIndex;
		}

		// Keep track if the add fails
		bAllPointsAdded = bAllPointsAdded && iCoverIndex != INVALID_COVER_POINT_INDEX;

		// If the point was successfully added or existing cover found
		if( iCoverIndex != INVALID_COVER_POINT_INDEX )
		{
			// Get a handle to the cover point
			CCoverPoint* pCoverPoint = CCover::FindCoverPointWithIndex( iCoverIndex );

			// Mark the coverpoint as thin if required
			pCoverPoint->SetIsThin( pCoverPointInfo->m_bThin );

			// Mark the coverpoint as low corner if required
			if( pCoverPointInfo->m_bLowCorner )
			{
				pCoverPoint->SetHeight( CCoverPoint::COVHEIGHT_LOW );
				pCoverPoint->SetFlag( CCoverPoint::COVFLAGS_IsLowCorner );
			}

			// Set the cached object cover index used to create the coverpoint
			if( AssertVerify(i < MAX_COVERPOINTS_PER_OBJECT ) )
			{
				pCoverPoint->SetCachedObjectCoverIndex((s8)i);
			}
		}
	}
	return bAllPointsAdded;
}

// If this fails, CObjectCoverExtension::m_CachedObjectCoverBlockedFlags (see Physical.h)
// needs to be resized to accomodate more than 16 index values, and this check updated to reflect the new capacity.
CompileTimeAssert( CCachedObjectCover::MAX_COVERPOINTS_PER_OBJECT <= 16 );

static float HEIGHT_CHECK_OFFSET = 2.0f;
//static float THIN_COVER_POINT_WIDTH = 0.25f;
//-------------------------------------------------------------------------
// Setup this cache from the object
//-------------------------------------------------------------------------
void CCachedObjectCover::Setup( CPhysical* pPhysical )
{
	CBaseModelInfo* pModelInfo = pPhysical->GetBaseModelInfo();
	m_iCoverPoints = 0;
	m_iModelHashKey = pModelInfo->GetHashKey();
	m_bActive = true;
	m_bForceTypeObject = false;

	// Get the object cover tuning, if any.
	// Set conditions according to any tuning settings.
	const CCoverTuning& coverTuning = CCoverTuningManager::GetInstance().GetTuningForModel(m_iModelHashKey);
	
	// Default is all zeros, any flag set means custom tuning data
	bool bHasCustomCoverTuningData = !( coverTuning.m_Flags == 0 );

	bool bSkipNorthFaceEast =	(coverTuning.m_Flags & CCoverTuning::NoCoverNorthFaceEast) > 0;
	bool bSkipNorthFaceWest =	(coverTuning.m_Flags & CCoverTuning::NoCoverNorthFaceWest) > 0;
	bool bSkipNorthFaceCenter =	(coverTuning.m_Flags & CCoverTuning::NoCoverNorthFaceCenter) > 0;
	bool bSkipSouthFaceEast =	(coverTuning.m_Flags & CCoverTuning::NoCoverSouthFaceEast) > 0;
	bool bSkipSouthFaceWest =	(coverTuning.m_Flags & CCoverTuning::NoCoverSouthFaceWest) > 0;
	bool bSkipSouthFaceCenter =	(coverTuning.m_Flags & CCoverTuning::NoCoverSouthFaceCenter) > 0;
	bool bSkipEastFaceNorth =	(coverTuning.m_Flags & CCoverTuning::NoCoverEastFaceNorth) > 0;
	bool bSkipEastFaceSouth =	(coverTuning.m_Flags & CCoverTuning::NoCoverEastFaceSouth) > 0;
	bool bSkipEastFaceCenter =	(coverTuning.m_Flags & CCoverTuning::NoCoverEastFaceCenter) > 0;
	bool bSkipWestFaceNorth =	(coverTuning.m_Flags & CCoverTuning::NoCoverWestFaceNorth) > 0;
	bool bSkipWestFaceSouth =	(coverTuning.m_Flags & CCoverTuning::NoCoverWestFaceSouth) > 0;
	bool bSkipWestFaceCenter =	(coverTuning.m_Flags & CCoverTuning::NoCoverWestFaceCenter) > 0;

	bool bForceLowCornerNorthFaceEast = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerNorthFaceEast) > 0;
	bool bForceLowCornerNorthFaceWest = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerNorthFaceWest) > 0;
	bool bForceLowCornerSouthFaceEast = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerSouthFaceEast) > 0;
	bool bForceLowCornerSouthFaceWest = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerSouthFaceWest) > 0;
	bool bForceLowCornerEastFaceNorth = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerEastFaceNorth) > 0;
	bool bForceLowCornerEastFaceSouth = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerEastFaceSouth) > 0;
	bool bForceLowCornerWestFaceNorth = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerWestFaceNorth) > 0;
	bool bForceLowCornerWestFaceSouth = (coverTuning.m_Flags & CCoverTuning::ForceLowCornerWestFaceSouth) > 0;

	// Initial object height
	const float fInitialObjectHeight = pPhysical->GetBoundingBoxMax().z - pPhysical->GetBoundingBoxMin().z;

	// Height check offset
	float fHeightCheckOffset = MIN(fInitialObjectHeight, HEIGHT_CHECK_OFFSET);

	// Estimate the orientation
	m_eObjectOrientation = CCachedObjectCoverManager::GetUpAxis(pPhysical);
	Matrix34 mFakeMat;
	mFakeMat = MAT34V_TO_MATRIX34(pPhysical->GetMatrix());
	if( m_eObjectOrientation == CO_XUp || m_eObjectOrientation == CO_XDown )
	{
		mFakeMat.a = Vector3(0.0f, 0.0f, 1.0f);
		mFakeMat.b = Vector3(1.0f, 0.0f, 0.0f);
		mFakeMat.c = Vector3(0.0f, 1.0f, 0.0f);
	}
	else if( m_eObjectOrientation == CO_YUp || m_eObjectOrientation == CO_YDown )
	{
		mFakeMat.a = Vector3(1.0f, 0.0f, 0.0f);
		mFakeMat.b = Vector3(0.0f, 0.0f, 1.0f);
		mFakeMat.c = Vector3(0.0f, 1.0f, 0.0f);
	}
	else if( m_eObjectOrientation == CO_ZUp || m_eObjectOrientation == CO_ZDown )
	{
		mFakeMat.a = Vector3(1.0f, 0.0f, 0.0f);
		mFakeMat.b = Vector3(0.0f, 1.0f, 0.0f);
		mFakeMat.c = Vector3(0.0f, 0.0f, 1.0f);
	}

	const Vector3 vPhysicalPosition = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());
	// Use the EntityBound class to calculate the real bounding box at ground coordinates
	// Raise the height up by 1 unit because the entity bound AI assumes it is 1 unit above the ground
	CEntityBoundAI pObjectBounds( *pPhysical, vPhysicalPosition.z + pPhysical->GetBoundingBoxMin().z + fHeightCheckOffset, 0.0f, false, &mFakeMat );

	// Get the list of corners from the entitybound
	Vector3 avCorners[4];
	pObjectBounds.GetCorners(avCorners);

	// These corners are in world coordinates, convert them to local
	for( int i = 0; i < 4; i++ )
	{
		avCorners[i] = avCorners[i] - vPhysicalPosition;
	}

	// Compute the Z height from the physical bounds
	float fBoundingMinZ = pPhysical->GetBoundingBoxMin().z;
	float fBoundingMaxZ = pPhysical->GetBoundingBoxMax().z;
	if( m_eObjectOrientation == CO_XUp || m_eObjectOrientation == CO_XDown )
	{
		fBoundingMinZ = pPhysical->GetBoundingBoxMin().x;
		fBoundingMaxZ = pPhysical->GetBoundingBoxMax().x;
	}	
	else if( m_eObjectOrientation == CO_YUp || m_eObjectOrientation == CO_YDown )
	{
		fBoundingMinZ = pPhysical->GetBoundingBoxMin().y;
		fBoundingMaxZ = pPhysical->GetBoundingBoxMax().y;
	}

	// Calculate the new bounding boxes
	Vector3 vBoundingMin( Min( avCorners[0].x, Min( avCorners[1].x, Min( avCorners[2].x, avCorners[3].x) ) ),
		Min( avCorners[0].y, Min( avCorners[1].y, Min( avCorners[2].y, avCorners[3].y) ) ),
		fBoundingMinZ );

	// Calculate the new bounding boxes
	Vector3 vBoundingMax( Max( avCorners[0].x, Max( avCorners[1].x, Max( avCorners[2].x, avCorners[3].x) ) ),
		Max( avCorners[0].y, Max( avCorners[1].y, Max( avCorners[2].y, avCorners[3].y) ) ),
		fBoundingMaxZ );

	// Record the bounding box extents in 2 dimensions
	m_vBoundingMin = vBoundingMin;
	m_vBoundingMax = vBoundingMax;

	// Work out the vertical and horizontal extents of the cover object
	const float fObjectHeight = vBoundingMax.z - vBoundingMin.z;
	const float fObjectWidthY = vBoundingMax.y - vBoundingMin.y;
	const float fObjectWidthX = vBoundingMax.x - vBoundingMin.x;

	// If the cover object is too short, do not create coverpoints on it
	if( fObjectHeight < MIN_OBJECT_HEIGHT_FOR_COVER )
	{
		return;
	}

	// Work out the height of the coverpoint from the maximum
	const CCoverPoint::eCoverHeight eCoverHeight = CCover::FindCoverPointHeight( fObjectHeight );
	
	// Distance to the object based on height
	const float fCoverToObject = eCoverHeight != CCoverPoint::COVHEIGHT_TOOHIGH ? DISTANCE_FROM_COVER_TO_OBJECT_LOW : DISTANCE_FROM_COVER_TO_OBJECT_HIGH;
	
	// Local variable for offset positions
	Vector3 vOffset(0.0f, 0.0f, 0.0f);

	// Local variable for cover height
	float fEffectiveCoverHeight = fObjectHeight;

	// Constant height for forcing low corner cover
	const float fCoverHeightLowCorner = LOW_COVER_MAX_HEIGHT - 0.01f;

	// If the width is less than this value, it will only have a single coverpoint, note this varies depending on low and high objects 
	// (whether the ped can see over the top)
	// NOTE: Too small objects are currently not generating ANY coverpoints -- see below bAddXX booleans (dmorales)
	const float fWidthForSingleCP = eCoverHeight == CCoverPoint::COVHEIGHT_TOOHIGH ? OBJECT_HIGH_WIDTH_FOR_SINGLE_END_CP : OBJECT_WIDTH_FOR_SINGLE_END_CP;
	const float fMinWidthForSingleCP = MIN_OBJECT_WIDTH_FOR_SINGLE_END_CP;

	// Examine object scale properties
	const bool bObjectWidthXQualifiesForCorners = fObjectWidthX >= fWidthForSingleCP;
	const bool bObjectWidthYQualifiesForCorners = fObjectWidthY >= fWidthForSingleCP;
	const bool bObjectWidthXQualifiesForMidsOnly = fObjectWidthX >= fMinWidthForSingleCP && fObjectWidthX < fWidthForSingleCP;
	const bool bObjectWidthYQualifiesForMidsOnly = fObjectWidthY >= fMinWidthForSingleCP && fObjectWidthY < fWidthForSingleCP;
	const bool bObjectWidthXAndHeightQualifiesForMids = fObjectWidthX > OBJECT_WIDTH_FOR_CENTER_CP && eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH ;
	const bool bObjectWidthYAndHeightQualifiesForMids = fObjectWidthY > OBJECT_WIDTH_FOR_CENTER_CP && eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH ;
	
	// Check for special small object case
	const bool bIsSmallObject = bObjectWidthXQualifiesForMidsOnly && bObjectWidthYQualifiesForMidsOnly;
	if( bIsSmallObject )
	{
		// Small object cover points:
		// The idea here is to use more points with narrower cover arcs,
		// to help AI make good use of small cover objects
		//
		//   yx  yx  yx
		//    |-----|				x points are corner cover added if the object is too high to shoot over
		//    |		|				y points are popover cover added if the object height qualifies for popover
		//  yx|		|yx				
		//    |		|
		//    |-----|
		//   yx  yx  yx
		//
		const float fCoverToSmallObjectDiagonal = 0.33f * fCoverToObject;
		LocalOffsetDataParams_Small smallOffsetParams(VECTOR3_TO_VEC3V(pPhysical->GetBoundingBoxMin()), VECTOR3_TO_VEC3V(pPhysical->GetBoundingBoxMax()), ScalarV(fCoverToObject), ScalarV(fCoverToSmallObjectDiagonal));
		LocalOffsetData_Small smallOffsets;
		if( !HelperComputeLocalOffsetData_Small(m_eObjectOrientation, smallOffsetParams, smallOffsets) )
		{
			m_bActive = false;
			return;
		}
		const bool bAddPopoverPoints = eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH;
		if( bAddPopoverPoints )
		{
			// Front popover
			if( !bSkipNorthFaceCenter )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_NorthCenter);
				m_aCoverPoints[m_iCoverPoints++].init( vOffset, smallOffsets.m_fDirCoverNorth, fObjectHeight, CCoverPoint::COVUSE_WALLTOBOTH, CCoverPoint::COVARC_45 );
			}

			// Rear popover
			if( !bSkipSouthFaceCenter )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_SouthCenter);
				m_aCoverPoints[m_iCoverPoints++].init( vOffset, smallOffsets.m_fDirCoverSouth, fObjectHeight, CCoverPoint::COVUSE_WALLTOBOTH, CCoverPoint::COVARC_45 );
			}

			// Left popover
			if( !bSkipWestFaceCenter )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_WestCenter);
				m_aCoverPoints[m_iCoverPoints++].init( vOffset, smallOffsets.m_fDirCoverWest, fObjectHeight, CCoverPoint::COVUSE_WALLTOBOTH, CCoverPoint::COVARC_45 );
			}

			// Right popover
			if( !bSkipEastFaceCenter )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_EastCenter);
				m_aCoverPoints[m_iCoverPoints++].init( vOffset, smallOffsets.m_fDirCoverEast, fObjectHeight, CCoverPoint::COVUSE_WALLTOBOTH, CCoverPoint::COVARC_45 );
			}

			// Front Left popover	
			if( !bSkipNorthFaceWest && !bSkipWestFaceNorth )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_NorthWest);
				m_aCoverPoints[m_iCoverPoints++].init( vOffset, smallOffsets.m_fDirCoverNorthWest, fObjectHeight, CCoverPoint::COVUSE_WALLTOBOTH, CCoverPoint::COVARC_45 );
			}

			// Front Right popover
			if( !bSkipNorthFaceEast && !bSkipEastFaceNorth )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_NorthEast);
				m_aCoverPoints[m_iCoverPoints++].init( vOffset, smallOffsets.m_fDirCoverNorthEast, fObjectHeight, CCoverPoint::COVUSE_WALLTOBOTH, CCoverPoint::COVARC_45 );
			}

			// Rear Left popover
			if( !bSkipSouthFaceWest && !bSkipWestFaceSouth )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_SouthWest);
				m_aCoverPoints[m_iCoverPoints++].init( vOffset, smallOffsets.m_fDirCoverSouthWest, fObjectHeight, CCoverPoint::COVUSE_WALLTOBOTH, CCoverPoint::COVARC_45 );
			}

			// Rear Right popover
			if( !bSkipSouthFaceEast && !bSkipEastFaceSouth )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_SouthEast);
				m_aCoverPoints[m_iCoverPoints++].init( vOffset, smallOffsets.m_fDirCoverSouthEast, fObjectHeight, CCoverPoint::COVUSE_WALLTOBOTH, CCoverPoint::COVARC_45 );
			}
		}
		else // add high corner cover points (eCoverHeight == CCoverPoint::COVHEIGHT_TOOHIGH)
		{
			// Front corner
			if( !bSkipNorthFaceCenter )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_NorthCenter);
				fEffectiveCoverHeight = fObjectHeight;
				// force low corner
				if(bForceLowCornerNorthFaceEast || bForceLowCornerNorthFaceWest)
				{
					fEffectiveCoverHeight = fCoverHeightLowCorner;
				}
				m_aCoverPoints[m_iCoverPoints].init( vOffset, smallOffsets.m_fDirCoverNorth, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
				m_iCoverPoints++;
			}

			// Rear corner
			if( !bSkipSouthFaceCenter )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_SouthCenter);
				fEffectiveCoverHeight = fObjectHeight;
				// force low corner
				if(bForceLowCornerSouthFaceEast || bForceLowCornerSouthFaceWest)
				{
					fEffectiveCoverHeight = fCoverHeightLowCorner;
				}
				m_aCoverPoints[m_iCoverPoints].init( vOffset, smallOffsets.m_fDirCoverSouth, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
				m_iCoverPoints++;
			}

			// Left corner
			if( !bSkipWestFaceCenter )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_WestCenter);
				fEffectiveCoverHeight = fObjectHeight;
				// force low corner
				if(bForceLowCornerWestFaceNorth || bForceLowCornerWestFaceSouth)
				{
					fEffectiveCoverHeight = fCoverHeightLowCorner;
				}
				m_aCoverPoints[m_iCoverPoints].init( vOffset, smallOffsets.m_fDirCoverWest, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
				m_iCoverPoints++;
			}

			// Right corner
			if( !bSkipEastFaceCenter )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_EastCenter);
				fEffectiveCoverHeight = fObjectHeight;
				// force low corner
				if(bForceLowCornerEastFaceNorth || bForceLowCornerEastFaceSouth)
				{
					fEffectiveCoverHeight = fCoverHeightLowCorner;
				}
				m_aCoverPoints[m_iCoverPoints].init( vOffset, smallOffsets.m_fDirCoverEast, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
				m_iCoverPoints++;
			}

			// Front Left corner
			if( !bSkipNorthFaceWest && !bSkipWestFaceNorth )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_NorthWest);
				fEffectiveCoverHeight = fObjectHeight;
				// force low corner
				if(bForceLowCornerNorthFaceWest || bForceLowCornerWestFaceNorth)
				{
					fEffectiveCoverHeight = fCoverHeightLowCorner;
				}
				m_aCoverPoints[m_iCoverPoints].init( vOffset, smallOffsets.m_fDirCoverNorthWest, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
				m_iCoverPoints++;
			}

			// Front Right corner
			if( !bSkipNorthFaceEast && !bSkipEastFaceNorth )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_NorthEast);
				fEffectiveCoverHeight = fObjectHeight;
				// force low corner
				if(bForceLowCornerNorthFaceEast || bForceLowCornerEastFaceNorth)
				{
					fEffectiveCoverHeight = fCoverHeightLowCorner;
				}
				m_aCoverPoints[m_iCoverPoints].init( vOffset, smallOffsets.m_fDirCoverNorthEast, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
				m_iCoverPoints++;
			}

			// Rear Left corner
			if( !bSkipSouthFaceWest && !bSkipWestFaceSouth )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_SouthWest);
				fEffectiveCoverHeight = fObjectHeight;
				// force low corner
				if(bForceLowCornerSouthFaceWest || bForceLowCornerWestFaceSouth)
				{
					fEffectiveCoverHeight = fCoverHeightLowCorner;
				}
				m_aCoverPoints[m_iCoverPoints].init( vOffset, smallOffsets.m_fDirCoverSouthWest, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
				m_iCoverPoints++;
			}

			// Rear Right corner
			if( !bSkipSouthFaceEast && !bSkipEastFaceSouth )
			{
				vOffset = VEC3V_TO_VECTOR3(smallOffsets.m_SouthEast);
				fEffectiveCoverHeight = fObjectHeight;
				// force low corner
				if(bForceLowCornerSouthFaceEast || bForceLowCornerEastFaceSouth)
				{
					fEffectiveCoverHeight = fCoverHeightLowCorner;
				}
				m_aCoverPoints[m_iCoverPoints].init( vOffset, smallOffsets.m_fDirCoverSouthEast, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
				m_iCoverPoints++;
			}
		}

		// If we have custom tuning data and one coverpoint, mark the member flag
		if( bHasCustomCoverTuningData && m_iCoverPoints == 1 )
		{
			m_bForceTypeObject = true;
		}
		
		// object cover assigned, exit this method.
		return;
	}


	//  General case object cover points:
	//
	//     x y x
	//	 x|-----|x				x points are added if the object is wide enough to have corner points at each corner
	//	  |		|				y points are added if the object is too narrow for x points, or is long enough to have 3 points
	//	  |		|				  along an edge
	//	 y|     |y
	//    |		|
	//    |		|
	//   x|-----|x
	//     x y x
	const bool bAddFrontAndBackCornerPoints = bObjectWidthXQualifiesForCorners;
	const bool bAddLeftAndRightCornerPoints = bObjectWidthYQualifiesForCorners;
	const bool bAddFrontAndBackMidPoints = bObjectWidthXQualifiesForMidsOnly || bObjectWidthXAndHeightQualifiesForMids;
	const bool bAddLeftAndRightMidPoints = bObjectWidthYQualifiesForMidsOnly || bObjectWidthYAndHeightQualifiesForMids;

	// Compute offsets for this orientation
	LocalOffsetDataParams_General generalParams(VECTOR3_TO_VEC3V(pPhysical->GetBoundingBoxMin()), VECTOR3_TO_VEC3V(pPhysical->GetBoundingBoxMax()), ScalarV(fCoverToObject), ScalarV(DISTANCE_FROM_COVER_TO_OBJECT_CORNER));
	LocalOffsetData_General generalOffsets;
	if( !HelperComputeLocalOffsetData_General(m_eObjectOrientation, generalParams, generalOffsets) )
	{
		m_bActive = false;
		return;
	}

	// Right Front
	if( bAddFrontAndBackCornerPoints && !bSkipNorthFaceEast )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_NorthEast);
		fEffectiveCoverHeight = fObjectHeight;
		// force low corner
		if(eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerNorthFaceEast) 
		{
			fEffectiveCoverHeight = fCoverHeightLowCorner;
		}
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverNorth, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTORIGHT, CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Single Front
	if( bAddFrontAndBackMidPoints && !bSkipNorthFaceCenter )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_NorthCenter);
		CCoverPoint::eCoverUsage coverUsage = bAddFrontAndBackCornerPoints ? CCoverPoint::COVUSE_WALLTOBOTH : CCoverPoint::COVUSE_WALLTONEITHER;
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverNorth, fObjectHeight, coverUsage, bAddFrontAndBackCornerPoints ? CCoverPoint::COVARC_120 : CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Left Front
	if( bAddFrontAndBackCornerPoints && !bSkipNorthFaceWest )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_NorthWest);
		fEffectiveCoverHeight = fObjectHeight;
		// force low corner
		if(eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerNorthFaceWest)
		{
			fEffectiveCoverHeight = fCoverHeightLowCorner;
		}
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverNorth, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Left Front Left
	if( bAddLeftAndRightCornerPoints && !bSkipWestFaceNorth )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_WestNorth);
		fEffectiveCoverHeight = fObjectHeight;
		// force low corner
		if(eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerWestFaceNorth)
		{
			fEffectiveCoverHeight = fCoverHeightLowCorner;
		}
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverWest, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTORIGHT, CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Single Left
	if( bAddLeftAndRightMidPoints && !bSkipWestFaceCenter )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_WestCenter);
		CCoverPoint::eCoverUsage coverUsage = bAddLeftAndRightCornerPoints ? CCoverPoint::COVUSE_WALLTOBOTH : CCoverPoint::COVUSE_WALLTONEITHER;
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverWest, fObjectHeight, coverUsage, bAddLeftAndRightCornerPoints ? CCoverPoint::COVARC_120 : CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Left back Left
	if( bAddLeftAndRightCornerPoints && !bSkipWestFaceSouth )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_WestSouth);
		fEffectiveCoverHeight = fObjectHeight;
		// force low corner
		if(eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerWestFaceSouth)
		{
			fEffectiveCoverHeight = fCoverHeightLowCorner;
		}
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverWest, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Left Rear
	if( bAddFrontAndBackCornerPoints && !bSkipSouthFaceWest )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_SouthWest);
		fEffectiveCoverHeight = fObjectHeight;
		// force low corner
		if(eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerSouthFaceWest)
		{
			fEffectiveCoverHeight = fCoverHeightLowCorner;
		}
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverSouth, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTORIGHT, CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Single Rear
	if( bAddFrontAndBackMidPoints && !bSkipSouthFaceCenter )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_SouthCenter);
		CCoverPoint::eCoverUsage coverUsage = bAddFrontAndBackCornerPoints ? CCoverPoint::COVUSE_WALLTOBOTH : CCoverPoint::COVUSE_WALLTONEITHER;
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverSouth, fObjectHeight, coverUsage, bAddFrontAndBackCornerPoints ? CCoverPoint::COVARC_120 : CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Right Rear
	if( bAddFrontAndBackCornerPoints && !bSkipSouthFaceEast )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_SouthEast);
		fEffectiveCoverHeight = fObjectHeight;
		// force low corner
		if(eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerSouthFaceEast)
		{
			fEffectiveCoverHeight = fCoverHeightLowCorner;
		}
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverSouth, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Right Front Right
	if( bAddLeftAndRightCornerPoints && !bSkipEastFaceNorth )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_EastNorth);
		fEffectiveCoverHeight = fObjectHeight;
		// force low corner
		if(eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerEastFaceNorth)
		{
			fEffectiveCoverHeight = fCoverHeightLowCorner;
		}
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverEast, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTOLEFT, CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Single Right
	if( bAddLeftAndRightMidPoints && !bSkipEastFaceCenter )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_EastCenter);
		CCoverPoint::eCoverUsage coverUsage = bAddLeftAndRightCornerPoints ? CCoverPoint::COVUSE_WALLTOBOTH : CCoverPoint::COVUSE_WALLTONEITHER;
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverEast, fObjectHeight, coverUsage, bAddLeftAndRightCornerPoints ? CCoverPoint::COVARC_120 : CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// Right back Right
	if( bAddLeftAndRightCornerPoints && !bSkipEastFaceSouth )
	{
		vOffset = VEC3V_TO_VECTOR3(generalOffsets.m_EastSouth);
		fEffectiveCoverHeight = fObjectHeight;
		// force low corner
		if(eCoverHeight < CCoverPoint::COVHEIGHT_TOOHIGH || bForceLowCornerEastFaceSouth)
		{
			fEffectiveCoverHeight = fCoverHeightLowCorner;
		}
		m_aCoverPoints[m_iCoverPoints].init( vOffset, generalOffsets.m_fDirCoverEast, fEffectiveCoverHeight, CCoverPoint::COVUSE_WALLTORIGHT, CCoverPoint::COVARC_90 );
		m_iCoverPoints++;
	}

	// If we have custom tuning data and one coverpoint, mark the member flag
	if( bHasCustomCoverTuningData && m_iCoverPoints == 1 )
	{
		m_bForceTypeObject = true;
	}
}


//-------------------------------------------------------------------------
// Returns true if any point stored overlaps the given position
//-------------------------------------------------------------------------
bool CCachedObjectCover::DoesPositionOverlapAnyPoint( const Vector3& vLocalPos )
{
	for( s32 i = 0; i < m_iCoverPoints; i++ )
	{
		if( m_aCoverPoints[i].m_vLocalOffset.Dist2( vLocalPos ) < rage::square( 0.05f) )
		{
			return true;
		}
	}
	return false;
}


//-------------------------------------------------------------------------
// returns the corrected bounds
//-------------------------------------------------------------------------
void CCachedObjectCover::GetCorrectedBoundingBoxes( Vector3& vBoundingMin, Vector3& vBoundingMax )
{
	vBoundingMin = m_vBoundingMin;
	vBoundingMax = m_vBoundingMax;
}

//-------------------------------------------------------------------------
// Returns the next coverpoint local offset in the direction given
//-------------------------------------------------------------------------
void CCachedObjectCover::FindNextCoverPointOffsetInDirection( Vector3& vOut, const Vector3& vStart, const bool bClockwise )
{
	float fCurrentBest = -999.0f;
	vOut = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 vStartVec = vStart;
	vStartVec.z = 0.0f;
	vStartVec.Normalize();
	Vector3 vRight;
	vRight.Cross(vStartVec, Vector3(0.0f, 0.0f, 1.0f));

	for( s32 i = 0; i < m_iCoverPoints; i++ )
	{
		// Don't check the starting point
		if( vStart != m_aCoverPoints[i].m_vLocalOffset )
		{
			Vector3 vThisVec = m_aCoverPoints[i].m_vLocalOffset;
			vThisVec.z = 0.0f;
			vThisVec.Normalize();
			float fDot = vStartVec.Dot(vThisVec);
			float fRightDot = vRight.Dot(vThisVec);
			if( ( fRightDot > 0.0f && bClockwise ) ||
				 ( fRightDot < 0.0f && !bClockwise ) )
			{
				fDot -= 2.0f;
			}
			if( fDot > fCurrentBest )
			{
				fCurrentBest = fDot;
				vOut = m_aCoverPoints[i].m_vLocalOffset;
			}
		}
	}
}

//
// We need this helper class to stop certain compilers (PC build) trying to play connect the dots with the various permutations of 
// different vector additions.
//
// Doing so causes compile time to go through the roof in optimised builds.
//
namespace coverHelpersLocalOffsetData
{
	// General variants ///////////////////////////////////////////////////////////////////////////////////////////////////////

	void GeneralZUp( const CCachedObjectCover::LocalOffsetDataParams_General& params, CCachedObjectCover::LocalOffsetData_General& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthCenter.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_NorthWest.SetX(params.m_vBoundsMin.GetX() + params.m_CoverToObjectCornerPadding);
		out_data.m_NorthWest.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_NorthEast.SetX(params.m_vBoundsMax.GetX() - params.m_CoverToObjectCornerPadding);
		out_data.m_NorthEast.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthEast.SetZ(params.m_vBoundsMin.GetZ());
		// South face
		out_data.m_SouthCenter.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthCenter.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_SouthWest.SetX(params.m_vBoundsMin.GetX() + params.m_CoverToObjectCornerPadding);
		out_data.m_SouthWest.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_SouthEast.SetX(params.m_vBoundsMax.GetX() - params.m_CoverToObjectCornerPadding);
		out_data.m_SouthEast.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthEast.SetZ(params.m_vBoundsMin.GetZ());
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastCenter.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_EastNorth.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastNorth.SetY(params.m_vBoundsMax.GetY() - params.m_CoverToObjectCornerPadding);
		out_data.m_EastNorth.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_EastSouth.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastSouth.SetY(params.m_vBoundsMin.GetY() + params.m_CoverToObjectCornerPadding);
		out_data.m_EastSouth.SetZ(params.m_vBoundsMin.GetZ());
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestCenter.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_WestNorth.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestNorth.SetY(params.m_vBoundsMax.GetY() - params.m_CoverToObjectCornerPadding);
		out_data.m_WestNorth.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_WestSouth.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestSouth.SetY(params.m_vBoundsMin.GetY() + params.m_CoverToObjectCornerPadding);
		out_data.m_WestSouth.SetZ(params.m_vBoundsMin.GetZ());

		out_data.m_fDirCoverNorth = 180.0f;
		out_data.m_fDirCoverSouth = 0.0f;
		out_data.m_fDirCoverEast = 90.0f;
		out_data.m_fDirCoverWest = 270.0f;
	};

	void GeneralZDown( const CCachedObjectCover::LocalOffsetDataParams_General& params, CCachedObjectCover::LocalOffsetData_General& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthCenter.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_NorthWest.SetX(params.m_vBoundsMax.GetX() - params.m_CoverToObjectCornerPadding);
		out_data.m_NorthWest.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_NorthEast.SetX(params.m_vBoundsMin.GetX() + params.m_CoverToObjectCornerPadding);
		out_data.m_NorthEast.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthEast.SetZ(params.m_vBoundsMax.GetZ());
		// South face
		out_data.m_SouthCenter.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthCenter.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_SouthEast.SetX(params.m_vBoundsMin.GetX() + params.m_CoverToObjectCornerPadding);
		out_data.m_SouthEast.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthEast.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_SouthWest.SetX(params.m_vBoundsMax.GetX() - params.m_CoverToObjectCornerPadding);
		out_data.m_SouthWest.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetZ(params.m_vBoundsMax.GetZ());
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_EastCenter.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_EastNorth.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_EastNorth.SetY(params.m_vBoundsMax.GetY() - params.m_CoverToObjectCornerPadding);
		out_data.m_EastNorth.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_EastSouth.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_EastSouth.SetY(params.m_vBoundsMin.GetY() + params.m_CoverToObjectCornerPadding);
		out_data.m_EastSouth.SetZ(params.m_vBoundsMax.GetZ());
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_WestCenter.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_WestNorth.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_WestNorth.SetY(params.m_vBoundsMax.GetY() - params.m_CoverToObjectCornerPadding);
		out_data.m_WestNorth.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_WestSouth.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_WestSouth.SetY(params.m_vBoundsMin.GetY() + params.m_CoverToObjectCornerPadding);
		out_data.m_WestSouth.SetZ(params.m_vBoundsMax.GetZ());

		out_data.m_fDirCoverNorth = 180.0f;
		out_data.m_fDirCoverSouth = 0.0f;
		out_data.m_fDirCoverEast = 90.0f;
		out_data.m_fDirCoverWest = 270.0f;
	};
	void GeneralYUp( const CCachedObjectCover::LocalOffsetDataParams_General& params, CCachedObjectCover::LocalOffsetData_General& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetY(params.m_vBoundsMin.GetY());
		out_data.m_NorthCenter.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetX(params.m_vBoundsMin.GetX() + params.m_CoverToObjectCornerPadding);
		out_data.m_NorthWest.SetY(params.m_vBoundsMin.GetY());
		out_data.m_NorthWest.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		out_data.m_NorthEast.SetX(params.m_vBoundsMax.GetX() - params.m_CoverToObjectCornerPadding);
		out_data.m_NorthEast.SetY(params.m_vBoundsMin.GetY());
		out_data.m_NorthEast.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		// South face
		out_data.m_SouthCenter.SetY(params.m_vBoundsMin.GetY());
		out_data.m_SouthCenter.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetX(params.m_vBoundsMin.GetX() + params.m_CoverToObjectCornerPadding);
		out_data.m_SouthWest.SetY(params.m_vBoundsMin.GetY());
		out_data.m_SouthWest.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		out_data.m_SouthEast.SetX(params.m_vBoundsMax.GetX() - params.m_CoverToObjectCornerPadding);
		out_data.m_SouthEast.SetY(params.m_vBoundsMin.GetY());
		out_data.m_SouthEast.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestCenter.SetY(params.m_vBoundsMin.GetY());
		out_data.m_WestNorth.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestNorth.SetY(params.m_vBoundsMin.GetY());
		out_data.m_WestNorth.SetZ(params.m_vBoundsMin.GetZ() + params.m_CoverToObjectCornerPadding);
		out_data.m_WestSouth.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestSouth.SetY(params.m_vBoundsMin.GetY());
		out_data.m_WestSouth.SetZ(params.m_vBoundsMax.GetZ() - params.m_CoverToObjectCornerPadding);
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastCenter.SetY(params.m_vBoundsMin.GetY());
		out_data.m_EastNorth.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastNorth.SetY(params.m_vBoundsMin.GetY());
		out_data.m_EastNorth.SetZ(params.m_vBoundsMin.GetZ() + params.m_CoverToObjectCornerPadding);
		out_data.m_EastSouth.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastSouth.SetY(params.m_vBoundsMin.GetY());
		out_data.m_EastSouth.SetZ(params.m_vBoundsMax.GetZ() - params.m_CoverToObjectCornerPadding);

		out_data.m_fDirCoverNorth = 0.0f;
		out_data.m_fDirCoverSouth = 180.0f;
		out_data.m_fDirCoverEast = 270.0f;
		out_data.m_fDirCoverWest = 90.0f;
	};
	void GeneralYDown( const CCachedObjectCover::LocalOffsetDataParams_General& params, CCachedObjectCover::LocalOffsetData_General& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetY(params.m_vBoundsMax.GetY());
		out_data.m_NorthCenter.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetX(params.m_vBoundsMin.GetX() + params.m_CoverToObjectCornerPadding);
		out_data.m_NorthWest.SetY(params.m_vBoundsMax.GetY());
		out_data.m_NorthWest.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		out_data.m_NorthEast.SetX(params.m_vBoundsMax.GetX() - params.m_CoverToObjectCornerPadding);
		out_data.m_NorthEast.SetY(params.m_vBoundsMax.GetY());
		out_data.m_NorthEast.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		// South face
		out_data.m_SouthCenter.SetY(params.m_vBoundsMax.GetY());
		out_data.m_SouthCenter.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetX(params.m_vBoundsMin.GetX() + params.m_CoverToObjectCornerPadding);
		out_data.m_SouthWest.SetY(params.m_vBoundsMax.GetY());
		out_data.m_SouthWest.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		out_data.m_SouthEast.SetX(params.m_vBoundsMax.GetX() - params.m_CoverToObjectCornerPadding);
		out_data.m_SouthEast.SetY(params.m_vBoundsMax.GetY());
		out_data.m_SouthEast.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestCenter.SetY(params.m_vBoundsMax.GetY());
		out_data.m_WestNorth.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestNorth.SetY(params.m_vBoundsMax.GetY());
		out_data.m_WestNorth.SetZ(params.m_vBoundsMax.GetZ() - params.m_CoverToObjectCornerPadding);
		out_data.m_WestSouth.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestSouth.SetY(params.m_vBoundsMax.GetY());
		out_data.m_WestSouth.SetZ(params.m_vBoundsMin.GetZ() + params.m_CoverToObjectCornerPadding);
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastCenter.SetY(params.m_vBoundsMax.GetY());
		out_data.m_EastNorth.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastNorth.SetY(params.m_vBoundsMax.GetY());
		out_data.m_EastNorth.SetZ(params.m_vBoundsMax.GetZ() - params.m_CoverToObjectCornerPadding);
		out_data.m_EastSouth.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastSouth.SetY(params.m_vBoundsMax.GetY());
		out_data.m_EastSouth.SetZ(params.m_vBoundsMin.GetZ() + params.m_CoverToObjectCornerPadding);

		out_data.m_fDirCoverNorth = 180.0f;
		out_data.m_fDirCoverSouth = 0.0f;
		out_data.m_fDirCoverEast = 90.0f;
		out_data.m_fDirCoverWest = 270.0f;
	};
	void GeneralXUp( const CCachedObjectCover::LocalOffsetDataParams_General& params, CCachedObjectCover::LocalOffsetData_General& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetX(params.m_vBoundsMin.GetX());
		out_data.m_NorthCenter.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetX(params.m_vBoundsMin.GetX());
		out_data.m_NorthWest.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetZ(params.m_vBoundsMax.GetZ() - params.m_CoverToObjectCornerPadding);
		out_data.m_NorthEast.SetX(params.m_vBoundsMin.GetX());
		out_data.m_NorthEast.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthEast.SetZ(params.m_vBoundsMin.GetZ() + params.m_CoverToObjectCornerPadding);
		// South face
		out_data.m_SouthCenter.SetX(params.m_vBoundsMin.GetX());
		out_data.m_SouthCenter.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetX(params.m_vBoundsMin.GetX());
		out_data.m_SouthWest.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetZ(params.m_vBoundsMax.GetZ() - params.m_CoverToObjectCornerPadding);
		out_data.m_SouthEast.SetX(params.m_vBoundsMin.GetX());
		out_data.m_SouthEast.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthEast.SetZ(params.m_vBoundsMin.GetZ() + params.m_CoverToObjectCornerPadding);
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMin.GetX());
		out_data.m_WestCenter.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		out_data.m_WestNorth.SetX(params.m_vBoundsMin.GetX());
		out_data.m_WestNorth.SetY(params.m_vBoundsMax.GetY() - params.m_CoverToObjectCornerPadding);
		out_data.m_WestNorth.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		out_data.m_WestSouth.SetX(params.m_vBoundsMin.GetX());
		out_data.m_WestSouth.SetY(params.m_vBoundsMin.GetY() + params.m_CoverToObjectCornerPadding);
		out_data.m_WestSouth.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMin.GetX());
		out_data.m_EastCenter.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		out_data.m_EastNorth.SetX(params.m_vBoundsMin.GetX());
		out_data.m_EastNorth.SetY(params.m_vBoundsMax.GetY() - params.m_CoverToObjectCornerPadding);
		out_data.m_EastNorth.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		out_data.m_EastSouth.SetX(params.m_vBoundsMin.GetX());
		out_data.m_EastSouth.SetY(params.m_vBoundsMin.GetY() + params.m_CoverToObjectCornerPadding);
		out_data.m_EastSouth.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);

		out_data.m_fDirCoverNorth = 90.0f;
		out_data.m_fDirCoverSouth = 270.0f;
		out_data.m_fDirCoverEast = 0.0f;
		out_data.m_fDirCoverWest = 180.0f;
	};
	void GeneralXDown( const CCachedObjectCover::LocalOffsetDataParams_General& params, CCachedObjectCover::LocalOffsetData_General& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetX(params.m_vBoundsMax.GetX());
		out_data.m_NorthCenter.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetX(params.m_vBoundsMax.GetX());
		out_data.m_NorthWest.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetZ(params.m_vBoundsMin.GetZ() + params.m_CoverToObjectCornerPadding);
		out_data.m_NorthEast.SetX(params.m_vBoundsMax.GetX());
		out_data.m_NorthEast.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthEast.SetZ(params.m_vBoundsMax.GetZ() - params.m_CoverToObjectCornerPadding);
		// South face
		out_data.m_SouthCenter.SetX(params.m_vBoundsMax.GetX());
		out_data.m_SouthCenter.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetX(params.m_vBoundsMax.GetX());
		out_data.m_SouthWest.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetZ(params.m_vBoundsMin.GetZ() + params.m_CoverToObjectCornerPadding);
		out_data.m_SouthEast.SetX(params.m_vBoundsMax.GetX());
		out_data.m_SouthEast.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthEast.SetZ(params.m_vBoundsMax.GetZ() - params.m_CoverToObjectCornerPadding);
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMax.GetX());
		out_data.m_WestCenter.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		out_data.m_WestNorth.SetX(params.m_vBoundsMax.GetX());
		out_data.m_WestNorth.SetY(params.m_vBoundsMax.GetY() - params.m_CoverToObjectCornerPadding);
		out_data.m_WestNorth.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		out_data.m_WestSouth.SetX(params.m_vBoundsMax.GetX());
		out_data.m_WestSouth.SetY(params.m_vBoundsMin.GetY() + params.m_CoverToObjectCornerPadding);
		out_data.m_WestSouth.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMax.GetX());
		out_data.m_EastCenter.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		out_data.m_EastNorth.SetX(params.m_vBoundsMax.GetX());
		out_data.m_EastNorth.SetY(params.m_vBoundsMax.GetY() - params.m_CoverToObjectCornerPadding);
		out_data.m_EastNorth.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		out_data.m_EastSouth.SetX(params.m_vBoundsMax.GetX());
		out_data.m_EastSouth.SetY(params.m_vBoundsMin.GetY() + params.m_CoverToObjectCornerPadding);
		out_data.m_EastSouth.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);

		out_data.m_fDirCoverNorth = 270.0f;
		out_data.m_fDirCoverSouth = 90.0f;
		out_data.m_fDirCoverEast = 180.0f;
		out_data.m_fDirCoverWest = 0.0f;
	};

	// Small data /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void SmallZUp( const CCachedObjectCover::LocalOffsetDataParams_Small& params, CCachedObjectCover::LocalOffsetData_Small& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthCenter.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_NorthWest.SetX(params.m_vBoundsMin.GetX() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthWest.SetY(params.m_vBoundsMax.GetY() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthWest.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_NorthEast.SetX(params.m_vBoundsMax.GetX() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetY(params.m_vBoundsMax.GetY() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetZ(params.m_vBoundsMin.GetZ());
		// South face
		out_data.m_SouthCenter.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthCenter.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_SouthWest.SetX(params.m_vBoundsMin.GetX() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthWest.SetY(params.m_vBoundsMin.GetY() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthWest.SetZ(params.m_vBoundsMin.GetZ());
		out_data.m_SouthEast.SetX(params.m_vBoundsMax.GetX() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetY(params.m_vBoundsMin.GetY() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetZ(params.m_vBoundsMin.GetZ());
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastCenter.SetZ(params.m_vBoundsMin.GetZ());
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestCenter.SetZ(params.m_vBoundsMin.GetZ());

		out_data.m_fDirCoverNorth = 180.0f;
		out_data.m_fDirCoverSouth = 0.0f;
		out_data.m_fDirCoverEast = 90.0f;
		out_data.m_fDirCoverWest = 270.0f;

		out_data.m_fDirCoverNorthWest = 225.0f;
		out_data.m_fDirCoverNorthEast = 135.0f;
		out_data.m_fDirCoverSouthWest = 315.0f;
		out_data.m_fDirCoverSouthEast = 45.0f;
	};
	void SmallZDown( const CCachedObjectCover::LocalOffsetDataParams_Small& params, CCachedObjectCover::LocalOffsetData_Small& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthCenter.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_NorthWest.SetX(params.m_vBoundsMax.GetX() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthWest.SetY(params.m_vBoundsMax.GetY() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthWest.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_NorthEast.SetX(params.m_vBoundsMin.GetX() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetY(params.m_vBoundsMax.GetY() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetZ(params.m_vBoundsMax.GetZ());
		// South face
		out_data.m_SouthCenter.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthCenter.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_SouthEast.SetX(params.m_vBoundsMin.GetX() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetY(params.m_vBoundsMin.GetY() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetZ(params.m_vBoundsMax.GetZ());
		out_data.m_SouthWest.SetX(params.m_vBoundsMax.GetX() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthWest.SetY(params.m_vBoundsMin.GetY() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthWest.SetZ(params.m_vBoundsMax.GetZ());
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_EastCenter.SetZ(params.m_vBoundsMax.GetZ());
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_WestCenter.SetZ(params.m_vBoundsMax.GetZ());

		out_data.m_fDirCoverNorth = 180.0f;
		out_data.m_fDirCoverSouth = 0.0f;
		out_data.m_fDirCoverEast = 90.0f;
		out_data.m_fDirCoverWest = 270.0f;

		out_data.m_fDirCoverNorthWest = 225.0f;
		out_data.m_fDirCoverNorthEast = 135.0f;
		out_data.m_fDirCoverSouthWest = 315.0f;
		out_data.m_fDirCoverSouthEast = 45.0f;
	};

	void SmallYUp( const CCachedObjectCover::LocalOffsetDataParams_Small& params, CCachedObjectCover::LocalOffsetData_Small& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetY(params.m_vBoundsMin.GetY());
		out_data.m_NorthCenter.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetX(params.m_vBoundsMin.GetX() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthWest.SetY(params.m_vBoundsMin.GetY());
		out_data.m_NorthWest.SetZ(params.m_vBoundsMin.GetZ() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetX(params.m_vBoundsMax.GetX() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetY(params.m_vBoundsMin.GetY());
		out_data.m_NorthEast.SetZ(params.m_vBoundsMin.GetZ() - params.m_fCoverToSmallObjectDiagonal);
		// South face
		out_data.m_SouthCenter.SetY(params.m_vBoundsMin.GetY());
		out_data.m_SouthCenter.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetX(params.m_vBoundsMin.GetX() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthWest.SetY(params.m_vBoundsMin.GetY());
		out_data.m_SouthWest.SetZ(params.m_vBoundsMax.GetZ() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetX(params.m_vBoundsMax.GetX() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetY(params.m_vBoundsMin.GetY());
		out_data.m_SouthEast.SetZ(params.m_vBoundsMax.GetZ() + params.m_fCoverToSmallObjectDiagonal);
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestCenter.SetY(params.m_vBoundsMin.GetY());
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastCenter.SetY(params.m_vBoundsMin.GetY());

		out_data.m_fDirCoverNorth = 0.0f;
		out_data.m_fDirCoverSouth = 180.0f;
		out_data.m_fDirCoverEast = 270.0f;
		out_data.m_fDirCoverWest = 90.0f;

		out_data.m_fDirCoverNorthWest = 45.0f;
		out_data.m_fDirCoverNorthEast = 315.0f;
		out_data.m_fDirCoverSouthWest = 135.0f;
		out_data.m_fDirCoverSouthEast = 225.0f;
	};

	void SmallYDown( const CCachedObjectCover::LocalOffsetDataParams_Small& params, CCachedObjectCover::LocalOffsetData_Small& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetY(params.m_vBoundsMax.GetY());
		out_data.m_NorthCenter.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetX(params.m_vBoundsMin.GetX() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthWest.SetY(params.m_vBoundsMax.GetY());
		out_data.m_NorthWest.SetZ(params.m_vBoundsMax.GetZ() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetX(params.m_vBoundsMax.GetX() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetY(params.m_vBoundsMax.GetY());
		out_data.m_NorthEast.SetZ(params.m_vBoundsMax.GetZ() + params.m_fCoverToSmallObjectDiagonal);
		// South face
		out_data.m_SouthCenter.SetY(params.m_vBoundsMax.GetY());
		out_data.m_SouthCenter.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetX(params.m_vBoundsMin.GetX() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthWest.SetY(params.m_vBoundsMax.GetY());
		out_data.m_SouthWest.SetZ(params.m_vBoundsMin.GetZ() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetX(params.m_vBoundsMax.GetX() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetY(params.m_vBoundsMax.GetY());
		out_data.m_SouthEast.SetZ(params.m_vBoundsMin.GetZ() - params.m_fCoverToSmallObjectDiagonal);
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMin.GetX() - params.m_CoverToObjectPadding);
		out_data.m_WestCenter.SetY(params.m_vBoundsMax.GetY());
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMax.GetX() + params.m_CoverToObjectPadding);
		out_data.m_EastCenter.SetY(params.m_vBoundsMax.GetY());

		out_data.m_fDirCoverNorth = 180.0f;
		out_data.m_fDirCoverSouth = 0.0f;
		out_data.m_fDirCoverEast = 90.0f;
		out_data.m_fDirCoverWest = 270.0f;

		out_data.m_fDirCoverNorthWest = 225.0f;
		out_data.m_fDirCoverNorthEast = 135.0f;
		out_data.m_fDirCoverSouthWest = 315.0f;
		out_data.m_fDirCoverSouthEast = 45.0f;
	};
	void SmallXUp( const CCachedObjectCover::LocalOffsetDataParams_Small& params, CCachedObjectCover::LocalOffsetData_Small& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetX(params.m_vBoundsMin.GetX());
		out_data.m_NorthCenter.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetX(params.m_vBoundsMin.GetX());
		out_data.m_NorthWest.SetY(params.m_vBoundsMax.GetY() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthWest.SetZ(params.m_vBoundsMax.GetZ() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetX(params.m_vBoundsMin.GetX());
		out_data.m_NorthEast.SetY(params.m_vBoundsMax.GetY() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetZ(params.m_vBoundsMin.GetZ() - params.m_fCoverToSmallObjectDiagonal);
		// South face
		out_data.m_SouthCenter.SetX(params.m_vBoundsMin.GetX());
		out_data.m_SouthCenter.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetX(params.m_vBoundsMin.GetX());
		out_data.m_SouthWest.SetY(params.m_vBoundsMin.GetY() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthWest.SetZ(params.m_vBoundsMax.GetZ() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetX(params.m_vBoundsMin.GetX());
		out_data.m_SouthEast.SetY(params.m_vBoundsMin.GetY() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetZ(params.m_vBoundsMin.GetZ() - params.m_fCoverToSmallObjectDiagonal);
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMin.GetX());
		out_data.m_WestCenter.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMin.GetX());
		out_data.m_EastCenter.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);

		out_data.m_fDirCoverNorth = 90.0f;
		out_data.m_fDirCoverSouth = 270.0f;
		out_data.m_fDirCoverEast = 0.0f;
		out_data.m_fDirCoverWest = 180.0f;

		out_data.m_fDirCoverNorthWest = 135.0f;
		out_data.m_fDirCoverNorthEast = 45.0f;
		out_data.m_fDirCoverSouthWest = 225.0f;
		out_data.m_fDirCoverSouthEast = 315.0f;
	};
	void SmallXDown( const CCachedObjectCover::LocalOffsetDataParams_Small& params, CCachedObjectCover::LocalOffsetData_Small& out_data)
	{
		// North face
		out_data.m_NorthCenter.SetX(params.m_vBoundsMax.GetX());
		out_data.m_NorthCenter.SetY(params.m_vBoundsMax.GetY() + params.m_CoverToObjectPadding);
		out_data.m_NorthWest.SetX(params.m_vBoundsMax.GetX());
		out_data.m_NorthWest.SetY(params.m_vBoundsMax.GetY() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthWest.SetZ(params.m_vBoundsMin.GetZ() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetX(params.m_vBoundsMax.GetX());
		out_data.m_NorthEast.SetY(params.m_vBoundsMax.GetY() + params.m_fCoverToSmallObjectDiagonal);
		out_data.m_NorthEast.SetZ(params.m_vBoundsMax.GetZ() + params.m_fCoverToSmallObjectDiagonal);
		// South face
		out_data.m_SouthCenter.SetX(params.m_vBoundsMax.GetX());
		out_data.m_SouthCenter.SetY(params.m_vBoundsMin.GetY() - params.m_CoverToObjectPadding);
		out_data.m_SouthWest.SetX(params.m_vBoundsMax.GetX());
		out_data.m_SouthWest.SetY(params.m_vBoundsMin.GetY() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthWest.SetZ(params.m_vBoundsMin.GetZ() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetX(params.m_vBoundsMax.GetX());
		out_data.m_SouthEast.SetY(params.m_vBoundsMin.GetY() - params.m_fCoverToSmallObjectDiagonal);
		out_data.m_SouthEast.SetZ(params.m_vBoundsMax.GetZ() + params.m_fCoverToSmallObjectDiagonal);
		// West face
		out_data.m_WestCenter.SetX(params.m_vBoundsMax.GetX());
		out_data.m_WestCenter.SetZ(params.m_vBoundsMin.GetZ() - params.m_CoverToObjectPadding);
		// East face
		out_data.m_EastCenter.SetX(params.m_vBoundsMax.GetX());
		out_data.m_EastCenter.SetZ(params.m_vBoundsMax.GetZ() + params.m_CoverToObjectPadding);

		out_data.m_fDirCoverNorth = 270.0f;
		out_data.m_fDirCoverSouth = 90.0f;
		out_data.m_fDirCoverEast = 180.0f;
		out_data.m_fDirCoverWest = 0.0f;

		out_data.m_fDirCoverNorthWest = 315.0f;
		out_data.m_fDirCoverNorthEast = 225.0f;
		out_data.m_fDirCoverSouthWest = 45.0f;
		out_data.m_fDirCoverSouthEast = 135.0f;
	};

};

bool CCachedObjectCover::HelperComputeLocalOffsetData_Small( const CoverOrientation& orientation, const LocalOffsetDataParams_Small& params, LocalOffsetData_Small& out_data )
{
	// Check for unusable orientations
	if( orientation == CO_Invalid || orientation == CO_Dirty )
	{
		Assert(orientation != CO_Invalid);
		Assert(orientation != CO_Dirty);
		return false;
	}

	// Then make specific changes as needed to reflect non-standard orientations
	// NOTES: Internal +Z is top, +Y is front, +X is right
	//        vBoundsMin values are negative and vBoundsMax are positive
	switch(orientation)
	{
		case CO_Invalid:
		case CO_Dirty:
			// do nothing -- these should be caught at top of this method with asserts
			return false;

		case CO_ZUp: // top is up (internal +Z is world +Z)
			coverHelpersLocalOffsetData::SmallZUp(params, out_data);
			return true;

		case CO_ZDown: // bottom is up (internal -Z is world +Z)
			coverHelpersLocalOffsetData::SmallZDown(params, out_data);
			return true;

		case CO_YUp: // front is up (internal +Y is world +Z)
			coverHelpersLocalOffsetData::SmallYUp(params, out_data);
			return true;
		
		case CO_YDown: // rear is up (internal -Y is world +Z)
			coverHelpersLocalOffsetData::SmallYDown(params, out_data);
			return true;
		
		case CO_XUp: // right side is up (internal +X is world +Z)
			coverHelpersLocalOffsetData::SmallXUp(params, out_data);
			return true;

		case CO_XDown: // left side is up (internal -X is world +Z)
			coverHelpersLocalOffsetData::SmallXDown(params, out_data);
			return true;
		
		default:
			Assertf(false, "Unsupported orientation value %d, ignored!", (int)orientation);
			break;
	}

	// report valid data not set
	return false;
}


bool CCachedObjectCover::HelperComputeLocalOffsetData_General( const CoverOrientation& orientation, const LocalOffsetDataParams_General& params, LocalOffsetData_General& out_data )
{
	// Check for unusable orientations
	if( orientation == CO_Invalid || orientation == CO_Dirty )
	{
		Assert(orientation != CO_Invalid);
		Assert(orientation != CO_Dirty);
		return false;
	}

	// Set internal offsets according to orientation
	// NOTES: vBounds dimensions are internal where +Z is top, +Y is front, +X is right
	//        vBoundsMin values are negative, and vBoundsMax are positive
	switch(orientation)
	{
		case CO_Invalid:
		case CO_Dirty:
			// do nothing -- these should be caught at top of this method with asserts
			return false;
		
		case CO_ZUp: // top is up (internal +Z is world +Z)
			coverHelpersLocalOffsetData::GeneralZUp(params, out_data);
			return true;

		case CO_ZDown: // bottom is up (internal -Z is world +Z)
			coverHelpersLocalOffsetData::GeneralZDown(params, out_data);
			return true;

		case CO_YUp: // front is up (internal +Y is world +Z)
			coverHelpersLocalOffsetData::GeneralYUp(params, out_data);
			return true;

		case CO_YDown: // rear is up (internal -Y is world +Z)
			coverHelpersLocalOffsetData::GeneralYDown(params, out_data);
			return true;

		case CO_XUp: // right side is up (internal +X is world +Z)
			coverHelpersLocalOffsetData::GeneralXUp(params, out_data);
			return true;

		case CO_XDown: // left side is up (internal -X is world +Z)
			coverHelpersLocalOffsetData::GeneralXDown(params, out_data);
			return true;

		default:
			Assertf(false, "Unsupported orientation value %d, ignored!", (int)orientation);
			break;
	}

	// report valid data not set
	return false;
}

#if __BANK
void CCachedObjectCover::DebugReset()
{
	m_iCoverPoints = 0;
	m_iModelHashKey = 0;
	m_bActive = false;
	m_bForceTypeObject = false;
	m_vBoundingMin = Vector3::ZeroType;
	m_vBoundingMax = Vector3::ZeroType;
}
#endif // __BANK

static bool DEBUG_FORCE_NO_CACHE_USE = false; // For debugging cover creation.

//-------------------------------------------------------------------------
// Adds cover points for the given object, from cache if possible
// Returns true if completely successful
//-------------------------------------------------------------------------
bool CCachedObjectCoverManager::AddCoverpointsForPhysical( CPhysical* pPhysical )
{
	CBaseModelInfo* pModelInfo = pPhysical->GetBaseModelInfo();

	// Ignore objects without physics instances
	if(!pPhysical->GetIsPhysical() || !pPhysical->GetCurrentPhysicsInst())
	{
		return false;
	}

	// IF the object is broken, we can't cache the status, so need to recalculate each time
	if( pPhysical->GetFragInst() )
	{
		fragInst* pFragInst = pPhysical->GetFragInst();
		if( pFragInst->GetPartBroken() )
		{
			CCachedObjectCover fakeObjectCache;
			fakeObjectCache.Setup(pPhysical);
			const bool bSuccessful = fakeObjectCache.AddCoverPoints(pPhysical);

			// Remove any old, unrelated coverpoints
			s16 iPoolStart = 0;
			s16 iPoolEnd = 0;
			CCover::CalculateCoverPoolExtents( CCover::CP_NavmeshAndDynamicPoints, iPoolStart, iPoolEnd );

			for( s32 i = iPoolStart; i < iPoolEnd; i++ )
			{
				if( CCover::m_aPoints[i].GetType() != CCoverPoint::COVTYPE_NONE &&
					CCover::m_aPoints[i].GetType() != CCoverPoint::COVTYPE_POINTONMAP &&
					CCover::m_aPoints[i].m_pEntity == (CEntity*) pPhysical )
				{
					Vector3 vLocalPos;
					CCover::m_aPoints[i].GetLocalPosition( vLocalPos	);
					if( !fakeObjectCache.DoesPositionOverlapAnyPoint( vLocalPos ) )
					{
						CCover::RemoveCoverPointWithIndex(i);
					}
				}
			}

			return bSuccessful;
		}
	}

	// Try to find a cached version with the cover points status
	CCachedObjectCover* pCachedObjectCover = FindObjectInCache( pModelInfo->GetHashKey(), GetUpAxis( pPhysical ) );

	// If no cached version available, add a new cache entry
	if( DEBUG_FORCE_NO_CACHE_USE || !pCachedObjectCover )
	{
		m_aCachedCoverPoints[m_iNextFreeCachedObject].Setup(pPhysical);
		pCachedObjectCover = &m_aCachedCoverPoints[m_iNextFreeCachedObject];
		++m_iNextFreeCachedObject;
		if( m_iNextFreeCachedObject >= MAX_CACHED_OBJECT_COVER_POINTS )
		{
			m_iNextFreeCachedObject = 0;
		}
	}

	Assert( pCachedObjectCover );

	return pCachedObjectCover->AddCoverPoints( pPhysical );
}

//-------------------------------------------------------------------------
// returns the corrected bounds for the object passed
//-------------------------------------------------------------------------
bool CCachedObjectCoverManager::GetCorrectedBoundsForObject( CObject* pObject, Vector3& vBoundMin, Vector3& vBoundMax )
{
	// Try to find a cached version with the cover points status
	CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo();
	CCachedObjectCover* pCachedObjectCover = FindObjectInCache( pModelInfo->GetHashKey(), GetUpAxis(pObject) );

	if( !pCachedObjectCover )
	{
		return false;
	}

	pCachedObjectCover->GetCorrectedBoundingBoxes(vBoundMin, vBoundMax);

	return true;
}

//-------------------------------------------------------------------------
// Returns the next coverpoint local offset in the direction given
//-------------------------------------------------------------------------
bool CCachedObjectCoverManager::FindNextCoverPointOffsetInDirection( CObject* pObject, Vector3& vOut, const Vector3& vStart, const bool bClockwise )
{
	// Try to find a cached version with the cover points status
	CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo();
	CCachedObjectCover* pCachedObjectCover = FindObjectInCache( pModelInfo->GetHashKey(), GetUpAxis( pObject ) );

	if( !pCachedObjectCover )
	{
		return false;
	}

	pCachedObjectCover->FindNextCoverPointOffsetInDirection( vOut, vStart, bClockwise );

	return true;
}

//-------------------------------------------------------------------------
// Returns the objects cached cover if its in cache
//-------------------------------------------------------------------------
CCachedObjectCover* CCachedObjectCoverManager::FindObjectInCache( const u32 iModelHashKey, const CoverOrientation eOrientation )
{
	for( s32 i = 0; i < MAX_CACHED_OBJECT_COVER_POINTS; i++ )
	{
		if( m_aCachedCoverPoints[i].IsActive() )
		{
			if( m_aCachedCoverPoints[i].GetModelHashKey() == iModelHashKey &&
				 m_aCachedCoverPoints[i].GetOrientation() == eOrientation )
			{
				return &m_aCachedCoverPoints[i];
			}
		}
	}
	return NULL;
}


//-------------------------------------------------------------------------
// Add an axis alligned are that blocks the creation of coverpoints
//-------------------------------------------------------------------------
void CCover::AddCoverBlockingArea( const Vector3& vStart, const Vector3& vEnd, const bool bBlockObjects, const bool bBlockVehicles, const bool bBlockMap, const bool bBlockPlayer )
{
	if( ms_blockingBoundingAreas.GetCount() == MAX_BOUNDING_AREAS )
	{
		Assertf( 0, "Ran out of cover blocking areas!" );
		return;
	}
	Vector3 vMin(MIN(vStart.x, vEnd.x), MIN(vStart.y, vEnd.y),MIN(vStart.z, vEnd.z) );
	Vector3 vMax(MAX(vStart.x, vEnd.x), MAX(vStart.y, vEnd.y),MAX(vStart.z, vEnd.z) );
	
	CCoverBlockingBoundingArea &blockingArea = ms_blockingBoundingAreas.Append();
	blockingArea.initFromMinMax( vMin, vMax );
	blockingArea.SetBlocksObjects(bBlockObjects);
	blockingArea.SetBlocksVehicles(bBlockVehicles);
	blockingArea.SetBlocksMap(bBlockMap);
	blockingArea.SetBlocksPlayer(bBlockPlayer);
}

void CCover::RemoveCoverBlockingAreasAtPosition( const Vector3& vPosition )
{
	// Loop over blocking areas and remove any at the specified position (start at the end of the array so count value doesn't mess up).
	for (int i = (ms_blockingBoundingAreas.GetCount() - 1); i >= 0; i--)
	{
		if (ms_blockingBoundingAreas[i].ContainsPoint(vPosition))
		{
			ms_blockingBoundingAreas.Delete(i);
		}
	}
}

void CCover::RemoveSpecificCoverBlockingArea( const Vector3& vStart, const Vector3& vEnd, const bool bBlockObjects, const bool bBlockVehicles, const bool bBlockMap, const bool bBlockPlayer )
{
	// Loop over blocking areas and remove any with the specified params (start at the end of the array so count value doesn't mess up).
	for (int i = (ms_blockingBoundingAreas.GetCount() - 1); i >= 0; i--)
	{
		Vector3 vMin(MIN(vStart.x, vEnd.x), MIN(vStart.y, vEnd.y),MIN(vStart.z, vEnd.z) );
		Vector3 vMax(MAX(vStart.x, vEnd.x), MAX(vStart.y, vEnd.y),MAX(vStart.z, vEnd.z) );
		Vector3 vBlockingAreaMin, vBlockingAreaMax;
		ms_blockingBoundingAreas[i].GetMin(vBlockingAreaMin);
		ms_blockingBoundingAreas[i].GetMax(vBlockingAreaMax);

		if ( vBlockingAreaMin == vMin &&
			 vBlockingAreaMax == vMax &&
			 ms_blockingBoundingAreas[i].GetBlocksObjects() == bBlockObjects &&
			 ms_blockingBoundingAreas[i].GetBlocksVehicles() == bBlockVehicles &&
			 ms_blockingBoundingAreas[i].GetBlocksMap() == bBlockMap &&
			 ms_blockingBoundingAreas[i].GetBlocksPlayer() == bBlockPlayer )
		{
			ms_blockingBoundingAreas.Delete(i);
		}
	}
}

void CCover::AddHardCodedCoverBlockingArea( const Vector3& vStart, const Vector3& vEnd )
{
	if( ms_iNumberHardCodedBlockingAreas == MAX_HARD_CODED_AREAS )
	{
		Assertf( 0, "Ran out of hard coded cover blocking areas, increase MAX_HARD_CODED_AREAS!" );
		return;
	}
	Vector3 vMin(MIN(vStart.x, vEnd.x), MIN(vStart.y, vEnd.y),MIN(vStart.z, vEnd.z) );
	Vector3 vMax(MAX(vStart.x, vEnd.x), MAX(vStart.y, vEnd.y),MAX(vStart.z, vEnd.z) );

	ms_hardCodedBlockingBoundingAreas[ms_iNumberHardCodedBlockingAreas].initFromMinMax( vMin, vMax );
	++ms_iNumberHardCodedBlockingAreas;
}

Vec3V_Out CCachedObjectCoverManager::RotateLocalZToWorld(Vec3V_In localPos, const CPhysical &physical)
{
	// Get a reference to the object's matrix. CPhysical should contain a stored copy these days.
	Mat34V_ConstRef mtrx = physical.GetMatrixRef();

	// Load the 3x3 part of the matrix into vector registers.
	const Vec3V mtrxAV = mtrx.GetCol0();
	const Vec3V mtrxBV = mtrx.GetCol1();
	const Vec3V mtrxCV = mtrx.GetCol2();

	// Load some constants we'll need.
	const ScalarV zUpForObjectToBeUprightV = LoadScalar32IntoScalarV(ZUPFOROBJECTTOBEUPRIGHT);
	const ScalarV zeroV(V_ZERO);

	// Get the Z components of the three axes into one vector, then get the absolute value of,
	// each component, then compare those against the threshold.
	const Vec3V mtrxABCZV(mtrxAV.GetZ(), mtrxBV.GetZ(), mtrxCV.GetZ());
	const Vec3V absMtrxABCZV = Abs(mtrxABCZV);
	const VecBoolV isXYZUpRawV = IsGreaterThan(absMtrxABCZV, Vec3V(zUpForObjectToBeUprightV));

	// We will do something different if the X or Y axis is up than if Z (or none?)
	// is up. GetAxis() would give the Z axis priority, so we compute a vector bool here which
	// is true if either the value in the X or Y axis was large enough, and the Z value was not.
	// Note: if ZUPFOROBJECTTOBEUPRIGHT is large enough, this is probably not necessary - no more than
	// one axis could be up. May as well handle it though, shouldn't be much difference in cost.
	const BoolV isXOrYUp2V = And(InvertBits(isXYZUpRawV.GetZ()), Or(isXYZUpRawV.GetX(), isXYZUpRawV.GetY()));

	// Now, we compute the arguments we would have passed in to atan2(), if the X or Y axis is up.
	// We use a Vec2V to store this.
	const Vec2V atanXYIfXOrYUpV(mtrxCV.GetY(), Negate(mtrxCV.GetX()));

	// Next, compute the atan2() arguments if the Z axis had been up - or down, in which case
	// we compute the angle differently. The result goes in a Vec2V.
	const ScalarV atanYArgIfZUpV = mtrxAV.GetY();
	const ScalarV atanYArgIfZDownV = Negate(mtrxAV.GetY());
	const BoolV invertedB = IsLessThanOrEqual(mtrxCV.GetZ(), zeroV);
	const ScalarV atanYArgIfZUpOrDownV = SelectFT(invertedB, atanYArgIfZUpV, atanYArgIfZDownV);
	const Vec2V atanXYIfZUpOrDownV(mtrxBV.GetY(), atanYArgIfZUpOrDownV);

	// Select the right arguments we would have passed in to atan2(), depending on which axis is up, etc.
	const Vec2V atanXYV = SelectFT(isXOrYUp2V, atanXYIfZUpOrDownV, atanXYIfXOrYUpV);

	// The old code would now do an atan2(), and then pass the result into RotateZ(). This
	// would require us to compute the sine and cosine of the angle again. That is however
	// the same as just normalizing the vector we've got, which will be much cheaper despite
	// the square root and division.
	const Vec2V tCosSinV = Normalize(atanXYV);

	// Now, we will essentially do what RotateZ() would have done, basically compute a rotation like this:
	//	inout_vLocalPos.x = localPos.x*tcos - localPos.y*tsin
	//	inout_vLocalPos.y = localPos.x*tsin + localPos.y*tcos
	// We can do this by building two 4D vectors, multiply them, and shift and add.
	const Vec4V localPosFactorsV(localPos.GetX(), -localPos.GetY(), localPos.GetX(), localPos.GetY());
	const Vec4V rotFactorsV(tCosSinV.GetX(), tCosSinV.GetY(), tCosSinV.GetY(), tCosSinV.GetX());
	const Vec4V rotTermsV = Scale(localPosFactorsV, rotFactorsV);
	const Vec4V shiftedRotTermsV = GetFromTwo<Vec::Y1, Vec::Z1, Vec::W1, Vec::X2>(rotTermsV, rotTermsV);
	const Vec4V addedTermsV = Add(rotTermsV, shiftedRotTermsV);

	// Finally, assemble the result and return it.
	const Vec3V retV(addedTermsV.GetX(), addedTermsV.GetZ(), localPos.GetZ());
	return retV;
}
