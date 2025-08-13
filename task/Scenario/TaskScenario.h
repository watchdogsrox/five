// FILE :    TaskScenario.h
// PURPOSE : Creates spawning tasks for peds on foot and peds in cars
//			 based on scenariopoints around the world
// AUTHOR :  Phil H
// CREATED : 22-01-2007

#ifndef INC_TASK_SCENARIO_H
#define INC_TASK_SCENARIO_H

// framework headers
#include "fwnet/nettypes.h"

// game includes
#include "pathserver/PathServer_DataTypes.h"
#include "peds/PedType.h"
#include "peds/QueriableInterface.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/ScenarioPoint.h"
#include "Task/Scenario/Info/ScenarioTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "modelinfo/VehicleModelInfo.h"

// Forward declarations
class CScenarioInfo;
class CScenarioParkedVehicleInfo;

class CClonedScenarioFSMInfoBase
{
public:

	CClonedScenarioFSMInfoBase()
		: m_scenarioType(-1)
		, m_bIsWorldScenario(false)
		, m_ScenarioId(-1)
		, m_ScenarioPos(VEC3_ZERO)
	{
	}

	~CClonedScenarioFSMInfoBase(){}

	CClonedScenarioFSMInfoBase(s16 scenarioType, CScenarioPoint* pScenarioPoint, bool bIsWorldScenario, int scenarioPointIndex);

	s32				GetScenarioId() const		{ return m_ScenarioId; }
	s32				GetScenarioType() const		{ return m_scenarioType; }
	bool			GetIsWorldScenario() const	{ return m_bIsWorldScenario; }
	CScenarioPoint*	GetScenarioPoint();
	CScenarioPoint*	GetScenarioPoint(s32 scenarioId);
	Vector3&		GetScenarioPosition() { return m_ScenarioPos; }
	
	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_UNSIGNED(serialiser, m_scenarioType, SIZEOF_SCENARIO_TYPE, "Scenario type");
		SERIALISE_BOOL(serialiser, m_bIsWorldScenario, "Is world Scenario");

		if (m_bIsWorldScenario && !serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 uScenarioId = (u32)m_ScenarioId;
			SERIALISE_UNSIGNED(serialiser, uScenarioId, SIZEOF_SCENARIO_ID, "Scenario id");
			m_ScenarioId = (s32)uScenarioId;
		}
		else
		{
			SERIALISE_POSITION(serialiser, m_ScenarioPos, "Scenario position");
		}
	}

protected:

	static const int SIZEOF_SCENARIO_TYPE = 8;
	static const int SIZEOF_SCENARIO_ID = 32;	// Covers regionId in range 0xff000000, clusterId in range 0x00fff000 and pointId in range 0x00000fff  

	Vector3		m_ScenarioPos;			// the position of the Scenario (if m_bIsWorldScenario = false)
	s32			m_ScenarioId;			// the index into the world Scenario store (if m_bIsWorldScenario = true)
	s16			m_scenarioType;
	bool		m_bIsWorldScenario;		// if true, the Scenario sits independently in the world, if false it is associated with an entity
};

//-------------------------------------------------------------------------
// Task info for CTaskScenario
//-------------------------------------------------------------------------
class CClonedScenarioInfo : public CSerialisedFSMTaskInfo, public CClonedScenarioFSMInfoBase
{
public:
	CClonedScenarioInfo(s16 scenarioType, CScenarioPoint* pScenarioPoint, bool bIsWorldScenario, int scenarioPointIndex);
	CClonedScenarioInfo();
	~CClonedScenarioInfo() {}


	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_SCENARIO;}

	virtual CTaskFSMClone *CreateCloneFSMTask() { return NULL; }

	void Serialise(CSyncDataBase& serialiser);

protected:

	CClonedScenarioInfo(const CClonedScenarioInfo &);
	CClonedScenarioInfo &operator=(const CClonedScenarioInfo &);
};


//--------------------------------------------------------------------------------
// Declarations for Templated version for scenarios derived from CTaskScenario
//--------------------------------------------------------------------------------

class CClonedScenarioFSMInfo : public CSerialisedFSMTaskInfo, public CClonedScenarioFSMInfoBase
{
public:
	CClonedScenarioFSMInfo(s32 state, s16 scenarioType, CScenarioPoint* pScenarioPoint, bool bIsWorldScenario, int scenarioPointIndex) :
	CClonedScenarioFSMInfoBase( scenarioType,  pScenarioPoint,  bIsWorldScenario, scenarioPointIndex)
	{
		CSerialisedFSMTaskInfo::SetStatusFromMainTaskState(state);
	}

	CClonedScenarioFSMInfo()
	{
	}

	~CClonedScenarioFSMInfo() {}

	virtual CTaskFSMClone *CreateCloneFSMTask() { return NULL; }

	void Serialise(CSyncDataBase& serialiser);
};

// info for scenarios that are not cloned over the network  
class CNonNetworkedClonedScenarioInfo : public CClonedScenarioInfo
{
public:

