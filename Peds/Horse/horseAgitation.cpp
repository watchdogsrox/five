#include "Peds/Horse/horseAgitation.h"

#if ENABLE_HORSE

#include "horseAgitation_parser.h"

#if __BANK
#include "bank/bank.h"
#endif

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// hrsAgitationControl /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

hrsAgitationControl::hrsAgitationControl()
{
	Reset();
}

////////////////////////////////////////////////////////////////////////////////

void hrsAgitationControl::Reset()
{
	m_State.Reset();
	m_fTimeSinceSpur = 0;
}

////////////////////////////////////////////////////////////////////////////////

void hrsAgitationControl::OnMount(const float fRiderSpdNorm)
{
	// Update agitation based on rider's speed:
	m_State.Reset(m_pTune->m_fBaselineAgitation);
	m_State.m_fCurrAgitation += m_pTune->GetAgitationOnMount(fRiderSpdNorm);
	m_State.m_fCurrAgitation = rage::Clamp(m_State.m_fCurrAgitation, 0.0f, 1.0f);
}

////////////////////////////////////////////////////////////////////////////////

void hrsAgitationControl::Update(const bool bSpurredThisFrame, const float fFreshness, const float fDTime)
{
	m_State.m_fPrevAgitation = m_State.m_fCurrAgitation;

	// Don't change the current agitation when suspended:
	if( m_State.m_iSuspendCount>0 )
		return;

	static dev_float MIN_TIME_BETWEEN_AGITATION_SPUR = 0.0f;
	bool bValidSpurThisFrame = (bSpurredThisFrame && (m_fTimeSinceSpur >= MIN_TIME_BETWEEN_AGITATION_SPUR));
	if (bValidSpurThisFrame) 
	{
		m_fTimeSinceSpur = 0.f;		
	} 
	else
	{
		m_fTimeSinceSpur += fDTime;
	}

	float fCurrAgitation = m_State.m_fCurrAgitation;

	// Decay:	
	if (m_fTimeSinceSpur >= m_pTune->m_fMinTimeSinceSpurForDecay)
		fCurrAgitation -= m_pTune->m_fDecayRate * fDTime;
	fCurrAgitation = Max(0.0f, fCurrAgitation);

	float fFresh = rage::Clamp(fFreshness, 0.0f, 1.0f);

	// Override:
	if( m_State.m_fOverrideAgitation>=0.0f )
	{
		fCurrAgitation = m_State.m_fOverrideAgitation;
		m_State.m_fOverrideAgitation = -1.0f;
	}

	// Spur:
	if( bValidSpurThisFrame )
		fCurrAgitation += m_pTune->GetAgitationOnSpur(fFresh);


	// Clamp to valid range:
	fCurrAgitation = rage::Clamp(fCurrAgitation, 0.0f, 1.0f);

	if( m_State.m_fCurrAgitation != fCurrAgitation )
	{
		m_State.m_fCurrAgitation = fCurrAgitation;
	}
}

////////////////////////////////////////////////////////////////////////////////

void hrsAgitationControl::ApplyAgitationFactor(float f)
{
	m_State.OverrideAgitation(GetCurrAgitation() * f);
}

////////////////////////////////////////////////////////////////////////////////

void hrsAgitationControl::ApplyAgitation(float f)
{
	m_State.OverrideAgitation(GetCurrAgitation() + f);
}

////////////////////////////////////////////////////////////////////////////////

void hrsAgitationControl::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("AgitationControl", false);
	hrsControl::AddWidgets(b);
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Tune /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

hrsAgitationControl::Tune::Tune()
{
	m_fBaselineAgitation = 0.0f;
	m_fDecayRate = 1.0f;
	m_bBuckingDisabled = true;
	m_KFByRiderMountSpeed.AddEntry(ScalarV(V_ZERO), Vec4V(V_ZERO));
	m_KFByFreshness.AddEntry(ScalarV(V_ZERO), Vec4V(V_ZERO));
}

////////////////////////////////////////////////////////////////////////////////

float hrsAgitationControl::Tune::GetAgitationOnMount(const float fRiderSpdNorm) const
{
	return m_KFByRiderMountSpeed.Query(ScalarV(fRiderSpdNorm)).GetXf();	
}

////////////////////////////////////////////////////////////////////////////////

float hrsAgitationControl::Tune::GetAgitationOnSpur(const float fFreshness) const
{	
	return m_KFByFreshness.Query(ScalarV(fFreshness)).GetXf();;	
}

////////////////////////////////////////////////////////////////////////////////

void hrsAgitationControl::Tune::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK

	b.PushGroup("Agitation Tuning", false);

	b.AddSlider("Baseline Agitation", &m_fBaselineAgitation, 0.0f, 1.0f, 0.01f);
	b.AddSlider("Decay Rate", &m_fDecayRate, 0.0f, 10.0f, 0.01f);
	b.AddSlider("MinTimeSinceSpurForDecay", &m_fMinTimeSinceSpurForDecay, 0.0f, 10.0f, 0.01f);	
	b.AddToggle("Bucking Disabled", &m_bBuckingDisabled);

	static const float sMin = 0.0f;
	static const float sMax = 1.0f;
	static const float sDelta = 0.01f;	

	Vec3V vMinMaxDeltaX = Vec3V(sMin, sMax, sDelta);
	Vec3V vMinMaxDeltaY = Vec3V(sMin, sMax, sDelta);

	Vec4V vDefaultValue = Vec4V(V_ZERO);

	static ptxKeyframeDefn s_WidgetKBRMS("Keyed by Rider Mount Speed", 0, PTXKEYFRAMETYPE_FLOAT, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "AgitationOnMount", NULL, NULL, NULL, false);
	static ptxKeyframeDefn s_WidgetKBF("Keyed by Freshness", 0, PTXKEYFRAMETYPE_FLOAT, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "AgitationOnSpur", NULL, NULL, NULL, false);

	b.PushGroup("Keyed by Rider Mount Speed", false);
	m_KFByRiderMountSpeed.SetDefn(&s_WidgetKBRMS);
	m_KFByRiderMountSpeed.AddWidgets(b);
	b.PopGroup();

	b.PushGroup("Keyed by Freshness", false);
	m_KFByFreshness.SetDefn(&s_WidgetKBF);
	m_KFByFreshness.AddWidgets(b);
	b.PopGroup();

	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// State ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void hrsAgitationControl::State::Reset(const float fInitialAgitation)
{
	m_fCurrAgitation = m_fPrevAgitation = fInitialAgitation;
	m_iSuspendCount = 0;
}

////////////////////////////////////////////////////////////////////////////////

void hrsAgitationControl::State::OverrideAgitation(float fNewAgitation)
{
	m_fOverrideAgitation = rage::Clamp(fNewAgitation, 0.0f, 1.0f);
}

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE