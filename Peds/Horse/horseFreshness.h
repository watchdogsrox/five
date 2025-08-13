
#ifndef HRS_FRESHNESS_H
#define HRS_FRESHNESS_H

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
// class hrsFreshnessControl
// 
// PURPOSE: Encapsulate all freshness-related tuning, state, and logic.  This
// class is used by the horse sim to determine a mount's current freshness,
// based on various factors (such as current speed and rider spurring).
//
class hrsFreshnessControl : public hrsControl
{
public:
	hrsFreshnessControl();

	// PURPOSE: Reset the internal state.
	void Reset(bool bResetMaxFreshnessOverride = false);

	// PURPOSE: Update the current freshness.
	// PARAMETERS:
	//	fCurrSpeedNorm - The current speed, normalized 0-1 (1 being top speed)
	//	bSpurredThisFrame - True if a spur happened this frame, false otherwise
	//	fDTime - Delta time
	void Update(const float fCurrSpdNorm, const bool bSpurredThisFrame, const float fDTime);

	// RETURNS: current freshness
	// NOTES: return value can be arbitrarily negative.
	float GetCurrFreshness() const { return IsEnabled() ? m_State.m_CurrFreshness : 1.0f; }
	// RETURNS: current freshness relative to maximum freshness.
	float GetCurrFreshnessRelativeToMax() const;
	// RETURNS: max freshness override
	// PURPOSE: allow external access to the current max freshness override
	float GetMaxFreshnessOverride() const { return m_State.m_MaxFreshnessOverride; }
	// RETURNS: previous freshness
	// NOTES: return value can be arbitrarily negative.
	float GetPrevFreshness() const { return IsEnabled() ? m_State.m_PrevFreshness : 1.0f; }
	// RETURNS: The delta freshness (curr - prev)
	float GetDeltaFreshness() const { return IsEnabled() ? m_State.m_CurrFreshness - m_State.m_PrevFreshness : 0.0f; }
	// RETURNS: maximum freshness, normalized 0-1
	float GetMaxFreshness() const { return IsEnabled() ? m_State.m_MaxFreshness : 1.0f; }

	// PURPOSE: allow external control over the current freshness
	void SetCurrFreshness(float f) { m_State.m_CurrFreshness = f; }
	// PURPOSE: allow external control over the current max freshness override
	void SetMaxFreshnessOverride(float f) { m_State.m_MaxFreshnessOverride = Clamp(f, 0.0f, 1.0f); }
	// PURPOSE: Lock the freshness at its current value
	void SetFreshnessLocked(bool b) { m_State.m_bLocked = b; }
	// RETURNS: true if freshness is locked, false otherwise
	bool GetIsFreshnessLocked() const { return m_State.m_bLocked; }
	// PURPOSE: Activate/deactivate the "infinite freshness" cheat
	static void SetInfiniteFreshnessCheat(bool bIsActive) { State::sm_bInfiniteFreshness = bIsActive; }

	void AddWidgets(bkBank &b);

protected:

	struct State
	{
		void Reset(bool bResetMaxFreshnessOverride = false);
		void AddWidgets(bkBank &b);

		struct KeyedBySpeed
		{
			float m_MaxFreshness;
			float m_ReplenishRate;
			float m_DropPerSpur;
		};

		float m_PrevFreshness;
		float m_CurrFreshness;
		float m_MaxFreshness;
		float m_MaxFreshnessOverride;
		float m_fReplenishTimer;
		bool m_bLocked;
		static bool sm_bInfiniteFreshness;
	};

public:

	struct Tune
	{
		Tune();
		void AddWidgets(bkBank &b);
		void GetKeyframeData(const float fCurrSpeed, hrsFreshnessControl::State::KeyedBySpeed &kbs) const;

		// PURPOSE: Register self and internal (private) structures
		static void RegisterParser()
		{
			static bool bInitialized = false;
			if( !bInitialized )
			{
				REGISTER_PARSABLE_CLASS(hrsFreshnessControl::Tune);
				bInitialized = true;
			}
		}

		// Keyed by current speed:
		//	- MaxFreshness
		//	- ReplenishRate
		//	- DropPerSpur
		ptxKeyframe m_KFBySpeed;

		float m_fReplenishDelay;

		PAR_SIMPLE_PARSABLE;
	};

	void AttachTuning(const Tune & t) { m_pTune = &t; }
	void DetachTuning() { m_pTune = 0; }
	const Tune & GetCurrTune() const { return *m_pTune; }

protected:

	const Tune * m_pTune;
	State m_State;
};

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE

#endif
