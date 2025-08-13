
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    Cover.h
// PURPOSE : Everything having to do with the taking cover and finding places to do it.
// AUTHOR :  Obbe
// CREATED : 19-2-04
//
//
//
//
//
//
/////////////////////////////////////////////////////////////////////////////////


#ifndef COVER_H
#define COVER_H

// Rage headers
#include "vector/vector3.h"
#include "system/performancetimer.h"

// Framework headers
#include "ai/aichannel.h"
#include "fwutil/PtrList.h"
#include "fwutil/flags.h"

// Game headers
#include "AI/AITarget.h"
#include "Scene/Entity.h"
#include "Scene/Physical.h"
#include "Scene/RegdRefTypes.h"
#include "task/Combat/Cover/coversupport.h"

//forward decs
class CCoverPointFilter;
class CPed;
class CObject;
class CVehicle;
class CCoverPointsGridCell;

//************************************************
// A cover point can be either:
// An object 	(bin, streetlight)
// A vehicle	(not moving)
// A point on the map
//************************************************

#define INVALID_COVER_POINT_INDEX (-1)
#define MAX_PEDS_PER_COVER_POINT			(1)
#define COVER_POINT_ANY_DIR					255

#define COVER_LEFT_FIRE_MODIFIER 1.0f
#define COVER_RIGHT_FIRE_MODIFIER 1.0f

static const float COVER_POINT_DIR_RANGE	= (float)COVER_POINT_ANY_DIR;

static const float LOW_COVER_MAX_HEIGHT = 1.2f;
static const float HIGH_COVER_MAX_HEIGHT = 1.4f;
static const float HIGH_COVER_MAX_HEIGHT_FOR_VEHICLES = 1.45f;

// define minimum door ratio to consider door as "open"
static const float MIN_RATIO_FOR_OPEN_DOOR_COVER = 0.75f;

static const float MAX_COVER_SIDEWAYS_REACH = 1.0f;
//#define MIN_STICK_INPUT_TO_MOVE_IN_COVER 25.0f
// 25 in range 127 is approx 0.1969
#define MIN_STICK_INPUT_TO_MOVE_IN_COVER 0.1969f

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Acts as a wrapper to cover points created in script. Instead of referencing
//			an index into the coverpoint pool, script instead reference a scripted coverpoint guid'
//			This allows script to add cover points they create to an item set
////////////////////////////////////////////////////////////////////////////////

class CScriptedCoverPoint : public fwExtensibleBase
{

	DECLARE_RTTI_DERIVED_CLASS(CScriptedCoverPoint, fwExtensibleBase);

public:

	////////////////////////////////////////////////////////////////////////////////

	CScriptedCoverPoint(s32 iScriptIndex);
	~CScriptedCoverPoint();

	s32	GetScriptIndex() const { return m_iScriptIndex; }

private:

	////////////////////////////////////////////////////////////////////////////////

	s32				m_iScriptIndex;

	////////////////////////////////////////////////////////////////////////////////
};

class CCoverPoint
{
public:

	CCoverPoint();

	void Copy(const CCoverPoint& other);

	enum eCoverType
	{
		COVTYPE_NONE,
		COVTYPE_OBJECT,
		COVTYPE_BIG_OBJECT,
		COVTYPE_VEHICLE,
		COVTYPE_POINTONMAP,
		COVTYPE_SCRIPTED
	};

	enum eCoverHeight
	{
		COVHEIGHT_LOW = 0,		// Low cover, can blind fire using normal clips
		COVHEIGHT_HIGH,			// Low cover, can blind fire but using over the top clips (we don't have these variation currently)
		COVHEIGHT_TOOHIGH		// High cover
	};
	enum eCoverUsage
	{
		COVUSE_WALLTOLEFT = 0,
		COVUSE_WALLTORIGHT,
		COVUSE_WALLTOBOTH,
		COVUSE_WALLTONEITHER,
		COVUSE_WALLTOLEFTANDLOWRIGHT,
		COVUSE_WALLTORIGHTANDLOWLEFT,
		COVUSE_NUM
	};
	enum eCoverArc
	{
		COVARC_180 = 0,
		COVARC_120,
		COVARC_90,
		COVARC_45,
		// Angle ranges below use cover forward as zero
		// and measure positive angle increase counter-clockwise(CCW)
		COVARC_0TO60,
		COVARC_300TO0,
		COVARC_0TO45,
		COVARC_315TO0,
		COVARC_0TO22,
		COVARC_338TO0
	};
	enum eCoverStatusFlags
	{
		COVSTATUS_Invalid			= BIT0,
		COVSTATUS_Clear				= BIT1,
		COVSTATUS_PositionBlocked	= BIT2,
		COVSTATUS_DirectionBlocked	= BIT3 		
	};
	
	enum eCoverFlags
	{
		COVFLAGS_Transient	= BIT0,	//Describes a cover point that is transient in nature, i.e. a vehicle door
		COVFLAGS_ThinPoint	= BIT1,	//Too thin to be used by AI
		COVFLAGS_IsPriority = BIT2,	//Player prefers this cover point if valid cover
		COVFLAGS_IsLowCorner		= BIT3,	//Cover is low height but needs step out vantage point
		COVFLAGS_WaitingForCleanup	= BIT4  //A scripted cover point that will be cleaned up when no longer being used
	};


#if __BANK
	static const char* GetCoverTypeName(s32 iCoverType)
	{
		switch (iCoverType)
		{
			case COVTYPE_NONE: return "COVTYPE_NONE";
			case COVTYPE_OBJECT: return "COVTYPE_OBJECT";
			case COVTYPE_BIG_OBJECT: return "COVTYPE_BIG_OBJECT";
			case COVTYPE_VEHICLE: return "COVTYPE_VEHICLE";
			case COVTYPE_POINTONMAP: return "COVTYPE_POINTONMAP";
			case COVTYPE_SCRIPTED: return "COVTYPE_SCRIPTED";
			default: break;
		}
		return "COVTYPE_INVALID";
	}

	static const char* GetCoverHeightName(s32 iCoverHeight)
	{
		switch (iCoverHeight)
		{
			case COVHEIGHT_LOW: return "COVHEIGHT_LOW";
			case COVHEIGHT_HIGH: return "COVHEIGHT_HIGH";
			case COVHEIGHT_TOOHIGH: return "COVHEIGHT_TOOHIGH";
			default: break;
		}
		return "COVUSE_INVALID";
	}

	static const char* GetCoverUsageName(s32 iCoverUsage)
	{
		switch (iCoverUsage)
		{
			case COVUSE_WALLTOLEFT:				return "COVUSE_WALLTOLEFT";
			case COVUSE_WALLTORIGHT:			return "COVUSE_WALLTORIGHT";
			case COVUSE_WALLTOBOTH:				return "COVUSE_WALLTOBOTH";
			case COVUSE_WALLTONEITHER:			return "COVUSE_WALLTONEITHER";
			case COVUSE_WALLTOLEFTANDLOWRIGHT:		return "COVUSE_WALLTOLEFTANDLOWRIGHT";
			case COVUSE_WALLTORIGHTANDLOWLEFT:	return "COVUSE_WALLTORIGHTANDLOWLEFT";
			default: break;
		}
		return "COVUSE_INVALID";
	}

	static const char* GetCoverArcName(s32 iCoverArc)
	{
		switch (iCoverArc)
		{
			case COVARC_180: return "COVARC_180";
			case COVARC_120: return "COVARC_120";
			case COVARC_90: return "COVARC_90";
			case COVARC_45: return "COVARC_45";
			case COVARC_0TO60: return "COVARC_0TO60";
			case COVARC_300TO0: return "COVARC_300TO0";
			case COVARC_0TO45: return "COVARC_0TO45";
			case COVARC_315TO0: return "COVARC_315TO0";
			case COVARC_0TO22: return "COVARC_0TO22";
			case COVARC_338TO0: return "COVARC_338TO0";
			default: break;
		}

		return "COVARC_INVALID";
	}

	static const char* GetFlagName(s32 iCoverFlag)
	{
		switch (iCoverFlag)
		{
			case COVFLAGS_Transient: return "COVFLAGS_Transient";
			case COVFLAGS_ThinPoint: return "COVFLAGS_ThinPoint";
			case COVFLAGS_IsPriority: return "COVFLAGS_IsPriority";
			case COVFLAGS_IsLowCorner: return "COVFLAGS_IsLowCorner";
			default: break;
		}

		return "COVFLAGS_INVALID";
	}
#endif

	//////////////////////////////////////////////////////
	// FUNCTION: CanAccomodateAnotherPed
	// PURPOSE:  Given the type and the number of peds already using this point,
	//			 works out whether it can handle another ped.
	//////////////////////////////////////////////////////
	inline bool	CanAccomodateAnotherPed() const
	{
		if(m_Type!=COVTYPE_NONE)
		{
			return m_pPedsUsingThisCover[0].Get() == NULL;
		}
		return false;
	}
	//////////////////////////////////////////////////////
	// FUNCTION: GetOccupiedBy
	// PURPOSE:  Given the type works out the occupant
	//////////////////////////////////////////////////////
	inline CPed* GetOccupiedBy() const
	{
		if(m_Type!=COVTYPE_NONE)
		{
			return (m_pPedsUsingThisCover[0].Get());
		}
		return NULL;
	}
	//////////////////////////////////////////////////////
	// FUNCTION: IsOccupied
	// PURPOSE:  Given the type works out whether it is currently occupied
	//////////////////////////////////////////////////////
	inline bool	IsOccupied() const
	{
		if(m_Type!=COVTYPE_NONE)
		{
			return m_pPedsUsingThisCover[0].Get() != NULL;
		}
		return false;
	}
	//////////////////////////////////////////////////////
	// FUNCTION: IsOccupiedBy
	// PURPOSE:  Given the type works out whether it is currently occupied by a ped
	//////////////////////////////////////////////////////
	inline bool	IsOccupiedBy(const CPed* pPed) const
	{
		if(m_Type!=COVTYPE_NONE)
		{
			return (pPed && (pPed == m_pPedsUsingThisCover[0].Get()));
		}
		return false;
	}

	void		ReserveCoverPointForPed(CPed *pPed);
	void 		ReleaseCoverPointForPed(CPed *pPed);
	float		GetWorldCoverHeading() const;
	Vec3V_Out	GetCoverDirectionVector(const Vec3V* pvTargetDir = NULL) const;
	Vector3		GetLocalDirectionVector();
	bool		GetCoverPointPosition( Vector3 & vCoverPos, const Vector3* pLocalOffset = NULL ) const;
	Vec3V_Out	GetCoverPointPosition() const;
	void		GetLocalPosition(Vector3 & vCoverPos) const;
	bool		IsCoverPointMoving( float fVelocityThreshhold );
	bool		PedClosestToFacingLeft(const CPed& rPed) const;

	// Functions to transform stuff into local or out of local space
	// the coverpoint is simplified as a rotation about 1 axis, depending on the
	// entities orientation
	//bool		UntransformPosition( Vector3 & vPos ) const;
	bool		TransformPosition( Vector3 & vPos ) const;
	//void		UpdateStatus();

	// Returns true if the coverpoint is dangerous at the moment, e.g. a car that is on fire
	bool IsDangerous( void ) const;
	bool IsEdgeCoverPoint(bool bFacingLeft) const;
	bool IsEdgeCoverPoint() const;
	bool IsEdgeOrDoubleEdgeCoverPoint() const;
	bool IsCloseToPlayer(const CPed& ped) const;
	static bool IsCloseToPlayer(const CPed& ped, Vec3V_In vCoverPointPosition);

	inline eCoverType GetType() const { return (eCoverType)m_Type; } 
	inline eCoverHeight GetHeight() const { return (eCoverHeight)m_Height; }
	inline eCoverUsage GetUsage() const { return (eCoverUsage)m_Usage; }
	inline eCoverArc GetArc() const { return (eCoverArc)m_CoverArc; }
	eCoverArc GetArcToUse(const CPed& ped, const CEntity* pTarget) const;
	eCoverArc GetArcToUse(bool bHasLosToTarget, bool bIsUsingDefensiveArea) const;
	inline u8 GetDirection() const { return m_CoverDirection; }
	inline u16 GetStatus() const {return m_CoverPointStatus; }
	inline bool GetIsThin() const {return GetFlag(COVFLAGS_ThinPoint); }	
	inline eHierarchyId GetVehicleHierarchyId() const { return m_VehHierarchyId; }	

	inline CEntity* GetEntity() const { return m_pEntity; }

	inline void SetType(const eCoverType type)
	{
		Assert( type != COVTYPE_NONE );
		m_Type = (u8)type;
	}
	inline void SetTypeNone() { m_Type = (u8)COVTYPE_NONE; }

	inline void SetHeight(const eCoverHeight height) { m_Height = (u8)height; }
	inline void SetUsage(const eCoverUsage usage) { m_Usage = (u16)usage; }
	inline void SetArc(const eCoverArc arc) { m_CoverArc = (u8)arc; }
	inline void SetDirection(const u8 dir) { m_CoverDirection = dir; }
	void SetWorldDirection(const u8 dir);
	inline void SetPosition(const Vector3 & vCoverPos) { m_CoorsX = vCoverPos.x; m_CoorsY = vCoverPos.y; m_CoorsZ = vCoverPos.z; }
	void SetWorldPosition(const Vector3& vCoverWorldPos);
	inline void SetIsThin( const bool bThin ) { if(bThin) { SetFlag(COVFLAGS_ThinPoint); } else { ClearFlag(COVFLAGS_ThinPoint); } }	
	inline void SetVehicleHierarchyId( eHierarchyId eVehHierarchyId ) { m_VehHierarchyId = eVehHierarchyId; }	
	inline void SetStatus(u16 status) { m_CoverPointStatus = status; }
	inline void ResetCoverPointStatus() { m_CoverPointStatus = 0; }
	inline void ResetCoverFlags() { m_Flags = 0; }
	
	inline bool GetFlag(const eCoverFlags nFlag) const { return ((m_Flags & nFlag) != 0); }
	inline void SetFlag(const eCoverFlags nFlag) { m_Flags |= nFlag; }
	inline void ClearFlag(const eCoverFlags nFlag) { m_Flags &= ~nFlag; }

	inline void SetCachedObjectCoverIndex(const s8 index) { m_iCachedObjectCoverIndex = index; }
	inline s8 GetCachedObjectCoverIndex() const { return m_iCachedObjectCoverIndex; }
	
private:

	u32		m_Type				:3;
	u32		m_Height			:2;
	u32		m_Usage				:4;			// To give the AI a clue as to how to attack the player.
	u32		m_CoverArc			:3;			// The covering arc this point protects the ped within
	u32		m_CoverPointStatus	:4;			// The status flags for this coverpoint, is it blocked
	u32		m_CoverDirection	:8;			// In what direction does this point provide cover. (255 = any)
	u32		m_Flags				:5;			// Custom cover flags
	
//	u32		m_iCoverPointUID:15;			// Unique identifier of coverpoint
	eHierarchyId	m_VehHierarchyId;		// eHierarchyId id of the vehicle door of the cover point
	float			m_CoorsX, m_CoorsY, m_CoorsZ;

	// The index into CCachedObjectCover::m_aCoverPoints used to generate this coverpoint.
	// This is only used for object coverpoints and is negative if not used.
	s8		m_iCachedObjectCoverIndex;

public:

	RegdEnt		m_pEntity;
	RegdPed		m_pPedsUsingThisCover[MAX_PEDS_PER_COVER_POINT];	

	// These members are used to manage the cover point's membership of the CCoverPointsGridCell it is in.
	// By keeping these pointers here we can cleanly unlink the coverpoint from the list when it is freed.
	CCoverPoint * m_pPrevCoverPoint;
	CCoverPoint * m_pNextCoverPoint;
	CCoverPointsGridCell * m_pOwningGridCell;

};



/////////////////////////////////////////////////////////////////////////////////
// Simple struct to define the type and size of the individual allocated pools of
// cover, used to split up the main cover array into separate pools
typedef struct 
{
	s32	m_iCoverPoolType;
	s16	m_iSize;

} SCoverPoolSize;

/////////////////////////////////////////////////////////////////////////////////
// Simple struct to define the position and extents of a script cover area
// for appending to the cover bounding areas
struct SCachedScriptedCoverArea
{
	SCachedScriptedCoverArea() : m_vCoverAreaPosition(Vector3::ZeroType), m_vCoverAreaExtents(Vector3::ZeroType) {}

	Vector3 m_vCoverAreaPosition;
	Vector3 m_vCoverAreaExtents;

	void Reset()
	{
		m_vCoverAreaPosition = Vector3::ZeroType;
		m_vCoverAreaExtents = Vector3::ZeroType;
	}
};


//*********************************************************************
//	CCoverPointsGridCell is a simple grid-cell storing the coverpoints
//	whose origins lie within a given min/max extent.  The coverpoints
//	are referenced from a central pool.
//*********************************************************************

class CCoverPointsGridCell
{
public:

	static const float ms_fSizeOfGrid;

	// PURPOSE:	Padding space added to the computed minimum of the bounding box.
	static const Vec3V ms_vPadMin;

	// PURPOSE:	Padding space added to the computed maximum of the bounding box.
	static const Vec3V ms_vPadMax;

	CCoverPointsGridCell()
	{
		m_pFirstCoverPoint = NULL;
		m_bBoundingBoxForContentsExists = false;
	}

	// PURPOSE:	Initialize this object as a grid cell around the given position.
	void InitCellAroundPos(Vec3V_ConstRef posV);

	// PURPOSE:	Check if a point is within the grid cell, not including the max boundary.
	bool IsPointWithinCellExclMax(Vec3V_In posV) const
	{
		const VecBoolV greaterThanMinV = IsGreaterThanOrEqual(posV, m_vBoundingBoxForCellMin);
		const VecBoolV lessThanMaxV = IsLessThan(posV, m_vBoundingBoxForCellMax);
		const VecBoolV inBoxV = And(greaterThanMinV, lessThanMaxV);
		return IsTrueXYZ(inBoxV);
	}

	// PURPOSE:	Check if the given axis-aligned box intersects with the cell.
	bool DoesAABBOverlapCell(Vec3V_In boxMinV, Vec3V_In boxMaxV) const
	{
		const VecBoolV testAgainstMinV = IsGreaterThanOrEqual(boxMaxV, m_vBoundingBoxForCellMin);
		const VecBoolV testAgainstMaxV = IsLessThanOrEqual(boxMinV, m_vBoundingBoxForCellMax);
		return IsTrueXYZ(And(testAgainstMinV, testAgainstMaxV));
	}

