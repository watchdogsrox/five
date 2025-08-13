
#include "script/script_hud.h"

// Rage headers
#include "grcore/allocscope.h"
#include "grcore/effect.h"
#include "grcore/effect_values.h"
#include "grcore/quads.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/NewHud.h"
#include "frontend/PauseMenu.h"
#include "frontend/CMapMenu.h"
#include "frontend/minimap.h"
#include "frontend/HudTools.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "game/Clock.h"
#include "network/NetworkInterface.h"
#include "peds/ped.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderTargetMgr.h"
#include "SaveLoad/GenericGameStorage.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "script/script_channel.h"
#include "script/script_debug.h"
#include "script/script_text_construction.h"
#include "streaming/defragmentation.h"
#include "text/TextFormat.h"
#include "vfx/misc/MovieManager.h"
#include "control/replay/replay.h"

#if RSG_DURANGO
#include "system/companion.h"
#endif

s32 CScriptHud::scriptTextRenderID;
s32 CScriptHud::ms_iOverrideTimeForAreaVehicleNames = -1;
bool CScriptHud::bUsingMissionCreator;
bool CScriptHud::bForceShowGPS = false;
bool CScriptHud::bSetDestinationInMapMenu = false;
bool CScriptHud::bWantsToBlockWantedFlash = false;
bool CScriptHud::bAllowMissionCreatorWarp;
bool CScriptHud::ms_bDisplayPlayerNameBlipTags;
eWIDESCREEN_FORMAT CScriptHud::CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;
bool CScriptHud::bScriptHasChangedWidescreenFormat = false;
s32 CScriptHud::ms_IndexOfDrawOrigin = -1;
u32 CScriptHud::ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD;
CHudAlignment CScriptHud::ms_CurrentScriptGfxAlignment;
bool CScriptHud::ms_bAdjustNextPosSize = false;
bool CScriptHud::ms_bUseMaskForNextSprite = false;
bool CScriptHud::ms_bInvertMaskForNextSprite = false;
s32 CScriptHud::iScriptReticleMode = -1;
bool CScriptHud::bUseVehicleTargetingReticule = false;
bool CScriptHud::bHideLoadingAnimThisFrame = false;
bool CScriptHud::bRenderFrontendBackgroundThisFrame = false;
bool CScriptHud::bRenderHudOverFadeThisFrame = false;
sFakeInteriorStruct CScriptHud::FakeInterior;
bool CScriptHud::ms_bFakeExteriorThisFrame;
bool CScriptHud::bDontZoomMiniMapWhenSnipingThisFrame = false;
bool CScriptHud::ms_bHideMiniMapExteriorMapThisFrame;
bool CScriptHud::ms_bHideMiniMapInteriorMapThisFrame;
Vector2 CScriptHud::vFakePauseMapPlayerPos;
Vector3 CScriptHud::vFakeGPSPlayerPos;
Vector2 CScriptHud::vInteriorFakePauseMapPlayerPos;
u32 CScriptHud::ms_iCurrentFakedInteriorHash;
bool CScriptHud::ms_bUseVerySmallInteriorZoom;
bool CScriptHud::ms_bUseVeryLargeInteriorZoom;
bool CScriptHud::bDontDisplayHudOrRadarThisFrame;
bool CScriptHud::bDontZoomMiniMapWhenRunningThisFrame;
bool CScriptHud::bDontTiltMiniMapThisFrame;
bool CScriptHud::bDisablePauseMenuThisFrame = false;
bool CScriptHud::bSuppressPauseMenuRenderThisFrame = false;
bool CScriptHud::bAllowPauseWhenInNotInStateOfPlayThisFrame = false;
float CScriptHud::fFakeMinimapAltimeterHeight = 0.0f;
bool CScriptHud::ms_bColourMinimapAltimeter = false;
eHUD_COLOURS CScriptHud::ms_MinimapAltimeterColor = HUD_COLOUR_PURPLE;
s32 CScriptHud::iFakeWantedLevel = 0;
s32 CScriptHud::iHudMaxHealthDisplay = 0;
s32 CScriptHud::iHudMaxArmourDisplay = 0;
s32 CScriptHud::iHudMaxEnduranceDisplay = 0;
Vector3 CScriptHud::vFakeWantedLevelPos(0,0,0);
bool CScriptHud::bUsePlayerColourInsteadOfTeamColour = false;
bool CScriptHud::bFlashWantedStarDisplay = false;
bool CScriptHud::bForceOnWantedStarFlash = false;
bool CScriptHud::bForceOffWantedStarFlash = false;
bool CScriptHud::bUpdateWantedThreatVisibility = false;
bool CScriptHud::bIsWantedThreatVisible = false;
bool CScriptHud::bUpdateWantedDrainLevel = false;
int CScriptHud::iWantedDrainLevelPercentage = 0;


char CScriptHud::cMultiplayerBriefTitle[TEXT_KEY_SIZE];
char CScriptHud::cMultiplayerBriefContent[TEXT_KEY_SIZE];
s32 CScriptHud::m_playerBlipColourOverride = BLIP_COLOUR_DEFAULT;
bool CScriptHud::ms_bUseAdjustedMouseCoords = false;

u32 CScriptHud::ms_MaxNumberOfTextLinesZeroOrOneNumbers = 0;
u32 CScriptHud::ms_MaxNumberOfTextLinesOneSubstring = 0;
u32 CScriptHud::ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers = 0;


#if ENABLE_LEGACY_SCRIPTED_RT_SUPPORT
grcRenderTarget* CScriptHud::m_offscreenRenderTarget = NULL;
#endif

CDisplayTextFormatting CScriptHud::ms_FormattingForNextDisplayText;

//	Buffer of literal strings - need a distinction between those that can be cleared every frame (DISPLAY_TEXT)
//	and those that should only be cleared when the string can no longer be displayed (PRINT commands)
CLiteralStrings CScriptHud::ScriptLiteralStrings;

u32 CScriptHud::ScriptMessagesLastClearedInFrame;

sRadarMessage CScriptHud::RadarMessage;
sMissionPassedCashMessage CScriptHud::MissionPassedCashMessage;

bool  CScriptHud::ms_bTurnOffMultiplayerWalletCashThisFrame;
bool  CScriptHud::ms_bTurnOffMultiplayerBankCashThisFrame;
bool  CScriptHud::ms_bTurnOnMultiplayerWalletCashThisFrame;
bool  CScriptHud::ms_bTurnOnMultiplayerBankCashThisFrame;
bool  CScriptHud::ms_bAllowDisplayOfMultiplayerCashText;

bool CScriptHud::bDisplayCashStar;

//	bool CScriptHud::bUseMessageFormatting;
//	u16 CScriptHud::MessageCentre;
//	u16 CScriptHud::MessageWidth;

s32 CScriptHud::iCurrentWebpageIdFromActionScript;
s32 CScriptHud::iCurrentWebsiteIdFromActionScript;
s32 CScriptHud::iActionScriptGlobalFlags[MAX_ACTIONSCRIPT_FLAGS];
s32 CScriptHud::iFindNextRadarBlipId;
CDblBuf<CDoubleBufferedIntroRects>	CScriptHud::ms_DBIntroRects;
CDblBuf<CDoubleBufferedIntroTexts>	CScriptHud::ms_DBIntroTexts;
CDblBuf<CDoubleBufferedDrawOrigins> CScriptHud::ms_DBDrawOrigins;

sScaleformComponent CScriptHud::ScriptScaleformMovie[NUM_SCRIPT_SCALEFORM_MOVIES];
s32 CScriptHud::iCurrentRequestedScaleformMovieId = 0;

bool CScriptHud::bHideFrontendMapBlips;
int CScriptHud::NumberOfMiniGamesAllowingNonMiniGameHelpMessages;

CAbovePlayersHeadDisplay CScriptHud::ms_AbovePlayersHeadDisplay;

float CScriptHud::ms_fMiniMapForcedZoomPercentage;
float CScriptHud::ms_fRadarZoomDistanceThisFrame;
s32 CScriptHud::ms_iRadarZoomValue;
bool CScriptHud::bDisplayHud;
CDblBuf<bool> CScriptHud::ms_bDisplayHudWhenNotInStateOfPlayThisFrame;
CDblBuf<bool> CScriptHud::ms_bDisplayHudWhenPausedThisFrame;
bool CScriptHud::bDisplayRadar;
bool CScriptHud::ms_bFakeSpectatorMode = false;

#if __ASSERT
bool CScriptHud::bLockScriptrendering = false;
#endif // __ASSERT

#if __BANK
bool CScriptHud::bDisplayScriptScaleformMovieDebug = false;
#endif

#if !__FINAL
u32 CScriptHud::ms_NextTimeAllowedToPrintContentsOfFullTextArray[SCRIPT_TEXT_MAX_TYPES];
#endif	//	!__FINAL


Vector2 CScriptHud::vMask[2];
bool	CScriptHud::bMaskCreated = false;
bool	CScriptHud::bUsedMaskLastTime = true;

bool CScriptHud::ms_bAddNextMessageToPreviousBriefs = true;
ePreviousBriefOverride CScriptHud::ms_PreviousBriefOverride = PREVIOUS_BRIEF_NO_OVERRIDE;

//	Blend States used when drawing script sprites and rectangles
grcBlendStateHandle CScriptHud::ms_StandardSpriteBlendStateHandle = grcStateBlock::BS_Invalid;
grcBlendStateHandle CScriptHud::ms_EntireScreenMaskBlendStateHandle = grcStateBlock::BS_Invalid;
grcBlendStateHandle CScriptHud::ms_MaskBlendStateHandle = grcStateBlock::BS_Invalid;
grcBlendStateHandle CScriptHud::ms_MaskedSpritesBlendStateHandle = grcStateBlock::BS_Invalid;
grcBlendStateHandle CScriptHud::ms_SpriteWithNoAlphaBlendStateHandle = grcStateBlock::BS_Invalid;

grcBlendStateHandle CScriptHud::ms_ClearAlpha_BS = grcStateBlock::BS_Invalid;
grcBlendStateHandle CScriptHud::ms_SetAlpha_BS = grcStateBlock::BS_Invalid;

grcDepthStencilStateHandle CScriptHud::ms_MarkStencil_DSS = grcStateBlock::DSS_Invalid;
grcDepthStencilStateHandle CScriptHud::ms_UseStencil_DSS = grcStateBlock::DSS_Invalid;

bool CScriptHud::ms_StopLoadingScreen = false;

CBlipFades CScriptHud::ms_BlipFades;

CMultiplayerFogOfWarSavegameDetails CScriptHud::ms_MultiplayerFogOfWarSavegameDetails;


void CAbovePlayersHeadDisplay::Reset(bool bIsConstructing)
{
	for (u32 row_loop = 0; row_loop < MaxNumberOfRows; row_loop++)
	{
		m_TextRows[row_loop][0] = '\0';
	}
	
	m_NumberOfRows = 0;
	m_bIsConstructingAbovePlayersHeadDisplay = bIsConstructing;
}

void CAbovePlayersHeadDisplay::AddRow(const char *pRowToAdd)
{
	if (scriptVerifyf(m_NumberOfRows < MaxNumberOfRows, "CAbovePlayersHeadDisplay::AddRow - no space left to add another row"))
	{
		safecpy(m_TextRows[m_NumberOfRows], pRowToAdd, MaxNumberOfCharactersInOneRow);
		m_NumberOfRows++;						
	}
}

void CAbovePlayersHeadDisplay::FillStringWithRows(char *pStringToFill, u32 MaxLengthOfString)
{
	if (scriptVerifyf(pStringToFill, "CAbovePlayersHeadDisplay::FillStringWithRows - string pointer is NULL"))
	{
		safecpy(pStringToFill, "", MaxLengthOfString);

		for (u32 row_loop = 0; row_loop < m_NumberOfRows; row_loop++)
		{
			if (row_loop > 0)
			{
				safecat(pStringToFill, "~n~", MaxLengthOfString);
			}
			safecat(pStringToFill, m_TextRows[row_loop], MaxLengthOfString);
		}
	}
}

void CDisplayTextFormatting::Reset()
{
	m_ScriptTextColor = CRGBA(225, 225, 225, 255);
//	m_ScriptTextDropShadowColour = CRGBA(0, 0, 0, 255);

	m_ScriptTextXScale = 0.48f;
	m_ScriptTextYScale = 1.12f;	//	0.56f;

	m_ScriptTextWrapStartX = 0.0f;	//	SCREEN_WIDTH;//182.0f;
	m_ScriptTextWrapEndX = 1.0f;	//	SCREEN_WIDTH;//182.0f;
//  m_ScriptTextLineHeightMult = (float) 1.1f;	//	SCREEN_WIDTH;//182.0f;

	m_ScriptTextFontStyle = FONT_STYLE_STANDARD; //_HEADING;

	m_ScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;
	m_bScriptUsingWidescreenAdjustment = false;
#if RSG_PC
	m_StereoFlag = 0;
#endif

	m_ScriptTextJustification = FONT_LEFT;
//	m_ScriptTextProportional = TRUE;

	m_ScriptTextBackgrnd = FALSE;
	m_ScriptTextLeading = 0;
	m_ScriptTextUseTextFileColours = true;

	m_bScriptTextDropShadow = false;
	m_bScriptTextEdge = false;
}

#if __BANK
void CDisplayTextFormatting::OutputDebugInfo(bool bDisplay, float fPosX, float fPosY)
{
	if (CTextFormat::ms_cDebugScriptedString[0] != '\0')
	{
		if (!stricmp(CScriptTextConstruction::GetMainTextLabel(), CTextFormat::ms_cDebugScriptedString))
		{
			Displayf("\n\n*****************************************\n");

			if (bDisplay)
				Displayf("****** DISPLAY_TEXT %s    - (%u)\n", CScriptTextConstruction::GetMainTextLabel(), fwTimer::GetFrameCount());
			else
				Displayf("****** GET_NUMBER_LINES %s    - (%u)\n", CScriptTextConstruction::GetMainTextLabel(), fwTimer::GetFrameCount());

			Displayf("*****************************************\n\n\n");
			Displayf("Justification = %d",  m_ScriptTextJustification);
			Displayf("Wrap = %0.9f, %0.9f", m_ScriptTextWrapStartX, m_ScriptTextWrapEndX);
			Displayf("Backgrnd = %d", (s32)m_ScriptTextBackgrnd);
			Displayf("Leading = %d", m_ScriptTextLeading);
			Displayf("Edge = %d", (s32)m_bScriptTextEdge);
			Displayf("DropShadow = %d", (s32)m_bScriptTextDropShadow);
			Displayf("FontStyle = %d", (u8)m_ScriptTextFontStyle);
			Displayf("Scale = %0.9f, %0.9f", m_ScriptTextXScale, m_ScriptTextYScale);
			Displayf("DisplayAt = %0.9f, %0.9f", fPosX, fPosY);
			Displayf("\n\n*****************************************\n\n\n\n\n");
		}
	}
}
#endif // __BANK

void CDisplayTextFormatting::SetTextLayoutForDraw(CTextLayout &ScriptTextLayout, Vector2 *pvPosition) const
{
	ScriptTextLayout.Default();

#if RSG_PC
	ScriptTextLayout.SetStereo(m_StereoFlag);
#endif
	ScriptTextLayout.SetOrientation(m_ScriptTextJustification);

	ScriptTextLayout.SetBackground(m_ScriptTextBackgrnd);
	ScriptTextLayout.SetLeading(m_ScriptTextLeading);
	ScriptTextLayout.SetBackgroundColor(CRGBA(128, 128, 128, 128));
//	ScriptTextLayout.SetProportional(m_ScriptTextProportional);

	if (m_bScriptTextEdge)
		ScriptTextLayout.SetEdge(m_bScriptTextEdge);
	else
		ScriptTextLayout.SetDropShadow(m_bScriptTextDropShadow);

	ScriptTextLayout.SetStyle((u8) m_ScriptTextFontStyle);

	ScriptTextLayout.SetColor(m_ScriptTextColor);

//	ScriptTextLayout.SetLineHeightMult(m_ScriptTextLineHeightMult);

	ScriptTextLayout.SetUseTextColours(m_ScriptTextUseTextFileColours);

	// deal with widescreen:
	Vector2 vWrapValue(m_ScriptTextWrapStartX, m_ScriptTextWrapEndX);
	Vector2 vScaleValue(m_ScriptTextXScale, m_ScriptTextYScale);

	if (m_bScriptUsingWidescreenAdjustment)
	{
		// adjust taking widescreen into account:
		CHudTools::AdjustForWidescreen(m_ScriptWidescreenFormat, pvPosition, &vScaleValue, &vWrapValue);

		ScriptTextLayout.SetAdjustForNonWidescreen(true);
	}

	ScriptTextLayout.SetWrap(&vWrapValue);
	ScriptTextLayout.SetScale(&vScaleValue);

/*	float fTextBoxWidth;
	
	if (vWrapValue.y > vWrapValue.x)
	{
		fTextBoxWidth = vWrapValue.y-vWrapValue.x;
	}
	else
	{
		fTextBoxWidth = vWrapValue.x-vWrapValue.y;
	}

	*pvPosition = CHudTools::CalculateHudPosition(*pvPosition, Vector2(fTextBoxWidth, (*pvPosition).y + m_ScriptTextYScale), m_ScriptTextAdjustment.alignX, m_ScriptTextAdjustment.alignY);
*/}

float CDisplayTextFormatting::GetCharacterHeightOnScreen()
{
	CTextLayout ScriptTextLayoutForWidth;
	ScriptTextLayoutForWidth.SetStyle((u8)m_ScriptTextFontStyle);
	ScriptTextLayoutForWidth.SetScale(Vector2(m_ScriptTextXScale, m_ScriptTextYScale));
	ScriptTextLayoutForWidth.SetEdge(m_bScriptTextEdge);
//	ScriptTextLayoutForWidth.SetProportional(m_ScriptTextProportional);

	return (ScriptTextLayoutForWidth.GetCharacterHeight());
}

float CDisplayTextFormatting::GetTextPaddingOnScreen()
{
	CTextLayout ScriptTextLayoutForWidth;
	ScriptTextLayoutForWidth.SetScale(Vector2(m_ScriptTextXScale, m_ScriptTextYScale));
	return (ScriptTextLayoutForWidth.GetTextPadding());
}

float CDisplayTextFormatting::GetStringWidthOnScreen(char *pCharacters, bool bIncludeSpaces)
{
	CTextLayout ScriptTextLayoutForWidth;
	ScriptTextLayoutForWidth.SetStyle((u8)m_ScriptTextFontStyle);
	ScriptTextLayoutForWidth.SetScale(Vector2(m_ScriptTextXScale, m_ScriptTextYScale));
	ScriptTextLayoutForWidth.SetEdge(m_bScriptTextEdge);
//	ScriptTextLayoutForWidth.SetProportional(m_ScriptTextProportional);

	// now we have our current scripted formatting set up, we can find out the rendered width:
	return (ScriptTextLayoutForWidth.GetStringWidthOnScreen(pCharacters, bIncludeSpaces));
}

s32 CDisplayTextFormatting::GetNumberOfLinesForString(float DisplayAtX, float DisplayAtY, char *pString)
{
	CTextLayout tempScriptTextLayout;

	tempScriptTextLayout.SetOrientation(m_ScriptTextJustification);

	tempScriptTextLayout.SetWrap(Vector2(m_ScriptTextWrapStartX, m_ScriptTextWrapEndX));
	tempScriptTextLayout.SetBackground(m_ScriptTextBackgrnd);
	tempScriptTextLayout.SetLeading(m_ScriptTextLeading);
//	tempScriptTextLayout.SetProportional(m_ScriptTextProportional);

	if (m_bScriptTextEdge)
		tempScriptTextLayout.SetEdge(m_bScriptTextEdge);
	else
		tempScriptTextLayout.SetDropShadow(m_bScriptTextDropShadow);

	tempScriptTextLayout.SetStyle((u8) m_ScriptTextFontStyle);
	tempScriptTextLayout.SetScale(Vector2(m_ScriptTextXScale, m_ScriptTextYScale));

#if __BANK
	CScriptHud::ms_FormattingForNextDisplayText.OutputDebugInfo(false, DisplayAtX, DisplayAtY);
#endif // __BANK

	// at this point the font code should have enough data from the script variables to calculate the number of lines:
	return tempScriptTextLayout.GetNumberOfLines(Vector2(DisplayAtX, DisplayAtY), pString);
}

void CDisplayTextFormatting::OffsetMargins(float offset)
{
	m_ScriptTextWrapStartX += offset;
	m_ScriptTextWrapEndX += offset;
}



// *********************************** CDisplayTextBaseClass ***********************************

void CDisplayTextBaseClass::Reset(int OrthoRenderID)
{
	m_Formatting.Reset();

	m_ScriptTextLabel[0] = '\0';

	m_RenderID = OrthoRenderID;
	m_IndexOfDrawOrigin = -1;
	m_ScriptTextAtX = 0.0f;
	m_ScriptTextAtY = 0.0f;

    m_ScriptTextDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD;
//	m_ScriptTextDrawProperties |= SCRIPT_GFX_VISIBLE_WHEN_PAUSED;  // for easy debug

    ClearExtraData();
}


void CDisplayTextBaseClass::Initialise(float fPosX, float fPosY, s32 renderID, s32 indexOfDrawOrigin, 
	u32 drawProperties, CDisplayTextFormatting &formattingForText, 
	const char *pMainTextLabel, 
	CNumberWithinMessage *pNumbersArray, u32 numberOfNumbers, 
	CSubStringWithinMessage *pSubStringsArray, u32 numberOfSubStrings)
{
	m_ScriptTextAtX = fPosX;
	m_ScriptTextAtY = fPosY;

	m_RenderID = renderID;
	m_IndexOfDrawOrigin = indexOfDrawOrigin;
	
	m_ScriptTextDrawProperties = drawProperties;

	m_Formatting = formattingForText;
	Assertf(strlen(pMainTextLabel)<SCRIPT_TEXT_LABEL_LENGTH,"Text label %s is too long,use a label of size %d",pMainTextLabel,SCRIPT_TEXT_LABEL_LENGTH-1);
	strncpy(m_ScriptTextLabel, pMainTextLabel, SCRIPT_TEXT_LABEL_LENGTH);

#if __BANK
	if (CScriptDebug::GetOutputScriptDisplayTextCommands())
	{
		scripthudDisplayf("CDisplayTextBaseClass::Initialise - pMainTextLabel=%s, fPosX=%f, fPosY=%f, numberOfNumbers=%u, numberOfSubStrings=%u",
			pMainTextLabel, fPosX, fPosY, numberOfNumbers, numberOfSubStrings);
	}
#endif	//	__BANK

	SetTextDetails(pNumbersArray, numberOfNumbers, 
		pSubStringsArray, numberOfSubStrings);
}



void CDisplayTextBaseClass::Draw(s32 CurrentRenderID, u32 iDrawOrderValue) const
{
	// strip pause flag so we can do a comparison check
	u32 iScriptGfxDrawPropertiesWithoutPauseFlag = m_ScriptTextDrawProperties & (~SCRIPT_GFX_VISIBLE_WHEN_PAUSED);

	bool bRenderThisItem = (iScriptGfxDrawPropertiesWithoutPauseFlag == iDrawOrderValue);

	if (CPauseMenu::IsActive())
		bRenderThisItem &= (m_ScriptTextDrawProperties & SCRIPT_GFX_VISIBLE_WHEN_PAUSED) == SCRIPT_GFX_VISIBLE_WHEN_PAUSED;

	if ( (CurrentRenderID == m_RenderID) && (bRenderThisItem) )
	{
		if (m_ScriptTextLabel[0] != 0)
		{
			Vector2 vPositionValue(m_ScriptTextAtX, m_ScriptTextAtY);

#if SUPPORT_MULTI_MONITOR
			const fwRect& area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
			vPositionValue.Multiply(Vector2(area.GetWidth(), area.GetHeight()));
			vPositionValue += Vector2(area.left, area.bottom);
#endif //SUPPORT_MULTI_MONITOR

			if (m_IndexOfDrawOrigin >= 0)
			{
				Vector2 vOrigin;
				int idx = CScriptHud::GetReadbufferIndex();
				if (CScriptHud::GetDrawOrigins()[idx].GetScreenCoords(m_IndexOfDrawOrigin, vOrigin))
				{
					vPositionValue += vOrigin;
				}
				else
				{
					return;	//	Is it okay to return early?
				}
			}

			CTextLayout ScriptTextLayout;  // to setup the new script layouts this frame

			m_Formatting.SetTextLayoutForDraw(ScriptTextLayout, &vPositionValue);

			char *pMainText = TheText.Get(m_ScriptTextLabel);

			//	Willie said that the mission descriptions in the 'M' Mission menu are longer than 400 characters
			//	so I had to increase the maximum length
			const u32 MAX_CHARS_IN_DISPLAY_TEXT_MESSAGE = (3 * MAX_CHARS_IN_MESSAGE);

#if __ASSERT
			u32 string_length = CTextConversion::GetByteCount(pMainText);
			scriptAssertf(string_length < MAX_CHARS_IN_DISPLAY_TEXT_MESSAGE, "CDisplayText::Draw - %s is too long. %d characters", m_ScriptTextLabel, string_length);
#endif

			char TempString[MAX_CHARS_IN_DISPLAY_TEXT_MESSAGE];

			CreateStringToDisplay(pMainText, TempString, MAX_CHARS_IN_DISPLAY_TEXT_MESSAGE);

#if __WIN32PC
			CMessages::InsertPlayerControlKeysInString(TempString);
#endif // __WIN32PC

#if __BANK
			if (CScriptDebug::GetOutputScriptDisplayTextCommands())
			{
				scripthudDisplayf("CDisplayTextBaseClass::Draw - String is %s", TempString);
			}
#endif	//	__BANK

			ScriptTextLayout.Render(&vPositionValue, TempString);
		}
	}
}


#if !__FINAL
void CDisplayTextBaseClass::DebugPrintContents()
{
	scriptDisplayf("m_ScriptTextLabel = %s", m_ScriptTextLabel);
}
#endif	//	!__FINAL


/*
void CDisplayTextBaseClass::ClearExtraData()
{
	//	Nothing to do here
}


void CDisplayTextBaseClass::SetTextDetails(const CNumberWithinMessage* UNUSED_PARAM(pArrayOfNumbers), u32 ASSERT_ONLY(SizeOfNumberArray), 
	const CSubStringWithinMessage* UNUSED_PARAM(pArrayOfSubStrings), u32 ASSERT_ONLY(SizeOfSubStringArray))
{
	ClearExtraData();

//	scriptAssertf(pArrayOfNumbers == NULL, "CDisplayTextBaseClass::SetTextDetails - expected pArrayOfNumbers to be NULL");
	scriptAssertf(SizeOfNumberArray == 0, "CDisplayTextBaseClass::SetTextDetails - expected SizeOfNumberArray to be 0");

//	scriptAssertf(pArrayOfSubStrings == NULL, "CDisplayTextBaseClass::SetTextDetails - expected pArrayOfSubStrings to be NULL");
	scriptAssertf(SizeOfSubStringArray == 0, "CDisplayTextBaseClass::SetTextDetails - expected SizeOfSubStringArray to be 0");
}


void CDisplayTextBaseClass::CreateStringToDisplay(const char *pMainText, char *pOutString, s32 maxLengthOfOutString) const
{
	CTextConversion::charStrcpy(pOutString, pMainText, maxLengthOfOutString - 1);
}
*/

// *********************************** CDisplayTextZeroOrOneNumbers ***********************************

void CDisplayTextZeroOrOneNumbers::ClearExtraData()
{
	m_NumberWithinMessage.Clear();
}

void CDisplayTextZeroOrOneNumbers::SetTextDetails(const CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
	const CSubStringWithinMessage* UNUSED_PARAM(pArrayOfSubStrings), u32 ASSERT_ONLY(SizeOfSubStringArray))
{
	ClearExtraData();

	if (scriptVerifyf(SizeOfNumberArray < 2, "CDisplayTextZeroOrOneNumbers::SetTextDetails - expected SizeOfNumberArray to be less than 2"))
	{
		if (scriptVerifyf(pArrayOfNumbers, "CDisplayTextZeroOrOneNumbers::SetTextDetails - didn't expect pArrayOfNumbers to be NULL"))
		{
			m_NumberWithinMessage = pArrayOfNumbers[0];
		}
	}

//	scriptAssertf(pArrayOfSubStrings == NULL, "CDisplayTextZeroOrOneNumbers::SetTextDetails - expected pArrayOfSubStrings to be NULL");
	scriptAssertf(SizeOfSubStringArray == 0, "CDisplayTextZeroOrOneNumbers::SetTextDetails - expected SizeOfSubStringArray to be 0");
}

void CDisplayTextZeroOrOneNumbers::CreateStringToDisplay(const char *pMainText, char *pOutString, s32 maxLengthOfOutString) const
{
	CMessages::InsertNumbersAndSubStringsIntoString(pMainText, 
		&m_NumberWithinMessage, 1, 
		NULL, 0, 
		pOutString, maxLengthOfOutString);
}

#if !__FINAL
void CDisplayTextZeroOrOneNumbers::DebugPrintContents()
{
	CDisplayTextBaseClass::DebugPrintContents();
	m_NumberWithinMessage.DebugPrintContents();
}
#endif	//	!__FINAL

// *********************************** CDisplayTextOneSubstring ***********************************

void CDisplayTextOneSubstring::ClearExtraData()
{
	m_SubstringWithinMessage.Clear(false, false);	//	I think false, false is correct. This literal string shouldn't be persistent.
}

void CDisplayTextOneSubstring::SetTextDetails(const CNumberWithinMessage* UNUSED_PARAM(pArrayOfNumbers), u32 ASSERT_ONLY(SizeOfNumberArray), 
											  const CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray)
{
	ClearExtraData();

//	scriptAssertf(pArrayOfNumbers == NULL, "CDisplayTextOneSubstring::SetTextDetails - expected pArrayOfNumbers to be NULL");
	scriptAssertf(SizeOfNumberArray == 0, "CDisplayTextOneSubstring::SetTextDetails - expected SizeOfNumberArray to be 0");

	if (scriptVerifyf(SizeOfSubStringArray == 1, "CDisplayTextOneSubstring::SetTextDetails - expected SizeOfSubStringArray to be 1"))
	{
		if (scriptVerifyf(pArrayOfSubStrings, "CDisplayTextOneSubstring::SetTextDetails - didn't expect pArrayOfSubStrings to be NULL"))
		{
			m_SubstringWithinMessage = pArrayOfSubStrings[0];
		}
	}
}

void CDisplayTextOneSubstring::CreateStringToDisplay(const char *pMainText, char *pOutString, s32 maxLengthOfOutString) const
{
	CMessages::InsertNumbersAndSubStringsIntoString(pMainText, 
		NULL, 0, 
		&m_SubstringWithinMessage, 1, 
		pOutString, maxLengthOfOutString);
}


#if !__FINAL
void CDisplayTextOneSubstring::DebugPrintContents()
{
	CDisplayTextBaseClass::DebugPrintContents();

	const char *pSubString = m_SubstringWithinMessage.GetString();
	if (pSubString)
	{
		scriptDisplayf("Substring = %s", pSubString);
	}
}
#endif	//	!__FINAL

// *********************************** CDisplayTextFourSubstringsThreeNumbers ***********************************

void CDisplayTextFourSubstringsThreeNumbers::ClearExtraData()
{
	for (u32 number_loop = 0; number_loop < MAX_NUMBERS_IN_DISPLAY_TEXT; number_loop++)
	{
		m_NumbersWithinMessage[number_loop].Clear();
	}

	for (u32 sub_string_loop = 0; sub_string_loop < MAX_SUBSTRINGS_IN_DISPLAY_TEXT; sub_string_loop++)
	{
		m_SubstringsWithinMessage[sub_string_loop].Clear(false, false);	//	I think false, false is correct. These literal strings shouldn't be persistent.
	}
}

void CDisplayTextFourSubstringsThreeNumbers::SetTextDetails(const CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
					const CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray)
{
	ClearExtraData();

	if (pArrayOfNumbers)
	{
		if (Verifyf(SizeOfNumberArray <= MAX_NUMBERS_IN_DISPLAY_TEXT, "CDisplayTextFourSubstringsThreeNumbers::SetTextDetails - new text has more than %d numbers", MAX_NUMBERS_IN_DISPLAY_TEXT))
		{
			for (u32 number_loop = 0; number_loop < SizeOfNumberArray; number_loop++)
			{
				m_NumbersWithinMessage[number_loop] = pArrayOfNumbers[number_loop];
			}
		}
	}

	if (pArrayOfSubStrings)
	{
		if (Verifyf(SizeOfSubStringArray <= MAX_SUBSTRINGS_IN_DISPLAY_TEXT, "CDisplayTextFourSubstringsThreeNumbers::SetTextDetails - new text has more than %d substrings", MAX_SUBSTRINGS_IN_DISPLAY_TEXT))
		{
			for (u32 string_loop = 0; string_loop < SizeOfSubStringArray; string_loop++)
			{
				m_SubstringsWithinMessage[string_loop] = pArrayOfSubStrings[string_loop];
			}
		}
	}
}

void CDisplayTextFourSubstringsThreeNumbers::CreateStringToDisplay(const char *pMainText, char *pOutString, s32 maxLengthOfOutString) const
{
	CMessages::InsertNumbersAndSubStringsIntoString(pMainText, 
		m_NumbersWithinMessage, MAX_NUMBERS_IN_DISPLAY_TEXT, 
		m_SubstringsWithinMessage, MAX_SUBSTRINGS_IN_DISPLAY_TEXT, 
		pOutString, maxLengthOfOutString);
}


#if !__FINAL
void CDisplayTextFourSubstringsThreeNumbers::DebugPrintContents()
{
	CDisplayTextBaseClass::DebugPrintContents();

	for (u32 subStringLoop = 0; subStringLoop < MAX_SUBSTRINGS_IN_DISPLAY_TEXT; subStringLoop++)
	{
		const char *pSubString = m_SubstringsWithinMessage[subStringLoop].GetString();
		if (pSubString)
		{
			scriptDisplayf("Substring = %s", pSubString);
		}
	}

	for (u32 numbersLoop = 0; numbersLoop < MAX_NUMBERS_IN_DISPLAY_TEXT; numbersLoop++)
	{
		m_NumbersWithinMessage[numbersLoop].DebugPrintContents();
	}
}
#endif	//	!__FINAL


// *********************************** intro_script_rectangle ***********************************

void intro_script_rectangle::Reset(bool bFullReset, int OrthoRenderID)
{
	if (bFullReset)
	{
		m_ScaleformMovieId = -1;		// -1 = no scaleform movie

		m_pTexture = NULL;		//	NULL = solid colour, no texture
		m_ScriptRectMin.x = 0.0f;
		m_ScriptRectMin.y = 0.0f;
		m_ScriptRectMax.x = 0.0f;
		m_ScriptRectMax.y = 0.0f;
		m_ScriptRectRotationOrWidth = 0.0f;
		m_RenderID = OrthoRenderID;
		m_ScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;
		m_ScriptRectColour = Color32(255, 255, 255, 255);
		m_IndexOfDrawOrigin = -1;

		m_ScriptRectTexUCoordX = 0.0f;
		m_ScriptRectTexUCoordY = 0.0f;
		m_ScriptRectTexVCoordX = 1.0f;
		m_ScriptRectTexVCoordY = 1.0f;
	}

	m_eGfxType = SCRIPT_GFX_NONE;
	m_ScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD;
//	m_ScriptGfxDrawProperties |= SCRIPT_GFX_VISIBLE_WHEN_PAUSED;  // for easy debug
	m_bUseMask = false;
	m_bInvertMask = false;
	m_bUseMovie = false;
	m_bInterface = true;
	m_bARAwareX = false;
}

void intro_script_rectangle::SetCommonValues(float fMinX, float fMinY, float fMaxX, float fMaxY)
{
	m_pTexture = NULL;
	m_ScriptRectRotationOrWidth = 0.0f;

	m_RenderID = CScriptHud::scriptTextRenderID;
	m_ScriptWidescreenFormat = CScriptHud::CurrentScriptWidescreenFormat;
	m_ScriptGfxDrawProperties = CScriptHud::ms_iCurrentScriptGfxDrawProperties;
	m_IndexOfDrawOrigin = CScriptHud::ms_IndexOfDrawOrigin;
	m_bUseMask = CScriptHud::ms_bUseMaskForNextSprite;
	m_bInvertMask = CScriptHud::ms_bInvertMaskForNextSprite;
	CScriptHud::ms_bUseMaskForNextSprite = false;
	CScriptHud::ms_bInvertMaskForNextSprite = false;

#if __BANK
	if (CScriptDebug::GetDebugMaskCommands())
	{
		if (m_bUseMask)
		{
			scriptDisplayf("intro_script_rectangle::SetCommonValues - m_bUseMask is TRUE. m_bInvertMask = %d, m_RenderID = %d, m_ScriptGfxDrawProperties = %d", m_bInvertMask, m_RenderID, m_ScriptGfxDrawProperties);
		}
	}
#endif	//	__BANK

	m_ScriptRectMin.x = fMinX;
	m_ScriptRectMin.y = fMinY;
	m_ScriptRectMax.x = fMaxX;
	m_ScriptRectMax.y = fMaxY;

	Vector2 vPos(m_ScriptRectMin.x, m_ScriptRectMin.y);
	Vector2 vSize(m_ScriptRectMax.x - m_ScriptRectMin.x, m_ScriptRectMax.y - m_ScriptRectMin.y);
	bool bDoScriptSuperWidescreenAdjustment = true;

	if (m_ScriptWidescreenFormat != WIDESCREEN_FORMAT_STRETCH)
	{
		if (m_ScriptRectMin.x != 0.0f || m_ScriptRectMin.y != 0.0f || m_ScriptRectMax.x != 1.0f || m_ScriptRectMax.y != 1.0f)  // dont scale for widescreen if it has been set already at full FULLSCREEN
		{
			if (CScriptHud::ms_CurrentScriptGfxAlignment.m_alignX != 'I')  // if we have an alignment on the X then we only want to adjust for widescreen with size only
			{
				m_ScriptWidescreenFormat = WIDESCREEN_FORMAT_SIZE_ONLY;
			}

			CHudTools::AdjustForWidescreen(m_ScriptWidescreenFormat, &vPos, &vSize, NULL);
		}
	}
	else if (CScriptHud::ms_bAdjustNextPosSize)
	{
		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(WIDESCREEN_FORMAT_AUTO, &vPos, &vSize);
		bDoScriptSuperWidescreenAdjustment = false;	// This adjustment already happens in AdjustNormalized

		CScriptHud::ms_bAdjustNextPosSize = false;
	}

	// work out the difference adjustment and apply the diff to the original values
	Vector2 vNewPos = CScriptHud::ms_CurrentScriptGfxAlignment.CalculateHudPosition(vPos, vSize, bDoScriptSuperWidescreenAdjustment);

	m_ScriptRectMin = vNewPos;
	m_ScriptRectMax = vNewPos + vSize;

	m_bUseMovie = false;
}

Vector2 CHudAlignment::CalculateHudPosition(Vector2 vPos)
{
	// apply alignment offset and size changes
	vPos += m_offset;
	return CHudTools::CalculateHudPosition(vPos, m_size, m_alignX, m_alignY, true);
}

Vector2 CHudAlignment::CalculateHudPosition(Vector2 vPos, Vector2 vSize, bool bDoScriptAdjustment/* = true*/)
{
	// apply alignment offset and size changes
	vPos += m_offset;
	if(m_size.x > 0.0f)
		vSize.x = m_size.x;
	if(m_size.y > 0.0f)
		vSize.y = m_size.y;

	return CHudTools::CalculateHudPosition(vPos, vSize, m_alignX, m_alignY, bDoScriptAdjustment);
}


void CDoubleBufferedDrawOrigins::Initialise()
{
	//	Should I be clearing the Origins array here too?
	NumberOfDrawOriginsThisFrame = 0;
}


s32 CDoubleBufferedDrawOrigins::GetNextFreeIndex()
{
	if (NumberOfDrawOriginsThisFrame < NUM_DRAW_ORIGINS)
	{
		NumberOfDrawOriginsThisFrame++;
		return (NumberOfDrawOriginsThisFrame-1);
	}

	return -1;
}


void CDoubleBufferedDrawOrigins::Set(s32 NewOriginIndex, Vector3 &vOrigin, bool bIs2d)
{
	if (Verifyf( (NewOriginIndex >= 0) && (NewOriginIndex < NumberOfDrawOriginsThisFrame), "CDoubleBufferedDrawOrigins::Set - array index is out of range"))
	{
		Origins[NewOriginIndex].Set(vOrigin, bIs2d);
	}
}


bool CDoubleBufferedDrawOrigins::GetScreenCoords(s32 OriginIndex, Vector2 &vReturnScreenCoords) const
{
	if (Verifyf( (OriginIndex >= 0) && (OriginIndex < NumberOfDrawOriginsThisFrame), "CDoubleBufferedDrawOrigins::GetScreenCoords - array index %d is out of range (0 - %u)", OriginIndex, NumberOfDrawOriginsThisFrame))
	{
		return Origins[OriginIndex].GetScreenCoords(vReturnScreenCoords);
	}

	return false;
}



void CDoubleBufferedIntroRects::Initialise(bool bResetArray)
{
	if (bResetArray)
	{
		for (u32 Counter = 0; Counter < TOTAL_NUM_SCRIPT_RECTANGLES; Counter++)
		{
			IntroRectangles[Counter].Reset(true, CRenderTargetMgr::RTI_MainScreen);
		}
	}

	NumberOfIntroReleaseRectanglesThisFrame = 0;
#if !__FINAL
	NumberOfIntroDebugRectanglesThisFrame = 0;
#endif
}


#if __BANK
void CDoubleBufferedIntroRects::OutputAllValuesForDebug()
{
	if (CScriptDebug::GetOutputListOfAllScriptSpritesWhenLimitIsHit())
	{
		u32 loop = 0;

#if !__FINAL
		Displayf("NumberOfIntroDebugRectanglesThisFrame = %d", NumberOfIntroDebugRectanglesThisFrame);

		for (loop = 0; loop < NumberOfIntroDebugRectanglesThisFrame; loop++)
		{
			Displayf("Debug rectangle %d = ", loop);
			IntroRectangles[NUM_SCRIPT_RELEASE_RECTANGLES + loop].OutputValuesOfMembersForDebug();
		}
#endif

		Displayf("NumberOfIntroReleaseRectanglesThisFrame = %d", NumberOfIntroReleaseRectanglesThisFrame);

		for (loop = 0; loop < NumberOfIntroReleaseRectanglesThisFrame; loop++)
		{
			Displayf("Release rectangle %d = ", loop);
			IntroRectangles[loop].OutputValuesOfMembersForDebug();
		}
	}
}
#endif	//	__BANK


intro_script_rectangle *CDoubleBufferedIntroRects::GetNextFree()
{
	intro_script_rectangle *pReturnRect = NULL;

	scriptAssertf(!CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CDoubleBufferedIntroRects::GetNextFree - didn't expect this to be called by the render thread");
	scriptAssertf(CScriptHud::bLockScriptrendering == false, "CDoubleBufferedIntroRects::GetNextFree - using script rendering while in a pause menu script");

#if !__FINAL
	bool bUseDebugPortionOfArray = false;
	if (CTheScripts::GetCurrentGtaScriptThread())
	{
		if (CTheScripts::GetCurrentGtaScriptThread()->UseDebugPortionOfScriptTextArrays())
		{
			bUseDebugPortionOfArray = true;
		}
	}

	if (bUseDebugPortionOfArray)
	{
		if (NumberOfIntroDebugRectanglesThisFrame < NUM_SCRIPT_DEBUG_RECTANGLES)
		{
			pReturnRect = &IntroRectangles[NUM_SCRIPT_RELEASE_RECTANGLES + NumberOfIntroDebugRectanglesThisFrame];
			NumberOfIntroDebugRectanglesThisFrame++;
		}
		else
		{
#if __BANK
			OutputAllValuesForDebug();
#endif
			scriptAssertf(0, "CDoubleBufferedIntroRects::GetNextFree - have already drawn %d debug rectangles this frame", NUM_SCRIPT_DEBUG_RECTANGLES);
		}
	}
	else
#endif	//	!__FINAL
	{
		if (NumberOfIntroReleaseRectanglesThisFrame < NUM_SCRIPT_RELEASE_RECTANGLES)
		{
			pReturnRect = &IntroRectangles[NumberOfIntroReleaseRectanglesThisFrame];
			NumberOfIntroReleaseRectanglesThisFrame++;
		}
		else
		{
#if __BANK
			OutputAllValuesForDebug();
#endif
			scriptAssertf(0, "CDoubleBufferedIntroRects::GetNextFree - have already drawn %d release rectangles this frame", NUM_SCRIPT_RELEASE_RECTANGLES);
		}
	}

	return pReturnRect;
}


void CDoubleBufferedIntroRects::Draw(int CurrentRenderID, u32 iDrawOrderValue) const
{
	u32 loop = 0;

	for (loop = 0; loop < NumberOfIntroReleaseRectanglesThisFrame; loop++)
	{
		IntroRectangles[loop].Draw(CurrentRenderID, iDrawOrderValue);
	}

#if !__FINAL
	for (loop = NUM_SCRIPT_RELEASE_RECTANGLES; loop < NUM_SCRIPT_RELEASE_RECTANGLES + NumberOfIntroDebugRectanglesThisFrame; loop++)
	{
		IntroRectangles[loop].Draw(CurrentRenderID, iDrawOrderValue);
	}
#endif
}

void CDoubleBufferedIntroRects::PushRefCount() const 
{
	u32 loop = 0;

	for (loop = 0; loop < NumberOfIntroReleaseRectanglesThisFrame; loop++)
	{
		IntroRectangles[loop].PushRefCount();
	}

#if !__FINAL
	for (loop = NUM_SCRIPT_RELEASE_RECTANGLES; loop < NUM_SCRIPT_RELEASE_RECTANGLES + NumberOfIntroDebugRectanglesThisFrame; loop++)
	{
		IntroRectangles[loop].PushRefCount();
	}
#endif
}



CDoubleBufferedIntroTexts::CDoubleBufferedIntroTexts() : m_numberOfTextLinesZeroOrOneNumbersThisFrame(0), m_numberOfTextLinesOneSubstringThisFrame(0), m_numberOfTextLinesFourSubstringsThreeNumbersThisFrame(0)
{
#if ENABLE_DEBUG_HEAP
	m_introDebugTextLines = NULL;
	m_numberOfIntroDebugTextLinesThisFrame = 0;
#endif
}

void CDoubleBufferedIntroTexts::Initialise(bool bResetArray)
{
	if (bResetArray)
	{
		u32 loop = 0;
// 		for (loop = 0; loop < CScriptHud::ms_MaxNumberOfTextLinesNoSubstringsNoNumbers; loop++)
// 		{
// 			m_pTextLinesNoSubstringsNoNumbers[loop].Reset(CRenderTargetMgr::RTI_MainScreen);
// 		}

		for (loop = 0; loop < CScriptHud::ms_MaxNumberOfTextLinesZeroOrOneNumbers; loop++)
		{
			m_pTextLinesZeroOrOneNumbers[loop].Reset(CRenderTargetMgr::RTI_MainScreen);
		}

		for (loop = 0; loop < CScriptHud::ms_MaxNumberOfTextLinesOneSubstring; loop++)
		{
			m_pTextLinesOneSubstring[loop].Reset(CRenderTargetMgr::RTI_MainScreen);
		}

		for (loop = 0; loop < CScriptHud::ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers; loop++)
		{
			m_pTextLinesFourSubstringsThreeNumbers[loop].Reset(CRenderTargetMgr::RTI_MainScreen);
		}

#if ENABLE_DEBUG_HEAP
		for (loop = 0; loop < SCRIPT_TEXT_NUM_DEBUG_LINES; loop++)
		{
			m_introDebugTextLines[loop].Reset(CRenderTargetMgr::RTI_MainScreen);
		}
#endif
	}

	m_numberOfTextLinesZeroOrOneNumbersThisFrame = 0;
	m_numberOfTextLinesOneSubstringThisFrame = 0;
	m_numberOfTextLinesFourSubstringsThreeNumbersThisFrame = 0;

#if ENABLE_DEBUG_HEAP
	m_numberOfIntroDebugTextLinesThisFrame = 0;
#endif
}

/*
CDisplayText *CDoubleBufferedIntroTexts::GetNextFree(bool ENABLE_DEBUG_HEAP_ONLY(bUseDebugPortionOfArray))
{
	CDisplayText *pReturnLine = NULL;

#if ENABLE_DEBUG_HEAP
	if (bUseDebugPortionOfArray)
	{
		if (m_numberOfIntroDebugTextLinesThisFrame < SCRIPT_TEXT_NUM_DEBUG_LINES)
		{
			pReturnLine = &m_introDebugTextLines[m_numberOfIntroDebugTextLinesThisFrame];
			m_numberOfIntroDebugTextLinesThisFrame++;
		}
	}
	else
#endif	//	ENABLE_DEBUG_HEAP
	{
		if (m_numberOfIntroReleaseTextLinesThisFrame < SCRIPT_TEXT_NUM_RELEASE_LINES)
		{
			pReturnLine = &m_introTextLines[m_numberOfIntroReleaseTextLinesThisFrame];
			m_numberOfIntroReleaseTextLinesThisFrame++;
		}
	}

#if !__FINAL
	if (pReturnLine == NULL)
	{
		if (fwTimer::GetGameTimer().GetTimeInMilliseconds() > CScriptHud::ms_NextTimeAllowedToPrintContentsOfFullTextArray)
		{
			CScriptHud::ms_NextTimeAllowedToPrintContentsOfFullTextArray = fwTimer::GetGameTimer().GetTimeInMilliseconds() + 7000;

			if (bUseDebugPortionOfArray)
			{
				scriptDisplayf("CDoubleBufferedIntroTexts::GetNextFree - all %d debug text slots have been used", SCRIPT_TEXT_NUM_DEBUG_LINES);
				for (u32 i = 0; i < SCRIPT_TEXT_NUM_DEBUG_LINES; i++)
				{
					m_introDebugTextLines[i].DebugPrintContents();
				}
			}
			else
			{
				scriptDisplayf("CDoubleBufferedIntroTexts::GetNextFree - all %d release text slots have been used", SCRIPT_TEXT_NUM_RELEASE_LINES);
				for (u32 i = 0; i < SCRIPT_TEXT_NUM_RELEASE_LINES; i++)
				{
					m_introTextLines[i].DebugPrintContents();
				}
			}
		}
	}
#endif	//	!__FINAL

	return pReturnLine;
}
*/

void CDoubleBufferedIntroTexts::AddTextLine(float fPosX, float fPosY, s32 renderID, s32 indexOfDrawOrigin, 
	u32 drawProperties, CDisplayTextFormatting &formattingForText, 
	const char *pMainTextLabel, 
	CNumberWithinMessage *pNumbersArray, u32 numberOfNumbers, 
	CSubStringWithinMessage *pSubStringsArray, u32 numberOfSubStrings, 
	bool ENABLE_DEBUG_HEAP_ONLY(bUseDebugDisplayTextLines))
{
	CDisplayTextBaseClass *pNewTextLine = NULL;

#if !__FINAL
	CScriptHud::eTypeOfScriptText text_type = CScriptHud::SCRIPT_TEXT_FOUR_SUBSTRINGS_THREE_NUMBERS;
#endif	//	!__FINAL

#if ENABLE_DEBUG_HEAP
	if (bUseDebugDisplayTextLines)
	{
#if !__FINAL
		text_type = CScriptHud::SCRIPT_TEXT_DEBUG;
#endif	//	!__FINAL
		if (m_numberOfIntroDebugTextLinesThisFrame < SCRIPT_TEXT_NUM_DEBUG_LINES)
		{
			pNewTextLine = &m_introDebugTextLines[m_numberOfIntroDebugTextLinesThisFrame];
			m_numberOfIntroDebugTextLinesThisFrame++;
		}
	}
	else
#endif	//	ENABLE_DEBUG_HEAP
	{
// 		if ( (numberOfNumbers == 0) && (numberOfSubStrings == 0) )
// 		{
// 			if (m_numberOfTextLinesNoSubstringsNoNumbersThisFrame < CScriptHud::ms_MaxNumberOfTextLinesNoSubstringsNoNumbers)
// 			{
// 				pNewTextLine = &m_pTextLinesNoSubstringsNoNumbers[m_numberOfTextLinesNoSubstringsNoNumbersThisFrame];
// 				m_numberOfTextLinesNoSubstringsNoNumbersThisFrame++;
// 			}
// 		}
		if ( (numberOfNumbers < 2) && (numberOfSubStrings == 0) )
		{
#if !__FINAL
			text_type = CScriptHud::SCRIPT_TEXT_ZERO_OR_ONE_NUMBERS;
#endif	//	!__FINAL

			if (m_numberOfTextLinesZeroOrOneNumbersThisFrame < CScriptHud::ms_MaxNumberOfTextLinesZeroOrOneNumbers)
			{
				pNewTextLine = &m_pTextLinesZeroOrOneNumbers[m_numberOfTextLinesZeroOrOneNumbersThisFrame];
				m_numberOfTextLinesZeroOrOneNumbersThisFrame++;
			}
		}
		else if ( (numberOfNumbers == 0) && (numberOfSubStrings == 1) )
		{
#if !__FINAL
			text_type = CScriptHud::SCRIPT_TEXT_ONE_SUBSTRING;
#endif	//	!__FINAL

			if (m_numberOfTextLinesOneSubstringThisFrame < CScriptHud::ms_MaxNumberOfTextLinesOneSubstring)
			{
				pNewTextLine = &m_pTextLinesOneSubstring[m_numberOfTextLinesOneSubstringThisFrame];
				m_numberOfTextLinesOneSubstringThisFrame++;
			}
		}
		else
		{	//	Use the structure which can contain up to 4 substrings and 3 numbers
#if !__FINAL
			text_type = CScriptHud::SCRIPT_TEXT_FOUR_SUBSTRINGS_THREE_NUMBERS;
#endif	//	!__FINAL

			if (m_numberOfTextLinesFourSubstringsThreeNumbersThisFrame < CScriptHud::ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers)
			{
				pNewTextLine = &m_pTextLinesFourSubstringsThreeNumbers[m_numberOfTextLinesFourSubstringsThreeNumbersThisFrame];
				m_numberOfTextLinesFourSubstringsThreeNumbersThisFrame++;
			}
		}
	}


	if (pNewTextLine)
	{
		pNewTextLine->Initialise(fPosX, fPosY, renderID, indexOfDrawOrigin, 
			drawProperties, formattingForText, 
			pMainTextLabel, 
			pNumbersArray, numberOfNumbers, 
			pSubStringsArray, numberOfSubStrings);
	}
#if !__FINAL
	else
	{
		if (fwTimer::GetGameTimer().GetTimeInMilliseconds() > CScriptHud::ms_NextTimeAllowedToPrintContentsOfFullTextArray[text_type])
		{
			CScriptHud::ms_NextTimeAllowedToPrintContentsOfFullTextArray[text_type] = fwTimer::GetGameTimer().GetTimeInMilliseconds() + 7000;

#if ENABLE_DEBUG_HEAP
			if (bUseDebugDisplayTextLines)
			{
				scriptDisplayf("CDoubleBufferedIntroTexts::AddTextLine - all %d debug text slots have been used", SCRIPT_TEXT_NUM_DEBUG_LINES);
				for (u32 i = 0; i < SCRIPT_TEXT_NUM_DEBUG_LINES; i++)
				{
					m_introDebugTextLines[i].DebugPrintContents();
				}
			}
			else
#endif
			{
// 				if ( (numberOfNumbers == 0) && (numberOfSubStrings == 0) )
// 				{
// 					scriptDisplayf("CDoubleBufferedIntroTexts::GetNextFree - all %u TextLinesNoSubstringsNoNumbers slots have been used", CScriptHud::ms_MaxNumberOfTextLinesNoSubstringsNoNumbers);
// 					for (u32 i = 0; i < CScriptHud::ms_MaxNumberOfTextLinesNoSubstringsNoNumbers; i++)
// 					{
// 						m_pTextLinesNoSubstringsNoNumbers[i].DebugPrintContents();
// 					}
// 				}
				if ( (numberOfNumbers < 2) && (numberOfSubStrings == 0) )
				{
					scriptDisplayf("CDoubleBufferedIntroTexts::GetNextFree - all %u TextLinesZeroOrOneNumbers slots have been used", CScriptHud::ms_MaxNumberOfTextLinesZeroOrOneNumbers);
					for (u32 i = 0; i < CScriptHud::ms_MaxNumberOfTextLinesZeroOrOneNumbers; i++)
					{
						m_pTextLinesZeroOrOneNumbers[i].DebugPrintContents();
					}
				}
				else if ( (numberOfNumbers == 0) && (numberOfSubStrings == 1) )
				{
					scriptDisplayf("CDoubleBufferedIntroTexts::GetNextFree - all %u TextLinesOneSubstring slots have been used", CScriptHud::ms_MaxNumberOfTextLinesOneSubstring);
					for (u32 i = 0; i < CScriptHud::ms_MaxNumberOfTextLinesOneSubstring; i++)
					{
						m_pTextLinesOneSubstring[i].DebugPrintContents();
					}
				}
				else
				{
					scriptDisplayf("CDoubleBufferedIntroTexts::GetNextFree - all %u TextLinesFourSubstringsThreeNumbers slots have been used", CScriptHud::ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers);
					for (u32 i = 0; i < CScriptHud::ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers; i++)
					{
						m_pTextLinesFourSubstringsThreeNumbers[i].DebugPrintContents();
					}
				}
			}
		}
	}
#endif	//	!__FINAL
}


void CDoubleBufferedIntroTexts::Draw(int CurrentRenderID, u32 iDrawOrderValue) const
{
#if __BANK
	if (CScriptDebug::GetOutputScriptDisplayTextCommands())
	{
		scripthudDisplayf("CDoubleBufferedIntroTexts::Draw - Start");
	}
#endif	//	__BANK

// 	for (u32 i = 0; i < m_numberOfTextLinesNoSubstringsNoNumbersThisFrame; i++)
// 	{
// 		m_pTextLinesNoSubstringsNoNumbers[i].Draw(CurrentRenderID, iDrawOrderValue);
// 	}
	
	for (u32 i = 0; i < m_numberOfTextLinesZeroOrOneNumbersThisFrame; i++)
	{
		m_pTextLinesZeroOrOneNumbers[i].Draw(CurrentRenderID, iDrawOrderValue);
	}
	
	for (u32 i = 0; i < m_numberOfTextLinesOneSubstringThisFrame; i++)
	{
		m_pTextLinesOneSubstring[i].Draw(CurrentRenderID, iDrawOrderValue);
	}
	
	for (u32 i = 0; i < m_numberOfTextLinesFourSubstringsThreeNumbersThisFrame; i++)
	{
		m_pTextLinesFourSubstringsThreeNumbers[i].Draw(CurrentRenderID, iDrawOrderValue);
	}

#if ENABLE_DEBUG_HEAP
	for (u32 i = 0; i < m_numberOfIntroDebugTextLinesThisFrame; i++)
	{
		m_introDebugTextLines[i].Draw(CurrentRenderID, iDrawOrderValue);
	}
#endif

#if __BANK
	if (CScriptDebug::GetOutputScriptDisplayTextCommands())
	{
		scripthudDisplayf("CDoubleBufferedIntroTexts::Draw - End");
	}
#endif	//	__BANK
}

void CDoubleBufferedIntroTexts::AllocateMemoryForAllTextLines()
{
// 	if (NULL == m_pTextLinesNoSubstringsNoNumbers)
// 	{			
// 		m_pTextLinesNoSubstringsNoNumbers = rage_new CDisplayTextNoSubstringsNoNumbers[CScriptHud::ms_MaxNumberOfTextLinesNoSubstringsNoNumbers];
// 		if (!m_pTextLinesNoSubstringsNoNumbers)
// 		{
// 			Quitf(ERR_SCR_TEXT_MEM_1,"Failed to allocate memory for TextLinesNoSubstringsNoNumbers array");
// 		}
// 	}

	if (NULL == m_pTextLinesZeroOrOneNumbers)
	{			
		m_pTextLinesZeroOrOneNumbers = rage_new CDisplayTextZeroOrOneNumbers[CScriptHud::ms_MaxNumberOfTextLinesZeroOrOneNumbers];
		if (!m_pTextLinesZeroOrOneNumbers)
		{
			Quitf(ERR_SCR_TEXT_MEM_2,"Failed to allocate memory for TextLinesZeroOrOneNumbers array");
		}
	}

	if (NULL == m_pTextLinesOneSubstring)
	{			
		m_pTextLinesOneSubstring = rage_new CDisplayTextOneSubstring[CScriptHud::ms_MaxNumberOfTextLinesOneSubstring];
		if (!m_pTextLinesOneSubstring)
		{
			Quitf(ERR_SCR_TEXT_MEM_3,"Failed to allocate memory for TextLinesOneSubstring array");
		}
	}

	if (NULL == m_pTextLinesFourSubstringsThreeNumbers)
	{			
		m_pTextLinesFourSubstringsThreeNumbers = rage_new CDisplayTextFourSubstringsThreeNumbers[CScriptHud::ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers];
		if (!m_pTextLinesFourSubstringsThreeNumbers)
		{
			Quitf(ERR_SCR_TEXT_MEM_4,"Failed to allocate memory for TextLinesFourSubstringsThreeNumbers array");
		}
	}


#if ENABLE_DEBUG_HEAP
	{
		USE_DEBUG_MEMORY();

		// HACK: EJ - Memory Optimization
		if (NULL == m_introDebugTextLines)
		{
			m_introDebugTextLines = rage_new CDisplayTextFourSubstringsThreeNumbers[SCRIPT_TEXT_NUM_DEBUG_LINES];
		}
	}
#endif
}

void CDoubleBufferedIntroTexts::FreeMemoryForAllTextLines()
{
//	delete [] m_pTextLinesNoSubstringsNoNumbers;
//	m_pTextLinesNoSubstringsNoNumbers = NULL;

	delete [] m_pTextLinesZeroOrOneNumbers;
	m_pTextLinesZeroOrOneNumbers = NULL;

	delete [] m_pTextLinesOneSubstring;
	m_pTextLinesOneSubstring = NULL;

	delete [] m_pTextLinesFourSubstringsThreeNumbers;
	m_pTextLinesFourSubstringsThreeNumbers = NULL;

#if ENABLE_DEBUG_HEAP
	{
		USE_DEBUG_MEMORY();

		delete [] m_introDebugTextLines;
		m_introDebugTextLines = NULL;
	}
#endif
}

// ****************************** sFakeInteriorStruct ************************************************

void sFakeInteriorStruct::Reset()
{
	bActive = false;
	iHash = 0;
	vPosition.Set(0.0f,0.0f);
	iRotation = 0;
	iLevel = sMiniMapInterior::INVALID_LEVEL;
}

// ****************************** CBlipFades ************************************************

void CBlipFades::sBlipFade::Clear()
{
	m_iUniqueBlipIndex = 0;
	m_iDestinationAlpha = 0;
	m_iTimeRemaining = 0;
}

void CBlipFades::ClearAll()
{
	for (u32 loop = 0; loop < MAX_BLIP_FADES; loop++)
	{
		m_ArrayOfBlipFades[loop].Clear();
	}
}

s32 CBlipFades::GetBlipFadeDirection(s32 uniqueBlipIndex)
{
	for (u32 loop = 0; loop < MAX_BLIP_FADES; loop++)
	{
		if (m_ArrayOfBlipFades[loop].m_iUniqueBlipIndex == uniqueBlipIndex)
		{
			CMiniMapBlip *pBlip = CMiniMap::GetBlip(uniqueBlipIndex);

			if (pBlip)
			{
				s32 currentAlpha = CMiniMap::GetBlipAlphaValue(pBlip);
				s32 alphaDifference = m_ArrayOfBlipFades[loop].m_iDestinationAlpha - currentAlpha;
				if (alphaDifference < 0)
				{
					return -1;
				}
				else if (alphaDifference > 0)
				{
					return 1;
				}
				else
				{
					return 0;
				}
			}
			else
			{	//	Not sure if this can ever happen
				scriptAssertf(0, "CBlipFades::GetBlipFadeDirection - blip not found. Clearing entry in array of blip fades");
				m_ArrayOfBlipFades[loop].Clear();
				return 0;
			}			
		}
	}

	//	No fade found for this blip so return 0 to indicate "not fading"
	return 0;
}

void CBlipFades::AddBlipFade(s32 uniqueBlipIndex, s32 destinationAlpha, s32 fadeDuration)
{
	s32 loop = (MAX_BLIP_FADES - 1);
	s32 freeSlot = -1;
	s32 indexOfExistingSlotForThisBlip = -1;

	while (loop >= 0)
	{
		if (m_ArrayOfBlipFades[loop].m_iUniqueBlipIndex == 0)
		{
			freeSlot = loop;
		}

		if (m_ArrayOfBlipFades[loop].m_iUniqueBlipIndex == uniqueBlipIndex)
		{
			indexOfExistingSlotForThisBlip = loop;
		}

		loop--;
	}

	s32 slotToUse = -1;
	if (indexOfExistingSlotForThisBlip != -1)
	{
		slotToUse = indexOfExistingSlotForThisBlip;
	}
	else if (freeSlot != -1)
	{
		slotToUse = freeSlot;
	}
	else
	{
		scriptAssertf(0, "CBlipFades::AddBlipFade - no free slots in array of blip fades. MAX_BLIP_FADES needs to be increased from %u", MAX_BLIP_FADES);
		return;
	}

	m_ArrayOfBlipFades[slotToUse].m_iUniqueBlipIndex = uniqueBlipIndex;
	m_ArrayOfBlipFades[slotToUse].m_iDestinationAlpha = destinationAlpha;
	m_ArrayOfBlipFades[slotToUse].m_iTimeRemaining = fadeDuration;
}

void CBlipFades::RemoveBlipFade(s32 uniqueBlipIndex)
{
	s32 loop = (MAX_BLIP_FADES - 1);

	while (loop >= 0)
	{
		if (m_ArrayOfBlipFades[loop].m_iUniqueBlipIndex == uniqueBlipIndex)
		{
			m_ArrayOfBlipFades[loop].Clear();
		}

		loop--;
	}
}

void CBlipFades::Update()
{
	for (u32 loop = 0; loop < MAX_BLIP_FADES; loop++)
	{
		if (m_ArrayOfBlipFades[loop].m_iUniqueBlipIndex != 0)
		{
			CMiniMapBlip *pBlip = CMiniMap::GetBlip(m_ArrayOfBlipFades[loop].m_iUniqueBlipIndex);

			if (pBlip)
			{
				const s32 timeStep = fwTimer::GetTimeStepInMilliseconds();

				if (m_ArrayOfBlipFades[loop].m_iTimeRemaining <= timeStep)
				{
					CMiniMap::SetBlipParameter(BLIP_CHANGE_ALPHA, m_ArrayOfBlipFades[loop].m_iUniqueBlipIndex, m_ArrayOfBlipFades[loop].m_iDestinationAlpha);
					m_ArrayOfBlipFades[loop].Clear();
				}
				else
				{
					s32 currentAlpha = CMiniMap::GetBlipAlphaValue(pBlip);
					s32 alphaDifference = m_ArrayOfBlipFades[loop].m_iDestinationAlpha - currentAlpha;

					float alphaOffsetForThisFrame = (float) (alphaDifference * timeStep) / (float) m_ArrayOfBlipFades[loop].m_iTimeRemaining;

					if ( (alphaOffsetForThisFrame >= -1.0f) && (alphaOffsetForThisFrame <= 1.0f) )
					{
						CMiniMap::SetBlipParameter(BLIP_CHANGE_ALPHA, m_ArrayOfBlipFades[loop].m_iUniqueBlipIndex, m_ArrayOfBlipFades[loop].m_iDestinationAlpha);
						m_ArrayOfBlipFades[loop].Clear();
					}
					else
					{
						CMiniMap::SetBlipParameter(BLIP_CHANGE_ALPHA, m_ArrayOfBlipFades[loop].m_iUniqueBlipIndex, currentAlpha + ((s32) alphaOffsetForThisFrame) );
						m_ArrayOfBlipFades[loop].m_iTimeRemaining -= timeStep;
					}
				}
			}
			else
			{	//	Blip no longer exists so clear entry in array
				m_ArrayOfBlipFades[loop].Clear();
			}
		}
	}	
}

// ****************************** CMultiplayerFogOfWarSavegameDetails ************************************************

void CMultiplayerFogOfWarSavegameDetails::Reset()
{
	Set(false, 0, 0, 0, 0, 0);
}

void CMultiplayerFogOfWarSavegameDetails::Set(bool bEnabled, u8 minX, u8 minY, u8 maxX, u8 maxY, u8 fillValueForRestOfMap)
{
	if ( (scriptVerifyf(minX <= maxX, "CMultiplayerFogOfWarSavegameDetails::Set - expected minX(%u) to be <= maxX(%u)", minX, maxX))
		&& (scriptVerifyf(minY <= maxY, "CMultiplayerFogOfWarSavegameDetails::Set - expected minY(%u) to be <= maxY(%u)", minY, maxY))
		&& (scriptVerifyf(maxX < FOG_OF_WAR_RT_SIZE, "CMultiplayerFogOfWarSavegameDetails::Set - expected maxX(%u) to be < %u", maxX, FOG_OF_WAR_RT_SIZE))
		&& (scriptVerifyf(maxY < FOG_OF_WAR_RT_SIZE, "CMultiplayerFogOfWarSavegameDetails::Set - expected maxY(%u) to be < %u", maxY, FOG_OF_WAR_RT_SIZE)) )
	{
		m_bEnabled = bEnabled;
		m_MinX = minX;
		m_MinY = minY;
		m_MaxX = maxX;
		m_MaxY = maxY;
		m_FillValueForRestOfMap = fillValueForRestOfMap;

		scriptDebugf1("CMultiplayerFogOfWarSavegameDetails::Set - m_bEnabled=%s m_MinX=%u m_MinY=%u m_MaxX=%u m_MaxY=%u m_FillValueForRestOfMap=%u",
			m_bEnabled?"true":"false",
			m_MinX, m_MinY, m_MaxX, m_MaxY, m_FillValueForRestOfMap);
	}
}

bool CMultiplayerFogOfWarSavegameDetails::Get(u8 &minX, u8 &minY, u8 &maxX, u8 &maxY, u8 &fillValueForRestOfMap)
{
	minX = m_MinX;
	minY = m_MinY;
	maxX = m_MaxX;
	maxY = m_MaxY;
	fillValueForRestOfMap = m_FillValueForRestOfMap;

	return m_bEnabled;
}


// ****************************** CScriptHud ************************************************

void CScriptHud::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
#if ENABLE_LEGACY_SCRIPTED_RT_SUPPORT
		// create render target used by scripters. Couldn't think of anywhere else in the NY project to put this
		u16 poolID = kRTPoolIDInvalid;
		PPU_ONLY(poolID = CRenderTargets::GetRenderTargetPoolID(PSN_RTMEMPOOL_P2048_TILED_LOCAL_COMPMODE_C32_2X1));	// mempools: only pitch is important
		XENON_ONLY(poolID = CRenderTargets::GetRenderTargetPoolID(XENON_RTMEMPOOL_GENERAL));
		m_offscreenRenderTarget = gRenderTargetMgr.CreateRenderTarget(poolID, "script_rt", 512, 512, 2);
		m_offscreenRenderTarget->AllocateMemoryFromPool();
		grcTextureFactory::GetInstance().RegisterTextureReference("script_rt", m_offscreenRenderTarget);
#endif

// stateblocks
		grcBlendStateDesc bsd;
//		Stateblock for sprite no alpha without mask
		ms_SpriteWithNoAlphaBlendStateHandle = grcStateBlock::CreateBlendState(bsd);

//	The remaining BlendStates should all have these two flags set
		bsd.BlendRTDesc[0].BlendEnable = true;

//		Stateblock for normal sprites and rectangles
		bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
		bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_SRCALPHA;
		bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
		bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
		ms_StandardSpriteBlendStateHandle = grcStateBlock::CreateBlendState(bsd);

//		Stateblock for entire screen when masking
		bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ZERO;
		bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_ZERO;
		bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALPHA;
		ms_EntireScreenMaskBlendStateHandle = grcStateBlock::CreateBlendState(bsd);

//		Stateblock for mask
		bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_REVSUBTRACT;
		bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_REVSUBTRACT;
		bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALPHA;
		ms_MaskBlendStateHandle = grcStateBlock::CreateBlendState(bsd);

//		Stateblock for sprites which use a mask
		bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_DESTALPHA;
		bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVDESTALPHA;
		bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_DESTALPHA;
		bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVDESTALPHA;
		bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
		ms_MaskedSpritesBlendStateHandle = grcStateBlock::CreateBlendState(bsd);
		
		{
			grcDepthStencilStateDesc DS_A;
			DS_A.DepthEnable = false;
			DS_A.DepthWriteMask = 0x0;
			DS_A.DepthFunc = grcRSV::CMP_LESS;
			DS_A.StencilEnable = true;
			DS_A.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;		
			DS_A.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
			DS_A.BackFace.StencilFunc = grcRSV::CMP_ALWAYS;		
			DS_A.BackFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;

			ms_MarkStencil_DSS = grcStateBlock::CreateDepthStencilState(DS_A);

			grcDepthStencilStateDesc DS_B;
			DS_B.DepthEnable = false;
			DS_B.DepthWriteMask = 0x0;
			DS_B.DepthFunc = grcRSV::CMP_LESS;
			DS_B.StencilEnable = true;
			DS_B.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;	
			DS_B.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
			DS_B.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;	
			DS_B.BackFace.StencilPassOp = grcRSV::STENCILOP_KEEP;

			ms_UseStencil_DSS = grcStateBlock::CreateDepthStencilState(DS_B);
			
			grcBlendStateDesc BS_AlphaSet;
			BS_AlphaSet.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALPHA;
			BS_AlphaSet.BlendRTDesc[0].BlendEnable = true;
			BS_AlphaSet.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
			BS_AlphaSet.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
			BS_AlphaSet.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
			ms_SetAlpha_BS = grcStateBlock::CreateBlendState(BS_AlphaSet);
			
			grcBlendStateDesc BS_AlphaClear;
			BS_AlphaClear.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALPHA;
			ms_ClearAlpha_BS = grcStateBlock::CreateBlendState(BS_AlphaClear);

		}
		
		ms_MaxNumberOfTextLinesZeroOrOneNumbers = (u32) CGameConfig::Get().GetConfigMaxNumberOfScriptTextLines("ScriptTextLinesZeroOrOneNumbers");
		if (ms_MaxNumberOfTextLinesZeroOrOneNumbers == 0)
		{
			ms_MaxNumberOfTextLinesZeroOrOneNumbers = 100;
			scriptDisplayf("CScriptHud::Init - max number of ScriptTextLinesZeroOrOneNumbers has not been set up in gameconfig.xml so set it to %u", ms_MaxNumberOfTextLinesZeroOrOneNumbers);
		}
		else
		{
			scriptDisplayf("CScriptHud::Init - max number of ScriptTextLinesZeroOrOneNumbers has been set as %u by gameconfig.xml", ms_MaxNumberOfTextLinesZeroOrOneNumbers);
		}

		ms_MaxNumberOfTextLinesOneSubstring = (u32) CGameConfig::Get().GetConfigMaxNumberOfScriptTextLines("ScriptTextLinesOneSubstring");
		if (ms_MaxNumberOfTextLinesOneSubstring == 0)
		{
			ms_MaxNumberOfTextLinesOneSubstring = 100;
			scriptDisplayf("CScriptHud::Init - max number of ScriptTextLinesOneSubstring has not been set up in gameconfig.xml so set it to %u", ms_MaxNumberOfTextLinesOneSubstring);
		}
		else
		{
			scriptDisplayf("CScriptHud::Init - max number of ScriptTextLinesOneSubstring has been set as %u by gameconfig.xml", ms_MaxNumberOfTextLinesOneSubstring);
		}

		ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers = (u32) CGameConfig::Get().GetConfigMaxNumberOfScriptTextLines("ScriptTextLinesFourSubstringsThreeNumbers");
		if (ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers == 0)
		{
			ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers = 25;
			scriptDisplayf("CScriptHud::Init - max number of ScriptTextLinesFourSubstringsThreeNumbers has not been set up in gameconfig.xml so set it to %u", ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers);
		}
		else
		{
			scriptDisplayf("CScriptHud::Init - max number of ScriptTextLinesFourSubstringsThreeNumbers has been set as %u by gameconfig.xml", ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers);
		}

		for (u32 loop = 0; loop < 2; loop++)
		{
			ms_DBIntroTexts[loop].AllocateMemoryForAllTextLines();
		}

#if __BANK
		scriptDisplayf("sizeof(CDisplayTextFormatting) = %" SIZETFMT "d", sizeof(CDisplayTextFormatting));

		scriptDisplayf("sizeof(CNumberWithinMessage) = %" SIZETFMT "d", sizeof(CNumberWithinMessage));
//		scriptDisplayf("3 * sizeof(CNumberWithinMessage) = %" SIZETFMT "d", 3 * sizeof(CNumberWithinMessage));

		scriptDisplayf("sizeof(CSubStringWithinMessage) = %" SIZETFMT "d", sizeof(CSubStringWithinMessage));
//		scriptDisplayf("4 * sizeof(CSubStringWithinMessage) = %" SIZETFMT "d", 4 * sizeof(CSubStringWithinMessage));

// 		u32 sizeOfDisplayTextBaseClass = (u32) sizeof(CDisplayTextBaseClass);
// 		scriptDisplayf("sizeof(CDisplayTextBaseClass) = %u. There are %u of them so total (double-buffered) size is %u", sizeOfDisplayTextNoSubstringsNoNumbers, ms_MaxNumberOfTextLinesNoSubstringsNoNumbers, (2 * ms_MaxNumberOfTextLinesNoSubstringsNoNumbers * sizeOfDisplayTextNoSubstringsNoNumbers) );

		u32 sizeOfDisplayTextZeroOrOneNumbers = (u32) sizeof(CDisplayTextZeroOrOneNumbers);
		scriptDisplayf("sizeof(CDisplayTextZeroOrOneNumbers) = %u. There are %u of them so total (double-buffered) size is %u", sizeOfDisplayTextZeroOrOneNumbers, ms_MaxNumberOfTextLinesZeroOrOneNumbers, (2 * ms_MaxNumberOfTextLinesZeroOrOneNumbers * sizeOfDisplayTextZeroOrOneNumbers) );

		u32 sizeOfDisplayTextOneSubstring = (u32) sizeof(CDisplayTextOneSubstring);
		scriptDisplayf("sizeof(CDisplayTextOneSubstring) = %u. There are %u of them so total (double-buffered) size is %u", sizeOfDisplayTextOneSubstring, ms_MaxNumberOfTextLinesOneSubstring, (2 * ms_MaxNumberOfTextLinesOneSubstring * sizeOfDisplayTextOneSubstring) );

		u32 sizeOfDisplayTextFourSubstringsThreeNumbers = (u32) sizeof(CDisplayTextFourSubstringsThreeNumbers);
		scriptDisplayf("sizeof(CDisplayTextFourSubstringsThreeNumbers) = %u. There are %u of them so total (double-buffered) size is %u", sizeOfDisplayTextFourSubstringsThreeNumbers, ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers, (2 * ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers * sizeOfDisplayTextFourSubstringsThreeNumbers) );
#endif	//	__BANK
    }
    else if(initMode == INIT_SESSION)
    {
		for (s32 iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES; iCount++)  // init all movie id's as -1
		{
			ResetScriptScaleformMovieVariables(iCount);
		}

#if !__FINAL
		for (u32 text_loop = 0; text_loop < SCRIPT_TEXT_MAX_TYPES; text_loop++)
		{
			ms_NextTimeAllowedToPrintContentsOfFullTextArray[text_loop] = 0;
		}
#endif	//	!__FINAL

	    iFindNextRadarBlipId = 0;
		iCurrentWebpageIdFromActionScript = 0;
		iCurrentWebsiteIdFromActionScript = 0;

		for (s32 iCount = 0; iCount < MAX_ACTIONSCRIPT_FLAGS; iCount++)
		{
			iActionScriptGlobalFlags[iCount] = 0;
		}

		ms_bTurnOffMultiplayerWalletCashThisFrame = false;
		ms_bTurnOffMultiplayerBankCashThisFrame = false;
		ms_bTurnOnMultiplayerWalletCashThisFrame = false;
		ms_bTurnOnMultiplayerBankCashThisFrame = false;

		ms_bAllowDisplayOfMultiplayerCashText = true;

		bDisplayCashStar = false;
	    RadarMessage.bActivate = false;
	    RadarMessage.bMessagesWaiting = false;
	    RadarMessage.bSleepModeActive = false;
	    MissionPassedCashMessage.bActivate = false;
	    MissionPassedCashMessage.cMessage[0] = '\0';
	    MissionPassedCashMessage.iHudColour = HUD_COLOUR_WHITE;

	    bHideFrontendMapBlips = false;
	    NumberOfMiniGamesAllowingNonMiniGameHelpMessages = 0;

		ms_fMiniMapForcedZoomPercentage = 0.0f;
		ms_fRadarZoomDistanceThisFrame = 0.0f;
	    ms_iRadarZoomValue = 0;
	    bDisplayHud = true;
		ms_bDisplayHudWhenNotInStateOfPlayThisFrame.GetWriteBuf() = false;
		ms_bDisplayHudWhenPausedThisFrame.GetWriteBuf() = false;
	    bDisplayRadar = true;
		ms_bFakeSpectatorMode = false;

		ms_BlipFades.ClearAll();
		ms_MultiplayerFogOfWarSavegameDetails.Reset();

	    ms_bAddNextMessageToPreviousBriefs = true;
		ms_PreviousBriefOverride = PREVIOUS_BRIEF_NO_OVERRIDE;

	    CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;
		bScriptHasChangedWidescreenFormat = false;
		ms_IndexOfDrawOrigin = -1;
		ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD;
		ms_CurrentScriptGfxAlignment.Reset();
		ms_bUseMaskForNextSprite = false;
		ms_bInvertMaskForNextSprite = false;
	    iScriptReticleMode = -1;
		bUseVehicleTargetingReticule = false;
	    bHideLoadingAnimThisFrame = false;
		bRenderFrontendBackgroundThisFrame = false;
		bRenderHudOverFadeThisFrame = false;
	    FakeInterior.Reset();
		ms_bFakeExteriorThisFrame = false;
		bDontZoomMiniMapWhenSnipingThisFrame = false;
		ms_bHideMiniMapExteriorMapThisFrame = false;
		ms_bHideMiniMapInteriorMapThisFrame = false;
		vFakePauseMapPlayerPos.Set(INVALID_FAKE_PLAYER_POS);
        vFakeGPSPlayerPos.Set(INVALID_FAKE_PLAYER_POS);
		vInteriorFakePauseMapPlayerPos.Set(INVALID_FAKE_PLAYER_POS);
		ms_iCurrentFakedInteriorHash = 0;
		ms_bUseVerySmallInteriorZoom = false;
		ms_bUseVeryLargeInteriorZoom = false;
	    bDontDisplayHudOrRadarThisFrame = false;
		bDontZoomMiniMapWhenRunningThisFrame = false;
		bDontTiltMiniMapThisFrame = false;
	    bDisplayCashStar = false;
	    bDisablePauseMenuThisFrame = false;
		bSuppressPauseMenuRenderThisFrame = false;
		bAllowPauseWhenInNotInStateOfPlayThisFrame = false;
		fFakeMinimapAltimeterHeight = 0.0f;
		ms_bColourMinimapAltimeter = false;
	    iFakeWantedLevel = 0;
		bFlashWantedStarDisplay = false;
		bForceOffWantedStarFlash = false;
		bForceOnWantedStarFlash = false;
		bUpdateWantedThreatVisibility = false;
		bIsWantedThreatVisible = false;
		bUpdateWantedDrainLevel = false;
		iWantedDrainLevelPercentage = 0;
		iHudMaxHealthDisplay = 0;
		iHudMaxArmourDisplay = 0;
		iHudMaxEnduranceDisplay = 0;
		vFakeWantedLevelPos.Set(0,0,0);
	    bUsePlayerColourInsteadOfTeamColour = false;
	    cMultiplayerBriefTitle[0] = '\0';
	    cMultiplayerBriefContent[0] = '\0';

	    m_playerBlipColourOverride = BLIP_COLOUR_DEFAULT;

	    scriptTextRenderID = CRenderTargetMgr::RTI_MainScreen;

		ms_iOverrideTimeForAreaVehicleNames = -1;
		bUsingMissionCreator = false;
		bForceShowGPS = false;
		bSetDestinationInMapMenu = false;
		bWantsToBlockWantedFlash = false;		
		bAllowMissionCreatorWarp = true;
		ms_bDisplayPlayerNameBlipTags = false;

		ms_FormattingForNextDisplayText.Reset();

	    ScriptMessagesLastClearedInFrame = fwTimer::GetSystemFrameCount();

//	    bUseMessageFormatting = false;
//	    MessageCentre = 0;
//	    MessageWidth = 0;

		ms_DBIntroTexts.GetWriteBuf().Initialise(true);
		ms_DBIntroRects.GetWriteBuf().Initialise(true);
		ms_DBDrawOrigins.GetWriteBuf().Initialise();

	    bMaskCreated = false;
		bUsedMaskLastTime = true;  // set to true so it defaults to "non mask" render states 1st time round
		
		ms_StopLoadingScreen = false;
	}

	ScriptLiteralStrings.Initialise(initMode);
}

