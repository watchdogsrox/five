

#ifndef HRS_CONTROL_H
#define HRS_CONTROL_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "data/base.h"

namespace rage
{
	class bkBank;
}

////////////////////////////////////////////////////////////////////////////////
// class hrsControl
// 
//	The base class for all hrs*Control classes.
//
class hrsControl : public datBase
{
public:
	hrsControl();

	void UpdateMountState(bool bIsMounted, bool bIsRiderPlayer, bool bIsHitched);

	bool IsMounted() const { return m_bIsMounted; }
	bool IsMountedByPlayer() const { return m_bIsMounted && m_bIsRiderPlayer; }
	bool IsHitched() const { return m_bIsHitched; }

	void Enable();
	void Disable();
	bool IsEnabled() const { return m_bEnabled; }

	virtual void OnEnable() {}
	virtual void OnDisable() {}
	void OnToggle();

	void AddWidgets(bkBank &b);

private:
	bool m_bEnabled;
	bool m_bIsMounted;
	bool m_bIsRiderPlayer;
	bool m_bIsHitched;
};

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE

#endif
