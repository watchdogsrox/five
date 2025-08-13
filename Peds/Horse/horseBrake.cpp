#include "Peds/Horse/horseBrake.h"

#if ENABLE_HORSE

#include "horseBrake_parser.h"

#if __BANK
#include "bank/bank.h"
#endif

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// hrsBrakeControl ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

hrsBrakeControl::hrsBrakeControl()
{
	Reset();
}

////////////////////////////////////////////////////////////////////////////////

void hrsBrakeControl::Reset()
{
	m_State.Reset();
	m_bInstantStopRequested=false;
}

////////////////////////////////////////////////////////////////////////////////

void hrsBrakeControl::Update(const float fSoftBrake, const bool bHardBrake, const float fMoveBlendRatio)
{
	bool bSoftBrake = (fSoftBrake>0.0f);
	// when horse riding at walk speed, pressing B should stop immediately
	m_State.m_bInstantStop = m_bInstantStopRequested || ((bSoftBrake && !m_State.m_bSoftBrake) && CPedMotionData::GetIsWalking(fMoveBlendRatio));
	m_bInstantStopRequested = false;

	m_State.m_bHardBrake = bHardBrake;
	m_State.m_bSoftBrake = bSoftBrake && !bHardBrake;

	if( bSoftBrake )
		m_State.m_fDeceleration = GetCurrTune().m_fSoftBrakeDecel * fSoftBrake;
	else if( bHardBrake )
		m_State.m_fDeceleration = GetCurrTune().m_fHardBrakeDecel;
}

////////////////////////////////////////////////////////////////////////////////

float hrsBrakeControl::ComputeSoftBrakeStopDist(const float fCurrSpdNorm, const float fCurrSpdMPS)
{
	static float s_fMinStopDist = 0.5f;
	if( m_State.m_bInstantStop || fCurrSpdNorm<=SMALL_FLOAT || fCurrSpdNorm <= SMALL_FLOAT )
		return s_fMinStopDist;

	float fDecel = GetCurrTune().m_fSoftBrakeDecel;

	// 1. compute the stop time
	// Vf = Vi + at
	// Vi = decel * t
	// t = Vi / decel;
	float fStopTime = fCurrSpdNorm / fDecel;

	// 2. compute the normalized stop distance from the time
	// d = 1/2 at^2 + Vit
	float fStopDistNorm = -(0.5f * fDecel * fStopTime * fStopTime) + fCurrSpdNorm * fStopTime;

	// 3. norm -> meter conversion
	float fStopDist = fStopDistNorm * fCurrSpdMPS / fCurrSpdNorm;

	return Max(fStopDist, s_fMinStopDist);
}

////////////////////////////////////////////////////////////////////////////////

void hrsBrakeControl::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("BrakeControl", false);
	hrsControl::AddWidgets(b);
	m_State.AddWidgets(b);
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Tune /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

hrsBrakeControl::Tune::Tune() {}

////////////////////////////////////////////////////////////////////////////////

void hrsBrakeControl::Tune::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("Brake Tuning", false);
	b.AddSlider("SoftBrakeDecel", &m_fSoftBrakeDecel, 0.0f, 1.0f, 0.01f);
	b.AddSlider("HardBrakeDecel", &m_fHardBrakeDecel, 0.0f, 1.0f, 0.01f);
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// State ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void hrsBrakeControl::State::Reset()
{
	m_bSoftBrake = false;
	m_bHardBrake = false;
	m_bInstantStop = false;
	m_fDeceleration = 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

void hrsBrakeControl::State::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("State", false);
	b.AddToggle("SoftBrake", &m_bSoftBrake);
	b.AddToggle("HardBrake", &m_bHardBrake);
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE