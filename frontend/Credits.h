/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Credits.h
// PURPOSE : manages the processing of end-game-credits
// AUTHOR  : Derek Payne
// STARTED : 14/06/2006
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _CREDITS_H_
#define _CREDITS_H_

// Rage headers
#include "atl/string.h"
#include "parser/macros.h"

// Game headers
#include "renderer/Sprite2d.h"
#include "text\text.h"
#include "Network\Cloud\CloudManager.h"

enum eCreditType
{
	JOB_BIG = 0,
	JOB_MED,
	JOB_SMALL,

	NAME_BIG,
	NAME_MED,
	NAME_SMALL,

	SPACE_BIG,
	SPACE_MED,
	SPACE_SMALL,
	SPACE_END,

	SPRITE_1,

	LEGALS,
	AUDIO_NAME,
	AUDIO_LEGALS,
	JOB_AND_NAME_MED
};

class CCreditItem
{
public:
	eCreditType LineType;
	atString cTextId1;
	atString cTextId2;
	atString cTextId3;
	bool bLiteral;

	PAR_SIMPLE_PARSABLE;
};

class CCreditArray
{
public:
	atArray<CCreditItem> CreditItems;

	PAR_SIMPLE_PARSABLE;
};

class CCreditsDownloadHelper : public CloudListener
{
public: // Methods
	CCreditsDownloadHelper(CCreditArray& creditsArray);
	~CCreditsDownloadHelper();

	CloudRequestID StartDownload(const char* contentName, unsigned nCloudRequestFlags = 0);
	void Clear();

	const bool IsDownloading() const { return m_fileRequestId != INVALID_CLOUD_REQUEST_ID && !m_downloadSuccess; }
	const bool IsDownloadSuccessful() const { return m_downloadSuccess; }
	const bool IsFileParseSuccessful() const { return m_fileParseSuccess; }
	const bool HasError() const { return m_hasError; }
	const bool UseLocalFile() const { return m_useLocal; }
	const CloudRequestID GetFileRequestedId() const { return m_fileRequestId; }

private: // Methods
	// CloudListener callback
	void OnCloudEvent(const CloudEvent* pEvent);

	bool ParseFile(const char* pFilename);
	eCreditType GetLineType(u32 creditHash);
	void SetupParserConversionDictionary();

private: // Members
	bool m_downloadSuccess;
	bool m_fileParseSuccess;
	bool m_hasError;
	bool m_useLocal;
	int m_resultCode;

	atString m_filename;
	CloudRequestID m_fileRequestId;

	CCreditArray& m_creditsArray;
	atMap<u32, eCreditType> m_parserConversionMap;
};

class CCredits
{
public:
	static	void	Init(unsigned initMode);
	static	void	Shutdown(unsigned shutdownMode);

	static	void	Update();
	static	void	UpdateInput();
	static	void	Render(bool bBeforeFade);

	static	bool	IsRunning() { return (bCreditsRunning); }
	static	bool	HaveReachedEnd() { return sm_bReachedEnd; }

	static	bool	bScriptWantsCreditsBeforeFade;
	static	void	SetToRenderBeforeFade(bool bBeforeFade) { bScriptWantsCreditsBeforeFade = bBeforeFade; }

	static void SetLaunchedFromPauseMenu(bool launchedFromPauseMenu) { sm_launchedFromPauseMenu = launchedFromPauseMenu; }
	static void AdjustCreditScrollSpeed(int fasterSlower);

	static void UpdateInstructionalButtons();

#if __BANK
	static	void	InitWidgets();
	static  s32		sm_iDebugPercentage;
#endif // __BANK

private:
	static	void	Load();
	static	void	LoadLocalCreditsFile();

	static	bool	PrintCreditText(float PosX, float ScaleX, float ScaleY, const char* pString, float& PixelsDown, float PixelsScrolled, bool bMoveOn);
	static	bool	PrintCreditSpace(float ScaleY, float& PixelsDown, float PixelsScrolled);
	static	bool	DrawSprite(float ScaleX, float ScaleY, float& PixelsDown, float PixelsScrolled);
	static Vector2  GetWrapLeftRight();

#if __BANK
	static	void	StartEndCreditsButton();
	static	void	JumpToPosition();
#endif // #if __BANK

	static CCreditArray m_CreditArray;

	static bool bShutdownPending;
	static	bool	bCreditsRunning;
	static	u32	CreditsStartTime;
	static float PixelsScrolled;
	static bool sm_bUsingLimitedFont;

	static float sm_currentSpeed;

	static u32 iStartLineRenderedThisFrame;
	static float fStartPixelsDown;

	static CTextLayout CreditTextLayout;

	static CSprite2d sm_Sprite;
	static strLocalIndex sm_iTxdId;
	static grcBlendStateHandle ms_StandardSpriteBlendStateHandle;
	static bool sm_bReachedEnd;
	static bool sm_launchedFromPauseMenu;

	static bool sm_usingCloudCredits;
	static CCreditsDownloadHelper* sm_creditsDownloadHelper;
};

#endif

// eof
