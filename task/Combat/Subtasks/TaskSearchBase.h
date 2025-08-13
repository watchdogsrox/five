// FILE :    TaskSearchBase.h
// PURPOSE : Base class for the search subtasks

#ifndef TASK_SEARCH_BASE_H
#define TASK_SEARCH_BASE_H

// Game headers
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CVehicle;

//=========================================================================
// CTaskSearchBase
//=========================================================================

class CTaskSearchBase : public CTask
{

public:

	struct Params
	{
		Params()
		: m_vPosition(V_ZERO)
		, m_pTarget(NULL)
		, m_fDirection(-1.0f)
		, m_fTimeToGiveUp(-1.0f)
		{}
		
		Vec3V			m_vPosition;
		RegdConstPed	m_pTarget;
		float			m_fDirection;
		float			m_fTimeToGiveUp;
	};

	struct Tunables : public CTuning
	{
		Tunables();

		float m_TimeToGiveUp;
		float m_MaxPositionVariance;
		float m_MaxDirectionVariance;

		PAR_PARSABLE;
	};

public:

	CTaskSearchBase(const Params& rParams);
	virtual ~CTaskSearchBase();
	
public:

#if !__FINAL
	virtual void Debug() const;
#endif
	
public:

	static void LoadDefaultParamsForPedAndTarget(const CPed& rPed, const CPed& rTarget, Params& rParams);
	
protected:

	virtual FSM_Return ProcessPreFSM();
	
private:

	bool ShouldGiveUp() const;
	
protected:

	Params m_Params;
	
private:

	static Tunables sm_Tunables;

};

//=========================================================================
// CTaskSearchInVehicleBase
//=========================================================================

class CTaskSearchInVehicleBase : public CTaskSearchBase
{

public:

	CTaskSearchInVehicleBase(const Params& rParams);
	virtual ~CTaskSearchInVehicleBase();
	
protected:

	CVehicle* GetVehicle() const;

protected:

	virtual FSM_Return ProcessPreFSM();

};

#endif // TASK_SEARCH_BASE_H
