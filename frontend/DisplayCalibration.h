/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : DisplayCalibration.cpp
// PURPOSE : 
// AUTHOR  : Derek Payne
// STARTED : 09/11/2012
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef __DISPLAY_CALIBRATION_H__
#define __DISPLAY_CALIBRATION_H__

//game headers
#include "frontend/scaleform/scaleformmgr.h"

class CScaleformMovieWrapper;

namespace rage
{
	class rlPresenceEvent;
}

//////////////////////////////////////////////////////////////////////////
class CDisplayCalibration
{
public:
	static bool ShouldActivateAtStartup();
	static void ActivateAtStartup(bool bActivate) { ms_ActivateAtStartup = bActivate; }
	static void SetActive(bool bValue) { ms_bActive = bValue; }
	static bool IsActive() { return ms_bActive; }
	static void LoadCalibrationMovie(int iPreviousValue = -1);
	static void RemoveCalibrationMovie();
	static bool UpdateInput();
	static void Render();
	static void Update();
	static s32  GetMovieID() { return ms_MovieWrapper.GetMovieID(); }

	static void OnPresenceEvent(const rlPresenceEvent* evt);
private:
	enum eArrowUID
	{
		LEFT_ARROW,
		RIGHT_ARROW
	};
	static void SignInBail();

	static CScaleformMovieWrapper ms_MovieWrapper;
	static bool ms_bActive;
	static bool ms_ActivateAtStartup;
	static int ms_iPreviousValue;
};

#endif // __DISPLAY_CALIBRATION_H__

// eof
