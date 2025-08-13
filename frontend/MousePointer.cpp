/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MousePointer.cpp
// PURPOSE : displays the global game mouse pointer, interacts with scaleform
//			 and has some script support
// DATE    : 14/03/14
// AUTHOR  : Derek Payne
//
/////////////////////////////////////////////////////////////////////////////////


#include "frontend/MousePointer.h"
#include "script/script_hud.h"

#include "input/mouse.h"

#include "frontend/DisplayCalibration.h"
#include "frontend/PauseMenu.h"
#include "frontend/ReportMenu.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/TextInputBox.h"

#include "Frontend/ui_channel.h"
#include "renderer/DrawLists/drawList.h"

#if RSG_PC

FRONTEND_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

#define MOUSE_POINTER_MOVIE_FILENAME "mouse_pointer"
#define MOUSE_POINTER_OBJECT_MC ("mousePointerMC")

s32 CMousePointer::ms_iMovieId;
sMousePointerStateStruct CMousePointer::ms_State;
CComplexObject CMousePointer::ms_MousePointerRoot;
CComplexObject CMousePointer::ms_MousePointerObject;
s32 CMousePointer::ms_iMouseEvent;
bool CMousePointer::ms_bMouseMoved = true;
bool CMousePointer::ms_bMouseRolledOverInstructionalButtons = false;
bool CMousePointer::ms_bFirstActiveUpdate = true;
sScaleformMouseEvent CMousePointer::ms_sfMouseEvent;

#if __BANK
bool CMousePointer::ms_bBankMousePointerWidgetsCreated = false;
s32 CMousePointer::ms_iWidgetCursorStyle = 0;
bool CMousePointer::ms_bWidgetCursorVisible = false;
bool CMousePointer::ms_bWidgetMousePointerOnThisFrame = false;
#endif // __BANK



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::Init
// PURPOSE:	init class
/////////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::Init(unsigned initMode)
{
	if( initMode == rage::INIT_CORE )
	{
		ms_iMovieId = SF_SCALEFORM_INACTIVE;
		ms_iMouseEvent = MOUSE_NONE;
		ms_State.m_cursorStyle = MOUSE_CURSOR_STYLE_ARROW;
		ms_State.m_bVisible = false;
		ms_State.m_bActiveLastFrame = false;
		ms_State.m_bActiveThisFrame = false;
		ms_bFirstActiveUpdate = true;
#if __BANK
		InitWidgets();
#endif // __BANK
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::Shutdown
// PURPOSE:	shuts down the mouse pointer class
/////////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::Shutdown(unsigned shutdownMode)
{
	if ( shutdownMode == rage::SHUTDOWN_CORE )
	{
		// flush so we can release the AS object at shutdown:
		gRenderThreadInterface.Flush();

		if (ms_MousePointerObject.IsActive())
		{
			ms_MousePointerObject.Release();
		}

		if (ms_MousePointerRoot.IsActive())
		{
			ms_MousePointerRoot.Release();
		}

		if (CScaleformMgr::IsMovieActive(ms_iMovieId))
		{
			CScaleformMgr::RequestRemoveMovie(ms_iMovieId);
			ms_iMovieId = SF_SCALEFORM_INACTIVE;
		}

#if __BANK
		ShutdownWidgets();
#endif // __BANK
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::AutomaticallyTurnOn
// PURPOSE:	some systems (like pausemenu) will require the pointer to be visible
/////////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::AutomaticallyTurnOn()
{
#if __BANK
	if (ms_bWidgetMousePointerOnThisFrame)
	{
		SetMouseCursorThisFrame();  // bank widget keeps mouse alive this frame only
	}
#endif // __BANK

	// Never set the mouse cursor as active if the system UI is up
	if(!CLiveManager::IsSystemUiShowing())
	{
		// always show under these conditions
		bool bAlwaysShowConditions = CLandingPageArbiter::IsLandingPageActive() 
			|| CWarningScreen::IsActive() 
			|| CDisplayCalibration::IsActive() 
			|| STextInputBox::GetInstance().IsActive();

		// these conditions are blocked by if we're mapping input
		bool bConditionsBlockedByMapping =  CPauseMenu::IsActive() REPLAY_ONLY(|| CVideoEditorUi::IsActive());

		if (bAlwaysShowConditions || (!CControlMgr::GetPlayerMappingControl().IsMappingInProgress() && bConditionsBlockedByMapping))
		{
			bool const c_hasFocus = ioMouse::ClientAreaHasFocus();

			if (c_hasFocus)
			{
				CMousePointer::SetMouseCursorThisFrame();
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::Update
// PURPOSE:	main update
/////////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::Update()
{
	AutomaticallyTurnOn();

	// if movie is not active, then check to see if it needs to be active
	if (!CScaleformMgr::IsMovieActive(ms_iMovieId))
	{
		if ( (!ms_State.m_bActiveLastFrame) && (ms_State.m_bActiveThisFrame) )
		{
			CScaleformMgr::CreateMovieParams params(MOUSE_POINTER_MOVIE_FILENAME);

			float fPosX = 0.0f;
			float fWidth = 1.0f;
#if SUPPORT_MULTI_MONITOR
			if( GRCDEVICE.GetMonitorConfig().isMultihead())
			{
				fPosX = -1.0f;
				fWidth = 3.0f;
			}
#endif // SUPPORT_MULTI_MONITOR

			params.vPos = Vector3(fPosX,0,0);
			params.vSize = Vector2(fWidth,1.0f);
			params.bRemovable = false;  // not removeable at shutdown session in scaleform automatically
			params.eScaleMode = GFxMovieView::SM_ExactFit;  // mouse pointer movie needs to always be full screen to translate correctly to the 0-1 screen space
			params.bIgnoreSuperWidescreenScaling = true;

			ms_iMovieId = CScaleformMgr::CreateMovie(params);
			ms_bFirstActiveUpdate = true;

			uiAssertf(ms_iMovieId != INVALID_MOVIE_ID, "CMousePointer - couldnt load '%s' movie!", MOUSE_POINTER_MOVIE_FILENAME);
		}
	}
	else  // if there is no mouse pointer object and it wasnt active last frame and isnt active this frame, then remove the movie
	{
		if ( (!ms_MousePointerObject.IsActive()) && (!ms_MousePointerRoot.IsActive()) && (!ms_State.m_bActiveLastFrame) && (!ms_State.m_bActiveThisFrame) )
		{
			SetMouseCursorVisible(false);
			CScaleformMgr::RequestRemoveMovie(ms_iMovieId);
			ms_iMovieId = SF_SCALEFORM_INACTIVE;
			ms_bMouseRolledOverInstructionalButtons = false;
		}
	}

	//
	// if movie is active...
	//
	if (CScaleformMgr::IsMovieActive(ms_iMovieId))
	{
		if(ms_bFirstActiveUpdate)
		{
			if(CHudTools::GetAspectRatio(true) != (16.0f / 9.0f))
				DeviceReset();
			ms_bFirstActiveUpdate = false;
		}

		UpdateInput();
		UpdateObjects();
			
		ms_State.m_bActiveLastFrame = ms_State.m_bActiveThisFrame;
		ms_State.m_bActiveThisFrame = false;
	}
}

void CMousePointer::DeviceReset()
{
	if(CScaleformMgr::IsMovieActive(ms_iMovieId))
	{
		float fPosX = 0.0f;
		float fWidth = 1.0f;

#if SUPPORT_MULTI_MONITOR
		if( GRCDEVICE.GetMonitorConfig().isMultihead())
		{
			fPosX = -1.0f;
			fWidth = 3.0f;
		}
#endif // SUPPORT_MULTI_MONITOR

		CScaleformMgr::ChangeMovieParams(
			ms_iMovieId,
			Vector2(fPosX,0),
			Vector2(fWidth,1.0f),
			GFxMovieView::SM_ExactFit);

		if(CScaleformMgr::BeginMethod(ms_iMovieId, SF_BASE_CLASS_GENERIC, "SET_SCREEN_ASPECT"))
		{
			const float SIXTEEN_BY_NINE = 16.0f/9.0f;
			float fPhysicalAspect = CHudTools::GetAspectRatio(true);
			float fPhysicalDifference = SIXTEEN_BY_NINE / fPhysicalAspect;
			CScaleformMgr::AddParamFloat(fPhysicalDifference);
			CScaleformMgr::EndMethod();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::UpdateInput
// PURPOSE: update the mouse event state based on the mouse input that translates to
//			ui actions
/////////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::UpdateInput()
{
	static float fPreviousMousePosX = ioMouse::GetNormX();
	static float fPreviousMousePosY = ioMouse::GetNormY();

	const float fMousePosX = ioMouse::GetNormX();
	const float fMousePosY = ioMouse::GetNormY();

	const s32 iMouseWheel = ioMouse::GetDZ();

	const bool bMouseLeftClick = (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT) != 0;
	const bool bMouseRightClick = (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT) != 0;
	const bool bMouseWheelClick = (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_MIDDLE) != 0;


	if (fMousePosX != fPreviousMousePosX ||
		fMousePosY != fPreviousMousePosY ||
		iMouseWheel != 0 ||
		bMouseLeftClick ||
		bMouseRightClick ||
		bMouseWheelClick)
	{
		ms_bMouseMoved = true;

		fPreviousMousePosX = fMousePosX;
		fPreviousMousePosY = fMousePosY;
	}

	if (iMouseWheel < 0)
		ms_iMouseEvent = MOUSE_WHEEL_DOWN;

	if (iMouseWheel > 0)
		ms_iMouseEvent = MOUSE_WHEEL_UP;

	if (bMouseRightClick)
	{
		ms_iMouseEvent = MOUSE_BACK;
	}

	if (bMouseWheelClick)
	{
		ms_iMouseEvent = MOUSE_WHEEL_PRESSED;
	}

	// Handle window losing and regaining focus
	if (fMousePosX == 0.0f && fMousePosY == 0.0f)
	{
		ms_bMouseRolledOverInstructionalButtons = false;
	}

	if (CWarningScreen::IsActive())  // the warning screens will need their own mouse support (at which point this can be removed, but for now it makes using the mouse easier in the video editor)
	{
		if (bMouseLeftClick)
		{
			ms_iMouseEvent = MOUSE_ACCEPT;
		}

		if (bMouseRightClick)
		{
			ms_iMouseEvent = MOUSE_BACK;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::UpdateObjects
// PURPOSE:	update to set actionscript objects
/////////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::UpdateObjects()
{
	const Vector2 vMousePos(ioMouse::GetNormX(), ioMouse::GetNormY());

	// turn off if it was active last frame but not active this frame
	if ( (ms_State.m_bActiveLastFrame) && (!ms_State.m_bActiveThisFrame) )
	{
		if (ms_MousePointerObject.IsActive())
		{
			ms_MousePointerObject.Release();
		}

		if (ms_MousePointerRoot.IsActive())
		{
			ms_MousePointerRoot.Release();
		}

		return;
	}

	// turn ON if it was not active last frame but is active this frame frame
	if ( (!ms_State.m_bActiveLastFrame) && (ms_State.m_bActiveThisFrame) )
	{

		if (!ms_MousePointerObject.IsActive())
		{
			if (!ms_MousePointerRoot.IsActive())
			{
				ms_MousePointerRoot = CComplexObject::GetStageRoot(ms_iMovieId);
			}

			if (ms_MousePointerRoot.IsActive())
			{
				ms_MousePointerObject = ms_MousePointerRoot.GetObject(MOUSE_POINTER_OBJECT_MC);
			}
		}
	
		if (ms_MousePointerObject.IsActive())
		{
			SetMouseCursorVisible(true);
			ms_MousePointerObject.SetPosition(vMousePos);
			ms_MousePointerObject.GotoFrame((s32)ms_State.m_cursorStyle);
		}
	}

	if (ms_MousePointerObject.IsActive())
	{
		if (ms_MousePointerObject.GetPosition() != vMousePos)
		{
			ms_MousePointerObject.SetPosition(vMousePos);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::Render
// PURPOSE:	main render function
/////////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::Render()
{
	if (CScaleformMgr::IsMovieActive(ms_iMovieId) && HasMouseInputOccurred())  // only render when movie is active, and mouse input has occurred
	{
		CScaleformMgr::RenderMovie(ms_iMovieId, 0.0f, true);
	}

}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::SetMouseCursorThisFrame
// PURPOSE:	sets the mouse pointer and active this frame
/////////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::SetMouseCursorThisFrame()
{
	ms_State.m_bActiveThisFrame = true;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::SetMouseCursorStyle
// PURPOSE:	sets the cursor style from an external system (ie script)
/////////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::SetMouseCursorStyle(const eMOUSE_CURSOR_STYLE iNewStyle)
{
	if (ms_State.m_cursorStyle != iNewStyle)
	{
		ms_State.m_cursorStyle = iNewStyle;

		if (ms_MousePointerObject.IsActive())
		{
			ms_MousePointerObject.GotoFrame((s32)ms_State.m_cursorStyle);
			sfDebugf3("ms_State.m_cursorStyle = %d", ms_State.m_cursorStyle);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::SetMouseCursorVisible
// PURPOSE:	sets the cursor visiblity from an external system (ie script)
/////////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::SetMouseCursorVisible(const bool bIsVisible)
{
	ms_State.m_bVisible = bIsVisible;

	if (ms_MousePointerObject.IsActive() && ms_MousePointerObject.IsVisible() != bIsVisible)
	{
		ms_MousePointerObject.SetVisible(ms_State.m_bVisible);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMousePointer::CheckIncomingFunctions
// PURPOSE:	listens for methods coming back to scripted scaleform movies from 
//			ActionScript
/////////////////////////////////////////////////////////////////////////////////////
void CMousePointer::CheckIncomingFunctions(const atHashWithStringBank methodName, const GFxValue* args, s32 iMovieCalledFrom)
{
	if (methodName == ATSTRINGHASH("MOUSE_EVENT",0x9f80751e))
	{
		if (uiVerifyf(args[1].IsNumber(), "MOUSE_EVENT params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			const s32 iMovieType = (s32)args[1].GetNumber();

			switch (iMovieType)  // movie type
			{
				case UI_MOVIE_VIDEO_EDITOR:
				{
#if GTA_REPLAY
					CVideoEditorUi::UpdateMouseEvent(args);
#endif
					break;
				}
				case UI_MOVIE_ONLINE_POLICY:
				{
					if(SSocialClubLegalsMenu::IsInstantiated())
					{
						SSocialClubLegalsMenu::GetInstance().UpdateMouseEvent(args);
					}
					break;
				}
				case UI_MOVIE_INSTRUCTIONAL_BUTTONS:
				{
					const int SF_INPUT_ACTION_ARG = 2;
					const int ROLL_OVER_OUT_ID = -999;	// Match value defined in //depot/gta5/UI/NG_TU/Actionscript/com/rockstargames/gtav/levelDesign/INSTRUCTIONAL_BUTTONS.as

					if(uiVerifyf(args[SF_INPUT_ACTION_ARG].IsNumber(), "MOUSE_EVENT::UI_MOVIE_INSTRUCTIONAL_BUTTONS params not compatible: %s", sfScaleformManager::GetTypeName(args[SF_INPUT_ACTION_ARG])))
					{
						if (args[SF_INPUT_ACTION_ARG].GetNumber() == ROLL_OVER_OUT_ID)
						{
							const int SF_MOUSE_EVENT_ARG = 3;

							if (uiVerifyf(args[SF_MOUSE_EVENT_ARG].IsNumber(), "MOUSE_EVENT::UI_MOVIE_INSTRUCTIONAL_BUTTONS mouse event params not compatible: %s", sfScaleformManager::GetTypeName(args[SF_MOUSE_EVENT_ARG])))
							{
								if (args[SF_MOUSE_EVENT_ARG].GetNumber() == EVENT_TYPE_MOUSE_ROLL_OVER)
								{
									ms_bMouseRolledOverInstructionalButtons = true;
									uiDebugf3("UI_MOVIE_INSTRUCTIONAL_BUTTONS - Mouse Roll Over Event");
								}
								else if (args[SF_MOUSE_EVENT_ARG].GetNumber() == EVENT_TYPE_MOUSE_ROLL_OUT)
								{
									ms_bMouseRolledOverInstructionalButtons = false;
									uiDebugf3("UI_MOVIE_INSTRUCTIONAL_BUTTONS - Mouse Roll Out Event");
								}
							}
						}
						else
						{
							const InputType action = static_cast<InputType>((s32)args[SF_INPUT_ACTION_ARG].GetNumber());

							CControl& controlObj = CControlMgr::GetMainFrontendControl(false);

							// Never update the disabled control!
							if(!CControlMgr::IsDisabledControl(&controlObj))
							{
								ioSource mouseSource;
								mouseSource.m_DeviceIndex = ioSource::IOMD_KEYBOARD_MOUSE;
								mouseSource.m_Device = IOMS_MOUSE_BUTTON;
								mouseSource.m_Parameter = (u32)ioSource::IOMD_SYSTEM;
								controlObj.SetInputValueNextFrame(action, 1.0f, mouseSource);
							}
#if !__NO_OUTPUT
							else
							{
								uiDebugf3("MOUSE_EVENT::UI_MOVIE_INSTRUCTIONAL_BUTTONS ignoring mouse input action=%d because main player control is disabled", action);
							}
#endif
						}
					}

					break;
				}
				case UI_MOVIE_REPORT_MENU:
				{
					const int SF_MOUSE_EVENT_ARG = 2;
					// const int SF_VARIABLE_NAME = 3; // Skip over this param, it's not needed here
					const int SF_UID_ARG = 4;
					const int SF_CONTEXT_ARG = 5;

					if (uiVerifyf(args[SF_MOUSE_EVENT_ARG].IsNumber(), "MOUSE_EVENT::%d simple mouse event params not compatible: %s", iMovieType, sfScaleformManager::GetTypeName(args[SF_MOUSE_EVENT_ARG])) &&
						uiVerifyf(args[SF_UID_ARG].IsNumber(), "MOUSE_EVENT::%d simple mouse event params not compatible: %s", iMovieType, sfScaleformManager::GetTypeName(args[SF_UID_ARG])) &&
						uiVerifyf(args[SF_CONTEXT_ARG].IsNumber(), "MOUSE_EVENT::%d simple mouse event params not compatible: %s", iMovieType, sfScaleformManager::GetTypeName(args[SF_CONTEXT_ARG])))
					{
						// Save simple mouse event data for script
						ms_sfMouseEvent.iMovieID = iMovieCalledFrom;
						ms_sfMouseEvent.evtType = static_cast<eMOUSE_EVT>((s32)args[SF_MOUSE_EVENT_ARG].GetNumber());
						ms_sfMouseEvent.iUID = (s32)args[SF_UID_ARG].GetNumber();
						ms_sfMouseEvent.iContext = (s32)args[SF_CONTEXT_ARG].GetNumber();
					}

					if(SReportMenu::IsInstantiated() && SReportMenu::GetInstance().IsActive())
					{
						if (!STextInputBox::GetInstance().IsActive())
						{
							SReportMenu::GetInstance().UpdateMouseEvents(args);
						}
					}
					break;
				}
				case UI_MOVIE_GENERIC:
				{
					const int SF_MOUSE_EVENT_ARG = 2;
					OUTPUT_ONLY( const int SF_VARIABLE_NAME = 3; ) // Skip over this param, it's not needed here
					const int SF_UID_ARG = 4;
					const int SF_CONTEXT_ARG = 5;

					if (uiVerifyf(args[SF_MOUSE_EVENT_ARG].IsNumber(), "MOUSE_EVENT::%d simple mouse event params not compatible: %s", iMovieType, sfScaleformManager::GetTypeName(args[SF_MOUSE_EVENT_ARG])) &&
						uiVerifyf(args[SF_UID_ARG].IsNumber(), "MOUSE_EVENT::%d simple mouse event params not compatible: %s", iMovieType, sfScaleformManager::GetTypeName(args[SF_UID_ARG])) &&
						uiVerifyf(args[SF_CONTEXT_ARG].IsNumber(), "MOUSE_EVENT::%d simple mouse event params not compatible: %s", iMovieType, sfScaleformManager::GetTypeName(args[SF_CONTEXT_ARG])))
					{
						// Save simple mouse event data for script
						ms_sfMouseEvent.iMovieID = iMovieCalledFrom;
						ms_sfMouseEvent.evtType = static_cast<eMOUSE_EVT>((s32)args[SF_MOUSE_EVENT_ARG].GetNumber());
						ms_sfMouseEvent.iUID = (s32)args[SF_UID_ARG].GetNumber();
						ms_sfMouseEvent.iContext = (s32)args[SF_CONTEXT_ARG].GetNumber();
						sfDebugf3("Storing Mouse Event, AS Object Name = %s, MovieID = %d, EventType = %d, UID = %d, Context = %d", args[SF_VARIABLE_NAME].GetString(), ms_sfMouseEvent.iMovieID, ms_sfMouseEvent.evtType, ms_sfMouseEvent.iUID, ms_sfMouseEvent.iContext);
					}
					break;
				}
                case UI_MOVIE_LANDING_MENU:
                {
                    // url:bugstar:6937749 exists to fill this in.
                    // This stub is just to avoid the below assert
                    break;
                }
				default:
				{
					uiAssertf(0, "CMousePointer::CheckIncomingFunctions - Invalid movie type (%d)", iMovieType);
				}
			}
		}
	}
}


void CMousePointer::ResetMouseInput()
{
	ms_sfMouseEvent.Clear();
	ms_iMouseEvent = MOUSE_NONE;
}

bool CMousePointer::IsSFMouseReleased(s32 movieIndex, s32 uID)
{
	const sScaleformMouseEvent evt = CMousePointer::GetLastMouseEvent();

	if(evt.iMovieID != SF_INVALID_MOVIE && 
		uiVerifyf(CScaleformMgr::IsMovieActive(movieIndex), "IsSFMouseReleased -- Attempting to check for mouse events on an inactive movie"))
	{
		return evt.iMovieID == movieIndex && 
				evt.iUID == uID &&
				evt.evtType == eMOUSE_EVT::EVENT_TYPE_MOUSE_RELEASE;
	}

	return false;
}

bool CMousePointer::IsSFMouseWheelUp(s32 movieIndex)
{
	const sScaleformMouseEvent evt = CMousePointer::GetLastMouseEvent();

	if(evt.iMovieID != SF_INVALID_MOVIE && 
		uiVerifyf(CScaleformMgr::IsMovieActive(movieIndex), "IsSFMouseWheelUp -- Attempting to check for mouse events on an inactive movie"))
	{
		return evt.iMovieID == movieIndex && 
			evt.evtType == eMOUSE_EVT::EVENT_TYPE_MOUSE_WHEEL_UP;
	}

	return false;
}

bool CMousePointer::IsSFMouseWheelDown(s32 movieIndex)
{
	const sScaleformMouseEvent evt = CMousePointer::GetLastMouseEvent();

	if(evt.iMovieID != SF_INVALID_MOVIE && 
		uiVerifyf(CScaleformMgr::IsMovieActive(movieIndex), "IsSFMouseWheelDown -- Attempting to check for mouse events on an inactive movie"))
	{
		return evt.iMovieID == movieIndex && 
			evt.evtType == eMOUSE_EVT::EVENT_TYPE_MOUSE_WHEEL_DOWN;
	}

	return false;
}

bool CMousePointer::IsSFMouseRollOver(s32 movieIndex, s32 uID)
{
	const sScaleformMouseEvent evt = CMousePointer::GetLastMouseEvent();

	if(evt.iMovieID != SF_INVALID_MOVIE && 
		uiVerifyf(CScaleformMgr::IsMovieActive(movieIndex), "IsSFMouseRollOver -- Attempting to check for mouse events on an inactive movie"))
	{
		return evt.iMovieID == movieIndex && 
			evt.iUID == uID &&
			evt.evtType == eMOUSE_EVT::EVENT_TYPE_MOUSE_ROLL_OVER;
	}

	return false;
}

bool CMousePointer::IsSFMouseRollOut(s32 movieIndex, s32 uID)
{
	const sScaleformMouseEvent evt = CMousePointer::GetLastMouseEvent();

	if(evt.iMovieID != SF_INVALID_MOVIE && 
		uiVerifyf(CScaleformMgr::IsMovieActive(movieIndex), "IsSFMouseRollOut -- Attempting to check for mouse events on an inactive movie"))
	{
		return evt.iMovieID == movieIndex && 
			evt.iUID == uID &&
			evt.evtType == eMOUSE_EVT::EVENT_TYPE_MOUSE_ROLL_OUT;
	}

	return false;
}


#if __BANK
void CMousePointer::DebugCreateMousePointerBankWidgets()
{
	if( ms_bBankMousePointerWidgetsCreated )
	{
		return; // Please do not press this button again.
	}

	bkBank* bank = BANKMGR.FindBank("ui");
	if( bank == NULL )
	{
		return;
	}

	bank->PushGroup("Mouse Pointer");

	bank->AddSlider("Set Cursor Style", &ms_iWidgetCursorStyle, 0, (s32)MAX_MOUSE_CURSOR_STYLES, 1, &DebugSetMouseCursorStyle);
	bank->AddToggle("Set Cursor Visible", &ms_bWidgetCursorVisible, &DebugSetMouseCursorVisible);
	bank->AddButton("Call Device Reset", &DebugDeviceReset);
	bank->AddToggle("Set Mouse Pointer On This Frame", &ms_bWidgetMousePointerOnThisFrame);

	bank->PopGroup(); // "Mouse Pointer"
	ms_bBankMousePointerWidgetsCreated = true;
}

void CMousePointer::InitWidgets()
{
	ms_bBankMousePointerWidgetsCreated = false;

	bkBank* pBank = BANKMGR.FindBank( "ui" );
	if( pBank == NULL )
	{
		pBank = &BANKMGR.CreateBank( "ui" );
	}

	if( pBank != NULL )
	{
		pBank->AddButton("Create the Mouse Pointer widgets", &DebugCreateMousePointerBankWidgets);
	}
}

void CMousePointer::ShutdownWidgets()
{
	bkBank *bank = BANKMGR.FindBank( "ui" );
	if( bank != NULL )
	{
		bank->Destroy();
	}
}

void CMousePointer::DebugSetMouseCursorStyle()
{
	SetMouseCursorStyle((eMOUSE_CURSOR_STYLE)ms_iWidgetCursorStyle);
}

void CMousePointer::DebugSetMouseCursorVisible()
{
	SetMouseCursorVisible(ms_bWidgetCursorVisible);
}

void CMousePointer::DebugDeviceReset()
{
	DeviceReset();
}

#endif // __BANK

#endif // #if RSG_PC

// eof