	// PURPOSE:	Check if the given axis-aligned box intersects with the *contents* of the cell,
	//			i.e. the bounding box around the cover points inside the cell.
	bool DoesAABBOverlapContents(Vec3V_In boxMinV, Vec3V_In boxMaxV) const
	{
		if(m_bBoundingBoxForContentsExists)
		{
			const VecBoolV testAgainstMinV = IsGreaterThanOrEqual(boxMaxV, m_vBoundingBoxForContentsMin);
			const VecBoolV testAgainstMaxV = IsLessThanOrEqual(boxMinV, m_vBoundingBoxForContentsMax);
			return IsTrueXYZ(And(testAgainstMinV, testAgainstMaxV));
		}
		else
		{
			return false;
		}
	}

	// PURPOSE:	Check if a sphere intersects a box including all the cover points currently in this cell.
	bool DoesSphereOverlapContents(Vec3V_In sphereCenterV, ScalarV_In sphereRadiusV) const
	{
		if(m_bBoundingBoxForContentsExists)
		{
			return CCoverSupport::SphereIntersectsAABB(sphereCenterV, sphereRadiusV, m_vBoundingBoxForContentsMin, m_vBoundingBoxForContentsMax);
		}
		else
		{
			return false;
		}
	}

	// PURPOSE:	Check if a hemisphere intersects a box including all the cover points currently in this cell.
	bool DoesHemisphereOverlapContents(Vec3V_In hemisphereCenterV, ScalarV_In hemisphereRadiusV, Vec3V_In planeNormalV) const
	{
		if(m_bBoundingBoxForContentsExists)
		{
			return CCoverSupport::HemisphereIntersectsAABB(hemisphereCenterV, hemisphereRadiusV, planeNormalV, m_vBoundingBoxForContentsMin, m_vBoundingBoxForContentsMax);
		}
		else
		{
			return false;
		}
	}

	// PURPOSE:	Clear out the bounding box around the cover points in the cell.
	void ClearBoundingBoxForContents();

	// PURPOSE:	Recompute the bounding box around the cover points in the cell.
	void RecomputeBoundingBoxForContents();

	// PURPOSE:	Extend the bounding box around the cover points, so that it includes the given point.
	void GrowBoundingBoxForAddedPoint(Vec3V_In posV);

	// PURPOSE:	Min corner of a bounding box around the whole cell, in its 3D grid.
	// NOTES:	We could potentially get rid of the min/max vectors for this cell and just store
	//			the center position instead, since they are all of the same dimensions.
	Vec3V	m_vBoundingBoxForCellMin;

	// PURPOSE:	Max corner of a bounding box around the whole cell, in its 3D grid.
	Vec3V	m_vBoundingBoxForCellMax;

	// PURPOSE:	Min corner of a bounding box around the cover points in this cell, valid if m_bBoundingBoxForContentsExists is set.
	Vec3V	m_vBoundingBoxForContentsMin;

	// PURPOSE:	Max corner of a bounding box around the cover points in this cell, valid if m_bBoundingBoxForContentsExists is set.
	Vec3V	m_vBoundingBoxForContentsMax;

	CCoverPoint * m_pFirstCoverPoint;
	bool	m_bBoundingBoxForContentsExists;
};

//***************************************************************************
//	CCoverPointsContainer is a container which holds a number of grid-cells,
//	each of which contains coverpoints.  The idea is that we can greatly
//	speed up queries which need to determine whether a coverpoint already
//	exists in a certain location, by only visiting the grids which are
//	intersecting the position.
//***************************************************************************

class CCoverPointsContainer
{
	friend class CCover;

public:

	CCoverPointsContainer(void) { Init(); }
	~CCoverPointsContainer(void) { Shutdown(); }

	static void Init(void);
	static void Shutdown(void);
	static void Clear(void);

	static CCoverPointsGridCell * GetGridCellForCoverPoint(const Vector3 & vPosition);
	static CCoverPointsGridCell * GetGridCellForIndex(int iIndex) { return ms_CoverPointGrids[iIndex]; }
	static CCoverPointsGridCell * CheckIfNoOverlapAndGetGridCell(const Vector3 & vWorldPos, CCoverPoint::eCoverType Type, CEntity *pEntity, CCoverPoint*& pOverlappingCoverPoint, bool bAssertOnOverlap = true);
	static CCoverPointsGridCell * MoveCoverPointToAnotherGridCellIfRequired(CCoverPoint * pCoverPoint);

	inline static int GetNumGrids(void) { return ms_CoverPointGrids.GetCount(); }

private:

	static atArray<CCoverPointsGridCell*> ms_CoverPointGrids;
	static const int ms_iMaxNumberOfGrids;
};

/////////////////////////////////////////////////////////////////////////////////
// Class used to build and maintain a bounding box describing an area within which
// cover points should be added
class CCoverBoundingArea
{
public:
	CCoverBoundingArea();
	~CCoverBoundingArea();

	// initialises the bounding box with the center and extents specified
	void init( const Vector3& vCentre, const Vector3& vExtents );
	void initFromMinMax( const Vector3& vMin, const Vector3& vMax );

	// Attempts to expand the bounding box to include the area specified, but keeping within the maximum extents, returns true if successful
	bool ExpandToInclude( const Vector3& vCentre, const Vector3& vExtents, const Vector3& vMaxExtents );

	// Returns the current extents
	inline void GetExtents( Vector3& vMin, Vector3& vMax ) const
	{
		vMin = m_vBoundingMin;
		vMax = m_vBoundingMax;
	}

	// Returns true if the box contains the point
	inline bool ContainsPoint( const Vector3& vPoint ) const
	{
		if( ( vPoint.x < m_vBoundingMin.x ) ||
			( vPoint.y < m_vBoundingMin.y ) ||
			( vPoint.z < m_vBoundingMin.z ) ||
			( vPoint.x > m_vBoundingMax.x ) ||
			( vPoint.y > m_vBoundingMax.y ) ||
			( vPoint.z > m_vBoundingMax.z ) )
		{
			return false;
		}
		else
			return true;
	}
	inline void GetMin(Vector3 & vMin) const { vMin = m_vBoundingMin; }
	inline void GetMax(Vector3 & vMax) const { vMax = m_vBoundingMax; }
	inline void SetMin(const Vector3 & vMin) { m_vBoundingMin = vMin; }
	inline void SetMax(const Vector3 & vMax) { m_vBoundingMax = vMax; }

private:

	Vector3 m_vBoundingMin;
	Vector3 m_vBoundingMax;
};

/////////////////////////////////////////////////////////////////////////////////
// Class used to build and maintain a bounding box blocking generation of certain coverpoints
class CCoverBlockingBoundingArea : public CCoverBoundingArea
{
public:
	CCoverBlockingBoundingArea(): m_bBlocksObjects(false), m_bBlocksVehicles(false), m_bBlocksMap(false), m_bBlocksPlayer(false) {}
	~CCoverBlockingBoundingArea(){}

	bool GetBlocksObjects		( void )		{return m_bBlocksObjects;}
	bool GetBlocksVehicles		( void )		{return m_bBlocksVehicles;}
	bool GetBlocksMap			( void )		{return m_bBlocksMap;}
	bool GetBlocksPlayer		( void )		{return m_bBlocksPlayer;}
	void SetBlocksObjects		( bool bBlocksObjects )		{m_bBlocksObjects = bBlocksObjects;}
	void SetBlocksVehicles		( bool bBlocksVehicles )	{m_bBlocksVehicles = bBlocksVehicles;}
	void SetBlocksMap			( bool bBlocksMap )			{m_bBlocksMap = bBlocksMap;}
	void SetBlocksPlayer		( bool bBlocksPlayer )		{m_bBlocksPlayer = bBlocksPlayer;}
private:
	bool m_bBlocksObjects;
	bool m_bBlocksVehicles;
	bool m_bBlocksMap;
	bool m_bBlocksPlayer;
};

class CCoverSearchFlags
{
public:

	enum
	{
		CSD_DoRangeCheck	 = BIT0,	// Cover points found must be within max range of the search position
		CSD_DoAvoidancePosCheck	 = BIT1,	// Cover points found must be within min/max range of the avoidance position
		CSD_DoDirectionCheck = BIT2,	// Cover points found must provide cover from the target direction
		CSD_DoVantageCheck	 = BIT3,	// Cover points found must provide a LOS from the vantage point to the target
		CSD_DoRouteCheck	 = BIT4,	// Cover points found must be accessible from a certain start position
		CSD_DoExceptionCheck = BIT5		// Discount cover points found that are in the exception list
	};
};

// Cover search within range data wrapped up in a class so we don't have to pass a load of params into the search functions 
// Should be easy to add new data to this structure (Maybe we should have a base class and refactor the other search funcs?)
class CCoverSearchWithinRangeData
{
public:

	static bank_float	ms_fDefaultMaxSearchDist;
	static bank_float	ms_fDefaultMinToThreatDist;
	static bank_float	ms_fDefaultMaxToThreatDist;

public:

	CCoverSearchWithinRangeData();

	// Copy constructor
	CCoverSearchWithinRangeData(const CCoverSearchWithinRangeData& other);
	~CCoverSearchWithinRangeData();

	CCoverSearchWithinRangeData& operator=(const CCoverSearchWithinRangeData& src);

	void					SetPed(const CPed* pPed)							{ m_pPed = pPed; }
	void					SetFlags(const s32 iFlags)							{ m_iSearchFlags = iFlags; }
	void					SetRouteStartPos(const Vector3& vRouteStartPos)			{ m_vRouteStartPos = vRouteStartPos; }
	void					SetSearchPos(const Vector3& vSearchPos)					{ m_vSearchPos = vSearchPos; }
	void					SetAvoidancePos(const Vector3& vThreatPos)				{ m_vAvoidancePos = vThreatPos; }
	void					SetMaxSearchDist(float fMaxSearchDist)					{ m_fMaxSearchDist = fMaxSearchDist; }
	void					SetMinDistToAvoidancePos(float fMinDistToAvoidancePos)	{ m_fMinDistToAvoidancePos = fMinDistToAvoidancePos; }
	void					SetMaxDistToAvoidancePos(float fMaxDistToAvoidancePos)	{ m_fMaxDistToAvoidancePos = fMaxDistToAvoidancePos; }
	void					SetExceptions(CCoverPoint** apExceptions)				{ m_apExceptions = apExceptions; }
	void					SetNumExceptions(s32 iNumExceptions)					{ m_iNumExceptions = iNumExceptions; }

	const CPed*				GetPed() const											{ return m_pPed; }
	bool					IsFlagSet(s32 iFlagToCheck) const						{ return m_iSearchFlags.IsFlagSet(iFlagToCheck); }
	Vector3					GetRouteStartPos() const								{ return m_vRouteStartPos; }
	Vector3					GetSearchPos() const									{ return m_vSearchPos; }
	Vector3					GetThreatPos() const									{ return m_vAvoidancePos; }
	float					GetMaxSearchDist() const								{ return m_fMaxSearchDist; }
	float					GetMinDistToThreat() const								{ return m_fMinDistToAvoidancePos; }
	float					GetMaxDistToThreat() const								{ return m_fMaxDistToAvoidancePos; }
	CCoverPoint**			GetExceptions() const									{ return m_apExceptions; }
	s32					GetNumExceptions() const								{ return m_iNumExceptions; }

private:

	void			From(const CCoverSearchWithinRangeData& src);

	RegdConstPed			m_pPed;
	CCoverPoint**			m_apExceptions;
	s32					m_iNumExceptions;
	fwFlags32				m_iSearchFlags;
	Vector3					m_vRouteStartPos;
	Vector3					m_vSearchPos;
	Vector3					m_vAvoidancePos;
	float					m_fMaxSearchDist;
	float					m_fMinDistToAvoidancePos;
	float					m_fMaxDistToAvoidancePos;
};

//*****************************************************************
//	CCover
//	The main class for storing & interacting with cover-points.
//*****************************************************************

#define EXTRA_COVER_RANGE 5.0f

class CCover
{
public:

	friend class CCachedObjectCoverManager;
// Enum of the different types of cover pools allocated in the cover array
typedef enum
{
	CP_NavmeshAndDynamicPoints = 0,
	CP_SciptedPoint,
	CP_NumPools

} CoverPoolType;

	// Enum defining what operation to perform when CCover::Update() is called.  Every N millisecs
	// we will perform the next operation.  Additionally, we will remove invalidated coverpoints
	// on a seperate timer.
	enum eCoverUpdateOp
	{
		ENoOperation,
		EAddFromVehicles,
		EAddFromObjects,
		EAddFromMap,
		ENumUpdateOps
	};

	enum eBoundingAreaOp
	{
		ENoBoundingAreaOp,
		EIncreaseBoundingArea,
		EReduceBoundingArea
	};

// navmesh cover-point generation which is creating way to many of them.
	enum
	{
		// These are the static coverpoints originating from the map or from dynamic objects
		NUM_NAVMESH_COVERPOINTS = 2560, // was 1024

		// These are coverpoints which are scripted by level designers
		NUM_SCRIPTED_COVERPOINTS = 64
	};

	enum
	{
		NUM_COVERPOINTS = NUM_NAVMESH_COVERPOINTS + NUM_SCRIPTED_COVERPOINTS
	};

	enum { MAX_BOUNDING_AREAS = 40 };
	enum { MAX_HARD_CODED_AREAS = 3 };

	// Enum used to vary the method used to search for cover
	enum eCoverSearchType
	{
		CS_ANY = 0,
		CS_PREFER_PROVIDE_COVER,
		CS_MUST_PROVIDE_COVER
	};

	enum eCoverSearchFlags
	{
		SF_CanBeOccupied			= BIT0,
		SF_Random					= BIT1,
		SF_CheckForTargetsInVehicle	= BIT2,
		SF_ForAttack				= BIT3
	};

	static atBitSet				m_UsedCoverPoints;
	static CCoverPoint			m_aPoints[NUM_COVERPOINTS];
	static s32					m_NumPoints;
	static fwPtrListDoubleLink	m_ListOfProcessedBuildings;
	static s16					m_aMinFreeSlot[CP_NumPools];

	static const float			ms_fMaxSpeedSqrForCarToBeCover;
	static const float			ms_fMaxSpeedSqrForCarToBeLowCover;
	
	static const float			ms_fMaxDistSqrCoverPointCanMove;

	static const float			ms_fMaxSpeedSqrForObjectToBeCover;

	static const float			ms_fMaxCoverPointDistance;

	// Variables used to search for cover points around peds and form bounding boxes
	static const float			ms_fMaxPointDistanceSearchXY;
	static const float			ms_fMaxPointDistanceSearchZ;
	static const float			ms_fMinPointDistanceSearchXY;
	static const float			ms_fMinPointDistanceSearchZ;
	static float				ms_fCoverPointDistanceSearchXY;
	static float				ms_fCoverPointDistanceSearchZ;

	static const float			ms_fMaxCoverBoundingBoxXY;
	static const float			ms_fMaxCoverBoundingBoxZ;
	static const float			ms_fMinCoverBoundingBoxXY;
	static const float			ms_fMinCoverBoundingBoxZ;
	static float				ms_fCurrentCoverBoundingBoxXY;
	static float				ms_fCurrentCoverBoundingBoxZ;

	static const float			ms_fStepSizeForDecreasingBoundingBoxes;
	static const float			ms_fStepSizeForIncreasingBoundingBoxes;

	static const float			ms_fMaxCoverPointDistanceSqr;

	static const float			ms_fMinDistBetweenCoverPoints;
	static const float			ms_fMinDistSqrBetweenCoverPoints;

	static const float			ms_fMinDistBetweenObjectCoverPoints;
	static const float			ms_fMinDistSqrBetweenObjectCoverPoints;

	static int					ms_iCurrentNumMapAndDynamicCoverPts;
	static int					ms_iCurrentNumScriptedCoverPts;

	static void Init();
	static void InitSession(unsigned iMode);

#if __BANK
	static void InitWidgets();
#endif

	static void Update();
	static void Shutdown();

	static void RemoveAndMoveCoverPoints();

	static void SetEntityCoverOrientationDirty(CCoverPoint* pPoint);

	static s16 AddCoverPoint(CoverPoolType CoverPool, CCoverPoint::eCoverType Type, class CEntity *pEntity, Vector3 *pCoors, CCoverPoint::eCoverHeight Height, CCoverPoint::eCoverUsage Usage, u8 CoverDir, CCoverPoint::eCoverArc CoverArc =  CCoverPoint::COVARC_90, bool bIsPriority = false, eHierarchyId eVehicleDoorId = VEH_INVALID_ID, s16* piOverlappingCoverpoint = NULL );

	static void RemoveCoverPointsForThisEntity(CEntity *pEntity);

	static void	RemoveCoverPoint(CCoverPoint *pPoint);
	static void RemoveCoverPointWithIndex(s32 iIndex);

	typedef bool (CoverPointFilterConstructFn)(CCoverPointFilter* pFilterOut, int numBytesAvailable, void* pData);
	typedef bool (CoverPointValidityFn) ( const CCoverPoint* pCoverPoint, const Vector3& vCoverPointCoords, void* pData  );
	typedef float (CoverPointScoringFn) ( const CCoverPoint* pCoverPoint, const Vector3& vCoverPointCoords, void* pData  );

	static inline int GetIndexOfCoverPoint(const CCoverPoint * pCoverPoint)
	{
		// NB : beware of the pointer arithmetic here..
		u32 iIndex = ptrdiff_t_to_int(pCoverPoint - m_aPoints);
		Assert(iIndex < CCover::NUM_COVERPOINTS);
		return iIndex;
	}

	static inline CCoverPoint* FindCoverPointWithIndex(s32 iIndex)
	{
		Assert( iIndex >= 0 && iIndex < NUM_COVERPOINTS );
		if (m_aPoints[iIndex].GetType() != CCoverPoint::COVTYPE_NONE)
		{
			return &m_aPoints[iIndex];
		}
		return NULL;
	}

	static bool IsAnyPedUsingCoverOnThisEntity(CEntity *pEntity);

	struct sCoverPoint
	{
		static bool SortAscending(const sCoverPoint& a, const sCoverPoint& b) { return a.fScore < b.fScore; }
		static bool SortDescending(const sCoverPoint& a, const sCoverPoint& b) { return b.fScore < a.fScore; }

		CCoverPoint* pCoverPoint;
		float fScore;
	};

