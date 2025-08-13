#ifndef TASK_VEHICLE_MISSION_BASE_H
#define TASK_VEHICLE_MISSION_BASE_H

// Gta headers.
#include "Network/General/NetworkUtil.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\racingline.h"
#include "VehicleAi\VehMission.h"
#include "Vehicles\Vehicle.h"

// framework headers
#include "fwnet/netserialisers.h"
#include "fwutil/Flags.h"

class CVehicle;
class CVehControls;
class CVehicleNodeList;
class CPhysical;

//Rage headers.
#include "Vector/Vector3.h"

class CTaskVehicleSerialiserBase;

struct sVehicleMissionParams
{
	sVehicleMissionParams()
	{
		Clear();

#if __ASSERT
		m_bTargetEntityEverSet = false;
		m_bTargetEntityCleared = false;
		m_bTargetPositionEverSet = false;
		m_bTargetPositionCleared = false;
		m_bLastTargetPositionNull = false;
#endif // __ASSERT
	}

	sVehicleMissionParams& operator =(const sVehicleMissionParams& rhs)
	{
		//m_Target = rhs.m_Target;
		m_TargetEntity = rhs.m_TargetEntity;
		m_vTargetPos = rhs.m_vTargetPos;
		m_fTargetObstructionSizeModifier = rhs.m_fTargetObstructionSizeModifier;
		m_fTargetArriveDist = rhs.m_fTargetArriveDist;
		m_iDrivingFlags = rhs.m_iDrivingFlags;
		m_fCruiseSpeed = rhs.m_fCruiseSpeed;
		m_fMaxCruiseSpeed = rhs.m_fMaxCruiseSpeed;

		return *this;
	}

	void Clear()
	{
		m_fTargetArriveDist = DEFAULT_TARGET_REACHED_DIST;
		m_fCruiseSpeed = DEFAULT_CRUISE_SPEED;
		m_fMaxCruiseSpeed = 0.0f;
		m_iDrivingFlags = DMode_StopForCars; 
		m_fTargetObstructionSizeModifier = 1.0f;
		ClearTargetEntity();
		ClearTargetPosition();
	}

	bool IsTargetValid() const
	{
		return IsTargetEntityValid() || (m_TargetEntity.GetEntity() == NULL && IsTargetPositionValid());
	}

	bool IsTargetEntityValid() const
	{
		return m_TargetEntity.GetEntity() != NULL && IsTargetPositionValidInternal(m_TargetEntity.GetEntity()->GetTransform().GetPosition());
	}

	bool IsTargetPositionValid() const
	{
		return IsTargetPositionValidInternal(RCC_VEC3V(m_vTargetPos));
	}

	void SetTargetEntity(CEntity* targetEntity)
	{
		if (targetEntity)
		{
			m_TargetEntity.SetEntity(targetEntity);
#if __ASSERT
			Assertf(IsTargetEntityValid(), "Invalid target entity (%p) (%f, %f, %f)", 
				m_TargetEntity.GetEntity(), 
				m_TargetEntity.GetEntity() ? m_TargetEntity.GetEntity()->GetTransform().GetPosition().GetX().Getf() : 0.0f, 
				m_TargetEntity.GetEntity() ? m_TargetEntity.GetEntity()->GetTransform().GetPosition().GetY().Getf() : 0.0f, 
				m_TargetEntity.GetEntity() ? m_TargetEntity.GetEntity()->GetTransform().GetPosition().GetZ().Getf() : 0.0f);
			m_bTargetEntityEverSet = true;
			m_bTargetEntityCleared = false;
#endif // __ASSERT
		}
		else
		{
			ClearTargetEntity();
		}
	}

	const CSyncedEntity& GetTargetEntity() const
	{
		return m_TargetEntity;
	}

	void SetTargetObstructionSizeModifier(float fModifier) { m_fTargetObstructionSizeModifier = fModifier; }
	float GetTargetObstructionSizeModifier() const { return m_fTargetObstructionSizeModifier; }

	void SerialiseTargetEntity(CSyncDataBase& serialiser, const char* name = NULL)
	{
		m_TargetEntity.Serialise(serialiser, name);
	}

	void SerialiseTargetPosition(const Vector3& vTargetPos)
	{
		if(ValidateTargetPosition(RCC_VEC3V(vTargetPos)))
		{
			SetTargetPosition(vTargetPos);
		}
		else
		{
			ClearTargetPosition();
		}
	}

	void ClearTargetEntity()
	{
		m_TargetEntity.Invalidate();
		ASSERT_ONLY(m_bTargetEntityCleared = true;)
	}

	void SetTargetPosition(const Vector3& vTargetPos)
	{
		m_vTargetPos = vTargetPos;
#if __ASSERT
		Assertf(IsTargetPositionValid() || IsTargetEntityValid(), "Invalid target position (%f, %f, %f)", m_vTargetPos.GetX(), m_vTargetPos.GetY(), m_vTargetPos.GetZ());
		m_bTargetPositionEverSet = true;
		m_bTargetPositionCleared = false;
		m_bLastTargetPositionNull = false;
#endif // __ASSERT
	}

	void SetTargetPosition(const Vector3* pTargetPos)
	{
		if (pTargetPos)
		{
			SetTargetPosition(*pTargetPos);
		}
		else
		{
			ASSERT_ONLY(m_bLastTargetPositionNull = true;)
		}
	}

	void SetTargetPosition(const CAITarget& aiTarget)
	{
		aiTarget.GetPosition(m_vTargetPos);
#if __ASSERT
		Assertf(IsTargetPositionValid(), "Invalid target position (%f, %f, %f)", m_vTargetPos.GetX(), m_vTargetPos.GetY(), m_vTargetPos.GetZ());
		m_bTargetPositionEverSet = true;
		m_bTargetPositionCleared = false;
		m_bLastTargetPositionNull = false;
#endif // __ASSERT
	}

	void ClearTargetPosition()
	{
		m_vTargetPos.Zero();
		ASSERT_ONLY(m_bTargetPositionCleared = true;)
	}

	const Vector3& GetTargetPosition() const
	{
		return m_vTargetPos;
	}

	void GetDominantTargetPosition(Vector3& outPosition) const
	{
		if (m_TargetEntity.GetEntity() != NULL)
		{
			outPosition = VEC3V_TO_VECTOR3(GetTargetEntity().GetEntity()->GetTransform().GetPosition());
		}
		else
		{
			outPosition = GetTargetPosition();
		}
	}

	//TODO: convert to CAITarget
	//CAITarget		m_Target;
private:
	static bool ValidateTargetPosition(const Vec3V& vPos)
	{
		return IsTrue(DistSquared(vPos, Vec3V(V_ZERO)) > ScalarV(1.0f));
	}

	bool IsTargetPositionValidInternal(const Vec3V& vTargetPos) const
	{
		Assertf(IsTrue(vTargetPos.GetX() >= ScalarV(WORLDLIMITS_XMIN)) && IsTrue(vTargetPos.GetX() <= ScalarV(WORLDLIMITS_XMAX)), "Invalid X position %f", vTargetPos.GetXf());
		Assertf(IsTrue(vTargetPos.GetY() >= ScalarV(WORLDLIMITS_YMIN)) && IsTrue(vTargetPos.GetY() <= ScalarV(WORLDLIMITS_YMAX)), "Invalid Y position %f", vTargetPos.GetYf());
		Assertf(IsTrue(vTargetPos.GetZ() >= ScalarV(WORLDLIMITS_ZMIN)) && IsTrue(vTargetPos.GetZ() <= ScalarV(WORLDLIMITS_ZMAX)), "Invalid Z position %f", vTargetPos.GetZf());

		return ValidateTargetPosition(vTargetPos);
	}

	CSyncedEntity			m_TargetEntity;
	Vector3					m_vTargetPos;
	float					m_fTargetObstructionSizeModifier;
public:
	fwFlags<s32>	m_iDrivingFlags;
	float				m_fTargetArriveDist;					// At what distance do we say we have reached the target
	float				m_fCruiseSpeed;	
	float				m_fMaxCruiseSpeed;		
private:
#if __ASSERT
	bool				m_bTargetEntityEverSet : 1;
	bool				m_bTargetEntityCleared : 1;
	bool				m_bTargetPositionEverSet : 1;
	bool				m_bTargetPositionCleared : 1;
	bool				m_bLastTargetPositionNull : 1;
#endif // __ASSERT

public:
	static const int SIZE_OF_DRIVING_FLAGS = 32; // see DrivingFlags
	static const int SIZE_OF_CRUISE_SPEED = 9;
	static const int SIZE_OF_ARRIVE_DIST  = 10;
	static const float DEFAULT_CRUISE_SPEED;// = 10;
	static const float DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST;	// At this distance the AI will go for the target in a straight line.
	static const float DEFAULT_TARGET_REACHED_DIST;//	= 4;			// At this distance the AI will consider mission passed.
};

//
//
//
class CTaskVehicleMissionBase : public CTask
{
public:
	static const float MAX_CRUISE_SPEED;// = 63;
	static const int DISTANCEWEWILLCHANGELANESAT = 20;
	static const int AIUPDATEFREQHELI = 500;
	static const int ACHIEVE_Z_DISTANCE =	5;		// Z distance at which the AI thinks its arrived or is close.
	static const int RECONSIDER_ROUTE_UPDATEFREQUENCY = 4000;

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		u32 m_MinTimeToKeepEngineAndLightsOnWhileParked; //Time in seconds vehicle must be at a Park scenario node to turn off its lights/engine

		PAR_PARSABLE;
	};

	 // Constructor/destructor
	CTaskVehicleMissionBase();
	CTaskVehicleMissionBase(const sVehicleMissionParams& params);

	virtual ~CTaskVehicleMissionBase();


	//target stuff
	void					SetTargetEntity					(const CPhysical *pNewTarget){ m_Params.SetTargetEntity(const_cast<CPhysical*>(pNewTarget));}
	const CPhysical*		GetTargetEntity					() const;
	CVehicle*				FindTargetVehicle				() const;

	const sVehicleMissionParams&	GetParams() const { return m_Params; }
	virtual void			SetParams(const sVehicleMissionParams& params) { m_Params = params; }

	virtual void			SetTargetPosition				(const Vector3 *pTargetPosition);
	const Vector3*			GetTargetPosition				();
	virtual void			FindTargetCoors					(const CVehicle *pVeh, sVehicleMissionParams &params) const;
	virtual void			FindTargetCoors					(const CVehicle* pVeh, Vector3 &TargetCoors) const;

	bool					HasMovingTarget					() const;

	virtual bool			IsVehicleMissionTask() const	{ return true; }
	virtual CVehicleNodeList * GetNodeList()				{ return NULL; }
	virtual void			CopyNodeList(const CVehicleNodeList * UNUSED_PARAM(pNodeList)) { }
	
	virtual const CVehicleFollowRouteHelper* GetFollowRouteHelper() const { return NULL; }
	virtual const CPathNodeRouteSearchHelper* GetRouteSearchHelper() const { return NULL; }

	void					SetTargetArriveDist				(float arrivalDist);

	void					SetCruiseSpeed					(float cruiseSpeed);
	float					GetCruiseSpeed					() const	{return m_Params.m_fCruiseSpeed;}
	void					SetMaxCruiseSpeed				(float maxCruiseSpeed);
	float					GetMaxCruiseSpeed				() const	{return m_Params.m_fMaxCruiseSpeed;}

	void					SetDrivingFlags(s32 flags) { m_Params.m_iDrivingFlags.SetAllFlags(flags); }
	void					SetDrivingFlag(s32 flag, bool bSet) { m_Params.m_iDrivingFlags.ChangeFlag(flag, bSet); }
	bool					IsDrivingFlagSet(s32 flag) const	{ return m_Params.m_iDrivingFlags.IsFlagSet(flag); }
	s32						GetDrivingFlags() const	{ return m_Params.m_iDrivingFlags; }

	const CPhysical			*FindVehicleWithPhysical		(const CPhysical *pPhysical) const;

	//possibly reconsider route
	void					HumaniseCarControlInput			(CVehicle* pVeh, CVehControls* pVehControls, bool bConservativeDriving, bool bGoingSlowly);

	// Should this task stop for traffic lights
	bool GetShouldObeyTrafficLights();

	// Should this task turn off engine/lights when stopped
	bool ShouldTurnOffEngineAndLights(const CVehicle* pVehicle);

	bool IsMissionAchieved() const {return m_bMissionAchieved;}

	// network serialisation
	void ReadTaskData(datBitBuffer &messageBuffer);
	void WriteTaskData(datBitBuffer &messageBuffer);
	void LogTaskData(netLoggingInterface &log);

	void ReadTaskMigrationData(datBitBuffer &messageBuffer);
	void WriteTaskMigrationData(datBitBuffer &messageBuffer);
	void LogTaskMigrationData(netLoggingInterface &log);

	virtual bool IsSyncedAcrossNetwork() const { return false; }
	virtual bool IsSyncedAcrossNetworkScript() const { return false; } // used when we only want to sync the task for script vehicles
	virtual bool IsIgnoredByNetwork() const { return false; }

	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return NULL; }
	virtual bool CanMigrate() const { return true; }

	virtual void CloneUpdate(CVehicle*) {}

	// called when the task is about to be applied to a local vehicle just after it has migrated in the network game
	virtual void SetupAfterMigration(CVehicle *pVehicle);

	virtual void RequestSoftReset() 
	{
		m_bSoftResetRequested = true;

		if (GetSubTask() && GetSubTask()->IsVehicleMissionTask())
		{
			static_cast<CTaskVehicleMissionBase*>(GetSubTask())->RequestSoftReset();
		}
	}
	virtual void DoSoftReset() 
	{
		m_bSoftResetRequested = false;
	}

	void SerialiseTarget(CSyncDataBase& serialiser)
	{
		bool bHasTargetEntity = m_Params.GetTargetEntity().GetEntityID() != NETWORK_INVALID_OBJECT_ID;

		SERIALISE_BOOL(serialiser, bHasTargetEntity);

		if (bHasTargetEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			m_Params.SerialiseTargetEntity(serialiser, "Target Entity");
		}
		else
		{
			Vector3 vTargetPosition = m_Params.GetTargetPosition();
			SERIALISE_POSITION(serialiser, vTargetPosition, "Target Position");

			m_Params.SerialiseTargetPosition(vTargetPosition);
		}
	}

	void Serialise(CSyncDataBase& serialiser)
	{
		bool bHasMaxCruiseSpeed = m_Params.m_fMaxCruiseSpeed > 0.0f;
		bool bHasTargetArriveDist = m_Params.m_fTargetArriveDist >= 0.0f;
		
		SerialiseTarget(serialiser);

		// Serialise using local var as the driving flags are stored as an fwFlags class
		u32 uDrivingFlags = (u32)m_Params.m_iDrivingFlags;
		SERIALISE_UNSIGNED(serialiser, uDrivingFlags, sVehicleMissionParams::SIZE_OF_DRIVING_FLAGS, "Driving style");
		m_Params.m_iDrivingFlags = (s32)uDrivingFlags;

		SERIALISE_PACKED_FLOAT(serialiser, m_Params.m_fCruiseSpeed, MAX_CRUISE_SPEED, sVehicleMissionParams::SIZE_OF_CRUISE_SPEED, "Cruise speed");

		SERIALISE_BOOL(serialiser, bHasMaxCruiseSpeed);

		if (bHasMaxCruiseSpeed || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_Params.m_fMaxCruiseSpeed, MAX_CRUISE_SPEED, sVehicleMissionParams::SIZE_OF_CRUISE_SPEED, "Max Cruise speed");
		}
		else
		{
			m_Params.m_fMaxCruiseSpeed = 0.0f;
		}

		SERIALISE_BOOL(serialiser, bHasTargetArriveDist);

		if (bHasTargetArriveDist || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_Params.m_fTargetArriveDist, 1000.0f, sVehicleMissionParams::SIZE_OF_ARRIVE_DIST, "Arrive dist");
		}
		else
		{
			m_Params.m_fTargetArriveDist = -1.0f;
		}

		SERIALISE_BOOL(serialiser, m_bMissionAchieved);

