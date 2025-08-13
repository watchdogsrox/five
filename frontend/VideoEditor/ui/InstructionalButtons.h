/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : InstructionalButtons.h
// PURPOSE :
// AUTHOR  : Derek Payne
// STARTED : 05/05/2014
//
/////////////////////////////////////////////////////////////////////////////////

#include "control/replay/replay.h"
#include "fwutil/Gen9Settings.h"

#if (defined( GTA_REPLAY ) && GTA_REPLAY) || ( GEN9_LANDING_PAGE_ENABLED )

#ifndef VIDEO_EDITOR_INSTRUCTIONAL_BUTTONS_H
#define VIDEO_EDITOR_INSTRUCTIONAL_BUTTONS_H

#include "frontend/Scaleform/ScaleFormMgr.h"

class CVideoEditorInstructionalButtons
{
public:
	static void Init();
	static void Shutdown();
    static void Refresh();

    static void Update();
	static void Render();

	static void Clear();
	static void UpdateSpinnerText(const char *pString);

	static CScaleformMovieWrapper& GetMovieId() { return ms_Movie; }

	static bool IsActive() { return ms_Movie.IsActive(); }

	static void AddButton(InputType const c_type, const char *c_textString, bool const c_mouseSupport = false);
	static void AddButton(InputType const c_inputType1, InputType const c_inputType2, const char *c_textString, bool const c_mouseSupport = false);
	static void AddButtonCombo(InputType const c_inputType1, InputType const c_inputType2, const char *c_textString);
	static void AddButton(InputGroup const c_inputGroup, const char *c_textString, bool const c_mouseSupport = false);
	static void AddButton(eInstructionButtons const c_button, const char *c_textString, bool const c_mouseSupport = false);

	static bool CheckIfUpdateIsNeeded();
	static bool CheckAndAutoUpdateIfNeeded();
	static bool LatestInstructionsAreKeyboardMouse();

private:
	static CScaleformMovieWrapper ms_Movie;
    static u32  ms_activeRefs;
	static s32  ms_buttonCreatedIndex;
	static bool ms_lastInputKeyboard;
	static bool ms_RequiresUpdate;

#if RSG_ORBIS
	static bool ms_gameConstrained;
#endif
};

#endif // VIDEO_EDITOR_INSTRUCTIONAL_BUTTONS_H

#endif // (defined( GTA_REPLAY ) && GTA_REPLAY) || ( GEN9_LANDING_PAGE_ENABLED )

// eof
