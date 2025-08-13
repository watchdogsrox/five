
#ifndef HRS_BRAKE_H
#define HRS_BRAKE_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "rmptfx/ptxkeyframe.h"
#include "parser/manager.h"
#include "Peds/Horse/horseControl.h"
#include "Peds/pedDefines.h"

namespace rage
{
	class bkBank;
}

////////////////////////////////////////////////////////////////////////////////
// class hrsBrakeControl
// 
// PURPOSE: Encapsulate all brake-related tuning, state, and logic.  This
// class is used by the horse sim to control the rate at which a horse can
// decelerate while hard- and soft-braking.
//
class hrsBrakeControl : public hrsControl
{
public:
	hrsBrakeControl();

	// PURPOSE: Reset the internal state.
	void Reset();

	// PURPOSE: Update the current braking state.
	// PARAMETERS:
	//	fSoftBrake - 0 < fSoftBrake <= 1 if soft braking, 0.0 otherwise
	//	bHardBrake - True if hard braking, false otherwise	
	//	iCurrGait - the current gait
	void Update(const float fSoftBrake, const bool bHardBrake, const float fMoveBlendRatio);

	// RETURNS: True if soft braking, false otherwise
	bool GetSoftBrake() const { return m_State.m_bSoftBrake && !m_State.m_bHardBrake; }
	// RETURNS: True if hard braking, false otherwise
	bool GetHardBrake() const { return m_State.m_bHardBrake; }
	// RETURNS: The deceleration due to braking, in meters per second squared
	// NOTES: Multiply the return value by dTime to get the drop in velocity
	float GetDeceleration() const { return m_State.m_fDeceleration; }
	// RETURNS: True if an instant stop is desired.
	bool GetInstantStop() const { return m_State.m_bInstantStop; }
	void RequestInstantStop() {m_bInstantStopRequested = true;}
	// RETURNS: The predicted stop distance, in meters
	float ComputeSoftBrakeStopDist(const float fCurrSpdNorm, const float fCurrSpdMPS);

	void AddWidgets(bkBank &b);

protected:

	struct State
	{
		void Reset();
		void AddWidgets(bkBank &b);

		bool m_bSoftBrake;
		bool m_bHardBrake;
		bool m_bInstantStop;
		float m_fDeceleration;
	};

public:

	struct Tune
	{
		Tune();
		void AddWidgets(bkBank &b);

		// PURPOSE: Register self and internal (private) structures
		static void RegisterParser()
		{
			static bool bInitialized = false;
			if( !bInitialized )
			{
				REGISTER_PARSABLE_CLASS(hrsBrakeControl::Tune);
				bInitialized = true;
			}
		}

		float m_fSoftBrakeDecel;
		float m_fHardBrakeDecel;

		PAR_SIMPLE_PARSABLE;
	};

	void AttachTuning(const Tune & t) { m_pTune = &t; }
	void DetachTuning() { m_pTune = 0; }
	const Tune & GetCurrTune() const { return *m_pTune; }

protected:

	const Tune * m_pTune;
	State m_State;
	bool  m_bInstantStopRequested;
};


////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE

#endif