void CScriptHud::OnNetworkClosed(void)
{
	// Hack to fix url:bugstar:3008428.  When a player signs out while a crew emblem is rendered, 
	// the crew emblem is released and we attempt to PushRefCount on the zeroth buffer that has 
	// a stale crew emblem texture pointer
	ms_DBIntroRects[0].Initialise(true);
}

void CScriptHud::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
		for (u32 loop = 0; loop < 2; loop++)
		{
			ms_DBIntroTexts[loop].FreeMemoryForAllTextLines();
		}
	}
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
		for (u32 bufferIndex = 0; bufferIndex < 2; bufferIndex++)
		{
			ms_DBIntroTexts[bufferIndex].Initialise(true);
			ms_DBIntroRects[bufferIndex].Initialise(true);
			ms_DBDrawOrigins[bufferIndex].Initialise();
		}

		for (s32 iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES; iCount++)  // init all movie id's as -1
		{
			if (ScriptScaleformMovie[iCount].iId != -1)
			{
				CallShutdownMovie(iCount);
				RemoveScaleformMovie(iCount);
				DeleteScriptScaleformMovie(iCount, true);
			}
		}
	}

	ScriptLiteralStrings.Shutdown(shutdownMode);
}

void CScriptHud::PreSceneProcess(void)
{
	if (CTheScripts::ShouldBeProcessed() REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()) )
	{
		// we must clear all the messages at the start of each frame - never anywhere else
		ClearMessages(); 
	}
}

void CScriptHud::ClearMapVisibilityPerFrameFlags(void)
{
	ms_bHideMiniMapExteriorMapThisFrame = false;
	ms_bHideMiniMapInteriorMapThisFrame = false;
}

