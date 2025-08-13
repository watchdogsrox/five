// Title	:	DispatchHelpers.h
// Purpose	:	Defines the different dispatch helpers
// Started	:	2/22/2012
#ifndef INC_DISPATCH_HELPERS_H_
#define INC_DISPATCH_HELPERS_H_

// Rage headers
#include "ai/navmesh/datatypes.h"
#include "atl/array.h"
#include "fwutil/Flags.h"
#include "fwvehicleai/pathfindtypes.h"

// Game headers
#include "debug/DebugDrawStore.h"
#include "game/Wanted.h"
#include "Peds/Area.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/TaskHelperFSM.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CBoat;
class CDispatchSpawnHelpers;
class CPed;
class CVehicle;

////////////////////////////////////////////////////////////////////////////////
// CDispatchSpawnProperties
////////////////////////////////////////////////////////////////////////////////

class CDispatchSpawnProperties
{

private:

	enum Flags
	{
		F_HasLocation			= BIT0,
		F_HasIdealSpawnDistance	= BIT1,
	};

private:

	CDispatchSpawnProperties();
	~CDispatchSpawnProperties();
	
public:

	static CDispatchSpawnProperties& GetInstance();
	
public:

	float		GetIdealSpawnDistance()	const { return m_fIdealSpawnDistance; }
	Vec3V_Out	GetLocation()			const { return m_vLocation; }

public:

	bool HasIdealSpawnDistance() const
	{
		return m_uFlags.IsFlagSet(F_HasIdealSpawnDistance);
	}

	bool HasLocation() const
	{
		return m_uFlags.IsFlagSet(F_HasLocation);
	}

	void ResetIdealSpawnDistance()
	{
		m_uFlags.ClearFlag(F_HasIdealSpawnDistance);
	}

	void ResetLocation()
	{
		m_uFlags.ClearFlag(F_HasLocation);
	}

	void SetIdealSpawnDistance(float fIdealSpawnDistance)
	{
		m_fIdealSpawnDistance = fIdealSpawnDistance;

		m_uFlags.SetFlag(F_HasIdealSpawnDistance);
	}

	void SetLocation(Vec3V_In vLocation)
	{
		m_vLocation = vLocation;

		m_uFlags.SetFlag(F_HasLocation);
	}
	
public:

	int		AddAngledBlockingArea(Vec3V_In vStart, Vec3V_In vEnd, const float fWidth);
	int		AddSphereBlockingArea(Vec3V_In vCenter, const float fRadius);
	bool	IsPositionInBlockingArea(Vec3V_In vPosition) const;
	void	RemoveBlockingArea(int iIndex);
	void	Reset();
	void	ResetBlockingAreas();
	
public:

#if __BANK
	void RenderDebug();
#endif

private:

	static const int sNumBlockingAreas = 10;

private:

	atRangeArray<CArea, sNumBlockingAreas>	m_aBlockingAreas;
	Vec3V									m_vLocation;
	float									m_fIdealSpawnDistance;
	fwFlags8								m_uFlags;

public:

	static bool sm_bRenderDebug;

};

////////////////////////////////////////////////////////////////////////////////
// CDispatchSpawnHelper
////////////////////////////////////////////////////////////////////////////////

class CDispatchSpawnHelper
{

private:

	friend class CDispatchSpawnHelpers;

public:

	enum Type
	{
		Basic,
		Advanced,
	};

private:

	enum RunningFlags
	{
		RF_IsActive					= BIT0,
		RF_FailedToFindDispatchNode	= BIT1,
	};
	
public:

	struct Tunables : CTuning
	{
		struct Rendering
		{
			Rendering()
			{}

			bool m_Enabled;
			bool m_DispatchNode;
			bool m_FindSpawnPointInDirection;
			bool m_IncidentLocation;

			PAR_SIMPLE_PARSABLE;
		};

		struct Restrictions
		{
			Restrictions()
			{}

			float	m_MaxDistanceFromCameraForViewportChecks;
			float	m_RadiusForViewportCheck;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		Restrictions	m_Restrictions;
		Rendering		m_Rendering;
		float			m_IdealSpawnDistance;
		float			m_MinDotForInFront;
		float			m_MaxDistanceTraveledMultiplier;
		float			m_MinSpeedToBeConsideredEscapingInVehicle;
		float			m_MaxDistanceForDispatchPosition;

		PAR_PARSABLE;
	};

protected:

	struct SpawnPoint
	{
		SpawnPoint()
		: m_Node()
		, m_PreviousNode()
		{}
		
		void Reset()
		{
			m_Node.SetEmpty();
			m_PreviousNode.SetEmpty();
		}

		CNodeAddress	m_Node;
		CNodeAddress	m_PreviousNode;
	};

public:

	CDispatchSpawnHelper();
	virtual ~CDispatchSpawnHelper();
	
public:

	const CNodeAddress& GetDispatchNode()	const { return m_DispatchNode; }
	const CIncident*	GetIncident()		const { return m_pIncident; }
	
public:

	bool			CanGenerateRandomDispatchPosition() const;
	bool			CanSpawnAtPosition(Vec3V_In vPosition) const;
	bool			CanSpawnBoat() const;
	bool			CanSpawnVehicle() const;
	bool			CanSpawnVehicleInFront() const;
	bool			GenerateRandomDispatchPosition(Vec3V_InOut vPosition) const;
	const CEntity*	GetEntity() const;
	float			GetIdealSpawnDistance() const;
	const CPed*		GetPed() const;
	bool			IsClosestRoadNodeWithinDistance(float fMaxDistance) const;
	CBoat*			SpawnBoat(fwModelId iModelId);
	CVehicle*		SpawnVehicle(fwModelId iModelId, bool bDisableSwitchedOffNodes = false);
	CVehicle*		SpawnVehicleInFront(fwModelId iModelId, const bool bPulledOver);

public:

	enum FindSpawnPointInDirectionFlags
	{
		FSPIDF_CanFollowOutgoingLinks = BIT0,
	};

	static bool FindSpawnPointInDirection(Vec3V_In vPosition, Vec3V_In vDirection, float fIdealSpawnDistance, fwFlags32 uFlags, Vec3V_InOut vSpawnPoint);
	static bool FindSpawnPointInDirection(const CNodeAddress& rNodeAddress, const Vector2& vDirection, float fIdealSpawnDistance, fwFlags32 uFlags, SpawnPoint& rSpawnPoint);
	
public:

	virtual bool FailedToFindSpawnPoint() const;

public:

	virtual Type GetType() const = 0;

public:

#if __BANK
	virtual void RenderDebug();
#endif
	
protected:

	bool			CanFindSpawnPointInDirection() const;
	bool 			FindSpawnPointInDirection(const Vector2& vDirection, SpawnPoint& rSpawnPoint) const;
	Vec3V_Out		GetDirection() const;
	Vec3V_Out		GetLocation() const;
	Vec3V_Out		GetLocationForNearestCarNode() const;
	CVehicle*		GetVehiclePedEscapingIn() const;
	const CWanted*	GetWanted() const;
	bool			IsPedEscapingInVehicle() const;
	
protected:

	virtual bool				CanChooseBestSpawnPoint() const = 0;
	virtual const SpawnPoint*	ChooseBestSpawnPoint() = 0;
	virtual const SpawnPoint*	ChooseNextBestSpawnPoint() = 0;
	virtual void				OnActivate();
	virtual void				OnDeactivate();
	virtual void				OnDispatchNodeChanged();
	virtual void				OnVehicleSpawned(CVehicle& rVehicle);
	virtual void				Update(float fTimeStep);

private:

	bool FailedToFindDispatchNode() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_FailedToFindDispatchNode));
	}

	bool IsActive() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_IsActive));
	}

	bool ShouldBeActive() const
	{
		return (m_pIncident != NULL);
	}

private:

	void Activate(const CIncident* pIncident);
	void Deactivate();
	bool ShouldUpdateSearchPositionForFindNearestCarNodeHelper() const;
	void UpdateFindNearestCarNodeHelper();
	void UpdateSearchPositionForFindNearestCarNodeHelper();
	void UpdateTimers(float fTimeStep);

protected:

	CFindNearestCarNodeHelper	m_FindNearestCarNodeHelper;
	CNodeAddress				m_DispatchNode;
	RegdConstIncident			m_pIncident;
	float						m_fTimeSinceSearchPositionForFindNearestCarNodeHelperWasUpdated;
	fwFlags8					m_uRunningFlags;
	
private:

	static Tunables sm_Tunables;

};

////////////////////////////////////////////////////////////////////////////////
// CDispatchBasicSpawnHelper
////////////////////////////////////////////////////////////////////////////////

class CDispatchBasicSpawnHelper : public CDispatchSpawnHelper
{

public:

	CDispatchBasicSpawnHelper();
	virtual ~CDispatchBasicSpawnHelper();

private:

	virtual Type GetType() const
	{
		return Basic;
	}
	
private:

	virtual bool				CanChooseBestSpawnPoint() const;
	virtual const SpawnPoint*	ChooseBestSpawnPoint();
	virtual const SpawnPoint*	ChooseNextBestSpawnPoint();
	virtual void				OnDeactivate();
	
private:

	SpawnPoint	m_SpawnPoint;
	
};

////////////////////////////////////////////////////////////////////////////////
// CDispatchAdvancedSpawnHelper
////////////////////////////////////////////////////////////////////////////////

class CDispatchAdvancedSpawnHelper : public CDispatchSpawnHelper
{

public:

	enum AdvancedRunningFlags
	{
		ARF_IsReady	= BIT0
	};

public:

	struct AdvancedSpawnPoint
	{
		AdvancedSpawnPoint()
		: m_SpawnPoint()
		, m_fScore(0.0f)
		, m_bValid(false)
		, m_bVisited(false)
		{}

		SpawnPoint	m_SpawnPoint;
		float		m_fScore;
		bool		m_bValid;
		bool		m_bVisited;
	};

	struct DispatchVehicle
	{
		enum ConfigFlags
		{
			CF_CantDespawn		= BIT0,
			CF_CanHaveNoDriver	= BIT1,
		};
		
		enum RunningFlags
		{
			RF_Valid	= BIT0,
		};
		
		DispatchVehicle()
		: m_pVehicle(NULL)
		, m_uConfigFlags(0)
		, m_uRunningFlags(0)
		{}
		
		bool GetCanHaveNoDriver() const
		{
			return m_uConfigFlags.IsFlagSet(CF_CanHaveNoDriver);
		}

		bool GetCantDespawn() const
		{
			return m_uConfigFlags.IsFlagSet(CF_CantDespawn);
		}
		
		bool GetIsValid() const
		{
			return m_uRunningFlags.IsFlagSet(RF_Valid);
		}
		
		void Invalidate()
		{
			m_uRunningFlags.ClearFlag(RF_Valid);
		}
		
		void Set(CVehicle* pVehicle, u8 uConfigFlags)
		{
			//Set the vehicle.
			m_pVehicle = pVehicle;
			
			//Set the config flags.
			m_uConfigFlags = uConfigFlags;
			
			//Set the running flags.
			m_uRunningFlags = RF_Valid;
		}

		RegdVeh		m_pVehicle;
		fwFlags8	m_uConfigFlags;
		fwFlags8	m_uRunningFlags;
	};
	
	struct Tunables : CTuning
	{
		struct Rendering
		{
			Rendering()
			{}

			bool	m_Enabled;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();
		
		Rendering	m_Rendering;
		float		m_TimeBetweenInvalidateInvalidDispatchVehicles;
		float		m_TimeBetweenMarkDispatchVehiclesForDespawn;

		PAR_PARSABLE;
	};

public:

	CDispatchAdvancedSpawnHelper();
	virtual ~CDispatchAdvancedSpawnHelper();

public:

	bool IsReady() const
	{
		return (m_uAdvancedRunningFlags.IsFlagSet(ARF_IsReady));
	}
	
public:

	void TrackDispatchVehicle(CVehicle& rVehicle, u8 uConfigFlags = 0);
	
public:

#if __BANK
	virtual void RenderDebug();
#endif

private:

	bool		CanChooseNextBestSpawnPoint() const;
	bool		CanFindNextSpawnPoint() const;
	bool		CanScoreSpawnPoints() const;
	ScalarV_Out	FindDistSqToClosestDispatchVehicle(Vec3V_In vPosition) const;
	void		FindNextSpawnPoint();
	void		FindSpawnPoint(const u8 uIndex);
	void		ForceDespawn(CVehicle& rVehicle);
	bool		HasValidSpawnPoint() const;
	void		InvalidateDispatchVehicles();
	void		InvalidateInvalidDispatchVehicles();
	void		InvalidateSpawnPoints();
	bool		IsWithinRangeOfDispatchVehicles(const CVehicle& rVehicle, float fRadius, u32 uCount) const;
	void		MarkDispatchVehiclesForDespawn();
	void		OnDispatchVehiclesMarkedForDespawn();
	void		OnInvalidDispatchVehiclesInvalidated();
	void		ScoreSpawnPoints();
	bool		ShouldInvalidateInvalidDispatchVehicles() const;
	bool		ShouldMarkDispatchVehiclesForDespawn() const;
	void		UpdateAdvancedTimers(const float fTimeStep);
	void		UpdateSpawnPoints();
	
private:

#if __BANK
	void CacheDebug();
#endif

private:

	virtual Type GetType() const
	{
		return Advanced;
	}

private:

	virtual bool				CanChooseBestSpawnPoint() const;
	virtual const SpawnPoint*	ChooseBestSpawnPoint();
	virtual const SpawnPoint*	ChooseNextBestSpawnPoint();
	virtual bool				FailedToFindSpawnPoint() const;
	virtual void				OnDeactivate();
	virtual void				OnVehicleSpawned(CVehicle& rVehicle);
	virtual void				Update(float fTimeStep);

private:

	static const int sNumSpawnPoints = 16;
	static const int sNumDispatchVehicles = 16;

private:

	atRangeArray<AdvancedSpawnPoint, sNumSpawnPoints>	m_aSpawnPoints;
	atRangeArray<DispatchVehicle, sNumDispatchVehicles>	m_aDispatchVehicles;
	float												m_fTimeSinceLastInvalidDispatchVehiclesInvalidated;
	float												m_fTimeSinceLastDispatchVehiclesMarkedForDespawn;
	float												m_fTimeSinceLastValidSpawnPointWasFound;
	fwFlags8											m_uAdvancedRunningFlags;
	u8													m_uIndex;
	
private:

	static Tunables sm_Tunables;

private:

#if __BANK
	static CDebugDrawStore ms_DebugDrawStore;
#endif

};

////////////////////////////////////////////////////////////////////////////////
// CDispatchSpawnHelpers
////////////////////////////////////////////////////////////////////////////////

class CDispatchSpawnHelpers
{

public:

	CDispatchSpawnHelpers();
	~CDispatchSpawnHelpers();
	
public:

	CDispatchSpawnHelper*	FindSpawnHelperForEntity(const CEntity& rEntity);
	CDispatchSpawnHelper*	FindSpawnHelperForIncident(const CIncident& rIncident);
	void					Update(float fTimeStep);
	
public:

#if __BANK
	void RenderDebug();
#endif
	
private:

	void							DeactivateInvalidSpawnHelpers();
	CDispatchAdvancedSpawnHelper*	FindInactiveAdvancedSpawnHelper();
	CDispatchBasicSpawnHelper*		FindInactiveBasicSpawnHelper();
	void							SetIncidentsForSpawnHelpers();
	bool							ShouldIncidentUseAdvancedSpawnHelper(const CIncident& rIncident) const;
	void							UpdateSpawnHelpers(float fTimeStep);
	
private:

	static const int sMaxAdvancedSpawnHelpers = 3;
	static const int sMaxBasicSpawnHelpers = 64;
	
private:

	atRangeArray<CDispatchAdvancedSpawnHelper, sMaxAdvancedSpawnHelpers>	m_aAdvancedSpawnHelpers;
	atRangeArray<CDispatchBasicSpawnHelper, sMaxBasicSpawnHelpers>			m_aBasicSpawnHelpers;
	
};

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperSearch
////////////////////////////////////////////////////////////////////////////////

class CDispatchHelperSearch
{

public:

	struct Constraints
	{
		Constraints()
		{}

		float CalculateMaxRadius(float fTimeSinceLastSpotted) const
		{
			fTimeSinceLastSpotted = Clamp(fTimeSinceLastSpotted, m_MinTimeSinceLastSpotted, m_MaxTimeSinceLastSpotted);
			return Lerp((fTimeSinceLastSpotted - m_MinTimeSinceLastSpotted) / (m_MaxTimeSinceLastSpotted - m_MinTimeSinceLastSpotted),
				m_MaxRadiusForMinTimeSinceLastSpotted, m_MaxRadiusForMaxTimeSinceLastSpotted);
		}
		
		float	m_MinTimeSinceLastSpotted;
		float	m_MaxTimeSinceLastSpotted;
		float	m_MaxRadiusForMinTimeSinceLastSpotted;
		float	m_MaxRadiusForMaxTimeSinceLastSpotted;
		float	m_MaxHeight;
		float	m_MaxAngle;
		bool	m_UseLastSeenPosition;
		bool	m_UseByDefault;
		bool	m_UseEnclosedSearchRegions;
		
		PAR_SIMPLE_PARSABLE;
	};

public:

	CDispatchHelperSearch();
	~CDispatchHelperSearch();
	
public:

	static bool Calculate(const CPed& rPed, const CPed& rTarget, const atArray<Constraints>& rConstraints, Vec3V_InOut rPosition);

private:

	static Vec3V_Out			CalculateSearchCenter(const CPed& rPed, const CPed& rTarget, const Constraints& rConstraints);
	static float				CalculateTimeSincePedWasLastSpotted(const CPed& rPed);
	static const Constraints*	ChooseConstraints(const atArray<Constraints>& rConstraints, float fTimeSinceLastSpotted);
	
};

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperSearchOnFoot
////////////////////////////////////////////////////////////////////////////////

class CDispatchHelperSearchOnFoot
{

public:

	struct Tunables : CTuning
	{
		Tunables();
		
		atArray<CDispatchHelperSearch::Constraints>	m_Constraints;
		float										m_MaxDistanceFromNavMesh;
		
		PAR_PARSABLE;
	};

public:

	CDispatchHelperSearchOnFoot();
	~CDispatchHelperSearchOnFoot();

public:

	bool HasStarted() const
	{
		return m_NavmeshClosestPositionHelper.IsSearchActive();
	}

	void Reset()
	{
		m_NavmeshClosestPositionHelper.ResetSearch();
	}

public:

	bool IsResultReady(bool& bWasSuccessful, Vec3V_InOut vPosition);
	bool Start(const CPed& rPed, const CPed& rTarget);
	
public:

	static bool		Calculate(const CPed& rPed, const CPed& rTarget, Vec3V_InOut rPosition);
	static float	GetMoveBlendRatio(const CPed& rPed);

private:

	CNavmeshClosestPositionHelper m_NavmeshClosestPositionHelper;
	
private:

	static Tunables sm_Tunables;
	
};

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperSearchInAutomobile
////////////////////////////////////////////////////////////////////////////////

class CDispatchHelperSearchInAutomobile
{

public:

	struct Tunables : CTuning
	{
		Tunables();

		atArray<CDispatchHelperSearch::Constraints>	m_Constraints;
		float										m_MaxDistanceFromRoadNode;
		float										m_CruiseSpeed;

		PAR_PARSABLE;
	};

public:

	CDispatchHelperSearchInAutomobile();
	~CDispatchHelperSearchInAutomobile();

public:

	static float GetCruiseSpeed()
	{
		return sm_Tunables.m_CruiseSpeed;
	}

public:

	static bool			Calculate(const CPed& rPed, const CPed& rTarget, Vec3V_InOut rPosition);
	static fwFlags32	GetDrivingFlags();

private:

	static Tunables sm_Tunables;

};

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperSearchInBoat
////////////////////////////////////////////////////////////////////////////////

class CDispatchHelperSearchInBoat
{

public:

	struct Tunables : CTuning
	{
		Tunables();

		atArray<CDispatchHelperSearch::Constraints>	m_Constraints;
		float										m_CruiseSpeed;

		PAR_PARSABLE;
	};

public:

	CDispatchHelperSearchInBoat();
	~CDispatchHelperSearchInBoat();

	static float GetCruiseSpeed()
	{
		return sm_Tunables.m_CruiseSpeed;
	}

public:

	static bool Calculate(const CPed& rPed, const CPed& rTarget, Vec3V_InOut rPosition);

private:

	static Tunables sm_Tunables;

};

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperSearchInHeli
////////////////////////////////////////////////////////////////////////////////

class CDispatchHelperSearchInHeli
{

public:

	struct Tunables : CTuning
	{
		Tunables();

		atArray<CDispatchHelperSearch::Constraints>	m_Constraints;

		PAR_PARSABLE;
	};

public:

	CDispatchHelperSearchInHeli();
	~CDispatchHelperSearchInHeli();

public:

	static bool Calculate(const CPed& rPed, const CPed& rTarget, Vec3V_InOut rPosition);

private:

	static Tunables sm_Tunables;

};

////////////////////////////////////////////////////////////////////////////////
// CDispatchHelperVolumes
////////////////////////////////////////////////////////////////////////////////

class CDispatchHelperVolumes
{

public:

	struct Tunables : CTuning
	{
		struct Rendering
		{
			Rendering()
			{}

			bool m_Enabled;
			bool m_LocationForNearestCarNodeOverrides;
			bool m_EnclosedSearchRegions;
			bool m_BlockingAreas;

			PAR_SIMPLE_PARSABLE;
		};

		struct Position
		{
			Position()
			{}

			Vec3V_Out ToVec3V() const
			{
				return Vec3V(m_X, m_Y, m_Z);
			}

			Vector3 ToVector3() const
			{
				return VEC3V_TO_VECTOR3(ToVec3V());
			}

			float m_X;
			float m_Y;
			float m_Z;

			PAR_SIMPLE_PARSABLE;
		};

		struct AngledArea
		{
			AngledArea()
			{}

			Position	m_Start;
			Position	m_End;
			float		m_Width;

			PAR_SIMPLE_PARSABLE;
		};

		struct AngledAreaWithPosition
		{
			AngledAreaWithPosition()
			{}

			AngledArea	m_AngledArea;
			Position	m_Position;

			PAR_SIMPLE_PARSABLE;
		};

		struct Sphere
		{
			Sphere()
			{}

			Position	m_Position;
			float		m_Radius;

			PAR_SIMPLE_PARSABLE;
		};

		struct SphereWithPosition
		{
			SphereWithPosition()
			{}

			Sphere		m_Sphere;
			Position	m_Position;

			PAR_SIMPLE_PARSABLE;
		};

		struct AreaWithPosition
		{
			AreaWithPosition()
			{}

			CArea		m_Area;
			Position	m_Position;
		};

		struct LocationForNearestCarNodeOverrides
		{
			LocationForNearestCarNodeOverrides()
			{}

			void Finalize()
			{
				m_Areas.Reset();
				m_Areas.Reserve(m_AngledAreas.GetCount() + m_Spheres.GetCount());
				for(int i = 0; i < m_AngledAreas.GetCount(); ++i)
				{
					AreaWithPosition& rArea = m_Areas.Append();
					rArea.m_Area.Set(m_AngledAreas[i].m_AngledArea.m_Start.ToVector3(),
						m_AngledAreas[i].m_AngledArea.m_End.ToVector3(), m_AngledAreas[i].m_AngledArea.m_Width);
					rArea.m_Position = m_AngledAreas[i].m_Position;
				}
				for(int i = 0; i < m_Spheres.GetCount(); ++i)
				{
					AreaWithPosition& rArea = m_Areas.Append();
					rArea.m_Area.SetAsSphere(m_Spheres[i].m_Sphere.m_Position.ToVector3(), m_Spheres[i].m_Sphere.m_Radius);
					rArea.m_Position = m_Spheres[i].m_Position;
				}
			}

			Vec3V_Out Transform(Vec3V_In vLocation) const;

#if __BANK
			void RenderDebug();
#endif

			atArray<AngledAreaWithPosition>	m_AngledAreas;
			atArray<SphereWithPosition>		m_Spheres;

			atArray<AreaWithPosition> m_Areas;

			PAR_SIMPLE_PARSABLE;
		};

		enum EnclosedSearchRegionFlags
		{
			UseAABB = BIT0,
		};

		struct EnclosedSearchAngledArea
		{
			EnclosedSearchAngledArea()
			{}

			AngledArea	m_AngledArea;
			fwFlags32	m_Flags;

			PAR_SIMPLE_PARSABLE;
		};

		struct EnclosedSearchSphere
		{
			EnclosedSearchSphere()
			{}

			Sphere		m_Sphere;
			fwFlags32	m_Flags;

			PAR_SIMPLE_PARSABLE;
		};

		struct EnclosedSearchRegion
		{
			EnclosedSearchRegion()
			{}

			bool ContainsPosition(Vec3V_In vPosition) const
			{
				if(!m_uFlags.IsFlagSet(UseAABB))
				{
					return (m_Area.IsPointInArea(VEC3V_TO_VECTOR3(vPosition)));
				}
				else
				{
					return (m_AABB.ContainsPoint(vPosition));
				}
			}

			float GetRadius() const
			{
				if(!m_uFlags.IsFlagSet(UseAABB))
				{
					float fRadius = LARGE_FLOAT;
					m_Area.GetMaxRadius(fRadius);
					return fRadius;
				}
				else
				{
					return (m_AABB.GetBoundingSphere().GetRadius().Getf());
				}
			}

			CArea		m_Area;
			spdAABB		m_AABB;
			fwFlags32	m_uFlags;
		};

		struct EnclosedSearchRegions
		{
			EnclosedSearchRegions()
			{}

			void Finalize()
			{
				m_Areas.Reset();
				m_Areas.Reserve(m_AngledAreas.GetCount() + m_Spheres.GetCount());
				for(int i = 0; i < m_AngledAreas.GetCount(); ++i)
				{
					EnclosedSearchRegion& rArea = m_Areas.Append();
					rArea.m_Area.Set(m_AngledAreas[i].m_AngledArea.m_Start.ToVector3(), m_AngledAreas[i].m_AngledArea.m_End.ToVector3(), m_AngledAreas[i].m_AngledArea.m_Width);
					rArea.m_uFlags = m_AngledAreas[i].m_Flags;
				}
				for(int i = 0; i < m_Spheres.GetCount(); ++i)
				{
					EnclosedSearchRegion& rArea = m_Areas.Append();
					rArea.m_Area.SetAsSphere(m_Spheres[i].m_Sphere.m_Position.ToVector3(), m_Spheres[i].m_Sphere.m_Radius);
					rArea.m_uFlags = m_Spheres[i].m_Flags;
				}
			}

			const EnclosedSearchRegion* Get(const CPed& rTarget) const;
			const EnclosedSearchRegion* Get(const CPed& rTarget, Vec3V_In vPosition) const;
			const EnclosedSearchRegion* GetInterior(const CPed& rTarget) const;

#if __BANK
			void RenderDebug();
#endif

			atArray<EnclosedSearchAngledArea>	m_AngledAreas;
			atArray<EnclosedSearchSphere>		m_Spheres;

			atArray<EnclosedSearchRegion> m_Areas;

			PAR_SIMPLE_PARSABLE;
		};

		struct BlockingAreas
		{
			BlockingAreas()
			{}

			void Finalize()
			{
				m_Areas.Reset();
				m_Areas.Reserve(m_AngledAreas.GetCount() + m_Spheres.GetCount());
				for(int i = 0; i < m_AngledAreas.GetCount(); ++i)
				{
					CArea& rArea = m_Areas.Append();
					rArea.Set(m_AngledAreas[i].m_Start.ToVector3(),
						m_AngledAreas[i].m_End.ToVector3(), m_AngledAreas[i].m_Width);
				}
				for(int i = 0; i < m_Spheres.GetCount(); ++i)
				{
					CArea& rArea = m_Areas.Append();
					rArea.SetAsSphere(m_Spheres[i].m_Position.ToVector3(), m_Spheres[i].m_Radius);
				}
			}

			bool Contains(Vec3V_In vPosition) const;

#if __BANK
			void RenderDebug();
#endif

			atArray<AngledArea>	m_AngledAreas;
			atArray<Sphere>		m_Spheres;

			atArray<CArea> m_Areas;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		virtual void Finalize()
		{
			m_LocationForNearestCarNodeOverrides.Finalize();
			m_EnclosedSearchRegions.Finalize();
			m_BlockingAreas.Finalize();
		}

		Rendering							m_Rendering;
		LocationForNearestCarNodeOverrides	m_LocationForNearestCarNodeOverrides;
		EnclosedSearchRegions				m_EnclosedSearchRegions;
		BlockingAreas						m_BlockingAreas;

		PAR_PARSABLE;
	};

public:

	static const Tunables& GetTunables() { return sm_Tunables; }

public:

#if __BANK
	static void RenderDebug();
#endif

private:

	static Tunables sm_Tunables;

};

/////////////////////////////////////////////////////////////////////////////////////

class CDispatchHelperForIncidentLocationOnFoot
{

public:

	CDispatchHelperForIncidentLocationOnFoot()
	{}

public:

	bool HasStarted() const
	{
		return m_NavmeshClosestPositionHelper.IsSearchActive();
	}

	void Reset()
	{
		m_NavmeshClosestPositionHelper.ResetSearch();
	}

public:

	bool IsResultReady(bool& bWasSuccessful, Vec3V_InOut vPosition);
	bool Start(const CPed& rPed);

public:

	static bool Calculate(const CPed& rPed, Vec3V_InOut vPosition);

private:

	static Vec3V_Out GetPositionForPed(const CPed& rPed);

private:

	CNavmeshClosestPositionHelper m_NavmeshClosestPositionHelper;

};

/////////////////////////////////////////////////////////////////////////////////////

class CDispatchHelperForIncidentLocationInVehicle
{

public:

	static float		GetCruiseSpeed(const CPed& rPed);
	static fwFlags32	GetDrivingFlags();
	static float		GetTargetArriveDist();

};

/////////////////////////////////////////////////////////////////////////////////////

struct CDispatchHelperPedPositionToUse
{
	static Vec3V_Out Get(const CPed& rPed);
};

/////////////////////////////////////////////////////////////////////////////////////

#endif