// 		u32 missionFlags = m_missionFlags.GetAllFlags();
// 		SERIALISE_UNSIGNED(serialiser, missionFlags, GetNumMissionFlags(), "Mission flags");
// 		m_missionFlags.SetAllFlags(missionFlags);
	}

	// some tasks serialise extra data when the vehicle migrates
	template <class Serialiser> 
	void SerialiseMigrationData(Serialiser &UNUSED_PARAM(serialiser))
	{
	}

protected:
	sVehicleMissionParams	m_Params;
	CVehControls			m_vehicleControls;
	taskRegdRef<CTaskVehicleSerialiserBase>  m_serialiser;
	bool					m_bMissionAchieved;	//needs an 8-byte aligned address for serialization
	bool					m_bSoftResetRequested:1;

	// PURPOSE: Instance of tunable task params
	static Tunables	sm_Tunables;	
};

//
// NETWORK SERIALISATION CLASSES
//
class CTaskVehicleSerialiserBase
{
public:
	FW_REGISTER_CLASS_POOL(CTaskVehicleSerialiserBase);

	virtual ~CTaskVehicleSerialiserBase() {}

	virtual void ReadTaskData(CTaskVehicleMissionBase* pTask, datBitBuffer &messageBuffer) = 0;
	virtual void WriteTaskData(CTaskVehicleMissionBase* pTask, datBitBuffer &messageBuffer) = 0;
	virtual void LogTaskData(CTaskVehicleMissionBase* pTask, netLoggingInterface &log) = 0;

	virtual void ReadTaskMigrationData(CTaskVehicleMissionBase* pTask, datBitBuffer &messageBuffer) = 0;
	virtual void WriteTaskMigrationData(CTaskVehicleMissionBase* pTask, datBitBuffer &messageBuffer) = 0;
	virtual void LogTaskMigrationData(CTaskVehicleMissionBase* pTask, netLoggingInterface &log) = 0;
};

template <class Derived> 
class CTaskVehicleSerialiser : public CTaskVehicleSerialiserBase
{
public:

	void ReadTaskData(CTaskVehicleMissionBase* pTask, datBitBuffer &messageBuffer)
	{
		CSyncDataReader serialiser(messageBuffer, 0);
		Derived *derived = static_cast<Derived *>(pTask);
		derived->Serialise(serialiser);
	}

	void WriteTaskData(CTaskVehicleMissionBase* pTask, datBitBuffer &messageBuffer)
	{
		CSyncDataWriter serialiser(messageBuffer, 0);
		Derived *derived = static_cast<Derived *>(pTask);
		derived->Serialise(serialiser);
	}

	void LogTaskData(CTaskVehicleMissionBase* pTask, netLoggingInterface &log)
	{
		CSyncDataLogger serialiser(&log);
		Derived *derived = static_cast<Derived *>(pTask);
		derived->Serialise(serialiser);
	}

	void ReadTaskMigrationData(CTaskVehicleMissionBase* pTask, datBitBuffer &messageBuffer)
	{
		CSyncDataReader serialiser(messageBuffer, 0);
		Derived *derived = static_cast<Derived *>(pTask);
		derived->SerialiseMigrationData(serialiser);
	}

	void WriteTaskMigrationData(CTaskVehicleMissionBase* pTask, datBitBuffer &messageBuffer)
	{
		CSyncDataWriter serialiser(messageBuffer, 0);
		Derived *derived = static_cast<Derived *>(pTask);
		derived->SerialiseMigrationData(serialiser);
	}

	void LogTaskMigrationData(CTaskVehicleMissionBase* pTask, netLoggingInterface &log)
	{
		CSyncDataLogger serialiser(&log);
		Derived *derived = static_cast<Derived *>(pTask);
		derived->SerialiseMigrationData(serialiser);
	}
};

#endif
