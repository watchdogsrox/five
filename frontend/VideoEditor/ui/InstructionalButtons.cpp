/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : InstructionalButtons.cpp
// PURPOSE : Manages the instructional button movie for the video editor
// AUTHOR  : Derek Payne
// STARTED : 05/05/2014
//
/////////////////////////////////////////////////////////////////////////////////
#include "frontend/VideoEditor/ui/InstructionalButtons.h"

#if (defined( GTA_REPLAY ) && GTA_REPLAY) || ( GEN9_LANDING_PAGE_ENABLED )

#include "frontend/BusySpinner.h"
#include "frontend/landing_page/LandingPage.h"
#include "frontend/PauseMenu.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/videoeditor/ui/Editor.h"
#include "frontend/videoeditor/ui/Menu.h"
#include "frontend/videoeditor/ui/Timeline.h"
#include "frontend/videoeditor/ui/Playback.h"

#include "Text/TextFormat.h"
#include "system/controlMgr.h"

#if RSG_ORBIS
#include "rline/rlsystemui.h"
#endif

#if RSG_PC
#include "rline/rlpc.h"
#endif // RSG_PC

FRONTEND_OPTIMISATIONS()

//OPTIMISATIONS_OFF()

#define VIDEO_EDITOR_BUTTONS_FILENAME "GENERIC_INSTRUCTIONAL_BUTTONS"  // reuse store buttons

CScaleformMovieWrapper CVideoEditorInstructionalButtons::ms_Movie;
u32 CVideoEditorInstructionalButtons::ms_activeRefs = 0;

s32 CVideoEditorInstructionalButtons::ms_buttonCreatedIndex;
bool CVideoEditorInstructionalButtons::ms_lastInputKeyboard;
bool CVideoEditorInstructionalButtons::ms_RequiresUpdate = false;

#if RSG_ORBIS
bool CVideoEditorInstructionalButtons::ms_gameConstrained = false;
#endif

void CVideoEditorInstructionalButtons::Init()
{
    if( ms_activeRefs > 0 )
    {
        ++ms_activeRefs;
        Clear();
    }
    else
    {
#if RSG_PC
        ms_lastInputKeyboard = CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource();
#else
        ms_lastInputKeyboard = false;
#endif
        ms_buttonCreatedIndex = 0;
        ms_Movie.CreateMovie(SF_BASE_CLASS_GENERIC, VIDEO_EDITOR_BUTTONS_FILENAME, Vector2(0,0), Vector2(1.0f,1.0f));

        uiAssertf(!ms_Movie.IsFree(), "couldnt load '%s' movie!", VIDEO_EDITOR_BUTTONS_FILENAME);

        CBusySpinner::RegisterInstructionalButtonMovie(ms_Movie.GetMovieID());  // register this "instructional button" movie with the spinner system

        if (CPauseMenu::IsActive())  // clear any pausemenu instructional buttons so nothing conflicts
        {
            CPauseMenu::ClearInstructionalButtons();
        }

        ms_activeRefs = 1;
    }
}

void CVideoEditorInstructionalButtons::Shutdown()
{
    if( ms_activeRefs > 1 )
    {
        --ms_activeRefs;
        Refresh();
    }
    else if( ms_activeRefs == 1 )
    {
        CBusySpinner::UnregisterInstructionalButtonMovie(ms_Movie.GetMovieID());
        ms_Movie.RemoveMovie();

        if (CPauseMenu::IsActive())  // put back any pausemenu instructional buttons if needed
        {
            if (CPauseMenu::ShouldDrawInstructionalButtons())
            {
                CPauseMenu::RedrawInstructionalButtons(MENU_UNIQUE_ID_REPLAY_EDITOR);
            }
        }

        ms_activeRefs = 0;
    }
}

void CVideoEditorInstructionalButtons::Clear()
{
    if ( ms_Movie.IsActive() )
    {
        ms_buttonCreatedIndex = 0;
        ms_Movie.CallMethod( "SET_DATA_SLOT_EMPTY" );
    }
}

void CVideoEditorInstructionalButtons::Refresh()
{
    ms_RequiresUpdate = true;
}

void CVideoEditorInstructionalButtons::Update()
{
#if RSG_ORBIS
    // B*2445131 - refresh buttons on constraint to refresh for video playback auto-pause
    // urgh, Orbis doesn't have nice messaging for this ... have to keep checking for dashboard
    // best to do this for everything. doesn't happen too often. might fix things in future. 
    bool isUIShowing = g_SystemUi.IsUiShowing();
    if (ms_gameConstrained != isUIShowing)
    {
        ms_RequiresUpdate = true;
    }
    ms_gameConstrained = isUIShowing;
#endif

    if (ms_RequiresUpdate && ms_Movie.IsActive())
    {
#if RSG_PC
        ms_lastInputKeyboard = CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource();
#endif  // #if RSG_PC

        ms_Movie.CallMethod( "CLEAR_ALL" );

        if (!CControlMgr::IsShowingKeyboardTextbox())  // dont set up new buttons if the textbox is up
        {
            ms_Movie.CallMethod( "TOGGLE_MOUSE_BUTTONS", true );  // we support mouse on these instructional buttons

            ms_buttonCreatedIndex = 0;

#if defined( GTA_REPLAY ) && GTA_REPLAY
            if (CVideoEditorUi::IsAdjustingStageText())
            {
                CVideoEditorMenu::UpdateInstructionalButtons( ms_Movie );
            }
            else if ( CVideoEditorPlayback::IsActive() )
            {
                CVideoEditorPlayback::UpdateInstructionalButtons( ms_Movie );
            }
            else if ( CVideoEditorTimeline::IsTimelineSelected() )
            {
                CVideoEditorTimeline::UpdateInstructionalButtons( ms_Movie );
            }
            else if( CVideoEditorMenu::IsOpen() )
            {
                CVideoEditorMenu::UpdateInstructionalButtons( ms_Movie );
            }
            else
#endif // defined( GTA_REPLAY ) && GTA_REPLAY
            
			if (CCredits::IsRunning())
            {
                CCredits::UpdateInstructionalButtons();
            }

#if GEN9_LANDING_PAGE_ENABLED
            if( CLandingPage::IsActive() )
            {

                CLandingPage::UpdateInstructionalButtons();
            }
#endif // GEN9_LANDING_PAGE_ENABLED

            ms_Movie.CallMethod( "DRAW_INSTRUCTIONAL_BUTTONS", 0 );
        }

        ms_Movie.ForceCollectGarbage();
        ms_RequiresUpdate = false;
    }
}

void CVideoEditorInstructionalButtons::Render()
{
#if RSG_PC
    // hide instructional buttons if SCUI is showing ...hints are confusing
    if (g_rlPc.IsUiShowing())
        return;
#endif

    //
    // render instructional buttons
    //
    if ( ms_Movie.IsActive() )
    {
        ms_Movie.Render( true );
    }
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorInstructionalButtons::UpdateSpinnerText
// PURPOSE:	just update the spinner text.   Assumes only 1 item
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorInstructionalButtons::UpdateSpinnerText(const char *pString)
{
    ms_Movie.CallMethod("OVERRIDE_RESPAWN_TEXT", 0, pString);
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorInstructionalButtons::AddButton
// PURPOSE:	add 1 button to the list shown
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorInstructionalButtons::AddButton(InputType const c_inputType, const char *c_textString, bool const c_mouseSupport)
{
    char buffer[64];

    CTextFormat::GetInputButtons(c_inputType, buffer, COUNTOF(buffer), NULL, 0);

    ms_Movie.BeginMethod("SET_DATA_SLOT");

    ms_Movie.AddParam(ms_buttonCreatedIndex++);
    ms_Movie.AddParam(buffer);
    ms_Movie.AddParam(c_textString);
    ms_Movie.AddParam(c_mouseSupport);

    if (c_mouseSupport)
    {
        ms_Movie.AddParam(c_inputType);
    }
    else
    {
        ms_Movie.AddParam(-1);
    }

    ms_Movie.EndMethod();
}

void CVideoEditorInstructionalButtons::AddButton(InputType const c_inputType1, InputType const c_inputType2, const char *c_textString, bool const c_mouseSupport)
{
    char buffer[3][64];

    CTextFormat::GetInputButtons(c_inputType1, buffer[0], COUNTOF(buffer[0]), NULL, 0);
    CTextFormat::GetInputButtons(c_inputType2, buffer[1], COUNTOF(buffer[1]), NULL, 0);

    formatf(buffer[2], "%s%%%s", buffer[0], buffer[1]);

    ms_Movie.BeginMethod("SET_DATA_SLOT");

    ms_Movie.AddParam(ms_buttonCreatedIndex++);
    ms_Movie.AddParam(buffer[2]);
    ms_Movie.AddParam(c_textString);
    ms_Movie.AddParam(c_mouseSupport);

    if (c_mouseSupport)
    {
        ms_Movie.AddParam(c_inputType1);
    }
    else
    {
        ms_Movie.AddParam(-1);
    }

    ms_Movie.EndMethod();
}

void CVideoEditorInstructionalButtons::AddButtonCombo(InputType const c_inputType1, InputType const c_inputType2, const char *c_textString)
{
    char buffer[3][64];

    CTextFormat::GetInputButtons(c_inputType2, buffer[0], COUNTOF(buffer[0]), NULL, 0);
    CTextFormat::GetInputButtons(c_inputType1, buffer[1], COUNTOF(buffer[1]), NULL, 0);

    formatf(buffer[2], "%s%%b_998%%%s", buffer[0], buffer[1]);  // 'b_998' is '+'

    ms_Movie.BeginMethod("SET_DATA_SLOT");

    ms_Movie.AddParam(ms_buttonCreatedIndex++);
    ms_Movie.AddParam(buffer[2]);
    ms_Movie.AddParam(c_textString);
    ms_Movie.AddParam(false);

    ms_Movie.AddParam(-1);

    ms_Movie.EndMethod();
}

void CVideoEditorInstructionalButtons::AddButton(InputGroup const c_inputGroup, const char *c_textString, bool const c_mouseSupport)
{
    char buffer[64];

    CTextFormat::GetInputGroupButtons(c_inputGroup, buffer, COUNTOF(buffer), NULL, 0);


    ms_Movie.BeginMethod("SET_DATA_SLOT");

    ms_Movie.AddParam(ms_buttonCreatedIndex++);
    ms_Movie.AddParam(buffer);
    ms_Movie.AddParam(c_textString);
    ms_Movie.AddParam(c_mouseSupport);

    if (c_mouseSupport)
    {
        ms_Movie.AddParam(c_inputGroup);
    }
    else
    {
        ms_Movie.AddParam(-1);
    }

    ms_Movie.EndMethod();
}

void CVideoEditorInstructionalButtons::AddButton(eInstructionButtons const c_button, const char *c_textString, bool const c_mouseSupport)
{
    ms_Movie.BeginMethod("SET_DATA_SLOT");

    ms_Movie.AddParam(ms_buttonCreatedIndex++);
    ms_Movie.AddParam(c_button);
    ms_Movie.AddParam(c_textString);
    ms_Movie.AddParam(c_mouseSupport);
    
    if (c_mouseSupport)
    {
        ms_Movie.AddParam(c_button);
    }
    else
    {
        ms_Movie.AddParam(-1);
    }

    ms_Movie.EndMethod();
}

bool CVideoEditorInstructionalButtons::CheckIfUpdateIsNeeded()
{
#if RSG_PC
    if( ms_lastInputKeyboard != CControlMgr::GetPlayerMappingControl().WasKeyboardMouseLastKnownSource() )
    {
        return true;
    }
#endif // #if RSG_PC

    return false;
}

bool CVideoEditorInstructionalButtons::CheckAndAutoUpdateIfNeeded()
{
    if(CheckIfUpdateIsNeeded())
    {
        Refresh();

        return true;
    }

    return false;
}

bool CVideoEditorInstructionalButtons::LatestInstructionsAreKeyboardMouse()
{
    return ms_lastInputKeyboard;
}

#endif // (defined( GTA_REPLAY ) && GTA_REPLAY) || ( GEN9_LANDING_PAGE_ENABLED )
