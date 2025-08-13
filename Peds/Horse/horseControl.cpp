#include "Peds/Horse/horseControl.h"

#if ENABLE_HORSE

#if __BANK
#include "bank/bank.h"
#endif

////////////////////////////////////////////////////////////////////////////////

hrsControl::hrsControl()
: m_bEnabled(true)
, m_bIsMounted(false)
, m_bIsRiderPlayer(false)
, m_bIsHitched(false)
{}

////////////////////////////////////////////////////////////////////////////////

void hrsControl::UpdateMountState(bool bIsMounted, bool bIsRiderPlayer, bool bIsHitched)
{
	m_bIsMounted = bIsMounted;
	m_bIsRiderPlayer = (bIsMounted && bIsRiderPlayer);
	m_bIsHitched = bIsHitched;
}

////////////////////////////////////////////////////////////////////////////////

void hrsControl::Enable()
{
	if( !m_bEnabled )
	{
		m_bEnabled = true;
		OnEnable();
	}
}

////////////////////////////////////////////////////////////////////////////////

void hrsControl::Disable()
{
	if( m_bEnabled )
	{
		m_bEnabled = false;
		OnDisable();
	}
}

////////////////////////////////////////////////////////////////////////////////

void hrsControl::OnToggle()
{
	if( m_bEnabled )
		OnEnable();
	else
		OnDisable();
}

////////////////////////////////////////////////////////////////////////////////

void hrsControl::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.AddToggle("Enabled", &m_bEnabled, datCallback(MFA(hrsControl::OnToggle), this));
#endif
}

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE