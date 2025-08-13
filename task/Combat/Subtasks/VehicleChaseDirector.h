#ifndef VEHICLE_CHASE_DIRECTOR_H
#define VEHICLE_CHASE_DIRECTOR_H

// Rage headers
#include "atl/array.h"
#include "scene/RegdRefTypes.h"

// Game headers
#include "task/System/TaskHelpers.h"

// Forward declarations
class CPhysical;
class CTaskVehicleChase;
class CVehicle;

//////////////////////////////////////////////////////////////////////////
// CVehicleChaseDirector
//////////////////////////////////////////////////////////////////////////

class CVehicleChaseDirector
{
private:

	FW_REGISTER_CLASS_POOL(CVehicleChaseDirector);
	
	CVehicleChaseDirector(const CPhysical& rTarget);
	virtual ~CVehicleChaseDirector();
	
	CVehicleChaseDirector(const CVehicleChaseDirector& other);
	CVehicleChaseDirector& operator=(const CVehicleChaseDirector& other);
	
public:

	const int			GetNumTasks()	const { return m_aTasks.GetCount(); }
	const CPhysical*	GetTarget()		const { return m_pTarget; }
	
public:

	bool Add(CTaskVehicleChase* pTask);
	bool Remove(CTaskVehicleChase* pTask);
	void Update(float fTimeStep);

public:

	static CVehicleChaseDirector*	FindVehicleChaseDirector(const CPhysical& rTarget);
	static void						Init(unsigned initMode);
	static void						Shutdown(unsigned shutdownMode);
	static void						UpdateVehicleChaseDirectors();
	
private:

	void RemoveInvalidTasks();
	bool ShouldUpdateDesiredOffsets() const;
	void UpdateBehaviorFlags();
	void UpdateDesiredOffsets();
	void UpdateTimers(float fTimeStep);
	
private:

	static const int sMaxTasks	= 16;
	
private:

	atFixedArray<RegdTask, sMaxTasks>	m_aTasks;
	RegdConstPhy						m_pTarget;
	float								m_fTimeSinceLastDesiredOffsetsUpdate;
	
};

#endif // VEHICLE_CHASE_DIRECTOR_H