void CScriptHud::Process(void)
{
	FakeInterior.Reset();
	ms_fRadarZoomDistanceThisFrame = 0.0f;
	ms_bFakeExteriorThisFrame = false;
	bDontZoomMiniMapWhenSnipingThisFrame = false;
	bRenderFrontendBackgroundThisFrame = false;
	bRenderHudOverFadeThisFrame = false;
	vFakePauseMapPlayerPos.Set(INVALID_FAKE_PLAYER_POS);
    vFakeGPSPlayerPos.Set(INVALID_FAKE_PLAYER_POS);
	bDontDisplayHudOrRadarThisFrame = false;
	bDontZoomMiniMapWhenRunningThisFrame = false;
	bDontTiltMiniMapThisFrame = false;
	bDisablePauseMenuThisFrame = false;
	bSuppressPauseMenuRenderThisFrame = false;
	bAllowPauseWhenInNotInStateOfPlayThisFrame = false;
	ms_bDisplayHudWhenNotInStateOfPlayThisFrame.GetWriteBuf() = false;
	ms_bDisplayHudWhenPausedThisFrame.GetWriteBuf() = false;
	bDisplayCashStar = false;

 	ms_bTurnOffMultiplayerWalletCashThisFrame = false;
 	ms_bTurnOffMultiplayerBankCashThisFrame = false;
 	ms_bTurnOnMultiplayerWalletCashThisFrame = false;
 	ms_bTurnOnMultiplayerBankCashThisFrame = false;

	ClearMapVisibilityPerFrameFlags();

	ms_BlipFades.Update();

	UpdateScaleformComponents();

}

//PURPOSE: Called before every script thread is updated. Used to reset certain parameters
void CScriptHud::BeforeThreadUpdate()
{
	scriptTextRenderID = CRenderTargetMgr::RTI_MainScreen;
	CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;
	bScriptHasChangedWidescreenFormat = false;
}

void CScriptHud::AfterThreadUpdate()
{
	scriptAssertf(ms_CurrentScriptGfxAlignment.IsReset(), "A script has setup safezone alignment without resetting it before the end of the script");
	if (!scriptVerifyf(!CScriptTextConstruction::IsConstructingText(), "A BEGIN text command has been called without a matching END command"))
	{
		CScriptTextConstruction::SetTextConstructionCommand(TEXT_CONSTRUCTION_NONE);
	}
}


void CScriptHud::DrawScriptText(int CurrentRenderID, u32 iDrawOrderValue)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	//	Draw intro text
	int bufferIdx = GetReadbufferIndex();
	ms_DBIntroTexts[bufferIdx].Draw(CurrentRenderID, iDrawOrderValue);

	CText::Flush();  // ensure all th
}

void intro_script_rectangle::Draw(int CurrentRenderID, u32 iDrawOrderValue) const
{
	Vector2 v[4];

	// deal with widescreen:
	Vector2 vPositionValueMin(m_ScriptRectMin.x, m_ScriptRectMin.y);
	Vector2 vPositionValueMax(m_ScriptRectMax.x, m_ScriptRectMax.y);

	if (m_IndexOfDrawOrigin >= 0)
	{
		Vector2 vOrigin;
		bool hasOrigin;
		int bufferIdx = CScriptHud::GetReadbufferIndex();
		hasOrigin = CScriptHud::GetDrawOrigins()[bufferIdx].GetScreenCoords(m_IndexOfDrawOrigin, vOrigin);
		
		if (hasOrigin)
		{
			// adjust the origin for widescreen if required
			if (vPositionValueMin.x != 0.0f && vPositionValueMin.y != 0.0f && vPositionValueMax.x != 1.0f && vPositionValueMax.y != 1.0f)  // dont scale for widescreen if it has been set already at full FULLSCREEN
			{
				CHudTools::AdjustForWidescreen(m_ScriptWidescreenFormat, &vOrigin, NULL, NULL);
			}

			vPositionValueMin += vOrigin;
			vPositionValueMax += vOrigin;
		}
		else
		{
			return;	//	Is it okay to return early?
		}
	}

	// strip pause flag so we can do a comparison check
	u32 iScriptGfxDrawPropertiesWithoutPauseFlag = m_ScriptGfxDrawProperties & (~SCRIPT_GFX_VISIBLE_WHEN_PAUSED);

	bool bRenderThisItem = (iScriptGfxDrawPropertiesWithoutPauseFlag == iDrawOrderValue);

	if (CPauseMenu::IsActive())
		bRenderThisItem &= (m_ScriptGfxDrawProperties & SCRIPT_GFX_VISIBLE_WHEN_PAUSED) == SCRIPT_GFX_VISIBLE_WHEN_PAUSED;

/*#if __DEV
	if ( (CurrentRenderID == m_RenderID) && (bRenderThisItem) )
	{
		Displayf("Gfx check %u %u - %0.2f %0.2f %0.2f %0.2f - RENDER: YES", iScriptGfxDrawPropertiesWithoutPauseFlag, iDrawOrderValueWithoutPauseFlag, m_ScriptRectMin.x, m_ScriptRectMin.y, m_ScriptRectMax.x, m_ScriptRectMax.y);
	}
	else
	{
		Displayf("Gfx check %u %u - %0.2f %0.2f %0.2f %0.2f - RENDER: NO", iScriptGfxDrawPropertiesWithoutPauseFlag, iDrawOrderValueWithoutPauseFlag, m_ScriptRectMin.x, m_ScriptRectMin.y, m_ScriptRectMax.x, m_ScriptRectMax.y);
	}
#endif // __DEV*/

	if ( (CurrentRenderID == m_RenderID) && (bRenderThisItem) )
	{
		// if we want to use a mask here:
		if (m_bUseMask)
		{
			if (scriptVerifyf(CScriptHud::bMaskCreated, "Mask not created yet! - ignorable assert. m_RenderID = %d, m_ScriptGfxDrawProperties = %d", m_RenderID, m_ScriptGfxDrawProperties))
			{
				if (!CScriptHud::bUsedMaskLastTime)
				{
					grcBlendStateHandle outsideMaskHandle = m_bInvertMask ? CScriptHud::ms_MaskBlendStateHandle : CScriptHud::ms_EntireScreenMaskBlendStateHandle;
					grcBlendStateHandle maskHandle = m_bInvertMask ? CScriptHud::ms_EntireScreenMaskBlendStateHandle : CScriptHud::ms_MaskBlendStateHandle;
					const CRGBA color( 0,0,0,m_ScriptRectColour.GetAlpha() );

					// create the mask using the alpha of this rectangle:
					//	Stateblock for entire screen when masking
					grcStateBlock::SetBlendState(outsideMaskHandle);
					const fwRect rWhole(0.f,1.f,1.f,0.f);
					CSprite2d::DrawRectSwitch( m_bInterface, rWhole, color );

					//	Stateblock for mask
					grcStateBlock::SetBlendState(maskHandle);
					const fwRect rMask( CScriptHud::vMask[0].x, CScriptHud::vMask[1].y, CScriptHud::vMask[1].x, CScriptHud::vMask[0].y );
					CSprite2d::DrawRectSwitch( m_bInterface, rMask, color );

					// 	Stateblock for sprites which use a mask
					grcStateBlock::SetBlendState(CScriptHud::ms_MaskedSpritesBlendStateHandle);
				}
			}
		}
		else
		{
			if (CScriptHud::bUsedMaskLastTime)
			{
				grcStateBlock::SetBlendState(CScriptHud::ms_StandardSpriteBlendStateHandle);
			}
		}

		CScriptHud::bUsedMaskLastTime = m_bUseMask;


		switch(m_eGfxType)
		{
			//
			// Do nothing
			//
			case SCRIPT_GFX_NONE :
			{
				// Do Nothing
				break;
			}

			//
			// Draw a rectangle of solid colour (not fancy)
			//
			case SCRIPT_GFX_SOLID_COLOUR_STEREO :
			case SCRIPT_GFX_SOLID_COLOUR :
			{
				Assert(m_pTexture == NULL);

#if RSG_PC
				if (m_eGfxType == SCRIPT_GFX_SOLID_COLOUR_STEREO)
					CShaderLib::SetStereoParams(Vector4(0.0f,1.0f,0.0f,0.0f));
				else
					CShaderLib::SetStereoParams(Vector4(0.0f,0.0f,0.0f,0.0f));
#endif
				fwRect BoxRect;
				
				BoxRect.top		= vPositionValueMin.y;
				BoxRect.bottom	= vPositionValueMax.y;
				BoxRect.left	= vPositionValueMin.x;
				BoxRect.right	= vPositionValueMax.x;
			
				CSprite2d::DrawRectSwitch( m_bInterface, BoxRect, m_ScriptRectColour);

#if RSG_PC
				CShaderLib::SetStereoParams(Vector4(0.0f,0.0f,0.0f,0.0f));
#endif
				break;
			}

			//
			// draw a mask used on subsequent gfx calls
			//
			case SCRIPT_GFX_MASK :
			{
				Assert(m_pTexture == NULL);

				CScriptHud::vMask[0] = vPositionValueMin;
				CScriptHud::vMask[1] = vPositionValueMax;
				CScriptHud::bMaskCreated = true;

#if __BANK
				if (CScriptDebug::GetDebugMaskCommands())
				{
					scriptDisplayf("intro_script_rectangle::Draw - mask created. m_RenderID = %d, m_ScriptGfxDrawProperties = %d", m_RenderID, m_ScriptGfxDrawProperties);
				}
#endif	//	__BANK
				break;
			}

			//
			// draw a sprite
			//
			case SCRIPT_GFX_SPRITE_NOALPHA :
				scriptAssertf(!m_bUseMask, "intro_script_rectangle::Draw - can't yet mix SCRIPT_GFX_SPRITE_NOALPHA and m_bUseMask");
				grcStateBlock::SetBlendState(CScriptHud::ms_SpriteWithNoAlphaBlendStateHandle);
				// FALL THROUGH...				
			case SCRIPT_GFX_SPRITE :
			case SCRIPT_GFX_SPRITE_NON_INTERFACE :
			case SCRIPT_GFX_SPRITE_STEREO:
			case SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW :
			{
#if RSG_PC
				if (m_eGfxType == SCRIPT_GFX_SPRITE_STEREO)
					CShaderLib::SetStereoParams(Vector4(0.0f,1.0f,0.0f,0.0f));
				else
					CShaderLib::SetStereoParams(Vector4(0.0f,0.0f,0.0f,0.0f));
#endif

#if __BANK
				if (CNewHud::bDebugScriptGfxDrawOrder)
				{
					Displayf("Sprite drawn at %0.2f,%0.2f using draw order %d", vPositionValueMin.x, vPositionValueMin.y, iDrawOrderValue);
				}
#endif // #if __DEV
				 
				if (m_ScriptRectRotationOrWidth == 0.0f)
				{
					v[0].Set(vPositionValueMin.x, vPositionValueMax.y);
					v[1].Set(vPositionValueMin.x, vPositionValueMin.y);
					v[2].Set(vPositionValueMax.x, vPositionValueMax.y);
					v[3].Set(vPositionValueMax.x, vPositionValueMin.y);
				}
				else
				{
					float aspectRatio		= 1.0f;
					float invAspectRatio	= 1.0f;
					if(m_bARAwareX)
					{
						aspectRatio = CHudTools::GetAspectRatio(false);
						invAspectRatio = 1.0f / aspectRatio;
					}


					float CentreX = (vPositionValueMin.x + vPositionValueMax.x) / 2.0f;
					const float CentreY = (vPositionValueMin.y + vPositionValueMax.y) / 2.0f;
					float Width = CentreX - vPositionValueMin.x;
					const float Height = CentreY - vPositionValueMin.y;

					if(m_bARAwareX)
					{	// revert applied screen AR to all X coords by script:
						CentreX *= aspectRatio;
						Width	*= aspectRatio;
					}

					const float cos_float = rage::Cosf(m_ScriptRectRotationOrWidth);
					const float sin_float = rage::Sinf(m_ScriptRectRotationOrWidth);

					const float WidthCos = Width * cos_float;
					const float WidthSin = Width * sin_float;
					const float HeightCos = Height * cos_float;
					const float HeightSin = Height * sin_float;

					if(m_bARAwareX)
					{	// reapply AR (if necessary):
						v[0].Set((CentreX - WidthCos - HeightSin) * invAspectRatio, CentreY - WidthSin + HeightCos);
						v[1].Set((CentreX - WidthCos + HeightSin) * invAspectRatio, CentreY - WidthSin - HeightCos);
						v[2].Set((CentreX + WidthCos - HeightSin) * invAspectRatio, CentreY + WidthSin + HeightCos);
						v[3].Set((CentreX + WidthCos + HeightSin) * invAspectRatio, CentreY + WidthSin - HeightCos);
					}
					else
					{	// reverted back to old (incorrect) 2D rotations as there's plenty of existing script functionality depending on this:
						// e.g. B*4031305 (Heist Hacking Minigame - Lines left behind by cursor do not display correctly):
						
						// correct:
						//v[0].Set(CentreX - WidthCos - HeightSin, CentreY - WidthSin + HeightCos);
						//v[1].Set(CentreX - WidthCos + HeightSin, CentreY - WidthSin - HeightCos);
						//v[2].Set(CentreX + WidthCos - HeightSin, CentreY + WidthSin + HeightCos);
						//v[3].Set(CentreX + WidthCos + HeightSin, CentreY + WidthSin - HeightCos);
						
						// incorrect:
						v[0].Set(CentreX - WidthCos - WidthSin, CentreY - HeightSin + HeightCos);
						v[1].Set(CentreX - WidthCos + WidthSin, CentreY - HeightSin - HeightCos);
						v[2].Set(CentreX + WidthCos - WidthSin, CentreY + HeightSin + HeightCos);
						v[3].Set(CentreX + WidthCos + WidthSin, CentreY + HeightSin - HeightCos);
					}
				}

				// Re-jigged to make life easier
				bool bMovieRet = false;
				if (m_bUseMovie)
				{
					bMovieRet = g_movieMgr.BeginDraw(m_BinkMovieId);
					CSprite2d::DrawSwitch(m_bInterface, false, 0.f, v[0],v[1],v[2],v[3], m_ScriptRectColour);
					g_movieMgr.EndDraw(m_BinkMovieId);	// Call this regardless of bMovieRet, it renders a black quad if (for whatever reason) the movie can't play
				} 
				else 
				{
					bool isLightEnabled = grcLightState::IsEnabled();
			
					grcLightState::SetEnabled(false);
					grcBindTexture(m_pTexture);
					CSprite2d::DrawSwitch(m_bInterface, false, 0.f, v[0],v[1],v[2],v[3], m_ScriptRectColour);

					grcBindTexture(NULL);
					grcLightState::SetEnabled(isLightEnabled);
				}

				if( m_eGfxType == SCRIPT_GFX_SPRITE_NOALPHA )
				{ // Reset alpha mode
					scriptAssertf(!m_bUseMask, "intro_script_rectangle::Draw - can't yet mix SCRIPT_GFX_SPRITE_NOALPHA and m_bUseMask");
					grcStateBlock::SetBlendState(CScriptHud::ms_StandardSpriteBlendStateHandle);
				}

#if RSG_PC
				CShaderLib::SetStereoParams(Vector4(0.0f,0.0f,0.0f,0.0f));
#endif
				break;
			}

			//
			// draw a sprite with uv coordinates
			//
			// Andrzej: fixed code path for SCRIPT_GFX_SPRITE_WITH_UV (as it was broken by selecting invalid techniques in im.fx shader) (BS#6200348: Arcade Games - Lines between tiles)
			case SCRIPT_GFX_SPRITE_WITH_UV :
			{
				Vector2 tc[4];

				if (m_ScriptRectTexUCoordX == 0.0f &&
					m_ScriptRectTexUCoordY == 0.0f &&
					m_ScriptRectTexVCoordX == 1.0f &&
					m_ScriptRectTexVCoordY == 1.0f)
				{
					// a call using this command without U,V we bring in the texture coords a bit:

					float fMin = 0.005f;
					float fMax = 1.0f - fMin;
					tc[0].Set(fMin, fMax);
					tc[1].Set(fMin, fMin);
					tc[2].Set(fMax, fMax);
					tc[3].Set(fMax, fMin);
				}
				else
				{
					tc[0].Set(m_ScriptRectTexUCoordX, m_ScriptRectTexVCoordY);
					tc[1].Set(m_ScriptRectTexUCoordX, m_ScriptRectTexUCoordY);
					tc[2].Set(m_ScriptRectTexVCoordX, m_ScriptRectTexVCoordY);
					tc[3].Set(m_ScriptRectTexVCoordX, m_ScriptRectTexUCoordY);
				}

				if (m_ScriptRectRotationOrWidth == 0.0f)
				{
					v[0].Set(vPositionValueMin.x, vPositionValueMax.y);
					v[1].Set(vPositionValueMin.x, vPositionValueMin.y);
					v[2].Set(vPositionValueMax.x, vPositionValueMax.y);
					v[3].Set(vPositionValueMax.x, vPositionValueMin.y);

				}
				else
				{
					float aspectRatio = 1.0f;
					float invAspectRatio = 1.0f;
					if (m_bARAwareX)
					{
						aspectRatio = CHudTools::GetAspectRatio(false);
						invAspectRatio = 1.0f / aspectRatio;
					}


					float CentreX = (vPositionValueMin.x + vPositionValueMax.x) / 2.0f;
					const float CentreY = (vPositionValueMin.y + vPositionValueMax.y) / 2.0f;
					float Width = CentreX - vPositionValueMin.x;
					const float Height = CentreY - vPositionValueMin.y;

					if (m_bARAwareX)
					{	// revert applied screen AR to all X coords by script:
						CentreX *= aspectRatio;
						Width *= aspectRatio;
					}

					const float cos_float = rage::Cosf(m_ScriptRectRotationOrWidth);
					const float sin_float = rage::Sinf(m_ScriptRectRotationOrWidth);

					const float WidthCos = Width * cos_float;
					const float WidthSin = Width * sin_float;
					const float HeightCos = Height * cos_float;
					const float HeightSin = Height * sin_float;

					if (m_bARAwareX)
					{	// reapply AR (if necessary):
						v[0].Set((CentreX - WidthCos - HeightSin) * invAspectRatio, CentreY - WidthSin + HeightCos);
						v[1].Set((CentreX - WidthCos + HeightSin) * invAspectRatio, CentreY - WidthSin - HeightCos);
						v[2].Set((CentreX + WidthCos - HeightSin) * invAspectRatio, CentreY + WidthSin + HeightCos);
						v[3].Set((CentreX + WidthCos + HeightSin) * invAspectRatio, CentreY + WidthSin - HeightCos);
					}
					else
					{	// reverted back to old (incorrect) 2D rotations as there's plenty of existing script functionality depending on this:
						// e.g. B*4031305 (Heist Hacking Minigame - Lines left behind by cursor do not display correctly):

						// correct:
						//v[0].Set(CentreX - WidthCos - HeightSin, CentreY - WidthSin + HeightCos);
						//v[1].Set(CentreX - WidthCos + HeightSin, CentreY - WidthSin - HeightCos);
						//v[2].Set(CentreX + WidthCos - HeightSin, CentreY + WidthSin + HeightCos);
						//v[3].Set(CentreX + WidthCos + HeightSin, CentreY + WidthSin - HeightCos);

						// incorrect:
						v[0].Set(CentreX - WidthCos - WidthSin, CentreY - HeightSin + HeightCos);
						v[1].Set(CentreX - WidthCos + WidthSin, CentreY - HeightSin - HeightCos);
						v[2].Set(CentreX + WidthCos - WidthSin, CentreY + HeightSin + HeightCos);
						v[3].Set(CentreX + WidthCos + WidthSin, CentreY + HeightSin - HeightCos);
					}
				}

				bool bMovieRet = false;
				if (m_bUseMovie)
				{
					bMovieRet = g_movieMgr.BeginDraw(m_BinkMovieId);
					CSprite2d::DrawSwitch(m_bInterface, false, 0.f, v[0], v[1], v[2], v[3], tc[0], tc[1], tc[2], tc[3], m_ScriptRectColour);
					g_movieMgr.EndDraw(m_BinkMovieId);	// Call this regardless of bMovieRet, it renders a black quad if (for whatever reason) the movie can't play
				}
				else
				{
					const bool isLightEnabled = grcLightState::IsEnabled();
					grcLightState::SetEnabled(false);
					grcBindTexture(m_pTexture);

					CSprite2d::DrawSwitch(m_bInterface, false, 0.f, v[0], v[1], v[2], v[3], tc[0], tc[1], tc[2], tc[3], m_ScriptRectColour);

					grcBindTexture(NULL);
					grcLightState::SetEnabled(isLightEnabled);
				}
				break;
			}

			case SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW_WITH_UV :	// Andrzej: this may require same new code path as SCRIPT_GFX_SPRITE_WITH_UV (see above)
			{
				Vector2 tc[4];

				if (m_ScriptRectTexUCoordX == 0.0f &&
					m_ScriptRectTexUCoordY == 0.0f &&
					m_ScriptRectTexVCoordX == 1.0f &&
					m_ScriptRectTexVCoordY == 1.0f)
				{
					// a call using this command without U,V we bring in the texture coords a bit:

					float fMin = 0.005f;
					float fMax = 1.0f - fMin;
					tc[0].Set(fMin,fMax);
					tc[1].Set(fMin,fMin);
					tc[2].Set(fMax,fMax);
					tc[3].Set(fMax,fMin);
				}
				else
				{
					tc[0].Set(m_ScriptRectTexUCoordX,m_ScriptRectTexVCoordY);
					tc[1].Set(m_ScriptRectTexUCoordX,m_ScriptRectTexUCoordY);
					tc[2].Set(m_ScriptRectTexVCoordX,m_ScriptRectTexVCoordY);
					tc[3].Set(m_ScriptRectTexVCoordX,m_ScriptRectTexUCoordY);
				}

				if (m_ScriptRectRotationOrWidth == 0.0f)
				{
					v[0].Set(vPositionValueMin.x, vPositionValueMax.y);
					v[1].Set(vPositionValueMin.x, vPositionValueMin.y);
					v[2].Set(vPositionValueMax.x, vPositionValueMax.y);
					v[3].Set(vPositionValueMax.x, vPositionValueMin.y);
				
				}
				else
				{
					float aspectRatio		= 1.0f;
					float invAspectRatio	= 1.0f;
					if(m_bARAwareX)
					{
						aspectRatio = CHudTools::GetAspectRatio(false);
						invAspectRatio = 1.0f / aspectRatio;
					}


					float CentreX = (vPositionValueMin.x + vPositionValueMax.x) / 2.0f;
					const float CentreY = (vPositionValueMin.y + vPositionValueMax.y) / 2.0f;
					float Width = CentreX - vPositionValueMin.x;
					const float Height = CentreY - vPositionValueMin.y;

					if(m_bARAwareX)
					{	// revert applied screen AR to all X coords by script:
						CentreX *= aspectRatio;
						Width	*= aspectRatio;
					}

					const float cos_float = rage::Cosf(m_ScriptRectRotationOrWidth);
					const float sin_float = rage::Sinf(m_ScriptRectRotationOrWidth);

					const float WidthCos = Width * cos_float;
					const float WidthSin = Width * sin_float;
					const float HeightCos = Height * cos_float;
					const float HeightSin = Height * sin_float;

					if(m_bARAwareX)
					{	// reapply AR (if necessary):
						v[0].Set((CentreX - WidthCos - HeightSin) * invAspectRatio, CentreY - WidthSin + HeightCos);
						v[1].Set((CentreX - WidthCos + HeightSin) * invAspectRatio, CentreY - WidthSin - HeightCos);
						v[2].Set((CentreX + WidthCos - HeightSin) * invAspectRatio, CentreY + WidthSin + HeightCos);
						v[3].Set((CentreX + WidthCos + HeightSin) * invAspectRatio, CentreY + WidthSin - HeightCos);
					}
					else
					{	// reverted back to old (incorrect) 2D rotations as there's plenty of existing script functionality depending on this:
						// e.g. B*4031305 (Heist Hacking Minigame - Lines left behind by cursor do not display correctly):

						// correct:
						//v[0].Set(CentreX - WidthCos - HeightSin, CentreY - WidthSin + HeightCos);
						//v[1].Set(CentreX - WidthCos + HeightSin, CentreY - WidthSin - HeightCos);
						//v[2].Set(CentreX + WidthCos - HeightSin, CentreY + WidthSin + HeightCos);
						//v[3].Set(CentreX + WidthCos + HeightSin, CentreY + WidthSin - HeightCos);

						// incorrect:
						v[0].Set(CentreX - WidthCos - WidthSin, CentreY - HeightSin + HeightCos);
						v[1].Set(CentreX - WidthCos + WidthSin, CentreY - HeightSin - HeightCos);
						v[2].Set(CentreX + WidthCos - WidthSin, CentreY + HeightSin + HeightCos);
						v[3].Set(CentreX + WidthCos + WidthSin, CentreY + HeightSin - HeightCos);
					}
				}

				bool bMovieRet = false;
				if (m_bUseMovie){
					bMovieRet = g_movieMgr.BeginDraw(m_BinkMovieId);
				} else {
					CSprite2d::SetRenderState(m_pTexture);
				}

				const bool bStretchSprite = (m_bUseMovie == false && m_bInterface);
				CSprite2d::DrawSwitch(bStretchSprite, true, 0.f, v[0], v[1], v[2], v[3], tc[0], tc[1], tc[2], tc[3], m_ScriptRectColour);

				if (m_bUseMovie){
					if (bMovieRet) {
						g_movieMgr.EndDraw(m_BinkMovieId);
					}
				}

				break;
			}

			//
			// draw a solid line
			//
			case SCRIPT_GFX_LINE :
			{
				Vector2 vCurrentPoint[4];

				Vector2 tc[4];
				tc[0].Set(0.0f,1.0f);
				tc[1].Set(0.0f,0.0f);
				tc[2].Set(1.0f,1.0f);
				tc[3].Set(1.0f,0.0f);

				float fWidth = m_ScriptRectRotationOrWidth;
/*
				// temp test stuff to test the line...
				float fWidth =  0.002f;
				static Vector2 vsPositionValueMin = Vector2(0.4f, 0.9f);
				static Vector2 vsPositionValueMax = Vector2(0.7f, 0.2f);

				vPositionValueMin.x = vsPositionValueMin.x;
				vPositionValueMin.y = vsPositionValueMin.y;
				vPositionValueMax.x = vsPositionValueMax.x;
				vPositionValueMax.y = vsPositionValueMax.y;
*/

				vCurrentPoint[0].Set(vPositionValueMin.x-fWidth, vPositionValueMin.y-fWidth);
				vCurrentPoint[1].Set(vPositionValueMin.x+fWidth, vPositionValueMin.y-fWidth);
				vCurrentPoint[2].Set(vPositionValueMax.x-fWidth, vPositionValueMax.y+fWidth);
				vCurrentPoint[3].Set(vPositionValueMax.x+fWidth, vPositionValueMax.y+fWidth);

#if SUPPORT_MULTI_MONITOR
				if (m_bInterface)
				{
					CSprite2d::MoveToScreenGUI(vCurrentPoint);
				}
#endif	//SUPPORT_MULTI_MONITOR

				grcBindTexture(NULL);

				grcBegin(drawTriStrip, 4);
				for (s32 j = 0; j < 4; j++)
				{
					grcVertex(vCurrentPoint[j].x, vCurrentPoint[j].y, 0.0f, 0, 0, -1, m_ScriptRectColour, tc[j].x, tc[j].y);
				}
				grcEnd();

				break;
			}

			//
			// Draw a 2D scaleform movie:
			//
			case SCRIPT_GFX_SCALEFORM_MOVIE_RETICULESTEREO : 
			case SCRIPT_GFX_SCALEFORM_MOVIE_STEREO :
			case SCRIPT_GFX_SCALEFORM_MOVIE :
			{
#if RSG_PC
				if (m_eGfxType == SCRIPT_GFX_SCALEFORM_MOVIE_RETICULESTEREO)
				{
					CShaderLib::SetReticuleDistTexture(true);
					CShaderLib::SetStereoParams(Vector4(0,0,0,1));
				}
				else if (m_eGfxType == SCRIPT_GFX_SCALEFORM_MOVIE_STEREO)
				{
					CShaderLib::SetStereoParams(Vector4(0,1,0,0));
				}
				else
					CShaderLib::SetStereoParams(Vector4(0,0,0,0));
#endif
				if (scriptVerifyf(m_ScaleformMovieId != -1, "intro_script_rectangle::Draw - Scaleform Movie Id is invalid"))
				{
					sScaleformComponent &ScaleformMovie = CScriptHud::ScriptScaleformMovie[m_ScaleformMovieId];

					if ( (ScaleformMovie.bActive) && (!ScaleformMovie.bDeleteRequested) && (CScaleformMgr::IsMovieActive(ScaleformMovie.iId)) )
					{
						if (ScaleformMovie.iBackgroundMaskedMovieId == -1)
						{
							// standard 1 movie:
							CScriptHud::RenderScriptedScaleformMovie(m_ScaleformMovieId,
																	 Vector2(vPositionValueMin.x, vPositionValueMin.y),
																	 Vector2(vPositionValueMax.x - vPositionValueMin.x, vPositionValueMax.y - vPositionValueMin.y), CurrentRenderID,ScaleformMovie.iLargeRT);
						}
						else
						{
							// two movies, foreground & background with masking inbetween:
							CScriptHud::RenderScriptedScaleformMovieWithMasking(m_ScaleformMovieId,
																				Vector2(vPositionValueMin.x, vPositionValueMin.y),
																				Vector2(vPositionValueMax.x - vPositionValueMin.x, vPositionValueMax.y - vPositionValueMin.y));
						}
					}
				}
#if RSG_PC
				if ((m_eGfxType == SCRIPT_GFX_SCALEFORM_MOVIE_RETICULESTEREO) || (m_eGfxType == SCRIPT_GFX_SCALEFORM_MOVIE_STEREO))
				{
					CShaderLib::SetReticuleDistTexture(false);
					CShaderLib::SetStereoParams(Vector4(0,0,0,0));
				}
#endif	
				break;
			}
		}
	}
}



void CScriptHud::RenderScriptedScaleformMovie(s32 iMovieId, Vector2 vPos, Vector2 vSize, int CurrentRenderID, u8 iLargeRT)
{
	sScaleformComponent &ScaleformMovie = CScriptHud::ScriptScaleformMovie[iMovieId];

	if ( (ScaleformMovie.bActive) && (!ScaleformMovie.bDeleteRequested) && (CScaleformMgr::IsMovieActive(ScaleformMovie.iId)) )
	{
		bool bForceUpdateBeforeRender = false;

		if (ScaleformMovie.bInvokedBeforeRender)
		{
			bForceUpdateBeforeRender = true;//fwTimer::IsGamePaused();
			ScaleformMovie.bInvokedBeforeRender = false;
		}

		GFxMovieView::ScaleModeType scaleMode = GFxMovieView::SM_ExactFit;

		// set the pos & size of the movie:
		if(!ScaleformMovie.bForceExactFit)
		{
			if (vPos.x == 0.0f && vPos.y == 0.0f && vSize.x == 1.0f && vSize.y == 1.0f && !CHudTools::IsSuperWideScreen())
			{
				if (CHudTools::GetWideScreen())
					scaleMode = GFxMovieView::SM_ShowAll;
				else
					scaleMode = GFxMovieView::SM_NoBorder;
			}
		}
		bool forceNormalBlendState = iLargeRT > 0;
		CScaleformMgr::GetMovieMgr()->GetRageRenderer().ForceNormalBlendState(forceNormalBlendState); // We assume that the largeRT is actually the Yacht name and force actual normal alpha.
		CScaleformMgr::ChangeMovieParams(ScaleformMovie.iId, vPos, vSize, scaleMode, CurrentRenderID, ScaleformMovie.bIgnoreSuperWidescreen,iLargeRT);

		// render the movie:
		// scriptDisplayf("Rendering Script Scaleform Movie %s (%d) --%d--", ScaleformMovie.cFilename, iMovieId+1, fwTimer::GetSystemFrameCount());

		float fTimer = 0.0f;

		if (ScaleformMovie.bUseSystemTimer)  // for 1333009
		{
			fTimer = fwTimer::GetSystemTimeStep();
		}


		// fix for 1563069 - some movies need to be updated when paused
		// HACK_OF_SORTS (ACTUALLY, NO, A REAL HACK)
		// Adam knows about this hack
		bool bUpdateWhenPaused = NetworkInterface::IsGameInProgress();

		if (!bUpdateWhenPaused)
		{
			u32 iFilenameHash = atStringHash(CScaleformMgr::GetMovieFilename(ScaleformMovie.iId));
			if ( (iFilenameHash == ATSTRINGHASH("MP_BIG_MESSAGE_FREEMODE",0xcd8897db)) ||
				(iFilenameHash == ATSTRINGHASH("INSTRUCTIONAL_BUTTONS",0xfb9a6ac1)) )
			{
				bUpdateWhenPaused = true;
			}
		}

		CScaleformMgr::RenderMovie(ScaleformMovie.iId, fTimer, bUpdateWhenPaused, bForceUpdateBeforeRender);
		ScaleformMovie.bRenderedThisFrame = true;
		CScaleformMgr::GetMovieMgr()->GetRageRenderer().ForceNormalBlendState(false);

#if GTA_REPLAY
		CNewHud::GetReplaySFController().StoreMovieBeingDrawn(ScaleformMovie.iId);
#endif // GTA_REPLAY
	}
}


void CScriptHud::RenderScriptedScaleformMovieWithMasking(s32 m_ScaleformMovieId, Vector2 vPos, Vector2 vSize)
{
	// background movie:
	sScaleformComponent &ScaleformMovieFG = CScriptHud::ScriptScaleformMovie[m_ScaleformMovieId];

	if ( (ScaleformMovieFG.bActive) && (ScaleformMovieFG.iBackgroundMaskedMovieId != -1) && (!ScaleformMovieFG.bDeleteRequested) && (CScaleformMgr::IsMovieActive(ScaleformMovieFG.iId)) )
	{
		// save current state blocks
		grcBlendStateHandle BS_Backup = grcStateBlock::BS_Active;
		grcDepthStencilStateHandle DSS_Backup = grcStateBlock::DSS_Active;
		u8 ActiveStencilRef_Backup = grcStateBlock::ActiveStencilRef;
		u32 ActiveBlendFactors_Backup = grcStateBlock::ActiveBlendFactors;
		u64 ActiveSampleMask_Backup = grcStateBlock::ActiveSampleMask;

		// Clear alpha
		grcStateBlock::SetBlendState(ms_ClearAlpha_BS);
		CSprite2d alphaClear;
		alphaClear.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0, 0.0, 0.0));
		alphaClear.BeginCustomList(CSprite2d::CS_BLIT, grcTexture::NoneWhite);
		grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0.1f,0.0f,0.0f,1.0f,1.0f,Color32(0x00000000));
		alphaClear.EndCustomList();
		
		// Clear stencil
		GRCDEVICE.Clear(false, Color32(0x00000000), false, 0.0f, true, 0x000000000 );
		
		// Setup alpha and stencil mask
		sScaleformComponent &ScaleformMovieBG = CScriptHud::ScriptScaleformMovie[ScaleformMovieFG.iBackgroundMaskedMovieId];

		bool bWasDynamicFontCacheEnabled = CScaleformMgr::GetMovieMgr()->GetLoader()->GetFontCacheManager()->IsDynamicCacheEnabled();

		if (bWasDynamicFontCacheEnabled)  // only mess with it if it was enabled previously
		{
			CScaleformMgr::GetMovieMgr()->GetLoader()->GetFontCacheManager()->EnableDynamicCache(false);  // fix for 1721145 - turn off dynamic cache
		}

		sfScaleformManager* pScaleformMgr = CScaleformMgr::GetMovieMgr();
		sfRendererBase& scRenderer = pScaleformMgr->GetRageRenderer();

		scRenderer.OverrideDepthStencilState(true);
		scRenderer.OverrideBlendState(true);

		grcStateBlock::SetBlendState(ms_SetAlpha_BS,~0U,~0ULL);

		grcStateBlock::SetDepthStencilState(ms_MarkStencil_DSS,0x88);

		if ( (ScaleformMovieBG.bActive) && (!ScaleformMovieBG.bDeleteRequested) && (CScaleformMgr::IsMovieActive(ScaleformMovieBG.iId)) )
		{
			RenderScriptedScaleformMovie(ScaleformMovieFG.iBackgroundMaskedMovieId, vPos, vSize);
		}

		scRenderer.OverrideBlendState(false);
		grcStateBlock::SetBlendState(BS_Backup,ActiveBlendFactors_Backup,ActiveSampleMask_Backup);

		// Render foreground movie, with holes.
		grcStateBlock::SetDepthStencilState(ms_UseStencil_DSS,0x88);
	
		RenderScriptedScaleformMovie(m_ScaleformMovieId, vPos, vSize);
		
		scRenderer.OverrideDepthStencilState(false);
		grcStateBlock::SetDepthStencilState(DSS_Backup,ActiveStencilRef_Backup);

		if (bWasDynamicFontCacheEnabled)  // turn it back on if it was previously enabled
		{
			CScaleformMgr::GetMovieMgr()->GetLoader()->GetFontCacheManager()->EnableDynamicCache(true);  // turn back on dynamic cache
		}
	}
}


void intro_script_rectangle::PushRefCount() const
{
	if( m_pTexture )
	{
		switch (m_eGfxType)
		{
			case SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW :
			case SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW_WITH_UV :
				//	Do I need to call AddRef on the "MISSION_CREATOR_TEXTURE" texture dictionary?
				break;

			default :
				{
					sysMemAllocator& alloc = *sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
					const void *const ptr = alloc.GetCanonicalBlockPtr(m_pTexture);
					strIndex owner = strStreamingEngine::GetAllocator().GetHeapPageOwner(ptr, false);
					if( owner.IsValid() )
					{
						strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModuleFromIndex(owner);
						strLocalIndex index = pModule->GetObjectIndex(owner);
						g_TxdStore.AddRef(index, REF_RENDER); // Make sure the reference sticks around for a few more frames
						gDrawListMgr->AddRefCountedModuleIndex(index.Get(), &g_TxdStore); 	
					}
				}
				break;
		}
	}
}
#if __BANK
void intro_script_rectangle::OutputValuesOfMembersForDebug() const
{
	switch(m_eGfxType)
	{
	case SCRIPT_GFX_NONE :
		Displayf("m_eGfxType = SCRIPT_GFX_NONE");
		break;
	case SCRIPT_GFX_SOLID_COLOUR :
		Displayf("m_eGfxType = SCRIPT_GFX_SOLID_COLOUR");
		break;
	case SCRIPT_GFX_SOLID_COLOUR_STEREO :
		Displayf("m_eGfxType = SCRIPT_GFX_SOLID_COLOUR_STEREO");
		break;
	case SCRIPT_GFX_SPRITE_NOALPHA :
		Displayf("m_eGfxType = SCRIPT_GFX_SPRITE_NOALPHA");
		break;
	case SCRIPT_GFX_SPRITE :
		Displayf("m_eGfxType = SCRIPT_GFX_SPRITE");
		break;
	case SCRIPT_GFX_SPRITE_NON_INTERFACE :
		Displayf("m_eGfxType = SCRIPT_GFX_SPRITE_NON_INTERFACE");
		break;
	case SCRIPT_GFX_SPRITE_STEREO :
		Displayf("m_eGfxType = SCRIPT_GFX_SPRITE_STEREO");
		break;
	case SCRIPT_GFX_SPRITE_WITH_UV :
		Displayf("m_eGfxType = SCRIPT_GFX_SPRITE_WITH_UV");
		break;
	case SCRIPT_GFX_MASK :
		Displayf("m_eGfxType = SCRIPT_GFX_MASK");
		break;
	case SCRIPT_GFX_LINE :
		Displayf("m_eGfxType = SCRIPT_GFX_LINE");
		break;
	case SCRIPT_GFX_SCALEFORM_MOVIE :
		Displayf("m_eGfxType = SCRIPT_GFX_SCALEFORM_MOVIE");
		break;
	case SCRIPT_GFX_SCALEFORM_MOVIE_RETICULESTEREO :
		Displayf("m_eGfxType = SCRIPT_GFX_SCALEFORM_MOVIE_RETICULESTEREO");
		break;
	case SCRIPT_GFX_SCALEFORM_MOVIE_STEREO :
		Displayf("m_eGfxType = SCRIPT_GFX_SCALEFORM_MOVIE_STEREO");
		break;
	case SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW :
		Displayf("m_eGfxType = SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW");
		break;
	case SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW_WITH_UV :
		Displayf("m_eGfxType = SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW_WITH_UV");
		break;
	}

	Displayf("m_ScriptRectMin.x = %f", m_ScriptRectMin.x);
	Displayf("m_ScriptRectMin.y = %f", m_ScriptRectMin.y);
	Displayf("m_ScriptRectMax.x = %f", m_ScriptRectMax.x);
	Displayf("m_ScriptRectMax.y = %f", m_ScriptRectMax.y);

	Displayf("m_pTexture = %p", (grcTexture *) m_pTexture);
	Displayf("m_ScaleformMovieId = %d", m_ScaleformMovieId);
	Displayf("m_BinkMovieId = %d", m_BinkMovieId);

	Displayf("m_ScriptRectColour R=%d, G=%d, B=%d, A=%d", m_ScriptRectColour.GetRed(), m_ScriptRectColour.GetGreen(), m_ScriptRectColour.GetBlue(), m_ScriptRectColour.GetAlpha());

	Displayf("m_ScriptRectRotationOrWidth = %f", m_ScriptRectRotationOrWidth);
	Displayf("m_RenderID = %d", m_RenderID);
	Displayf("m_ScriptWidescreenFormat = %d", m_ScriptWidescreenFormat);
	Displayf("m_IndexOfDrawOrigin = %d", m_IndexOfDrawOrigin);

	Displayf("m_ScriptGfxDrawProperties = %d", (s32)m_ScriptGfxDrawProperties);
	Displayf("m_bUseMask = %s", m_bUseMask?"TRUE":"FALSE");
	Displayf("m_bUseMovie = %s", m_bUseMovie?"TRUE":"FALSE");

	Displayf("m_ScriptRectTexUCoordX = %f", m_ScriptRectTexUCoordX);
	Displayf("m_ScriptRectTexUCoordY = %f", m_ScriptRectTexUCoordY);
	Displayf("m_ScriptRectTexVCoordX = %f", m_ScriptRectTexVCoordX);
	Displayf("m_ScriptRectTexVCoordY = %f", m_ScriptRectTexVCoordY);
}
#endif	//	__BANK


void CDrawOrigins::Set(const Vector3 &vOrigin, bool b2dOrigin)
{
	m_vOrigin = vOrigin;
	m_bIs2dOrigin = b2dOrigin;
}


bool CDrawOrigins::GetScreenCoords(Vector2 &vReturnScreenCoords) const
{
	if (!m_bIs2dOrigin)
	{
		float fScreenX = 0.0f;
		float fScreenY = 0.0f;
		if (CScriptHud::GetScreenPosFromWorldCoord(m_vOrigin, fScreenX, fScreenY))
		{
//			m_vOffset.x = fScreenX;
//			m_vOffset.y = fScreenY;
//			m_vOffset.z = 0.0f;
//			m_bIs2dOffset = true;
			vReturnScreenCoords.x = fScreenX;
			vReturnScreenCoords.y = fScreenY;
			return true;
		}
	}

	if (m_bIs2dOrigin)
	{
		vReturnScreenCoords.x = m_vOrigin.x;
		vReturnScreenCoords.y = m_vOrigin.y;

		return true;
	}

	return false;
}


bool CScriptHud::FreezeRendering()
{
	if( (!NetworkInterface::IsGameInProgress() && CPauseMenu::IsActive() && fwTimer::IsUserPaused()) || fwTimer::IsSystemPaused() )
	{
		return true;
	}

	return false;
}

int CScriptHud::GetReadbufferIndex()
{
	if( FreezeRendering() )
		return 0;
		
	return gRenderThreadInterface.GetRenderBuffer();
}

void CScriptHud::DrawScriptSpritesAndRectangles(int CurrentRenderID, u32 iDrawOrderValue)
{
	PF_AUTO_PUSH_TIMEBAR("DrawScriptSpritesAndRectangles");
	bMaskCreated = false;
	bUsedMaskLastTime = true;

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	CShaderLib::SetGlobalAlpha(1.0f);
	CShaderLib::SetGlobalEmissiveScale(1.0f, true);
	
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetBlendState(ms_StandardSpriteBlendStateHandle);

	int bufferIdx = CScriptHud::GetReadbufferIndex();
	ms_DBIntroRects[bufferIdx].Draw(CurrentRenderID, iDrawOrderValue);

	// Reset stateblocks after drawing script sprites
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);

	CSprite2d::ClearRenderState();
	CShaderLib::SetGlobalEmissiveScale(1.0f, true);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ClearMessages
// PURPOSE :  this should be called after the text has been rendered every frame
//            must be called on the render thread
/////////////////////////////////////////////////////////////////////////////////
void CScriptHud::ClearMessages(void)
{
	u32 CurrentFrame = fwTimer::GetSystemFrameCount();
	if (CurrentFrame == ScriptMessagesLastClearedInFrame)
	{	//	to try to stop this function happening more than once in a frame and breaking the double buffer
		return;
	}

	ScriptMessagesLastClearedInFrame = CurrentFrame;

	CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;
	bScriptHasChangedWidescreenFormat = false;
	
	ms_FormattingForNextDisplayText.Reset();

	ms_DBIntroTexts.GetWriteBuf().Initialise(false);
	ms_DBIntroRects.GetWriteBuf().Initialise(false);
	ms_DBDrawOrigins.GetWriteBuf().Initialise();
	ms_IndexOfDrawOrigin = -1;
	ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD;
//	ms_iCurrentScriptGfxDrawProperties |= SCRIPT_GFX_VISIBLE_WHEN_PAUSED;  // for easy debug
	ms_bUseMaskForNextSprite = false;
	ms_bInvertMaskForNextSprite = false;

	ScriptLiteralStrings.ClearSingleFrameStrings(false);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScriptHud::SetupScaleformMovie
// PURPOSE:	sets up a scaleform movie to be used by script
/////////////////////////////////////////////////////////////////////////////////////
s32 CScriptHud::SetupScaleformMovie(s32 iNewId, const char *pFilename)
{
	if (iNewId != -1)
	{
		CScaleformMgr::SetScriptRequired(iNewId, true);

		s32 iCount;
		// looks for movie id that may already exist and is available to be used
		for (iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES; iCount++)
		{
			if (ScriptScaleformMovie[iCount].iId == iNewId)
			{
#if __BANK
				if (CScaleformMgr::ms_bShowExtraDebugInfo)
				{
#if !__FINAL
					scrThread::PrePrintStackTrace();
#endif
				}
#endif // __BANK
				sfDebugf1("Reused Script id for %s is %s (%d) (SF %d) (del_req=%d)", pFilename, ScriptScaleformMovie[iCount].cFilename, iCount+1, ScriptScaleformMovie[iCount].iId, ScriptScaleformMovie[iCount].bDeleteRequested);

				if (ScriptScaleformMovie[iCount].bDeleteRequested)
					ScriptScaleformMovie[iCount].bDeleteRequested = false;

				return iCount+1;
			}
		}

		// adds it to a new id slot and returns the value:
		for (iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES; iCount++)
		{
			if (ScriptScaleformMovie[iCount].iId == -1 &&
				!ScriptScaleformMovie[iCount].bDeleteRequested &&
				!ScriptScaleformMovie[iCount].bActive)
			{
				ScriptScaleformMovie[iCount].iId = iNewId;
				ScriptScaleformMovie[iCount].iParentMovie = -1;  // not a parent movie
				ScriptScaleformMovie[iCount].bGfxReady = false;
				ScriptScaleformMovie[iCount].bUseSystemTimer = false;
				strcpy(ScriptScaleformMovie[iCount].cFilename, pFilename);
				ScriptScaleformMovie[iCount].bForceExactFit = false;
				ScriptScaleformMovie[iCount].bSkipWidthOffset = false;
				WIN32PC_ONLY(ScriptScaleformMovie[iCount].bIgnoreSuperWidescreen = CScaleformMgr::GetMovieShouldIgnoreSuperWidescreen(iNewId);)

				if( stricmp(pFilename, "traffic_cam") == 0 ||
					stricmp(pFilename, "security_cam") == 0 ||
					stricmp(pFilename, "hacking_pc") == 0 ||
					stricmp(pFilename, "hacking_message") == 0 ||
					stricmp(pFilename, "heli_cam") == 0 ||
					stricmp(pFilename, "camera_gallery") == 0)
				{
					ScriptScaleformMovie[iCount].bForceExactFit = true;
				}

				if (stricmp(pFilename, "mp_celebration") == 0 ||
					stricmp(pFilename, "mp_celebration_fg") == 0 ||
					stricmp(pFilename, "mp_celebration_bg") == 0 )
				{
					ScriptScaleformMovie[iCount].bSkipWidthOffset = true;
				}


				sfDebugf1("New Script id for %s is %d (SF %d)", pFilename, iCount+1, ScriptScaleformMovie[iCount].iId);
				return iCount+1;
			}
		}
	}

	return 0;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScriptHud::RemoveScaleformMovie
// PURPOSE:	removes ref and movie if ref is at 0
/////////////////////////////////////////////////////////////////////////////////////
void CScriptHud::RemoveScaleformMovie(s32 iId)
{
	// flag the movie as no longer used in the script id array:
	ScriptScaleformMovie[iId].bDeleteRequested = true;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScriptHud::GetScaleform
// PURPOSE:	Returns the actual movie id associated with this script id
/////////////////////////////////////////////////////////////////////////////////////
s32 CScriptHud::GetScaleformMovieID(s32 iId)
{
	s32 iMovieIndex = iId - 1;
	if(iMovieIndex < NUM_SCRIPT_SCALEFORM_MOVIES && iMovieIndex >= 0)
	{
		return ScriptScaleformMovie[iMovieIndex].iId;
	}
	
	return -1;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScriptHud::CheckIncomingFunctions
// PURPOSE:	listens for methods coming back to scripted scaleform movies from 
//			ActionScript
/////////////////////////////////////////////////////////////////////////////////////
void CScriptHud::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args)
{
	// public function REQUEST_REMOVALfilename:String):Void
	if (methodName == ATSTRINGHASH("REQUEST_REMOVAL",0x8852bd3f))
	{
		if (scriptVerifyf(args[1].IsString(), "REQUEST_REMOVAL params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			char cFilename[100];
			safecpy(cFilename, args[1].GetString(), NELEM(cFilename));

			// request that the movie is streamed in:
			bool bFoundFile = false;
			for (s32 iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES && !bFoundFile; iCount++)
			{
				if (!::strcmpi(ScriptScaleformMovie[iCount].cFilename, cFilename))
				{
	#if __DEV
					sfDebugf1("SF SCRIPT: ActionScript wants to delete movie %s id:%d.\n", cFilename, ScriptScaleformMovie[iCount].iId);
	#endif
					if (ScriptScaleformMovie[iCount].iParentMovie != -1)  // yes this is a child movie as it has a parent
					{
						if (CScaleformMgr::IsMovieActive(ScriptScaleformMovie[ScriptScaleformMovie[iCount].iParentMovie].iId))  // fixes 1411004 - movie is not considered active anyway now & about to be removed so no point in telling it to remove child movie
						{
							if (CScaleformMgr::BeginMethod(ScriptScaleformMovie[ScriptScaleformMovie[iCount].iParentMovie].iId, SF_BASE_CLASS_SCRIPT, "REMOVE_CHILD_MOVIE", ScriptScaleformMovie[iCount].iId))
							{
								CScaleformMgr::AddParamString(cFilename);
								CScaleformMgr::EndMethod();
							}
						}
					}

					RemoveScaleformMovie(iCount);

					bFoundFile = true;
				}
			}
		}

		return;
	}

	// public function REQUEST_GFX_STREAM(uid:Number, filename:String):Void
	if (methodName == ATSTRINGHASH("REQUEST_GFX_STREAM",0x9b2dc57c))
	{
		if (scriptVerifyf(args[1].IsNumber() && args[2].IsString() && args[3].IsString(), "REQUEST_GFX_STREAM params not compatible: %s %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3])))
		{
			// we have a request for a stream.
			char cRequestedFilename[100];
			char cParentFilename[100];
			s32 iId = static_cast<s32>(args[1].GetNumber());
			safecpy(cRequestedFilename, args[2].GetString(), NELEM(cRequestedFilename));
			safecpy(cParentFilename, args[3].GetString(), NELEM(cParentFilename));

	#if __BANK
			sfDebugf3("SF SCRIPT: ActionScript wants movie %s to load in '%s' with id %d.\n", cParentFilename, cRequestedFilename, iId);
	#endif
			s32 iParentId = -1;
			for (s32 iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES && iParentId == -1; iCount++)
			{
				if (!::strcmpi(ScriptScaleformMovie[iCount].cFilename, cParentFilename))
				{
					iParentId = iCount;
					break;
				}
			}

			//scriptAssertf(iParentId != -1, "Scaleform parent movie '%s' not found!", cParentFilename);
			if (iParentId == -1)  // fix 1539784 - too late to alter this.
			{
				// movie not found, its been deleted

				return;
			}

			s32 iNewId = -1;

			if (CScaleformMgr::DoesMovieExistInImage(cRequestedFilename))
			{
				iNewId = CScaleformMgr::CreateMovie(cRequestedFilename, Vector2(0,0), Vector2(0,0), true, ScriptScaleformMovie[iParentId].iId, -1, false, SF_MOVIE_TAGGED_BY_SCRIPT);
				scriptAssertf(iNewId != -1, "Scaleform movie '%s' failed to stream", cRequestedFilename);
			}

			if (iNewId != -1)
			{
				s32 iScriptId = SetupScaleformMovie(iNewId, cRequestedFilename) - 1;  // we dont need to adjust from -1 here as we are dealing with code, not script

	#if __BANK
				if (iScriptId == -1)  // if we failed, then dump all the slot info we have before it asserts:
				{
					for (s32 iTempIndex = 0; iTempIndex < NUM_SCRIPT_SCALEFORM_MOVIES; iTempIndex++)
					{
						scriptDisplayf("Scripted_Scaleform_Movie %d: %s (SF %d)", iTempIndex+1, ScriptScaleformMovie[iTempIndex].cFilename, ScriptScaleformMovie[iTempIndex].iId);
					}
				}
	#endif  // __BANK
				scriptAssertf(iScriptId >= 0, "Not enough Scaleform script slots for movie '%s' (Returned Id=%d)", cRequestedFilename, iScriptId);

				// found the requested filename - stream it in
				ScriptScaleformMovie[iScriptId].iLinkId = iId;  // store the link id we have for this movie
				ScriptScaleformMovie[iScriptId].iParentMovie = iParentId;

	// removed assert as this whole thing is getting replaced with a new system.			scriptAssertf(iCurrentRequestedScaleformMovieId == 0, "Script has not polled for id %d yet", iCurrentRequestedScaleformMovieId);
				iCurrentRequestedScaleformMovieId = iScriptId+1;
			}
			else
			{
				if (CScaleformMgr::BeginMethod(ScriptScaleformMovie[iParentId].iId, (eSCALEFORM_BASE_CLASS)(s32)args[0].GetNumber(), "STREAM_RESPONSE_FAILED", ScriptScaleformMovie[iParentId].iId))
				{
					CScaleformMgr::AddParamInt(iId);
					CScaleformMgr::EndMethod();
				}

				// only assert on SCRIPT movies (not Web pages)
	#if __ASSERT
				if ((eSCALEFORM_BASE_CLASS)(s32)args[0].GetNumber() == SF_BASE_CLASS_SCRIPT)
				{
					scriptAssertf(iNewId == -1, "SF SCRIPT: Cannot find gfx file %s in image!", cRequestedFilename);
				}
	#endif // __ASSERT
			}
		}

		return;
	}

	// public function GFX_READY filename:String):Void
	if (methodName == ATSTRINGHASH("GFX_READY",0xeeb22793))
	{
		if (scriptVerifyf(args[1].IsString(), "GFX_READY params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			char cFilename[100];
			safecpy(cFilename, args[1].GetString(), NELEM(cFilename));

			// confirm that the movie is ready:
			bool bFoundFile = false;
			for (s32 iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES && !bFoundFile; iCount++)
			{
				if (!::strcmpi(ScriptScaleformMovie[iCount].cFilename, cFilename))
				{
					ScriptScaleformMovie[iCount].bGfxReady = true;

					bFoundFile = true;
				}
			}
		}

		return;
	}

	// public function SET_CURRENT_WEBPAGE_ID filename:String):Void
	if (methodName == ATSTRINGHASH("SET_CURRENT_WEBPAGE_ID",0xb085bb2d) )
	{
		if (scriptVerifyf(args[1].IsNumber(), "SET_CURRENT_WEBPAGE_ID params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			s32 iCurrentWebpageId = (s32)args[1].GetNumber();

			scriptDisplayf("SET_CURRENT_WEBPAGE_ID set from actionscript with value %d", iCurrentWebpageId);

			CScriptHud::iCurrentWebpageIdFromActionScript = iCurrentWebpageId;
		}

		return;
	}

	// public function SET_CURRENT_WEBSITE_ID filename:String):Void
	if (methodName == ATSTRINGHASH("SET_CURRENT_WEBSITE_ID",0x4f7ef418) )
	{
		if (scriptVerifyf(args[1].IsNumber(), "SET_CURRENT_WEBSITE_ID params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			s32 iCurrentWebsiteId = (s32)args[1].GetNumber();

			scriptDisplayf("SET_CURRENT_WEBSITE_ID set from actionscript with value %d", iCurrentWebsiteId);

			CScriptHud::iCurrentWebsiteIdFromActionScript = iCurrentWebsiteId;
		}

		return;
	}

	// public function SET_CURRENT_WEBSITE_ID filename:String):Void
	if (methodName == ATSTRINGHASH("SET_GLOBAL_ACTIONSCRIPT_FLAG",0x7a6b07c0) )
	{
		if (scriptVerifyf(args[1].IsNumber() && args[2].IsNumber(), "SET_GLOBAL_ACTIONSCRIPT_FLAG params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			s32 iId = (s32)args[1].GetNumber();
			s32 iContent = (s32)args[2].GetNumber();

			if (scriptVerifyf(iId < MAX_ACTIONSCRIPT_FLAGS, "SET_GLOBAL_ACTIONSCRIPT_FLAG tried to use an id outwith the range (range = 0 to %d)", MAX_ACTIONSCRIPT_FLAGS-1))
			{
				scriptDisplayf("SET_GLOBAL_ACTIONSCRIPT_FLAG[%d] set from actionscript with value %d", iId, iContent);

				CScriptHud::iActionScriptGlobalFlags[iId] = iContent;
			}
		}

		return;
	}

#if RSG_DURANGO && COMPANION_APP
	if (methodName == ATSTRINGHASH("SMARTGLASS_SHOW_KEYBOARD", 0x5F2F256C))
	{
		// no parameters
		CCompanionData::GetInstance()->SetTextModeForCurrentDevice(true);
	}

	if (methodName == ATSTRINGHASH("SMARTGLASS_ENTER_TEXT", 0x1754A21F))
	{
		if (uiVerifyf(args[1].IsString(), "SMARTGLASS_ENTER_TEXT params must be a string: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			const atString textEntry = atString(args[1].GetString());

			CCompanionData::GetInstance()->SetTextEntryForCurrentDevice(textEntry);
		}
	}

	if (methodName == ATSTRINGHASH("SMARTGLASS_HIDE_KEYBOARD", 0xDC27865F))
	{
		CCompanionData::GetInstance()->SetTextModeForCurrentDevice(false);
	}
#endif
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScriptHud::ResetScaleformComponentsOnRTPerFrame
// PURPOSE:	goes through each movie before any render calls are made and sorts out
//			the status of the rendering of each movie
/////////////////////////////////////////////////////////////////////////////////////
void CScriptHud::ResetScaleformComponentsOnRTPerFrame()
{
	for (s32 iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES; iCount++)
	{
		if (ScriptScaleformMovie[iCount].bActive)
		{
			if ( (ScriptScaleformMovie[iCount].bRenderedOnce) && (!ScriptScaleformMovie[iCount].bRenderedThisFrame) ) // if rendered once AND not rendered this frame, then reset back to render 1st time again
			{
				ScriptScaleformMovie[iCount].bRenderedOnce = false;
			}
			else
			{
				ScriptScaleformMovie[iCount].bRenderedThisFrame = false;  // reset so we can know whether its rendered this frame later on
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScriptHud::UpdateScaleformComponents
// PURPOSE:	updates each script scaleform component and deals with any deletion etc
/////////////////////////////////////////////////////////////////////////////////////
void CScriptHud::UpdateScaleformComponents()
{
	PF_PUSH_TIMEBAR_DETAIL("Scaleform Movie Update (Script)");

	for (s32 iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES; iCount++)
	{
		if (ScriptScaleformMovie[iCount].bActive)
		{
// #if __DEV
// 			scriptDisplayf("SCALEFORM SCRIPT MOVIE: %s %d (SF %d)", ScriptScaleformMovie[iCount].cFilename, iCount+1, ScriptScaleformMovie[iCount].iId);
// #endif // __DEV

			// reset if the movie is getting deleted, so the slot can be reused
			if (ScriptScaleformMovie[iCount].bDeleteRequested)
			{
				DeleteScriptScaleformMovie(iCount);
			}
		}
		else
		{
			// it has now loaded, so set to be active
			if (CScaleformMgr::IsMovieActive(ScriptScaleformMovie[iCount].iId))
			{
				ScriptScaleformMovie[iCount].bRenderedOnce = false;  // this gets set to true once the movie has been rendered for the 1st time by script - this allows us to invoke just before the render but only the 1st time

				// if we have a parent link, we need to inform ActionScript that this movie has loaded in via the parent movie (which needs to be active and usable before we can invoke)
				if (ScriptScaleformMovie[iCount].iParentMovie != -1 && CScaleformMgr::IsMovieActive(ScriptScaleformMovie[ScriptScaleformMovie[iCount].iParentMovie].iId))  // ensure its usable before we invoke
				{
					if (CScaleformMgr::BeginMethod(ScriptScaleformMovie[ScriptScaleformMovie[iCount].iParentMovie].iId, SF_BASE_CLASS_SCRIPT, "STREAM_RESPONSE", ScriptScaleformMovie[iCount].iId))
					{
						CScaleformMgr::AddParamInt(ScriptScaleformMovie[iCount].iLinkId);
						CScaleformMgr::AddParamString(ScriptScaleformMovie[iCount].cFilename);
						CScaleformMgr::EndMethod();

						ScriptScaleformMovie[iCount].bActive = true;  // set as active now everything is ready
					}
				}
				else
				{
					ScriptScaleformMovie[iCount].bActive = true;  // no parent but its loaded, so set to be active
				}
			}
		}
	}
	PF_POP_TIMEBAR_DETAIL();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScriptHud::CallShutdownMovie
// PURPOSE:	ensures any script movies that have children have "shutdown_movie"
//			invoked on them to fix 2710868
/////////////////////////////////////////////////////////////////////////////////////
void CScriptHud::CallShutdownMovie(const s32 c_movieIndex)
{
	scriptDisplayf("CallShutdownMovie - Shutdown on movie index %d", c_movieIndex);

	// if this movie is a parent movie of another, then we need to trigger a shutdown movie call on it
	for (s32 childCounter = 0; childCounter < NUM_SCRIPT_SCALEFORM_MOVIES; childCounter++)
	{
		if (c_movieIndex == childCounter)
			continue;

		// found a child of this movie so request to remove that too: (child's parent link matches the movie getting deleted)
		if (ScriptScaleformMovie[childCounter].bActive && ScriptScaleformMovie[childCounter].iParentMovie == c_movieIndex)
		{
			const char *cp_shutdownMethodName = "SHUTDOWN_MOVIE";

			if (CScaleformMgr::IsFunctionAvailable(c_movieIndex, SF_BASE_CLASS_SCRIPT, cp_shutdownMethodName))
			{
				scriptDisplayf("CallShutdownMovie - Shutting down movie index %d", c_movieIndex);
				CScaleformMgr::CallMethod(c_movieIndex, SF_BASE_CLASS_SCRIPT, cp_shutdownMethodName, true);
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScriptHud::DeleteScriptScaleformMovie
// PURPOSE:	deletes one scripted scaleform movie and any children attached to it
/////////////////////////////////////////////////////////////////////////////////////
void CScriptHud::DeleteScriptScaleformMovie(s32 iIndex, bool bForceDelete)
{
//	scriptDisplayf("Movie %s (%d) has been removed from the script slot", ScriptScaleformMovie[iIndex].cFilename, iIndex+1);

	// if this movie is a parent movie of another, then we want to remove all of its children...
	for (s32 iChildCount = 0; iChildCount < NUM_SCRIPT_SCALEFORM_MOVIES; iChildCount++)
	{
		if (iIndex == iChildCount)
			continue;

		// found a child of this movie so request to remove that too: (child's parent link matches the movie getting deleted)
		if (ScriptScaleformMovie[iChildCount].bActive && ScriptScaleformMovie[iChildCount].iParentMovie == iIndex)
		{
			RemoveScaleformMovie(iChildCount);
		}
	}

	// remove this movie:
	CScaleformMgr::SetScriptRequired(ScriptScaleformMovie[iIndex].iId, false);  // set as not required by script
	CScaleformMgr::RequestRemoveMovie(ScriptScaleformMovie[iIndex].iId, SF_MOVIE_TAGGED_BY_SCRIPT);  // flag the movie as no longer needed
	
	if(bForceDelete)
	{
		CScaleformMgr::SetForceShutdown(ScriptScaleformMovie[iIndex].iId);
	}

	ResetScriptScaleformMovieVariables(iIndex);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScriptHud::ResetScriptScaleformMovieVariables
// PURPOSE:	resets all the variables in the ScriptScaleformMovie struct to defaults
/////////////////////////////////////////////////////////////////////////////////////
void CScriptHud::ResetScriptScaleformMovieVariables(s32 iIndex)
{
	ScriptScaleformMovie[iIndex].bActive = false;
	ScriptScaleformMovie[iIndex].bRenderedOnce = false;
	ScriptScaleformMovie[iIndex].iBackgroundMaskedMovieId = -1;
	ScriptScaleformMovie[iIndex].bInvokedBeforeRender = false;
	ScriptScaleformMovie[iIndex].bRenderedThisFrame = false;
	ScriptScaleformMovie[iIndex].bDeleteRequested = false;
	ScriptScaleformMovie[iIndex].iId = -1;
	ScriptScaleformMovie[iIndex].iParentMovie = -1;
	ScriptScaleformMovie[iIndex].bGfxReady = false;
	ScriptScaleformMovie[iIndex].bUseSystemTimer = false;
	ScriptScaleformMovie[iIndex].iLinkId = -1;
	ScriptScaleformMovie[iIndex].cFilename[0] = '\0';
	ScriptScaleformMovie[iIndex].bSkipWidthOffset = false;
	ScriptScaleformMovie[iIndex].bIgnoreSuperWidescreen = false;
	ScriptScaleformMovie[iIndex].iLargeRT = 0;
};



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScriptHud::DrawScriptGraphics
// PURPOSE:	renders scripted graphics
/////////////////////////////////////////////////////////////////////////////////////
void CScriptHud::DrawScriptGraphics(int CurrentRenderID, u32 iDrawOrderValue)
{
#if __BANK
	if (!CHudTools::bDisplayHud)  // dont render any script text if this debug flag is set
		return;
#endif
#if __ASSERT && RSG_PC
	if (CurrentRenderID == CRenderTargetMgr::RTI_PhoneScreen)
	{
		Assert(DRAWLISTMGR->IsBuildingScriptDrawList());
	}
#endif

	if( FreezeRendering() )
	{
		ms_DBIntroRects[0].PushRefCount();
	}

	DLC_Add(DrawScriptSpritesAndRectangles, CurrentRenderID, (iDrawOrderValue | SCRIPT_GFX_ORDER_PRIORITY_LOW));
	DLC_Add(DrawScriptText, CurrentRenderID, (iDrawOrderValue | SCRIPT_GFX_ORDER_PRIORITY_LOW));

	DLC_Add(DrawScriptSpritesAndRectangles, CurrentRenderID, (iDrawOrderValue));
	DLC_Add(DrawScriptText, CurrentRenderID, (iDrawOrderValue));

	DLC_Add(DrawScriptSpritesAndRectangles, CurrentRenderID, (iDrawOrderValue | SCRIPT_GFX_ORDER_PRIORITY_HIGH));
	DLC_Add(DrawScriptText, CurrentRenderID, (iDrawOrderValue | SCRIPT_GFX_ORDER_PRIORITY_HIGH));
}



#if __BANK
void CScriptHud::DrawScriptScaleformDebugInfo()
{
	//
	// display some debug info to the screen so we can display what script scaleform movies are in use and their current state/condition:
	//
	if (CScriptHud::bDisplayScriptScaleformMovieDebug)
	{
		float fOffset = 0.0f;

		for (s32 iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES; iCount++)
		{
			if (ScriptScaleformMovie[iCount].iId != -1)
			{
				char cDebugString[512];

				sprintf(cDebugString, "id %d : %s", iCount+1, ScriptScaleformMovie[iCount].cFilename);

				if (ScriptScaleformMovie[iCount].bActive)
					safecat(cDebugString, " : Active", 512);

				if (ScriptScaleformMovie[iCount].bDeleteRequested)
					safecat(cDebugString, " : Delete Requested", 512);

				if (!ScriptScaleformMovie[iCount].bGfxReady && !ScriptScaleformMovie[iCount].bActive)
					safecat(cDebugString, " : Loading", 512);

				if (ScriptScaleformMovie[iCount].iParentMovie != -1)
				{
					char cSubDebugString[32];
					sprintf(cSubDebugString, "%d", ScriptScaleformMovie[iCount].iParentMovie+1);
					safecat(cDebugString, " : Child Movie of ", 512);
					safecat(cDebugString, cSubDebugString, 512);
				}

				CTextLayout DebugTextLayout;

				DebugTextLayout.SetScale(Vector2(0.13f, 0.28f));
				DebugTextLayout.SetColor(CRGBA(225,225,225,255));

				DebugTextLayout.Render(Vector2(0.04f, 0.45f+fOffset), cDebugString );

				fOffset += 0.024f;
			}
		}
	}
}
#endif // __BANK


bool CScriptHud::ShouldNonMiniGameHelpMessagesBeDisplayed()
{
	return (CTheScripts::GetNumberOfMiniGamesInProgress() == NumberOfMiniGamesAllowingNonMiniGameHelpMessages);
}

bool CScriptHud::GetScreenPosFromWorldCoord(const Vector3 &vecWorldPos, float &NormalisedX, float &NormalisedY)
{
	const grcViewport *pCurrentGrcViewport = gVpMan.GetCurrentGameGrcViewport();

	if(pCurrentGrcViewport->IsSphereVisible( vecWorldPos.x, vecWorldPos.y, vecWorldPos.z, 0.01f))
	{
		float fWindowX = 0.0f, fWindowY = 0.0f;
		pCurrentGrcViewport->Transform(RCC_VEC3V(vecWorldPos), fWindowX, fWindowY);

		float fWidth = (float)pCurrentGrcViewport->GetWidth();
		float fHeight = (float)pCurrentGrcViewport->GetHeight();

#if SUPPORT_MULTI_MONITOR
		if(GRCDEVICE.GetMonitorConfig().isMultihead())
		{
			const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
			float fOffset = fWidth * mon.getArea().GetWidth();
			fWidth = fOffset;
			fWindowX -= fWidth;
		}
#endif // SUPPORT_MULTI_MONITOR
		NormalisedX = fWindowX / fWidth;
		NormalisedY = fWindowY / fHeight;

		if((NormalisedX < 0.0f || NormalisedX >1.0f) ||  (NormalisedY < 0.0f || NormalisedY > 1.0f))
		{
			NormalisedX=-1.0f;
			NormalisedY=-1.0f;
			return false;
		}
		return true;
	}

	NormalisedX=-1.0f;
	NormalisedY=-1.0f;
	return false;
}

bool CScriptHud::GetFakedPauseMapPlayerPos(Vector2 &vFakePlayerPos)
{
	if ( (ms_iCurrentFakedInteriorHash != 0 && !CMiniMap::IsInCustomMap()) &&
		( (CPauseMenu::IsActive() && !CMapMenu::GetIsInInteriorMode()) || CMiniMap::IsInBigMap()) )
	{
		// We're currently faking the player's exterior position for the duration of this interior and showing an exterior map
		vFakePlayerPos = vInteriorFakePauseMapPlayerPos;
		return true;
	}

	if (vFakePauseMapPlayerPos.x != INVALID_FAKE_PLAYER_POS && vFakePauseMapPlayerPos.y != INVALID_FAKE_PLAYER_POS)
	{
		// We're faking this frame
		vFakePlayerPos = vFakePauseMapPlayerPos;
		return true;
	}

	return false;
}

bool CScriptHud::GetFakedGPSPlayerPos( Vector3 &vFakePlayerPos )
{
    bool result = false;

    if (vFakeGPSPlayerPos.x != INVALID_FAKE_PLAYER_POS && vFakeGPSPlayerPos.y != INVALID_FAKE_PLAYER_POS)
    {
        // We're faking this frame
        vFakePlayerPos = vFakeGPSPlayerPos;
        result = true;
    }

    return result;
}

void CDoubleBufferedSingleFrameLiterals::Initialise(bool bResetArray)
{
	if (bResetArray)
	{
		for (int loop = 0; loop < SCRIPT_TEXT_NUM_SINGLE_FRAME_LITERAL_STRINGS; loop++)
		{
			m_LiteralString[loop][0] = 0;
		}
	}

	m_NumberOfReleaseStrings = 0;
#if !__FINAL
	m_NumberOfDebugStrings = 0;
#endif
}

bool CDoubleBufferedSingleFrameLiterals::FindFreeIndex(bool NOTFINAL_ONLY(bUseDebugPortionOfSingleFrameArray), s32 &ReturnIndex)
{
#if !__FINAL
	if (bUseDebugPortionOfSingleFrameArray)
	{
		if (m_NumberOfDebugStrings < SCRIPT_TEXT_NUM_DEBUG_SINGLE_FRAME_LITERAL_STRINGS)
		{
			ReturnIndex = SCRIPT_TEXT_NUM_RELEASE_SINGLE_FRAME_LITERAL_STRINGS + m_NumberOfDebugStrings;
			m_NumberOfDebugStrings++;
			return true;
		}
		else
		{
			scriptDisplayf( "Exhausted amount of debug literal strings available! Need to up SCRIPT_TEXT_NUM_DEBUG_SINGLE_FRAME_LITERAL_STRINGS" );
		}
	}
	else
#endif	//	__FINAL
	{
		if (m_NumberOfReleaseStrings < SCRIPT_TEXT_NUM_RELEASE_SINGLE_FRAME_LITERAL_STRINGS)
		{
			ReturnIndex = m_NumberOfReleaseStrings;
			m_NumberOfReleaseStrings++;
			return true;
		}
	}

	return false;
}

#if __ASSERT
void CDoubleBufferedSingleFrameLiterals::DisplayAllLiteralStrings()
{
	u32 loop = 0;
	for (loop = 0; loop < m_NumberOfReleaseStrings; loop++)
	{
		scriptDisplayf("%d %s", loop, m_LiteralString[loop]);
	}

#if !__FINAL
	for (loop = 0; loop < m_NumberOfDebugStrings; loop++)
	{
		u32 debugIndex = SCRIPT_TEXT_NUM_RELEASE_SINGLE_FRAME_LITERAL_STRINGS + loop;
		scriptDisplayf("%d %s", debugIndex, m_LiteralString[debugIndex] );
	}
#endif
}
#endif


// ******************************************************
//	CLiteralStringsForImmediateUse
// ******************************************************
void CLiteralStringsForImmediateUse::AllocateMemory(u32 numberOfLiterals, u32 maxStringLength)
{
	for (u32 threadLoop = 0; threadLoop < 2; threadLoop++)
	{
		m_LiteralStringsForThreads[threadLoop].m_pLiteralStrings.Reserve(numberOfLiterals);

		for (u32 stringLoop = 0; stringLoop < numberOfLiterals; stringLoop++)
		{
			char* &newArrayElement = m_LiteralStringsForThreads[threadLoop].m_pLiteralStrings.Append();
			newArrayElement = rage_new char[maxStringLength];
		}

		m_LiteralStringsForThreads[threadLoop].m_NumberOfUsedLiteralStrings = 0;
	}

	m_MaxStringsInThread = numberOfLiterals;
	m_MaxCharsInString = maxStringLength;
}

void CLiteralStringsForImmediateUse::FreeMemory()
{
	for (u32 threadLoop = 0; threadLoop < 2; threadLoop++)
	{
		for (u32 stringLoop = 0; stringLoop < m_MaxStringsInThread; stringLoop++)
		{
			delete[] m_LiteralStringsForThreads[threadLoop].m_pLiteralStrings[stringLoop];
			m_LiteralStringsForThreads[threadLoop].m_pLiteralStrings[stringLoop] = NULL;
		}

		m_LiteralStringsForThreads[threadLoop].m_pLiteralStrings.Reset();
	}

	m_MaxStringsInThread = 0;
	m_MaxCharsInString = 0;
}


void CLiteralStringsForImmediateUse::ClearStrings(bool bClearBothBuffers)
{
	if (bClearBothBuffers)
	{
		for (u32 threadLoop = 0; threadLoop < 2; threadLoop++)
		{
			for (int loop = 0; loop < m_MaxStringsInThread; loop++)
			{
				m_LiteralStringsForThreads[threadLoop].m_pLiteralStrings[loop][0] = 0;
			}

			m_LiteralStringsForThreads[threadLoop].m_NumberOfUsedLiteralStrings = 0;
		}
	}
	else
	{
		if (sysThreadType::IsUpdateThread() || sysThreadType::IsRenderThread())
		{	//	Fix for Bug 1899649. This function is getting called by CControlMgr::UpdateThread when you press D or Tab on the keyboard.
			//	If any threads other than the Update or Render thread actually need to add these literal strings then I might have to increase the size of m_LiteralStringsForThreads[2]
			//	and update GetIndexForCurrentThread() to return an index for each of these other threads.
			const u32 indexForCurrentThread = GetIndexForCurrentThread();

			for (int loop = 0; loop < m_MaxStringsInThread; loop++)
			{
				m_LiteralStringsForThreads[indexForCurrentThread].m_pLiteralStrings[loop][0] = 0;
			}

			m_LiteralStringsForThreads[indexForCurrentThread].m_NumberOfUsedLiteralStrings = 0;
		}
	}
}

bool CLiteralStringsForImmediateUse::FindFreeIndex(s32 &ReturnIndex)
{
	const u32 indexForCurrentThread = GetIndexForCurrentThread();

	if (m_LiteralStringsForThreads[indexForCurrentThread].m_NumberOfUsedLiteralStrings < m_MaxStringsInThread)
	{
		ReturnIndex = m_LiteralStringsForThreads[indexForCurrentThread].m_NumberOfUsedLiteralStrings;
		m_LiteralStringsForThreads[indexForCurrentThread].m_NumberOfUsedLiteralStrings++;
		return true;
	}

	return false;
}

void CLiteralStringsForImmediateUse::SetLiteralString(s32 LiteralStringIndex, const char *pNewString)
{
	const u32 indexForCurrentThread = GetIndexForCurrentThread();
	if (scriptVerifyf(pNewString, "CLiteralStringsForImmediateUse::SetLiteralString - pNewString is NULL"))
	{
		scriptAssertf(strlen(pNewString) < m_MaxCharsInString, "Attempting to copy a string, but it's longer than our allowed limit of %d, and will be truncated! String was: \"%s\"", m_MaxCharsInString, pNewString);
		safecpy(m_LiteralStringsForThreads[indexForCurrentThread].m_pLiteralStrings[LiteralStringIndex], pNewString, (m_MaxCharsInString - 1) );
	}
	else
	{
		safecpy(m_LiteralStringsForThreads[indexForCurrentThread].m_pLiteralStrings[LiteralStringIndex], "", (m_MaxCharsInString - 1) );
	}
}

const char *CLiteralStringsForImmediateUse::GetLiteralString(s32 LiteralStringIndex) const
{
	static char emptyString[2] = "";

	const u32 indexForCurrentThread = GetIndexForCurrentThread();

	if (scriptVerifyf( (indexForCurrentThread == 0) || (indexForCurrentThread == 1), "CLiteralStringsForImmediateUse::GetLiteralString - expected indexForCurrentThread to be 0 or 1, but it's %u", indexForCurrentThread))
	{
		if (scriptVerifyf( (LiteralStringIndex >= 0) && (LiteralStringIndex < m_LiteralStringsForThreads[indexForCurrentThread].m_NumberOfUsedLiteralStrings), "CLiteralStringsForImmediateUse::GetLiteralString - LiteralStringIndex is %d. Expected it to be >= 0 and < %u", LiteralStringIndex, m_LiteralStringsForThreads[indexForCurrentThread].m_NumberOfUsedLiteralStrings))
		{
			if (scriptVerifyf(m_LiteralStringsForThreads[indexForCurrentThread].m_pLiteralStrings[LiteralStringIndex], "CLiteralStringsForImmediateUse::GetLiteralString - literal string %d for thread %u is NULL so returning an empty string instead", LiteralStringIndex, indexForCurrentThread))
			{
				return m_LiteralStringsForThreads[indexForCurrentThread].m_pLiteralStrings[LiteralStringIndex];
			}
		}
	}

	return emptyString;
}

u32 CLiteralStringsForImmediateUse::GetIndexForCurrentThread() const
{
	if (sysThreadType::IsUpdateThread())
	{
		return 0;
	}
	else
	{
		scriptAssertf(sysThreadType::IsRenderThread(), "CLiteralStringsForImmediateUse::GetIndexForCurrentThread - if this hasn't been called on the update thread then we expect it to be the render thread");
		return 1;
	}
}

#if __ASSERT
void CLiteralStringsForImmediateUse::DisplayAllLiteralStrings() const
{
//	const u32 MAX_CHARS_TO_DISPLAY_IN_DEBUG_OUTPUT = 200;

//	char cTempAsciiString[MAX_CHARS_TO_DISPLAY_IN_DEBUG_OUTPUT];
	const u32 indexForCurrentThread = GetIndexForCurrentThread();

	for (u32 loop = 0; loop < m_LiteralStringsForThreads[indexForCurrentThread].m_NumberOfUsedLiteralStrings; loop++)
	{
		scriptDisplayf("%d %s", loop, m_LiteralStringsForThreads[indexForCurrentThread].m_pLiteralStrings[loop]);
	}
}
#endif



// ******************************************************
//	CLiteralStrings
// ******************************************************
void CLiteralStrings::Initialise(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		m_LiteralStringsForImmediateUse.AllocateMemory(10, 100);			//	LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE
		m_LongLiteralStringsForImmediateUse.AllocateMemory(2, 300);			//	LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG
	}

	for (int loop = 0; loop < SCRIPT_TEXT_NUM_PERSISTENT_LITERAL_STRINGS; loop++)
	{
		m_PersistentLiteralStrings[loop].Initialise();
	}

	ClearSingleFrameStrings(true);

	ClearArrayOfImmediateUseLiteralStrings(true);
}

void CLiteralStrings::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		m_LiteralStringsForImmediateUse.FreeMemory();
		m_LongLiteralStringsForImmediateUse.FreeMemory();
	}
}

void CLiteralStrings::ClearSingleFrameStrings(bool bResetSingleFrameArray)
{
	m_DBSingleFrameLiteralStrings.GetWriteBuf().Initialise(bResetSingleFrameArray);
}


void CPersistentLiteral::Clear(bool bClearingFromPreviousBriefs)
{
	if (bClearingFromPreviousBriefs)
	{
		m_bOccursInPreviousBriefs = false;
	}
	else
	{
		m_bOccursInSubtitles = false;
	}

	if ( (m_bOccursInPreviousBriefs == false)
		&& (m_bOccursInSubtitles == false) )
	{
		Initialise();
	}
}

void CLiteralStrings::ClearPersistentString(int StringIndex, bool bClearingFromPreviousBriefs)
{
	if (StringIndex >= 0)
	{
		m_PersistentLiteralStrings[StringIndex].Clear(bClearingFromPreviousBriefs);
	}
}

void CLiteralStrings::ClearArrayOfImmediateUseLiteralStrings(bool bClearBothBuffers)
{
	m_LiteralStringsForImmediateUse.ClearStrings(bClearBothBuffers);
	m_LongLiteralStringsForImmediateUse.ClearStrings(bClearBothBuffers);
}

#if __ASSERT
void CLiteralStrings::DisplayAllLiteralStrings(CSubStringWithinMessage::eLiteralStringType literalStringType)
{
	switch (literalStringType)
	{
		case CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT :
			{
				int loop = 0;

				while (loop < SCRIPT_TEXT_NUM_PERSISTENT_LITERAL_STRINGS)
				{
					if (m_PersistentLiteralStrings[loop].GetIsUsed())
					{
						scriptDisplayf("%d %s", loop, m_PersistentLiteralStrings[loop].Get() );
					}
					loop++;
				}
			}
			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_SINGLE_FRAME :
			{
				m_DBSingleFrameLiteralStrings.GetWriteBuf().DisplayAllLiteralStrings();
			}
			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE :
			m_LiteralStringsForImmediateUse.DisplayAllLiteralStrings();
			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG :
			m_LongLiteralStringsForImmediateUse.DisplayAllLiteralStrings();
			break;
	}
}
#endif	//	__ASSERT

void CLiteralStrings::SetPersistentLiteralOccursInPreviousBriefs(int StringIndex, bool bValue)
{
	if (scriptVerifyf(StringIndex >= 0, "CLiteralStrings::SetPersistentLiteralOccursInPreviousBriefs - string index should be >= 0"))
	{
		m_PersistentLiteralStrings[StringIndex].SetOccursInPreviousBriefs(bValue);
	}
}


void CLiteralStrings::SetPersistentLiteralOccursInSubtitles(int StringIndex, bool bValue)
{
	if (scriptVerifyf(StringIndex >= 0, "CLiteralStrings::SetPersistentLiteralOccursInSubtitles - string index should be >= 0"))
	{
		m_PersistentLiteralStrings[StringIndex].SetOccursInSubtitles(bValue);
	}
}


const char *CLiteralStrings::GetLiteralString(int StringIndex, CSubStringWithinMessage::eLiteralStringType literalStringType) const
{
	switch (literalStringType)
	{
		case CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT :
			return m_PersistentLiteralStrings[StringIndex].Get();
//			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_SINGLE_FRAME :
			{
				scriptAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CLiteralStrings::GetLiteralString - only expected this to be called by the render thread");
				int bufferIdx = CScriptHud::GetReadbufferIndex();
				return m_DBSingleFrameLiteralStrings[bufferIdx].GetLiteralString(StringIndex);
			}
//			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE :
			return m_LiteralStringsForImmediateUse.GetLiteralString(StringIndex);
//			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG :
			return m_LongLiteralStringsForImmediateUse.GetLiteralString(StringIndex);
//			break;
	}

	return NULL;
}

const char *CLiteralStrings::GetSingleFrameLiteralStringFromWriteBuffer(s32 StringIndex)
{
	scriptAssertf(!CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CLiteralStrings::GetSingleFrameLiteralStringFromWriteBuffer - didn't expect this to be called by the render thread");
	return m_DBSingleFrameLiteralStrings.GetWriteBuf().GetLiteralString(StringIndex);
}

