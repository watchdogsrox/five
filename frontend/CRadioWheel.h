#ifndef __CRADIO_WHEEL__
#define __CRADIO_WHEEL__

#include "CSelectionWheel.h"

class CPed;
class CRadioWheel : public CSelectionWheel
{
	bool	m_bIsActive;
	EdgeBool m_bVisible;
	EdgeBool m_bRetuning;
	EdgeBool m_ActivatedAudioSlowMo;
	EdgeBool m_bForcedOpen;
	Vector2	m_vPreviousPos;
	EdgeInt m_iCurrentSelection;
	EdgeInt m_iQueuedTuning;
	EdgeInt m_iCurrentTrack;
	EdgeInt m_iLastSharedTrack;
	EdgeInt m_iStationCount;
	CSimpleTimer m_iTimeToTuneRadio;
	CSimpleTimer m_iForceOpenTimer;

	BankInt32 RADIO_STATION_RETUNE_TIME;
	BankInt32 FORCE_ACTIVE_TIME;

public:
	CRadioWheel();

	void Update(bool forceShow, bool isHudHidden);

	void Hide();

	bool IsActive() const			{ return m_bIsActive; }
	bool IsBeingForcedOpen() const	{ return m_bVisible.GetState() && !m_iForceOpenTimer.IsStarted(); }

	bool CanPlayerControlRadio(CPed *pPlayerPed);
#if __BANK
	void CreateBankWidgets(bkBank* pOwningBank);
#endif

#if __DEV
	bool SetDisabledReason(const char* pszReasonWhy);
	const char* GetDisabledReason() const { return m_pszDisabledReason; }
	const char* m_pszDisabledReason;
#endif

protected:
	virtual void GetInputAxis(const ioValue*& LRAxis, const ioValue*& UDAxis) const;

	void Clear(bool bHardSetTime = true );
	void RetuneStation(int newStationId);
	void SetWeightedStationSelector(int newStationIndex );
	int RadioWheelIndexToStationIndex(int iRadioWheelIndex);
	int StationIndexToRadioWheelIndex(int iStationIndex);
#if RSG_PC
	void HandleWeightedSelector(Vector2& ioStickPos);
#endif

	bool ContentPrivilegesAreOk( ) const;

	enum Highlight
	{
		HL_Queued,
		HL_Playing
	};
	void SELECT_RADIO_STATION( int newSelection );

	static const int OFF_STATION = 0;
	static const int INVALID_STATION = -1;

#if __BANK
	bool m_bDbgInvertButton;
#endif

#if RSG_PC
	Vector2 m_vMouseWeightedSelector;
	BankFloat SELECTOR_WEIGHT;
	BankFloat MOUSE_MAG_SQRD;
	bool m_bLastInputWasMouse;
	static bool sm_bDrawWeightedSelector;
#endif
};

#endif // __CRADIO_WHEEL__

