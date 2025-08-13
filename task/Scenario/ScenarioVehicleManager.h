//
// task/Scenario/ScenarioVehicleManager.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SCENARIO_SCENARIOVEHICLEMANAGER_H
#define TASK_SCENARIO_SCENARIOVEHICLEMANAGER_H

#include "atl/array.h"
#include "Task/System/Tuning.h"

class CAmbientModelSet;
class CAmbientModelSetFilter;
class CScenarioPoint;
class CVehicle;

namespace rage
{
	class bkBank;
}

//-----------------------------------------------------------------------------

/*
PURPOSE
	Manager for vehicle scenario mechanisms. Specifically, it currently
	handles vehicle scenarios that attract vehicles from the regular traffic.
*/
class CVehicleScenarioManager
{
protected:
	// PURPOSE:	Data that we keep track of for each vehicle scenario attractor.
	struct AttractorData
	{
		// PURPOSE:	Initialize a new attractor.
		void Init();

		// PURPOSE:	Time stamp in ms for when this attractor can attract something again,
		//			used for example to prevent attraction again if we just attracted a vehicle.
		u32		m_TimeAllowToAttractNext;
	};

public:
	// PURPOSE:	Constructor.
	CVehicleScenarioManager();

	// PURPOSE:	Destructor.
	~CVehicleScenarioManager();

	// PURPOSE:	Main update function.
	// NOTES:	This has to be called between CVehicle::StartAiUpdate() and FinishAiUpdate(),
	//			because it makes use of the spatial array.
	void Update();

	// PURPOSE:	Add a scenario point as an attractor. The point must not already have been added,
	//			and should be of the right type.
	// PARAMS:	pt	- The point to add.
	// NOTES:	This is normally called automatically when points get added, from CScenarioManager::OnAddScenario().
	//			If called, a matching call to RemoveAttractor() needs to be made, or crashes will likely result.
	void AddAttractor(CScenarioPoint& pt);

	// PURPOSE:	Remove a scenario point as an attractor. This will silently accept
	//			if the attractor has not been added.
	// PARAMS:	pt	- The point to remove.
	// NOTES:	This is normally called automatically when points get added, from CScenarioManager::OnRemoveScenario().
	void RemoveAttractor(CScenarioPoint& pt);

#if __BANK
	// PURPOSE:	Add widgets.
	// PARAMS:	bank - The bank to add to.
	void AddWidgets(bkBank& bank);

	// PURPOSE:	Perform debug rendering.
	void DebugRender() const;

	// PURPOSE:	Reset the timer in all the attractors, to avoid having to wait during testing.
	void ClearAttractionTimers();

	// PURPOSE:	True if debug drawing of attractors is enabled.
	bool	m_DebugDrawAttractors;
#endif	// __BANK

	// PURPOSE:	Tuning data for the code in the manager.
	struct AttractorTuning
	{
		struct Tunables : CTuning
		{
			Tunables();

			// PURPOSE:	Threshold controlling the angle in the front within which a vehicle
			//			may be attracted. This is actually the square of the cosine of the angle
			//			(and the angle can be no more than 90 degrees, otherwise we would square a negative value).
			float	m_ForwardDirectionThresholdCosSquared;

			// PURPOSE:	Max distance to a vehicle path the attractor must be within in order to be attracted.
			float	m_MaxDistToPathDefault;

			// PURPOSE:	Max distance from the attractor at which a vehicle may get attracted.
			float	m_MaxDistToVehicle;

			// PURPOSE:	Min distance at which a vehicle may be attracted, perhaps needed because
			//			we probably don't want to start driving to a point if we are already very close.
			float	m_MinDistToVehicle;

			// PURPOSE:	The number of attractors that get updated each frame in a round robin fashion.
			int		m_NumToUpdatePerFrame;

			// PURPOSE:	The number of ms that must pass before an attractor can operate after attracting a vehicle successfully.
			u32		m_TimeAfterAttractionMs;

			// PURPOSE:	Time to wait if the test of the scenario chain failed, due to a parking spot being blocked or something similar.
			u32		m_TimeAfterChainTestFailedMs;

			// PURPOSE:	The number of ms that must pass after the conditions failed for a vehicle attractor.
			u32		m_TimeAfterFailedConditionsMs;

