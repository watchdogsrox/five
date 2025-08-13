#ifndef BOAT_CHASE_DIRECTOR_H
#define BOAT_CHASE_DIRECTOR_H

// Rage headers
#include "atl/array.h"
#include "scene/RegdRefTypes.h"

// Game headers
#include "task/System/TaskHelpers.h"

// Forward declarations
class CPhysical;
class CTaskBoatChase;
class CVehicle;

//////////////////////////////////////////////////////////////////////////
// CBoatChaseDirector
//////////////////////////////////////////////////////////////////////////

class CBoatChaseDirector
{

private:

	FW_REGISTER_CLASS_POOL(CBoatChaseDirector);

private:
	
	CBoatChaseDirector(const CPhysical& rTarget);
	virtual ~CBoatChaseDirector();
	
	CBoatChaseDirector(const CBoatChaseDirector& other);
	CBoatChaseDirector& operator=(const CBoatChaseDirector& other);

public:

	const CPhysical* GetTarget() const { return m_pTarget; }

public:

	int GetNumTasks() const
	{
		return m_aTasks.GetCount();
	}
	
public:

	bool Add(CTaskBoatChase* pTask);
	bool Remove(CTaskBoatChase* pTask);
	void Update(float fTimeStep);

public:

	static CBoatChaseDirector*	Find(const CPhysical& rTarget);
	static void					Init(unsigned initMode);
	static void					Shutdown(unsigned shutdownMode);
	static void					UpdateAll();
	
private:

	void RemoveInvalidTasks();
	bool ShouldUpdateDesiredOffsets() const;
	void UpdateDesiredOffsets();
	void UpdateTimers(float fTimeStep);
	
private:

	static const int sMaxTasks = 16;
	
private:

	atFixedArray<RegdTask, sMaxTasks>	m_aTasks;
	RegdConstPhy						m_pTarget;
	float								m_fTimeSinceLastDesiredOffsetsUpdate;
	
};

#endif // BOAT_CHASE_DIRECTOR_H