	static s32 FindCoverPoints(CPed* pPed, const Vector3& vSearchStartPos, const Vector3 &TargetCoors, const fwFlags32& iSearchFlags, CCoverPoint** paCoverpoints, s32 iNumCoverPointsToFind, CCoverPoint** paExceptions = NULL, int iNumExceptions = 0,
												 const eCoverSearchType eSearchType = CS_MUST_PROVIDE_COVER, const float fMinDistance = 0.0f, const float fMaxDistance =20.0f, CoverPointFilterConstructFn pFilterConstructFn = NULL, void* pData = NULL, const CPed* pTargetPed = NULL);

	static s32 FindCoverPointInArc(const CPed* pPed, Vector3* pvOut, const Vector3& vPos, const Vector3& vDirection, const float fArc, const float fMinDistance, const float fMaxDistance,
									const fwFlags32& iSearchFlags, const Vector3& vTarget, CCoverPoint** paCoverpoints, s32 iNumCoverPointsToFind, const eCoverSearchType eSearchType = CS_MUST_PROVIDE_COVER,
									CCoverPoint** paExceptions = NULL, int iNumExceptions = 0, CoverPointFilterConstructFn pFilterConstructFn = NULL, void* pData = NULL, const CPed* pTargetPed = NULL);

	static CCoverPoint* FindClosestCoverPoint(const CPed* pPed, const Vector3& vPos, const Vector3* pvTarget = NULL, const eCoverSearchType eSearchType = CS_MUST_PROVIDE_COVER, CCoverPoint::eCoverType eCoverType = CCoverPoint::COVTYPE_NONE, CEntity* pEntityConstraint = NULL, const bool bCoverPointMustBeFree = true );
	static CCoverPoint* FindClosestCoverPointWithCB(const CPed* pPed, const Vector3& vPos, float fMaxDistanceFromPed, const Vector3* pvTarget, const eCoverSearchType eSearchType, CoverPointScoringFn pScoringFn, CoverPointValidityFn pValidityFn, void* pData );
	static bool			FindCoverPointsWithinRange(CCoverPoint** paCoverpoints, s32& iNumCoverPoints, const CCoverSearchWithinRangeData& coverSearchData);

	struct sScoreCoverPointParams
	{

		sScoreCoverPointParams(	const CPed* ped, const CPed* targetPed, const Vector3& searchStartPos, const Vector3 &targetCoors,	Vector3& pointCoors, const fwFlags32& searchFlags,
								CCoverPoint** exceptions, int numExceptions, const eCoverSearchType searchType,	const float minDistanceSq, const float maxDistanceSq,
								const float pedToSearchStartDistanceSq, const float optimalCoverDistance, const bool hasCoverPoint, const bool hasLosToTarget, const bool usingDefensiveArea,
								const bool disableCoverAdjustments)
								: pPed(ped)
								, pTargetPed(targetPed)
								, vSearchStartPos(searchStartPos)
								, TargetCoors(targetCoors)
								, pCoverPoint(NULL)
								, PointCoors(pointCoors)
								, iSearchFlags(searchFlags)
								, paExceptions(exceptions)
								, iNumExceptions(numExceptions)
								, eSearchType(searchType)
								, fMinDistanceSq(minDistanceSq)
								, fMaxDistanceSq(maxDistanceSq)
								, fScore(0.0f)
								, fPedToSearchStartDistanceSq(pedToSearchStartDistanceSq)
								, fOptimalCoverDistanceSq(square(optimalCoverDistance))
								, bHasCoverPoint(hasCoverPoint)
								, bHasLosToTarget(hasLosToTarget)
								, bIsUsingDefensiveArea(usingDefensiveArea)
								, bDisableCoverAdjustments(disableCoverAdjustments)
								{
									bHasOptimalCoverDistance = optimalCoverDistance > 0.0f;
									bIsPedWithinOptimalCoverDistance = pedToSearchStartDistanceSq < square(optimalCoverDistance + EXTRA_COVER_RANGE);
								}
	 
		const CPed* pPed;
		const CPed* pTargetPed;
		const Vector3& vSearchStartPos;
		const Vector3 &TargetCoors;
		CCoverPoint* pCoverPoint;
		Vector3& PointCoors;
		const fwFlags32& iSearchFlags;
		CCoverPoint** paExceptions; 
		int iNumExceptions;
		const eCoverSearchType eSearchType;
		const float fMinDistanceSq;
		const float fMaxDistanceSq;
		const float fPedToSearchStartDistanceSq;
		float fOptimalCoverDistanceSq;
		bool bHasOptimalCoverDistance;
		bool bIsPedWithinOptimalCoverDistance;
		const bool bHasCoverPoint;
		const bool bHasLosToTarget;
		const bool bIsUsingDefensiveArea;
		const bool bDisableCoverAdjustments;
		float fScore;
	};

	static bool ScoreCoverPoint(sScoreCoverPointParams& scoringParams, const CCoverPointFilter* pFilter);
	static bool	CheckForTargetsInVehicle(CCoverPoint *pCoverPoint, const CPed* pPed);

	// The first function will check to make sure the ped has the cover point reserved before doing the checks
	static bool GetPedsCoverHeight(const CPed& rPed, CCoverPoint::eCoverHeight& coverPointHeight);
	static bool FindCoverDirectionForPed(const CPed& rPed, Vector3& vDirection, const Vector3& vToTarget);
	static bool FindCoverCoordinatesForPed(const CPed& rPed, const Vector3& vDirection, Vector3& vCoverCoords, Vector3* pVantagePointCoors = NULL, Vector3* pCoverDirection = NULL);
	static bool FindCoordinatesCoverPoint(CCoverPoint *pCoverPoint, const CPed *pPed, const Vector3 &vDirection, Vector3 &ResultCoors, Vector3 *pVantagePointCoors = NULL);
	static bool FindCoordinatesCoverPointNotReserved(CCoverPoint *pCoverPoint, const Vector3 &vDirection, Vector3 &ResultCoors, Vector3 *pVantagePointCoors);
	
	static void CalculateCoverArcDirections(const CCoverPoint& rCoverPoint, Vector3& vCoverArcDir1, Vector3& vCoverArcDir2);

	static void CalculateVantagePointForCoverPoint( CCoverPoint *pCoverPoint, const Vector3 &vDirection, Vector3 &vCoverCoors, Vector3& vVantagePointCoors, bool bIgnoreProvidesCoverCheck = false );
	static void CalculateVantagePointFromPosition( CCoverPoint *pCoverPoint, const Vector3 &vPosition, const Vector3 &vDirection, Vector3 &vCoverCoors, Vector3& vVantagePointCoors );

	//////////////////////////////////////////////////////
	// FUNCTION: FindDirFromVector
	// PURPOSE:  Given a vector this function will return a direction value in the range (0-255).
	//////////////////////////////////////////////////////
	static inline u8 FindDirFromVector(const Vector3& Dir)
	{
		return (u8)(rage::Atan2f(-Dir.x, Dir.y) * (COVER_POINT_DIR_RANGE / (2.0f * PI)));
	}
	//////////////////////////////////////////////////////
	// FUNCTION: FindVectorFromDir
	// PURPOSE:  Given a direction this function will return the vector to go with it.
	//////////////////////////////////////////////////////

	static inline Vec3V_Out FindVectorFromDir(const u32 &dir)
	{
		// First, we need to get the direction into a vector register.
		// This is somewhat expensive, probably a LHS in the calling code since
		// the direction is stored inside of a bitfield that we can't easily
		// load from without a temporary variable.
		const ScalarV dirIntV = ScalarVFromU32(dir);
		const ScalarV dirV = IntToFloatRaw<0>(dirIntV);

		// We will need a few constants, which we can squeeze together into one vector.
		const Vec4V constantsV(Vec::V4VConstant<
				FLOAT_TO_INT(-2.0f*PI/COVER_POINT_ANY_DIR),
				FLOAT_TO_INT(-PI),
				FLOAT_TO_INT(2.0f*PI),
				FLOAT_TO_INT(0.5f*PI)>());

		// Extract the constants.
		const ScalarV scaleV = constantsV.GetX();
		const ScalarV negPiV = constantsV.GetY();
		const ScalarV twoPiV = constantsV.GetZ();
		const ScalarV halfPiV = constantsV.GetW();
		const ScalarV zeroV(V_ZERO);

		// Multiply the direction (integer value) by a scaling factor to get
		// an angle in radians. It's negated relative to the old FindVectorFromDir() function,
		// so we don't have to negate after the SinFast() call.
		const ScalarV baseAngleV = Scale(dirV, scaleV);

		// We will compute both the sine and cosine values as a single call to SinFast(). To do this,
		// we first build a vector containing the angle, the angle + half pi (to get the cosine),
		// and zero in the Z component. Since sin(0)=0, this should come through in the output.
		const Vec3V anglesV(baseAngleV, Add(baseAngleV, halfPiV), zeroV);

		// SinFast() can only handle angles in the -PI..PI range. Some will be less than PI here,
		// since we negated them before, so we need to selectively add 2*PI to the components that need it.
		const Vec3V anglesPlusTwoPiV = Add(anglesV, Vec3V(twoPiV));
		const Vec3V normalizedAnglesV = SelectFT(IsLessThan(anglesV, Vec3V(negPiV)), anglesV, anglesPlusTwoPiV);

		// Compute the sine (in X) and cosine (in Y), with 0 in the Z component.
		const Vec3V sinAndCosV = SinFast(normalizedAnglesV);
		return sinAndCosV;
	}

	static bool RemoveCoverPointIfEntityLost(CCoverPoint *pPoint);