			// PURPOSE:	The number of ms that must pass if physics bounds are not streamed in around an attractor.
			u32		m_TimeAfterNoBoundsMs;

			// PURPOSE: The minimum (inclusive) number of passengers a vehicle must have to be attracted.
			u16		m_MinPassengersForAttraction;

			// PURPOSE: The maximum (inclusive) number of passengers a vehicle must have to be attracted.
			u16		m_MaxPassengersForAttraction;	

			PAR_PARSABLE;
		};
	};

protected:
	// PURPOSE:	Check if the conditions of a given attractor are fulfilled.
	// PARAMS:	attractorIndex		- The index of the attractor to check.
	// RETURNS:	True if the attractor can be used.
	bool CanAttractorBeUsed(int attractorIndex) const;

	// PURPOSE:	Check if a vehicle is eligible to be attracted by any scenario at all,
	//			checking to make sure it's a random vehicle, etc.
	// PARAMS:	veh					- The vehicle to check.
	// RETURNS:	True if the vehicle can be attracted.
	bool CanVehicleBeAttractedByAnything(const CVehicle& veh) const;

	// PURPOSE:	Check if a vehicle can be attracted by a specific point, right at this time.
	// PARAMS:	veh					- The vehicle to check.
	//			attractorIndex		- The index of the attractor to check.
	//			pAllowedModelSet	- Model set the vehicle must belong to, or NULL.
	// RETURNS:	True if the vehicle can be attracted by this point.
	// NOTES:	Calls CanVehicleBeAttractedByAnything() on the vehicle, but not
	//			CanAttractorBeUsed() on the point, as that should have been done already.
	bool CanVehicleBeAttractedByPoint(const CVehicle& veh, const CScenarioPoint& pt, const CAmbientModelSet* pAllowedModelSet, const CAmbientModelSetFilter * pModelsFilter) const;

	// PURPOSE:	Find the index of a given scenario in the attractor arrays.
	// PARAMS:	pt					- The scenario point to look for.
	// RETURNS:	The index of the attractor for the scenario point, or -1 if not added.
	// NOTES:	Currently an O(n) operation.
	int FindAttractor(CScenarioPoint& pt);

	// PURPOSE:	Set the timer in the given attractor, preventing it from operating for the
	//			given amount of time.
	// PARAMS:	attractorIndex		- The index of the attractor.
	//			preventedForTimeMs	- Time in ms to not attract anything.
	void PreventAttractionForTime(int attractorIndex, u32 preventedForTimeMs);

	// PURPOSE:	Try to take control over a vehicle, making it go to the given attractor.
	// PARAMS:	veh					- The vehicle to control.
	//			attractorIndex		- The attractor to use.
	// RETURNS:	True if successful.
	bool TakeControlOverVehicle(CVehicle& veh, int attractorIndex, const CAmbientModelSetFilter * pModelsFilter);

	// PURPOSE:	Try to attract a vehicle for the given attractor.
	// PARAMS:	attractorIndex		- The attractor to use.
	void TryToAttractVehicle(int attractorIndex, bool& retryNextFrameOut);

	// PURPOSE:	Get the tuning data for this class.
	// RETURNS:	Tuning data reference.
	const AttractorTuning::Tunables & GetTuning() const { return sm_Tuning; }

	// PURPOSE:	Array of pointers to scenario points that act as attractors.
	// NOTES:	These are intentionally not registered objects, since hopefully
	//			we'll be able safely remove them from here when the objects get
	//			removed.
	atArray<CScenarioPoint*>	m_AttractorScenarios;

	// PURPOSE:	Parallel array to m_AttractorScenarios, containing additional
	//			data about the attractor.
	atArray<AttractorData>		m_AttractorData;

	// PURPOSE:	Index of the next attractor to update, if possible.
	int							m_NextToUpdate;

	// PURPOSE:	Tuning data regarding the vehicle attractors.
	static AttractorTuning::Tunables sm_Tuning;
};

//-----------------------------------------------------------------------------

#endif	// TASK_SCENARIO_SCENARIOVEHICLEMANAGER_H

// End of file 'task/Scenario/ScenarioVehicleManager.h'
