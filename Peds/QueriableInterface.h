// Title	:	QueriableInterface.h
// Author	:	Phil Hooker
// Started	:	25/07/06
//
// Implements a small queriable set of ped intelligence interface, thats compact enough to be sent between
// machines during network games.  All external ped queries in the game should be using this interface.

#ifndef QUERIABLE_INTERFACE_H
#define QUERIABLE_INTERFACE_H

// Framework headers
#include "fwnet/netserialisers.h"

// Game headers
//#include "animation\AnimIds.h"
#include "modelinfo/VehicleModelInfo.h"
#include "network/NetworkInterface.h"
#include "network/General/NetworkSerialisers.h"
#include "scene/RegdRefTypes.h"
#include "fwtl/pool.h"
#include "Script/ScriptTaskTypes.h"
#include "Task/System/TaskManager.h"
#include "task/system/TaskTypes.h"
#include "Task/System/TaskClassInfo.h"
#include "fwtl/pool.h"
#include "peds/pedDefines.h"
#include "network/general/NetworkUtil.h"
#include "system/poolallocator.h"

#define MAX_TASK_INFOS_PER_PED (8)

// If you are changing the number of peds for a __CONSOLE platform,
// also need to increase TASKINFO_HEAP_SIZE in PedFactory.cpp
#define MAX_NUM_TASK_INFOS (((RSG_PC || RSG_DURANGO || RSG_ORBIS) ? 268 : 115) * MAX_TASK_INFOS_PER_PED)

class CEntity;
class CTask;
class CTaskTree;
class CPhysical;
class CPed;
class CScenarioPoint;

namespace rage
{
    class datBitBuffer;
    class netLoggingInterface;
}

//-------------------------------------------------------------------------
// Keeps track of a persistent sequence ID for a particular task type
// running on an instance of a queriable interface. This is sent over the
// network so remote clients can determine whether tasks received have
// already by run as clone tasks
//-------------------------------------------------------------------------
struct TaskSequenceInfo
{
    TaskSequenceInfo(u32 taskType);

    FW_REGISTER_CLASS_POOL(TaskSequenceInfo);

    u32               m_taskType;           // task type represented by task
    u32               m_sequenceID;         // sequence ID for this task type for this ped
    TaskSequenceInfo *m_nextSequenceInfo;   // pointer to next item in this linked list

#if __BANK 
	static void PoolFullCallback(void* pItem);
#endif // __DEV

private:

    TaskSequenceInfo();
};

const unsigned INVALID_TASK_SEQUENCE_ID		= ~0u;
const unsigned SIZEOF_TASK_SEQUENCE_ID		= 5; 
const unsigned MAX_TASK_SEQUENCE_ID			= (1<<SIZEOF_TASK_SEQUENCE_ID)-1; 

//-------------------------------------------------------------------------
// Stores the state information about a single task, includes a task type
// and a union of data to represent any additional state information
//-------------------------------------------------------------------------
class CTaskInfo
{
public:
	CTaskInfo();
	virtual ~CTaskInfo() {}

	REGISTER_POOL_ALLOCATOR(CTaskInfo, false);

	// Set/Get the type of the task information
	void SetType( s32 eTaskType ) {m_eTaskType = eTaskType;}
	s32 GetTaskType( ) const {return m_eTaskType;}

	enum 
	{
		INFO_TYPE_NONE = 0, 
		INFO_TYPE_TARGET, 
		INFO_TYPE_STATUS, 
		INFO_TYPE_CLONED_FSM, 
		INFO_TYPE_SHUNT_CLONED_FSM, 
		INFO_TYPE_COVER_CLONED_FSM,
		INFO_TYPE_COVER_INTRO_CLONED_FSM,
		INFO_TYPE_USE_COVER_CLONED_FSM,
		INFO_TYPE_MOTION_IN_COVER_CLONED_FSM,
		INFO_TYPE_ENTER_VEHICLE_CLONED_FSM, 
		INFO_TYPE_ENTER_VEHICLE_CLONED,
		INFO_TYPE_ENTER_VEHICLE_OPEN_DOOR_CLONED,
		INFO_TYPE_ENTER_VEHICLE_ENTER_SEAT_CLONED,
		INFO_TYPE_ENTER_VEHICLE_CLOSE_DOOR_CLONED,
		INFO_TYPE_MOUNT_ANIMAL_CLONED,
		INFO_TYPE_DISMOUNT_ANIMAL_CLONED,
		INFO_TYPE_OPEN_VEHICLE_DOOR_CLONED_FSM, 
		INFO_TYPE_CLEAR_VEHICLE_SEAT_CLONED_FSM, 
        INFO_TYPE_SHUFFLE_SEAT_CLONED,
		INFO_TYPE_EXIT_VEHICLE_CLONED_FSM, 
		INFO_TYPE_EXIT_VEHICLE_CLONED,
		INFO_TYPE_STATUS_AND_TARGET, 
		INFO_TYPE_VEHICLE, 
		INFO_TYPE_MELEE, 
		INFO_TYPE_MELEE_ACTION_RESULT, 
		INFO_TYPE_CAR_MISSION, 
		INFO_TYPE_SEQUENCE, 
		INFO_TYPE_POLICE_SEARCH, 
		INFO_TYPE_MEDIC, 
		INFO_TYPE_IN_AIR_AND_LAND,
		INFO_TYPE_FALL_OVER,
		INFO_TYPE_GET_UP,
		INFO_TYPE_FALL_AND_GET_UP,
		INFO_TYPE_CRAWL,
		INFO_TYPE_CAR_DRIVE,
		INFO_TYPE_PLAYER_DRIVE,
		INFO_TYPE_VEHICLE_GUN,
		INFO_TYPE_GUN,
		INFO_TYPE_AIM_GUN_ON_FOOT,
		INFO_TYPE_AIM_GUN_BLIND_FIRE,
		INFO_TYPE_AIM_GUN_STRAFE,
        INFO_TYPE_AIM_GUN_CLIMBING,
		INFO_TYPE_AIM_GUN_VEHICLE_DRIVE_BY,
		INFO_TYPE_AIM_GUN_STANDING,
        INFO_TYPE_AIM_GUN_RUN_AND_GUN,
		INFO_TYPE_AIM_GUN_SCRIPTED,
		INFO_TYPE_MOUNT_THROW_PROJECTILE,
		INFO_TYPE_GET_IN_VEHICLE,
		INFO_TYPE_OPEN_VEHICLE_DOOR,
		INFO_TYPE_CLEAR_VEHICLE_SEAT,
		INFO_TYPE_CLIMB_INTO_VEHICLE,
		INFO_TYPE_EXIT_VEHICLE,
		INFO_TYPE_JACK,
		INFO_TYPE_PICKUP_AND_CARRY_OBJECT,
		INFO_TYPE_CLIMB,
		INFO_TYPE_CLIMB_LADDER,
		INFO_TYPE_PLAY_RANDOM_AMBIENTS,
		INFO_TYPE_DIE,
		INFO_TYPE_DEAD,
		INFO_TYPE_DYINGDEAD,
		INFO_TYPE_SIT_IDLE_THEN_STAND,
		INFO_TYPE_SIT_IDLE,
		INFO_TYPE_STAND_UP,
		INFO_TYPE_ANIM,
		INFO_TYPE_SCRIPT_ANIM,
		INFO_TYPE_SWAP_WEAPON,
		INFO_TYPE_ON_FIRE,
		INFO_TYPE_NEW_SLIDE_INTO_COVER,
		INFO_TYPE_NEW_USE_COVER,
		INFO_TYPE_AIM_PROJECTILE,
		INFO_TYPE_NM_CONTROL,
		INFO_TYPE_NM_BRACE, 
		INFO_TYPE_NM_BUOYANCY, 
		INFO_TYPE_NM_INJURED_ON_GROUND, 
		INFO_TYPE_NM_SHOT,
        INFO_TYPE_NM_ELECTROCUTE,
		INFO_TYPE_NM_EXPLOSION,
		INFO_TYPE_NM_HIGHFALL,
		INFO_TYPE_NM_PROTOTYPE,
		INFO_TYPE_NM_ONFIRE,
		INFO_TYPE_NM_RELAX,
		INFO_TYPE_NM_RIVERRAPIDS,
		INFO_TYPE_NM_ROLLUPANDRELAX,
		INFO_TYPE_NM_BALANCE,
		INFO_TYPE_NM_FLINCH,
		INFO_TYPE_NM_JUMPROLL,
		INFO_TYPE_NM_FALLDOWN,
		INFO_TYPE_NM_GENERICATTACH,
		INFO_TYPE_NM_POSE,
		INFO_TYPE_NM_SIMPLE,
		INFO_TYPE_NM_SIT,
        INFO_TYPE_NM_THROUGH_WINDSCREEN,
		INFO_TYPE_PAUSE,
		INFO_TYPE_RAGE_RAGDOLL,
		INFO_TYPE_THROW_PROJECTILE,
		INFO_TYPE_COMBAT_RETREAT,
		INFO_TYPE_SCENARIO,
		INFO_TYPE_USE_VEHICLE_SCENARIO,
		INFO_TYPE_SCENARIO_COWER,
		INFO_TYPE_SCENARIO_SEATED,
		INFO_TYPE_WANDER_SCENARIO,
		INFO_TYPE_SCENARIO_END, // this must come after all scenario types
		INFO_TYPE_COMBAT,
		INFO_TYPE_BOMB,
        INFO_TYPE_COMBAT_CLOSEST_TARGET_IN_AREA,
		INFO_TYPE_CONTROL_VEHICLE,
		INFO_TYPE_DAMAGE_ELECTRIC,
		INFO_TYPE_SMART_FLEE,
		INFO_TYPE_RELOAD_GUN,
		INFO_TYPE_FLEE_AND_DIVE,
		INFO_TYPE_ESCAPE_BLAST,
		INFO_TYPE_FLY_AWAY,
		INFO_TYPE_SCENARIO_FLEE,
		INFO_TYPE_SHARK_ATTACK,
		INFO_TYPE_REVIVE,
		INFO_TYPE_VEHICLE_MOUNTED_WEAPON,
		INFO_TYPE_HELI_PASSENGER_RAPPEL,
		INFO_TYPE_SCRIPTED_ANIMATION,
		INFO_TYPE_PARACHUTE,
		INFO_TYPE_PARACHUTE_OBJECT,
		INFO_TYPE_FALL,
		INFO_TYPE_VAULT,
		INFO_TYPE_DROP_DOWN,
        INFO_TYPE_LEAVE_ANY_CAR,
        INFO_TYPE_DRIVE_CAR_WANDER,
        INFO_TYPE_FOLLOW_NAVMESH_TO_COORD,
        INFO_TYPE_GOTO_POINT_AND_STAND_STILL_TIMED,
        INFO_TYPE_STAND_STILL,
		INFO_TYPE_GO_TO_AND_CLIMB_LADDER,
        INFO_TYPE_RAPPEL_DOWN_WALL,
		INFO_TYPE_AMBIENT_CLIPS,
		INFO_TYPE_USE_SCENARIO,
		INFO_TYPE_COWER,
		INFO_TYPE_CROUCH,
		INFO_TYPE_JUMP,
		INFO_TYPE_GENERAL_SWEEP,
		INFO_TYPE_ARREST_PED2,
		INFO_TYPE_SLOPE_SCRAMBLE,
		INFO_TYPE_ARREST,
		INFO_TYPE_CUFFED,
		INFO_TYPE_IN_CUSTODY,
		INFO_TYPE_MOVE_SCRIPTED,
		INFO_TYPE_REACT_AND_FLEE,
		INFO_TYPE_WEAPON_BLOCKED,
		INFO_TYPE_GO_TO_POINT_AIMING,
		INFO_TYPE_GO_TO_POINT_ANY_MEANS,
		INFO_TYPE_ACHIEVE_HEADING,
		INFO_TYPE_TAKE_OFF_PED_VARIATION,
		INFO_TYPE_TURN_TO_FACE_ENTITY,
		INFO_TYPE_SEEK_ENTITY_AIMING,
		INFO_TYPE_SEEK_ENTITY_LAST_NAV_MESH_INTERSECTION,
		INFO_TYPE_SEEK_ENTITY_STANDARD,
		INFO_TYPE_SEEK_ENTITY_OFFSET_ROTATE,
		INFO_TYPE_SEEK_ENTITY_OFFSET_FIXED,
		INFO_TYPE_SEEK_ENTITY_RADIUS_ANGLE,
		INFO_TYPE_LOOK_AT,
		INFO_TYPE_SLIDE_TO_COORD,
		INFO_TYPE_CONTROL_MOVEMENT,
		INFO_TYPE_SET_DEFENSIVE_AREA,
		INFO_TYPE_SET_AND_GUARD_AREA,
		INFO_TYPE_STAND_GUARD,
		INFO_TYPE_STAY_IN_COVER,
		INFO_TYPE_WRITHE,
		INFO_TYPE_THREAT_RESPONSE,
		INFO_TYPE_ANIMATED_ATTACH,
		INFO_TYPE_HANDS_UP,
		INFO_TYPE_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS,
		INFO_TYPE_FORCE_MOTION_STATE,
        INFO_TYPE_SYNCHRONISED_SCENE,
		INFO_TYPE_REACT_AIM_WEAPON,
		INFO_TYPE_WANDER,
		INFO_TYPE_CLEAR_LOOK_AT,
		INFO_TYPE_UNALERTED,
		INFO_TYPE_VEHICLE_PROJECTILE,
		INFO_TYPE_PLANE_CHASE,
		INFO_TYPE_JETPACK,
		INFO_TYPE_AMBULANCE_PATROL,
		INFO_TYPE_REACT_TO_EXPLOSION,
		INFO_TYPE_DIVE_TO_GROUND,
		INFO_TYPE_EVASIVE_STEP,
		INFO_TYPE_VARIED_AIM_POSE,
		INFO_TYPE_HELI_CHASE,
		INFO_TYPE_INCAPACITATED,
		INFO_TYPE_PATROL
	};

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NONE;}

	// Get/set task status variables
	void SetIsActiveTask( const bool bActiveTask )			{ m_bActiveTask = bActiveTask; }
	void SetTaskPriority( const s32 iTaskPriority )	{ Assert( iTaskPriority <= 4 ); m_iTaskPriority = iTaskPriority; }
    void SetTaskTreeDepth( const u32 iTaskTreeDepth) { m_iTaskTreeDepth = iTaskTreeDepth; }
	bool GetIsActiveTask( void ) const						{ return m_bActiveTask; }
	u32 GetTaskPriority( void )	const					{ return m_iTaskPriority; }
    u32 GetTaskTreeDepth( void ) const                   { return m_iTaskTreeDepth; }
	bool GetIsTemporaryTask( void )	const;

	// Set the next and previous tasks linked to this task
	void SetNextTaskInfo( CTaskInfo* pTaskInfo ) { m_pNextTask = pTaskInfo; }
	void SetPrevTaskInfo( CTaskInfo* pTaskInfo ) { m_pPrevTask = pTaskInfo; }
	CTaskInfo* GetNextTaskInfo( void ) const { return m_pNextTask; }
	CTaskInfo* GetPrevTaskInfo( void ) const { return m_pPrevTask; }

	// read/write/compare/log functions for network code
	virtual void   ReadSpecificTaskInfo   (datBitBuffer &messageBuffer);
	virtual void   WriteSpecificTaskInfo  (datBitBuffer &messageBuffer);
	virtual void   LogSpecificTaskInfo    (netLoggingInterface &log);
	virtual u32 GetSpecificTaskInfoSize();

	// if this returns true, the clone task is automatically created when the info is received
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return true; }

    virtual CTask         *CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity));
    virtual CTaskFSMClone *CreateCloneFSMTask() { return 0; };

    // network sequence ID functions
	virtual bool   HasNetSequences()        { return false; }
	virtual u32    GetNetSequenceID()       { return 0; }
	virtual void   SetNetSequenceID(u32) {}

    static const unsigned int MAX_SPECIFIC_TASK_INFO_SIZE = 602; // CTaskMoVEScripted
    static const unsigned int LARGEST_TASK_INFO_TASK_TYPE = CTaskTypes::TASK_SIT_DOWN;

	// some task infos contain anim info
	virtual bool				ContainsAnimInfo()		{ return false; }
	virtual fwMvClipSetId		GetClipSetId() const	{ return CLIP_SET_ID_INVALID; }
	virtual fwMvClipId			GetClipId()				{ return CLIP_ID_INVALID; }

	// Overriden by FSM task info
	virtual s32			GetState()	const		{ return -1; }

	// TaskInfo's can override this
	virtual const CEntity*	GetTarget()	const				{ return NULL; }
	virtual CEntity*		GetTarget()						{ return NULL; }
	virtual s32			    GetTargetEntryExitPoint() const	{ return -1; }
    virtual int             GetScriptedTaskType() const     { return SCRIPT_TASK_INVALID; }

	// task infos can override this to prevent themselves being networked in some circumstances 
	virtual bool IsNetworked() const	{ return false; }

#if !__NO_OUTPUT && !__FINAL
	static void PoolFullCallback(void* pItem);
#endif

private:
	s32 m_eTaskType;			 // RANGE 0 - MAX_NUM_TASK_TYPES
	u32 m_bActiveTask : 1;	 // RANGE TRUE/FALSE
	u32 m_iTaskPriority : 3;  // RANGE 0 - 4
    u32 m_iTaskTreeDepth : 3; // RANGE 0 - 4

	// Next and previous tasks in the list
	CTaskInfo     *m_pNextTask;
	CTaskInfo     *m_pPrevTask;
};



//-------------------------------------------------------------------------
// Helper class for syncing a weapon target to avoid duplication in multiple tasks...
//-------------------------------------------------------------------------

class CClonedTaskInfoWeaponTargetHelper
{
	public:

		CClonedTaskInfoWeaponTargetHelper();

		CClonedTaskInfoWeaponTargetHelper(CAITarget const* target);

		~CClonedTaskInfoWeaponTargetHelper() {}

		inline bool				HasTargetEntity(void) const				{ return m_bHasTargetEntity; }
		inline bool				HasTargetPosition(void) const			{ return m_bHasTargetPosition; }
		inline CEntity const*	GetTargetEntity(void) const				{ return m_pTargetEntity.GetEntity(); }
		inline ObjectId			GetTargetEntityId(void) const			{ return m_pTargetEntity.GetEntityID(); }
		inline Vector3 const&	GetTargetPosition(void) const			{ return m_vTargetPosition; }

		void ReadQueriableState(CAITarget& target);
		
		void UpdateTarget(CAITarget& target);

		void Serialise(CSyncDataBase& serialiser)
		{
			SERIALISE_BOOL(serialiser, m_bHasTargetEntity, "Has Target Entity");
			if(m_bHasTargetEntity || serialiser.GetIsMaximumSizeSerialiser())
			{
				ObjectId targetID = m_pTargetEntity.GetEntityID();
				SERIALISE_OBJECTID(serialiser, targetID, "Target Entity");
				m_pTargetEntity.SetEntityID(targetID);
			}

			SERIALISE_BOOL(serialiser, m_bHasTargetPosition, "Has Target Position");
			if(m_bHasTargetPosition || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_POSITION(serialiser, m_vTargetPosition, "Target Position");
			}		
		}

	private:

		void Clear();

		Vector3			m_vTargetPosition;
		CSyncedEntity	m_pTargetEntity;
		bool			m_bHasTargetEntity;
		bool			m_bHasTargetPosition;
};

//-------------------------------------------------------------------------
// Task info that stores a status
//-------------------------------------------------------------------------
class CClonedFSMTaskInfo : public CTaskInfo
{
public:
	CClonedFSMTaskInfo():m_iState(-1) {}
	CClonedFSMTaskInfo(s32 iState):m_iState(iState) {}

	s32			GetTaskInfoType( ) const			{ return INFO_TYPE_CLONED_FSM; }
	s32			GetState() const					{ return m_iState; }

	// you need to overload these for any derived classes
    virtual bool    HasState() const { return false; }
    virtual u32	GetSizeOfState() const { return 0;}
#if !__FINAL
	const char*	GetStateName(u32 iState ) const { return TASKCLASSINFOMGR.GetTaskStateName(GetTaskType(), iState); }
#endif

	// you may not use all of the corresponding game task status, this function will convert to your info status
	virtual void SetStatusFromMainTaskState(s32 mainTaskState) { m_iState = mainTaskState; }

	virtual bool IsNetworked() const	{ return true; }

protected:

	s32	m_iState;
};

//-------------------------------------------------------------------------
// Task info that creates net clone tasks
//-------------------------------------------------------------------------
class CSerialisedFSMTaskInfo : public CClonedFSMTaskInfo
{
public:
    CSerialisedFSMTaskInfo() : m_sequenceID(0) {}
	virtual ~CSerialisedFSMTaskInfo() {}

	// HACK: EJ - Made this virtual to remove template meta-programming from code (memory optimization)
	virtual void Serialise(CSyncDataBase& serialiser);

	// read/write/compare/log functions for network code
	void ReadSpecificTaskInfo(datBitBuffer &messageBuffer);
	void WriteSpecificTaskInfo(datBitBuffer &messageBuffer);
	void LogSpecificTaskInfo(netLoggingInterface &log);
    u32 GetSpecificTaskInfoSize();

