
#ifndef HRS_AGITATION_H
#define HRS_AGITATION_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "Peds/Horse/horseControl.h"
#include "rmptfx/ptxkeyframe.h"
#include "parser/manager.h"

namespace rage
{
	class bkBank;
}

////////////////////////////////////////////////////////////////////////////////
// class hrsAgitationControl
// 
// PURPOSE: Encapsulate all agitation-related tuning, state, and logic.  This
// class is used by the horse sim to determine a mount's agitation (anger or
// irritation), which affects his tendency to show an emotive animation or
// buck the rider off.
//
class hrsAgitationControl : public hrsControl
{
public:
	hrsAgitationControl();

	// PURPOSE: Reset the internal state.
	void Reset();

	// PURPOSE: Respond to mounting, based on mounting player's speed
	// PARAMETERS:
	//	fPlyrNormSpd - The mounting rider's speed, normalized 0-1
	// NOTES: Some horses might not like it when you approach them too quickly!
	void OnMount(const float fRiderSpdNorm);

	// PURPOSE: Update the current agitation.
	// PARAMETERS:
	//	bSpurredThisFrame - True if a spur happened this frame, false otherwise
	//	fDTime - Delta time
	void Update(const bool bSpurredThisFrame, const float fFreshness, const float fDTime);

	// RETURNS: The current agitation (normalized [0-1]).
	float GetCurrAgitation() const { return IsEnabled() ? m_State.m_fCurrAgitation : 0.0f; }

	// RETURNS: The previous agitation (normalized [0-1]).
	float GetPrevAgitation() const { return IsEnabled() ? m_State.m_fPrevAgitation : 0.0f; }

	// RETURNS: The delta agitation (curr - prev), normalized [0-1].
	float GetDeltaAgitation() const { return IsEnabled() ? m_State.m_fCurrAgitation - m_State.m_fPrevAgitation : 0.0f; }

	// PURPOSE: Temporarily suspend agitation updates
	// NOTES:
	//	- m_CurrAgitation will hold at its current value until EndSuspend() is called.
	//	- m_PrevAgitation == m_CurrAgitation while suspended
	void StartSuspend() { m_State.m_iSuspendCount++; }

	// PURPOSE: End temporary suspension.
	void EndSuspend() { m_State.m_iSuspendCount--; }

	// PURPOSE: Apply a factor to the current agitation due to an external influence
	// PARAMETERS:
	//	f - The factor by which to multiply our current agitation.
	// NOTES: The resulting value will be clamped to [0-1].
	void ApplyAgitationFactor(float f);

	// 
	// Set current agitation to the given value.
	// 
	void SetCurrAgitation( float f) { m_State.m_fCurrAgitation = f; }

	bool IsBuckingDisabled() const {return m_pTune->m_bBuckingDisabled;}

	// PURPOSE: Add to or subtract from the current agitation due to an external influence
	// PARAMETERS:
	//	f - The amount to add or subtract
	// NOTES: The resulting value will be clamped to [0-1].
	void ApplyAgitation(float f);

	void AddWidgets(bkBank &b);

protected:

	struct State
	{
		void Reset(const float fInitialAgitation = 0.0f);		

		void OverrideAgitation(float fNewAgitation);

		float m_fCurrAgitation;
		float m_fPrevAgitation;
		float m_fOverrideAgitation;
		int m_iSuspendCount;
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
				REGISTER_PARSABLE_CLASS(hrsAgitationControl::Tune);
				bInitialized = true;
			}
		}

		float GetAgitationOnMount(const float fRiderSpdNorm) const;
		float GetAgitationOnSpur(const float fFreshness) const;

		float m_fBaselineAgitation;
		float m_fDecayRate;
		float m_fMinTimeSinceSpurForDecay;
		bool  m_bBuckingDisabled;

		// Keyed by rider's mount speed:
		//	- AgitationOnMount
		ptxKeyframe m_KFByRiderMountSpeed;

		// Keyed by freshness:
		//	- AgitationPerSpur
		ptxKeyframe m_KFByFreshness;

		PAR_SIMPLE_PARSABLE;
	};

	void AttachTuning(const Tune & t) { m_pTune = &t; }
	void DetachTuning() { m_pTune = 0; }
	const Tune & GetCurrTune() const { return *m_pTune; }

protected:

	const Tune * m_pTune;
	State m_State;
	float m_fTimeSinceSpur;
};

////////////////////////////////////////////////////////////////////////////////

#endif //ENABLE_HORSE

#endif