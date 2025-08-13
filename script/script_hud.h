#ifndef _SCRIPT_HUD_H_
#define _SCRIPT_HUD_H_

// Rage headers
#include "atl/array.h"
#include "grcore/effect.h"
#include "grcore/stateblock.h"
#include "scaleform/scaleform.h"

// Game headers
#include "SaveLoad/savegame_queued_operations.h"
#include "text/TextFile.h"
#include "frontend/NewHud.h"
#include "frontend/hud_colour.h"
#include "frontend/MousePointer.h"
#include "renderer/color.h"
#include "renderer/rendertargetmgr.h"
#include "text/TextConversion.h"


//
// eHudOverwrite - this means we can have commands force stuff on and off or leave alone with just 1 command.
//
enum eHudOverwrite
{
	HUD_OVERWRITE_FORCE_OFF = 0,
	HUD_OVERWRITE_FORCE_ON,
	HUD_OVERWRITE_UNTOUCH
};

enum eScriptGfxDrawProperties
{
	SCRIPT_GFX_ORDER_BEFORE_HUD		= 1 << 1,
	SCRIPT_GFX_ORDER_AFTER_HUD		= 1 << 2,
	SCRIPT_GFX_ORDER_AFTER_FADE		= 1 << 3,

	SCRIPT_GFX_VISIBLE_WHEN_PAUSED	= 1 << 4,

	SCRIPT_GFX_ORDER_PRIORITY_LOW	= 1 << 5,
	SCRIPT_GFX_ORDER_PRIORITY_HIGH	= 1 << 6,
};

// PURPOSE: Class defining how a hud element aligns with the safezone
class CHudAlignment
{
public:
	CHudAlignment() {Reset();}
	void Reset() {m_alignX='I'; m_alignY='I'; m_offset.Zero(); m_size.Zero();}
	bool IsReset() {return (m_alignX=='I' && m_alignY=='I' && m_offset.IsZero() && m_size.IsZero());}
	Vector2 CalculateHudPosition(Vector2 pos);
	Vector2 CalculateHudPosition(Vector2 pos, Vector2 size, bool bDoScriptAdjustment = true);

	u8	m_alignX;
	u8	m_alignY;
	Vector2 m_offset;
	Vector2 m_size;
};

struct sRadarMessage
{
	bool bActivate;
	s32 iType;
	char cMessage[TEXT_KEY_SIZE];
	s32 iNumber;
	bool bMessagesWaiting;
	bool bSleepModeActive;
};


struct sMissionPassedCashMessage
{
	bool bActivate;
	char cMessage[50];
	eHUD_COLOURS iHudColour;
};

struct sScaleformComponent
{
	s32 iId;
	bool bActive;
	bool bDeleteRequested;
	bool bGfxReady;
	bool bRenderedOnce;
	s32 iBackgroundMaskedMovieId;
	bool bInvokedBeforeRender;
	bool bRenderedThisFrame;
	bool bUseSystemTimer;
	bool bIgnoreSuperWidescreen;
	u8 iLargeRT;
	s32 iParentMovie;
	s32 iLinkId;
	char cFilename[40];
	bool bForceExactFit;
	bool bSkipWidthOffset;
};

#if ENABLE_DEBUG_HEAP
	#define SCRIPT_TEXT_NUM_DEBUG_LINES		(720)
#endif

#define SCRIPT_TEXT_LABEL_LENGTH		(20)


class CAbovePlayersHeadDisplay
{
public:
	void Reset(bool bIsConstructing);
	void AddRow(const char *pRowToAdd);
	void FillStringWithRows(char *pStringToFill, u32 MaxLengthOfString);

	bool HasFreeRows() { return m_NumberOfRows < MaxNumberOfRows; }
	bool IsConstructing() { return m_bIsConstructingAbovePlayersHeadDisplay; }

private:
	static const u32 MaxNumberOfCharactersInOneRow = 64;
	static const u32 MaxNumberOfRows = 6;

	char m_TextRows[MaxNumberOfRows][MaxNumberOfCharactersInOneRow];
	u32 m_NumberOfRows;
	bool m_bIsConstructingAbovePlayersHeadDisplay;
};


class CDisplayTextFormatting
{
public:
	void Reset();
	void SetTextLayoutForDraw(CTextLayout &ScriptTextLayout, Vector2 *pvPosition) const;
	float GetCharacterHeightOnScreen();
	float GetTextPaddingOnScreen();
	float GetStringWidthOnScreen(char *pCharacters, bool bIncludeSpaces);
	s32 GetNumberOfLinesForString(float DisplayAtX, float DisplayAtY, char *pString);

	//	Setters
	void SetColor(CRGBA &Color) { m_ScriptTextColor = Color; }
	void SetTextDropShadow(bool bDropAmount) { /* m_ScriptTextDropShadowColour = DropShadowColour; */ m_bScriptTextDropShadow = bDropAmount; }
	void SetTextEdge(bool bEdgeAmount) { /* m_ScriptTextDropShadowColour = Colour; */ m_bScriptTextEdge = bEdgeAmount; }

	void SetTextScale(float TextXScale, float TextYScale) { m_ScriptTextXScale = TextXScale; m_ScriptTextYScale = TextYScale; }
	void SetTextWrap(float textWrapStart, float textWrapEnd) { m_ScriptTextWrapStartX = textWrapStart; m_ScriptTextWrapEndX = textWrapEnd; }

	void SetTextFontStyle(s32 FontStyle) { m_ScriptTextFontStyle = FontStyle; }

	void SetScriptWidescreenFormat(eWIDESCREEN_FORMAT NewFormat, bool bHasBeenChanged) { m_ScriptWidescreenFormat = NewFormat; m_bScriptUsingWidescreenAdjustment = bHasBeenChanged;}

#if RSG_PC
	void SetStereo(int StereoFlag)	{ m_StereoFlag = StereoFlag; }
	int GetStereo()				{ return m_StereoFlag; }
#endif

	void SetTextJustification(u8 Justification) { m_ScriptTextJustification = Justification; }

	void SetTextBackground(bool bTextBackground) { m_ScriptTextBackgrnd = bTextBackground; }
	void SetTextLeading(u8 iTextLeading) { m_ScriptTextLeading = iTextLeading; }
	void SetUseTextFileColours(bool bUseTextColours) { m_ScriptTextUseTextFileColours = bUseTextColours; }

	float GetWidth() {return m_ScriptTextWrapEndX - m_ScriptTextWrapStartX;}
	float GetHeight() {return m_ScriptTextYScale;}
	void OffsetMargins(float offset);

#if __BANK
	void OutputDebugInfo(bool bDisplay, float fPosX, float fPosY);
#endif  // __BANK

private:
	CRGBA m_ScriptTextColor;

	float m_ScriptTextXScale;
	float m_ScriptTextYScale;

	float m_ScriptTextWrapStartX;
	float m_ScriptTextWrapEndX;

	s32 m_ScriptTextFontStyle;

	eWIDESCREEN_FORMAT m_ScriptWidescreenFormat;

	u8 m_ScriptTextJustification;
	u8 m_ScriptTextLeading;

	bool m_bScriptTextDropShadow;
	bool m_bScriptTextEdge;
	bool m_ScriptTextBackgrnd;
	bool m_ScriptTextUseTextFileColours;
	bool m_bScriptUsingWidescreenAdjustment;
#if RSG_PC
	int m_StereoFlag;
#endif
};


class CDisplayTextBaseClass
{
public:

	virtual ~CDisplayTextBaseClass() {}

	void Reset(int OrthoRenderID);

	void Initialise(float fPosX, float fPosY, s32 renderID, s32 indexOfDrawOrigin, 
		u32 drawProperties, CDisplayTextFormatting &formattingForText, 
		const char *pMainTextLabel, 
		CNumberWithinMessage *pNumbersArray, u32 numberOfNumbers, 
		CSubStringWithinMessage *pSubStringsArray, u32 numberOfSubStrings);

	void Draw(s32 CurrentRenderID, u32 iDrawOrderValue) const;

#if !__FINAL
	virtual void DebugPrintContents();
#endif	//	!__FINAL

private:
	CDisplayTextFormatting m_Formatting;

	char m_ScriptTextLabel[SCRIPT_TEXT_LABEL_LENGTH];	//	store the label rather than char* so that missing text labels can be reported correctly

	s32 m_RenderID;
	s32 m_IndexOfDrawOrigin;
	float m_ScriptTextAtX;
	float m_ScriptTextAtY;

	u32 m_ScriptTextDrawProperties;
	

// Private functions
	virtual void ClearExtraData() = 0;

	virtual void SetTextDetails(const CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
		const CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray) = 0;

	virtual void CreateStringToDisplay(const char *pMainText, char *pOutString, s32 maxLengthOfOutString) const = 0;
};


class CDisplayTextZeroOrOneNumbers : public CDisplayTextBaseClass
{
public:
#if !__FINAL
	virtual void DebugPrintContents();
#endif	//	!__FINAL

private:
	CNumberWithinMessage m_NumberWithinMessage;

// Private functions
	virtual void ClearExtraData();

	virtual void SetTextDetails(const CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
		const CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray);

	virtual void CreateStringToDisplay(const char *pMainText, char *pOutString, s32 maxLengthOfOutString) const;
};


class CDisplayTextOneSubstring : public CDisplayTextBaseClass
{
public:
#if !__FINAL
	virtual void DebugPrintContents();
#endif	//	!__FINAL

private:
	CSubStringWithinMessage	m_SubstringWithinMessage;

// Private functions
	virtual void ClearExtraData();

	virtual void SetTextDetails(const CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
		const CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray);

	virtual void CreateStringToDisplay(const char *pMainText, char *pOutString, s32 maxLengthOfOutString) const;
};

class CDisplayTextFourSubstringsThreeNumbers : public CDisplayTextBaseClass
{
public:
#if !__FINAL
	virtual void DebugPrintContents();
#endif	//	!__FINAL

private:
	static const u32 MAX_SUBSTRINGS_IN_DISPLAY_TEXT = 4;
	static const u32 MAX_NUMBERS_IN_DISPLAY_TEXT = 3;

	CNumberWithinMessage m_NumbersWithinMessage[MAX_NUMBERS_IN_DISPLAY_TEXT];
	CSubStringWithinMessage	m_SubstringsWithinMessage[MAX_SUBSTRINGS_IN_DISPLAY_TEXT];

// Private functions
	virtual void ClearExtraData();

	virtual void SetTextDetails(const CNumberWithinMessage *pArrayOfNumbers, u32 SizeOfNumberArray, 
		const CSubStringWithinMessage *pArrayOfSubStrings, u32 SizeOfSubStringArray);

	virtual void CreateStringToDisplay(const char *pMainText, char *pOutString, s32 maxLengthOfOutString) const;
};



#define SCRIPT_TEXT_NUM_RELEASE_SINGLE_FRAME_LITERAL_STRINGS	(160)	//	03/04/15 - Increased from 120 to 160 for leaderboard bug 2201825

#if __FINAL
	#define SCRIPT_TEXT_NUM_SINGLE_FRAME_LITERAL_STRINGS		(SCRIPT_TEXT_NUM_RELEASE_SINGLE_FRAME_LITERAL_STRINGS)
#else
	#define SCRIPT_TEXT_NUM_DEBUG_SINGLE_FRAME_LITERAL_STRINGS		(256)
	#define SCRIPT_TEXT_NUM_SINGLE_FRAME_LITERAL_STRINGS		(SCRIPT_TEXT_NUM_RELEASE_SINGLE_FRAME_LITERAL_STRINGS + SCRIPT_TEXT_NUM_DEBUG_SINGLE_FRAME_LITERAL_STRINGS)
#endif	//	__FINAL


#define SCRIPT_TEXT_NUM_PERSISTENT_LITERAL_STRINGS			(64)
#define SCRIPT_MAX_CHARS_IN_LITERAL_STRING	(100)


class CDoubleBufferedSingleFrameLiterals
{
public:
	void Initialise(bool bResetArray);

	bool FindFreeIndex(bool NOTFINAL_ONLY(bUseDebugPortionOfSingleFrameArray), s32 &ReturnIndex);

	void SetLiteralString(s32 LiteralStringIndex, const char *pNewString) { safecpy(m_LiteralString[LiteralStringIndex], pNewString, SCRIPT_MAX_CHARS_IN_LITERAL_STRING ); }
	const char *GetLiteralString(s32 LiteralStringIndex) const { return m_LiteralString[LiteralStringIndex]; }

#if __ASSERT
	void DisplayAllLiteralStrings();
#endif

private:
	char m_LiteralString[SCRIPT_TEXT_NUM_SINGLE_FRAME_LITERAL_STRINGS][SCRIPT_MAX_CHARS_IN_LITERAL_STRING];

	u32 m_NumberOfReleaseStrings;
#if !__FINAL
	u32 m_NumberOfDebugStrings;
#endif
};


class CLiteralStringsForImmediateUse
{
public:
	void AllocateMemory(u32 numberOfLiterals, u32 maxStringLength);
	void FreeMemory();

	void ClearStrings(bool bClearBothBuffers);

	bool FindFreeIndex(s32 &ReturnIndex);

	void SetLiteralString(s32 LiteralStringIndex, const char *pNewString);
	const char *GetLiteralString(s32 LiteralStringIndex) const;

#if __ASSERT
	void DisplayAllLiteralStrings() const;
#endif

private:
	u32 GetIndexForCurrentThread() const;

private:
	struct sLiteralStringsForOneThread
	{
		atArray<char*> m_pLiteralStrings;

		u32 m_NumberOfUsedLiteralStrings;
	};

	sLiteralStringsForOneThread m_LiteralStringsForThreads[2];

	u32 m_MaxStringsInThread;
	u32 m_MaxCharsInString;
};


class CPersistentLiteral
{
public:
	void Initialise() { m_LiteralString[0] = 0; m_bIsUsed = false; m_bOccursInPreviousBriefs= false; m_bOccursInSubtitles = false; }

	void Set(const char *pNewString) { safecpy(m_LiteralString, pNewString, SCRIPT_MAX_CHARS_IN_LITERAL_STRING ); m_bIsUsed = true; }

	const char *Get() const { return m_LiteralString; }

	bool GetIsUsed() const { return m_bIsUsed; }

	void Clear(bool bClearingFromPreviousBriefs);

	void SetOccursInPreviousBriefs(bool bValue) { m_bOccursInPreviousBriefs = bValue; }
	void SetOccursInSubtitles(bool bValue) { m_bOccursInSubtitles = bValue; }

private:
	char m_LiteralString[SCRIPT_MAX_CHARS_IN_LITERAL_STRING];
	bool m_bIsUsed;
	bool m_bOccursInPreviousBriefs;
	bool m_bOccursInSubtitles;
};


//	Graeme - I've re-introduced the single frame and persistent literals that were combined in Changelist 1290505
class CLiteralStrings
{
private:
	CDblBuf<CDoubleBufferedSingleFrameLiterals> m_DBSingleFrameLiteralStrings;

	CPersistentLiteral m_PersistentLiteralStrings[SCRIPT_TEXT_NUM_PERSISTENT_LITERAL_STRINGS];

	CLiteralStringsForImmediateUse m_LiteralStringsForImmediateUse;
	CLiteralStringsForImmediateUse m_LongLiteralStringsForImmediateUse;

#if __ASSERT
	void DisplayAllLiteralStrings(CSubStringWithinMessage::eLiteralStringType literalStringType);
#endif

public:
	void Initialise(unsigned initMode);
	void Shutdown(unsigned shutdownMode);

	void ClearSingleFrameStrings(bool bResetSingleFrameArray);
	void ClearPersistentString(int StringIndex, bool bClearingFromPreviousBriefs);

	void ClearArrayOfImmediateUseLiteralStrings(bool bClearBothBuffers);

	//	_Type could be char or const char
	template<typename _Type> int SetLiteralString(_Type* pNewString, CSubStringWithinMessage::eLiteralStringType literalStringType, bool bUseDebugPortionOfSingleFrameArray);

//	int SetLiteralStringWithGxtString(char* pNewGxtString, bool bPersistentString);

	void SetPersistentLiteralOccursInPreviousBriefs(int StringIndex, bool bValue);
	void SetPersistentLiteralOccursInSubtitles(int StringIndex, bool bValue);

	const char *GetLiteralString(int StringIndex, CSubStringWithinMessage::eLiteralStringType literalStringType) const;
	const char *GetSingleFrameLiteralStringFromWriteBuffer(s32 StringIndex);
};


#define NUM_SCRIPT_RELEASE_RECTANGLES			(500)

#if __FINAL
	#define TOTAL_NUM_SCRIPT_RECTANGLES			(NUM_SCRIPT_RELEASE_RECTANGLES)
#else
	#define NUM_SCRIPT_DEBUG_RECTANGLES			(300)
	#define TOTAL_NUM_SCRIPT_RECTANGLES			(NUM_SCRIPT_RELEASE_RECTANGLES + NUM_SCRIPT_DEBUG_RECTANGLES)
#endif

#define NUM_SCRIPT_SCALEFORM_MOVIES			(20)
#define SCRIPT_HUD_MOVIE_ID					(0)

#define MAX_ACTIONSCRIPT_FLAGS				(20)

// All the different windows we can draw using the script
enum EScriptGfxType
{
	SCRIPT_GFX_NONE,
	SCRIPT_GFX_SOLID_COLOUR,
	SCRIPT_GFX_SOLID_COLOUR_STEREO,
	SCRIPT_GFX_SPRITE_NOALPHA,
	SCRIPT_GFX_SPRITE,
	SCRIPT_GFX_SPRITE_NON_INTERFACE,
	SCRIPT_GFX_SPRITE_AUTO_INTERFACE,
	SCRIPT_GFX_SPRITE_STEREO,
	SCRIPT_GFX_SPRITE_WITH_UV,
	SCRIPT_GFX_MASK,
	SCRIPT_GFX_LINE,
	SCRIPT_GFX_SCALEFORM_MOVIE,
	SCRIPT_GFX_SCALEFORM_MOVIE_RETICULESTEREO,
	SCRIPT_GFX_SCALEFORM_MOVIE_STEREO,
	SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW,
	SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW_WITH_UV
};

class intro_script_rectangle
{
public:
	void Reset(bool bFullReset, int OrthoRenderID);
	void SetCommonValues(float fMinX, float fMinY, float fMaxX, float fMaxY);
	void Draw(int CurrentRenderID, u32 iDrawOrderValue) const;
	void PushRefCount() const;
	
	intro_script_rectangle()	{ Reset(true,0); }

	void SetTexture(grcTexture *pTexture) { m_pTexture = pTexture; }
	void SetScaleformMovieId(s32 MovieId) { m_ScaleformMovieId = MovieId; }
	void SetRectRotationOrWidth(float fRotationOrWidth) { m_ScriptRectRotationOrWidth = fRotationOrWidth; }
	void SetColour(s32 R, s32 G, s32 B, s32 A) { m_ScriptRectColour = Color32(R, G, B, A); }
	void SetGfxType(EScriptGfxType GfxType) { m_eGfxType = GfxType; m_bInterface = (GfxType != SCRIPT_GFX_SPRITE_NON_INTERFACE); }
	void SetUseMask(bool bUseMask) { m_bUseMask = bUseMask; }
	void SetInvertMask(bool bInvertMask) { m_bInvertMask = bInvertMask; }
	void SetUseMovie(u32 idx) { m_bUseMovie = true; m_BinkMovieId = idx; }
	void SetTextureCoords(float ux, float uy, float vx, float vy) { m_ScriptRectTexUCoordX= ux; m_ScriptRectTexUCoordY = uy; m_ScriptRectTexVCoordX = vx; m_ScriptRectTexVCoordY = vy; }
	void SetARAwareX(bool aware) { m_bARAwareX=aware; }

	int GetRenderID() const { return m_RenderID; }

#if __BANK
	void OutputValuesOfMembersForDebug() const;
#endif	//	__BANK

private:
	grcTextureHandle	m_pTexture;				// NULL = solid colour, no texture
	s32					m_ScaleformMovieId;		// -1 = no scaleform movie
	Vector2				m_ScriptRectMin;			// Start
	Vector2				m_ScriptRectMax;			// End
	float				m_ScriptRectRotationOrWidth;		// Rotation or width (if any)
	int					m_RenderID;
	eWIDESCREEN_FORMAT	m_ScriptWidescreenFormat;
	Color32				m_ScriptRectColour;		// The Colour
	s32					m_IndexOfDrawOrigin;
	u32					m_ScriptGfxDrawProperties;	// ??
	u32					m_eGfxType		:4;			// The Type of window to draw
	u32					m_bUseMask		:1;
	u32					m_bInvertMask	:1;
	u32					m_bUseMovie		:1;			// use the current movie as source texture
	u32					m_bInterface	:1;			// belongs to GUI, should be displayed on the central screen
	u32					m_bARAwareX		:1;			// "X-coords Screen Aspect Ratio" aware flag
	u32					/*m_UnusedBits0*/:23; 
	u32					m_BinkMovieId;
	float				m_ScriptRectTexUCoordX;			// u
	float				m_ScriptRectTexUCoordY;			// u
	float				m_ScriptRectTexVCoordX;			// v
	float				m_ScriptRectTexVCoordY;			// v
};


#define NUM_DRAW_ORIGINS	(32)

class CDrawOrigins
{
public:
	void Set(const Vector3 &vOrigin, bool b2dOrigin);
	bool GetScreenCoords(Vector2 &vReturnScreenCoords) const;

private:
	Vector3 m_vOrigin;
	bool m_bIs2dOrigin;
};

class CDoubleBufferedDrawOrigins
{
public:
	void Initialise();

	s32 GetNextFreeIndex();

	void Set(s32 NewOriginIndex, Vector3 &vOrigin, bool bIs2d);
	bool GetScreenCoords(s32 OriginIndex, Vector2 &vReturnScreenCoords) const;

private:
	CDrawOrigins Origins[NUM_DRAW_ORIGINS];
	u32 NumberOfDrawOriginsThisFrame;
};


class CDoubleBufferedIntroRects
{
public:
	void Initialise(bool bResetArray);

	intro_script_rectangle *GetNextFree();

	void Draw(int CurrentRenderID, u32 iDrawOrderValue) const;
	void PushRefCount() const;
private:

#if __BANK
	void OutputAllValuesForDebug();
#endif	//	__BANK

	intro_script_rectangle IntroRectangles[TOTAL_NUM_SCRIPT_RECTANGLES];
	u32 NumberOfIntroReleaseRectanglesThisFrame;
#if !__FINAL
	u32 NumberOfIntroDebugRectanglesThisFrame;
#endif
};

class CDoubleBufferedIntroTexts
{
public:
	CDoubleBufferedIntroTexts();

	void Initialise(bool bResetArray);

	void AddTextLine(float fPosX, float fPosY, s32 renderID, s32 indexOfDrawOrigin, 
		u32 drawProperties, CDisplayTextFormatting &formattingForText, 
		const char *pMainTextLabel, 
		CNumberWithinMessage *pNumbersArray, u32 numberOfNumbers, 
		CSubStringWithinMessage *pSubStringsArray, u32 numberOfSubStrings, 
		bool bUseDebugDisplayTextLines);

	void Draw(int CurrentRenderID, u32 iDrawOrderValue) const;

	void AllocateMemoryForAllTextLines();
	void FreeMemoryForAllTextLines();

private:
//	CDisplayTextNoSubstringsNoNumbers *m_pTextLinesNoSubstringsNoNumbers;
//	u32 m_numberOfTextLinesNoSubstringsNoNumbersThisFrame;

	CDisplayTextZeroOrOneNumbers *m_pTextLinesZeroOrOneNumbers;
	u32 m_numberOfTextLinesZeroOrOneNumbersThisFrame;

	CDisplayTextOneSubstring *m_pTextLinesOneSubstring;
	u32 m_numberOfTextLinesOneSubstringThisFrame;

	CDisplayTextFourSubstringsThreeNumbers *m_pTextLinesFourSubstringsThreeNumbers;
	u32 m_numberOfTextLinesFourSubstringsThreeNumbersThisFrame;

#if ENABLE_DEBUG_HEAP
	CDisplayTextFourSubstringsThreeNumbers* m_introDebugTextLines;
	u32 m_numberOfIntroDebugTextLinesThisFrame;
#endif
};


struct sFakeInteriorStruct
{
	bool	bActive;
	u32		iHash;
	Vector2	vPosition;
	s32		iRotation;
	s32		iLevel;

	void Reset();
};


class CBlipFades
{
	struct sBlipFade
	{
		s32 m_iUniqueBlipIndex;
		s32 m_iDestinationAlpha;
		s32 m_iTimeRemaining;

		void Clear();
	};

public:
	void AddBlipFade(s32 uniqueBlipIndex, s32 destinationAlpha, s32 fadeDuration);
	void RemoveBlipFade(s32 uniqueBlipIndex);
	s32 GetBlipFadeDirection(s32 uniqueBlipIndex);

	void ClearAll();
	void Update();

private:
	static const u32 MAX_BLIP_FADES = 32;

	sBlipFade m_ArrayOfBlipFades[MAX_BLIP_FADES];
};


class CMultiplayerFogOfWarSavegameDetails
{
public :
	CMultiplayerFogOfWarSavegameDetails() { Reset(); }

	void Reset();
	void Set(bool bEnabled, u8 minX, u8 minY, u8 maxX, u8 maxY, u8 fillValueForRestOfMap);
	bool Get(u8 &minX, u8 &minY, u8 &maxX, u8 &maxY, u8 &fillValueForRestOfMap);

private:
	bool m_bEnabled;
	u8 m_MinX;
	u8 m_MinY;
	u8 m_MaxX;
	u8 m_MaxY;
	u8 m_FillValueForRestOfMap;
};

#define INVALID_FAKE_PLAYER_POS -9999.0f

class CScriptHud
{
public:

//	Public functions
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void PreSceneProcess(void);
	static void ClearMapVisibilityPerFrameFlags(void);
	static void Process(void);

	static void OnNetworkClosed(void);
	
	static void DrawScriptText(int CurrentViewportID, u32 iDrawOrderValue);
	static void DrawScriptSpritesAndRectangles(int CurrentViewportID, u32 iDrawOrderValue);

#if __BANK
	static void DrawScriptScaleformDebugInfo();
#endif  // __BANK

	static s32 SetupScaleformMovie(s32 iNewId, const char *pFilename);
	static void RemoveScaleformMovie(s32 iId);
	static s32 GetScaleformMovieID(s32 iId);
	static void ResetScaleformComponentsOnRTPerFrame();
	static void UpdateScaleformComponents();
	static void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);
	static void ClearMessages(void);

	static void SetUseAdjustedMouseCoords(bool bUseAdjustMouseCoords) { ms_bUseAdjustedMouseCoords = bUseAdjustMouseCoords; }
	static bool GetUseAdjustedMouseCoords() { return ms_bUseAdjustedMouseCoords; }

	static void DrawScriptGraphics(int CurrentViewportID, u32 iDrawOrderValue);

	static bool ShouldNonMiniGameHelpMessagesBeDisplayed();

	static bool GetScreenPosFromWorldCoord(const Vector3 &vecWorldPos, float &NormalisedX, float &NormalisedY);

	static  CDblBuf<CDoubleBufferedIntroRects>& GetIntroRects(void) { return(ms_DBIntroRects); }
	static  CDblBuf<CDoubleBufferedIntroTexts>& GetIntroTexts(void) { return(ms_DBIntroTexts); }
	static	CDblBuf<CDoubleBufferedDrawOrigins>& GetDrawOrigins(void) { return ms_DBDrawOrigins; }

	static void SetAddNextMessageToPreviousBriefs(bool bAddToBriefs) { ms_bAddNextMessageToPreviousBriefs = bAddToBriefs; }
	static bool GetAddNextMessageToPreviousBriefs() { return ms_bAddNextMessageToPreviousBriefs; }

	static void SetNextMessagePreviousBriefOverride(ePreviousBriefOverride Override) { ms_PreviousBriefOverride =  Override; }
	static ePreviousBriefOverride GetNextMessagePreviousBriefOverride() { return ms_PreviousBriefOverride; }

	static CBlipFades &GetBlipFades() { return ms_BlipFades; }
	static CMultiplayerFogOfWarSavegameDetails &GetMultiplayerFogOfWarSavegameDetails() { return ms_MultiplayerFogOfWarSavegameDetails; }

    static bool GetFakedPauseMapPlayerPos(Vector2 &vFakePlayerPos);
    static bool GetFakedGPSPlayerPos(Vector3 &vFakePlayerPos);

#if ENABLE_LEGACY_SCRIPTED_RT_SUPPORT
	static grcRenderTarget* GetRenderTarget() { return m_offscreenRenderTarget;}
#endif // ENABLE_LEGACY_SCRIPTED_RT_SUPPORT

	static void BeforeThreadUpdate();
	static void AfterThreadUpdate();

	static bool FreezeRendering();
	static int GetReadbufferIndex();

	static bool GetStopLoadingScreen()	{ return ms_StopLoadingScreen; }
	static void SetStopLoadingScreen(bool value)	{ ms_StopLoadingScreen = value; }

	static void RenderScriptedScaleformMovie(s32 iMovieId, Vector2 vPos, Vector2 vSize, int CurrentRenderID = -1, u8 iLargeRT = 0);
	static void RenderScriptedScaleformMovieWithMasking(s32 iMovieId, Vector2 vPos, Vector2 vSize);

private: 
//	Private members
#if ENABLE_LEGACY_SCRIPTED_RT_SUPPORT	
	static grcRenderTarget* m_offscreenRenderTarget;
#endif // ENABLE_LEGACY_SCRIPTED_RT_SUPPORT

	static	CDblBuf<CDoubleBufferedIntroRects>	ms_DBIntroRects;
	static	CDblBuf<CDoubleBufferedIntroTexts>	ms_DBIntroTexts;
	static	CDblBuf<CDoubleBufferedDrawOrigins> ms_DBDrawOrigins;

	static bool ms_bAddNextMessageToPreviousBriefs;
	static ePreviousBriefOverride ms_PreviousBriefOverride;

	static CBlipFades ms_BlipFades;

	static CMultiplayerFogOfWarSavegameDetails ms_MultiplayerFogOfWarSavegameDetails;

	// private functions
	static void CallShutdownMovie(const s32 c_movieIndex);
	static void DeleteScriptScaleformMovie(s32 iIndex, bool bForceDelete = false);
	static void ResetScriptScaleformMovieVariables(s32 iIndex);

public:
#if __ASSERT
	static bool bLockScriptrendering;
#endif // __ASSERT

#if __BANK
	static bool bDisplayScriptScaleformMovieDebug;
#endif

#if !__FINAL
	enum eTypeOfScriptText
	{
		SCRIPT_TEXT_DEBUG,
		SCRIPT_TEXT_ZERO_OR_ONE_NUMBERS,
		SCRIPT_TEXT_ONE_SUBSTRING,
		SCRIPT_TEXT_FOUR_SUBSTRINGS_THREE_NUMBERS,
		SCRIPT_TEXT_MAX_TYPES
	};
	static u32 ms_NextTimeAllowedToPrintContentsOfFullTextArray[SCRIPT_TEXT_MAX_TYPES];
#endif	//	!__FINAL

	static s32 iCurrentWebpageIdFromActionScript;
	static s32 iCurrentWebsiteIdFromActionScript;

	static s32 iActionScriptGlobalFlags[MAX_ACTIONSCRIPT_FLAGS];

	static s32 iFindNextRadarBlipId;

	static s32 scriptTextRenderID;
	static bool bUsingMissionCreator;
	static bool bForceShowGPS;
	static bool bSetDestinationInMapMenu;
	static bool bWantsToBlockWantedFlash;
	static s32 ms_iOverrideTimeForAreaVehicleNames;
	static bool bAllowMissionCreatorWarp;
	static bool ms_bDisplayPlayerNameBlipTags;
	static s32 iScriptReticleMode;
	static bool bUseVehicleTargetingReticule;
	static bool bHideLoadingAnimThisFrame;
	static sFakeInteriorStruct FakeInterior;
	static bool ms_bFakeExteriorThisFrame;
	static bool ms_bHideMiniMapExteriorMapThisFrame;
	static bool ms_bHideMiniMapInteriorMapThisFrame;
	static bool bRenderFrontendBackgroundThisFrame;
	static bool bRenderHudOverFadeThisFrame;
    static Vector2 vFakePauseMapPlayerPos;
    static Vector3 vFakeGPSPlayerPos;
	static Vector2 vInteriorFakePauseMapPlayerPos;
	static u32 ms_iCurrentFakedInteriorHash;
	static bool ms_bUseVerySmallInteriorZoom;
	static bool ms_bUseVeryLargeInteriorZoom;
	static bool bDontDisplayHudOrRadarThisFrame;
	static bool bDontZoomMiniMapWhenRunningThisFrame;
	static bool bDontZoomMiniMapWhenSnipingThisFrame;
	static bool bDontTiltMiniMapThisFrame;
	static float fFakeMinimapAltimeterHeight;
	static bool ms_bColourMinimapAltimeter;
	static eHUD_COLOURS ms_MinimapAltimeterColor;
	static s32 iFakeWantedLevel;
	static s32 iHudMaxHealthDisplay;
	static s32 iHudMaxArmourDisplay;
	static s32 iHudMaxEnduranceDisplay;
	static Vector3 vFakeWantedLevelPos;
	static bool bFlashWantedStarDisplay;
	static bool bForceOffWantedStarFlash;
	static bool bForceOnWantedStarFlash;
	static bool bUpdateWantedThreatVisibility;
	static bool bIsWantedThreatVisible;	
	static bool bUpdateWantedDrainLevel;
	static int iWantedDrainLevelPercentage;
	static bool bUsePlayerColourInsteadOfTeamColour;
	static s32 m_playerBlipColourOverride;
	static char cMultiplayerBriefTitle[TEXT_KEY_SIZE];
	static char cMultiplayerBriefContent[TEXT_KEY_SIZE];
	static eWIDESCREEN_FORMAT CurrentScriptWidescreenFormat;
	static bool bScriptHasChangedWidescreenFormat;
	static s32 ms_IndexOfDrawOrigin;
	static CHudAlignment ms_CurrentScriptGfxAlignment;
	static u32 ms_iCurrentScriptGfxDrawProperties;
	static bool ms_bAdjustNextPosSize;
	static bool ms_bUseMaskForNextSprite;
	static bool ms_bInvertMaskForNextSprite;
	static bool ms_bUseAdjustedMouseCoords;

	static CDisplayTextFormatting ms_FormattingForNextDisplayText;

	//	Buffer of literal strings - need a distinction between those that can be cleared every frame (DISPLAY_TEXT)
	//	and those that should only be cleared when the string can no longer be displayed (PRINT commands)
	static CLiteralStrings ScriptLiteralStrings;	//	Persistent strings will need saved if the previous briefs are restored on loading a saved game
	//	since a previous brief could contain a literal string

	static u32 ScriptMessagesLastClearedInFrame;

	//	static bool bUseMessageFormatting;
	//	static u16 MessageCentre;
	//	static u16 MessageWidth;

	static sScaleformComponent ScriptScaleformMovie[NUM_SCRIPT_SCALEFORM_MOVIES];
	static s32 iCurrentRequestedScaleformMovieId;

	static sRadarMessage RadarMessage;
	static sMissionPassedCashMessage MissionPassedCashMessage;

	static bool ms_bAllowDisplayOfMultiplayerCashText;
	static bool  ms_bTurnOnMultiplayerWalletCashThisFrame;
	static bool  ms_bTurnOnMultiplayerBankCashThisFrame;
	static bool  ms_bTurnOffMultiplayerWalletCashThisFrame;
	static bool  ms_bTurnOffMultiplayerBankCashThisFrame;

	static bool bDisplayCashStar;

	static CAbovePlayersHeadDisplay ms_AbovePlayersHeadDisplay;
	static float ms_fMiniMapForcedZoomPercentage;
	static float ms_fRadarZoomDistanceThisFrame;
	static s32 ms_iRadarZoomValue;
	static bool bDisplayHud;
	static CDblBuf<bool> ms_bDisplayHudWhenNotInStateOfPlayThisFrame;
	static CDblBuf<bool> ms_bDisplayHudWhenPausedThisFrame;
	static bool bDisplayRadar;
	static bool ms_bFakeSpectatorMode;

	static Vector2 vMask[2];
	static bool	bMaskCreated;
	static bool bUsedMaskLastTime;

	static bool bHideFrontendMapBlips;

	static int NumberOfMiniGamesAllowingNonMiniGameHelpMessages;

	static bool bDisablePauseMenuThisFrame;
	static bool bAllowPauseWhenInNotInStateOfPlayThisFrame;
	static bool bSuppressPauseMenuRenderThisFrame;

//	static u32 ms_MaxNumberOfTextLinesNoSubstringsNoNumbers;
	static u32 ms_MaxNumberOfTextLinesZeroOrOneNumbers;
	static u32 ms_MaxNumberOfTextLinesOneSubstring;
	static u32 ms_MaxNumberOfTextLinesFourSubstringsThreeNumbers;

	//	Blend States used when drawing script sprites and rectangles
	static grcBlendStateHandle ms_StandardSpriteBlendStateHandle;
	static grcBlendStateHandle ms_EntireScreenMaskBlendStateHandle;
	static grcBlendStateHandle ms_MaskBlendStateHandle;
	static grcBlendStateHandle ms_MaskedSpritesBlendStateHandle;
	static grcBlendStateHandle ms_SpriteWithNoAlphaBlendStateHandle;
	
	static grcDepthStencilStateHandle ms_MarkStencil_DSS;
	static grcDepthStencilStateHandle ms_UseStencil_DSS;

	static grcBlendStateHandle ms_ClearAlpha_BS;
	static grcBlendStateHandle ms_SetAlpha_BS;
	
	static bool ms_StopLoadingScreen;
#if RSG_PC
	static bool ms_StereorizeHUD;
#endif
};


template<typename _Type> int CLiteralStrings::SetLiteralString(_Type* pNewString, CSubStringWithinMessage::eLiteralStringType literalStringType, bool bUseDebugPortionOfSingleFrameArray)
{
	int loop = 0;
	bool bFound = false;
	Assertf(pNewString, "CLiteralStrings::SetLiteralString - string pointer is NULL");

	switch (literalStringType)
	{
		case CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT :
			{	//	find a free slot in array of persistent strings
				while ((loop < SCRIPT_TEXT_NUM_PERSISTENT_LITERAL_STRINGS) && !bFound)
				{
					if (m_PersistentLiteralStrings[loop].GetIsUsed() == false)
					{
						m_PersistentLiteralStrings[loop].Set(pNewString);
						m_PersistentLiteralStrings[loop].SetOccursInSubtitles(true);
						m_PersistentLiteralStrings[loop].SetOccursInPreviousBriefs(false);

						bFound = true;
					}
					else
					{
						loop++;
					}
				}
			}
			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_SINGLE_FRAME :
			{	//	find a free slot in array of single frame strings
				bFound = m_DBSingleFrameLiteralStrings.GetWriteBuf().FindFreeIndex(bUseDebugPortionOfSingleFrameArray, loop);
				if (bFound)
				{
					m_DBSingleFrameLiteralStrings.GetWriteBuf().SetLiteralString(loop, pNewString);
				}
			}
			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE :
			bFound = m_LiteralStringsForImmediateUse.FindFreeIndex(loop);
			if (bFound)
			{
				m_LiteralStringsForImmediateUse.SetLiteralString(loop, pNewString);
			}
			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG :
			bFound = m_LongLiteralStringsForImmediateUse.FindFreeIndex(loop);
			if (bFound)
			{
				m_LongLiteralStringsForImmediateUse.SetLiteralString(loop, pNewString);
			}
			break;
	}

	if (bFound)
	{
		return loop;
	}

#if __ASSERT
	DisplayAllLiteralStrings(literalStringType);
	switch (literalStringType)
	{
		case CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT :
			Assertf(0, "CLiteralStrings::SetLiteralString - no space left to add another persistent literal string");
			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_SINGLE_FRAME :
			Assertf(0, "CLiteralStrings::SetLiteralString - no space left to add another single frame literal string");
			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE :
			Assertf(0, "CLiteralStrings::SetLiteralString - no space left to add another short immediate use literal string");
			break;

		case CSubStringWithinMessage::LITERAL_STRING_TYPE_FOR_IMMEDIATE_USE_LONG :
			Assertf(0, "CLiteralStrings::SetLiteralString - no space left to add another long immediate use literal string");
			break;
	}
#endif
	return -1;
}


#endif	//	_SCRIPT_HUD_H_