    // network sequence ID functions
    bool	HasNetSequences()                   { return true; }
    u32		GetNetSequenceID()                  { return m_sequenceID; }
    void	SetNetSequenceID(u32 sequenceID)	{ m_sequenceID = sequenceID; }

private:

	u32 m_sequenceID; // net sequence ID needed to disguish different task instances
};

//-------------------------------------------------------------------------
// Base class for a task info for running an anim - for shared enums
//-------------------------------------------------------------------------
class CAnimTaskInfoBase : public CTaskInfo
{
public:

	enum
	{
		SIZEOF_GROUPID      =   32,
		SIZEOF_ANIMID       =   32,
		SIZEOF_ANIMPRIORITY	=   3,
		SIZEOF_BLEND        =   8,
		SIZEOF_RATE         =   8,
		SIZEOF_FLAGS        =   31,
	};

	enum
	{
		MAX_RATE = (1<<(SIZEOF_RATE))-1
	};
};

//-------------------------------------------------------------------------
// Implements a small queriable set of ped intelligence interface, stores
// information about task status to be transmitted accross the network
//-------------------------------------------------------------------------
class CTaskTreeClone;
class CNetObjPed;

class CQueriableInterface
{
public:

	friend class CNetObjPed;
	friend class CNetObjObject;
    friend class CTaskTreeClone;

	CQueriableInterface();
	~CQueriableInterface();

	// Resets all currently stored data
	void Reset( void );

	// ------------------- TASK INTERFACE --------------------
	// Resets just the target information
	void		ResetTaskInfo( void );
	// Resets just the scripted task information
	void		ResetScriptTaskInfo( void );
	// Adds information about a particular task, returns false if no room for information.
	void		AddTaskInformation( CTask* pTask, const bool bActive, const s32 iTaskPriority, fwEntity* pEntity );
	// Creates a normal task from the current queriable state
	CTask*		CreateNewTaskFromQueriableState( fwEntity* pEntity );
    // Parses and logs the contents of a task data buffer for a specific task type
    static void LogTaskInfo(netLoggingInterface *log, u32 taskType, const u8 *dataBlock, const int numBits);

    // ------------------- TASK NETWORK INTERFACE --------------------
	// Adds a task info structure directly from the network (usually clones of prior existing task info structures)
	void AddTaskInfoFromNetwork(CTaskInfo *taskInfo);
	// Adds information about a particular task from the network, returns false if no room for information.
	CTaskInfo *AddTaskInformationFromNetwork(u32 taskType, const bool bActive, const s32 iTaskPriority, const u32 iTaskSequenceId );
	// Creates an empty task info structure for network code
	static CTaskInfo* CreateEmptyTaskInfo(u32 taskType, bool bAssert = false);	
	// Creates information about a particular task from the network, returns NULL if no room for information.
	static CTaskInfo *CreateTaskInformationForNetwork(u32 taskType, const bool bActive, const s32 iTaskPriority, const u32 iTaskSequenceId );
	// Unlinks the specified task info from the queriable state without deleting it
	void UnlinkTaskInfoFromNetwork(CTaskInfo *taskInfoToUnlink);
	// Unlinks the specified task sequence info from the queriable state without deleting it
	void UnlinkTaskSequenceInfoFromNetwork(TaskSequenceInfo *taskSequenceInfoToUnlink);
   // Get task specific size of packed data for sending over network
    static u32  GetTaskSpecificNetworkPackedSize(u32 taskType);
    // Get the latest task sequence for the specified task type
    u32 GetTaskNetSequenceForType(s32 taskType);
	// Informs the queriable interface that a new task has been generated to replace an existing clone task
	void HandleCloneToLocalTaskSwitch(CTaskFSMClone& newTask);

	// ------------------- QUERY INTERFACE --------------------
	// Returns true if the task is currently running on the ped. If netSequenceId is specified, then this only returns true if the running task has the same sequence id
	inline bool			IsTaskCurrentlyRunning( s32 iTaskType, bool bMustBeActive = false, u32 netSequenceId = INVALID_TASK_SEQUENCE_ID ) const;
	// Returns a pointer to the CTaskInfo if the task is currently running on the ped
	inline CTaskInfo*	FindTaskInfoIfCurrentlyRunning( s32 iTaskType, bool bMustBeActive = false, u32 netSequenceId = INVALID_TASK_SEQUENCE_ID ) const;
	// Returns true if the task is currently running on the ped
	bool			IsTaskPresentAtPriority( s32 iTaskType, u32 iPriority = PED_TASK_PRIORITY_MAX) const;
	// Returns the target entity stored with this task
	const CEntity*		GetTargetForTaskType( s32 iTaskType, u32 iPriority = PED_TASK_PRIORITY_MAX) const;
	CEntity*		GetTargetForTaskType( s32 iTaskType, u32 iPriority = PED_TASK_PRIORITY_MAX);
	// Returns the task status stored with this task
	s32			GetStateForTaskType( s32 iTaskType , u32 iPriority = PED_TASK_PRIORITY_MAX) const;
	// Returns the target entry point stored with this task
	s32			GetTargetEntryPointForTaskType( s32 iTaskType , u32 iPriority = PED_TASK_PRIORITY_MAX);
	// Returns the progress of the sequence task
	s8			GetSequenceProgressForTaskType( int iInRequestedProgress, s32 iTaskType, u32 iPriority = PED_TASK_PRIORITY_MAX);
	// Wrapper function to estimate the target for an attacking character
	CEntity*	GetHostileTarget() const
	{
		CEntity* pTgt = m_CachedHostileTarget;
		Assert(!m_CachedInfoMayBeOutOfDate);
		// This has been reported to fail on occasion in MP games. That might be an issue,
		// but it seems very rare and perhaps not worth spending the time debugging if we don't
		// notice any problems that we think could be related. So far, the reported failures have
		// been from the audio code, which is probably not all that sensitive. Quite possibly
		// it would get corrected on the next update. See B* 1016898 for more info.
		//	Assertf(pTgt == const_cast<CQueriableInterface*>(this)->FindHostileTarget(),
		//		"HostileTarget mismatch in CQueriableInterface: %p %p %d", (void*)pTgt, (void*)const_cast<CQueriableInterface*>(this)->FindHostileTarget(),
		//		(int)IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT));
		return pTgt;
	}
	// Returns the scenario type stored with this task
	static s32 GetScenarioTypeFromTaskInfo(CTaskInfo& taskInfo);
	static const CScenarioPoint* GetScenarioPointFromTaskInfo(CTaskInfo& taskInfo);
	// Returns the first task info structure
	s32	GetRunningScenarioType(const bool bMustBeRunning = false);
	CScenarioPoint const* GetRunningScenarioPoint(const bool bMustBeRunning = false);
    // Returns the network synchronised scene ID
    int GetNetworkSynchronisedSceneID() const;

	// Returns the first task info structure
	CTaskInfo *     GetFirstTaskInfo() { return m_pTaskInfoFirst; }
    // Returns the task info for this task type
	CTaskInfo* GetTaskInfoForTaskType(s32 iTaskType, u32 iPriority, bool bAssertIfNotFound = true ) const;

	// Returns the first task sequence info structure
	TaskSequenceInfo *     GetFirstTaskSequenceInfo() const { return m_pFirstTaskSequenceInfo; }
    // Gets the sequence info for the specified task type
    TaskSequenceInfo *GetTaskSequenceInfo(u32 taskType);
	// Adds a task sequence info structure
	void	AddTaskSequenceInfo(TaskSequenceInfo* taskSequenceInfo);
    // Removes a task sequence info structure
	void	RemoveTaskSequenceInfo(TaskSequenceInfo* taskSequenceInfo);

	// ----------------- CACHED INFO -------------------------

	// Updates any data we have cached for performance reasons
	void	UpdateCachedInfo();

	// ---------------- SCRIPTED TASK INTERFACE --------------
	void	SetScriptedTaskStatus( s32 iScriptCommand, u8 iTaskStage ) { m_iScriptCommand = iScriptCommand; m_iScriptTaskStage = iTaskStage; }
	s32	GetScriptedTaskCommand( void )	{ return m_iScriptCommand; }
	u8	GetScriptedTaskStage( void )	{ return m_iScriptTaskStage; }

#if !__FINAL
	s32 GetDebugTaskType( s32 i ) const;
	s32 GetDebugTaskInfoType( s32 i ) const;
	//	const u32 GetPoliceSearchSector( s32 i ) const;
	const CTaskInfo* GetDebugTask( s32 i ) const;
	u32 GetDebugTaskPriority( s32 i ) const;
#endif

    static bool IsTaskHandledByNetwork(s32 taskType, bool *createsCloneTask = 0, bool bAssertIfNot = false);
    static bool IsTaskSentOverNetwork (s32 taskType, bool *createsCloneTask = 0);
    static bool IsTaskIgnoredByNetwork(s32 taskType);
    static bool CanTaskBeGivenToNonLocalPeds(s32 taskType);

#if __ASSERT
    static void ValidateTaskInfos();
#endif // __ASSERT

private:
	// Internal function for adding a task info to the linked list
	void AddTaskInfo(CTaskInfo *taskInfo);
	// Internal function which sets any task specific data in the task information union
	CTaskInfo* GenerateTaskSpecificInfo( CTask* pTask, fwEntity *pEntity );
	// Finds the first non-temporary task (the first task that doesn't exist in a temporary slot
	CTaskInfo* GetFirstNonTemporaryTask() const;
	// Finds the first task of the given priority
	CTaskInfo* GetFirstTaskOfPriority( u32 iPriority ) const;
	// Returns true if the given task type is a task which runs a simple anim
	static bool IsAnimTask(u32 taskType );
    // Adds a new sequence info for the specified type to the linked list
    TaskSequenceInfo *AddTaskSequenceInfo(u32 taskType);
    // Resets all the task sequence info
    void ResetTaskSequenceInfo();
	// Find the current hostile target, used by UpdateCachedInfo()
	CEntity* FindHostileTarget();

	// ----------------- TASK DATA STORAGE -------------------
	CTaskInfo* m_pTaskInfoFirst; // not synced
	CTaskInfo* m_pTaskInfoLast;  // not synced

    // ----------------- TASK SEQUENCE INFO -------------------
    TaskSequenceInfo *m_pFirstTaskSequenceInfo; // linked list of task sequence infos

	// ----------------- CACHED INFO -------------------------
	RegdEnt m_CachedHostileTarget;				// Output of FindHostileTarget(), returned by GetHostileTarget()
#if __ASSERT
	bool	m_CachedInfoMayBeOutOfDate;			// True if certain functions have been called, indicating that the data stored by UpdateCachedInfo() may no longer be up to date.
#endif

	// ----------------- SCRIPTED TASK STATUS ----------------
	s32	m_iScriptCommand;		// hash of the script command
	u8	m_iScriptTaskStage;		// 0 to CPedScriptedTaskRecordData::MAX_NUM_STAGES
};

//-------------------------------------------------------------------------
// Returns true if the task is currently running on the ped
//-------------------------------------------------------------------------
bool CQueriableInterface::IsTaskCurrentlyRunning( s32 iTaskType, bool bMustBeActive, u32 netSequenceId ) const
{
	// Note: users of IsTaskCurrentlyRunning() should consider calling FindTaskInfoIfCurrentlyRunning()\
	// directly instead, in particular if they have any use for the associated CTaskInfo object.
	return FindTaskInfoIfCurrentlyRunning(iTaskType, bMustBeActive, netSequenceId) != NULL;
}

//-------------------------------------------------------------------------
// Returns a pointer to the CTaskInfo if the task is currently running on the ped
//-------------------------------------------------------------------------
CTaskInfo* CQueriableInterface::FindTaskInfoIfCurrentlyRunning( s32 iTaskType, bool bMustBeActive, u32 netSequenceId ) const
{
#if __ASSERT
	// ensure all task types that are queried by the AI are synchronised over the network
	IsTaskHandledByNetwork(iTaskType, 0, true);
#endif

	CTaskInfo* pNextTaskInfo = m_pTaskInfoFirst;
	u32 iPreviousPriority = pNextTaskInfo ? pNextTaskInfo->GetTaskPriority() : 0;

	while( pNextTaskInfo )
	{
		// When checking the currently running tasks, only check the first
		// non-temporary level, so if a non-temporary response is running (combat)
		// any script command underneath is ignored.
		if( ( iPreviousPriority < pNextTaskInfo->GetTaskPriority() ) &&
			( iPreviousPriority >= PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP || bMustBeActive ) )
		{
			return NULL;
		}

		iPreviousPriority = pNextTaskInfo->GetTaskPriority();
		if( pNextTaskInfo->GetTaskType() == iTaskType )
		{
			if ( netSequenceId == INVALID_TASK_SEQUENCE_ID || (pNextTaskInfo->GetNetSequenceID() == netSequenceId))
			{
				return pNextTaskInfo;
			}
		}
		pNextTaskInfo = pNextTaskInfo->GetNextTaskInfo();
	}
	return NULL;
}

#endif