	static inline bool DoesCoverPointProvideCover(CCoverPoint *pPoint, CCoverPoint::eCoverArc arcToUse, const Vector3& TargetCoors)
	{
		Vector3 vPosition;
		pPoint->GetCoverPointPosition(vPosition);
		return DoesCoverPointProvideCover(pPoint, arcToUse, TargetCoors, vPosition);
	}
	static bool DoesCoverPointProvideCover(CCoverPoint *pPoint, CCoverPoint::eCoverArc arcToUse, const Vector3& TargetCoors, const Vector3 & vCoverPos);

	static inline bool DoesCoverPointProvideCoverFromTargets(const CPed* pPed, const Vector3& vTargetPos, CCoverPoint *pPoint, CCoverPoint::eCoverArc arcToUse,
															 const CEntity* pTargetEntity = NULL)
	{
		Vector3 vPosition;
		if(pPoint->GetCoverPointPosition(vPosition))
		{
			return DoesCoverPointProvideCoverFromTargets(pPed, vTargetPos, pPoint, arcToUse, vPosition, pTargetEntity);
		}
		
		return false;
	}
	static bool DoesCoverPointProvideCoverFromTargets(const CPed* pPed, const Vector3& vTargetPos, CCoverPoint *pPoint, CCoverPoint::eCoverArc arcToUse,
													  const Vector3 &vCoverPos, const CEntity* pTargetEntity = NULL);
	static bool DoesCoverPointProvideCoverFromDirection(CCoverPoint *pPoint, CCoverPoint::eCoverArc arcToUse, const Vector3& TargetDirection);

	// Helper functions to add cover points for objects and vehicles
	// RETURN: Whether the given entity generated cover
	static bool AddCoverPointFromObject( CObject* pObj );
	static bool AddCoverPointFromVehicle( CVehicle* pVeh );

	// Helper to add cover points for an upright vehicle
	// Returns TRUE if all coverpoint addition succeeded, FALSE if any failed.
	static bool HelperAddCoverForUprightVehicle( CVehicle* pVeh );

	// Helper to add cover points for a non-upright vehicle
	// Returns TRUE if all coverpoint addition succeeded, FALSE if any failed.
	static bool HelperAddCoverForNonUprightVehicle( CVehicle* pVeh );

	// Helper to remove cover points for a vehicle
	static inline void RemoveCoverPointsForVehicle( CVehicle* pVeh );

	// Helper to add open forward door cover point for a vehicle
	static s16 HelperAddCoverPointOpenForwardDoor( CVehicle* pVeh, eHierarchyId doorID, int doorBoneIndex );

	// Helper to add closed door cover point for a vehicle
	static s16 HelperAddCoverPointClosedDoor( CVehicle* pVeh, eHierarchyId doorID, int doorBoneIndex );

	// Helpers to check vehicle doors for change to trigger cover recalculation
	static inline bool ComputeHasAnyVehicleDoorChangedForCover( CVehicle* pVeh );
	static inline bool ComputeHasDoorChangedForCover( CVehicle* pVeh, int iDoorIndex );

	// Helper to map simple index (0, 1, 2...) to vehicle door hierarchy ID (e.g. VEH_DOOR_DSIDE_F)
	static inline eHierarchyId HelperMapIndexToHierarchyID( int index );

	static s32 LastTimeRecorded;
	static s32 SlowestTimeRecorded;

	// How often we should perform coverpoint adds from the map
	static const u32 ms_iAddFromMapOperationFrequencyMS;
	// How often we should perform coverpoint adds from objects
	static const u32 ms_iAddFromObjectsOperationFrequencyMS;
	// How often we should perform coverpoint adds from vehicles
	static const u32 ms_iAddFromVehiclesOperationFrequencyMS;
	// How often we should perform the removal operation
	static const u32 ms_iCoverPointRemovalFrequency;
	// How many coverpoint indices should we check per operation call
	static const u32 ms_iNumRemovalChecksPerFrame;
	// The last coverpoint index we checked in the removal operation
	static u32 ms_iLastRemovalStopIndex;
	// The next time in millisecs that cover should be added from the map
	static u32 ms_iNextAddCoverPointsFromMapTime;
	// The next time in millisecs that cover should be added from objects
	static u32 ms_iNextAddCoverPointsFromObjectsTime;
	// The next time in millisecs that cover should be added from vehicles
	static u32 ms_iNextAddCoverPointsFromVehiclesTime;
	// The last time in millisecs that coverpoints were removed
	static u32 ms_iLastTimeCoverPointsRemoved;
	// This flag will force all coverpoints to be updated the next frame
	static bool ms_bForceUpdateAll;
	// The last coverpoint index whose status was updated
	static s32 ms_iLastStatusCheckIndex;
	static s32 ms_iNumStatusUpdatesPerFrame;

	// PURPOSE:	Index into the CObject pool for the next slot we will consider updating cover for.
	static u32 ms_iLastAddFromObjectStopIndex;

	// PURPOSE:	The number of elements in the CObject pool that we process on each frame dedicated to processing object cover.
	static dev_u32 ms_iNumAddFromObjectChecksPerFrame;

	// PURPOSE: The number of expensive actual additions permitted in the same frame
	static dev_u32 ms_iNumActualAddsFromObjectsPerFrame;

	// PURPOSE:	Index into the CVehicle pool for the next slot we will consider updating cover for.
	static u32 ms_iLastAddFromVehicleStopIndex;

	// PURPOSE:	The number of elements in the CVehicle pool that we process on each frame dedicated to processing vehicle cover.
	static dev_u32 ms_iNumAddFromVehicleChecksPerFrame;

	// PURPOSE: The number of expensive actual additions permitted in the same frame
	static dev_u32 ms_iNumActualAddsFromVehiclesPerFrame;

	// PURPOSE: The number of expensive actual additions permitted in the same frame
	static dev_u32 ms_iNumActualAddsFromMapPerFrame;

	// What operation to perform on the bounding areas next update (if any)
	static eBoundingAreaOp ms_eBoundingAreaOperation;

#if !__FINAL && !__PPU
	// Timing variables for debugging.  The linear search through the coverpoints list every time we want
	// to add a new coverpoint is **extremely** inefficient, so we really need to keep an eye on this..
	static sysPerformanceTimer	* ms_pPerfTimer;
	static float ms_fMsecsTakenToRemoveCoverPoints;
	static float ms_fMsecsTakenToAddFromVehicles;
	static float ms_fMsecsTakenToAddFromObjects;
	static float ms_fMsecsTakenToAddFromMap;
#endif

#if !__FINAL
	static void Render();

	// Debug rendering of cover bounding areas
	static void RenderCoverAreaBoundingBoxes();

	// Render a bounding box in a particular color
	static void RenderBoundingBox(const Vector3& vMin, const Vector3& vMax, Color32 color);

	// PURPOSE:	Render the cells which are used as a spatial data structure for cover.
	static void RenderCoverCells();

#endif

#if __BANK
	static bool ms_bUseDebugCameraAsCoverOrigin;
#endif

	// Calculate the cover point height enum from the given floating point height
	static CCoverPoint::eCoverHeight FindCoverPointHeight( const float fHeight, s32 iType = CCoverPoint::COVTYPE_NONE);
	// Calculate the cover point usage from the object width
	static CCoverPoint::eCoverUsage GetCoverPointUsageFromWidth( const float fCoverWidth );
	// Determine whether a cover point is a low corner
	static bool IsCoverLowCorner( const CCoverPoint::eCoverHeight eHeight, const CCoverPoint::eCoverUsage eUsage);
	// Examines all currently existing peds interested in cover and generates a set of bounding boxes to encompass them
	static void GenerateCoverAreaBoundingBoxes();
	// Helper function for GenerateCoverAreaBoundingBoxes
	static bool AddNewCoverArea(const Vector3& vCoverSearchCenter, const Vector3& vCoverExtents);
	// Called from the native to add a scripted cover area to be processed at the end of GenerateCoverAreaBoundingBoxes
	static bool AddNewScriptedCoverArea(const Vector3& vCoverAreaCenter, const float fRadius);
	// Returns true if the given point is in a given cover point area
	static bool IsPointWithinValidArea( const Vector3& vPoint, const CCoverPoint::eCoverType coverType, bool bIsPlayer = false);
	// Copies the array of bounding areas, and returns the count (for use when copying data to CPathServer class)
	static s32 CopyBoundingAreas(CCoverBoundingArea * pAreasBuffer);
	// Recalculates the max extents of boundings areas for AI & player
	static void RecalculateCurrentCoverExtents();
	// Reduces the bounding extents by the given amount
	static bool ReduceBoundingAreaSize(const float fDecrement);
	// Increases the bounding extents by the given amount
	static bool IncreaseBoundingAreaSize(const float fDecrement);
	// Returns whether the usage of the given cover pool exceeds the input ratio, where (0.0f==empty & 1.0f=full)
	static bool IsCoverPoolOverComitted(const int iPool, const float fRatio);

	// Add/remove a blocking area
	static void AddCoverBlockingArea( const Vector3& vStart, const Vector3& vEnd, const bool bBlockObjects, const bool bBlockVehicles, const bool bBlockMap, const bool bBlockPlayerCoverOnly );
	static void RemoveCoverBlockingAreasAtPosition(const Vector3& vPosition);
	static void RemoveSpecificCoverBlockingArea( const Vector3& vStart, const Vector3& vEnd, const bool bBlockObjects, const bool bBlockVehicles, const bool bBlockMap, const bool bBlockPlayerCoverOnly );
	static void AddHardCodedCoverBlockingArea( const Vector3& vStart, const Vector3& vEnd );
	//-------------------------------------------------------------------------
	// Flush all areas that block the creation of coverpoints
	//-------------------------------------------------------------------------
	inline static void FlushCoverBlockingAreas()
	{
		ms_blockingBoundingAreas.Reset();
	}
private:

	// Given an array of cover points, a new cover point, some scores etc we will try to add a cover point to an array
	static void PossiblyAddCoverPointToArray(sCoverPoint *paCoverPoints, CCoverPoint *pCoverPoint, const float &fScore, float &fWorstScore, s32 &iWorstIndex, s32 &iNumCoverPoints, const s32 &iNumCoverPointsToFind, bool bIsHighScoreBetter);

	// Adjusts the arc and direction of the cover point depending on the usage
	static void AdjustMapCoverPoint( CCoverPoint* pPoint );

	// Static structure defining the allocation of the main cover point array into pools
	static SCoverPoolSize m_aCoverPoolSizes[CP_NumPools];

	// Static structure defining a scripted cover area which can be appended to the cover bounding areas
	static SCachedScriptedCoverArea m_sCachedScriptedCoverArea;

	// Calculates the cover pool type extents
	static bool CalculateCoverPoolExtents( CoverPoolType CoverPool, s16 &iPoolMin, s16 &iPoolMax );

	// Fetch the next available slot for the given cover pool
	static bool GetNextAvailableSlot( CoverPoolType CoverPool, s16& out_iAvailableSlot );

	// Modify static data to account for new slot in use
	static void SetSlotInUse( CoverPoolType CoverPool, s16 iSlotInUse );

	// Modify static data to account for new available slot
	static void SetSlotAvailable( CoverPoolType CoverPool, s16 iAvailableSlot );

	// Get which pool this coverpoint should belong to
	static inline CoverPoolType GetCoverPoolType( CCoverPoint::eCoverType iCoverType )
	{
		if(iCoverType==CCoverPoint::COVTYPE_OBJECT || iCoverType==CCoverPoint::COVTYPE_BIG_OBJECT || iCoverType==CCoverPoint::COVTYPE_VEHICLE || iCoverType==CCoverPoint::COVTYPE_POINTONMAP)
			return CP_NavmeshAndDynamicPoints;
		else if(iCoverType==CCoverPoint::COVTYPE_SCRIPTED)
			return CP_SciptedPoint;
		
		Assert(0);
		return CP_NavmeshAndDynamicPoints;
	}

	// Whether the given CoverPool value is valid
	static inline bool IsCoverPoolTypeValid( CoverPoolType CoverPool )
	{
		if( CoverPool == CP_NavmeshAndDynamicPoints || CoverPool == CP_SciptedPoint )
		{
			return true;
		}
		return false;
	}

	// Areas describing locations where coverpoint should be added
	static CCoverBoundingArea ms_boundingAreas[MAX_BOUNDING_AREAS];
	static u32 ms_iNumberAreas;

	// Areas set by script to block the generation of coverpoints
	static atFixedArray<CCoverBlockingBoundingArea, MAX_BOUNDING_AREAS> ms_blockingBoundingAreas;
	static CCoverBlockingBoundingArea ms_hardCodedBlockingBoundingAreas[MAX_HARD_CODED_AREAS];
	static u32 ms_iNumberHardCodedBlockingAreas;

	// static variables describing the extents required around the player and AI peds for cover
	// AND the maximum extents for any box
	static Vector3 ms_vPlayerCoverExtents;
	static Vector3 ms_vAiCoverExtents;
	static Vector3 ms_vMaxCoverExtents;

};

//-------------------------------------------------------------------------
// Class describing a local cover point
//-------------------------------------------------------------------------
class CCoverPointInfo
{
public:
	CCoverPointInfo() 
		: m_vLocalOffset(0.0f, 0.0f, 0.0f)
		, m_iLocalDir(0)
		, m_iCoverHeight(0)
		, m_iCoverArc(0)
		, m_iUsage(0)
		, m_bThin(false)
		, m_bLowCorner(false)
	{

	}
	//virtual ~CCoverPointInfo() {}

	// Setup the cover point
	void init( Vector3& vOffset, u8 iDir, float fCoverHeight, s32 iUsage, s32 iCoverArc, s32 iType = CCoverPoint::COVTYPE_NONE);
	void init( Vector3& vOffset, float fDir, float fCoverHeight, s32 iUsage, s32 iCoverArc, s32 iType = CCoverPoint::COVTYPE_NONE);

	// Modify cover arc for corner cover
	void AdjustArcForCorner();

	Vector3 m_vLocalOffset;
	u8	m_iLocalDir;
	s32	m_iCoverHeight;
	s32	m_iCoverArc;
	s32	m_iUsage;
	u32	m_bThin : 1;
	u32 m_bLowCorner : 1;
};


//-------------------------------------------------------------------------
// Orientation of the object for the cover generation
//-------------------------------------------------------------------------
typedef enum 
{
	CO_Invalid,
	CO_ZUp,
	CO_ZDown,
	CO_YUp,
	CO_YDown,
	CO_XUp,
	CO_XDown,
	CO_Dirty	// We had some orientation set before, but we may now need to re-add the points because the object may have moved.
} CoverOrientation;

//-------------------------------------------------------------------------
// Single cached set of coverpoints for an object
//-------------------------------------------------------------------------
class CCachedObjectCover
{
public:

	// Maximum no. of cover points per object
	enum { MAX_COVERPOINTS_PER_OBJECT = 12 };

	// All objects may have cover centered on a major face
	class LocalOffsetData_Base
	{
	protected:

		LocalOffsetData_Base() :	m_NorthCenter(V_ZERO), m_SouthCenter(V_ZERO), m_WestCenter(V_ZERO), m_EastCenter(V_ZERO),
									m_fDirCoverNorth(0.0f), m_fDirCoverSouth(0.0f), m_fDirCoverEast(0.0f), m_fDirCoverWest(0.0f) {}

	public:

		Vec3V m_NorthCenter;
		Vec3V m_SouthCenter;
		Vec3V m_WestCenter;
		Vec3V m_EastCenter;

		// direction cover should face towards object center on each face
		float m_fDirCoverNorth; 
		float m_fDirCoverSouth;
		float m_fDirCoverEast;
		float m_fDirCoverWest;
	};

	// General case may add corners at the ends of each major face
	class LocalOffsetData_General : public LocalOffsetData_Base
	{
	public:

		LocalOffsetData_General() : m_NorthWest(V_ZERO), m_NorthEast(V_ZERO), m_EastNorth(V_ZERO), m_EastSouth(V_ZERO), m_SouthWest(V_ZERO), m_SouthEast(V_ZERO), m_WestNorth(V_ZERO), m_WestSouth(V_ZERO) {}

		Vec3V m_NorthWest;
		Vec3V m_NorthEast;	
		Vec3V m_EastNorth;
		Vec3V m_EastSouth;
		Vec3V m_SouthWest;
		Vec3V m_SouthEast;
		Vec3V m_WestNorth;
		Vec3V m_WestSouth;
	};

	// Small objects may add diagonal corners that are shared on two major faces
	class LocalOffsetData_Small : public LocalOffsetData_Base
	{
	public:

		LocalOffsetData_Small() :	m_NorthWest(V_ZERO), m_NorthEast(V_ZERO), m_SouthWest(V_ZERO), m_SouthEast(V_ZERO),
									m_fDirCoverNorthWest(0.0f), m_fDirCoverNorthEast(0.0f), m_fDirCoverSouthWest(0.0f), m_fDirCoverSouthEast(0.0f) {}

		Vec3V m_NorthWest;
		Vec3V m_NorthEast;
		Vec3V m_SouthWest;
		Vec3V m_SouthEast;

		// direction cover should face towards object center on each diagonal
		float m_fDirCoverNorthWest; 
		float m_fDirCoverNorthEast;
		float m_fDirCoverSouthWest;
		float m_fDirCoverSouthEast;
	};

	class LocalOffsetDataParams_Base
	{
	protected:
		
		LocalOffsetDataParams_Base(Vec3V_In vBoundsMin, Vec3V_In vBoundsMax, ScalarV_In fCoverToObject) : m_vBoundsMin(vBoundsMin), m_vBoundsMax(vBoundsMax), m_CoverToObjectPadding(fCoverToObject) {}
	
	public:
		
		// Bounds of the object being analyzed
		Vec3V m_vBoundsMin;
		Vec3V m_vBoundsMax;

		// How far away from the object each offset should be
		ScalarV m_CoverToObjectPadding;

	private:
		
		LocalOffsetDataParams_Base() {} // don't allow default construction, including derived classes
	};

	class LocalOffsetDataParams_General : public LocalOffsetDataParams_Base
	{
	public:
		
		LocalOffsetDataParams_General(Vec3V_In vBoundsMin, Vec3V_In vBoundsMax, ScalarV_In fCoverToObject, ScalarV_In fCoverToObjectCorner)
			: LocalOffsetDataParams_Base(vBoundsMin, vBoundsMax, fCoverToObject)
			, m_CoverToObjectCornerPadding(fCoverToObjectCorner)
		{
			// empty
		}

		// How far to pull in from corners for corner offsets
		ScalarV m_CoverToObjectCornerPadding;
	};

	class LocalOffsetDataParams_Small : public LocalOffsetDataParams_Base
	{
	public:
		
		LocalOffsetDataParams_Small(Vec3V_In vBoundsMin, Vec3V_In vBoundsMax, ScalarV_In fCoverToObject, ScalarV_In fCoverToSmallObjectDiagonal)
			: LocalOffsetDataParams_Base(vBoundsMin, vBoundsMax, fCoverToObject)
			, m_fCoverToSmallObjectDiagonal(fCoverToSmallObjectDiagonal)
		{
			// empty
		}
		
		// How far from the object on diagonals so they don't float too far out
		ScalarV m_fCoverToSmallObjectDiagonal;
	};

	// CONSTRUCTOR
	CCachedObjectCover()
		: m_iCoverPoints(0)
		, m_iModelHashKey(0)
		, m_vBoundingMin(0.0f, 0.0f, 0.0f)
		, m_vBoundingMax(0.0f, 0.0f, 0.0f)
		, m_bActive(false)
		, m_bForceTypeObject(false)
	{

	};

	// Query fns
	bool	IsActive( void ) {return m_bActive;}
	u32	GetModelHashKey( void ) {return m_iModelHashKey;}

	void	Setup( CPhysical* pObject );
	bool	AddCoverPoints( CPhysical* pPhysical );

	bool	DoesPositionOverlapAnyPoint( const Vector3& vLocalPos );

	// returns the corrected bounds
	void	GetCorrectedBoundingBoxes( Vector3& vBoundingMin, Vector3& vBoundingMax );

	// Returns the next coverpoint local offset in the direction given
	void FindNextCoverPointOffsetInDirection( Vector3& vOut, const Vector3& vStart, const bool bClockwise );

	// Returns the orientation
	CoverOrientation GetOrientation() { return m_eObjectOrientation; }

	// Computes local offset data for the given orientation and parameters
	// Returns TRUE if data was set, otherwise FALSE
	bool HelperComputeLocalOffsetData_Small( const CoverOrientation& orientation, const LocalOffsetDataParams_Small& params, LocalOffsetData_Small& out_data );

	// Computes local offset data for the given orientation and parameters
	// Returns TRUE if data was set, otherwise FALSE
	bool HelperComputeLocalOffsetData_General( const CoverOrientation& orientation, const LocalOffsetDataParams_General& params, LocalOffsetData_General& out_data );

#if __BANK
	void DebugReset();
#endif // __BANK

private:
	// Cover point storage
	CCoverPointInfo		m_aCoverPoints[MAX_COVERPOINTS_PER_OBJECT];
	s32					m_iCoverPoints;
	u32					m_iModelHashKey;
	Vector3				m_vBoundingMin;
	Vector3				m_vBoundingMax;
	CoverOrientation	m_eObjectOrientation;
	u32					m_bActive : 1;
	u32					m_bForceTypeObject : 1;
};


//-------------------------------------------------------------------------
// manager responsible for cached object cover points
//-------------------------------------------------------------------------
class CCachedObjectCoverManager
{
public:
	// Adds cover points for the given objects, sourcing them from the cached set if present
	static bool AddCoverpointsForPhysical( CPhysical* pPhysical );
	// returns the corrected bounds for the object passed
	static bool GetCorrectedBoundsForObject( CObject* pObject, Vector3& vBoundMin, Vector3& vBoundMax );
	// Returns the next coverpoint local offset in the direction given
	static bool FindNextCoverPointOffsetInDirection( CObject* pObject, Vector3& vOut, const Vector3& vStart, const bool bClockwise );
	// Returns the local UP axis of the object
	inline static CoverOrientation GetUpAxis( const CPhysical* pObject );
	inline static CoverOrientation GetUpAxis( const Matrix34 & mat );
	// Rotates the given local position in local Z by the given physical's world heading
	static Vec3V_Out RotateLocalZToWorld(Vec3V_In localPos, const CPhysical& physical);
	// Rotates the given global position in local Z by the given physical's world heading
	inline static void RotateWorldToLocalZ( Vector3& inout_vGlobalPos, const CPhysical* pPhysical);
	// Rotates and translates the given local position into  world coordinates
	inline static void TransformLocalToWorld( Vector3& inout_vLocalPos, const CPhysical* pPhysical );
#if __BANK
	inline static void DebugResetCache();
#endif // __BANK
private:
	// Returns the objects cached cover if its in cache
	static CCachedObjectCover* FindObjectInCache( const u32 iModelHashKey, const CoverOrientation eOrientation );
	// Maximum no. of cached object cover points
	enum { MAX_CACHED_OBJECT_COVER_POINTS = 50 };
	static CCachedObjectCover m_aCachedCoverPoints[MAX_CACHED_OBJECT_COVER_POINTS];
	static s32			  m_iNextFreeCachedObject;
};

extern dev_float		ZUPFOROBJECTTOBEUPRIGHT;
extern dev_float		COVERPOINT_LOS_Z_ELEVATION;

//-------------------------------------------------------------------------
// Returns the local UP axis of the object
//-------------------------------------------------------------------------
inline CoverOrientation CCachedObjectCoverManager::GetUpAxis( const CPhysical* pPhysical )
{
	const Matrix34& physMat = RCC_MATRIX34(pPhysical->GetMatrixRef());
	return GetUpAxis(physMat);
}

inline CoverOrientation CCachedObjectCoverManager::GetUpAxis( const Matrix34 & mat )
{
	if( mat.c.z > ZUPFOROBJECTTOBEUPRIGHT )
	{
		return CO_ZUp;
	}
	else if( mat.c.z < -ZUPFOROBJECTTOBEUPRIGHT )
	{
		return CO_ZDown;
	}
	else if( mat.b.z > ZUPFOROBJECTTOBEUPRIGHT )
	{
		return CO_YUp;
	}
	else if( mat.b.z < -ZUPFOROBJECTTOBEUPRIGHT )
	{
		return CO_YDown;
	}
	else if( mat.a.z > ZUPFOROBJECTTOBEUPRIGHT )
	{
		return CO_XUp;
	}
	else if( mat.a.z < -ZUPFOROBJECTTOBEUPRIGHT )
	{
		return CO_XDown;
	}
	return CO_Invalid;
}

#if 0	// Old unoptimized version, for reference only.

//----------------------------------------------------------------------------------
// Rotates the given local position in local Z by the given physical's world heading
//----------------------------------------------------------------------------------
inline void CCachedObjectCoverManager::RotateLocalZToWorld( Vector3& inout_vLocalPos, const CPhysical* pPhysical )
{
	Matrix34 refMat;
	pPhysical->GetMatrixCopy(refMat);
	const CoverOrientation eOrientation = GetUpAxis(refMat);
	if( eOrientation == CO_ZUp )
	{
		inout_vLocalPos.RotateZ( pPhysical->GetTransform().GetHeading() );
	}
	else if( eOrientation == CO_ZDown)
	{
			inout_vLocalPos.RotateZ( -pPhysical->GetTransform().GetHeading() );
		}
	else
		{
		inout_vLocalPos.RotateZ(rage::Atan2f(-refMat.c.x, refMat.c.y));
		}
	}

#endif	// 0 - Old unoptimized version, for reference only.

//----------------------------------------------------------------------------------
// Rotates the given global position in local Z by the given physical's world heading
// (Copy-paste of the above)
//----------------------------------------------------------------------------------
inline void CCachedObjectCoverManager::RotateWorldToLocalZ( Vector3& inout_vGlobalPos, const CPhysical* pPhysical )
{
	// We are only applying rotation:
	const Matrix34& refMat = RCC_MATRIX34(pPhysical->GetMatrixRef());
	const CoverOrientation eOrientation = GetUpAxis(refMat);
	if( eOrientation == CO_XUp || eOrientation == CO_YUp )
	{
		inout_vGlobalPos.RotateZ(-rage::Atan2f(-refMat.c.x, refMat.c.y));
	}
	else
	{
		// Check if the object is inverted
		Vector3 vPhysicalUp = VEC3V_TO_VECTOR3(pPhysical->GetMatrixRef().GetCol2());
		float fDotVertical = vPhysicalUp.GetUp();
		bool bInverted = fDotVertical <= 0.0f;
		// If inverted, apply opposite signed heading value to get desired rotation result in world space
		if( bInverted )
		{
			inout_vGlobalPos.RotateZ( pPhysical->GetTransform().GetHeading() );
		}
		else // apply heading rotation directly
		{
			inout_vGlobalPos.RotateZ( -pPhysical->GetTransform().GetHeading() );
		}
	}
}

//-------------------------------------------------------------------------
// Transforms the given position from the local coordinate reference frame
// of the given CPhysical into the corresponding game world coordinates
//-------------------------------------------------------------------------
inline void CCachedObjectCoverManager::TransformLocalToWorld( Vector3& inout_vLocalPos, const CPhysical* pPhysical )
{
	// Get the physical matrix in world space
	const Matrix34& refMat = RCC_MATRIX34(pPhysical->GetMatrixRef());
	// Use it to transform the given local space offset into world space
	refMat.Transform(inout_vLocalPos);
}

#if __BANK
inline void CCachedObjectCoverManager::DebugResetCache()
{
	for(int i=0; i < MAX_CACHED_OBJECT_COVER_POINTS; i++)
	{
		m_aCachedCoverPoints[i].DebugReset();
	}
}
#endif // __BANK

#endif
