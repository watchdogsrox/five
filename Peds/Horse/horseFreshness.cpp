#include "Peds/Horse/horseFreshness.h"

#if ENABLE_HORSE

#include "horseFreshness_parser.h"
#include "math/amath.h"

#if __BANK
#include "bank/bank.h"
#endif

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// hrsFreshnessControl /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

hrsFreshnessControl::hrsFreshnessControl()
{
	Reset(true);
}

////////////////////////////////////////////////////////////////////////////////

void hrsFreshnessControl::Reset(bool bResetMaxFreshnessOverride)
{
	m_State.Reset(bResetMaxFreshnessOverride);
}

////////////////////////////////////////////////////////////////////////////////

void hrsFreshnessControl::Update(const float fCurrSpdNorm, const bool bSpurredThisFrame, const float fDTime)
{
	// Do nothing if we're locked
	if( State::sm_bInfiniteFreshness && IsMountedByPlayer() )
	{
		m_State.m_CurrFreshness = 1.0f;
		m_State.m_MaxFreshness = 1.0f;
	}
	else if( !GetIsFreshnessLocked() )
	{
		// Make sure speed is normalized:
		float fSpd = rage::Clamp(fCurrSpdNorm, 0.0f, 1.0f);
		if( fSpd != fCurrSpdNorm )
			Warningf("hrsFreshnessControl::Update: Non-normalized speed %f", fCurrSpdNorm);

		// Compute the interpolated values for the current freshness and normalized speed:
		// TODO: Keep the results as part of our state?  It might be helpful to
		// graph these values in the EKG.
		State::KeyedBySpeed kbs;
		GetCurrTune().GetKeyframeData(fSpd, kbs);

		// Save the previous freshness:
		m_State.m_PrevFreshness = m_State.m_CurrFreshness;

		// Compute the max freshness
		if( IsMountedByPlayer() )
		{
			m_State.m_MaxFreshness = Clamp(kbs.m_MaxFreshness,  0.0f, m_State.m_MaxFreshnessOverride);
		}

		// Compute the new freshness:
		if( bSpurredThisFrame )
		{
			m_State.m_CurrFreshness -= kbs.m_DropPerSpur;
			m_State.m_fReplenishTimer = GetCurrTune().m_fReplenishDelay;
		}
		else if( m_State.m_fReplenishTimer > 0.0f )
		{
			m_State.m_fReplenishTimer = rage::Max(0.0f, m_State.m_fReplenishTimer -fDTime);
		}
		else
		{
			m_State.m_CurrFreshness += kbs.m_ReplenishRate * fDTime * m_State.m_MaxFreshness;
		}
	}
	else if( IsMountedByPlayer() )
	{
		m_State.m_MaxFreshness = Clamp(m_State.m_MaxFreshness,  0.0f, m_State.m_MaxFreshnessOverride);
	}

	m_State.m_CurrFreshness = Clamp(m_State.m_CurrFreshness, 0.0f, m_State.m_MaxFreshness);

}

////////////////////////////////////////////////////////////////////////////////

float hrsFreshnessControl::GetCurrFreshnessRelativeToMax() const
{
	if(IsEnabled())
	{
		const float maxVal = Max(m_State.m_MaxFreshness, SMALL_FLOAT);
		return Min(m_State.m_CurrFreshness/maxVal, 1.0f);
	}
	return 1.0f;
}

////////////////////////////////////////////////////////////////////////////////

void hrsFreshnessControl::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("FreshnessControl", false);
	hrsControl::AddWidgets(b);
	m_State.AddWidgets(b);
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Tune /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

hrsFreshnessControl::Tune::Tune()
{
}

////////////////////////////////////////////////////////////////////////////////

void hrsFreshnessControl::Tune::GetKeyframeData(const float fCurrSpdNorm, hrsFreshnessControl::State::KeyedBySpeed &kbs) const
{	
	Vec4V vKBS  = m_KFBySpeed.Query(ScalarV(fCurrSpdNorm));
	// Keyed by current speed:
	//	- MaxFreshness
	//	- ReplenishRate
	//	- DropPerSpur
	kbs.m_MaxFreshness = vKBS.GetXf();
	kbs.m_ReplenishRate = vKBS.GetYf();
	kbs.m_DropPerSpur = vKBS.GetZf();
}

////////////////////////////////////////////////////////////////////////////////

void hrsFreshnessControl::Tune::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK

	static const float sMin = 0.0f;
	static const float sMax = 1.0f;
	static const float sDelta = 0.01f;	

	Vec3V vMinMaxDeltaX = Vec3V(sMin, sMax, sDelta);
	Vec3V vMinMaxDeltaY = Vec3V(sMin, sMax, sDelta);

	Vec4V vDefaultValue = Vec4V(V_ZERO);

	static ptxKeyframeDefn s_WidgetKBS("Keyed by Speed", 0, PTXKEYFRAMETYPE_FLOAT3, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "MaxFreshness", "ReplenishRate", "DropPerSpur", NULL, false);

	b.PushGroup("Freshness Tuning", false);
	b.AddSlider("Replenish Delay", &m_fReplenishDelay, 0.0f, 10.0f, 0.01f);
	b.PushGroup("Keyed by Speed", false);
	m_KFBySpeed.SetDefn(&s_WidgetKBS);
	m_KFBySpeed.AddWidgets(b);
	b.PopGroup();
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// State ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool hrsFreshnessControl::State::sm_bInfiniteFreshness = false;

void hrsFreshnessControl::State::Reset(bool bResetMaxFreshnessOverride)
{
	m_CurrFreshness = 1.0f;
	m_PrevFreshness = 1.0f;
	m_MaxFreshness = 1.0f;
	if( bResetMaxFreshnessOverride )
		m_MaxFreshnessOverride = 1.0f;
	m_fReplenishTimer = 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

void hrsFreshnessControl::State::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("State", false);
	b.AddSlider("Freshness", &m_CurrFreshness, 0.0f, 1.0f, 0.0f);
	b.AddToggle("Locked", &m_bLocked);
	b.AddToggle("Infinite Freshness Cheat (static)", &sm_bInfiniteFreshness);
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE