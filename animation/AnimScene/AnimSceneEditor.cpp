#if __BANK

#include "AnimSceneEditor.h"
#include <time.h>

// Rage headers
#include "bank/group.h"
#include "bank/msgbox.h"
#include "grcore/font.h"
#include "parser/macros.h"

// framework headers
#include "fwdebug/picker.h"
#include "debug/GtaPicker.h"

// game headers
#include "animation/AnimScene/AnimScene.h"
#include "animation/AnimScene/AnimSceneManager.h"
#include "animation/animscene/entities/AnimSceneEntities.h"
#include "animation/AnimScene/entities/AnimSceneEntityInitialisers.h"
#include "animation/AnimScene/events/AnimScenePlayAnimEvent.h"
#include "animation/AnimScene/events/AnimScenePlayCameraAnimEvent.h"
#include "animation/AnimScene/events/AnimSceneEventInitialisers.h"
#include "AnimSceneScrollbar.h"
#include "animation/debug/AnimDebug.h"
#include "peds/ped.h"
#include "scene/physical.h"
#include "system/controlMgr.h"
#include "debug/DebugScene.h"
#include "text/Text.h"

ANIM_OPTIMISATIONS()

//--------------------------------------------------------------------------------
// VERSION NUMBER
//--------------------------------------------------------------------------------
#define ASE_MAJOR_VERSION (0)
#define ASE_MINOR_VERSION (2)

	// Version number text color
#define ASE_VersionColor (Color32(242, 242, 242, 160))
#define ASE_VersionShadowColor (Color32(0, 0, 0, 255))

	// Picker color
#define ASE_EntityPickerBoundingBoxColor (Color32(255, 0, 0, 255))

	//Panel Colors
#define ASE_Panel_BackGroundColor (Color32(0, 0, 0, 128))
#define ASE_Panel_BackGroundColorDark (Color32(0, 0, 0, 160))
#define ASE_Panel_OutlineColor (Color32(242, 242, 242, 160))
#define ASE_Panel_ReadOnlyTextColor (Color32(256, 256, 256, 128))


	//Panel Button Color
#define ASE_Panel_Button_Color (Color32(242, 242, 242, 160))
#define ASE_Panel_Button_BackGroundColor (Color32(0, 0, 0, 128))
#define ASE_Panel_Button_HoverBackgroundColor (Color32(20, 20, 20, 160))

#define ASE_Panel_Button_HoverColor (Color32(255, 255, 255, 255))
#define ASE_Panel_Button_ActiveHoverColor (Color32(255, 255, 255, 255))

#define ASE_Panel_Button_ActiveColor (Color32(242, 242, 242, 160))
#define ASE_Panel_Button_ActiveBackgroundColor (Color32(40, 40, 40, 180))
#define ASE_Panel_Button_ActiveOutlineColor (Color32(256, 0, 0, 160))
#define ASE_Panel_Button_OutlineColor (Color32(256, 0, 0, 160))

//Text Colours
#define ASE_Editor_DefaultTextColor (Color32(242, 242, 242, 160))
#define ASE_Editor_DefaultTextOutlineColor (Color32(200, 200, 200, 128))
#define ASE_Editor_DefaultTextBackgroundColor (Color32(0, 0, 0, 128))
#define ASE_Editor_HoveredTextColor (Color32(242, 242, 242, 160))
#define ASE_Editor_WarningTextColor (Color32(255, 204, 0, 160))

#define ASE_Editor_SelectedTextColor (Color32(242, 242, 242, 160))
#define ASE_Editor_SelectedTextOutlineColor (Color32(256, 0, 0, 100))
#define ASE_Editor_FocusTextOutlineColor (Color32(256, 0, 0, 200))
#define ASE_Editor_SelectedTextBackgroundColor (Color32(0, 0, 0, 160))
#define ASE_Editor_SelectedHoveredTextColor (Color32(255, 255, 255, 160))

#define ASE_Editor_DisabledTextColor (Color32(160, 160, 160, 90))
#define ASE_Editor_DisabledOutlineColor (Color32(160, 160, 160, 90))
#define ASE_Editor_DisabledBackgroundColor (Color32(0, 0, 0, 90))

//Event Colors
#define ASE_Panel_EventColor (Color32(0, 0, 0, 128))

#define ASE_Panel_EventBakerlooBrownColor (Color32(179, 99, 5, 255))
#define ASE_Panel_EventCentraRedColor (Color32(227, 32, 23, 255))
#define ASE_Panel_EventCircleYellowColor (Color32(255, 211, 0, 255))
#define ASE_Panel_EventDistrictGreen (Color32(0, 120, 42, 255))
#define ASE_Panel_EventHCPinkColor (Color32(243, 169, 187, 255))
#define ASE_Panel_EventJubileeGreyColor (Color32(160, 165, 169, 255))
#define ASE_Panel_EventMetropolitanPurpleColor (Color32(155, 0, 86, 255))
#define ASE_Panel_EventNorthernBlackColor (Color32(0, 0, 0, 255))
#define ASE_Panel_EventPiccadilyBlueColor (Color32(0, 54, 136, 255))
#define ASE_Panel_EventVictoriaBlueColor (Color32(0, 152, 212, 255))
#define ASE_Panel_EventWCGreenColor (Color32(149, 205, 186, 255))
#define ASE_Panel_EventDLRBluekColor (Color32(0, 164, 167, 255))
#define ASE_Panel_EventOGOrangeColor (Color32(238, 124, 14, 255))
#define ASE_Panel_EventTLGreenColor (Color32(132, 184, 23, 255))

#define ASE_Panel_EventMarkerColor (Color32(255, 255, 255, 128))
#define ASE_Panel_EventOutlineColor (Color32(255, 255, 255, 128))
#define ASE_Panel_EventBackgroundColor (Color32(200, 200, 255, 128))

#define ASE_Panel_SelectedEventColor (Color32(0, 0, 0, 192))
#define ASE_Panel_SelectedEventMarkerColor (Color32(256, 0, 0, 192))
#define ASE_Panel_SelectedEventOutlineColor (Color32(256, 0, 0, 192))
#define ASE_Panel_SelectedEventDraggingColor (Color32(180, 75, 256, 255))

#define ASE_Panel_EventPreloadlineColor (Color32(0, 0, 0, 160))
#define ASE_Panel_EventCurrentTimeLineColor (Color32(180, 180, 180, 255))

// Timeline Colors
#define ASE_Timeline_Marker1FrameColor (Color32(64, 64, 64, 160))
#define ASE_Timeline_Marker5FrameColor (Color32(96, 96, 96, 160))
#define ASE_Timeline_Marker30FrameColor (Color32(128, 128, 128, 160))

float CAnimSceneEditor::EventLayer::ms_layerHeight = 15.0f;
bool CAnimSceneEditor::m_editorSettingUseSmoothFont = false;
bool CAnimSceneEditor::m_editorSettingLoopPlayback = false;
bool CAnimSceneEditor::m_editorSettingHoldAtLastFrame = true;
bool CAnimSceneEditor::m_editorSettingHideEvents = false;
float CAnimSceneEditor::ms_editorSettingBackgroundAlpha = 0.5f;
bool CAnimSceneEditor::ms_editorSettingUseEventColourForMarker = true;

//TUNE_FLOAT(ms_layerHeight, 30.0f, 0.0f, 100.0f, 1.0f)
//TUNE_FLOAT(ButtonWidth, 50.0f, 0.0f, 100.0f, 1.0f)

//--------------------------------------------------------------------------------
// TEXT RENDERING
//--------------------------------------------------------------------------------

// NOTE: This is pretty hard-coded at the moment due to Scaleform font details.
// The Scaleform font's already have some padding, so we have to counter-act that to give us the precise text positioning and sizing we desire.
// The Scaleform font rendering is also based on normalised screen space, where as Anim Scene Editor layout code uses pixels, so we have to convert accordingly.

// NOTE: Any hard-coded values below were calculated when the scale was 1.0, and hence scaled accordingly.

// Overall scale.  This defines the overall scaling of the font, and these values are based on the debug information bar at the bottom of the screen.  See bar.cpp
const float fTextOverallScaleX = 0.1f;
const float fTextOverallScaleY = 0.3f;

const float fTextOverallSizeScale = 0.9f;

// Calculate the final scaling
const float fFinalTextScaleX = fTextOverallScaleX * fTextOverallSizeScale;
const float fFinalTextScaleY = fTextOverallScaleY * fTextOverallSizeScale;

// Scaleform has some built in padding.  Remember this as an offset for when we come to render the actual text.
// This padding value is based on the space above and below the CAPITAL letters.
const float fScaleformTextFixedVerticalOffset = 2.0f;
const float fScaleformTextVerticalPaddingTop = fScaleformTextFixedVerticalOffset + (11.0f * fTextOverallScaleY);
const float fScaleformTextVerticalPaddingBottom = 13.0f * fTextOverallScaleY;

// Text Helper functions
float GetTextWidth(const char* pText)
{
	Assert(pText);

	if (CAnimSceneEditor::GetUseSmoothFont())
	{
		CTextLayout textLayout;
		textLayout.SetScale(Vector2(fFinalTextScaleX, fFinalTextScaleY));

		// Get width and scale back to pixels
		return textLayout.GetStringWidthOnScreen(pText, true) * (float)GRCDEVICE.GetWidth();
	}
	else
	{
		return (float)grcFont::GetCurrent().GetStringWidth(pText, (int)strlen(pText));
	}
}

float GetTextHeight()
{
	if (CAnimSceneEditor::GetUseSmoothFont())
	{
		CTextLayout textLayout;
		textLayout.SetScale(Vector2(fFinalTextScaleX, fFinalTextScaleY));

		return textLayout.GetCharacterHeight() * (float)GRCDEVICE.GetHeight();
	}
	else
	{
		return (float)grcFont::GetCurrent().GetHeight();
	}
}

void RenderText(const char* pText, float posX, float posY, const Color32& textColour)
{
	Assert(pText);

	grcLighting(false);

	if (CAnimSceneEditor::GetUseSmoothFont())
	{
		CTextLayout textLayout;
		textLayout.SetColor(textColour);
		textLayout.SetScale(Vector2(fFinalTextScaleX, fFinalTextScaleY));
		textLayout.SetWrap(Vector2(0.f,2.f));//don't wrap (only do so far past the right hand side of the screen)
		// TODO - get proper screen dimensions
		textLayout.Render(Vector2(posX * (1.0f / (float)GRCDEVICE.GetWidth()), (posY - fScaleformTextVerticalPaddingTop) * (1.0f / (float)GRCDEVICE.GetHeight())), pText);
		CText::Flush();
	}
	else
	{
		grcColor(textColour);
		grcDraw2dText(posX, posY, pText);
	}
}

void RenderTextWithBackground(const char* pText, float posX, float posY, const Color32& textColour, const Color32& backgroundColour, float fBackGroundPadding = 0.0f, bool renderOutline = false, const Color32& outlineColour = ASE_Editor_DefaultTextOutlineColor)
{
	Assert(pText);

	grcLighting(false);

	const float fFontHeight = GetTextHeight();
	const float fTextWidth = GetTextWidth(pText);

	// Render background
	if (fBackGroundPadding >= 0.0f)
	{
		grcColor(backgroundColour);
		grcBegin(drawTriStrip, 4);
		grcVertex2f(posX - fBackGroundPadding,				posY - fBackGroundPadding);
		grcVertex2f(posX + fBackGroundPadding + fTextWidth,	posY - fBackGroundPadding);
		grcVertex2f(posX - fBackGroundPadding,				posY + fBackGroundPadding + fFontHeight - (CAnimSceneEditor::GetUseSmoothFont() ? fScaleformTextVerticalPaddingBottom : 0.0f));
		grcVertex2f(posX + fBackGroundPadding + fTextWidth,	posY + fBackGroundPadding + fFontHeight - (CAnimSceneEditor::GetUseSmoothFont() ? fScaleformTextVerticalPaddingBottom : 0.0f));
		grcEnd();

		if(renderOutline)
		{
			grcColor(outlineColour);
			grcBegin(drawLineStrip, 5);
			grcVertex2f(posX - fBackGroundPadding,	posY - fBackGroundPadding);
			grcVertex2f(posX + fBackGroundPadding + fTextWidth,	posY - fBackGroundPadding);
			grcVertex2f(posX + fBackGroundPadding + fTextWidth,	posY + fBackGroundPadding + fFontHeight - (CAnimSceneEditor::GetUseSmoothFont() ? fScaleformTextVerticalPaddingBottom : 0.0f));
			grcVertex2f(posX - fBackGroundPadding,	posY + fBackGroundPadding + fFontHeight - (CAnimSceneEditor::GetUseSmoothFont() ? fScaleformTextVerticalPaddingBottom : 0.0f));
			grcVertex2f(posX - fBackGroundPadding,	posY - fBackGroundPadding);
			grcEnd();
		}
	}

	if (CAnimSceneEditor::GetUseSmoothFont())
	{
		CTextLayout textLayout;
		textLayout.SetColor(textColour);
		textLayout.SetScale(Vector2(fFinalTextScaleX, fFinalTextScaleY));
		textLayout.SetWrap(Vector2(0.f,2.f));//don't wrap (only do so far past the right hand side of the screen)
		// TODO - get proper screen dimensions
		textLayout.Render(Vector2(posX * (1.0f / (float)GRCDEVICE.GetWidth()), (posY - fScaleformTextVerticalPaddingTop) * (1.0f / (float)GRCDEVICE.GetHeight())), pText);
		CText::Flush();
	}
	else
	{
		grcColor(textColour);
		grcDraw2dText(posX, posY, pText);
	}
}

//////////////////////////////////////////////////////////////////////////
// CScreenExtents
//////////////////////////////////////////////////////////////////////////

void CScreenExtents::DrawBorder(Color32 color)
{
	grcColor(color);
	grcBegin(drawLineStrip, 5);
	grcVertex2f(m_min.x, m_min.y);
	grcVertex2f(m_min.x, m_max.y);
	grcVertex2f(m_max.x, m_max.y);
	grcVertex2f(m_max.x, m_min.y);
	grcVertex2f(m_min.x, m_min.y);
	grcEnd();
}

//--------------------------------------------------------------------------------
// CSceneInformationDisplay
//--------------------------------------------------------------------------------

CSceneInformationDisplay::CSceneInformationDisplay()
{

}

CSceneInformationDisplay::CSceneInformationDisplay(const CScreenExtents& screenExtents)
{
	m_screenExtents = screenExtents;
}

CSceneInformationDisplay::~CSceneInformationDisplay()
{

}

// PURPOSE: Set the screen extents of the display
void CSceneInformationDisplay::SetScreenExtents(const CScreenExtents& screenExtents)
{
	m_screenExtents = screenExtents;
}

// PURPOSE: Call once per frame to update the display
void CSceneInformationDisplay::Process(float fCurrentTime, float fTotalSceneTime)
{
	//////////////////////////////////////////////////////////////////////////
	// Render - background and outline
	//////////////////////////////////////////////////////////////////////////

	// Render dark background
	grcColor(GetColor(kDisplayBackgroundColor));
	grcBegin(drawTriStrip, 4);
	{
		grcVertex2f(m_screenExtents.GetMinX(),	m_screenExtents.GetMinY());
		grcVertex2f(m_screenExtents.GetMaxX(),	m_screenExtents.GetMinY());
		grcVertex2f(m_screenExtents.GetMinX(),	m_screenExtents.GetMaxY());
		grcVertex2f(m_screenExtents.GetMaxX(),	m_screenExtents.GetMaxY());
	}
	grcEnd();

	// Render display white outline
	grcColor(GetColor(kDisplayOutlineColor));
	grcBegin(drawLineStrip, 5);
	{
		grcVertex2f(m_screenExtents.GetMinX(), m_screenExtents.GetMinY());
		grcVertex2f(m_screenExtents.GetMaxX(), m_screenExtents.GetMinY());
		grcVertex2f(m_screenExtents.GetMaxX(), m_screenExtents.GetMaxY());
		grcVertex2f(m_screenExtents.GetMinX(), m_screenExtents.GetMaxY());
		grcVertex2f(m_screenExtents.GetMinX(), m_screenExtents.GetMinY());
	}
	grcEnd();

	//////////////////////////////////////////////////////////////////////////
	// Render - Information
	//////////////////////////////////////////////////////////////////////////

	const float fSidePadding = 3.0f;
	const float fTopLineY = m_screenExtents.GetMinY() + fSidePadding;
	const float fBottomLineY = fTopLineY + (((m_screenExtents.GetMaxY() - m_screenExtents.GetMinY()) - (fSidePadding * 2.0f)) * 0.5f);

	atVarString currentTimeStr("%.2f", fCurrentTime);
	atVarString totalTimeStr("/ %.2f", fTotalSceneTime);

	atVarString currentFrameStr("%d", (int)(fCurrentTime * 30.0f));
	atVarString totalFrameStr("/ %d", (int)(fTotalSceneTime * 30.0f));

	float fTotalTimeWidth =  GetTextWidth(totalTimeStr.c_str());
	float fTotalFrameWidth = GetTextWidth(totalFrameStr.c_str());

	float fTotalsX = m_screenExtents.GetMaxX() - fSidePadding - (fTotalTimeWidth > fTotalFrameWidth ? fTotalTimeWidth : fTotalFrameWidth);

	atString frameLabel("FRAME: ");
	atString timeLabel("TIME: ");

	float fCurrentsX = m_screenExtents.GetMinX() + GetTextWidth(frameLabel.c_str()) + fSidePadding;

	// Key
	RenderText(frameLabel.c_str(), m_screenExtents.GetMinX() + fSidePadding, fTopLineY, m_textColour);
	RenderText(timeLabel.c_str(), m_screenExtents.GetMinX() + fSidePadding, fBottomLineY, m_textColour);

	// Totals
	RenderText(totalFrameStr.c_str(), fTotalsX, fTopLineY, m_textColour);
	RenderText(totalTimeStr.c_str(), fTotalsX, fBottomLineY, m_textColour);

	// Current Times
	RenderText(currentFrameStr.c_str(), fCurrentsX, fTopLineY, m_textColour);
	RenderText(currentTimeStr.c_str(), fCurrentsX, fBottomLineY, m_textColour);
}

Color32 CSceneInformationDisplay::GetColor(eColorType type)
{
	switch (type)
	{
	case CSceneInformationDisplay::kDisplayOutlineColor:
		return Color32(200, 200, 200, 128);
	case CSceneInformationDisplay::kDisplayBackgroundColor:
		return Color32(0, 0, 0, 128);

	default:
		return Color32(0,0,0,0);
	}
}

//--------------------------------------------------------------------------------
// COnScreenTimeline
//--------------------------------------------------------------------------------

CAuthoringHelperDisplay::CAuthoringHelperDisplay()
{
	Init(); 
}

CAuthoringHelperDisplay::~CAuthoringHelperDisplay()
{

}

void CAuthoringHelperDisplay::Process()
{	
	Vector3 points[4];
	points[0].Set(m_screenExtents.GetMinX(), m_screenExtents.GetMinY(), 0.0f);
	points[1].Set(m_screenExtents.GetMinX(), m_screenExtents.GetMaxY(), 0.0f);
	points[2].Set(m_screenExtents.GetMaxX(), m_screenExtents.GetMaxY(), 0.0f);
	points[3].Set(m_screenExtents.GetMaxX(), m_screenExtents.GetMinY(), 0.0f);

	grcDrawPolygon(points, 4, NULL, true, GetColor(kVideoControlPanelBackgroundColor) ); 

	Vector3 borderPoints[4];
	borderPoints[0].Set(m_screenExtents.GetMinX(), m_screenExtents.GetMinY(), 0.0f);
	borderPoints[1].Set(m_screenExtents.GetMinX(), m_screenExtents.GetMaxY(), 0.0f);
	borderPoints[2].Set(m_screenExtents.GetMaxX(), m_screenExtents.GetMaxY(), 0.0f);
	borderPoints[3].Set(m_screenExtents.GetMaxX(), m_screenExtents.GetMinY(), 0.0f);

	grcDrawPolygon(borderPoints, 4, NULL, false, GetColor(kVideoControlPanelOutlineColor) ); 


	for(int i =0; i < kMaxNumButtons; i++)
	{
		m_buttons[i].Update(); 
		m_buttons[i].Draw(); 
	}
}

void CAuthoringHelperDisplay::SetScreenExtents(const CScreenExtents& screenExtents)
{
	TUNE_FLOAT(ButtonWidth, 50.0f, 0.0f, 100.0f, 1.0f)

	m_screenExtents = screenExtents;

	for(int i=0; i < kMaxNumButtons; i ++)
	{
		float minX = m_screenExtents.GetMinX() + (ButtonWidth * i); 
		float maxX = m_screenExtents.GetMinX() + (ButtonWidth * i) + ButtonWidth; 
		m_buttons[i].SetScreenExtents(CScreenExtents(minX, m_screenExtents.GetMinY(), maxX, m_screenExtents.GetMaxY())); 
	}
}

void CAuthoringHelperDisplay::SetColor(u32 colorType, Color32 color)
{
	if(colorType < kMaxNumColors)
	{
		m_colors[colorType] = color; 
	}
}

COnScreenButton* CAuthoringHelperDisplay::GetButton(const u32& button) 
{
	if(button < kMaxNumButtons)
	{
		return &m_buttons[button]; 
	}

	return NULL; 
}

Color32 CAuthoringHelperDisplay::GetColor(eColorType type)
{
	if(type < kMaxNumColors)
	{
		return m_colors[type];
	}

	return Color32(0,0,0,0);
}

bool CAuthoringHelperDisplay::DrawButtonBackground(const CScreenExtents& screenExtents, const u32& button)
{
	if(m_buttons[button].WasPressed())
	{
		Vector3 points[4];
		points[0].Set(screenExtents.GetMinX(), screenExtents.GetMinY(), 0.0f);
		points[1].Set(screenExtents.GetMinX(), screenExtents.GetMaxY(), 0.0f);
		points[2].Set(screenExtents.GetMaxX(), screenExtents.GetMaxY(), 0.0f);
		points[3].Set(screenExtents.GetMaxX(), screenExtents.GetMinY(), 0.0f);

		grcDrawPolygon(points, 4, NULL, false, GetColor(kVideoControlActiveButtonBackgroundColor) );
	}

	if(m_buttons[button].IsActive() || m_buttons[button].IsHeld())
	{
		Vector3 bgpoints[4];
		bgpoints[0].Set(screenExtents.GetMinX(), screenExtents.GetMinY(), 0.0f);
		bgpoints[1].Set(screenExtents.GetMinX(), screenExtents.GetMaxY(), 0.0f);
		bgpoints[2].Set(screenExtents.GetMaxX(), screenExtents.GetMaxY(), 0.0f);
		bgpoints[3].Set(screenExtents.GetMaxX(), screenExtents.GetMinY(), 0.0f);

		grcDrawPolygon(bgpoints, 4, NULL, true, GetColor(kVideoControlActiveButtonBackgroundColor) );

		Vector3 outlinePoints[4];
		outlinePoints[0].Set(screenExtents.GetMinX(), screenExtents.GetMinY(), 0.0f);
		outlinePoints[1].Set(screenExtents.GetMinX(), screenExtents.GetMaxY(), 0.0f);
		outlinePoints[2].Set(screenExtents.GetMaxX(), screenExtents.GetMaxY(), 0.0f);
		outlinePoints[3].Set(screenExtents.GetMaxX(), screenExtents.GetMinY(), 0.0f);

		grcDrawPolygon(outlinePoints, 4, NULL, false, GetColor(kVideoControlPanelActiveButtonOutlineColor) );

		return true; 
	}

	return false; 
}

void CAuthoringHelperDisplay::Init()
{
	for(int i=0; i < kMaxNumColors; i ++)
	{
		switch (i)
		{
		case CAuthoringHelperDisplay::kVideoControlPanelBackgroundColor:
			m_colors[i] =  Color32(0, 0, 0, 128);
		case CAuthoringHelperDisplay::kVideoControlButtonColor:
			m_colors[i] =  Color32(96, 96, 96, 128);
		case CAuthoringHelperDisplay::kVideoControlHoverButtonColor:
			m_colors[i] =  Color32(128, 128, 128, 128);
		case CAuthoringHelperDisplay::kVideoControlActiveButtonColor:
			m_colors[i] =  Color32(160, 160, 160, 128);
		case CAuthoringHelperDisplay::kVideoControlActiveButtonHighLigtColor: 
			m_colors[i] =  Color32(192, 192, 192, 128);
		case CAuthoringHelperDisplay::kVideoControlActiveButtonBackgroundColor: 
			m_colors[i] =  Color32(0, 0, 0, 160);
		case CAuthoringHelperDisplay::kVideoControlPanelOutlineColor:
			m_colors[i] =  Color32(128, 128, 128, 128);	
		}

	}

	for(int i=0; i < kMaxNumButtons; i ++)
	{
		switch (i)
		{
		case kTransGbl:
			{
				m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &CAuthoringHelperDisplay::DrawTransGbl))); 
			}
			break;

			case kTransLcl:
			{
			m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &CAuthoringHelperDisplay::DrawTransLcl))); 
			}
			break;

			case kRotLcl:
			{
			m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &CAuthoringHelperDisplay::DrawRotLcl))); 
			}
			break;

			case kRotGbl:
			{
			m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &CAuthoringHelperDisplay::DrawRotGbl))); 
			}
			break;
		}
	}
}

void CAuthoringHelperDisplay::DrawTransGbl(const CScreenExtents& screenExtents)
{
	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas);

	float borderOffset = 2.0f; 

	TUNE_FLOAT(Texty, 0.0f, 0.0f, 100.0f, 1.0f)

	float Height = GetTextHeight();
	float yOffset = (Canvas.GetHeight() - Height) / 2.0f;  

	if(DrawButtonBackground(Canvas, kTransGbl))
	{
		// Do not render the label if it's off the end of the timeline
		Color32 textCol = m_buttons[kTransGbl].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor);
		RenderText("T:Gbl", Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + yOffset, textCol);
	}
	else
	{
		Color32 textCol = m_buttons[kTransGbl].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor);
		RenderText("T:Gbl", Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + yOffset, textCol);
	}
}

void CAuthoringHelperDisplay::DrawTransLcl(const CScreenExtents& screenExtents)
{
	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas);

	float borderOffset = 2.0f; 

	float Height = GetTextHeight();
	float yOffset = (Canvas.GetHeight() - Height) / 2.0f;  

	if(DrawButtonBackground(Canvas, kTransLcl))
	{
		// Do not render the label if it's off the end of the timeline
		Color32 textCol = m_buttons[kTransLcl].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor);
		RenderText("T:Lcl", Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + yOffset, textCol);
	}
	else
	{
		Color32 textCol = m_buttons[kTransLcl].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor);
		RenderText("T:Lcl", Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + yOffset, textCol);
	}
}

void CAuthoringHelperDisplay::DrawRotLcl(const CScreenExtents& screenExtents)
{
	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas);

	float borderOffset = 2.0f; 

	TUNE_FLOAT(Texty, 0.0f, 0.0f, 100.0f, 1.0f)

	float Height = GetTextHeight();
	float yOffset = (Canvas.GetHeight() - Height) / 2.0f;  

	if(DrawButtonBackground(Canvas, kRotLcl))
	{
		// Do not render the label if it's off the end of the timeline
		Color32 textCol = m_buttons[kRotLcl].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor);
		RenderText("R:Lcl", Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + yOffset, textCol);
	}
	else
	{
		Color32 textCol = m_buttons[kRotLcl].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor);
		RenderText("R:Lcl", Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + yOffset, textCol);
	}
}

void CAuthoringHelperDisplay::DrawRotGbl(const CScreenExtents& screenExtents)
{
	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas);

	float borderOffset = 2.0f; 
	
	float Height = GetTextHeight();
	float yOffset = (Canvas.GetHeight() - Height) / 2.0f; 

	TUNE_FLOAT(Texty, 0.0f, 0.0f, 100.0f, 1.0f)

	if(DrawButtonBackground(Canvas, kRotGbl))
	{
		// Do not render the label if it's off the end of the timeline
		Color32 textCol = m_buttons[kRotGbl].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor);
		RenderText("R:Gbl", Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + yOffset, textCol);
	}
	else
	{
		Color32 textCol = m_buttons[kRotGbl].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor);
		RenderText("R:Gbl", Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + yOffset, textCol);
	}
}


void CAuthoringHelperDisplay::SetCanvasSize(CScreenExtents& screenExtents)
{
	TUNE_FLOAT(CanvasOffset, 2.0f, 0.0f, 100.0f, 1.0f)

	screenExtents.Set(screenExtents.GetMinX() + CanvasOffset, screenExtents.GetMinY() + CanvasOffset, screenExtents.GetMaxX() - CanvasOffset, screenExtents.GetMaxY() - CanvasOffset); 
}



COnScreenButton::COnScreenButton()
:m_drawButtonCB(NULL)
,m_buttonCB(NULL)
,m_isActive(false)
,m_isHeld(false)
,m_isFocus(false)
,m_wasPressed(false)
{

}

COnScreenButton::COnScreenButton(const CScreenExtents& screenExtents)
:m_drawButtonCB(NULL)
,m_buttonCB(NULL)
,m_isActive(false)
,m_isHeld(false)
,m_isFocus(false)
,m_wasPressed(false)
{
	m_screenExtents = screenExtents; 
}

void COnScreenButton::Draw()
{
	if (m_drawButtonCB)
	{
		m_drawButtonCB(m_screenExtents);
	}
}


bool COnScreenButton::IsHoveringOver()
{
	float mouseX = static_cast<float>(ioMouse::GetX());
	float mouseY = static_cast<float>(ioMouse::GetY());

	bool hoveringOver = (mouseX>m_screenExtents.GetMinX() && (mouseX<m_screenExtents.GetMaxX()))
		&& (mouseY>m_screenExtents.GetMinY() && mouseY<m_screenExtents.GetMaxY());

	return hoveringOver; 

}

void COnScreenButton::Update()
{
	bool leftMouseClicked = ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT;
	bool leftMouseHeld = ioMouse::GetButtons() & ioMouse::MOUSE_LEFT;
	bool leftMouseReleased = ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT;
	m_isHeld = false; 

	//hovering over the button
	if(IsHoveringOver())
	{
		//if clicked set to focus
		if(leftMouseClicked && !m_isFocus)
		{
			m_isFocus = true; 
		}
		
		//if the focus button and hovering change active state
		if(m_isFocus && leftMouseReleased)
		{
			m_isActive = !m_isActive; 
			
			if(m_isActive)
			{
				m_isActive = m_buttonCB(kButtonPressed);
			}
			else
			{
				m_isActive = m_buttonCB(kButtonReleased);
			}
			m_wasPressed = true; 
			
			m_isFocus = m_isActive; 
		}
	}


	if(m_isFocus && (leftMouseClicked || leftMouseHeld))
	{
		m_isHeld = true; 
	}


	if(m_buttonCB && m_isActive)
	{
		m_isActive = m_buttonCB(kButtonHeld);

		if(!m_isActive)
		{
			m_isFocus = false; 
			m_wasPressed = false; 
		}
	}

	if(!IsHoveringOver())
	{
		if(leftMouseClicked || leftMouseReleased)
		{
			if(!m_isActive)
			{
				m_wasPressed = false; 
				m_isFocus = false; 
			}
		}
	}
}

COnScreenVideoControl::COnScreenVideoControl()
:m_renderSolid(true)
{
	 Init(); 
}

COnScreenVideoControl::~COnScreenVideoControl()
{

}

void COnScreenVideoControl::SetScreenExtents(const CScreenExtents& screenExtents)
{
	TUNE_FLOAT(ButtonWidth, 20.0f, 0.0f, 100.0f, 1.0f)

	m_screenExtents = screenExtents;

	for(int i=0; i < kMaxNumButtons; i ++)
	{
		float minX = m_screenExtents.GetMinX() + (ButtonWidth * i); 
		float maxX = m_screenExtents.GetMinX() + (ButtonWidth * i) + ButtonWidth; 
		m_buttons[i].SetScreenExtents(CScreenExtents(minX, m_screenExtents.GetMinY(), maxX, m_screenExtents.GetMaxY())); 
	}
}

void COnScreenVideoControl::SetColor(u32 colorType, Color32 color)
{
	if(colorType < kMaxNumColors)
	{
		m_colors[colorType] = color; 
	}
}

void COnScreenVideoControl::Init()
{
	//for(int i=0; i < kMaxNumColors; i ++)
	//{
	//	switch (i)
	//	{
	//	case COnScreenVideoControl::kVideoControlSelectedButtonOutline:
	//		m_colors[i] = Color32(256, 0, 0, 160);
	//	case COnScreenVideoControl::kVideoControlBackgroundColor:
	//		m_colors[i] =  Color32(0, 0, 0, 128);
	//	case COnScreenVideoControl::kVideoControlButtonColor:
	//		m_colors[i] =  Color32(96, 96, 96, 128);
	//	case COnScreenVideoControl::kVideoControlHoverButtonColor:
	//		m_colors[i] =  Color32(128, 128, 128, 128);
	//	case COnScreenVideoControl::kVideoControlActiveButtonColor:
	//		m_colors[i] =  Color32(160, 160, 160, 128);
	//	case COnScreenVideoControl::kVideoControlActiveButtonHighLigtColor: 
	//		m_colors[i] =  Color32(192, 192, 192, 128);
	//	case COnScreenVideoControl::kVideoControlActiveButtonBackgroundColor: 
	//		m_colors[i] =  Color32(0, 0, 0, 160);
	//	case COnScreenVideoControl::kVideoControlDisplayOutlineColor:
	//		m_colors[i] =  Color32(128, 128, 128, 128);	
	//	case COnScreenVideoControl::kVideoControlPanelActiveButtonOutlineColor: 
	//		m_colors[i] =  Color32(0, 0, 0, 160);
	//	case COnScreenVideoControl::kVideoControlPanelButtonOutlineColor: 
	//		m_colors[i] =  Color32(0, 0, 0, 160);
	//	}
	//}

	for(int i=0; i < kMaxNumButtons; i ++)
	{
		switch (i)
		{
		case kPlayForwards:
			{
				m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &COnScreenVideoControl::DrawPlayForwardsButton))); 
			}
			break;
	
		case kStop:
			{
				m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &COnScreenVideoControl::DrawStopButton))); 
			}
			break;

		case kPlayBackwards:
			{
				m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &COnScreenVideoControl::DrawPlayBackwardsButton))); 
			}
			break;

		case kStepBackwards:
			{
				m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &COnScreenVideoControl::DrawStepBackwardsButton))); 
			}
			break;

		case kStepForwards:
			{
				m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &COnScreenVideoControl::DrawStepForwardsButton))); 
			}
			break;
			
		case kStepToEnd:
			{
				m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &COnScreenVideoControl::DrawStepToEndButton))); 
			}
			break; 
		case kStepToStart:
			{
				m_buttons[i].SetButtonDrawCallBack((MakeFunctor(*this, &COnScreenVideoControl::DrawStepToStartButton))); 
			}
			break;
		}
	}
}

COnScreenButton* COnScreenVideoControl::GetButton(const u32& button) 
{
	if(button < kMaxNumButtons)
	{
		return &m_buttons[button]; 
	}

	return NULL; 
}

void COnScreenVideoControl::Process()
{	
	Vector3 points[4];
	points[0].Set(m_screenExtents.GetMinX(), m_screenExtents.GetMinY(), 0.0f);
	points[1].Set(m_screenExtents.GetMinX(), m_screenExtents.GetMaxY(), 0.0f);
	points[2].Set(m_screenExtents.GetMaxX(), m_screenExtents.GetMaxY(), 0.0f);
	points[3].Set(m_screenExtents.GetMaxX(), m_screenExtents.GetMinY(), 0.0f);

	grcDrawPolygon(points, 4, NULL, true, GetColor(kVideoControlBackgroundColor) ); 
	
	Vector3 borderPoints[4];
	borderPoints[0].Set(m_screenExtents.GetMinX(), m_screenExtents.GetMinY(), 0.0f);
	borderPoints[1].Set(m_screenExtents.GetMinX(), m_screenExtents.GetMaxY(), 0.0f);
	borderPoints[2].Set(m_screenExtents.GetMaxX(), m_screenExtents.GetMaxY(), 0.0f);
	borderPoints[3].Set(m_screenExtents.GetMaxX(), m_screenExtents.GetMinY(), 0.0f);

	grcDrawPolygon(borderPoints, 4, NULL, false, GetColor(kVideoControlDisplayOutlineColor) ); 
	
	for(int i =0; i < kMaxNumButtons; i++)
	{
		m_buttons[i].Update(); 
		m_buttons[i].Draw(); 
	}

}

// PURPOSE: Gets the colour of a particular element
Color32 COnScreenVideoControl::GetColor(eColorType type)
{
	if(type < kMaxNumColors)
	{
		return m_colors[type];
	}

	return Color32(0,0,0,0);
}

void COnScreenVideoControl::SetCanvasSize(CScreenExtents& screenExtents)
{
	TUNE_FLOAT(CanvasOffset, 2.0f, 0.0f, 100.0f, 1.0f)

	screenExtents.Set(screenExtents.GetMinX() + CanvasOffset, screenExtents.GetMinY() + CanvasOffset, screenExtents.GetMaxX() - CanvasOffset, screenExtents.GetMaxY() - CanvasOffset); 
}

void COnScreenVideoControl::DrawStepToStartButton(const CScreenExtents& screenExtents)
{
	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas); 

	TUNE_FLOAT(borderOffset, 2.0f, 0.0f, 100.0f, 1.0f)
	TUNE_FLOAT(barwidthOffset, 2.0f, 0.0f, 100.0f, 1.0f)
	TUNE_FLOAT(firstArrowPecent, 0.56f, 0.0f, 5.0f, 0.01f)
	TUNE_FLOAT(secondArrowPecent, 0.44f, 0.0f, 5.0f, 0.01f)

		//vertical bar
	Vector3 points[4];
	points[0].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	points[1].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[2].Set(Canvas.GetMinX() + borderOffset + barwidthOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[3].Set(Canvas.GetMinX() + borderOffset + barwidthOffset, Canvas.GetMinY() + borderOffset, 0.0f);

	Vector3 firstArrowpoints[3];
	firstArrowpoints[0].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	firstArrowpoints[1].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	firstArrowpoints[2].Set(Canvas.GetMinX() + (Canvas.GetWidth() * firstArrowPecent), (Canvas.GetMinY() + borderOffset) + (((Canvas.GetMaxY() - borderOffset) - (Canvas.GetMinY() + borderOffset)) / 2.0f) , 0.0f);

	Vector3 secondArrowpoints[3];
	secondArrowpoints[0].Set(Canvas.GetMaxX() - (Canvas.GetWidth() * secondArrowPecent), Canvas.GetMinY() + borderOffset, 0.0f);
	secondArrowpoints[1].Set(Canvas.GetMaxX() - (Canvas.GetWidth() * secondArrowPecent), Canvas.GetMaxY() - borderOffset, 0.0f);
	secondArrowpoints[2].Set(Canvas.GetMinX() + borderOffset + barwidthOffset, (Canvas.GetMinY() + borderOffset) + (((Canvas.GetMaxY() - borderOffset) - (Canvas.GetMinY() + borderOffset)) / 2.0f) , 0.0f);

	if(DrawButtonBackground(Canvas, kStepToStart))
	{
		grcDrawPolygon(points, 4, NULL, m_renderSolid, m_buttons[kStepToStart].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor));
		grcDrawPolygon(firstArrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepToStart].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor));
		grcDrawPolygon(secondArrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepToStart].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor));
	}
	else
	{
		grcDrawPolygon(points, 4, NULL, m_renderSolid, m_buttons[kStepToStart].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));
		grcDrawPolygon(firstArrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepToStart].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));
		grcDrawPolygon(secondArrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepToStart].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));

	}
}


void COnScreenVideoControl::DrawStepBackwardsButton(const CScreenExtents& screenExtents)
{
	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas); 

	TUNE_FLOAT(borderOffset, 2.0f, 0.0f, 100.0f, 1.0f)
		TUNE_FLOAT(barwidthOffset, 2.0f, 0.0f, 100.0f, 1.0f)

		//vertical bar
		Vector3 points[4];
	points[0].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	points[1].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[2].Set(Canvas.GetMinX() + borderOffset + barwidthOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[3].Set(Canvas.GetMinX() + borderOffset + barwidthOffset, Canvas.GetMinY() + borderOffset, 0.0f);

	Vector3 arrowpoints[3];
	arrowpoints[0].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	arrowpoints[1].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	arrowpoints[2].Set(Canvas.GetMinX() + borderOffset + barwidthOffset, (Canvas.GetMinY() + borderOffset) + (((Canvas.GetMaxY() - borderOffset) - (Canvas.GetMinY() + borderOffset)) / 2.0f) , 0.0f);

	if(DrawButtonBackground(Canvas, kStepBackwards))
	{
		grcDrawPolygon(arrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepBackwards].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor));
		grcDrawPolygon(points, 4, NULL, m_renderSolid, m_buttons[kStepBackwards].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor));
	}
	else
	{
		grcDrawPolygon(arrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepBackwards].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));
		grcDrawPolygon(points, 4, NULL, m_renderSolid, m_buttons[kStepBackwards].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));
	}
}

void COnScreenVideoControl::DrawPlayForwardsButton(const CScreenExtents& screenExtents)
{
	TUNE_FLOAT(borderOffset, 2.0f, 0.0f, 100.0f, 1.0f)

	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas);

	//draw an arrow 
	Vector3 points[3];
	points[0].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	points[1].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[2].Set(Canvas.GetMaxX() - borderOffset, (Canvas.GetMinY() + borderOffset) + (((Canvas.GetMaxY() - borderOffset) - (Canvas.GetMinY() + borderOffset)) / 2.0f) , 0.0f);

	if(DrawButtonBackground(Canvas, kPlayForwards))
	{
		grcDrawPolygon(points, 3, NULL, m_renderSolid, m_buttons[kPlayForwards].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor));
	}
	else
	{
		grcDrawPolygon(points, 3, NULL, m_renderSolid, m_buttons[kPlayForwards].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));
	}

}

void COnScreenVideoControl::DrawPlayBackwardsButton(const CScreenExtents& screenExtents)
{
	TUNE_FLOAT(borderOffset, 2.0f, 0.0f, 100.0f, 1.0f)

	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas);

	//draw an arrow 
	Vector3 points[3];
	points[0].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	points[1].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[2].Set(Canvas.GetMinX() + borderOffset, (Canvas.GetMinY() + borderOffset) + (((Canvas.GetMaxY() - borderOffset) - (Canvas.GetMinY() + borderOffset)) / 2.0f) , 0.0f);

	if(DrawButtonBackground(Canvas, kPlayBackwards))
	{
		grcDrawPolygon(points, 3, NULL, m_renderSolid, m_buttons[kPlayBackwards].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor));
	}
	else
	{
		grcDrawPolygon(points, 3, NULL, m_renderSolid, m_buttons[kPlayBackwards].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));
	}
}

void COnScreenVideoControl::DrawStopButton(const CScreenExtents& screenExtents)
{
	TUNE_FLOAT(borderOffset, 2.0f, 0.0f, 100.0f, 1.0f)

	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas);

	Vector3 points[4];
	points[0].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	points[1].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[2].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[3].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);

	if(DrawButtonBackground(Canvas, kStop))
	{
		grcDrawPolygon(points, 4, NULL, m_renderSolid, m_buttons[kStop].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor));
	}
	else
	{
		grcDrawPolygon(points, 4, NULL, m_renderSolid, m_buttons[kStop].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));
	}
}
void COnScreenVideoControl::DrawStepForwardsButton(const CScreenExtents& screenExtents)
{
	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas); 

	TUNE_FLOAT(borderOffset, 2.0f, 0.0f, 100.0f, 1.0f)
	TUNE_FLOAT(barwidthOffset, 2.0f, 0.0f, 100.0f, 1.0f)

	//vertical bar
	Vector3 points[4];
	points[0].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	points[1].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[2].Set(Canvas.GetMaxX() - borderOffset - barwidthOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[3].Set(Canvas.GetMaxX() - borderOffset - barwidthOffset, Canvas.GetMinY() + borderOffset, 0.0f);

	Vector3 arrowpoints[3];
	arrowpoints[0].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	arrowpoints[1].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	arrowpoints[2].Set(Canvas.GetMaxX() - borderOffset - barwidthOffset, (Canvas.GetMinY() + borderOffset) + (((Canvas.GetMaxY() - borderOffset) - (Canvas.GetMinY() + borderOffset)) / 2.0f) , 0.0f);

	if(DrawButtonBackground(Canvas, kStepForwards))
	{
		grcDrawPolygon(arrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepForwards].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor));
		grcDrawPolygon(points, 4, NULL, m_renderSolid, m_buttons[kStepForwards].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonColor));
	}
	else
	{
		grcDrawPolygon(arrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepForwards].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));
		grcDrawPolygon(points, 4, NULL, m_renderSolid, m_buttons[kStepForwards].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));
	}
}

void COnScreenVideoControl::DrawStepToEndButton(const CScreenExtents& screenExtents)
{
	CScreenExtents Canvas(screenExtents.GetMin(), screenExtents.GetMax()); 
	SetCanvasSize(Canvas); 

	TUNE_FLOAT(borderOffset, 2.0f, 0.0f, 100.0f, 1.0f)
	TUNE_FLOAT(barwidthOffset, 2.0f, 0.0f, 100.0f, 1.0f)
	TUNE_FLOAT(firstArrowPecent, 0.56f, 0.0f, 5.0f, 0.01f)
	TUNE_FLOAT(secondArrowPecent, 0.44f, 0.0f, 5.0f, 0.01f)

	//vertical bar
	Vector3 points[4];
	points[0].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	points[1].Set(Canvas.GetMaxX() - borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[2].Set(Canvas.GetMaxX() - borderOffset - barwidthOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	points[3].Set(Canvas.GetMaxX() - borderOffset - barwidthOffset, Canvas.GetMinY() + borderOffset, 0.0f);

	Vector3 firstArrowpoints[3];
	firstArrowpoints[0].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMinY() + borderOffset, 0.0f);
	firstArrowpoints[1].Set(Canvas.GetMinX() + borderOffset, Canvas.GetMaxY() - borderOffset, 0.0f);
	firstArrowpoints[2].Set(Canvas.GetMaxX() - (Canvas.GetWidth() * firstArrowPecent) , (Canvas.GetMinY() + borderOffset) + (((Canvas.GetMaxY() - borderOffset) - (Canvas.GetMinY() + borderOffset)) / 2.0f) , 0.0f);


	Vector3 secondArrowpoints[3];
	secondArrowpoints[0].Set(Canvas.GetMinX() + (Canvas.GetWidth() * secondArrowPecent), Canvas.GetMinY() + borderOffset, 0.0f);
	secondArrowpoints[1].Set(Canvas.GetMinX() + (Canvas.GetWidth() * secondArrowPecent), Canvas.GetMaxY() - borderOffset, 0.0f);
	secondArrowpoints[2].Set(Canvas.GetMaxX() - borderOffset - barwidthOffset, (Canvas.GetMinY() + borderOffset) + (((Canvas.GetMaxY() - borderOffset) - (Canvas.GetMinY() + borderOffset)) / 2.0f) , 0.0f);

	
	if(DrawButtonBackground(Canvas, kStepToEnd))
	{
		grcDrawPolygon(points, 4, NULL, m_renderSolid, m_buttons[kStepToEnd].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonHighLigtColor));

		grcDrawPolygon(firstArrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepToEnd].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonHighLigtColor));

		grcDrawPolygon(secondArrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepToEnd].IsHoveringOver() ? GetColor(kVideoControlActiveButtonHighLigtColor) : GetColor(kVideoControlActiveButtonHighLigtColor));
	}
	else
	{
		grcDrawPolygon(points, 4, NULL, m_renderSolid, m_buttons[kStepToEnd].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));

		grcDrawPolygon(firstArrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepToEnd].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));

		grcDrawPolygon(secondArrowpoints, 3, NULL, m_renderSolid, m_buttons[kStepToEnd].IsHoveringOver() ? GetColor(kVideoControlHoverButtonColor) : GetColor(kVideoControlButtonColor));
	}
}

bool COnScreenVideoControl::DrawButtonBackground(const CScreenExtents& screenExtents, const u32& button)
{
	if(m_buttons[button].WasPressed())
	{
		Vector3 outlinePoints[4];
		outlinePoints[0].Set(screenExtents.GetMinX(), screenExtents.GetMinY(), 0.0f);
		outlinePoints[1].Set(screenExtents.GetMinX(), screenExtents.GetMaxY(), 0.0f);
		outlinePoints[2].Set(screenExtents.GetMaxX(), screenExtents.GetMaxY(), 0.0f);
		outlinePoints[3].Set(screenExtents.GetMaxX(), screenExtents.GetMinY(), 0.0f);

		grcDrawPolygon(outlinePoints, 4, NULL, false, GetColor(kVideoControlPanelButtonOutlineColor) );
	}

	if(m_buttons[button].IsActive() || m_buttons[button].IsHeld())
	{
		Vector3 bgpoints[4];
		bgpoints[0].Set(screenExtents.GetMinX(), screenExtents.GetMinY(), 0.0f);
		bgpoints[1].Set(screenExtents.GetMinX(), screenExtents.GetMaxY(), 0.0f);
		bgpoints[2].Set(screenExtents.GetMaxX(), screenExtents.GetMaxY(), 0.0f);
		bgpoints[3].Set(screenExtents.GetMaxX(), screenExtents.GetMinY(), 0.0f);

		grcDrawPolygon(bgpoints, 4, NULL, true, GetColor(kVideoControlActiveButtonBackgroundColor) );

		Vector3 outlinePoints[4];
		outlinePoints[0].Set(screenExtents.GetMinX(), screenExtents.GetMinY(), 0.0f);
		outlinePoints[1].Set(screenExtents.GetMinX(), screenExtents.GetMaxY(), 0.0f);
		outlinePoints[2].Set(screenExtents.GetMaxX(), screenExtents.GetMaxY(), 0.0f);
		outlinePoints[3].Set(screenExtents.GetMaxX(), screenExtents.GetMinY(), 0.0f);

		grcDrawPolygon(outlinePoints, 4, NULL, false, GetColor(kVideoControlPanelActiveButtonOutlineColor) );

		return true; 
	}

	return false; 
}

CEventPanel::CEventPanel(EventInfo info)
:m_event(info)
,m_eventHeight(10.0f)
,m_offscreenArrowMarker(5.0f)
{
}

CEventPanel::~CEventPanel()
{
}

void CEventPanel::RenderToolsOverlay(CAnimSceneEditor* animSceneEditor, CScreenExtents& GroupSceenExtents, CEventPanelGroup* PanelGroup, int colourId)
{
	if(m_event.event)
	{
		RenderEventToolsOverlay(animSceneEditor, m_event, GroupSceenExtents, PanelGroup, colourId); 
	}
}

//Render the blend in/out markers
void CEventPanel::RenderEventToolsOverlay(CAnimSceneEditor* animSceneEditor, EventInfo& info, CScreenExtents& GroupSceenExtents, CEventPanelGroup* PanelGroup, int colourId)
{
	static float s_boldLineSize = 1.f;
	static float s_minTagWidth = 2.0f;

	CAnimSceneEvent* evt = info.event;
	if (evt)
	{
		//////////////////////////////////////////////////////////////////////////
		// Get screen space information for the event
		//////////////////////////////////////////////////////////////////////////
		float startX = animSceneEditor->GetTimeLine().GetScreenX(evt->GetStart());
		float endX = animSceneEditor->GetTimeLine().GetScreenX(evt->GetEnd());
		float blendInStartX = animSceneEditor->GetTimeLine().GetScreenX(evt->GetStart());

		//float groupSpacing = PanelGroup->GetGroupSpacing();
		bool endXPastRight = false; bool startXPastLeft = false;
		float arrowSize = 	Clamp(PanelGroup->GetGroupHeight() / 8.0f, 1.5f, 5.0f);

		//Handle colour when selected
		Color32 eventColor;
		evt->GetEditorColor(eventColor, *animSceneEditor, colourId);
		bool isSelected = (evt==animSceneEditor->GetSelectedEvent());
		if(isSelected)
		{
			eventColor = animSceneEditor->GetColor(CAnimSceneEditor::kSelectedEventMarkerColor);
		}
		eventColor.SetAlpha(255);

		//Detect event extents being off the drawable area
		if(endX > GroupSceenExtents.GetMaxX())
		{
			endXPastRight = true;
		}

		if(startX < GroupSceenExtents.GetMinX())
		{
			startXPastLeft = true;
		}

		// Get the blend in and out information
		bool hasBlendIn = false; float blendInDuration = 0.f;  float blendInEndX = 0.f;
		bool hasBlendOut = false;float blendOutDuration = 0.f; float blendOutEndX = 0.f;
		if(evt->GetType() == AnimSceneEventType::PlayAnimEvent)
		{
			CAnimScenePlayAnimEvent* evtPA = static_cast<CAnimScenePlayAnimEvent*>(evt);
			blendInDuration = evtPA->GetPoseBlendInDuration();
			blendOutDuration = evtPA->GetPoseBlendOutDuration();
			if(blendInDuration > 0.f)
			{
				hasBlendIn = true;
				blendInEndX = animSceneEditor->GetTimeLine().GetScreenX(blendInDuration + evtPA->GetStart());
				if(blendInEndX < GroupSceenExtents.GetMinX())
				{
					hasBlendIn = false;
				}
			}
			if(blendOutDuration > 0.f)
			{
				hasBlendOut = true;
				blendOutEndX = animSceneEditor->GetTimeLine().GetScreenX(blendOutDuration + evtPA->GetEnd());
				if(blendOutEndX < endX || blendOutEndX < GroupSceenExtents.GetMinX())
				{
					hasBlendOut = false;
				}
			}			
		}//end 	if(evt->GetType() == AnimSceneEventType::PlayAnimEvent)
		//Camera blending support
		if(evt->GetType() == AnimSceneEventType::PlayCameraAnimEvent)
		{
			CAnimScenePlayCameraAnimEvent* evtPA = static_cast<CAnimScenePlayCameraAnimEvent*>(evt);
			blendInDuration = evtPA->GetBlendInDuration();
			blendOutDuration = evtPA->GetBlendOutDuration();
			if(blendInDuration > 0.f)
			{
				hasBlendIn = true;
				blendInEndX = animSceneEditor->GetTimeLine().GetScreenX(blendInDuration + evtPA->GetStart());
				if(blendInEndX < GroupSceenExtents.GetMinX())
				{
					hasBlendIn = false;
				}
			}
			if(blendOutDuration > 0.f)
			{
				hasBlendOut = true;
				blendOutEndX = animSceneEditor->GetTimeLine().GetScreenX(blendOutDuration + evtPA->GetEnd());
				if(blendOutEndX < endX || blendOutEndX < GroupSceenExtents.GetMinX())
				{
					hasBlendOut = false;
				}
			}			
		}
		
		// bail for events offscreen, and handle their blendouts
		if (endX < GroupSceenExtents.GetMinX())
		{
			if(hasBlendOut)
			{
				//calculate intersection of line from bottom of event start to final location
				Vector3 blendStart(endX,  GroupSceenExtents.GetMaxY(), 0.0f);
				Vector3 blendEnd(blendOutEndX,GroupSceenExtents.GetMinY(), 0.0f);
				
				//y = mx + c
				//computing m
				float xdiff = endX - blendOutEndX;
				float ydiff = GroupSceenExtents.GetMaxY() - GroupSceenExtents.GetMinY();
				float m = ydiff / xdiff;
				//c = y - mx
				float c = GroupSceenExtents.GetMaxY() - m*endX;
				//y value at point
				float blendOutStartY = m*GroupSceenExtents.GetMinX() + c;

				//Draw white background triangle
				Color32 boColour = animSceneEditor->GetColor(CAnimSceneEditor::kEventBackgroundColor);
				boColour.SetAlpha((int) (CAnimSceneEditor::ms_editorSettingBackgroundAlpha*255));
				Vector3 points[4]; //buffer for (up to 4 point) polygons
				points[0].Set(GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMinY(), 0.0f);//top left
				points[1].Set(blendOutEndX, GroupSceenExtents.GetMinY(), 0.0f);//Arrow past right of event
				points[2].Set(GroupSceenExtents.GetMinX(), blendOutStartY, 0.0f);//bottom left
				grcDrawPolygon(points, 3, NULL, true, boColour);	

				//Draw coloured line indicating blend amount
				if(CAnimSceneEditor::ms_editorSettingUseEventColourForMarker)
				{
					boColour = eventColor;
				}
				else
				{
					boColour = animSceneEditor->GetColor(CAnimSceneEditor::kBlendOutMarkerColor);
				}
				boColour.SetAlpha(255);
				grcDrawLine(points[1],points[2],boColour,boColour);

				//Coloured line along the top
				points[0].Set(GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMinY(), 0.0f);//top right upper
				points[1].Set(blendOutEndX, GroupSceenExtents.GetMinY(), 0.0f);//Arrow past right of event (up and to the right) upper
				points[2].Set(blendOutEndX, GroupSceenExtents.GetMinY() - s_boldLineSize, 0.0f);//Arrow past right of event (up and to the right) lower
				points[3].Set(GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMinY() - s_boldLineSize, 0.0f);//bottom left
				grcDrawPolygon(points, 4, NULL, TRUE, boColour);

				//Coloured Triangle (denoting end of blend out)
				points[0].Set(blendOutEndX - 3*arrowSize, GroupSceenExtents.GetMinY(), 0.0f);
				points[1].Set(blendOutEndX + 3*arrowSize, GroupSceenExtents.GetMinY(), 0.0f);
				points[2].Set(blendOutEndX, GroupSceenExtents.GetMinY() + 3*arrowSize, 0.0f);
				grcDrawPolygon(points, 3, NULL, TRUE, boColour);

				//Draw a dashed line under the block to indicate final clip length
				points[0].Set(blendOutEndX, GroupSceenExtents.GetMinY(),0.f);
				points[1].Set(blendOutEndX, GroupSceenExtents.GetMaxY(),0);
				DrawDashedLine(points[0],points[1],boColour,5);
			}	
			return;
		}			

		// Ignore drawing of tools if off the end of the right hand side of the screen
		if (startX > GroupSceenExtents.GetMaxX())
		{
			return;
		}

		// Clamp to drawable range
		startX = Clamp(startX, GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMaxX());
		endX = Clamp(endX, GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMaxX());
		CScreenExtents EventScreenExtents(startX, GroupSceenExtents.GetMinY(), endX, GroupSceenExtents.GetMaxY()); 

		bool isDurationless = false;

		// make sure there's always at least a thin line
		if (endX - startX<s_minTagWidth)
		{
			isDurationless = true;
			endX = startX + s_minTagWidth;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Properties
		//////////////////////////////////////////////////////////////////////////
		float mouseX = static_cast<float>(ioMouse::GetX());
		float mouseY = static_cast<float>(ioMouse::GetY());
		bool hoveringEvent = EventScreenExtents.Contains(mouseX, mouseY);

		//////////////////////////////////////////////////////////////////////////
		// Render
		//////////////////////////////////////////////////////////////////////////
		Vector3 points[4];
		eventColor.SetAlpha(255);

		if (hoveringEvent && !isSelected)
		{
			eventColor.SetRed(Clamp(eventColor.GetRed() + 25, 0, 255)); 
			eventColor.SetBlue(Clamp(eventColor.GetBlue() + 25, 0, 255));
			eventColor.SetGreen(Clamp(eventColor.GetGreen() + 25, 0, 255));
		}

		//Draw blend in - white triangles, coloured ramps and indicators
		//There is an assumption that a blend will not go across the whole timeline here.
		if(hasBlendIn)
		{
			Color32 biColour = animSceneEditor->GetColor(CAnimSceneEditor::kEventBackgroundColor);
			biColour.SetAlpha((int) (CAnimSceneEditor::ms_editorSettingBackgroundAlpha*255) );
			//Handle being only partially in the scene (left side)
			if(blendInStartX < GroupSceenExtents.GetMinX())
			{
				//Line equation: y = mx + c
				//computing m
				float xdiff = blendInStartX - blendInEndX;
				float ydiff = GroupSceenExtents.GetMaxY() - GroupSceenExtents.GetMinY();
				float m = ydiff / xdiff;
				//c = y - mx
				float c = GroupSceenExtents.GetMaxY() - m*blendInStartX;
				//y value at point
				float blendInStartY = m*GroupSceenExtents.GetMinX() + c;

				//Draw blend in partial triangle
				Vector3 points[4];
				points[0].Set(GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMaxY(), 0.0f);//bottom left
				points[1].Set(GroupSceenExtents.GetMinX(),blendInStartY, 0.0f);//start height
				points[2].Set(blendInEndX, GroupSceenExtents.GetMinY(), 0.0f);//start of fully blended in
				points[3].Set(blendInEndX, GroupSceenExtents.GetMaxY(), 0.0f);//bottom right
				grcDrawPolygon(points, 4, NULL, true,biColour);
				if(!CAnimSceneEditor::ms_editorSettingUseEventColourForMarker)
				{
					biColour = animSceneEditor->GetColor(CAnimSceneEditor::kBlendInMarkerColor);
				}
				else
				{
					biColour = eventColor;
				}
				//Draw coloured blend amount
				grcDrawLine(points[1],points[2],biColour,biColour);


				if(!CAnimSceneEditor::ms_editorSettingUseEventColourForMarker)
				{
					biColour = animSceneEditor->GetColor(CAnimSceneEditor::kBlendInMarkerColor);
				}
				biColour.SetAlpha(255);

				//Along the bottom
				points[0].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY(), 0.0f);//top left upper
				points[1].Set(blendInEndX, EventScreenExtents.GetMaxY(), 0.0f);//Arrow past right of event (up and to the right) upper
				points[2].Set(blendInEndX, EventScreenExtents.GetMaxY() + s_boldLineSize, 0.0f);//Arrow past right of event (up and to the right) lower
				points[3].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY() + s_boldLineSize, 0.0f);//bottom left
				grcDrawPolygon(points, 4, NULL, TRUE,biColour);

				//Upward line
				points[0].Set(blendInEndX - s_boldLineSize/2.0f, EventScreenExtents.GetMaxY(), 0.0f);
				points[1].Set(blendInEndX - s_boldLineSize/2.0f, EventScreenExtents.GetMinY(), 0.0f);
				points[2].Set(blendInEndX + s_boldLineSize/2.0f, EventScreenExtents.GetMinY(), 0.0f);
				points[3].Set(blendInEndX + s_boldLineSize/2.0f, EventScreenExtents.GetMaxY(), 0.0f);
				grcDrawPolygon(points, 4, NULL, TRUE, biColour);
				//Draw an arrow pointing up
				points[0].Set(blendInEndX - 3*arrowSize, EventScreenExtents.GetMaxY(), 0.0f);
				points[1].Set(blendInEndX + 3*arrowSize, EventScreenExtents.GetMaxY(), 0.0f);
				points[2].Set(blendInEndX, EventScreenExtents.GetMaxY() - 3*arrowSize, 0.0f);
				grcDrawPolygon(points, 3, NULL, TRUE, biColour);
			}
			else if(blendInStartX < GroupSceenExtents.GetMaxX() && blendInEndX > GroupSceenExtents.GetMaxX()) // Check whether we are off the right hand side of the screen
			{
				//Line equation: y = mx + c
				//computing m
				float xdiff = blendInStartX - blendInEndX;
				float ydiff = GroupSceenExtents.GetMaxY() - GroupSceenExtents.GetMinY();
				float m = ydiff / xdiff;
				//c = y - mx
				float c = GroupSceenExtents.GetMaxY() - m*blendInStartX;
				//y value at point
				float blendInEndY = m*GroupSceenExtents.GetMaxX() + c;

				//Draw blend in partial triangle
				points[0].Set(blendInStartX, GroupSceenExtents.GetMaxY(), 0.0f);//bottom left
				points[1].Set(GroupSceenExtents.GetMaxX(),GroupSceenExtents.GetMaxY(), 0.0f);//bottom right
				points[2].Set(GroupSceenExtents.GetMaxX(), blendInEndY, 0.0f);//point of intersection with end of screen
				grcDrawPolygon(points, 3, NULL, true,biColour);
				if(!CAnimSceneEditor::ms_editorSettingUseEventColourForMarker)
				{
					biColour = animSceneEditor->GetColor(CAnimSceneEditor::kBlendInMarkerColor);
				}
				else
				{
					biColour = eventColor;
				}
				biColour.SetAlpha(255);
				//Draw coloured blend amount
				grcDrawLine(points[0],points[2],biColour,biColour);				

				//Along the bottom (clipped at end)
				points[0].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY(), 0.0f);
				points[1].Set(GroupSceenExtents.GetMaxX(), EventScreenExtents.GetMaxY(), 0.0f);
				points[2].Set(GroupSceenExtents.GetMaxX(), EventScreenExtents.GetMaxY() + s_boldLineSize, 0.0f);
				points[3].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY() + s_boldLineSize, 0.0f);
				grcDrawPolygon(points, 4, NULL, TRUE,biColour);
				//Triangle (beginning)
				points[0].Set(EventScreenExtents.GetMinX() - 3*arrowSize, EventScreenExtents.GetMaxY(), 0.0f);
				points[1].Set(EventScreenExtents.GetMinX() + 3*arrowSize, EventScreenExtents.GetMaxY(), 0.0f);
				points[2].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY() - 3*arrowSize, 0.0f);
				grcDrawPolygon(points, 3, NULL, TRUE, biColour);
				//Draw a 'background' coloured line over the line that already exists (from even block rendering) to make it dashed.
				points[0].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMinY(),0.f);
				points[1].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY(),0.f);
				DrawDashedLine(points[0],points[1],animSceneEditor->GetColor(CAnimSceneEditor::kEventBackgroundColor),5);
			}
			else // The blend indicator is entirely within the display area
			{
				points[0].Set(blendInEndX, EventScreenExtents.GetMaxY(), 0.0f);//top left
				points[1].Set(blendInEndX, EventScreenExtents.GetMinY(), 0.0f);//bottom left
				points[2].Set(blendInStartX, EventScreenExtents.GetMaxY(), 0.0f);//Arrow INSIDE left of event on top
				grcDrawPolygon(points, 3, NULL, true,biColour);

				if(!CAnimSceneEditor::ms_editorSettingUseEventColourForMarker)
				{
					biColour = animSceneEditor->GetColor(CAnimSceneEditor::kBlendInMarkerColor);
				}
				else
				{
					biColour = eventColor;
				}
				grcDrawLine(points[1],points[2],biColour,biColour);

				//Triangle pointing upwards (beginning)
				points[0].Set(EventScreenExtents.GetMinX() - 3*arrowSize, EventScreenExtents.GetMaxY(), 0.0f);
				points[1].Set(EventScreenExtents.GetMinX() + 3*arrowSize, EventScreenExtents.GetMaxY(), 0.0f);
				points[2].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY() - 3*arrowSize, 0.0f);
				grcDrawPolygon(points, 3, NULL, TRUE, biColour);
				//Draw a 'background' coloured line over the line that already exists (from even block rendering) to make it dashed.
				points[0].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMinY(),0.f);
				points[1].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY(),0.f);
				DrawDashedLine(points[0],points[1],animSceneEditor->GetColor(CAnimSceneEditor::kEventBackgroundColor),5);

				if(!CAnimSceneEditor::ms_editorSettingUseEventColourForMarker)
				{
					biColour = animSceneEditor->GetColor(CAnimSceneEditor::kBlendInMarkerColor);
				}
				biColour.SetAlpha(255);

				//Along the bottom
				points[0].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY(), 0.0f);
				points[1].Set(blendInEndX, EventScreenExtents.GetMaxY(), 0.0f);
				points[2].Set(blendInEndX, EventScreenExtents.GetMaxY() + s_boldLineSize, 0.0f);
				points[3].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY() + s_boldLineSize, 0.0f);
				grcDrawPolygon(points, 4, NULL, TRUE,biColour);
				//Upward line at end
				points[0].Set(blendInEndX - s_boldLineSize/2.0f, EventScreenExtents.GetMaxY(), 0.0f);
				points[1].Set(blendInEndX - s_boldLineSize/2.0f, EventScreenExtents.GetMinY(), 0.0f);
				points[2].Set(blendInEndX + s_boldLineSize/2.0f, EventScreenExtents.GetMinY(), 0.0f);
				points[3].Set(blendInEndX + s_boldLineSize/2.0f, EventScreenExtents.GetMaxY(), 0.0f);
				grcDrawPolygon(points, 4, NULL, TRUE, biColour);
				//Draw an arrow pointing up at end
				points[0].Set(blendInEndX - 3*arrowSize, EventScreenExtents.GetMaxY(), 0.0f);
				points[1].Set(blendInEndX + 3*arrowSize, EventScreenExtents.GetMaxY(), 0.0f);
				points[2].Set(blendInEndX, EventScreenExtents.GetMaxY() - 3*arrowSize, 0.0f);
				grcDrawPolygon(points, 3, NULL, TRUE, biColour);
			}


		}//end has blend in

		//Draw blend out (if in range)
		if(hasBlendOut && !endXPastRight)
		{
			Color32 boColour = animSceneEditor->GetColor(CAnimSceneEditor::kEventBackgroundColor);
			boColour.SetAlpha( (int) (CAnimSceneEditor::ms_editorSettingBackgroundAlpha*255) );

			//Handle being only partially in the scene
			if(blendOutEndX > GroupSceenExtents.GetMaxX())
			{
				//Line equation: y = mx + c
				//computing m
				float xdiff = EventScreenExtents.GetMaxX() - blendOutEndX;
				float ydiff = GroupSceenExtents.GetMaxY() - GroupSceenExtents.GetMinY();
				float m = ydiff / xdiff;
				//c = y - mx
				float c = GroupSceenExtents.GetMaxY() - m*EventScreenExtents.GetMaxX();
				//y value at point
				float blendOutEndY = m*GroupSceenExtents.GetMaxX() + c;

				//Draw blend out partial triangle
				points[0].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMinY(), 0.0f);//top right
				points[1].Set(GroupSceenExtents.GetMaxX(), EventScreenExtents.GetMinY(), 0.0f);//Arrow past right of event
				points[2].Set(GroupSceenExtents.GetMaxX(), blendOutEndY, 0.0f);//Arrow past right of event//Arrow lower past right of event
				points[3].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMaxY(), 0.0f);//bottom right
				grcDrawPolygon(points, 4, NULL, true,boColour);

				if(!CAnimSceneEditor::ms_editorSettingUseEventColourForMarker)
				{
					boColour = animSceneEditor->GetColor(CAnimSceneEditor::kBlendOutMarkerColor);
				}
				else
				{
					boColour = eventColor;
				}
				boColour.SetAlpha(255);
				//Draw the line representing the blend amount
				grcDrawLine(points[2],points[3],boColour,boColour);

				// draw line along the top
				points[0].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMinY(), 0.0f);//top right upper
				points[1].Set(GroupSceenExtents.GetMaxX(), EventScreenExtents.GetMinY(), 0.0f);//Arrow past right of event (up and to the right) upper
				points[2].Set(GroupSceenExtents.GetMaxX(), EventScreenExtents.GetMinY() - s_boldLineSize, 0.0f);//Arrow past right of event (up and to the right) lower
				points[3].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMinY() - s_boldLineSize, 0.0f);//bottom left
				grcDrawPolygon(points, 4, NULL, TRUE,boColour);
			}
			else // The blend indicator is entirely within the display area
			{
				//Draw blend out triangle
				points[0].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMaxY(), 0.0f);//top right
				points[1].Set(blendOutEndX, EventScreenExtents.GetMinY(), 0.0f);//Arrow past right of event
				points[2].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMinY(), 0.0f);//bottom right
				grcDrawPolygon(points, 3, NULL, true,boColour);

				if(!CAnimSceneEditor::ms_editorSettingUseEventColourForMarker)
				{
					boColour = animSceneEditor->GetColor(CAnimSceneEditor::kBlendOutMarkerColor);
				}
				else
				{
					boColour = eventColor;
				}
				boColour.SetAlpha(255);
				//Draw the line representing the blend amount
				grcDrawLine(points[0],points[1],boColour,boColour);

				// draw line along the top
				points[0].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMinY(), 0.0f);//top right upper
				points[1].Set(blendOutEndX, EventScreenExtents.GetMinY(), 0.0f);//Arrow past right of event (up and to the right) upper
				points[2].Set(blendOutEndX, EventScreenExtents.GetMinY() - s_boldLineSize, 0.0f);//Arrow past right of event (up and to the right) lower
				points[3].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMinY() - s_boldLineSize, 0.0f);//bottom left
				grcDrawPolygon(points, 4, NULL, TRUE,boColour);

				//Triangle (end)
				points[0].Set(blendOutEndX - 3*arrowSize, EventScreenExtents.GetMinY(), 0.0f);
				points[1].Set(blendOutEndX + 3*arrowSize, EventScreenExtents.GetMinY(), 0.0f);
				points[2].Set(blendOutEndX, EventScreenExtents.GetMinY() + 3*arrowSize, 0.0f);
				grcDrawPolygon(points, 3, NULL, TRUE,boColour);

				//Draw a dashed line under the block
				points[0].Set(blendOutEndX, EventScreenExtents.GetMinY(),0.f);
				points[1].Set(blendOutEndX, EventScreenExtents.GetMaxY(),0.f);
				DrawDashedLine(points[0],points[1],boColour,5);
			}

			//Upward line
			points[0].Set(EventScreenExtents.GetMaxX() + s_boldLineSize/2.0f, EventScreenExtents.GetMaxY(), 0.0f);
			points[1].Set(EventScreenExtents.GetMaxX() + s_boldLineSize/2.0f, EventScreenExtents.GetMinY(), 0.0f);
			points[2].Set(EventScreenExtents.GetMaxX() - s_boldLineSize/2.0f, EventScreenExtents.GetMinY(), 0.0f);
			points[3].Set(EventScreenExtents.GetMaxX() - s_boldLineSize/2.0f, EventScreenExtents.GetMaxY(), 0.0f);
			grcDrawPolygon(points, 4, NULL, TRUE,boColour);

			//draw an arrow pointing down
			points[0].Set(EventScreenExtents.GetMaxX() - 3*arrowSize, EventScreenExtents.GetMinY(), 0.0f);
			points[1].Set(EventScreenExtents.GetMaxX() + 3*arrowSize, EventScreenExtents.GetMinY(), 0.0f);
			points[2].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMinY() + 3*arrowSize, 0.0f);
			grcDrawPolygon(points, 3, NULL, TRUE,boColour);
		}//end has blend out
	}
}


void CEventPanel::Render(CAnimSceneEditor* animSceneEditor, CScreenExtents& GroupSceenExtents, CEventPanelGroup* PanelGroup, int colourId)
{
	if(m_event.event)
	{
		RenderEvent(animSceneEditor, m_event, GroupSceenExtents, PanelGroup, colourId); 
	}
}

void CEventPanel::RenderEvent(CAnimSceneEditor* animSceneEditor, EventInfo& info, CScreenExtents& GroupSceenExtents, CEventPanelGroup* PanelGroup, int colourId)
{
	TUNE_FLOAT(ScreenPos, 0.0f, 0.0f, 10000.0f, 1.0f)
	TUNE_FLOAT(eventHeightOffset, 12.0f, 0.0f, 10000.0f, 1.0f)
	
	float eventHeight = GetTextHeight() + eventHeightOffset;
	CAnimSceneEvent* evt = info.event;

	if (evt)
	{
		//////////////////////////////////////////////////////////////////////////
		// Get screen space information for the event
		//////////////////////////////////////////////////////////////////////////
		float startX = animSceneEditor->GetTimeLine().GetScreenX(evt->GetStart()) ;
		float endX = animSceneEditor->GetTimeLine().GetScreenX(evt->GetEnd()) ;
		//float currentTimeX = animSceneEditor->GetTimeLine().GetScreenX(animSceneEditor->GetScene()->GetTime()); 
		float startFullyInX = animSceneEditor->GetTimeLine().GetScreenX(evt->GetStart());
		float endFullyOutX = endX;

		m_eventHeight = eventHeight; 	

		//Check the start time if there is a blend
		if(evt->GetType() == AnimSceneEventType::PlayAnimEvent)
		{
			CAnimScenePlayAnimEvent* evtPA = static_cast<CAnimScenePlayAnimEvent*>(evt);
			startFullyInX = animSceneEditor->GetTimeLine().GetScreenX(evtPA->GetStartFullBlendInTime());
			endFullyOutX = animSceneEditor->GetTimeLine().GetScreenX(evtPA->GetPoseBlendOutDuration() + evtPA->GetEnd());
		}
		//Camera blending support
		if(evt->GetType() == AnimSceneEventType::PlayCameraAnimEvent)
		{
			CAnimScenePlayCameraAnimEvent* evtPA = static_cast<CAnimScenePlayCameraAnimEvent*>(evt);
			startFullyInX = animSceneEditor->GetTimeLine().GetScreenX(evtPA->GetBlendInDuration() + evtPA->GetStart());
			//Act like play anim events
			endFullyOutX = animSceneEditor->GetTimeLine().GetScreenX(evtPA->GetEnd() + evtPA->GetBlendOutDuration());
			endX = animSceneEditor->GetTimeLine().GetScreenX(evtPA->GetEnd());
		}
		Color32 eventColor;
		evt->GetEditorColor(eventColor, *animSceneEditor, colourId); 
		eventColor.SetAlpha(255);
		
		// bail for events offscreen
		if (endFullyOutX < GroupSceenExtents.GetMinX())
		{		
			return;
		}

		if (startX > GroupSceenExtents.GetMaxX())
		{
			return;
		}

		bool drawStartArrow = startX<GroupSceenExtents.GetMinX();
		bool drawEndArrow = endFullyOutX>GroupSceenExtents.GetMaxX();
		bool endXPastRight = false;	bool endXPastLeft = false;
		bool startFullyInXPastRight = false;

		if(endX > GroupSceenExtents.GetMaxX())
		{
			endXPastRight = true;
		}
		if(endX < GroupSceenExtents.GetMinX())
		{
			endXPastLeft = true;
		}

		if(startFullyInX > GroupSceenExtents.GetMaxX())
		{
			startFullyInXPastRight = true;
		}

		startX = Clamp(startFullyInX, GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMaxX());
		endX = Clamp(endX, GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMaxX());

		CScreenExtents EventScreenExtents(startX, GroupSceenExtents.GetMinY(), endX, GroupSceenExtents.GetMaxY()); 
		
		static float s_minTagWidth = 2.0f;
		bool isDurationless = false;

		// make sure there's always at least a thin line
		if (endX - startX<s_minTagWidth)
		{
			isDurationless = true;
			endX = startX + s_minTagWidth;
		}

		static float s_markerBoxSize = 3.0f;
		static float s_preloadBoxSize = 3.0f;
		static float s_preloadYOffset = 1.0f;

		float preloadLength = Min(evt->GetPreloadDuration(), startX-GroupSceenExtents.GetMinX());
		float preloadX = Max(animSceneEditor->GetTimeLine().GetScreenX(evt->GetPreloadStart()), GroupSceenExtents.GetMinX());
		float preloadY = GroupSceenExtents.GetMaxY()+s_preloadYOffset;

		//Compute blendin/out info
		bool hasBlendIn = false; float blendInDuration = 0.f;  float blendInStartX = 0.f;
		bool hasBlendOut = false;float blendOutDuration = 0.f; float blendOutEndX = 0.f;
		if(evt->GetType() == AnimSceneEventType::PlayAnimEvent)
		{
			CAnimScenePlayAnimEvent* evtPA = static_cast<CAnimScenePlayAnimEvent*>(evt);
			blendInDuration = evtPA->GetPoseBlendInDuration();
			blendOutDuration = evtPA->GetPoseBlendOutDuration();

			if(blendInDuration > 0.f)
			{
				hasBlendIn = true;
				if(startFullyInX < EventScreenExtents.GetMinX())
				{
					hasBlendIn = false;
				}
				blendInStartX = animSceneEditor->GetTimeLine().GetScreenX(evtPA->GetStart());
			}
			if(blendOutDuration > 0.f)
			{
				hasBlendOut = true;
				blendOutEndX = animSceneEditor->GetTimeLine().GetScreenX(blendOutDuration + evtPA->GetEnd());
				if(blendOutEndX < endX)
				{
					hasBlendOut = false;
				}
			}			
		}//end 	if(evt->GetType() == AnimSceneEventType::PlayAnimEvent)
		//Camera blending support
		if(evt->GetType() == AnimSceneEventType::PlayCameraAnimEvent)
		{
			CAnimScenePlayCameraAnimEvent* evtPA = static_cast<CAnimScenePlayCameraAnimEvent*>(evt);
			blendInDuration = evtPA->GetBlendInDuration();
			blendOutDuration = evtPA->GetBlendOutDuration();
			if(blendInDuration > 0.f)
			{
				hasBlendIn = true;
				if(startFullyInX < EventScreenExtents.GetMinX())
				{
					hasBlendIn = false;
				}
			}
			if(blendOutDuration > 0.f)
			{
				hasBlendOut = true;
				//act like playanim event
				blendOutEndX = animSceneEditor->GetTimeLine().GetScreenX(blendOutDuration + evtPA->GetEnd());
				if(blendOutEndX < endX)
				{
					hasBlendOut = false;
				}
			}			
		}

		//////////////////////////////////////////////////////////////////////////
		// Update
		//////////////////////////////////////////////////////////////////////////
		float mouseX = static_cast<float>(ioMouse::GetX());
		float mouseY = static_cast<float>(ioMouse::GetY());
		bool hoveringEvent = EventScreenExtents.Contains(mouseX, mouseY);
		bool hoveringMarker = isDurationless && (Abs(mouseX - startX)<s_markerBoxSize) && (Abs(mouseY - GroupSceenExtents.GetMinY())<s_markerBoxSize);

		if (!animSceneEditor->IsDragging())
		{			
			bool leftMouseDown = ioMouse::GetButtons() & ioMouse::MOUSE_LEFT;
			bool leftMousePressed = ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT;

			if (leftMouseDown)
			{
				static float s_edgeSelectTolerance = 3.0f;

				bool hoveringEventStart = hoveringEvent && (Abs(mouseX - startX)<s_edgeSelectTolerance);
				bool hoveringEventEnd = hoveringEvent && (Abs(mouseX - endX)<s_edgeSelectTolerance);
				bool hoveringPreload = animSceneEditor->GetEditEventPreloads() && (Abs(mouseX - preloadX)<s_preloadBoxSize) && (Abs(mouseY - preloadY)<s_preloadBoxSize);
				
				if (hoveringMarker)
				{
					//select the event and activate dragging
					if (leftMousePressed)
					{
						animSceneEditor->SelectEvent(evt);
					}
					if (animSceneEditor->CanDragEvent(evt))
					{
						animSceneEditor->SetFlag(CAnimSceneEditor::kDraggingEvent); 
					}					
				}
				else if (hoveringPreload)
				{
					//select the event and activate dragging
					if (leftMousePressed)
					{
						animSceneEditor->SelectEvent(evt);
					}
					if (animSceneEditor->CanDragEvent(evt))
					{
						animSceneEditor->SetFlag(CAnimSceneEditor::kDraggingEventPreLoad); 
					}
				}
				else if (hoveringEventEnd)
				{
					//select the event and activate dragging
					if (leftMousePressed)
					{
						animSceneEditor->SelectEvent(evt);
					}
					if (animSceneEditor->CanDragEvent(evt))
					{
						animSceneEditor->SetFlag(CAnimSceneEditor::kDraggingEventEnd); 
					}
				}
				else if (hoveringEventStart)
				{
					//select the event and activate dragging
					if (leftMousePressed)
					{
						animSceneEditor->SelectEvent(evt);
					}
					if (animSceneEditor->CanDragEvent(evt))
					{
						animSceneEditor->SetFlag(CAnimSceneEditor::kDraggingEventStart); 
					}
				}
				else if (hoveringEvent)
				{
					//select the event and activate dragging
					if (leftMousePressed)
					{
						animSceneEditor->SelectEvent(evt);
					}
					if (animSceneEditor->CanDragEvent(evt))
					{
						animSceneEditor->SetFlag(CAnimSceneEditor::kDraggingEvent); 
					}
				}
			}
			else // if (leftMouseDown)
			{
			
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Render
		//////////////////////////////////////////////////////////////////////////

		bool isSelected = (evt==animSceneEditor->GetSelectedEvent());
		bool hasFocus = isSelected && animSceneEditor->GetFocus()==CAnimSceneEditor::kFocusEvent;
		bool isBeingDragged = isSelected && animSceneEditor->IsDraggingEvent();

		Vector3 points[4];
		points[0].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY(), 0.0f);
		points[1].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMaxY(), 0.0f);
		points[2].Set(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMinY(), 0.0f);
		points[3].Set(EventScreenExtents.GetMinX(), EventScreenExtents.GetMinY(), 0.0f);

		eventColor.SetAlpha(255); //No visual alpha blending on events

		bool drawEventBackground = true;
		if(drawEventBackground)
		{
			Color32 backColor(eventColor);
			backColor = animSceneEditor->GetColor(CAnimSceneEditor::kEventBackgroundColor);
			backColor.SetAlpha((int) (CAnimSceneEditor::ms_editorSettingBackgroundAlpha*255) );
			grcDrawPolygon(points, 4, NULL, true,backColor);
		}
		
		if (hoveringEvent && !isSelected)
		{
			eventColor.SetRed(Clamp(eventColor.GetRed() + 25, 0, 255)); 
			eventColor.SetBlue(Clamp(eventColor.GetBlue() + 25, 0, 255));
			eventColor.SetGreen(Clamp(eventColor.GetGreen() + 25, 0, 255));
			grcDrawPolygon(points, 4, NULL, false,eventColor);
		}
		else
		{
			grcDrawPolygon(points, 4, NULL, false,eventColor);
		}

		// render the event outline
		grcLighting(false);

		CAnimSceneEditor::eColorType markerColorType = isSelected ? CAnimSceneEditor::kSelectedEventMarkerColor : CAnimSceneEditor::kEventMarkerColor;
		CAnimSceneEditor::eColorType outlineColorType = isSelected ? isBeingDragged ? CAnimSceneEditor::kSelectedEventDraggingColor : CAnimSceneEditor::kSelectedEventOutlineColor : CAnimSceneEditor::kEventOutlineColor;
		CAnimSceneEditor::eColorType eventColorType = isSelected ? isBeingDragged ? CAnimSceneEditor::kSelectedEventDraggingColor : CAnimSceneEditor::kSelectedEventColor : CAnimSceneEditor::kDefaultEventColor;
		
		if(isSelected)
		{
			Color32 outlineColour = animSceneEditor->GetColor(outlineColorType);
			outlineColour.SetAlpha(255); 
			grcColor(outlineColour);
			grcBegin(drawLineStrip, 5);
			grcVertex2f(EventScreenExtents.GetMinX(), EventScreenExtents.GetMinY());
			grcVertex2f(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMinY());
			grcVertex2f(EventScreenExtents.GetMaxX(), EventScreenExtents.GetMaxY());
			grcVertex2f(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY());
			grcVertex2f(EventScreenExtents.GetMinX(), EventScreenExtents.GetMinY());
			grcEnd();
		}

		if (isDurationless && !endXPastLeft && !startFullyInXPastRight)
		{
			// need to draw a marker to support moving durationless tags around
			Vector3 points[4];
			points[0].Set(EventScreenExtents.GetMinX() - s_markerBoxSize, EventScreenExtents.GetMinY() - s_markerBoxSize, 0.0f);
			points[1].Set(EventScreenExtents.GetMinX() + s_markerBoxSize, EventScreenExtents.GetMinY() - s_markerBoxSize, 0.0f);
			points[2].Set(EventScreenExtents.GetMinX() + s_markerBoxSize, EventScreenExtents.GetMinY() + s_markerBoxSize, 0.0f);
			points[3].Set(EventScreenExtents.GetMinX() - s_markerBoxSize, EventScreenExtents.GetMinY() + s_markerBoxSize, 0.0f);
			grcDrawPolygon(points, 4, NULL, true, animSceneEditor->GetColor(markerColorType));

			Vector3 Outline[4];
			Outline[0].Set(EventScreenExtents.GetMinX() - s_markerBoxSize, EventScreenExtents.GetMinY() - s_markerBoxSize, 0.0f);
			Outline[1].Set(EventScreenExtents.GetMinX() + s_markerBoxSize, EventScreenExtents.GetMinY() - s_markerBoxSize, 0.0f);
			Outline[2].Set(EventScreenExtents.GetMinX() + s_markerBoxSize, EventScreenExtents.GetMinY() + s_markerBoxSize, 0.0f);
			Outline[3].Set(EventScreenExtents.GetMinX() - s_markerBoxSize, EventScreenExtents.GetMinY() + s_markerBoxSize, 0.0f);
			grcDrawPolygon(Outline, 4, NULL, false, animSceneEditor->GetColor(outlineColorType));
		}

		if (drawStartArrow)
		{
			Vector3 points[3];
			points[0].Set(GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMinY(), 0.0f);
			points[1].Set(GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMaxY(), 0.0f);
			points[2].Set(GroupSceenExtents.GetMinX() - m_offscreenArrowMarker, GroupSceenExtents.GetMinY() + ((GroupSceenExtents.GetMaxY()-GroupSceenExtents.GetMinY())*0.5f), 0.0f);
			grcDrawPolygon(points, 3, NULL, true, animSceneEditor->GetColor(eventColorType));

			Vector3 arrowOutline[3];
			arrowOutline[0].Set(GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMinY(), 0.0f);
			arrowOutline[1].Set(GroupSceenExtents.GetMinX(), GroupSceenExtents.GetMaxY(), 0.0f);
			arrowOutline[2].Set(GroupSceenExtents.GetMinX() - m_offscreenArrowMarker, GroupSceenExtents.GetMinY() + ((GroupSceenExtents.GetMaxY()-GroupSceenExtents.GetMinY())*0.5f), 0.0f);
			grcDrawPolygon(arrowOutline, 3, NULL, false, animSceneEditor->GetColor(outlineColorType));
		}
		if (drawEndArrow)
		{
			Vector3 points[3];
			points[0].Set(GroupSceenExtents.GetMaxX(), GroupSceenExtents.GetMinY(), 0.0f);
			points[2].Set(GroupSceenExtents.GetMaxX() + m_offscreenArrowMarker, GroupSceenExtents.GetMinY() + ((GroupSceenExtents.GetMaxY()-GroupSceenExtents.GetMinY())*0.5f), 0.0f);
			points[1].Set(GroupSceenExtents.GetMaxX(), EventScreenExtents.GetMaxY(), 0.0f);
			grcDrawPolygon(points, 3, NULL, true, animSceneEditor->GetColor(eventColorType));

			Vector3 arrowOutline[3];
			arrowOutline[0].Set(GroupSceenExtents.GetMaxX(), GroupSceenExtents.GetMinY(), 0.0f);
			arrowOutline[2].Set(GroupSceenExtents.GetMaxX() + m_offscreenArrowMarker, GroupSceenExtents.GetMinY() + ((GroupSceenExtents.GetMaxY()-GroupSceenExtents.GetMinY())*0.5f), 0.0f);
			arrowOutline[1].Set(GroupSceenExtents.GetMaxX(), GroupSceenExtents.GetMaxY(), 0.0f);
			grcDrawPolygon(arrowOutline, 3, NULL, false, animSceneEditor->GetColor(outlineColorType));
		}
		
		if (preloadLength>0.0f && animSceneEditor->GetEditEventPreloads())
		{
			Vector3 points[4];
			points[0].Set(preloadX-s_preloadBoxSize, preloadY-s_preloadBoxSize, 0.0f);
			points[1].Set(preloadX+s_preloadBoxSize, preloadY-s_preloadBoxSize, 0.0f);
			points[2].Set(preloadX+s_preloadBoxSize, preloadY+s_preloadBoxSize, 0.0f);
			points[3].Set(preloadX-s_preloadBoxSize, preloadY+s_preloadBoxSize, 0.0f);
			grcDrawPolygon(points, 4, NULL, true, animSceneEditor->GetColor(markerColorType));

			grcColor(animSceneEditor->GetColor(CAnimSceneEditor::kEventPreloadLineColor));
			grcBegin(drawLineStrip, 3);
			grcVertex2f(EventScreenExtents.GetMinX(), EventScreenExtents.GetMaxY());
			grcVertex2f(EventScreenExtents.GetMinX(), preloadY);
			grcVertex2f(preloadX, preloadY);
			grcEnd();
		}

		TUNE_FLOAT(TextPadding, 1.0f, -1.0f, 100.0f, 1.0f)
		TUNE_FLOAT(TextScale, 1.0f, 0.0f, 100.0f, 1.0f)
		TUNE_FLOAT(TextYOffset, 1.0f, 0.0f, 100.0f, 1.0f)
		TUNE_FLOAT(TextXOffset, 1.0f, 0.0f, 100.0f, 1.0f)
		TUNE_BOOL(renderOutline, false)

		// render the name of the event in the centre of the event bar.
		atString evtText;
		if(evt->GetEditorEventText(evtText, *animSceneEditor))
		{
			Color32 textcolor = isSelected ? animSceneEditor->GetColor(isBeingDragged ? CAnimSceneEditor::kSelectedEventDraggingColor : CAnimSceneEditor::kSelectedEventOutlineColor) : eventColor;
			const float fTextWidth = GetTextWidth(evtText.c_str());
			if(fTextWidth < EventScreenExtents.GetWidth())
			{
				float RenderTextPosx = EventScreenExtents.GetMinX() + EventScreenExtents.GetWidth() / 2.0f - fTextWidth/2.0f; 

				RenderTextWithBackground(evtText.c_str(), RenderTextPosx, EventScreenExtents.GetMinY() + TextYOffset, textcolor, 
					animSceneEditor->GetColor(CAnimSceneEditor::kTextBackgroundColor), -1.0f, renderOutline, animSceneEditor->GetColor(hasFocus? CAnimSceneEditor::kFocusTextOutlineColor : isSelected ? CAnimSceneEditor::kSelectedTextOutlineColor : CAnimSceneEditor::kTextOutlineColor));
			}
		}

		if (hoveringEvent || hoveringMarker)
		{
			atString hoverText;
			if(evt->GetEditorHoverText(hoverText, *animSceneEditor))
			{
				PanelGroup->AddEventInfo(hoverText.c_str());
			}
		}
	}
}

void CEventPanel::DrawDashedLine(const Vector3& v1, const Vector3& v2, const Color32& colour, int numDashes)
{
	//starting at v1 in the direction of v2 - v1
	Vector3 dirInc = (v2 - v1) / (2.0f * numDashes);
	Vector3 line;
	for(int i = 0; i < numDashes; ++i)
	{
		line = v1 + (dirInc* (float)i *2.0f);
		grcDrawLine(line,line+dirInc, colour);
	}
}

void CEventPanel::DrawTransitionBlock(const Vector3 *points, const Color32& leftColour, const Color32& rightColour)
{
	//void grcDrawPolygon(const Vector3 *points, int count, const Vector3 *pNormal, bool solid, Color32 *color)
	Color32 colourList[4];
	colourList[0] = Color32(leftColour);
	colourList[1] = Color32(leftColour);
	colourList[2] = Color32(rightColour);
	colourList[3] = Color32(rightColour);
	grcDrawPolygon(points,4,NULL,true,colourList);
}



CEventPanelGroup::CEventPanelGroup(atHashString name)
:m_name(name)
,m_GroupHeight(10.0f)
,m_GroupSpacing(3.0f)
{

}



CEventPanelGroup::~CEventPanelGroup()
{
	RemoveAllEvents();
}

void CEventPanelGroup::AddEvent(EventInfo Info)
{
	for(int i=0; i < m_events.GetCount(); i++) 
	{
		if(m_events[i])
		{
			if(Info.event == m_events[i]->GetEventInfo().event)
			{
				return; 
			}
		}
	}

	m_events.Grow() = rage_new CEventPanel(Info);
}

void CEventPanelGroup::RemoveAllEvents()
{
	for(int i=0; i < m_events.GetCount(); i++) 
	{
		if(m_events[i])
		{
			delete m_events[i];
			m_events[i] = NULL; 
		}
	}

	m_events.Reset(); 
}


void CEventPanelGroup::RemoveEvent(EventInfo Info)
{
	for(int i=0; i < m_events.GetCount(); i++) 
	{
		if(m_events[i])
		{
			if(Info.event == m_events[i]->GetEventInfo().event)
			{
				delete m_events[i];
				m_events[i] = NULL; 

				m_events.Delete(i); 
			}
		}

	}
}

void CEventPanelGroup::SetScreenExtents(const CScreenExtents& screenExtents)
{
	m_screenExtents = screenExtents;
}

int CEventPanelGroup::CompareEvents( CEventPanel* const* pEvent1,  CEventPanel* const* pEvent2)
{
	Assert(pEvent1);
	Assert(pEvent2);

	if (pEvent1 && pEvent2)
	{
		if((*pEvent1)->GetEventInfo().event->GetStart() < (*pEvent2)->GetEventInfo().event->GetStart())
		{
			return -1;
		}
		else
		{
			return 1; 
		}
	}

	return 0;
}



void CEventPanelGroup::Render(CAnimSceneEditor* animSceneEditor)
{
	TUNE_FLOAT(GroupHeightOffset, 3.0f, 0.0f, 1000.0f, 1.0f)
	TUNE_FLOAT(GroupSpacing, 6.0f, 0.0f, 1000.0f, 1.0f)
	TUNE_FLOAT(GapMarkerOffset, 34.0f, 0.0f, 100.0f, 0.5f)

	m_HoveringInfo.Reset(); 

	m_GroupHeight = GetTextHeight() + GroupHeightOffset; 
	m_GroupSpacing = GroupSpacing; 

	m_events.QSort(0, -1, &CompareEvents); 

	//render the track's extents
	/*
	grcColor(animSceneEditor->GetColor(CAnimSceneEditor::kEventCurrentTimeLineColor));
	grcBegin(drawLineStrip, 5);
	grcVertex2f(m_screenExtents.GetMinX() , m_screenExtents.GetMinY() );
	grcVertex2f(m_screenExtents.GetMaxX() , m_screenExtents.GetMinY() );

	grcVertex2f(m_screenExtents.GetMaxX() , m_screenExtents.GetMaxY() );
	grcVertex2f(m_screenExtents.GetMinX() , m_screenExtents.GetMaxY() );

	grcVertex2f(m_screenExtents.GetMinX() , m_screenExtents.GetMinY() );
	grcEnd();*/

	//Render section starts as black lines behind events
	if(animSceneEditor->GetScene())
	{

		//Draw double section time marker at beginning of anim scene timeline
		float animSceneStartX = animSceneEditor->GetTimeLine().GetScreenX(0.f);
		if( animSceneStartX <= m_screenExtents.GetMaxX()  && animSceneStartX >= m_screenExtents.GetMinX() )
		{
			grcColor(animSceneEditor->GetColor(CAnimSceneEditor::kSectionDividerColor));
			grcBegin(drawLineStrip, 2);
			grcVertex2f(animSceneStartX,
				m_screenExtents.GetMinY() - GetGroupSpacing() );
			grcVertex2f(animSceneStartX, 
				m_screenExtents.GetMaxY() + GetGroupSpacing() );
			grcEnd();
			grcBegin(drawLineStrip, 2);
			grcVertex2f(animSceneStartX -2.f,
				m_screenExtents.GetMinY() - GetGroupSpacing() );
			grcVertex2f(animSceneStartX -2.f, 
				m_screenExtents.GetMaxY() + GetGroupSpacing() );
			grcEnd();
		}

		CAnimScenePlaybackList::Id currentPBL = animSceneEditor->GetScene()->GetPlayBackList();
		if(currentPBL == ANIM_SCENE_SECTION_DEFAULT || currentPBL == ANIM_SCENE_SECTION_ID_INVALID)
		{
			//render for all sections if we are the default play back list
			CAnimSceneSectionIterator sectionIt(*animSceneEditor->GetScene(), true);
			while (*sectionIt)
			{
				CAnimSceneSection::Id secId = sectionIt.GetId();
				CAnimSceneSection* section = animSceneEditor->GetScene()->GetSection(secId);
				animAssertf(section,"Section was invalid for id %s",secId.TryGetCStr());
				float secStartTime = animSceneEditor->GetScene()->CalcSectionStartTime(section,currentPBL);
				float secStartTimeXLoc = animSceneEditor->GetTimeLine().GetScreenX(secStartTime);

				if( secStartTimeXLoc <= m_screenExtents.GetMaxX() && secStartTimeXLoc >= m_screenExtents.GetMinX() )
				{
					grcColor(animSceneEditor->GetColor(CAnimSceneEditor::kSectionDividerColor));
					grcBegin(drawLineStrip, 2);
					grcVertex2f(secStartTimeXLoc, m_screenExtents.GetMinY() - GetGroupSpacing() );
					grcVertex2f(secStartTimeXLoc, m_screenExtents.GetMaxY() + GetGroupSpacing() );
					grcEnd();
				}
				++sectionIt;
			}	
		}
		else if(animSceneEditor->GetScene()->GetPlayBackList(currentPBL))
		{
			for(int i = 0; i < animSceneEditor->GetScene()->GetPlayBackList(currentPBL)->GetNumSections(); ++i)
			{
				CAnimSceneSection::Id secId = animSceneEditor->GetScene()->GetPlayBackList(currentPBL)->GetSectionId(i);
				CAnimSceneSection* section = animSceneEditor->GetScene()->GetSection(secId);
				animAssertf(section,"Section was invalid for id %s",secId.TryGetCStr());
				float secStartTime = animSceneEditor->GetScene()->CalcSectionStartTime(section,currentPBL);
				float secStartTimeXLoc = animSceneEditor->GetTimeLine().GetScreenX(secStartTime);

				if( secStartTimeXLoc <= m_screenExtents.GetMaxX() && secStartTimeXLoc >= m_screenExtents.GetMinX() )
				{
					grcColor(animSceneEditor->GetColor(CAnimSceneEditor::kSectionDividerColor));
					grcBegin(drawLineStrip, 2);
					grcVertex2f(secStartTimeXLoc, m_screenExtents.GetMinY() - GetGroupSpacing() );
					grcVertex2f(secStartTimeXLoc, m_screenExtents.GetMaxY() + GetGroupSpacing() );
					grcEnd();
				}
			}
		}		

		//Draw double section time marker at end of anim scene timeline
		float animSceneEndX = animSceneEditor->GetTimeLine().GetScreenX(animSceneEditor->GetScene()->GetDuration());
		if( animSceneEndX <= m_screenExtents.GetMaxX()  && animSceneEndX >= m_screenExtents.GetMinX() )
		{
			grcColor(animSceneEditor->GetColor(CAnimSceneEditor::kSectionDividerColor));
			grcBegin(drawLineStrip, 2);
			grcVertex2f(animSceneEndX,
				m_screenExtents.GetMinY() - GetGroupSpacing() );
			grcVertex2f(animSceneEndX, 
				m_screenExtents.GetMaxY() + GetGroupSpacing() );
			grcEnd();
			grcBegin(drawLineStrip, 2);
			grcVertex2f(animSceneEndX +2.f,
				m_screenExtents.GetMinY() - GetGroupSpacing() );
			grcVertex2f(animSceneEndX +2.f, 
				m_screenExtents.GetMaxY() + GetGroupSpacing() );
			grcEnd();
		}
	}

	//Render the actual event boxes
	for(int i=0; i < m_events.GetCount(); i++) 
	{
		m_events[i]->Render(animSceneEditor, m_screenExtents, this, i); 
	}

	//Might be a good idea to have a seperate 'render event controls' to separate rendering the events and the overlayed blending markers
	//This renders both
	for(int i=0; i < m_events.GetCount(); i++) 
	{
		m_events[i]->RenderToolsOverlay(animSceneEditor, m_screenExtents, this, i); 
	}

	// Render current time marker
	float currentTimeX = animSceneEditor->GetTimeLine().GetScreenX(animSceneEditor->GetScene()->GetTime()); 
	if (currentTimeX >= m_screenExtents.GetMinX() && currentTimeX <= m_screenExtents.GetMaxX())
	{
		// White line at time
		grcColor(animSceneEditor->GetColor(CAnimSceneEditor::kEventCurrentTimeLineColor));
		grcBegin(drawLineStrip, 2);
		grcVertex2f(currentTimeX, m_screenExtents.GetMinY() - GetGroupSpacing() );
		grcVertex2f(currentTimeX, m_screenExtents.GetMaxY() + GetGroupSpacing() );
		grcEnd();
	}

	RenderEventInfoOverlay(animSceneEditor); 
	
	//Check the first event to see if it's a play anim event, and if so, check and notify if there are gaps
	if(m_events.size() > 0  
		&& m_events[0]
		&& m_events[0]->GetEventInfo().event
		&& m_events[0]->GetEventInfo().event->GetType() == AnimSceneEventType::PlayAnimEvent  
		&& DoesGroupHaveGaps())
	{
		RenderTextWithBackground("(!)", m_screenExtents.GetMinX() - GapMarkerOffset, m_screenExtents.GetMinY(), animSceneEditor->GetColor(CAnimSceneEditor::kTextWarningColor), 
			animSceneEditor->GetColor(CAnimSceneEditor::kTextBackgroundColor), 1.1f, true, animSceneEditor->GetColor(CAnimSceneEditor::kTextOutlineColor));
	}
}

void CEventPanelGroup::AddEventInfo(const char* EventInfo)
{
	atString& mString = m_HoveringInfo.Grow(); 

	mString += EventInfo; 
}

void CEventPanelGroup::RenderEventInfoOverlay(CAnimSceneEditor* animSceneEditor)
{
	float mouseX = static_cast<float>(ioMouse::GetX());
	float mouseY = static_cast<float>(ioMouse::GetY());
	TUNE_FLOAT(mouseyOffset, -22.0f, -100.0f, 10000.0f, 1.0f)
	TUNE_FLOAT(mouseXOffset, 0.0f, 0.0f, 10000.0f, 1.0f)
	TUNE_FLOAT(textSize, 10.0f, 0.0f, 10000.0f, 1.0f)
	for(int i = 0; i < m_HoveringInfo.GetCount() ; i++)
	{
		RenderTextWithBackground(m_HoveringInfo[i], (mouseX + mouseXOffset), mouseY + mouseyOffset + (i * textSize) , animSceneEditor->GetColor(CAnimSceneEditor::kMainTextColor), 
			animSceneEditor->GetColor(CAnimSceneEditor::kTextBackgroundColor), 1.0f, true, animSceneEditor->GetColor(CAnimSceneEditor::kTextOutlineColor));
	}
	
}

//This function does not assume m_events is sorted
bool CEventPanelGroup::DoesGroupHaveGaps()
{
	//list of X locations or times
	atArray<float> locStarts;
	atArray<float> locEnds;

	//the blend in and out times
	atArray<float> blendStarts;
	atArray<float> blendEnds;

	locStarts.Reserve(m_events.size());
	locEnds.Reserve(m_events.size());
	blendStarts.Reserve(m_events.size());
	blendEnds.Reserve(m_events.size());

	for(int i = 0; i < m_events.size(); ++i)
	{
		CAnimSceneEvent* event = m_events[i]->GetEventInfo().event;	
		if(event->GetType() == AnimSceneEventType::PlayAnimEvent)
		{
			CAnimScenePlayAnimEvent* evtPA = static_cast<CAnimScenePlayAnimEvent*>(event);
			locStarts.PushAndGrow(evtPA->GetStartFullBlendInTime());
			blendStarts.PushAndGrow(evtPA->GetStart());
			locEnds.PushAndGrow(evtPA->GetEnd());
			blendEnds.PushAndGrow(evtPA->GetEnd() + evtPA->GetPoseBlendOutDuration());
		}
		else
		{
			locStarts.PushAndGrow(event->GetStart());
			blendStarts.PushAndGrow(event->GetStart());
			locEnds.PushAndGrow(event->GetEnd());
			blendEnds.PushAndGrow(event->GetEnd());
		}	
		
	}

	int numMismatchedStarts = 0;
	for(int i = 0; i < locStarts.size(); ++i)
	{
		//find an end for each start (end must be after start, and beginning must be before)
		float toFind = locStarts[i];
		//Custom float find with epsilon
		float eps = 1e-3f;
		bool found = false;
		for(int j = 0; j < locEnds.size(); ++j)
		{
			if(i == j) continue;
			//toFind is in between start and end of other clip (or on it)
			if(toFind > locStarts[j] - eps && toFind < locEnds[j] + eps)
			{
				found = true;
				break;
			}
		}

		if(!found)
		{
			//Check whether there is a blend prior to this, and if that starts at
			//the end of a previous clip
			int findId = -1;
			float bStart = blendStarts[i];
			for(int j = 0; j < locEnds.size(); ++j)
			{
				if(i == j) continue;

				//The blend occurs during the main section of event j
				if(bStart > locStarts[j] - eps && bStart < locEnds[j] + eps)
				{
					findId = j;
					//For extra care, check the  end of the previous clip blend finishes at
					//the start of this event being fully blended in (or later)
					if(blendEnds[findId] > locStarts[i] - eps)
					{
						found = true;
						break;
					}					
				}
			}

			if(!found)
			{
				++numMismatchedStarts;
				//should have one initial start which does not match to an end.
				if(numMismatchedStarts > 1)
				{
					return true;
				}
			}
		}
	}

	//There were 1 or 0 mismatched starts
	return false;
}




CEventLayerPanel::CEventLayerPanel(atHashString name)
:m_name(name)
,m_basePanelInfoHeight(15.0f)
,m_basePanelSpacing(15.0f)
{

}

CEventLayerPanel::~CEventLayerPanel()
{
	for(int i = 0; i < m_groups.GetCount(); i++)
	{
		delete m_groups[i]; 
		m_groups[i] = NULL; 
	}

	m_groups.Reset(); 
}

void CEventLayerPanel::AddEventGroup(atHashString name)
{
	m_groups.Grow() = rage_new CEventPanelGroup(name);
}

void CEventLayerPanel::RemoveEventGroup(u32 i)
{
	if(i < m_groups.GetCount())
	{
		delete m_groups[i]; 
		m_groups[i] = NULL; 
		m_groups.Delete(i); 
	}
}

void CEventLayerPanel::RenderPanelInfo(CAnimSceneEditor* animSceneEditor)
{
	if(animSceneEditor)
	{
		float mouseX = static_cast<float>(ioMouse::GetX());
		float mouseY = static_cast<float>(ioMouse::GetY());

		bool isSelected = (GetName().GetHash()==animSceneEditor->GetSelectedEntityId().GetHash());
		bool hasFocus = isSelected && animSceneEditor->GetFocus()==CAnimSceneEditor::kFocusEntity;
		bool isHovering = false; 

		if(m_name.GetHash() != 0)
		{
			const float fTextWidth = GetTextWidth(m_name.GetCStr());

			CScreenExtents EventLayerPanelIdScreenExtents(m_screenExtents.GetMinX(), m_screenExtents.GetMinY(), m_screenExtents.GetMinX()+fTextWidth, m_screenExtents.GetMinY()+ m_basePanelInfoHeight); 

			if (EventLayerPanelIdScreenExtents.Contains(mouseX,mouseY))
			{
				if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT && !animSceneEditor->IsDragging())
				{
					if(animSceneEditor->GetScene())
					{
						animSceneEditor->SelectEntity(animSceneEditor->GetScene()->GetEntity(GetName().GetCStr()));
					}
					animSceneEditor->SetFlag(CAnimSceneEditor::kMadeEntitySelectionThisFrame); 
				}
				else
				{
					isHovering = true; 
				}
			}

			if(m_screenExtents.Contains(mouseX, mouseY))
			{
				RenderTextWithBackground(GetName().GetCStr(), m_screenExtents.GetMinX(),  m_screenExtents.GetMinY(), 
					animSceneEditor->GetColor(isHovering ? CAnimSceneEditor::kSelectedTextHoverColor : CAnimSceneEditor::kSelectedTextColor), 
					animSceneEditor->GetColor(isSelected ? CAnimSceneEditor::kSelectedTextBackGroundColor :CAnimSceneEditor::kTextBackgroundColor), 3.0f, true, 
					animSceneEditor->GetColor(hasFocus ? CAnimSceneEditor::kFocusTextOutlineColor : isSelected ? CAnimSceneEditor::kSelectedTextOutlineColor :CAnimSceneEditor::kTextOutlineColor));
			}
			else
			{
				RenderTextWithBackground(GetName().GetCStr(), m_screenExtents.GetMinX(), m_screenExtents.GetMinY(), 
					animSceneEditor->GetColor(isHovering ? CAnimSceneEditor::kTextHoverColor : CAnimSceneEditor::kMainTextColor), 
					animSceneEditor->GetColor(isSelected ? CAnimSceneEditor::kSelectedTextBackGroundColor :CAnimSceneEditor::kTextBackgroundColor), 3.0f, true, 
					animSceneEditor->GetColor(hasFocus ? CAnimSceneEditor::kFocusTextOutlineColor : isSelected ? CAnimSceneEditor::kSelectedTextOutlineColor : CAnimSceneEditor::kTextOutlineColor));
			}

			if (isSelected)
			{
				// draw a colored box to mark the selection
				CScreenExtents outline(m_screenExtents.GetMinX() - 6.0f, m_screenExtents.GetMinY() - 6.0f, animSceneEditor->GetTimeLine().GetScreenExtents().GetMaxX() - 6.0f,  m_screenExtents.GetMaxY() - 6.0f);
				outline.DrawBorder(animSceneEditor->GetColor(hasFocus ? CAnimSceneEditor::kFocusTextOutlineColor : CAnimSceneEditor::kSelectedTextOutlineColor));
			}
		}
	}
}

void CEventLayerPanel::Render(CAnimSceneEditor* animSceneEditor)
{
	TUNE_FLOAT(basePanelSpacing, 6.000f, 0.0f, 100.0f, 1.0f)
	
	m_basePanelInfoHeight= GetTextHeight();
	m_basePanelSpacing = basePanelSpacing; 

	RenderPanelInfo(animSceneEditor);

	CScreenExtents m_EventGroupExtents; 
	for(int i= 0; i < m_groups.GetCount(); i++)
	{
		if(m_groups[i])
		{
			float EventGroupHeightMin =  (i * (float)m_groups[i]->GetGroupHeight()) + (i>0 ? (float)m_groups[i]->GetGroupSpacing() : 0.0f);
			float EventGroupHeightMax = EventGroupHeightMin + (float)(m_groups[i]->GetGroupHeight()); 
			
			m_EventGroupExtents.Set(animSceneEditor->GetTimeLine().GetScreenExtents().GetMinX(), m_screenExtents.GetMinY() + EventGroupHeightMin, 
				animSceneEditor->GetTimeLine().GetScreenExtents().GetMaxX(), m_screenExtents.GetMinY() + EventGroupHeightMax); 

			m_groups[i]->SetScreenExtents(CScreenExtents(m_EventGroupExtents)); 
			m_groups[i]->Render(animSceneEditor); 
		}
	}
}

float CEventLayerPanel::GetTotalPanelHeight()
{
	float panelHeight = 0.0f; 
	for(int i =0; i < m_groups.GetCount(); i++)
	{
		panelHeight += m_groups[i]->GetGroupPanelHeight(); 
	}

	panelHeight+=m_basePanelSpacing; 

	if(panelHeight> m_basePanelInfoHeight + m_basePanelSpacing)
	{
		return panelHeight; 
	}
	else
	{
		return m_basePanelInfoHeight + m_basePanelSpacing;
	}

}

void CEventLayerPanel::SetScreenExtents(const CScreenExtents& screenExtents)
{
	m_screenExtents = screenExtents;
}

CEventLayersPane::CEventLayersPane()
{
	m_eventLayerPanels.Reset(); 
}

CEventLayersPane::~CEventLayersPane()
{
	for(int i = 0; i < m_eventLayerPanels.GetCount(); i++)
	{
		delete m_eventLayerPanels[i]; 
		m_eventLayerPanels[i] = NULL; 
	}

	m_eventLayerPanels.Reset(); 
}


void CEventLayersPane::AddEventLayerPanel(atHashString Name)
{
	m_eventLayerPanels.Grow() = rage_new CEventLayerPanel(Name); 
}

void CEventLayersPane::SetScreenExtents(const CScreenExtents& screenExtents)
{
	m_screenExtents = screenExtents;
}

CEventPanelGroup* CEventLayersPane::GetEventPanelGroup(u32 PanelLayer, u32 Group)
{
	CEventPanelGroup* pGroupPanel = NULL; 

	if(PanelLayer < (u32)m_eventLayerPanels.GetCount())
	{
		if(m_eventLayerPanels[PanelLayer] && Group < m_eventLayerPanels[PanelLayer]->GetEventGroup().GetCount())
		{
			pGroupPanel = m_eventLayerPanels[PanelLayer]->GetEventGroup()[Group]; 
		}
	}

	return pGroupPanel; 
}

void CEventLayersPane::Render(CAnimSceneEditor* animSceneEditor)
{	
	Vector2 EventPanelMin(m_screenExtents.GetMinX(), m_screenExtents.GetMinY());

	EventPanelMin.y += animSceneEditor->GetEventLayersYScreenOffset(); 

	Vector2 EventPanelMax(m_screenExtents.GetMaxX(), m_screenExtents.GetMaxY());

	//render the whole event pane
	//Render container surround groups and the timeline
	/*
	float eventsHeightMin = m_screenExtents.GetMinY();
	//float eventsHeightMax =  m_screenExtents.GetMinY() + eventsHeightMin + ( m_groups.GetCount() > 0 ? (float)(m_groups[0]->GetGroupHeight()) :  0 ); 
	float padding = 15.0f;
	float underneathTimeline = (float)SCREEN_HEIGHT - 10.0f;

	grcColor(animSceneEditor->GetColor(CAnimSceneEditor::kSelectedTextColor));
	grcBegin(drawLineStrip,5);
	grcVertex2f(m_screenExtents.GetMinX() - padding, eventsHeightMin);
	grcVertex2f(m_screenExtents.GetMaxX() + padding, eventsHeightMin);
	grcVertex2f(m_screenExtents.GetMaxX() + padding, underneathTimeline);
	grcVertex2f(m_screenExtents.GetMinX() - padding, underneathTimeline);

	grcVertex2f(m_screenExtents.GetMinX() - padding, eventsHeightMin);
	grcEnd();*/

	for(int i = 0; i < m_eventLayerPanels.GetCount(); i++)
	{
		float PanelHeight = m_eventLayerPanels[i]->GetTotalPanelHeight(); 
		
		 EventPanelMax.y = EventPanelMin.y + PanelHeight; 

		 if(! (EventPanelMin.y < m_screenExtents.GetMinY() || EventPanelMax.y > m_screenExtents.GetMaxY()) )
		 {
			m_eventLayerPanels[i]->SetScreenExtents(CScreenExtents(EventPanelMin, EventPanelMax)); 
			m_eventLayerPanels[i]->Render(animSceneEditor); 
		 }
		 
		 EventPanelMin.y = EventPanelMax.y;
	}
}

void CEventLayersPane::RemoveEventLayerPanel(int i)
{
	if(m_eventLayerPanels[i])
	{
		delete m_eventLayerPanels[i]; 
		m_eventLayerPanels[i] = NULL; 

		m_eventLayerPanels.Delete(i); 
	}
}

atHashString CSectioningPanel::ms_NoListString("*");
atHashString CSectioningPanel::ms_InvalidPlaybackListId(ANIM_SCENE_PLAYBACK_LIST_ID_INVALID);
atHashString CSectioningPanel::ms_InvalidSectionId(ANIM_SCENE_SECTION_ID_INVALID);

//////////////////////////////////////////////////////////////////////////

void CSectioningPanel::Render(CAnimSceneEditor* animSceneEditor)
{
	Vector2 EventPanelMin(m_screenExtents.GetMinX(), m_screenExtents.GetMinY());
	Vector2 EventPanelMax(m_screenExtents.GetMaxX(), m_screenExtents.GetMaxY());
	float mouseX = static_cast<float>(ioMouse::GetX());
	float mouseY = static_cast<float>(ioMouse::GetY());
	bool leftMouseDown = ioMouse::GetButtons() & ioMouse::MOUSE_LEFT;

	bool playbackListHasFocus = false;

	CAnimScene* pScene = animSceneEditor->GetScene();

	if (pScene)
	{
		float xOffset = m_scrollOffset;
		const float spacing = 15.0f;
		s32 hoveredSectionIndex = -1;
		Vector2 selectedListBL(0.0f, 0.0f), selectedListBR(0.0f, 0.0f);

		//Render complete extents
		Color32 outlineColour = animSceneEditor->GetColor(CAnimSceneEditor::kRegionOutlineColor);
		outlineColour.SetAlpha(255);
		grcColor(outlineColour);

		grcBegin(drawLineStrip,5);
		grcVertex2f(m_screenExtents.GetMinX() - spacing + xOffset,m_screenExtents.GetMinY() - spacing);
		grcVertex2f(m_screenExtents.GetMinX() - spacing + xOffset,m_screenExtents.GetMaxY() + spacing);
		
		grcVertex2f((float)SCREEN_WIDTH,m_screenExtents.GetMaxY() + spacing);
		grcVertex2f((float)SCREEN_WIDTH,m_screenExtents.GetMinY() - spacing);

		grcVertex2f(m_screenExtents.GetMinX() - spacing + xOffset,m_screenExtents.GetMinY() - spacing);
		grcEnd();
		
		//Render a draggable bar to adjust x offset of playback list and sections
		//Initially go through and work out the total size of the playback lists and sections
		float playbackListTotalXSize = 0.f;
		float sectionListTotalXSize = 0.f;
		for (s32 i=-1; i<pScene->GetPlayBackLists().GetCount(); i++)
		{
			CAnimScenePlaybackList::Id listId;

			if (i==-1)
			{
				listId = ms_NoListString;
			}
			else
			{
				listId = *pScene->GetPlayBackLists().GetKey(i);
			}

			float width = GetTextWidth(listId.GetCStr()) + spacing;
			playbackListTotalXSize += width;
		}
		CAnimSceneSectionIterator sectionIt(*pScene, true);
		while (*sectionIt)
		{
			CAnimSceneSection::Id id = sectionIt.GetId();
			float width = GetTextWidth(id.GetCStr()) + spacing;
			sectionListTotalXSize += width;
			++sectionIt;
		}

		//Check whether the playback list or section list goes off the side of the screen
		if(m_screenExtents.GetMinX() + playbackListTotalXSize > m_screenExtents.GetMaxX() ||
			m_screenExtents.GetMinX() + sectionListTotalXSize > m_screenExtents.GetMaxX() )
		{
			//percentage of playback list on screen
			float totalScreenX = m_screenExtents.GetMaxX() - m_screenExtents.GetMinX();
			float playbackListOnScreenAmount = totalScreenX/playbackListTotalXSize;

			float sectionsOnScreenAmount = totalScreenX/sectionListTotalXSize;

			//We use the playback list size if there's less on screen (it is larger)
			bool usingPlaybackListSize = playbackListOnScreenAmount < sectionsOnScreenAmount;

			m_scrollbar.SetBackgroundColour(animSceneEditor->GetColor(CAnimSceneEditor::kScrollBarBackgroundColour));
			m_scrollbar.SetForegroundColour(animSceneEditor->GetColor(CAnimSceneEditor::kMainTextColor));
			m_scrollbar.SetTopLeft(Vector2(m_screenExtents.GetMinX(),m_screenExtents.GetMaxY()));
			m_scrollbar.SetMinValue(0.f);

			if(usingPlaybackListSize)
			{
				m_scrollbar.SetMaxValue(playbackListTotalXSize);
				m_scrollbar.SetBarSize(playbackListOnScreenAmount);
				if(playbackListOnScreenAmount >= 1.f)
				{
					m_scrollbar.SetBarHidden(true);
				}
			}
			else
			{
				m_scrollbar.SetMaxValue(sectionListTotalXSize);
				m_scrollbar.SetBarSize(sectionsOnScreenAmount);
				if(sectionsOnScreenAmount >= 1.f)
				{
					m_scrollbar.SetBarHidden(true);
				}
			}

			m_scrollbar.SetLength(m_screenExtents.GetMaxX() - m_screenExtents.GetMinX());
			m_scrollbar.SetValue(m_scrollOffset*-1.f);
			if(m_scrollbar.IsDragging() || !animSceneEditor->IsDragging())
			{
				m_scrollbar.ProcessMouseInput();
			}			
			m_scrollbar.Render();
			m_scrollOffset = m_scrollbar.GetValue() * -1.f;
			if(m_scrollbar.IsDragging())
			{
				m_flags.SetFlag(kDraggingScrollBar);
			}
			else
			{
				m_flags.ClearFlag(kDraggingScrollBar);
			}
		}
		else //All sections and PBLs are on screen, ensure the screen is positioned at the default location
		{
			m_scrollOffset = 0.f;
		}
		
		// render the available playback lists in a row.
		for (s32 i=-1; i<pScene->GetPlayBackLists().GetCount(); i++)
		{
			CAnimScenePlaybackList::Id listId;

			if (i==-1)
			{
				listId = ms_NoListString;
			}
			else
			{
				listId = *pScene->GetPlayBackLists().GetKey(i);
			}

			float width = GetTextWidth(listId.GetCStr()) + spacing;
			bool isHovering = (mouseX>=m_screenExtents.GetMinX() + xOffset) && (mouseX<=m_screenExtents.GetMinX() + xOffset + GetTextWidth(listId.GetCStr()))
				&& (mouseY>=m_screenExtents.GetMinY()) && (mouseY<=m_screenExtents.GetMinY()+GetTextHeight());
			bool isSelected = listId.GetHash()==m_selectedList.GetHash();
			bool hasFocus = isSelected && animSceneEditor->GetFocus()==CAnimSceneEditor::kFocusPlaybackList;
			playbackListHasFocus |= hasFocus;
			
			RenderTextWithBackground(listId.GetCStr(), m_screenExtents.GetMinX() + xOffset,  m_screenExtents.GetMinY(), 
				animSceneEditor->GetColor(isHovering ? CAnimSceneEditor::kSelectedTextHoverColor : isSelected ? CAnimSceneEditor::kSelectedTextColor : CAnimSceneEditor::kMainTextColor), 
				animSceneEditor->GetColor(isSelected ? CAnimSceneEditor::kSelectedTextBackGroundColor :CAnimSceneEditor::kTextBackgroundColor), 3.0f, true, 
				animSceneEditor->GetColor(hasFocus ? CAnimSceneEditor::kFocusTextOutlineColor : isSelected ? CAnimSceneEditor::kSelectedTextOutlineColor :CAnimSceneEditor::kTextOutlineColor));

			if (isHovering && !animSceneEditor->IsDragging() && leftMouseDown)
			{
				m_selectedList = listId;
				m_flags.SetFlag(kSelectedListChanged);
				animSceneEditor->SetFocus(CAnimSceneEditor::kFocusPlaybackList);
				if (m_selectedList!=ms_InvalidPlaybackListId)
				{
					// always reset the selected section if not displaying the master list
					m_selectedSection = ANIM_SCENE_SECTION_ID_INVALID;
				}
			}

			// render a selection box around the selected list
			if (m_selectedList==listId)
			{
				float xMin = m_screenExtents.GetMinX() + xOffset - 6.0f;
				float xMax = xMin + width - 3.0f;
				float yMin = m_screenExtents.GetMinY() - 6.0f;
				float yMax = yMin + (m_screenExtents.GetHeight()*0.333f);

				selectedListBL.x = xMin;
				selectedListBL.y = yMax;
				selectedListBR.x = xMax;
				selectedListBR.y = yMax;

				grcColor(animSceneEditor->GetColor(hasFocus ? CAnimSceneEditor::kFocusTextOutlineColor : CAnimSceneEditor::kSelectedTextOutlineColor));
				grcBegin(drawLineStrip, 4);
				grcVertex2f(xMin, yMax);
				grcVertex2f(xMin, yMin);
				grcVertex2f(xMax, yMin);
				grcVertex2f(xMax, yMax);
				grcEnd();
			}

			xOffset += width;
		}
		
		xOffset = m_scrollOffset;

		// render the currently selected sections in a row.
		CAnimSceneSectionIterator it(*pScene);
		bool hasSections = false;
		float lastXMid = -FLT_MAX;
		s32 index = 0;

		float yMin = m_screenExtents.GetMinY() + m_screenExtents.GetHeight()*0.333f;
		float yMax = m_screenExtents.GetMinY() + m_screenExtents.GetHeight()*0.666f;
		float totalSectionsWidth = 0.f;

		while (*it)
		{
			CAnimSceneSection::Id id = it.GetId();

			if (id!=m_draggedSection)
			{
				float width = GetTextWidth(id.GetCStr()) + spacing;
				float xMid = m_screenExtents.GetMinX() + xOffset + (GetTextWidth(id.GetCStr())*0.5f);
				float textPadding = 3.0f;

				if (IsDraggingSection() && mouseX>=lastXMid && mouseX<xMid)
				{
					hoveredSectionIndex = index;
					xOffset+=GetTextWidth(m_draggedSection.GetCStr()) + spacing;
				}

				bool isHovering = (mouseX>=m_screenExtents.GetMinX() + xOffset) && (mouseX<=m_screenExtents.GetMinX() + xOffset + GetTextWidth(id.GetCStr()))
					&& (mouseY>=yMin - textPadding) && (mouseY<=yMin + textPadding);
				bool displayingMasterList = (GetSelectedPlaybackList()==ms_InvalidPlaybackListId);
				bool isSelected = displayingMasterList && m_selectedSection.GetHash()==id.GetHash();

				RenderTextWithBackground(id.GetCStr(), m_screenExtents.GetMinX() + xOffset,  yMin, 
					animSceneEditor->GetColor(isHovering ? CAnimSceneEditor::kSelectedTextHoverColor : isSelected ? CAnimSceneEditor::kSelectedTextColor : CAnimSceneEditor::kMainTextColor), 
					animSceneEditor->GetColor(isSelected ? CAnimSceneEditor::kSelectedTextBackGroundColor :CAnimSceneEditor::kTextBackgroundColor), textPadding, true, 
					animSceneEditor->GetColor(isSelected ? CAnimSceneEditor::kSelectedTextOutlineColor :CAnimSceneEditor::kTextOutlineColor));

				// start dragging or select sections for delete as appropriate
				if (isHovering && leftMouseDown && !animSceneEditor->IsDragging())
				{
					if (displayingMasterList)
					{
						// when displaying the master list, allow selecting sections for deletion
						m_selectedSection = id;
						animSceneEditor->SetFocus(CAnimSceneEditor::kFocusSection);
					}
					else
					{
						// when showing a playlist, allow dragging sections out of the list
						m_dragStartTime = fwTimer::GetTimeInMilliseconds();
						m_draggedSection = id;
						m_flags.SetFlag(kDraggingSectionFromPlaybackList);
					}
				}

				lastXMid = xMid;
				hasSections = true;
				xOffset += width;
				totalSectionsWidth += width;
				index++;
			}
			
			++it;
		}

		if (!hasSections)
		{
			atHashString str("Drag sections here to add them to the list");
			float yMin = m_screenExtents.GetMinY() + m_screenExtents.GetHeight()*0.333f;
			float yMax = m_screenExtents.GetMinY() + m_screenExtents.GetHeight()*0.666f;
			bool isHovering = (mouseX>=m_screenExtents.GetMinX() + xOffset) && (mouseX<=m_screenExtents.GetMinX() + xOffset + GetTextWidth(str.GetCStr()))
				&& (mouseY>=yMin) && (mouseY<=yMax);

			RenderText(str.GetCStr(), m_screenExtents.GetMinX() + xOffset,  m_screenExtents.GetMinY() + m_screenExtents.GetHeight()*0.333f, 
				animSceneEditor->GetColor(isHovering ? CAnimSceneEditor::kTextWarningColor : CAnimSceneEditor::kTextWarningColor));

			xOffset = GetTextWidth(str.GetCStr());
		}

		// Draw a connected border around the section list for the selected playback list
		if (m_selectedList!=ANIM_SCENE_PLAYBACK_LIST_ID_INVALID)
		{
			float xMin = m_screenExtents.GetMinX() + m_scrollOffset;
			float xMax = totalSectionsWidth + xMin; // m_screenExtents.GetMaxX();

			grcColor(animSceneEditor->GetColor(playbackListHasFocus ? CAnimSceneEditor::kFocusTextOutlineColor : CAnimSceneEditor::kSelectedTextOutlineColor));
			grcBegin(drawLineStrip, 6);
			grcVertex2f(selectedListBL.x, selectedListBL.y);
			grcVertex2f(xMin-6.0f, yMin-6.0f);
			grcVertex2f(xMin-6.0f, yMax-6.0f);
			grcVertex2f(xMax-6.0f, yMax-6.0f);
			grcVertex2f(xMax-6.0f, yMin-6.0f);
			grcVertex2f(selectedListBR.x, selectedListBR.y);
			grcEnd();
		}
		

		// Render the master list of sections
		if (GetSelectedPlaybackList()!=ms_InvalidPlaybackListId)
		{
			if (IsDraggingSection())
			{
				// render the dragged section at the mouse pointer
				RenderTextWithBackground(m_draggedSection.GetCStr(), mouseX,  mouseY, 
					animSceneEditor->GetColor( CAnimSceneEditor::kSelectedTextHoverColor), 
					animSceneEditor->GetColor( CAnimSceneEditor::kSelectedTextBackGroundColor), 3.0f, true, 
					animSceneEditor->GetColor( CAnimSceneEditor::kSelectedTextOutlineColor));

				if(!m_scrollbar.IsBarHidden())
				{
					if(mouseX > m_screenExtents.GetMaxX() - 10.f)
					{
						float amount = mouseX - m_screenExtents.GetMaxX() + 5.f;
						m_scrollbar.ScrollPositiveAmount(amount);
						m_scrollOffset = -m_scrollbar.GetValue();
					}
					else if (mouseX < m_screenExtents.GetMinX() + 10.f)
					{
						float amount =  m_screenExtents.GetMinX() - mouseX + 5.f;
						m_scrollbar.ScrollNegativeAmount(amount);
						m_scrollOffset = -m_scrollbar.GetValue();
					}
				}
				

				if (!leftMouseDown)
				{
					static u32 s_ClickTime = 150;
					if ((fwTimer::GetTimeInMilliseconds() - m_dragStartTime) < s_ClickTime)
					{
						// set the scene time to the start of the 
						animSceneEditor->GetScene()->Skip((animSceneEditor->GetScene()->GetSection(m_draggedSection)->GetStart()));
					}

					// TODO - check if we're over the list pane and add the section to the list
					float yMin = m_screenExtents.GetMinY() + m_screenExtents.GetHeight()*0.2f;
					float yMax = m_screenExtents.GetMinY() + m_screenExtents.GetHeight()*0.666f;
					bool isHovering = (mouseX>=(m_screenExtents.GetMinX() - 30.0f)) && (mouseX<=m_screenExtents.GetMaxX())
						&& (mouseY>=yMin) && (mouseY<=yMax);

					if (isHovering && m_flags.IsFlagSet(kDraggingSectionFromMasterList))
					{
						animSceneEditor->GetScene()->RemoveSectionFromPlaybackList(m_draggedSection, m_selectedList);
						animSceneEditor->GetScene()->AddSectionToPlaybackList(m_draggedSection, m_selectedList, hoveredSectionIndex);
					}
					else if (m_flags.IsFlagSet(kDraggingSectionFromPlaybackList))
					{
						if (!isHovering)
						{
							animSceneEditor->GetScene()->RemoveSectionFromPlaybackList(m_draggedSection, m_selectedList);
						}
						else
						{
							// reorder the entry in the list
							animSceneEditor->GetScene()->RemoveSectionFromPlaybackList(m_draggedSection, m_selectedList);
							animSceneEditor->GetScene()->AddSectionToPlaybackList(m_draggedSection, m_selectedList, hoveredSectionIndex);
						}						
					}

					m_draggedSection = ms_InvalidSectionId;
					m_flags.ClearFlag(kDraggingSectionFromMasterList);
					m_flags.ClearFlag(kDraggingSectionFromPlaybackList);
					m_dragStartTime = 0;
				}
			}
			
			// render all the sections in a row.
			float offset = m_scrollOffset;//0.0f;
			const float spacing = 15.0f;
			CAnimSceneSectionIterator it(*pScene, true);
			while (*it)
			{
				CAnimSceneSection::Id id = it.GetId();
				float width = GetTextWidth(id.GetCStr()) + spacing;
				float yMin = m_screenExtents.GetMinY() + m_screenExtents.GetHeight()*0.666f;
				float yMax = m_screenExtents.GetMaxY();
				bool isDisabled = IsDraggingSection() || animSceneEditor->GetScene()->ContainsSection(id);
				bool isHovering = !isDisabled && (mouseX>=m_screenExtents.GetMinX() + offset) && (mouseX<=m_screenExtents.GetMinX() + offset + GetTextWidth(id.GetCStr()))
					&& (mouseY>=yMin) && (mouseY<=yMax);

				RenderTextWithBackground(id.GetCStr(), m_screenExtents.GetMinX() + offset,  m_screenExtents.GetMinY() + m_screenExtents.GetHeight()*0.666f, 
					animSceneEditor->GetColor( isDisabled ? CAnimSceneEditor::kDisabledTextColor : isHovering ? CAnimSceneEditor::kSelectedTextHoverColor : CAnimSceneEditor::kMainTextColor), 
					animSceneEditor->GetColor( isDisabled ? CAnimSceneEditor::kDisabledBackgroundColor : isHovering ? CAnimSceneEditor::kSelectedTextBackGroundColor : CAnimSceneEditor::kTextBackgroundColor), 3.0f, true, 
					animSceneEditor->GetColor( isDisabled ? CAnimSceneEditor::kDisabledOutlineColor : isHovering ? CAnimSceneEditor::kSelectedTextOutlineColor : CAnimSceneEditor::kTextOutlineColor));

				// start dragging if appropriate
				if ( isHovering && leftMouseDown && !animSceneEditor->IsDragging() && !IsDraggingScrollbar())
				{
					m_dragStartTime = fwTimer::GetTimeInMilliseconds();
					m_draggedSection = id;
					m_flags.SetFlag(kDraggingSectionFromMasterList);
					animSceneEditor->SetFocus(CAnimSceneEditor::kFocusSection);
				}

				offset += width;
				++it;
			}
	
		}
		
	}
}

COnScreenTimeline::COnScreenTimeline()
	: m_currentTime(0.0f)
	, m_minSelectableTime(0.0f)
	, m_maxSelectableTime(1000.0f)
	, m_lastMouseX(0.0f)
	, m_timeMode(kDisplayFrames)
	, m_bMarkerSnapToFrames(true)
	, m_timelineSelectorDraggedCB(NULL)
{

}

COnScreenTimeline::COnScreenTimeline(const CScreenExtents& screenExtents)
	: m_currentTime(0.0f)
	, m_minSelectableTime(0.0f)
	, m_maxSelectableTime(1000.0f)
	, m_lastMouseX(0.0f)
	, m_timeMode(kDisplayFrames)
	, m_bMarkerSnapToFrames(true)
	, m_timelineSelectorDraggedCB(NULL)
{
	m_screenExtents = screenExtents;
}

COnScreenTimeline::~COnScreenTimeline()
{

}

// PURPOSE: Call once per frame to update the timeline
void COnScreenTimeline::Process()
{
	// Duration of a single frame
	static float fFrameDuration = 1.0f / 30.0f;

	float mouseX = static_cast<float>(ioMouse::GetX());
	float mouseY = static_cast<float>(ioMouse::GetY());
	float invZoom = 1.0f/ m_zoomController.m_fZoom;

	// Extents of the entire timeline - bar and text
	const float xMin = m_screenExtents.GetMinX();
	const float xMax = m_screenExtents.GetMaxX();
	const float yMin = m_screenExtents.GetMinY();
	const float yMax = m_screenExtents.GetMaxY();
	const float timelineWidth = xMax - xMin;

	static float minimumTimeLineHeight = 25.0f; // We don't want it too thin

	// Arrow for when current selected time is off either end of the timeline
	static float fArrowWidth = 10.0f;		// Width of arrow when it is off the left or right of the timeline
	static float fArrowGap = 3.0f;			// Gap to the left/right sides of the timeline between it and the arrow.

	// Timeline bar itself
	float timelineMinY = yMin;
	float timelineMaxY = yMax;

	if ((timelineMaxY - timelineMinY) < minimumTimeLineHeight)
	{
		timelineMaxY = timelineMinY + minimumTimeLineHeight;
	}

	float timelineHeight = timelineMaxY - timelineMinY;

	// Limit mouse hovering to the timeline itself
	CScreenExtents timelineExtents(xMin, timelineMinY, xMax, timelineMaxY);
	bool mouseHovering = timelineExtents.Contains(mouseX,mouseY);

	// Is the mouseHovering over the areas of the arrows at either end?
	CScreenExtents leftArrowExtents(xMin - fArrowWidth - fArrowGap, timelineMinY, xMin, timelineMaxY);
	CScreenExtents rightArrowExtents(xMax, timelineMinY, xMax + fArrowWidth + fArrowGap, timelineMaxY);
	bool mouseHoveringLeftArrow = leftArrowExtents.Contains(mouseX, mouseY);
	bool mouseHoveringRightArrow = rightArrowExtents.Contains(mouseX, mouseY);

	// Should we render the current frame label?  Only do this when dragging
	bool bRenderCurrentFrameLabel = false;

	//////////////////////////////////////////////////////////////////////////
	// Update
	//////////////////////////////////////////////////////////////////////////

	if (!m_flags.IsFlagSet(kUserControlDisabled))
	{		
		if (m_flags.IsFlagSet(kDraggingSelector))
		{
			// selector dragging mode
			m_flags.ClearFlag(kDraggingTimeline);
			if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)
			{
				// update the selected time by the mouse x movement
				float dt = (mouseX - m_lastMouseX) * invZoom;

				float fNewTime = GetCurrentTime() + dt;
				SetCurrentTime(Clamp(fNewTime, GetMinVisibleTime(), GetMaxVisibleTime()));

				// As we drag the marker around, show the helper current frame label
				bRenderCurrentFrameLabel = true;
			}
			else
			{
				// Upon stopping dragging of the marker, should it snap to the start of the frame boundary?
				if (m_bMarkerSnapToFrames)
				{
					int iFrameNumber = (int)(GetCurrentTime() / (1.0f/30.0f));
					float fNewTime = (float)iFrameNumber * (1.0f/30.0f);

					// If past half-way point of a frame, round up instead...
					if ((GetCurrentTime() - fNewTime) >= (0.5f * (1.0f/30.0f)))
					{
						fNewTime = (float)(iFrameNumber + 1) * (1.0f/30.0f);
					}

					SetCurrentTime(fNewTime);
				}

				m_flags.ClearFlag(kDraggingSelector);
			}
			
			// Time changed due to dragging selector callback
			if (m_timelineSelectorDraggedCB)
			{
				m_timelineSelectorDraggedCB(GetCurrentTime());
			}
		}
		else if (m_flags.IsFlagSet(kDraggingTimeline))
		{
			// Timeline dragging mode
			if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)
			{
				m_zoomController.UpdateZoom(xMin, xMax, mouseX, this);
			}
			else
			{
				m_flags.ClearFlag(kDraggingTimeline);
			}
		}
		else if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT )
		{
			// free mouse mode
			if (IsHoveringSelector())
			{
				m_flags.SetFlag(kDraggingSelector);
			}
			else if (mouseHovering)
			{
				m_flags.SetFlag(kDraggingTimeline);
			}
			else if ((mouseHoveringLeftArrow && GetCurrentTime() < GetMinVisibleTime()) || (mouseHoveringRightArrow && GetCurrentTime() > GetMaxVisibleTime()))
			{
				MoveMarkerToVisibleTimelinePosition(0.5f);
			}
		}
		else
		{
			// update just the zooming control for the timeline
			m_zoomController.UpdateZoom(xMin, xMax, mouseX, this, true);
		}
	}

	// Clamp timeline offset to keep it visible
	ClampTimelineOffset();

	// Calculate some information we'll use throughout
	const float timelineStartTime = GetMinVisibleTime();
	const float timelineEndTime = GetMaxVisibleTime();
	const float timelineRangeTime = timelineEndTime - timelineStartTime;

	const int timelineStartFrame = (int)(ceil(timelineStartTime / fFrameDuration));
	const int timelineEndFrame = (int)(ceil(timelineEndTime / fFrameDuration));

	const int numVisibleFrames = timelineEndFrame - timelineStartFrame;
	float fPixelsPerFrame = 1.0f;
	if (numVisibleFrames > 0)
	{
		fPixelsPerFrame = timelineWidth / (float)numVisibleFrames;
	}

	const float timelineMinSelectableTime = timelineStartTime < m_minSelectableTime ? m_minSelectableTime : timelineStartTime;
	const int timelineMinSelectableFrame = (int)(ceil(timelineMinSelectableTime / fFrameDuration));
	 
	//////////////////////////////////////////////////////////////////////////
	// Render - Timeline Bar & End of selectable range lines
	//////////////////////////////////////////////////////////////////////////

	// Render the timeline to the screen at the specified extents
	grcLighting(false);

	bool bStartOfSelectableRangeIsInView = (m_minSelectableTime >= timelineStartTime && m_minSelectableTime <= timelineEndTime);
	float fStartOfSelectableRangeTimelineX = bStartOfSelectableRangeIsInView ? GetScreenX(m_minSelectableTime) : xMin;

	bool bEndOfSelectableRangeIsInView = (m_maxSelectableTime >= timelineStartTime && m_maxSelectableTime <= timelineEndTime);
	float fEndOfSelectableRangeTimelineX = bEndOfSelectableRangeIsInView ? GetScreenX(m_maxSelectableTime) : xMax;

	// Render timeline dark background
	grcColor(GetColor(kTimelineBackgroundInRangeColor));
	grcBegin(drawTriStrip, 4);
	{
		grcVertex2f(fStartOfSelectableRangeTimelineX,	timelineMinY);
		grcVertex2f(fEndOfSelectableRangeTimelineX,		timelineMinY);
		grcVertex2f(fStartOfSelectableRangeTimelineX,	timelineMaxY);
		grcVertex2f(fEndOfSelectableRangeTimelineX,		timelineMaxY);
	}
	grcEnd();

	// Render even darker background before the range of the selectable range
	if (bStartOfSelectableRangeIsInView)
	{
		grcColor(GetColor(kTimelineBackgroundOutOfRangeColor));
		grcBegin(drawTriStrip, 4);
		{
			grcVertex2f(xMin, timelineMinY);
			grcVertex2f(fStartOfSelectableRangeTimelineX, timelineMinY);
			grcVertex2f(xMin, timelineMaxY);
			grcVertex2f(fStartOfSelectableRangeTimelineX, timelineMaxY);
		}
		grcEnd();
	}

	// Render even darker background beyond the range of the selectable range
	if (bEndOfSelectableRangeIsInView)
	{
		grcColor(GetColor(kTimelineBackgroundOutOfRangeColor));
		grcBegin(drawTriStrip, 4);
		{
			grcVertex2f(fEndOfSelectableRangeTimelineX, timelineMinY);
			grcVertex2f(xMax, timelineMinY);
			grcVertex2f(fEndOfSelectableRangeTimelineX, timelineMaxY);
			grcVertex2f(xMax, timelineMaxY);
		}
		grcEnd();
	}

	// White for outlines
	grcColor(GetColor(kTimelineOutlineColor));

	// Render a solid white line, top to bottom at the start of the scene.
	if (bStartOfSelectableRangeIsInView)
	{
		grcBegin(drawLineStrip, 2);
		grcVertex2f(fStartOfSelectableRangeTimelineX, timelineMinY);
		grcVertex2f(fStartOfSelectableRangeTimelineX, timelineMaxY);
		grcEnd();
	}

	// Render a solid white line, top to bottom at the end of the scene.
	if (bEndOfSelectableRangeIsInView)
	{
		grcBegin(drawLineStrip, 2);
		grcVertex2f(fEndOfSelectableRangeTimelineX, timelineMinY);
		grcVertex2f(fEndOfSelectableRangeTimelineX, timelineMaxY);
		grcEnd();
	}

	// Render timeline white outline
	grcBegin(drawLineStrip, 5);
	grcVertex2f(xMin, timelineMinY);
	grcVertex2f(xMax, timelineMinY);
	grcVertex2f(xMax, timelineMaxY);
	grcVertex2f(xMin, timelineMaxY);
	grcVertex2f(xMin, timelineMinY);
	grcEnd();

	//////////////////////////////////////////////////////////////////////////
	// Render - Timeline Bar - Time Markers
	//////////////////////////////////////////////////////////////////////////

	static float fTimeMarkerHeight30 = 0.5f * timelineHeight;	// Largest marker for every 30 frame (1 second) mark
	static float fTimeMarkerHeight5 = 0.35f * timelineHeight;
	static float fTimeMarkerHeight1 = 0.2f * timelineHeight;	// Smallest marker for every 1 frame mark

	static float fMinimumTimeMarkerGap = 3.0f;	// Minimum gap allowed between time markers

	// Should we render the smaller markers?
	bool bRender1FrameMarkers = (fPixelsPerFrame > fMinimumTimeMarkerGap);
	bool bRender5FrameMarkers = ((5.0f * fPixelsPerFrame) > fMinimumTimeMarkerGap);

	// Optimisation: If we know we're not rendering the smaller frame markers, then we can avoid going over every frame in the below for loop
	int iMarkerStartFrame = timelineMinSelectableFrame;
	int iMarkerJumpAmount = 1;

	if (!bRender5FrameMarkers)
	{
		// We won't be rendering the 5 or 1 frame markers here, so the for-loop increments should be to the 30 frame markers
		iMarkerJumpAmount = 30;
		if (iMarkerStartFrame % 30 != 0)
		{
			iMarkerStartFrame = iMarkerStartFrame + (30 - (iMarkerStartFrame % 30));
		}
	}
	else if (!bRender1FrameMarkers)
	{
		// We won't be rendering the 1 frame markers here, so the for-loop increments should be to the 5 frame markers
		iMarkerJumpAmount = 5;
		if (iMarkerStartFrame % 5 != 0)
		{
			iMarkerStartFrame = iMarkerStartFrame + (5 - (iMarkerStartFrame % 5));
		}
	}

	// Render the markers
	for (int i = iMarkerStartFrame; i < timelineEndFrame; i += iMarkerJumpAmount)
	{
		// Don't render markers beyond the end of the selectable timeline.
		float fMarkerTime = i * fFrameDuration;
		if (fMarkerTime >= m_maxSelectableTime)
		{
			break;
		}

		// X position of the marker to be rendered
		float markerPointX = GetScreenX(fMarkerTime);

		// Don't render 1-frame time markers where we'll be rendering larger ones.
		if (bRender1FrameMarkers && i % 5 != 0 && i % 30 != 0)
		{
			grcColor(GetColor(kTimelineTimeMarker1FrameColor));
			grcBegin(drawLineStrip, 2);
			grcVertex2f(markerPointX, timelineMinY + fTimeMarkerHeight1);
			grcVertex2f(markerPointX, timelineMinY);
			grcEnd();
		}
		else if (bRender5FrameMarkers && i % 5 == 0 && i % 30 != 0)
		{
			grcColor(GetColor(kTimelineTimeMarker5FrameColor));
			grcBegin(drawLineStrip, 2);
			grcVertex2f(markerPointX, timelineMinY + fTimeMarkerHeight5);
			grcVertex2f(markerPointX, timelineMinY);
			grcEnd();
		}
		else if (i % 30 == 0)
		{
			grcColor(GetColor(kTimelineTimeMarker30FrameColor));
			grcBegin(drawLineStrip, 2);
			grcVertex2f(markerPointX, timelineMinY + fTimeMarkerHeight30);
			grcVertex2f(markerPointX, timelineMinY);
			grcEnd();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Render - Timeline Bar - Time Markers - Text
	//////////////////////////////////////////////////////////////////////////

	// How many frames does the timeline represent?
	int iNumFramesVisible = (int)(timelineRangeTime / fFrameDuration);

	const int idealNumberOfLabels = 12;
	int iFramesBetweenLabels = iNumFramesVisible / idealNumberOfLabels;

	if (iFramesBetweenLabels >= 25 && iFramesBetweenLabels % 30 != 0)
	{
		// Round it up to a multiple of 30
		iFramesBetweenLabels += (30 - (iFramesBetweenLabels % 30));
	}	
	else if (iFramesBetweenLabels % 5 != 0)
	{
		// Round it up to a multiple of 5
		iFramesBetweenLabels += (5 - (iFramesBetweenLabels % 5));
	}

	// What's the first label to render?
	int iFirstLabelFrame = timelineMinSelectableFrame;
	if (iFirstLabelFrame % iFramesBetweenLabels != 0)
	{
		iFirstLabelFrame = iFirstLabelFrame + (iFramesBetweenLabels - (iFirstLabelFrame % iFramesBetweenLabels));
	}

	const float fLabelY = timelineMinY + fTimeMarkerHeight30;

	for (int i = iFirstLabelFrame; i < timelineEndFrame; i += iFramesBetweenLabels)
	{
		// Don't render labels beyond the end of the selectable timeline.
		float fLabelTime = i * fFrameDuration;
		if (fLabelTime >= m_maxSelectableTime)
		{
			break;
		}

		float fLabelX = GetScreenX(fLabelTime);

		// Generate the label
		atString labelString;
		if (m_timeMode == kDisplayFrames)
		{
			atVarString label("%d", i);
			labelString = label;

		}
		else if (m_timeMode == kDisplayTimes)
		{
			atVarString label("%.2f", fLabelTime);
			labelString = label;
		}

		// Do not render the label if it's off the end of the timeline
		float fLabelWidth = GetTextWidth(labelString.c_str());
		if ((fLabelX + fLabelWidth) < xMax)
		{
			RenderText(labelString.c_str(), fLabelX, fLabelY, GetColor(kTimelineTimeMarkerLabelColor));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Render - Timeline Selector and Arrows
	//////////////////////////////////////////////////////////////////////////

	float selectedScreenX = GetScreenX(GetCurrentTime());

	// Selector arrow visible when the current selected time is within the visible timeline
	static float fSelectorArrowGap = 3.0f;	// Space at the top and bottom between the selector arrow and the timeline bar edges.
	static float fSelectorWidth = 5.0f;

	const float fArrowTopY = timelineMinY + fSelectorArrowGap;
	const float fArrowBottomY = timelineMaxY - fSelectorArrowGap;
	const float fArrowMidY = fArrowTopY + ((fArrowBottomY - fArrowTopY) * 0.5f);

	if (selectedScreenX < xMin)
	{
		// Render a left arrow.
		grcColor(mouseHoveringLeftArrow ? GetColor(kTimelineSelectorHoverColor) : GetColor(kTimelineSelectorColor));
		grcBegin(drawTris, 3);
		grcVertex2f(xMin - fArrowGap, fArrowTopY);
		grcVertex2f(xMin - fArrowGap, fArrowBottomY);
		grcVertex2f(xMin - fArrowWidth - fArrowGap, fArrowMidY);
		grcEnd();
	}
	else if (selectedScreenX > xMax)
	{
		// Render a right arrow.
		grcColor(mouseHoveringRightArrow ? GetColor(kTimelineSelectorHoverColor) : GetColor(kTimelineSelectorColor));
		grcBegin(drawTris, 3);
		grcVertex2f(xMax + fArrowGap, fArrowTopY);
		grcVertex2f(xMax + fArrowWidth + fArrowGap, fArrowMidY);
		grcVertex2f(xMax + fArrowGap, fArrowBottomY);		
		grcEnd();
	}
	else
	{
		// Render a triangle selector for the current selected time.
		grcColor(GetColor(kTimelineBackgroundOutOfRangeColor));
		//render a black outline
		grcBegin(drawTris, 3);
		grcVertex2f(selectedScreenX - fSelectorWidth - 1.0f, timelineMaxY - fSelectorArrowGap + 1.0f);
		grcVertex2f(selectedScreenX + fSelectorWidth + 1.0f, timelineMaxY - fSelectorArrowGap + 1.0f);
		grcVertex2f(selectedScreenX, timelineMinY + 0.5f);
		grcEnd();

		if (m_flags.IsFlagSet(kDraggingSelector))
		{
			grcColor(GetColor(kTimelineSelectorDraggingColor));
		}
		else if (IsHoveringSelector())
		{
			grcColor(GetColor(kTimelineSelectorHoverColor));
		}
		else
		{
			grcColor(GetColor(kTimelineSelectorColor));
		}		
		grcBegin(drawTris, 3);
		grcVertex2f(selectedScreenX - fSelectorWidth, timelineMaxY - fSelectorArrowGap);
		grcVertex2f(selectedScreenX + fSelectorWidth, timelineMaxY - fSelectorArrowGap);
		grcVertex2f(selectedScreenX, timelineMinY + 1.0f);
		grcEnd();
	}

	//////////////////////////////////////////////////////////////////////////
	// Render - Current Selector Time
	//////////////////////////////////////////////////////////////////////////

	if (bRenderCurrentFrameLabel)
	{
		// Lower text position Y
		float selectorTimeTextY = timelineMinY + fTimeMarkerHeight30;

		atString selectorTimeStr;

		if (m_timeMode == kDisplayFrames)
		{
			int iFrameNumber = int(m_currentTime / fFrameDuration);
			atVarString str("%d", iFrameNumber);
			selectorTimeStr = str;
		}
		else if (m_timeMode == kDisplayTimes)
		{
			atVarString str("%.2f", m_currentTime);
			selectorTimeStr = str;
		}

		float fStringWidth = GetTextWidth(selectorTimeStr.c_str());
		float fStringPosX = GetScreenX(m_currentTime);
		fStringPosX -= (0.5f * fStringWidth);

		fStringPosX = Clamp(fStringPosX, xMin, xMax - fStringWidth);
		RenderTextWithBackground(selectorTimeStr.c_str(), fStringPosX, selectorTimeTextY, GetColor(kTimelineSelectorTimeTextColor), GetColor(kTimelineSelectorTimeBackgroundColor));
	}

	//////////////////////////////////////////////////////////////////////////
	// Remember any values for next update
	//////////////////////////////////////////////////////////////////////////

	m_lastMouseX = mouseX;
}

void COnScreenTimeline::SetColor(u32 colorType, Color32 color)
{
	if (colorType < kTimelineColor_MaxNum)
	{
		m_colors[colorType] = color; 
	}
}


Color32 COnScreenTimeline::GetColor(eColorType type)
{
	if (type < kTimelineColor_MaxNum)
	{
		return m_colors[type];
	}

	return Color32(0,0,0,0);

	/*

	switch (type)
	{
	case COnScreenTimeline::kTimelineSelectorColor:
		return Color32(160, 160, 160, 160);
	case COnScreenTimeline::kTimelineSelectorHoverColor:
		return Color32(255, 255, 255, 160);
	case COnScreenTimeline::kTimelineSelectorDraggingColor:
		return Color32(255, 0, 0, 160);
	case COnScreenTimeline::kTimelineSelectorTimeTextColor:
		return Color32(0, 0, 0, 128);
	case COnScreenTimeline::kTimelineSelectorTimeBackgroundColor:
		return Color32(128, 128, 128, 128);
	case COnScreenTimeline::kTimelineOutlineColor:
		return Color32(128, 128, 128, 128);
	case COnScreenTimeline::kTimelineBackgroundInRangeColor:
		return Color32(0, 0, 0, 128);
	case COnScreenTimeline::kTimelineBackgroundOutOfRangeColor:
		return Color32(0, 0, 0, 160);
	case COnScreenTimeline::kTimelineTimeMarkerLabelColor:
		return Color32(128, 128, 128, 128);
	case COnScreenTimeline::kTimelineTimeMarker1FrameColor:
		return Color32(64, 64, 64, 128);
	case COnScreenTimeline::kTimelineTimeMarker5FrameColor:
		return Color32(96, 96, 96, 128);
	case COnScreenTimeline::kTimelineTimeMarker30FrameColor:
		return Color32(128, 128, 128, 128);

	default:
		return Color32(0,0,0,0);
	}
	*/
}

void COnScreenTimeline::ClampTimelineOffset()
{
	float invZoom = 1.0f / m_zoomController.m_fZoom;
	float timelineWidth = m_screenExtents.GetMaxX() - m_screenExtents.GetMinX();
	float timelineTimeRange = timelineWidth * invZoom;
	float fMinimumTimelineStartTime = m_minSelectableTime - timelineTimeRange;

	// Don't go below the minimum range
	if (m_zoomController.m_fTimeLineOffset > -(fMinimumTimelineStartTime * m_zoomController.m_fZoom))
	{
		m_zoomController.m_fTimeLineOffset = -(fMinimumTimelineStartTime * m_zoomController.m_fZoom);
	}

	// Don't go above the maximum range
	if (m_zoomController.m_fTimeLineOffset < (-m_maxSelectableTime * m_zoomController.m_fZoom))
	{
		m_zoomController.m_fTimeLineOffset = (-m_maxSelectableTime * m_zoomController.m_fZoom);
	}
}

void COnScreenTimeline::TimeLineZoom::CalculateInitialZoom(float xMin, float xMax, float fRangeStart, float fRangeEnd)
{
	float fRange = fRangeEnd - fRangeStart;
	if (fRange < 0.01f)
	{
		m_fZoom = 10.0f;
		return;
	}

	// Initially display the range as approx 90% of the timeline
	fRange *= 1.1f;

	float fTimelineWidth = xMax - xMin;

	m_fZoom = fTimelineWidth / fRange;
	m_fTimeLineOffset = 0;
}

void COnScreenTimeline::TimeLineZoom::UpdateZoom(float xMin, float xMax, float fMouse, COnScreenTimeline* owningTimeline, bool bZoomOnly /*=false*/)
{
	Assert(owningTimeline);
	if (ioMouse::HasWheel() && (ioMouse::GetDZ()!=0))
	{
		// Get the time at the mouse position
		float timeAtMousePos = owningTimeline->GetTimeForScreenX(fMouse);

		// Update zoom
		const float kMouseZoomFactor = 1.25f;
		m_fZoom *= ioMouse::GetDZ() > 0 ? kMouseZoomFactor : 1.0f / kMouseZoomFactor;
		m_fZoom = Clamp(m_fZoom, 1.0f, 1000.0f);

		// Get how far the mouse is along the timeline as a ratio.
		float fMousePosXRatio = (fMouse - xMin) / (xMax - xMin);

		// Get the range of the timeline
		float fTimelineRange = owningTimeline->GetMaxVisibleTime() - owningTimeline->GetMinVisibleTime();

		// Calculate the time the start of the timeline needs to be in order to keep the same time at the mouse position post zoom.
		float fNewTimelineStartTime = timeAtMousePos - (fTimelineRange * fMousePosXRatio);

		// Now work out the offset required to put the start time there
		m_fTimeLineOffset = -(fNewTimelineStartTime * m_fZoom);
	}
	else if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT && !bZoomOnly)
	{
		if (m_LastMouseTrackFrame == (TIME.GetFrameCount() - 1))
		{
			float fDeltaX = fMouse - m_fLastMouseX;
			m_fTimeLineOffset += fDeltaX;
		}

		m_LastMouseTrackFrame = TIME.GetFrameCount();
		m_fLastMouseX = fMouse;
	}
}

void COnScreenTimeline::CalculateInitialZoom(float fRangeStart, float fRangeEnd)
{
	const float xMin = m_screenExtents.GetMinX();
	const float xMax = m_screenExtents.GetMaxX();

	 m_zoomController.CalculateInitialZoom(xMin, xMax, fRangeStart, fRangeEnd);
}

float COnScreenTimeline::GetSelectorVisualPosition()
{
	const float fTimelineRange = GetMaxVisibleTime() - GetMinVisibleTime();
	const float fSelectorCurrentTime = GetCurrentTime();

	const float fSelectorPos = fSelectorCurrentTime - GetMinVisibleTime();
	
	return fSelectorPos / fTimelineRange;
}

void COnScreenTimeline::MoveMarkerToVisibleTimelinePosition(float fPositionOnVisibleTimeline)
{
	const float fTimelineRange = GetMaxVisibleTime() - GetMinVisibleTime();

	// Calculate the time the start of the timeline needs to be in order to put the current time in the centre
	const float fNewTimelineStartTime = GetCurrentTime() - (fTimelineRange * fPositionOnVisibleTimeline);

	// Now work out the offset required to put the new start time there
	m_zoomController.m_fTimeLineOffset = -(fNewTimelineStartTime * m_zoomController.m_fZoom);

	ClampTimelineOffset();
}

float COnScreenTimeline::GetScreenX(float time)
{
	float minTime = GetMinVisibleTime();
	float maxTime = GetMaxVisibleTime();

	float n = (time - minTime) / (maxTime-minTime);

	return Lerp(n, m_screenExtents.GetMinX(), m_screenExtents.GetMaxX());
}

float COnScreenTimeline::GetTimeForScreenX(float x)
{
	float n = (x - m_screenExtents.GetMinX()) / (m_screenExtents.GetMaxX() - m_screenExtents.GetMinX());

	n = Clamp(n, 0.0f, 1.0f);
	return Lerp(n, GetMinVisibleTime(), GetMaxVisibleTime());
}

float COnScreenTimeline::GetMinVisibleTime()
{
	float invZoom = 1.0f/m_zoomController.m_fZoom;

	return -(m_zoomController.m_fTimeLineOffset*invZoom);
}

float COnScreenTimeline::GetMaxVisibleTime()
{
	float invZoom = 1.0f/m_zoomController.m_fZoom;
	float timelineWidth = m_screenExtents.GetMaxX() - m_screenExtents.GetMinX();
	return GetMinVisibleTime()+(timelineWidth*invZoom);
}

// PURPOSE: Set the screen extents of the timeline control
void COnScreenTimeline::SetScreenExtents(const CScreenExtents& screenExtents)
{
	m_screenExtents = screenExtents;
}

bool COnScreenTimeline::IsHoveringSelector()
{
	static float s_selectorTolerance = 5.0f;
	float mouseX = static_cast<float>(ioMouse::GetX());
	float mouseY = static_cast<float>(ioMouse::GetY());
	if (mouseY>=m_screenExtents.GetMinY() && mouseY<=m_screenExtents.GetMaxY())
	{
		if (Abs(mouseX - GetScreenX(GetCurrentTime()))<s_selectorTolerance)
		{
			return true;
		}
	}
	return false;
}

CAnimSceneEditor::CAnimSceneEditor()
	: m_pScene(NULL)
	, m_pickerSettings(NULL)
{
	SetAnimScene(NULL);
	m_widgets.Init();

	// Editor setting defaults
	m_editorSettingTimelinePlaybackMode = kTimelinePlaybackMode_Static;
	m_editorSettingTimelineDisplayMode = kTimelineDisplayMode_Frames;
	m_editorSettingTimelineSnapToFrames = true;
	m_fSelectorTimelinePositionUponPlayback = 0.5f;
	m_editorSettingEventEditorSnapToFrames = true;
	m_editorSettingEditEventPreloads = false;
	m_editorSettingPauseSceneAtFirstStart = true;
	m_editorSettingResetSceneOnPlaybackListChange = true;
	m_editorSettingAutoRegisterPlayer = false;

	m_EventLayersYScreenOffset = 22.0f;
}

// Constructor. Set the extents of the editor on screen
CAnimSceneEditor::CAnimSceneEditor(CAnimScene* pScene, const CScreenExtents& screenExtents)
	: m_pScene(NULL)
	, m_pickerSettings(NULL)
{
	Init(pScene, screenExtents);
}

CAnimSceneEditor::~CAnimSceneEditor()
{
	// cleanup
}


void CAnimSceneEditor::SetEditorAndComponentColors()
{
	SetVideoControlPanelColors(); 
	SetAuthoringHelperColors(); 
	SetAnimSceneEditorColors();
	SetTimelinePanelColors();
}

void CAnimSceneEditor::SetVideoControlPanelColors()
{
	m_videoPlayBackControl.SetColor(COnScreenVideoControl::kVideoControlBackgroundColor, ASE_Panel_BackGroundColor); 
	m_videoPlayBackControl.SetColor(COnScreenVideoControl::kVideoControlDisplayOutlineColor, ASE_Panel_OutlineColor); 
	m_videoPlayBackControl.SetColor(COnScreenVideoControl::kVideoControlButtonColor, ASE_Panel_Button_Color); 
	m_videoPlayBackControl.SetColor(COnScreenVideoControl::kVideoControlHoverButtonColor, ASE_Panel_Button_HoverColor); 
	m_videoPlayBackControl.SetColor(COnScreenVideoControl::kVideoControlActiveButtonColor, ASE_Panel_Button_ActiveColor); 
	m_videoPlayBackControl.SetColor(COnScreenVideoControl::kVideoControlActiveButtonHighLigtColor, ASE_Panel_Button_ActiveHoverColor); 
	m_videoPlayBackControl.SetColor(COnScreenVideoControl::kVideoControlActiveButtonBackgroundColor, ASE_Panel_Button_ActiveBackgroundColor); 
	m_videoPlayBackControl.SetColor(COnScreenVideoControl::kVideoControlPanelButtonOutlineColor, ASE_Panel_Button_ActiveOutlineColor); 
	m_videoPlayBackControl.SetColor(COnScreenVideoControl::kVideoControlPanelActiveButtonOutlineColor, ASE_Panel_Button_OutlineColor); 

}

void CAnimSceneEditor::SetTimelinePanelColors()
{
	m_timeline.SetColor(COnScreenTimeline::kTimelineSelectorColor, ASE_Panel_Button_Color);
	m_timeline.SetColor(COnScreenTimeline::kTimelineSelectorHoverColor, ASE_Panel_Button_HoverColor);
	m_timeline.SetColor(COnScreenTimeline::kTimelineSelectorDraggingColor, ASE_Panel_Button_ActiveOutlineColor);
	m_timeline.SetColor(COnScreenTimeline::kTimelineSelectorTimeTextColor, ASE_Panel_BackGroundColor);
	m_timeline.SetColor(COnScreenTimeline::kTimelineSelectorTimeBackgroundColor, ASE_Panel_Button_HoverColor);
	m_timeline.SetColor(COnScreenTimeline::kTimelineTimeMarkerLabelColor, ASE_Panel_OutlineColor);
	m_timeline.SetColor(COnScreenTimeline::kTimelineOutlineColor, ASE_Panel_OutlineColor);
	m_timeline.SetColor(COnScreenTimeline::kTimelineBackgroundInRangeColor, ASE_Panel_BackGroundColor);
	m_timeline.SetColor(COnScreenTimeline::kTimelineBackgroundOutOfRangeColor, ASE_Panel_BackGroundColorDark);
	m_timeline.SetColor(COnScreenTimeline::kTimelineTimeMarker1FrameColor, ASE_Timeline_Marker1FrameColor);
	m_timeline.SetColor(COnScreenTimeline::kTimelineTimeMarker5FrameColor, ASE_Timeline_Marker5FrameColor);
	m_timeline.SetColor(COnScreenTimeline::kTimelineTimeMarker30FrameColor, ASE_Timeline_Marker30FrameColor);
}


void CAnimSceneEditor::InitVideoControlPanel()
{
	if(m_videoPlayBackControl.GetButton(COnScreenVideoControl::kStepToStart))
	{
		m_videoPlayBackControl.GetButton(COnScreenVideoControl::kStepToStart)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::StepToStartCB)); 
	}

	if(m_videoPlayBackControl.GetButton(COnScreenVideoControl::kStepBackwards))
	{
		m_videoPlayBackControl.GetButton(COnScreenVideoControl::kStepBackwards)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::StepBackwardCB)); 
	}

	if(m_videoPlayBackControl.GetButton(COnScreenVideoControl::kStop))
	{
		m_videoPlayBackControl.GetButton(COnScreenVideoControl::kStop)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::StopCB)); 
	}

	if(m_videoPlayBackControl.GetButton(COnScreenVideoControl::kPlayBackwards))
	{
		m_videoPlayBackControl.GetButton(COnScreenVideoControl::kPlayBackwards)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::PlaySceneBackwardsCB)); 
	}

	if(m_videoPlayBackControl.GetButton(COnScreenVideoControl::kPlayForwards))
	{
		m_videoPlayBackControl.GetButton(COnScreenVideoControl::kPlayForwards)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::PlaySceneForwardsCB)); 
	}

	if(m_videoPlayBackControl.GetButton(COnScreenVideoControl::kStepForwards))
	{
		m_videoPlayBackControl.GetButton(COnScreenVideoControl::kStepForwards)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::StepForwardCB)); 
	}

	if(m_videoPlayBackControl.GetButton(COnScreenVideoControl::kStepToEnd))
	{
		m_videoPlayBackControl.GetButton(COnScreenVideoControl::kStepToEnd)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::StepToEndCB)); 
	}

	SetVideoControlPanelColors(); 
}

void CAnimSceneEditor::SetAuthoringHelperColors()
{
	m_authoringDisplay.SetColor(CAuthoringHelperDisplay::kVideoControlPanelBackgroundColor, ASE_Panel_BackGroundColor); 
	m_authoringDisplay.SetColor(CAuthoringHelperDisplay::kVideoControlPanelOutlineColor, ASE_Panel_OutlineColor); 
	m_authoringDisplay.SetColor(CAuthoringHelperDisplay::kVideoControlButtonColor, ASE_Panel_Button_Color); 
	m_authoringDisplay.SetColor(CAuthoringHelperDisplay::kVideoControlHoverButtonColor, ASE_Panel_Button_HoverColor); 
	m_authoringDisplay.SetColor(CAuthoringHelperDisplay::kVideoControlActiveButtonColor, ASE_Panel_Button_ActiveColor); 
	m_authoringDisplay.SetColor(CAuthoringHelperDisplay::kVideoControlActiveButtonHighLigtColor, ASE_Panel_Button_ActiveHoverColor); 
	m_authoringDisplay.SetColor(CAuthoringHelperDisplay::kVideoControlActiveButtonBackgroundColor, ASE_Panel_Button_ActiveBackgroundColor); 
	m_authoringDisplay.SetColor(CAuthoringHelperDisplay::kVideoControlPanelActiveButtonOutlineColor, ASE_Panel_Button_ActiveOutlineColor); 
	m_authoringDisplay.SetColor(CAuthoringHelperDisplay::kVideoControlPanelButtonOutlineColor, ASE_Panel_Button_OutlineColor); 

}

void CAnimSceneEditor::InitAuthoringHelperPanel()
{
	if(m_authoringDisplay.GetButton(CAuthoringHelperDisplay::kTransGbl))
	{
		m_authoringDisplay.GetButton(CAuthoringHelperDisplay::kTransGbl)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::SetAuthoringHelperTransGbl)); 
	}

	if(m_authoringDisplay.GetButton(CAuthoringHelperDisplay::kTransLcl))
	{
		m_authoringDisplay.GetButton(CAuthoringHelperDisplay::kTransLcl)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::SetAuthoringHelperTransLcl)); 
	}

	if(m_authoringDisplay.GetButton(CAuthoringHelperDisplay::kRotLcl))
	{
		m_authoringDisplay.GetButton(CAuthoringHelperDisplay::kRotLcl)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::SetAuthoringHelperRotLcl)); 
	}

	if(m_authoringDisplay.GetButton(CAuthoringHelperDisplay::kRotGbl))
	{
		m_authoringDisplay.GetButton(CAuthoringHelperDisplay::kRotGbl)->SetButtonCallBack(MakeFunctorRet(*this, &CAnimSceneEditor::SetAuthoringHelperRotGbl)); 
	}

	SetAuthoringHelperColors(); 
}

void CAnimSceneEditor::InitScenePicker()
{
	//if(!g_PickerManager.IsEnabled())
	//{
	//	g_PickerManager.SetEnabled(true);
	//	g_PickerManager.SetUiEnabled(false);
	//}

	//if(!m_pickerSettings)
	//{
	//	m_pickerSettings = rage_new CGtaPickerInterfaceSettings(); 
	//}

	//m_pickerSettings->m_bShowHideEnabled = false; 
	//m_pickerSettings->m_ShowHideMode = PICKER_SHOW_ALL; 

	//g_PickerManager.SetPickerInterfaceSettings(m_pickerSettings);

	//g_PickerManager.TakeControlOfPickerWidget("Anim Scenes"); 
	//g_PickerManager.ResetList(true); 
}

void CAnimSceneEditor::Init(CAnimScene* pScene, const CScreenExtents& screenExtents)
{
	//InitColours(); 
	SetAnimScene(pScene);
	CheckAnimSceneVersion();
	SetScreenExtents(screenExtents);

	m_pickerSettings = NULL; 

	m_timeline.SetTimelineSelectorDraggedCallback(MakeFunctor(*this, &CAnimSceneEditor::TimelineSelectorDragged));
	m_timeline.CalculateInitialZoom(0.0f, pScene ? pScene->GetDuration() : 0.0f);
	
	InitVideoControlPanel();
	InitAuthoringHelperPanel(); 
	SetAnimSceneEditorColors(); 
	SetTimelinePanelColors();

	m_widgets.Init();

	InitScenePicker();

	InitEntityInitialisers();
	InitEventInitialisers();
	InitEventScrollbar();
	m_firstPlayback = true;
}

void CAnimSceneEditor::Widgets::Init()
{
	m_pSelectionGroup = NULL;
	m_pAddEntityGroup = NULL;
	m_pSaveStatus = NULL;
	m_pSaveButton = NULL;
	m_pCloseButton = NULL;
	m_pEntityTypeCombo = NULL;
	m_selectedEntityType=0;
	m_newEntityId.SetHash(0);
// 	m_boolValue = false;
	m_pEventTypeCombo = NULL;
	m_selectedEventType = 0;
	m_pAddEventGroup = NULL;
	m_pAddSectionGroup = NULL;
	m_pSceneGroup = NULL;
/*	m_useEntityClipSelector = false;*/
}

void CAnimSceneEditor::Shutdown()
{
	CleanupEventInitialiserWidgets();
	CleanupEntityInitialiserWidgets();
	if(m_pScene)
	{
		m_pScene->ShutdownWidgets();
	}
	m_widgets.Shutdown();
	m_selected.SelectEntity(NULL);
	m_selected.SelectEvent(NULL);
	if (m_pScene)
		CAnimSceneManager::BankDestroyAnimScene(m_pScene);
	SetAnimScene(NULL);
	m_flags.ClearAllFlags();

	if(m_pickerSettings)
	{
		delete m_pickerSettings; 

		m_pickerSettings = NULL; 
	}

	for (s32 i=0; i<m_eventInitialisers.GetCount(); i++)
	{
		delete m_eventInitialisers[i];
		m_eventInitialisers[i] = NULL;
	}
	m_eventInitialisers.clear();

	for (s32 i=0; i<m_entityInitialisers.GetCount(); i++)
	{
		delete m_entityInitialisers[i];
		m_entityInitialisers[i] = NULL;
	}
	m_entityInitialisers.clear();
	m_firstPlayback = true;
}

void CAnimSceneEditor::Widgets::Shutdown()
{
	//////////////////////////////////////////////////////////////////////////
	// Shut down the Selected group
	//////////////////////////////////////////////////////////////////////////
	m_playbackListLeadInClipSetSelector.RemoveWidgets();

	if (m_pSelectionGroup)
	{
		m_pSelectionGroup->Destroy();
	}
	m_pSelectionGroup = NULL;


	//////////////////////////////////////////////////////////////////////////
	// Shut down the add event group (widgets first)
	//////////////////////////////////////////////////////////////////////////

	m_addEventSectionSelector.SetAnimScene(NULL);
	m_addEventSectionSelector.RemoveWidgets();

	if (m_pEventTypeCombo)
	{
		m_pEventTypeCombo->Destroy();
	}
	m_pEventTypeCombo = NULL;

	if (m_pAddEventGroup)
	{
		m_pAddEventGroup->Destroy();
	}
	m_pAddEventGroup = NULL;

	m_selectedEventType = 0;
	m_eventNameList.clear();

	if (m_pAddSectionGroup)
	{
		m_pAddSectionGroup->Destroy();
	}
	m_pAddSectionGroup = NULL;

	//////////////////////////////////////////////////////////////////////////
	// Shut down the add entity group (widgets first)
	//////////////////////////////////////////////////////////////////////////

	if (m_pEntityTypeCombo)
	{
		m_pEntityTypeCombo->Destroy();
	}
	m_pEntityTypeCombo = NULL;

	if (m_pAddEntityGroup)
	{
		m_pAddEntityGroup->Destroy();
	}
	m_pAddEntityGroup = NULL;

	m_selectedEntityType=0;
	m_entityNameList.clear();

	//////////////////////////////////////////////////////////////////////////
	// Remove the core widgets
	//////////////////////////////////////////////////////////////////////////

	if (m_pSceneGroup)
	{
		m_pSceneGroup->Destroy();
	}
	m_pSceneGroup = NULL;

	if (m_pSaveStatus)
	{
		m_pSaveStatus->Destroy();
	}
	m_pSaveStatus = NULL;

	if (m_pSaveButton)
	{
		m_pSaveButton->Destroy();
	}
	m_pSaveButton = NULL;

	if (m_pCloseButton)
	{
		m_pCloseButton->Destroy();
	}
	m_pCloseButton = NULL;
}

void CAnimSceneEditor::SyncPickerFromAnimSceneEntity()
{
	CAnimSceneEntity* AnimSceneEntity = GetSelectedEntity();

	if(AnimSceneEntity)
	{
		 CEntity* pEnt = m_pScene->GetPhysicalEntity(AnimSceneEntity->GetId(), false); ; 
		 if(AnimSceneEntity->GetType() == ANIM_SCENE_ENTITY_PED )
		 {
			 pEnt = static_cast<CAnimScenePed*>(AnimSceneEntity)->GetPed(false); 
		 }
		 else if(AnimSceneEntity->GetType() == ANIM_SCENE_ENTITY_VEHICLE )
		 {
			 pEnt = static_cast<CAnimSceneVehicle*>(AnimSceneEntity)->GetVehicle(false); 
		 }
		 else if (AnimSceneEntity->GetType() == ANIM_SCENE_ENTITY_OBJECT )
		 {
			 pEnt = static_cast<CAnimSceneObject*>(AnimSceneEntity)->GetObject(false); 
		 }

		g_PickerManager.AddEntityToPickerResults(pEnt, true, true);
	}
};

void CAnimSceneEditor::RenderSelectedAnimScenesPossibleNewGameEntity()
{
	CEntity* pickerEntity = static_cast<CEntity*>(g_PickerManager.GetEntity(0));
	if(pickerEntity)
	{
		if(GetSelectedEntity())
		{
			CEntity* pEnt = m_pScene->GetPhysicalEntity(GetSelectedEntity()->GetId(), false); 

			if(pEnt && pEnt != pickerEntity)
			{
				if(pickerEntity->GetIsTypePed() == pEnt->GetIsTypePed()
					|| pickerEntity->GetIsTypeObject() == pEnt->GetIsTypeObject()
					|| pickerEntity->GetIsTypeVehicle() == pEnt->GetIsTypeVehicle())
				{
					char name[128]; 
					formatf(name, "register to %s", GetSelectedEntity()->GetId().GetCStr()); 
					grcDebugDraw::Text(pickerEntity->GetTransform().GetPosition(), GetColor(kSelectedTextColor), name, true); 
					return; 
				}
			}
		}
	}
}

void CAnimSceneEditor::OnGameEntityRegister()
{
	if(m_pScene)
	{
		m_pScene->Stop(); 
		m_pScene->Reset(); 
	}
}

bool CAnimSceneEditor::PlaySceneForwardsCB(u32 buttonState) 
{
	if(m_pScene)
	{
		float rate = m_pScene->GetRate(); 
		
		if(buttonState == COnScreenButton::kButtonPressed)
		{
			if( m_pScene->IsFinished())
			{
				m_pScene->Reset();
			}
			else if ( m_pScene->GetDuration() <= m_pScene->GetTimeWithSkipping() )
			{
				m_pScene->Skip(0.f);
			}
			m_pScene->SetPaused(false); 

			if(rate < 0.0f)
			{
				m_pScene->SetRate(rate * -1.0f);
			}

			// Remember the selector position when we play
			m_fSelectorTimelinePositionUponPlayback = m_timeline.GetSelectorVisualPosition();

			return true; 
		}
		
		if(buttonState == COnScreenButton::kButtonReleased)
		{
			m_pScene->SetPaused(true);
			return false; 
		}

		if(buttonState == COnScreenButton::kButtonHeld)
		{
			if(!m_pScene->IsPlayingBack() || m_pScene->GetPaused() || rate < 0.0f)
			{
				return false;
			}

			if(m_pScene->GetDuration() <= m_pScene->GetTimeWithSkipping())
			{
				return false;
			}

			return true;
		}
	}
	
	return false; 
};

bool CAnimSceneEditor::PlaySceneBackwardsCB(u32 buttonState) 
{
	if(m_pScene)
	{
		float rate = m_pScene->GetRate(); 

		if(buttonState == COnScreenButton::kButtonPressed)
		{
			m_pScene->SetPaused(false); 
			
			if(rate > 0.0f)
			{
				m_pScene->SetRate(rate * -1.0f);
			}

			// Remember the selector position when we play
			m_fSelectorTimelinePositionUponPlayback = m_timeline.GetSelectorVisualPosition();

			return true; 
		}

		if(buttonState == COnScreenButton::kButtonReleased)
		{
			if(!m_pScene->GetPaused())
			{
				m_pScene->SetPaused(true);
				
			}
			return false; 
		}

		if(buttonState == COnScreenButton::kButtonHeld)
		{
			if(!m_pScene->IsPlayingBack() || m_pScene->GetPaused() || rate > 0.0f)
			{
				return false;
			}

			return true;
		}
	}

	return false; 
};

bool CAnimSceneEditor::StepForwardCB(u32 buttonState) 
{
	const float SINGLE_FRAME_DURATION = (1.0f / 30.0f);

	if(m_pScene)
	{
		if(buttonState == COnScreenButton::kButtonPressed)
		{
			/*
			if( m_pScene->IsFinished() || (m_pScene->GetDuration() <= m_pScene->GetTimeWithSkipping()) )
			{
				m_pScene->Reset();
				m_pScene->SetPaused(true);
				return false;
			}*/

			m_pScene->SetPaused(true); 
			float newTime = m_pScene->GetTime();

			if (m_editorSettingTimelineSnapToFrames)
			{
				int iCurrentFrame = (int)(newTime / SINGLE_FRAME_DURATION);
				newTime = (float)(iCurrentFrame + 1) * SINGLE_FRAME_DURATION;
			}
			else
			{
				newTime += SINGLE_FRAME_DURATION;
			}

			m_pScene->Skip(newTime);
		}
	}
	else
	{
		return false; 
	}

	return false; 

};

bool CAnimSceneEditor::StepBackwardCB(u32 buttonState) 
{
	const float SINGLE_FRAME_DURATION = (1.0f / 30.0f);

	if(m_pScene)
	{
		if(buttonState == COnScreenButton::kButtonPressed)
		{
			m_pScene->SetPaused(true); 
			float newTime = m_pScene->GetTime();
			
			if (m_editorSettingTimelineSnapToFrames)
			{
				int iCurrentFrame = (int)(newTime / SINGLE_FRAME_DURATION);
				float fCurrentFrameTime = (float)(iCurrentFrame) * SINGLE_FRAME_DURATION;

				if (newTime - fCurrentFrameTime < 0.001f)
				{
					newTime = (float)(iCurrentFrame - 1) * SINGLE_FRAME_DURATION;
				}
				else
				{
					newTime = (float)(iCurrentFrame) * SINGLE_FRAME_DURATION;
				}
			}
			else
			{
				newTime -= SINGLE_FRAME_DURATION;
			}

			m_pScene->Skip(newTime);
		}
	}
	else
	{
		return false; 
	}

	return false; 

};

bool CAnimSceneEditor::StepToStartCB(u32 buttonState) 
{
	if(m_pScene)
	{
		if(buttonState == COnScreenButton::kButtonPressed)
		{
			if(m_pScene->IsFinished())
			{
				m_pScene->Reset();
				m_pScene->SetPaused(true);
				return false;
			}
			else
			{
				m_pScene->SetPaused(true); 
				m_pScene->Skip(0.0f); 
				return false;
			} 
		}
	}
	else
	{
		return false; 
	}

	return false; 

};

bool CAnimSceneEditor::StepToEndCB(u32 buttonState) 
{
	if(m_pScene)
	{
		if(buttonState == COnScreenButton::kButtonPressed)
		{
			m_pScene->SetPaused(true); 
			m_pScene->Skip(m_pScene->GetDuration()); 
		}
	}
	else
	{
		return false; 
	}

	return false; 

};

bool CAnimSceneEditor::StopCB(u32 buttonState) 
{
	if(m_pScene)
	{
		if(buttonState == COnScreenButton::kButtonPressed)
		{
			m_pScene->SetPaused(true); 
		}
	}
	else
	{
		return false; 
	}

	return false; 

};

bool CAnimSceneEditor::SetAuthoringHelperTransGbl(u32 buttonState)
{
	if(buttonState == COnScreenButton::kButtonPressed)
	{
		CAuthoringHelper::SetAuthoringMode(CAuthoringHelper::TRANS_GBL); 
		return true; 
	}

	if(buttonState == COnScreenButton::kButtonReleased)
	{
		CAuthoringHelper::SetAuthoringMode(CAuthoringHelper::NO_AUTHOR_MODE); 
		return false; 
	}

	if(CAuthoringHelper::GetCurrentAuthoringMode() == CAuthoringHelper::TRANS_GBL && buttonState == COnScreenButton::kButtonHeld)
	{
		return true; 
	}
	
	return false; 
}

bool CAnimSceneEditor::SetAuthoringHelperTransLcl(u32 buttonState)
{
	if(buttonState == COnScreenButton::kButtonPressed)
	{
		CAuthoringHelper::SetAuthoringMode(CAuthoringHelper::TRANS_LCL); 
		return true; 
	}

	if(buttonState == COnScreenButton::kButtonReleased)
	{
		CAuthoringHelper::SetAuthoringMode(CAuthoringHelper::NO_AUTHOR_MODE); 
		return false; 
	}

	if(CAuthoringHelper::GetCurrentAuthoringMode() == CAuthoringHelper::TRANS_LCL && buttonState == COnScreenButton::kButtonHeld)
	{
		return true; 
	}

	return false; 
}

bool CAnimSceneEditor::SetAuthoringHelperRotGbl(u32 buttonState)
{
	if(buttonState == COnScreenButton::kButtonPressed)
	{
		CAuthoringHelper::SetAuthoringMode(CAuthoringHelper::ROT_GBL); 
		return true; 
	}

	if(buttonState == COnScreenButton::kButtonReleased)
	{
		CAuthoringHelper::SetAuthoringMode(CAuthoringHelper::NO_AUTHOR_MODE); 
		return false; 
	}

	if(CAuthoringHelper::GetCurrentAuthoringMode() == CAuthoringHelper::ROT_GBL && buttonState == COnScreenButton::kButtonHeld)
	{
		return true; 
	}
	return false; 
}

bool CAnimSceneEditor::SetAuthoringHelperRotLcl(u32 buttonState)
{
	if(buttonState == COnScreenButton::kButtonPressed)
	{
		CAuthoringHelper::SetAuthoringMode(CAuthoringHelper::ROT_LCL); 
		return true; 
	}

	if(buttonState == COnScreenButton::kButtonReleased)
	{
		CAuthoringHelper::SetAuthoringMode(CAuthoringHelper::NO_AUTHOR_MODE); 
		return false; 
	}

	if(CAuthoringHelper::GetCurrentAuthoringMode() == CAuthoringHelper::ROT_LCL && buttonState == COnScreenButton::kButtonHeld)
	{
		return true; 
	}
	return false; 
}


bool CAnimSceneEditor::IsSelectingADifferentGameEntity()
{
	CEntity* hoverEnt = static_cast<CEntity*>(g_PickerManager.GetHoverEntity()); 

	if(hoverEnt)
	{
		//set the current anim scene physical entity
		for(int i=0; i < m_pScene->GetNumEntities(); i++)
		{
			CAnimSceneEntity* localAnimSceneEntity = m_pScene->GetEntity(i); 

			if(localAnimSceneEntity->GetId())
			{
				CEntity* pEnt = m_pScene->GetPhysicalEntity(localAnimSceneEntity->GetId(), false); 

				if(pEnt && pEnt == hoverEnt)
				{
					return true; 
				}
			}
		}
		
		
		if(GetSelectedEntity())
		{
			CEntity* pEnt = m_pScene->GetPhysicalEntity(GetSelectedEntity()->GetId(), false);

			if(hoverEnt && pEnt)
			{
				if(pEnt != hoverEnt)
				{
					if(pEnt->GetIsTypePed() && hoverEnt->GetIsTypePed())
					{
						return true;  
					}
					else if (pEnt->GetIsTypeVehicle() && hoverEnt->GetIsTypeVehicle())
					{
						return true;  
					}
					else if (pEnt->GetIsTypeObject() && hoverEnt->GetIsTypeObject())
					{
						return true;  
					}
				}
			}
		}
	}

	return false; 
}

void CAnimSceneEditor::SyncAnimSceneEntityFromPicker()
{
	if(GetScene() && ((ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT) || (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)))
	{
		if(g_PickerManager.IsEnabled())
		{
			CEntity* pickerEnt = static_cast<CEntity*>(g_PickerManager.GetEntity(0)); 
			
			//set the current anim scene physical entity
			for(int i=0; i < m_pScene->GetNumEntities(); i++)
			{
				CAnimSceneEntity* localAnimSceneEntity = m_pScene->GetEntity(i); 

				if(localAnimSceneEntity->GetId())
				{
					CEntity* pEnt = m_pScene->GetPhysicalEntity(localAnimSceneEntity->GetId(), false); 

					if(pEnt && pEnt == pickerEnt)
					{
						SelectEntity(localAnimSceneEntity); 
					}
				}
			}
		}
	}
}

// PURPOSE: Call once per frame to update the editor
void CAnimSceneEditor::Process_RenderThread()
{
	if (m_pScene && m_pScene->IsLoaded())
	{
		m_mouseState.Update();

		//Switch to a 2d display mode
		grcViewport* prevViewport=grcViewport::GetCurrent();
		grcViewport screenViewport;

		grcViewport::SetCurrent(&screenViewport);
		grcViewport::GetCurrent()->Screen();

		grcStateBlock::SetBlendState(grcStateBlock::BS_CompositeAlpha);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);

		// Render the selected entity
		// TODO: This will need fixing up once we have multiple selection of entities.
		if (m_selected.GetEntity())
		{
			CPhysical* pPhysEnt = m_pScene->GetPhysicalEntity(m_selected.GetEntity()->GetId(), false);
			if (pPhysEnt)
			{
				CDebugScene::DrawEntityBoundingBox(pPhysEnt, ASE_EntityPickerBoundingBoxColor);
			}
		}
		
		// Render the version number (top-left)
		grcLighting(false);
		atVarString versionNumber("ANIM SCENE EDITOR (BETA) v%d.%d - %s (ASV%d)", 
			ASE_MAJOR_VERSION, ASE_MINOR_VERSION, m_pScene->GetName().TryGetCStr(), m_pScene->GetVersion());
		RenderTextWithBackground(versionNumber.c_str(), m_screenExtents.GetMinX(), m_screenExtents.GetMinY() + 5.0f, ASE_VersionColor,
			ASE_Panel_BackGroundColor, 3.0f, true, ASE_Panel_OutlineColor);

		m_sectionPanel.Render(this);

		if (m_flags.IsFlagSet(kDraggingEvent))
		{
			m_timeline.DisableUserControl();
		}
		else
		{
			m_timeline.EnableUserControl();
		}

		// keep the selectable range on the timeline synced to the scene duration
		m_timeline.SetSelectableRange(0.0f, m_pScene->GetDuration());

		// render the timeline
		m_timeline.Process();

		// render the scene information display
		m_sceneInfoDisplay.Process(m_timeline.GetCurrentTime(), m_pScene->GetDuration());

		// Event pointers in the event layers need to be reset every frame
		InitEventLayers();

		// render the events (also fills out the event layers)
		m_pScene->SceneEditorRender(*this);

		// remove any layers that contain no events
		CullEmptyEventLayers();

		// render the event layers
		if(!m_editorSettingHideEvents)
		{
			m_EventLayersWindow.Render(this); 
			// Render the events scroll bar
			RenderEventsScrollbar();
		}		

		// render the scene playback controls
		m_videoPlayBackControl.Process(); 

		m_authoringDisplay.Process(); 

		grcViewport::SetCurrent(prevViewport);
	}
}

void CAnimSceneEditor::InitEventLayers()
{
	// make sure there's a layer for each entity
	for (s32 i=0; i<m_pScene->GetNumEntities(); i++)
	{
		const CAnimSceneEntity* pEnt = m_pScene->GetEntity(i);

		if (!pEnt)
		{
			return;
		}

		AddEventLayerIfMissing(pEnt->GetId());
	}

	AddEventLayerIfMissing(ANIM_SCENE_ENTITY_ID_INVALID);
	
	// clear out the stored event pointers before we update
	for (s32 i=0 ; i<m_EventLayersWindow.GetEventLayers().GetCount(); i++)
	{
		if(m_EventLayersWindow.GetEventLayers()[i])
		{
			for (s32 j=0; j< m_EventLayersWindow.GetEventLayers()[i]->GetEventGroup().GetCount(); j++)
			{
				CEventPanelGroup* pGroup = m_EventLayersWindow.GetEventPanelGroup((u32)i,(u32)j); 
				if(pGroup)
				{
					pGroup->RemoveAllEvents(); 
				}
			}
		}
	}
}

void CAnimSceneEditor::AddEventLayerIfMissing(atHashString name)
{
	bool layerExists = false;

	for (s32 j=0 ; j<m_EventLayersWindow.GetEventLayers().GetCount(); j++)
	{
		if (m_EventLayersWindow.GetEventLayers()[j]->GetName()==name)
		{
			layerExists = true;
			break;
		}
	}

	if (!layerExists)
	{
		m_EventLayersWindow.AddEventLayerPanel(name); 
	}
}

void CAnimSceneEditor::CullEmptyEventLayers()
{
	s32 i = m_EventLayersWindow.GetEventLayers().GetCount()-1;
	while (i>-1)
	{
		s32 j=m_EventLayersWindow.GetEventLayers()[i]->GetEventGroup().GetCount()-1;
		while (j>-1)
		{
			CEventPanelGroup* pGroup = m_EventLayersWindow.GetEventPanelGroup((u32)i,(u32)j); 
			if(pGroup)
			{
				if(pGroup->GetEvents().GetCount() == 0)
				{
					m_EventLayersWindow.GetEventLayers()[i]->RemoveEventGroup(j); 
				}
			}
			//if (m_eventLayers[i].m_subGroups[j].m_events.GetCount()==0)
			//{
			//	// cull the sub group
			//	m_eventLayers[i].m_subGroups.Delete(j);
			//}
			j--;
		}



		if (m_EventLayersWindow.GetEventLayers()[i]->GetEventGroup().GetCount()==0 && !m_pScene->GetEntity(m_EventLayersWindow.GetEventLayers()[i]->GetName()))
		{
			m_EventLayersWindow.RemoveEventLayerPanel(i);
		}

		i--;
	}
}

//void CAnimSceneEditor::RenderEventLayers()
//{
//	TUNE_FLOAT(layerHeight, 15.0f, 0.0f, 100.0f, 1.0f)
//	EventLayer::ms_layerHeight = layerHeight; 
//
//	float height = EventLayer::ms_layerHeight;
//
//	//static float s_subGroupIndent = 100.0f;
//	m_flags.ClearFlag(kMadeEntitySelectionThisFrame); 
//
//	for (s32 i=0 ; i<GetEventLayers().GetCount(); i++)
//	{
//		CEventLayerPanel& layer = *m_EventLayersWindow.GetEventLayers()[i];
//
//		bool isSelected = (layer.GetName().GetHash()==GetSelectedEntityId().GetHash());
//
//		// render the main layer string
//		
//		float mouseX = static_cast<float>(ioMouse::GetX());
//		float mouseY = static_cast<float>(ioMouse::GetY());
//		bool isHovering = false; 
//
//		if (mouseX>=m_EventLayersWindow.GetScreenExtents().GetMinX(), m_EventLayersWindow.GetScreenExtents().GetMinY(), timelineMinX, m_timeline.GetScreenExtents().GetMinY().GetMinX() && mouseX<=(m_EventLayersWindow.GetScreenExtents().GetMinX()+grcFont::GetCurrent().GetStringWidth(layer.GetName().GetCStr(), (s32)strlen(layer.GetName().GetCStr()))))
//		{
//			if (mouseY>=m_EventLayersWindow.GetScreenExtents().GetMinY()+height && mouseY<=(m_EventLayersWindow.GetScreenExtents().GetMinY()+height+grcFont::GetCurrent().GetHeight()))
//			{
//				if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT && !IsDraggingEvent() && !m_timeline.IsDragging())
//				{
//					SelectEntity(m_pScene->GetEntity(layer.GetName().GetCStr()));
//					m_flags.SetFlag(kMadeEntitySelectionThisFrame); 
//				}
//				else
//				{
//					isHovering = true; 
//				}
//			}
//		}
//
//		if(isSelected)
//		{
//			RenderTextWithBackground(layer.GetName().GetCStr(), m_EventLayersWindow.GetScreenExtents().GetMinX(), m_EventLayersWindow.GetScreenExtents().GetMinY()+height, 
//				GetColor(isHovering ? kSelectedTextHoverColor : kSelectedTextColor), 
//				GetColor(isSelected ? kSelectedTextBackGroundColor :kTextBackgroundColor), 3.0f, true, 
//				GetColor(isSelected ? kSelectedTextOutlineColor :kTextOutlineColor));
//		}
//		else
//		{
//			RenderTextWithBackground(layer.GetName().GetCStr(), m_EventLayersWindow.GetScreenExtents().GetMinX(), m_EventLayersWindow.GetScreenExtents().GetMinY()+height, 
//				GetColor(isHovering ? kTextHoverColor : kMainTextColor), 
//				GetColor(isSelected ? kSelectedTextBackGroundColor :kTextBackgroundColor), 3.0f, true, 
//				GetColor(isSelected ? kSelectedTextOutlineColor :kTextOutlineColor));
//		}
//
//		for (s32 j=0; j<layer.GetEventGroup().GetCount(); j++)
//		{
//			for(s32 k=0; k< layer.GetEventGroup()[j]->GetEvents().GetCount(); k++)
//			{
//				RenderEvent(layer.GetEventGroup()[j]->GetEvents()[k]->GetEventInfo().event, m_EventLayersWindow.GetScreenExtents().GetMinY()+height); 
//			}
//			
//			//RenderTextWithBackground(layer.m_subGroups[j].m_name.GetCStr(), m_entityPaneExtents.GetMinX()+s_subGroupIndent, m_entityPaneExtents.GetMinY()+height, textColour, 
//			//	GetColor(kTextBackgroundColor), 3.0f,  false, GetColor(isSelected ? kSelectedTextOutlineColor : kTextOutlineColor));
//			height+=EventLayer::ms_layerHeight;
//		}
//
//		// make sure we always move down at least one row
//		if (layer.GetEventGroup().GetCount()==0)
//			height+=EventLayer::ms_layerHeight;
//	}
//}

void CAnimSceneEditor::Process_MainThread()
{
	SyncAnimSceneEntityFromPicker(); 	

	if (m_flags.IsFlagSet(kCloseCurrentScene))
	{
		Shutdown();
		return;
	}

	// sync up the playback list if necessary
	if (GetScene() && GetScene()->GetPlayBackList().GetHash() != m_sectionPanel.GetSelectedPlaybackList().GetHash())
	{
		if (m_sectionPanel.SelectedListChangedThisFrame())
		{
			GetScene()->SetPlayBackList(m_sectionPanel.GetSelectedPlaybackList());

			// This allows us to mimic how script changes playback lists (i.e. no reset on the entire scene).
			if (m_editorSettingResetSceneOnPlaybackListChange)
			{
				GetScene()->Reset();
				GetScene()->SetPaused(true);
			}
			else
			{
				// The playback list has changed, but without a reset the timeline won't adjust to the new range.  So force that here.
				m_timeline.CalculateInitialZoom(0.0f, m_pScene->GetDuration());
			}
			
			m_widgets.m_addEventSectionSelector.Refresh(); // refresh the section selector to keep it in sync with the current list
			
			if (GetSelectedEntity() && GetSelectedEntity()->GetType()==ANIM_SCENE_ENTITY_PED)
			{
				// if we have a selected entity, refresh the lead in clip set drop down to display the correct name
				fwMvClipSetId leadInClipSetId(CLIP_SET_ID_INVALID);
				CAnimScenePed* pPedEnt = static_cast<CAnimScenePed*>(GetSelectedEntity());
				CAnimSceneLeadInData* pData = pPedEnt->FindLeadIn(m_sectionPanel.GetSelectedPlaybackList());
				if (pData)
				{
					leadInClipSetId = pData->m_clipSet.GetId();
				}

				if (leadInClipSetId==CLIP_SET_ID_INVALID)
				{
					m_widgets.m_playbackListLeadInClipSetSelector.SelectNone();
				}
				else
				{
					m_widgets.m_playbackListLeadInClipSetSelector.Select(leadInClipSetId.GetCStr(), false);
				}
			}
						
			m_sectionPanel.GetFlags().ClearFlag(CSectioningPanel::kSelectedListChanged);	
		}
		else
		{
			m_sectionPanel.SetSelectedPlaybackList(GetScene()->GetPlayBackList());
		}		
	}

// 	// render the debug for the selected entity lead in
// 	if (GetScene() && GetSelectedEntity() && GetSelectedEntity()->GetType()==ANIM_SCENE_ENTITY_PED)
// 	{
// 		fwMvClipSetId leadInClipSetId(CLIP_SET_ID_INVALID);
// 		CAnimScenePed* pPedEnt = static_cast<CAnimScenePed*>(GetSelectedEntity());
// 		CAnimSceneLeadInData* pData = pPedEnt->FindLeadIn(m_sectionPanel.GetSelectedPlaybackList());
// 		if (pData)
// 		{
// 			leadInClipSetId = pData->m_clipSet.GetId();
// 			CMovementTransitionHelper::DebugRenderClipSet(fwClipSetManager::GetClipSet(leadInClipSetId), leadInClipSetId, m_pScene->GetSceneOrigin());
// 		}
// 	}
	
	// Handle dragging and resizing of events
	float mouseX = m_mouseState.m_mouseCoords.x;
	bool leftMouseDown = m_mouseState.m_leftMouseDown;
	bool leftMousePressed = m_mouseState.m_leftMousePressed;
	bool leftMouseReleased = m_mouseState.m_leftMouseReleased;

	if (m_flags.IsFlagSet(kDraggingEvent) && m_selected.GetEvent())
	{
		if (leftMouseDown)
		{
			// move the selected event by the mouse delta
			float dt = (mouseX - m_mouseState.m_lastMouseCoords.x) * m_timeline.GetScale();
			CAnimSceneEvent* pEvt = m_selected.GetEvent();
			float length = pEvt->GetEnd() - pEvt->GetStart();
			pEvt->SetStart(Clamp(pEvt->GetStart()+dt, 0.0f, m_timeline.GetMaxVisibleTime()));
			pEvt->SetEnd(pEvt->GetStart()+length);
		}
		else
		{
			// Finished dragging event.  Snap it to a frame boundary.
			if (m_editorSettingEventEditorSnapToFrames && m_selected.GetEvent())
			{
				CAnimSceneEvent* pEvt = m_selected.GetEvent();
				float length = pEvt->GetEnd() - pEvt->GetStart();

				int iFrameNumber = (int)(pEvt->GetStart() / (1.0f/30.0f));
				pEvt->SetStart((float)iFrameNumber * (1.0f/30.0f));
				pEvt->SetEnd(pEvt->GetStart()+length);
			}

			m_flags.ClearFlag(kDraggingEvent);
		}
	}
	else if (m_flags.IsFlagSet(kDraggingEventStart) && m_selected.GetEvent())
	{
		if (leftMouseDown)
		{
			// move the selected event start by the mouse delta
			float dt = (mouseX - m_mouseState.m_lastMouseCoords.x) * m_timeline.GetScale();
			CAnimSceneEvent* pEvt = m_selected.GetEvent();
			pEvt->SetStart(Clamp(pEvt->GetStart()+dt, Max(0.0f, m_timeline.GetMinVisibleTime()), Min(m_timeline.GetMaxVisibleTime(), pEvt->GetEnd())));
		}
		else
		{
			// Finished dragging event.  Snap it to a frame boundary.
			if (m_editorSettingEventEditorSnapToFrames && m_selected.GetEvent())
			{
				CAnimSceneEvent* pEvt = m_selected.GetEvent();

				int iFrameNumber = (int)(pEvt->GetStart() / (1.0f/30.0f));
				pEvt->SetStart((float)iFrameNumber * (1.0f/30.0f));
			}

			m_flags.ClearFlag(kDraggingEventStart);
		}
	}
	else if (m_flags.IsFlagSet(kDraggingEventEnd) && m_selected.GetEvent())
	{
		if (leftMouseDown)
		{
			// move the selected event end by the mouse delta
			float dt = (mouseX - m_mouseState.m_lastMouseCoords.x) * m_timeline.GetScale();
			CAnimSceneEvent* pEvt = m_selected.GetEvent();
			pEvt->SetEnd(Clamp(pEvt->GetEnd()+dt, Max(pEvt->GetStart(), m_timeline.GetMinVisibleTime()), m_timeline.GetMaxVisibleTime()));
		}
		else
		{
			// Finished dragging event.  Snap it to a frame boundary.
			if (m_editorSettingEventEditorSnapToFrames && m_selected.GetEvent())
			{
				CAnimSceneEvent* pEvt = m_selected.GetEvent();

				int iFrameNumber = (int)(pEvt->GetEnd() / (1.0f/30.0f));
				pEvt->SetEnd((float)iFrameNumber * (1.0f/30.0f));
			}

			m_flags.ClearFlag(kDraggingEventEnd);
		}
	}
	else if (m_flags.IsFlagSet(kDraggingEventPreLoad) && m_selected.GetEvent())
	{
		if (leftMouseDown)
		{
			// move the selected event by the mouse delta
			float dt = -((mouseX - m_mouseState.m_lastMouseCoords.x) * m_timeline.GetScale());
			CAnimSceneEvent* pEvt = m_selected.GetEvent();
			pEvt->SetPreloadDuration(Clamp(pEvt->GetPreloadDuration()+dt, 0.0f, pEvt->GetStart() - Max(0.0f, m_timeline.GetMinVisibleTime())));
		}
		else
		{
			m_flags.ClearFlag(kDraggingEventPreLoad);
		}
	}
	else
	{
		// left click with nothing highlighted
		if (leftMousePressed && !m_flags.IsFlagSet(kMadeEventSelectionThisClick))
		{
			// deselect the currently selected event
			SelectEvent(NULL);
		}
	}

	if (!leftMouseDown && leftMouseReleased)
	{
		m_flags.ClearFlag(kMadeEventSelectionThisClick);
		//Ensure that dragging is disabled
		m_flags.ClearFlag(kDraggingEvent | kDraggingEventEnd | kDraggingEventPreLoad | kDraggingEventStart);
	}

	// keep the scene duration in sync with the events
	if (GetSelectedEvent() && IsDraggingEvent())
	{
		GetSelectedEvent()->NotifyRangeUpdated();
	}

	if(m_flags.IsFlagSet(kRegenerateEventTypeWidgets))
	{
		if(m_bAllowWidgetAutoRegen)
		{
			RegenEventTypeWidgets();
		}
		m_flags.ClearFlag(kRegenerateEventTypeWidgets);
	}

	if (m_flags.IsFlagSet(kRegenerateNewEventWidgets))
	{
		bkBank* pBank = CDebugClipSelector::FindBank(m_widgets.m_pAddEventGroup);
		if (pBank)
		{
			AddNewEventWidgetsForEntity(GetSelectedEntity(), m_widgets.m_pAddEventGroup);
		}
		m_flags.ClearFlag(kRegenerateNewEventWidgets);
	}

	if (m_flags.IsFlagSet(kRegenerateSelectionWidgets))
	{
		// regen the editing widgets
		if (m_widgets.m_pSelectionGroup)
		{
			if (GetScene())
			{
				GetScene()->DoWidgetShutdownCallbacks();
			}

			m_widgets.m_playbackListLeadInClipSetSelector.RemoveWidgets();
			ClearSelectionWidgets();
			// if an event is selected, show the widgets for that, otherwise, if only an entity is selected, show its widgets
			bkBank* pBank = CDebugClipSelector::FindBank(m_widgets.m_pSelectionGroup);
			if (GetSelectedEvent() && pBank)
			{
				pBank->SetCurrentGroup(*m_widgets.m_pSelectionGroup);
				CAnimScene::SetCurrentAddWidgetsScene(m_pScene);

				PARSER.AddWidgets(*pBank, GetSelectedEvent());

				CAnimScene::SetCurrentAddWidgetsScene(NULL);
				pBank->UnSetCurrentGroup(*m_widgets.m_pSelectionGroup);
			}
			else if (GetSelectedEntity() && pBank)
			{
				pBank->SetCurrentGroup(*m_widgets.m_pSelectionGroup);
				CAnimScene::SetCurrentAddWidgetsScene(m_pScene);

				PARSER.AddWidgets(*pBank, GetSelectedEntity());

				CAnimScene::SetCurrentAddWidgetsScene(NULL);

				if (GetSelectedEntity()->GetType()==ANIM_SCENE_ENTITY_PED)
				{
					m_widgets.m_playbackListLeadInClipSetSelector.AddWidgets(pBank, "LeadIn Clipset Selector:", datCallback(MFA(CAnimSceneEditor::LeadInSelected), this));
				}

				pBank->UnSetCurrentGroup(*m_widgets.m_pSelectionGroup);
			}
		}

		SyncPickerFromAnimSceneEntity();
		m_flags.SetFlag(kRegenerateEventTypeWidgets);
		m_flags.ClearFlag(kRegenerateSelectionWidgets);
	}


	
	// handle scene control.
	if (m_pScene)
	{
		if (!m_pScene->IsInitialised())
		{
			// load the scene
			m_pScene->BeginLoad();
			SynchroniseHoldAtEndFlag();
		}
		else if (m_pScene->IsFinished())
		{
			// restart the scene
			if(m_editorSettingLoopPlayback)
			{
				m_pScene->Reset();
			}			
		}
		else if (m_pScene->IsLoaded() && !m_pScene->IsPlayingBack())
		{
			if (m_editorSettingAutoRegisterPlayer)
			{
				CPed* pPlayerPed = FindPlayerPed();

				if (pPlayerPed)
				{
					CAnimScenePed* pScenePed = m_pScene->GetEntity<CAnimScenePed>(pPlayerPed->GetModelName(), ANIM_SCENE_ENTITY_PED);
					if (pScenePed)
					{
						pScenePed->SetPed(pPlayerPed);
					}
				}
			}

			if (!m_flags.IsFlagSet(kInitialTimelineZoomSet))
			{
				m_timeline.CalculateInitialZoom(0.0f, m_pScene->GetDuration());
				m_flags.SetFlag(kInitialTimelineZoomSet);
			}

			bool wasPaused = m_pScene->GetPaused();

			// start the scene
			m_pScene->Start();		

			m_pScene->SetPaused(wasPaused);
			
			if (m_editorSettingPauseSceneAtFirstStart && m_firstPlayback)
			{
				m_pScene->SetPaused(true);
				m_firstPlayback = false;
			}
		}
		else if (m_pScene->IsPlayingBack())
		{
			if (!m_timeline.IsDraggingSelector())
			{
				// sync the timeline to the scene progress
				m_timeline.SetCurrentTime(m_pScene->GetTimeWithSkipping());

				if (!m_pScene->GetPaused())
				{
					// Update visual timeline according to user preference
					if (m_editorSettingTimelinePlaybackMode == kTimelinePlaybackMode_SelectorInCentre)
					{
						m_timeline.MoveMarkerToVisibleTimelinePosition(0.5f);
					}
					else if (m_editorSettingTimelinePlaybackMode == kTimelinePlaybackMode_MaintainSelectorPosition)
					{
						m_timeline.MoveMarkerToVisibleTimelinePosition(m_fSelectorTimelinePositionUponPlayback);
					}					
				}

				// overrides holding at last frame, restart the scene if looping is enabled
				/*
				if(m_editorSettingLoopPlayback)
				{
					if(m_timeline.GetCurrentTime >= m_pScene->GetDuration())
					{
						m_pScene->Reset();
					}
				}*/
			}
		}

		// handle deleting selected scene elements using the DELETE key.
		// for some reason the delete key checks below sometimes return TRUE
		// for multiple frames in succession, so we ignore subsequent delete commands
		// for a few frames.

		static u32 s_lastDeleteDelay = 3;
		static u32 s_lastDeleteFrame = 0;
		
		if (((fwTimer::GetFrameCount() - s_lastDeleteFrame) > s_lastDeleteDelay) && 
			(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DELETE, KEYBOARD_MODE_GAME) || CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DELETE, KEYBOARD_MODE_DEBUG) || m_pScene->GetShouldDeleteEntityFromWidgetCB()))
		{

			if (GetSelectedEvent() && GetFocus()==kFocusEvent)
			{
				char msg[200];
				atString eventText;
				GetSelectedEvent()->GetEditorEventText(eventText,*this);
				formatf(msg,200,"[%s] Are you sure you wish to delete this event? You won't be able to undo this.",eventText.c_str());
				if (bkMessageBox("Confirm delete", msg, bkMsgYesNo, bkMsgQuestion))
				{
					CAnimSceneEvent* pEventToDelete = GetSelectedEvent();
					ClearSelectionWidgets();
					SelectEvent(NULL);
					m_pScene->DeleteEvent(pEventToDelete);			
					//recalculate section lengths
					m_pScene->RecalculateSceneDurations();
				}
			}
			else if (GetSelectedEntity() && GetFocus()==kFocusEntity)
			{
				char msg[200];
				atString entityText;
				CAnimSceneEntity::Id entityId = GetSelectedEntity()->GetId();
				formatf(msg,200,"[%s] Are you sure you wish to delete this entity? You won't be able to undo this",entityId.TryGetCStr());
				if (bkMessageBox("Confirm delete", msg, bkMsgYesNo, bkMsgQuestion))
				{
					CAnimSceneEntity* pEntityToDelete = GetSelectedEntity();
					ClearSelectionWidgets();
					SelectEvent(NULL);
					SelectEntity(NULL);
					m_pScene->DeleteAllEventsForEntity(pEntityToDelete->GetId());
					m_pScene->DeleteEntity(pEntityToDelete);
					m_pScene->RecalculateSceneDurations();
				}
			}
			else if (m_sectionPanel.GetSelectedPlaybackList()!=ANIM_SCENE_PLAYBACK_LIST_ID_INVALID && GetFocus()==kFocusPlaybackList)
			{
				char msg[200];
				formatf(msg,200,"[%s] Are you sure you wish to delete this playback list? You won't be able to undo this",m_sectionPanel.GetSelectedPlaybackList().GetCStr());
				if (bkMessageBox("Confirm delete", msg, bkMsgYesNo, bkMsgQuestion))
				{
					m_pScene->DeletePlaybackList(m_sectionPanel.GetSelectedPlaybackList());
					m_sectionPanel.SetSelectedPlaybackList(ANIM_SCENE_PLAYBACK_LIST_ID_INVALID);
				}
			}
			else if (m_sectionPanel.GetSelectedSection()!=ANIM_SCENE_SECTION_ID_INVALID && GetFocus()==kFocusSection)
			{
				char msg[200];
				formatf(msg,200,"[%s] Are you sure you wish to delete this section? You won't be able to undo this",m_sectionPanel.GetSelectedSection().GetCStr());
				if (bkMessageBox("Confirm delete", msg, bkMsgYesNo, bkMsgQuestion))
				{
					m_pScene->DeleteSection(m_sectionPanel.GetSelectedSection());
					m_sectionPanel.SetSelectedSection(ANIM_SCENE_SECTION_ID_INVALID);
				}
			}
			
			m_pScene->SetShouldDeleteEntityFromWidgetCB(false);
			s_lastDeleteFrame = fwTimer::GetFrameCount();
		}

		// deselect the selected entity when clicking on the map, etc
		if (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT && !IsDragging() && !m_flags.IsFlagSet(kMadeEntitySelectionThisFrame) && !IsSelectingADifferentGameEntity())
		{
			SelectEntity(NULL);
		}

		if (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT)
		{
			m_pSelectedEventOnLastRelease = m_selected.GetEvent();
		}
	}

	//---- Handle display of scrollbar if there are too many tracks displayed on screen
	//Compute sum of events (timelines) Y size
	float totalYSize = 0.f;
	atArray<CEventLayerPanel*>& eventLayers = m_EventLayersWindow.GetEventLayers();
	for(int i= 0; i < eventLayers.GetCount(); i++)
	{
		if(eventLayers[i])
		{
			totalYSize += eventLayers[i]->GetTotalPanelHeight();
		}
	}
	//proportion of events on screen as compared to the whole number of events
	float screenEventsProportion = (m_EventLayersWindow.GetScreenExtents().GetMaxY() - m_EventLayersWindow.GetScreenExtents().GetMinY()) / totalYSize;
	if(screenEventsProportion < 1.f)
	{
		m_eventsScrollbar.SetBarHidden(false);
		m_eventsScrollbar.SetBarSize(screenEventsProportion);
		m_eventsScrollbar.SetMinValue(0.f);
		m_eventsScrollbar.SetMaxValue(totalYSize);
		m_eventsScrollbar.SetValue(-m_EventLayersYScreenOffset);
		if(m_eventsScrollbar.IsDragging() || !IsDragging())
		{
			m_eventsScrollbar.ProcessMouseInput();
		}		
		m_EventLayersYScreenOffset = -m_eventsScrollbar.GetValue();
	}
	else
	{
		m_eventsScrollbar.SetBarHidden(true);
		m_eventsScrollbar.SetMaxValue(totalYSize);
		m_eventsScrollbar.SetMinValue(0.f);
		m_EventLayersYScreenOffset = 0.f;
	}	
	
	RenderSelectedAnimScenesPossibleNewGameEntity(); 
}

bool CAnimSceneEditor::GetDisplayMiniMapThisUpdate()
{
	return !IsActive();
}

void CAnimSceneEditor::TimelineSelectorDragged(float fNewTime)
{
	// if dragging the time control, skip the scene to the selected time on the timeline, and pause the scene
	if (m_pScene && m_pScene->IsPlayingBack())
	{
		m_pScene->Skip(fNewTime);
		m_pScene->SetPaused(true);
	}
}

// PURPOSE: Set the screen extents of the editor
void CAnimSceneEditor::SetScreenExtents(const CScreenExtents& screenExtents)
{
	TUNE_FLOAT(testWidth, 200.0f, 0.0f, 2000.0f, 1.0f)
	TUNE_FLOAT(testHeight, 22.0f, 0.0f, 2000.0f, 1.0f)
	m_screenExtents = screenExtents;

	static float s_textPadding = 12.0f;
	static float s_entityPaneWidth = 200.0f;
	static float s_timelineControlHeightOffset = 2.0f;
	static float s_sceneControlHeight = 40.0f;
	static float s_timelineSceneControlGap = 10.0f;
	static float s_windowGaps = 5.0f;
	static float s_videoPanelWidth = 140.0f; 
	static float s_videoPanelHeight = 22.0f; 
	const float timelineMinX = m_screenExtents.GetMinX() + s_entityPaneWidth;
	const float timelineMaxX = m_screenExtents.GetMaxX();

	float textBoxHeight = GetTextHeight() + 2.0f*s_textPadding;
	float sceneControlHeight = Max(s_sceneControlHeight, textBoxHeight);
	float timelineControlHeight = textBoxHeight + s_timelineControlHeightOffset;

	// section panel at the top
	m_sectionPanel.SetScreenExtents(CScreenExtents(m_screenExtents.GetMinX(), screenExtents.GetMinY() + textBoxHeight, screenExtents.GetMaxX(),screenExtents.GetMinY()+(textBoxHeight*3.0f)));

	// Scene control widgets at bottom of screen, below the timeline
	m_videoPlayBackControl.SetScreenExtents(CScreenExtents(timelineMinX, screenExtents.GetMaxY() - sceneControlHeight, timelineMinX +s_videoPanelWidth, screenExtents.GetMaxY() - sceneControlHeight + s_videoPanelHeight)); 

	// Translation & Rotation Helper buttons
	m_authoringDisplay.SetScreenExtents(CScreenExtents(m_videoPlayBackControl.GetScreenExtents().GetMaxX() + s_windowGaps, screenExtents.GetMaxY() - sceneControlHeight, m_videoPlayBackControl.GetScreenExtents().GetMaxX() + s_windowGaps + testWidth, screenExtents.GetMaxY() - sceneControlHeight+ testHeight)); 

	// Timeline at the bottom of the screen, above the scene control widgets
	m_timeline.SetScreenExtents(CScreenExtents(timelineMinX, m_videoPlayBackControl.GetScreenExtents().GetMinY() - timelineControlHeight - s_timelineSceneControlGap, timelineMaxX, m_videoPlayBackControl.GetScreenExtents().GetMinY() - s_timelineSceneControlGap));

	// Scene display next to the timeline
	m_sceneInfoDisplay.SetScreenExtents(CScreenExtents(m_screenExtents.GetMinX(), m_videoPlayBackControl.GetScreenExtents().GetMinY() - timelineControlHeight - s_timelineSceneControlGap, timelineMinX - 5.0f, m_timeline.GetScreenExtents().GetMinY() + (GetTextHeight()*2.0f) + 6.0f));
	m_sceneInfoDisplay.SetTextColour(GetColor(kMainTextColor));
	
	// Left column for the entity pane
	m_EventLayersWindow.SetScreenExtents(CScreenExtents(m_screenExtents.GetMinX(), m_sectionPanel.GetScreenExtents().GetMaxY() + s_videoPanelHeight, timelineMinX, m_timeline.GetScreenExtents().GetMinY()));


}

// PURPOSE: Set the anim scene currently being edited
void CAnimSceneEditor::SetAnimScene(CAnimScene* pScene)
{
	m_pScene = pScene;
}

void CAnimSceneEditor::ComputeEventLayerInfo(CAnimSceneEvent* evt)
{
	animAssert(evt);

	TUNE_FLOAT(eventRenderFix, 0.0f, -1000.0f, 1000.0f, 1.0f)

	atHashString eventLayer(evt->GetPrimaryEntityId());
	atString eventSubGroupText(evt->parser_GetStructure()->GetName());
	eventSubGroupText.Replace("CAnimScene", "");
	eventSubGroupText.Replace("Event","");
	atHashString eventSubGroup(eventSubGroupText.c_str());

	//float yOffset = m_screenExtents.GetMinY() + EventLayer::ms_layerHeight;
	u32 position = 0;
	// first check if we've already encountered this layer, and add
	// a new one if not:
	for (s32 i=0 ; i<m_EventLayersWindow.GetEventLayers().GetCount(); i++)
	{
		//layer is an entitys group
		CEventLayerPanel& layer = *m_EventLayersWindow.GetEventLayers()[i];
		if (layer.GetName().GetHash()==eventLayer.GetHash())
		{
			// found the layer, now check the sub group
			for  (s32 j=0; j<layer.GetEventGroup().GetCount(); j++)
			{
				if (layer.GetEventGroup()[j]->GetName().GetHash()==eventSubGroup.GetHash())
				{
					// check if we overlap with any other events in the group
					//layer.m_subGroups[i].m_events.PushAndGrow(evt);
					
					EventInfo Info;
					Info.event = evt;
					Info.position = position; 
					layer.GetEventGroup()[j]->AddEvent(Info); 
					return;
				}
				//yOffset+=EventLayer::ms_layerHeight;
				position++; 
			}

			layer.AddEventGroup(eventSubGroup); 

			for  (s32 j=0; j<layer.GetEventGroup().GetCount(); j++)
			{
				if (layer.GetEventGroup()[j]->GetName().GetHash()==eventSubGroup.GetHash())
				{
					// check if we overlap with any other events in the group
					//layer.m_subGroups[i].m_events.PushAndGrow(evt);

					EventInfo Info;
					Info.event = evt;
					Info.position = position; 
					layer.GetEventGroup()[j]->AddEvent(Info); 
					return;
				}
				//yOffset+=EventLayer::ms_layerHeight;
				position++; 
			}
			return; 
			//return yOffset;
		}
		else
		{
			//yOffset += Max(layer.m_subGroups.GetCount(), 1)*EventLayer::ms_layerHeight;
		}		
	}

	m_EventLayersWindow.AddEventLayerPanel(eventLayer);
	//return yOffset;
}

void CAnimSceneEditor::UpdateEventLayerInfo(CAnimSceneEvent* evt)
{
	if (evt)
	{
		ComputeEventLayerInfo(evt);
	}
}





void CAnimSceneEditor::AddWidgets(bkGroup* pParentGroup)
{
	Assert(pParentGroup);

	sprintf(m_widgets.m_saveStatusTextArray, "");

	m_widgets.m_pSaveButton = pParentGroup->AddButton("Save scene", datCallback(MFA(CAnimSceneEditor::SaveCurrentScene), this));
	m_widgets.m_pSaveStatus = pParentGroup->AddText("", m_widgets.m_saveStatusTextArray, 31, true);
	m_widgets.m_pCloseButton = pParentGroup->AddButton("Close scene", datCallback(MFA(CAnimSceneEditor::TriggerCloseCurrentScene), this) );
	
	m_widgets.m_pSceneGroup = pParentGroup->AddGroup("Scene details");
	bkBank* pBank = CDebugClipSelector::FindBank(m_widgets.m_pSceneGroup);
	pBank->SetCurrentGroup(*m_widgets.m_pSceneGroup);
	PARSER.AddWidgets(*pBank, m_pScene);
	pBank->UnSetCurrentGroup(*m_widgets.m_pSceneGroup);
	
	m_widgets.m_pAddEntityGroup = pParentGroup->AddGroup("Add Entity");

	// create the selection list for entities
	{
		m_widgets.m_pAddEntityGroup->AddText("Entity Id", &m_widgets.m_newEntityId);

		m_widgets.m_entityNameList.clear();
		m_widgets.m_entityNameList.PushAndGrow("Select an entity to add");

		for (s32 i=0 ;i<m_entityInitialisers.GetCount(); i++)
		{
			if (m_entityInitialisers[i])
			{
				m_widgets.m_entityNameList.PushAndGrow(m_entityInitialisers[i]->GetFriendlyEntityTypeName().GetCStr());
			}
		}
		
		if(m_entityInitialisers.GetCount() > 3)
		{
			m_widgets.m_selectedEntityType = 3; // Is probably Ped, but really this should be done properly.
		}
		m_widgets.m_pEntityTypeCombo = m_widgets.m_pAddEntityGroup->AddCombo("Entity type", (unsigned char *)(&(m_widgets.m_selectedEntityType)), m_widgets.m_entityNameList.GetCount(), &(m_widgets.m_entityNameList[0]), datCallback(MFA(CAnimSceneEditor::RegenEntityTypeWidgets), this), 0, NULL);
		RegenEntityTypeWidgets();
	}

	m_widgets.m_pAddSectionGroup = pParentGroup->AddGroup("Sectioning");
	{
		m_widgets.m_pAddSectionGroup->AddText("New section / playback list name", &m_widgets.m_newSectionId);
		m_widgets.m_pAddSectionGroup->AddButton("Add new section", datCallback(MFA(CAnimSceneEditor::AddNewSection), this));
		m_widgets.m_pAddSectionGroup->AddButton("Delete this section", datCallback(MFA(CAnimSceneEditor::DeleteSection), this));
		m_widgets.m_pAddSectionGroup->AddButton("Add new playback list", datCallback(MFA(CAnimSceneEditor::AddNewPlaybackList), this));
	}

	m_widgets.m_pAddEventGroup = pParentGroup->AddGroup("Add Event");
	{

	}

	m_widgets.m_pSelectionGroup = pParentGroup->AddGroup("Current selection");
}

void CAnimSceneEditor::AddEditorWidgets(bkGroup* pParentGroup)
{
	Assert(pParentGroup);

	bkGroup* pGeneralGroup = pParentGroup->AddGroup("General");
	{
		pGeneralGroup->AddToggle("Use Smooth Font", &m_editorSettingUseSmoothFont, datCallback(MFA(CAnimSceneEditor::RefreshScreenExtents), this));
		pGeneralGroup->AddToggle("Hide events tracks", &m_editorSettingHideEvents);
		pGeneralGroup->AddToggle("Hold at last frame", &m_editorSettingHoldAtLastFrame,  datCallback(MFA(CAnimSceneEditor::SynchroniseHoldAtEndFlag), this));
		pGeneralGroup->AddToggle("Loop playback in editor (only if not holding)", &m_editorSettingLoopPlayback, datCallback(MFA(CAnimSceneEditor::LoopEditorToggleCB), this));
		pGeneralGroup->AddToggle("Pause scene after first load", &m_editorSettingPauseSceneAtFirstStart);
		pGeneralGroup->AddToggle("Reset scene on playback list change", &m_editorSettingResetSceneOnPlaybackListChange);
		pGeneralGroup->AddToggle("Auto register player (using model name)", &m_editorSettingAutoRegisterPlayer);
		pGeneralGroup->AddToggle("Use event colour for marker rendering.", &ms_editorSettingUseEventColourForMarker);
		pGeneralGroup->AddToggle("Regen widgets automatically when new event is selected (for repeated play anim adding).", &m_bAllowWidgetAutoRegen);
		pGeneralGroup->AddSlider("Background alpha.", &ms_editorSettingBackgroundAlpha,0.0f,1.0f,0.05f);
		pGeneralGroup->AddButton("Validate Scene", datCallback(MFA(CAnimSceneEditor::ValidateScene), (datBase*)this));
	}

	bkGroup* pTimelineGroup = pParentGroup->AddGroup("Timeline");
	{
		static const char* playbackModes[kTimelinePlaybackMode_Num] = {"Maintain Selector Position", "Keep Selector In Centre", "Do not move timeline"};
		pTimelineGroup->AddCombo("Playback Behaviour", &m_editorSettingTimelinePlaybackMode, kTimelinePlaybackMode_Num, playbackModes);

		static const char* timelineDisplayMode[kTimelineDisplayMode_Num] = {"Frames", "Seconds"};
		pTimelineGroup->AddCombo("Time Display Mode", &m_editorSettingTimelineDisplayMode, kTimelineDisplayMode_Num, timelineDisplayMode, datCallback(MFA(CAnimSceneEditor::TimelineDisplayModeChangedCB), (datBase*)this) );

		pTimelineGroup->AddToggle("Snap To Frames", &m_editorSettingTimelineSnapToFrames, datCallback(MFA(CAnimSceneEditor::TimelineSnapChangedCB), (datBase*)this));
	}

	bkGroup* pEditorGroup = pParentGroup->AddGroup("Event Editing");
	{
		pEditorGroup->AddToggle("Edit event preload times", &m_editorSettingEditEventPreloads);
		pEditorGroup->AddToggle("Clamp anim event lengths to anim lengths", &CAnimScenePlayAnimEvent::ms_editorSettings.m_clampEventLengthToAnim);
		pEditorGroup->AddToggle("Display anim event dictionaries (when hovering)", &CAnimScenePlayAnimEvent::ms_editorSettings.m_displayDictionaryNames);
		pEditorGroup->AddToggle("Snap To Frames When Editing Events", &m_editorSettingEventEditorSnapToFrames);
		pEditorGroup->AddSlider("Events Scrolling", &m_EventLayersYScreenOffset, -1000.0f, 500.0f, 1.0f);
		
	}
}

void CAnimSceneEditor::SelectEntity(CAnimSceneEntity* pEntity, bool setFocus /*= true*/)
{
	if (pEntity!= m_selected.GetEntity())
	{
		m_flags.SetFlag(kRegenerateSelectionWidgets | kRegenerateNewEventWidgets);
	}

	m_selected.SelectEntity(pEntity);

	if (pEntity && setFocus)
	{
		SetFocus(kFocusEntity);
	}
}

void CAnimSceneEditor::SelectEvent(CAnimSceneEvent* pEvent, bool setFocus /*= true*/)
{
	if (pEvent!= m_selected.GetEvent())
	{
		m_flags.SetFlag(kRegenerateSelectionWidgets);

		CAnimSceneEntity::Id eventEntId = pEvent ? pEvent->GetPrimaryEntityId() : ANIM_SCENE_ENTITY_ID_INVALID;
		CAnimSceneEntity::Id selectedEntId = GetSelectedEntity() ? GetSelectedEntity()->GetId() : ANIM_SCENE_ENTITY_ID_INVALID;

		if (eventEntId.GetHash()!=selectedEntId.GetHash())
		{
			// GetEntity returns null if the id is invalid
			SelectEntity(m_pScene->GetEntity(eventEntId));
			m_flags.SetFlag(kMadeEventSelectionThisClick);
		}
		else if (pEvent==NULL)
		{
			m_pSelectedEventOnLastRelease = NULL;
		}
	}

	m_selected.SelectEvent(pEvent);
	if (pEvent && setFocus)
	{
		SetFocus(kFocusEvent);
	}
}

void CAnimSceneEditor::SaveCurrentScene()
{
	sprintf(m_widgets.m_saveStatusTextArray, "");
	if (m_pScene)
	{
		bool bResult = m_pScene->Save();

		sprintf(m_widgets.m_saveStatusTextArray, "Save %s", bResult ? "Successful" : "Failed");
		
		CAnimSceneManager::LoadSceneNameList();
	}
}

void CAnimSceneEditor::TriggerCloseCurrentScene()
{
	m_flags.SetFlag(kCloseCurrentScene);
}

Color32 CAnimSceneEditor::GetColor(eColorType type)
{
	if(type < kMaxNumColors)
	{
		return m_colors[type];
	}

	return Color32(0,0,0,0);
}

void CAnimSceneEditor::SetColor(u32 type, Color32 color)
{
	if(type < kMaxNumColors)
	{
		m_colors[type] = color;
	}
}

void CAnimSceneEditor::SetAnimSceneEditorColors()
{
	SetColor(kMainTextColor, ASE_Editor_DefaultTextColor); 
	SetColor(kSelectedTextColor, ASE_Editor_SelectedTextColor); 
	SetColor(kSelectedTextOutlineColor, ASE_Editor_SelectedTextOutlineColor);
	SetColor(kFocusTextOutlineColor, ASE_Editor_FocusTextOutlineColor); 
	SetColor(kTextHoverColor, ASE_Editor_HoveredTextColor); 
	SetColor(kTextWarningColor, ASE_Editor_WarningTextColor); 
	SetColor(kSelectedTextHoverColor, ASE_Editor_SelectedHoveredTextColor); 

	SetColor(kTextBackgroundColor, ASE_Editor_DefaultTextBackgroundColor); 
	SetColor(kSelectedTextBackGroundColor, ASE_Editor_SelectedTextBackgroundColor); 
	SetColor(kTextOutlineColor, ASE_Editor_DefaultTextOutlineColor); 
	SetColor(kRegionOutlineColor, ASE_Panel_OutlineColor); 

	SetColor(kDisabledTextColor, ASE_Editor_DisabledTextColor); 
	SetColor(kDisabledBackgroundColor, ASE_Editor_DisabledBackgroundColor); 
	SetColor(kDisabledOutlineColor, ASE_Editor_DisabledOutlineColor); 
	
	SetColor(kDefaultEventColor, ASE_Panel_EventNorthernBlackColor);
	SetColor(kEventMarkerColor, ASE_Panel_EventMarkerColor);
	SetColor(kEventOutlineColor, ASE_Panel_EventOutlineColor);
	SetColor(kEventBackgroundColor, ASE_Panel_EventBackgroundColor); 

	SetColor(kSelectedEventColor, ASE_Panel_SelectedEventColor);
	SetColor(kSelectedEventMarkerColor, ASE_Panel_SelectedEventMarkerColor);
	SetColor(kSelectedEventOutlineColor, ASE_Panel_SelectedEventOutlineColor);
	SetColor(kSelectedEventDraggingColor, ASE_Panel_SelectedEventDraggingColor);

	SetColor(kVfxEventColor, ASE_Panel_EventJubileeGreyColor);

	SetColor(kPlayAnimEventColor, ASE_Panel_EventVictoriaBlueColor);
	SetColor(kPlayAnimEventColorAlt, ASE_Panel_EventDistrictGreen );  //ASE_Panel_EventDistrictGreen, or ASE_Panel_EventNorthernBlackColor

	SetColor(kEventPreloadLineColor, ASE_Panel_EventPreloadlineColor);
	SetColor(kEventCurrentTimeLineColor, ASE_Panel_EventCurrentTimeLineColor);

	SetColor(kBlendInMarkerColor, ASE_Panel_EventOGOrangeColor);
	SetColor(kBlendOutMarkerColor, ASE_Panel_EventWCGreenColor);

	SetColor(kScrollBarBackgroundColour, ASE_Panel_Button_BackGroundColor);
	SetColor(kScrollBarActiveBackgroundColour, ASE_Panel_Button_ActiveBackgroundColor);
	SetColor(kScrollBarHoverBackgroundColour, ASE_Panel_Button_HoverBackgroundColor);

	SetColor(kSectionDividerColor, ASE_Panel_EventNorthernBlackColor); 
}


bool CAnimSceneEditor::IsDraggingMatrixHelper()
{
	for(int i=0; i < CAnimSceneMatrix::m_AnimSceneMatrix.GetCount(); i++)
	{
		CAnimSceneMatrix* pAnimSceneMatrix = CAnimSceneMatrix::m_AnimSceneMatrix[i]; 
		if (pAnimSceneMatrix && pAnimSceneMatrix->GetAuthoringHelper().GetCurrentInput())
		{
			return true;
		}
	}
	return false;
}

atHashString CAnimSceneEditor::GetSelectedEntityId()
{
	return GetSelectedEntity() ? GetSelectedEntity()->GetId() : ANIM_SCENE_ENTITY_ID_INVALID;
}

void CAnimSceneEditor::CloseCurrentScene()
{
	Shutdown();
}

void CAnimSceneEditor::RegenEntityTypeWidgets()
{
	CleanupEntityInitialiserWidgets();

	// clear out any previous widgets:
	while (m_widgets.m_pEntityTypeCombo->GetNext())
	{
		m_widgets.m_pEntityTypeCombo->GetNext()->Destroy();
	}

	s32 selectedType = m_widgets.m_selectedEntityType;

	if (selectedType>0 && selectedType < m_widgets.m_entityNameList.GetCount())
	{
		atHashString type(m_widgets.m_entityNameList[selectedType]);
		
		bkBank* pBank = CDebugClipSelector::FindBank(m_widgets.m_pAddEntityGroup);
		pBank->SetCurrentGroup(*(m_widgets.m_pAddEntityGroup));

		CAnimSceneEntityInitialiser* pInit = GetEntityInitialiserFromFriendlyName(type);
		
		pInit->AddWidgets(m_widgets.m_pAddEntityGroup, this);
		pInit->m_bRequiresWidgetCleanup = true;

		m_widgets.m_pAddEntityGroup->AddButton("Add entity", datCallback(MFA(CAnimSceneEditor::AddNewEntity), this));

		pBank->UnSetCurrentGroup(*(m_widgets.m_pAddEntityGroup));
	}
}

void CAnimSceneEditor::RegenEventTypeWidgets()
{
	CleanupEventInitialiserWidgets();

	// clear out any previous widgets:
	while (m_widgets.m_pEventTypeCombo->GetNext())
	{
		m_widgets.m_pEventTypeCombo->GetNext()->Destroy();
	}

	s32 selectedType = m_widgets.m_selectedEventType;
	if (selectedType>0 && selectedType < m_widgets.m_eventNameList.GetCount())
	{
		atHashString type(m_widgets.m_eventNameList[selectedType]);

		bkBank* pBank = CDebugClipSelector::FindBank(m_widgets.m_pAddEventGroup);
		bkGroup& group = *(m_widgets.m_pAddEventGroup);

		pBank->SetCurrentGroup(group);

		CAnimSceneEventInitialiser* pInitialiser = GetEventInitialiserFromFriendlyName(type);

		pInitialiser->AddWidgets(&group, this);
		pInitialiser->m_bRequiresWidgetCleanup = true;

		group.AddButton("Add event", datCallback(MFA(CAnimSceneEditor::AddNewEvent), this));

		pBank->UnSetCurrentGroup(group);
	}
}

void CAnimSceneEditor::ClearSelectionWidgets()
{
	if (m_widgets.m_pSelectionGroup)
	{
		m_widgets.m_playbackListLeadInClipSetSelector.RemoveWidgets();

		while (m_widgets.m_pSelectionGroup->GetChild())
		{
			m_widgets.m_pSelectionGroup->GetChild()->Destroy();
		}
	}
}

CAnimSceneEntity*  CAnimSceneEditor::CreateEntityAndAddToSceneEditor(CAnimSceneEntity::Id newID, CAnimSceneEntityInitialiser* pInitialiser, parStructure* pStruct)
{
	if (animVerifyf(!m_pScene->GetEntity(newID), "An entity with id '%s' already exists in the scene!", newID.GetCStr()))
	{
		CAnimSceneEntity* pNewEnt = (CAnimSceneEntity*)pStruct->Create();
		pNewEnt->SetId(newID);
		pNewEnt->InitForEditor(pInitialiser);

		// Don't add again if the entity was already added in InitForEditor above
		if (!m_pScene->GetEntity(newID))
		{
			m_pScene->SceneEditorAddEntity(newID, pNewEnt);
		}

		return pNewEnt;
	}

	return NULL;
}

void CAnimSceneEditor::AddNewEntity()
{
	s32 selectedType = m_widgets.m_selectedEntityType;
	if (selectedType>=0 && selectedType < m_widgets.m_entityNameList.GetCount())
	{
		const char * pType = m_widgets.m_entityNameList[selectedType];

		CAnimSceneEntity::Id newID(m_widgets.m_newEntityId);

		if (newID==ANIM_SCENE_ID_INVALID)
		{
			bkMessageBox("Enter an entity Id", "You must enter a valid entity id (empty).", bkMsgOk, bkMsgInfo);
			return;
		}

		if( !CAnimSceneEntity::IsValidId(newID) )
		{
			animDisplayf("[Anim Scene Editor] Attempting to fix entered entity id: \"%s\"", newID.TryGetCStr());
			if(!FixInvalidId(newID)) //returns true if able to fix
			{
				bkMessageBox("Entered Entity Id", "Unable to automatically fix entity id (empty or contains invalid characters).", bkMsgOk, bkMsgInfo);
				return;
			}
			else
			{
				animDisplayf("[Anim Scene Editor] Successfully fixed to: \"%s\"", newID.TryGetCStr());
			}
		}

		if (m_pScene->GetEntity(newID))
		{
			bkMessageBox("Entity Id must be unique", "An entity with this Id already exists. Please enter a unique name.", bkMsgOk, bkMsgInfo);
			return;
		}

		CAnimSceneEntityInitialiser* pInit = GetEntityInitialiserFromFriendlyName(pType);

		if(!pInit)
		{
			bkMessageBox("Invalid anim scene entity initialiser", "Error adding a new entity! The entity initialiser couldn't be created from the name of the type.", bkMsgOk, bkMsgError);
			return;
		}

		parStructure* pStruct = PARSER.FindStructure(pInit->GetEntityTypeName().GetCStr());

		if(!pStruct)
		{
			bkMessageBox("Invalid entity type", "Error adding a new entity! The selected entity type is not recognised", bkMsgOk, bkMsgError);
			return;
		}

		atHashString type(pType);

		CreateEntityAndAddToSceneEditor(newID, pInit, pStruct);
	}	
}

CAnimSceneEvent* CAnimSceneEditor::CreateEventAndAddToCurrentScene(const char * eventTypeName, CAnimSceneEventInitialiser* pInitialiser, atHashString sectionId)
{
	CAnimSceneEvent* pNewEvent = CreateNewEventOfType(eventTypeName);

	pInitialiser->m_pEditor = this;
	pInitialiser->m_pSection = m_pScene->GetSection(sectionId);

	pNewEvent->SetParentList(&m_pScene->GetSection(sectionId)->GetEvents());

	if(!pNewEvent->InitForEditor(pInitialiser))
	{
		delete pNewEvent;
		return NULL;
	}

	m_pScene->SceneEditorAddEvent(pNewEvent, sectionId);

	return pNewEvent;
}

CAnimSceneEvent* CAnimSceneEditor::CreateNewEventOfType(const char * typeName)
{
	parStructure* pStruct = PARSER.FindStructure(typeName);

	if(!pStruct)
	{
		bkMessageBox("Invalid event type", "Error creating a new event! The selected event type is not recognised", bkMsgOk, bkMsgError);
		return NULL;
	}

	return (CAnimSceneEvent*)pStruct->Create();
}

void CAnimSceneEditor::AddNewEvent()
{
	s32 selectedType = m_widgets.m_selectedEventType;
	if (selectedType>=0 && selectedType < m_widgets.m_eventNameList.GetCount())
	{
		CAnimSceneEventInitialiser* pInitialiser = GetEventInitialiserFromFriendlyName(atHashString(m_widgets.m_eventNameList[selectedType]));
		pInitialiser->m_pEntity = GetSelectedEntity();
		animAssertf(pInitialiser->m_pEntity,"When adding new event, couldn't get selected entity.");

		const char * pType = pInitialiser->GetEventTypeName().GetCStr();

		CAnimSceneSection::Id sectionId;

		if (!m_widgets.m_addEventSectionSelector.HasSelection() || m_widgets.m_addEventSectionSelector.GetSelectedIndex()==0)
		{
			sectionId = m_pScene->FindSectionForTime(GetCurrentTime());
		}
		else
		{
			sectionId.SetFromString(m_widgets.m_addEventSectionSelector.GetSelectedName());
		}

		CreateEventAndAddToCurrentScene(pType, pInitialiser, sectionId);
	}	
}

//This needs to refresh the section on the add event section in RAG (if it exists) 
void CAnimSceneEditor::AddNewSection()
{
	m_pScene->AddNewSection(m_widgets.m_newSectionId);
}

void CAnimSceneEditor::AddNewPlaybackList()
{
	m_pScene->AddNewPlaybackList(m_widgets.m_newSectionId);
}

void CAnimSceneEditor::LeadInSelected()
{
	CAnimSceneEntity* pEnt = GetSelectedEntity();
	if (m_pScene && pEnt && pEnt->GetType()==ANIM_SCENE_ENTITY_PED)
	{
		CAnimScenePed* pPedEnt = static_cast<CAnimScenePed*>(pEnt);
		fwMvClipSetId clipSet(CLIP_SET_ID_INVALID);
		if (m_widgets.m_playbackListLeadInClipSetSelector.HasSelection())
		{
			clipSet.SetFromString(m_widgets.m_playbackListLeadInClipSetSelector.GetSelectedName());
		}

		pPedEnt->SetLeadInClipSet(m_sectionPanel.GetSelectedPlaybackList(), clipSet);
	}

}

void CAnimSceneEditor::DeleteSection()
{
	if (bkMessageBox("Confirm delete", "Are you sure you wish to delete this section? You won't be able to undo this.", bkMsgYesNo, bkMsgQuestion))
	{
		m_pScene->DeleteSection(m_widgets.m_newSectionId);
	}
}

void CAnimSceneEditor::AddNewEventWidgetsForEntity(CAnimSceneEntity* pEntity, bkGroup* pGroup)
{
	// clear the group contents
	CleanupEventInitialiserWidgets();

	m_widgets.m_addEventSectionSelector.RemoveWidgets();

	while (pGroup->GetChild())
	{
		pGroup->GetChild()->Destroy();
	}

	bkBank* pBank = CDebugClipSelector::FindBank(pGroup);

	pBank->SetCurrentGroup(*pGroup);
	m_widgets.m_addEventSectionSelector.SetAnimScene(m_pScene);
	m_widgets.m_addEventSectionSelector.Select(CDebugAnimSceneSectionSelector::sm_FromTimeMarkerString); // by default, selects the section using the current time
	m_widgets.m_addEventSectionSelector.AddWidgets(pBank, "Section:");
	pBank->UnSetCurrentGroup(*pGroup);

	m_widgets.m_eventNameList.clear();
	m_widgets.m_eventNameList.PushAndGrow("Select an event to add");

	eAnimSceneEntityType type = pEntity ? pEntity->GetType() : ANIM_SCENE_ENTITY_NONE;

	for (s32 i=0 ;i<m_eventInitialisers.GetCount(); i++)
	{
		if (m_eventInitialisers[i] && m_eventInitialisers[i]->SupportsEntityType(type))
		{
			m_widgets.m_eventNameList.PushAndGrow(m_eventInitialisers[i]->GetFriendlyEventTypeName().GetCStr());
		}
	}

	if(m_bAllowWidgetAutoRegen)
	{
		m_widgets.m_selectedEntityType = 0;
	}
	
	m_widgets.m_pEventTypeCombo = pGroup->AddCombo("Event type", (unsigned char *)(&(m_widgets.m_selectedEventType)), m_widgets.m_eventNameList.GetCount(), &(m_widgets.m_eventNameList[0]), datCallback(MFA(CAnimSceneEditor::RegenEventTypeWidgets), this), 0, NULL);
}

void CAnimSceneEditor::Selection::SelectEntity(CAnimSceneEntity* pEnt)
{
	if (pEnt)
	{
		// deselect the selected event if it doesn't belong to the selected entity
		if (m_pEvent && m_pEvent->GetPrimaryEntityId()!=pEnt->GetId())
		{
			m_pEvent = NULL;
		}
	}
	else
	{
		if (m_pEvent && m_pEvent->GetPrimaryEntityId()!=ANIM_SCENE_ENTITY_ID_INVALID)
		{
			m_pEvent = NULL;
		}
	}

	m_pEntity = pEnt;
}

void CAnimSceneEditor::Selection::SelectEvent(CAnimSceneEvent* pEvt)
{
	if (pEvt)
	{
		// deselect the selected entity if it doesn't belong to the selected event
		if (m_pEntity && m_pEntity->GetId()!=pEvt->GetPrimaryEntityId())
		{
			m_pEntity = NULL;
		}
	}

	m_pEvent = pEvt;
}

// Rag callbacks
void CAnimSceneEditor::TimelineDisplayModeChangedCB()
{
	if (m_editorSettingTimelineDisplayMode == kTimelineDisplayMode_Frames)
	{
		m_timeline.SetTimeDisplayMode(COnScreenTimeline::kDisplayFrames);
	}
	else
	{
		m_timeline.SetTimeDisplayMode(COnScreenTimeline::kDisplayTimes);
	}
}

void CAnimSceneEditor::TimelineSnapChangedCB()
{
	m_timeline.SetMarkerSnapToFrames(m_editorSettingTimelineSnapToFrames);
}

void CAnimSceneEditor::InitEventInitialisers()
{
	// run over the event initialiser definitions and make one of each of them
	const parStructure* pEventInitialiserStruct = PARSER.FindStructure("CAnimSceneEventInitialiser");

	parManager::StructureMap::ConstIterator it = PARSER.GetStructureMap().CreateIterator();

	for( ; !it.AtEnd(); it.Next())
	{
		parStructure* pStruct = it.GetData();
		if (pEventInitialiserStruct && pStruct->IsSubclassOf(pEventInitialiserStruct) && pStruct!=pEventInitialiserStruct)
		{
			CAnimSceneEventInitialiser* pInst = (CAnimSceneEventInitialiser*)pStruct->Create();
			pInst->m_pEditor = this;
			// instantiate an initialiser and add it to the list
			m_eventInitialisers.PushAndGrow(pInst);
		}
	}
}

void CAnimSceneEditor::InitEventScrollbar()
{
	m_eventsScrollbar.Init(0.f,10.f,0.f,
		GetColor(kScrollBarBackgroundColour),GetColor(kTextHoverColor),CAnimSceneScrollbar::ORIENTATION_VERTICAL);
	m_eventsScrollbar.SetTopLeft(Vector2(m_EventLayersWindow.GetScreenExtents().GetMinX()-20.f,m_EventLayersWindow.GetScreenExtents().GetMinY() + 10.f));
	m_eventsScrollbar.SetLength(m_EventLayersWindow.GetScreenExtents().GetMaxY() - m_EventLayersWindow.GetScreenExtents().GetMinY() - 20.f);
	m_eventsScrollbar.SetBarHidden(true);
}

CAnimSceneEventInitialiser* CAnimSceneEditor::GetEventInitialiserFromFriendlyName(atHashString friendlyName)
{
	for (s32 i=0; i<m_eventInitialisers.GetCount(); i++)
	{
		if (m_eventInitialisers[i]->GetFriendlyEventTypeName()==friendlyName)
		{
			return m_eventInitialisers[i];
		}
	}
	return NULL;
}

CAnimSceneEventInitialiser* CAnimSceneEditor::GetEventInitialiser(atHashString eventClassName)
{
	for (s32 i=0; i<m_eventInitialisers.GetCount(); i++)
	{
		if (m_eventInitialisers[i]->GetEventTypeName()==eventClassName)
		{
			return m_eventInitialisers[i];
		}
	}
	return NULL;
}

void CAnimSceneEditor::CleanupEventInitialiserWidgets()
{
	for (s32 i=0;i<m_eventInitialisers.GetCount(); i++)
	{
		if (m_eventInitialisers[i]->m_bRequiresWidgetCleanup)
		{
			m_eventInitialisers[i]->RemoveWidgets(m_widgets.m_pAddEventGroup, this);
			m_eventInitialisers[i]->m_bRequiresWidgetCleanup = false;
		}
	}
}

void CAnimSceneEditor::InitEntityInitialisers()
{
	// run over the event initialiser definitions and make one of each of them
	const parStructure* pInitialiserStruct = PARSER.FindStructure("CAnimSceneEntityInitialiser");

	parManager::StructureMap::ConstIterator it = PARSER.GetStructureMap().CreateIterator();

	for( ; !it.AtEnd(); it.Next())
	{
		parStructure* pStruct = it.GetData();
		if (pInitialiserStruct && pStruct->IsSubclassOf(pInitialiserStruct) && pStruct!=pInitialiserStruct)
		{
			CAnimSceneEntityInitialiser* pInst = (CAnimSceneEntityInitialiser*)pStruct->Create();
			pInst->m_pEditor = this;
			// instantiate an initialiser and add it to the list
			m_entityInitialisers.PushAndGrow(pInst);
		}
	}
}

CAnimSceneEntityInitialiser* CAnimSceneEditor::GetEntityInitialiserFromFriendlyName(atHashString friendlyName)
{
	for (s32 i=0; i<m_entityInitialisers.GetCount(); i++)
	{
		if (m_entityInitialisers[i]->GetFriendlyEntityTypeName()==friendlyName)
		{
			return m_entityInitialisers[i];
		}
	}
	return NULL;
}

CAnimSceneEntityInitialiser* CAnimSceneEditor::GetEntityInitialiser(atHashString eventClassName)
{
	for (s32 i=0; i<m_entityInitialisers.GetCount(); i++)
	{
		if (m_entityInitialisers[i]->GetEntityTypeName()==eventClassName)
		{
			return m_entityInitialisers[i];
		}
	}
	return NULL;
}

void CAnimSceneEditor::CleanupEntityInitialiserWidgets()
{
	for (s32 i=0;i<m_entityInitialisers.GetCount(); i++)
	{
		if (m_entityInitialisers[i]->m_bRequiresWidgetCleanup)
		{
			m_entityInitialisers[i]->RemoveWidgets(m_widgets.m_pAddEventGroup, this);
			m_entityInitialisers[i]->m_bRequiresWidgetCleanup = false;
		}
	}
}

bool CAnimSceneEditor::FixInvalidId(CAnimSceneEntity::Id& id)
{
	const char* idStr = id.TryGetCStr();
	std::string idStrS = idStr;

	for(int i = 0; i < idStrS.length(); )
	{
		if(idStrS[i] == '\n' || idStrS[i] == '\r')
		{
			idStrS.erase(i,1);//Only erase this character
			//Can be changed to isStrS.erase(i) to erase all characters afterwards as well.
		}
		else
		{
			++i;
		}
	}

	id.SetFromString(idStrS.c_str());

	return CAnimSceneEntity::IsValidId(id);
}

void CAnimSceneEditor::CheckAnimSceneVersion()
{
	if(!m_pScene)
	{
		return;
	}
	if(m_pScene->GetVersion() != ANIM_SCENE_CURRENT_VERSION)
	{
		animWarningf("Anim scene is version: %d, current anim scene version is: %d", m_pScene->GetVersion(), ANIM_SCENE_CURRENT_VERSION);
	}

}

void CAnimSceneEditor::RenderEventsScrollbar()
{
	m_eventsScrollbar.Render();
}

bool CAnimSceneEditor::ValidateScene()
{
	if(m_pScene)
	{
		return m_pScene->Validate();
	}
	return false;
}

void CAnimSceneEditor::SynchroniseHoldAtEndFlag()
{
	if(m_pScene)
	{
		m_pScene->SetEditorHoldAtEnd(m_editorSettingHoldAtLastFrame);
	}
}

void CAnimSceneEditor::LoopEditorToggleCB()
{
	if(m_editorSettingLoopPlayback)
	{
		m_editorSettingHoldAtLastFrame = false;
		SynchroniseHoldAtEndFlag();
	}
}

CAnimSceneEditor::MouseState::MouseState()
	: m_mouseCoords()
	, m_lastMouseCoords()
	, m_leftMouseDown(false)
	, m_leftMousePressed(false)
	, m_leftMouseReleased(false)
	, m_rightMouseDown(false)
	, m_rightMousePressed(false)
	, m_rightMouseReleased(false)
{

}

void CAnimSceneEditor::MouseState::Update()
{
	m_lastMouseCoords.x = m_mouseCoords.x;
	m_lastMouseCoords.y = m_mouseCoords.y;
	m_mouseCoords.x = static_cast<float>(ioMouse::GetX());
	m_mouseCoords.y = static_cast<float>(ioMouse::GetY());
	m_leftMouseDown = (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT);
	m_leftMousePressed = (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT);
	m_leftMouseReleased = (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT);
	m_rightMouseDown = ((ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT)!=0);
	m_rightMousePressed = ((ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT)!=0);
	m_rightMouseReleased = ((ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT)!=0);

	animDebugf3("[%d:%d] LMD: %s LMP: %s LMR: %s MouseX:%.3f(%.3f) MouseY:%.3f(%.3f)"
		, fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount()
		, m_leftMouseDown? "TRUE" : "FALSE"
		, m_leftMousePressed? "TRUE" : "FALSE"
		, m_leftMouseReleased? "TRUE" : "FALSE"
		, m_mouseCoords.x, m_lastMouseCoords.x, m_mouseCoords.y, m_lastMouseCoords.y);
}


#endif // __BANK