	CNonNetworkedClonedScenarioInfo(s16 scenarioType, CScenarioPoint* pScenarioPoint, bool bIsWorldScenario, int scenarioPointIndex) :
		CClonedScenarioInfo(scenarioType, pScenarioPoint, bIsWorldScenario, scenarioPointIndex)
	{
	}

	virtual bool HasNetSequences() { return false; }
};

//-------------------------------------------------------------------------
// Base scenario class
//-------------------------------------------------------------------------
class CTaskScenario : public CTaskFSMClone
{
public:

	CTaskScenario( s32 scenarioType, CScenarioPoint* pScenarioPoint, bool bManageUseCount=true );
	CTaskScenario( const CTaskScenario& other);
	virtual ~CTaskScenario();

	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;

	// Scenario component accessors
    virtual CScenarioPoint*	GetScenarioPoint() const	{ return m_pScenarioPoint;  }
	virtual int GetScenarioPointType() const			{ return m_iScenarioIndex;	}
	s32				GetScenarioType() const				{ return m_iScenarioIndex;	}
	CEntity*		GetScenarioEntity() const			{ return (m_pScenarioPoint) ? m_pScenarioPoint->GetEntity() : NULL; }
	const CScenarioInfo& GetScenarioInfo() const;
	
	// Functions for checking the validity of scenario info
	bool			GetHadValidEntity() const			{ return m_TaskFlags.IsFlagSet(TS_HadValidEntity); }
	bool			GetIsWaitingForEntity() const		{ return m_TaskFlags.IsFlagSet(TS_WaitingForEntity); }
	virtual bool	IsTaskValidForChatting() const		{ return true; }
	bool			ValidateScenarioInfo() const		{ return !m_TaskFlags.IsFlagSet(TS_HadValidEntity) || (m_pScenarioPoint && m_pScenarioPoint->GetEntity()); }

	// Get or set the "is world Scenario" property
	bool			GetIsWorldScenario() const			{ return m_TaskFlags.IsFlagSet(TS_IsWorldScenario); }
	void			SetIsWorldScenario(bool b)			{ m_TaskFlags.ChangeFlag(TS_IsWorldScenario, b); }

	void			CreateProp()						{ m_TaskFlags.SetFlag(TS_CreateProp); }
	void			SetCreateProp(bool b)				{ m_TaskFlags.ChangeFlag(TS_CreateProp, b); }
	bool			GetCreateProp() const				{ return m_TaskFlags.IsFlagSet(TS_CreateProp); }

	// Interface for setting when the ped leaves.
	virtual void SetTimeToLeave(float UNUSED_PARAM(fTime))	{ }

	// Remove/set the current pScenarioPoint
    void            SetScenarioPoint(CScenarioPoint* pScenarioPoint);
    void            RemoveScenarioPoint();

	// Returns the appropriate ambient flags
	virtual u32	GetAmbientFlags();

	// Task updates that are called before the FSM update
	virtual FSM_Return			ProcessPreFSM	();

	void	CloneFindScenarioPoint();
	bool	CloneCheckScenarioPointValidAndGetPointer(CScenarioPoint** ppScenarioPoint);

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32, const FSM_Event) { return FSM_Quit; } 

	void ClearCachedWorldScenarioPointIndex() { m_iCachedWorldScenarioPointIndex = -1; }
#if __BANK
	void ValidateScenarioType(s32 iScenarioType);
#endif
protected:
	// Get the index of m_pScenarioPoint in the point manager, if it's a world point. Will use a previously cached result if possible.
	int GetWorldScenarioPointIndex() const;
	int GetWorldScenarioPointIndex(CScenarioPoint* pScenarioPoint, s32 *pCacheIndex = NULL, bool bAssertOnWorldPointWithEntity=true ) const;
	int GetCachedWorldScenarioPointIndex() const { return m_iCachedWorldScenarioPointIndex; }
	void SetCachedWorldScenarioPointIndex(int worldScenarioPointIndex)  { m_iCachedWorldScenarioPointIndex = worldScenarioPointIndex; }

	s32	m_iScenarioIndex;
private:
	enum 
	{
		TS_HadValidEntity		= BIT0,
		TS_IsWorldScenario		= BIT1,
		TS_WaitingForEntity		= BIT2,
		TS_CreateProp			= BIT3,
		TS_ManageUseCount		= BIT4,
	};

	void SetHadValidEntity(bool bOnoff)		{ m_TaskFlags.ChangeFlag(TS_HadValidEntity, bOnoff); }
	void SetWaitingForEntity(bool bOnoff)	{ m_TaskFlags.ChangeFlag(TS_WaitingForEntity, bOnoff); } 

	static const int WAITING_FOR_ENTITY_TIME = 20;

    RegdScenarioPnt m_pScenarioPoint;
	mutable s32		m_iCachedWorldScenarioPointIndex;	// Index of m_pScenarioPoint in SCENARIOPOINTMGR, or -1 if not computed or not a world point.
	u8				m_waitingForEntityTimer;
	fwFlags8		m_TaskFlags;
};

#endif // INC_TASK_SCENARIO_H
