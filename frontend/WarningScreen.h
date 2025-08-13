/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WarningScreen.h
// PURPOSE : header for WarningScreen.cpp
// AUTHOR  : Derek Payne
// STARTED : 10/07/2012
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef INC_WARNINGSCREEN_H_
#define INC_WARNINGSCREEN_H_

// Rage headers
#include "system/bit.h"
#include "data/callback.h"

// Game headers
#include "text/messages.h"
#include "text/GxtString.h"
#include "Frontend/CMenuBase.h"
#include "frontend/WarningScreenEnums.h"
#include "Scaleform/ScaleFormMgr.h"

#define HAS_YT_DISCONNECT_MESSAGE (1 && (RSG_DURANGO || RSG_ORBIS))

// Warning message style flags
enum eWarningButtonFlags : u64
{
	// They are determined by frontend.xml!
	// ensure that you're in alignment if you add a reference here!
	FE_WARNING_NONE		= 0,
	FE_WARNING_SELECT	= BIT(0),
	FE_WARNING_OK		= BIT(1),
	FE_WARNING_YES		= BIT(2),
	FE_WARNING_BACK		= BIT(3),
	FE_WARNING_CANCEL	= BIT(4),
	FE_WARNING_NO		= BIT(5),
	FE_WARNING_RETRY	= BIT(6),
	FE_WARNING_RESTART	= BIT(7),
	FE_WARNING_SKIP		= BIT(8),
	FE_WARNING_QUIT		= BIT(9),
	FE_WARNING_ADJUST_LEFTRIGHT = BIT(10),
	FE_WARNING_IGNORE	= BIT(11),
	FE_WARNING_SHARE	= BIT(12),
	FE_WARNING_SIGNIN	= BIT(13),
	FE_WARNING_CONTINUE = BIT(14),
	FE_WARNING_ADJUST_STICKLEFTRIGHT = BIT(15),
	FE_WARNING_ADJUST_UPDOWN = BIT(16),
	FE_WARNING_OVERWRITE = BIT(17),
	FE_WARNING_SOCIAL_CLUB = BIT(18),
	FE_WARNING_CONFIRM = BIT(19),

	FE_WARNING_CONTNOSAVE = BIT(20),
	FE_WARNING_RETRY_ON_A = BIT(21),
	FE_WARNING_BACK_WITH_ACCEPT_SOUND = BIT(22),
    // Jump from 22 to 27 is NOT a mistake, they exist in the frontend.xml but not exposed to the enum
	FE_WARNING_SPINNER = BIT(27),
	FE_WARNING_RETURNSP		= BIT(28),
	FE_WARNING_CANCEL_ON_B	= BIT(29),

	// flag meaning don't process any sound
	FE_WARNING_NOSOUND	= BIT(30),

	FE_WARNING_EXIT		= BIT(31),
	FE_WARNING_NO_ON_X	= BIT_64(32),
	
	// Custom script stuff
	FE_WARNING_HOST		= BIT_64(33),
	FE_WARNING_ANY_JOB	= BIT_64(34),

	// Play Station promotion.
	FE_WARNING_PS_PROMOTION = BIT_64(35),
	FE_WARNING_FREEMODE = BIT_64(36),

	// Commonly combined flags for easy of use.
	FE_WARNING_YES_NO = FE_WARNING_YES|FE_WARNING_NO,
	FE_WARNING_OK_CANCEL = FE_WARNING_OK|FE_WARNING_CANCEL,
	FE_WARNING_YES_NO_CANCEL = FE_WARNING_YES|FE_WARNING_NO_ON_X|FE_WARNING_CANCEL_ON_B,
};

DEFINE_CLASS_ENUM_FLAG_FUNCTIONS(eWarningButtonFlags)


class CMenuButtonWithSound : public CMenuButton
{
public:
	CMenuButtonWithSound() : CMenuButton(), m_DebounceCheck(true) {};

	atHashWithStringBank	m_Sound;
	bool m_DebounceCheck;
};

class CWarningScreen
{
public:

	enum eWarningScreenUnpause
	{
		UNPAUSE_ALWAYS = 0,
		UNPAUSE_ON_ACCEPT,
		UNPAUSE_ON_CANCEL,
		UNPAUSE_NEVER,
		UNPAUSE_MAX
	};

	static void Init(unsigned initMode);
//	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void Render();
	static void HandleXML(parTreeNode* pButtonMenu);

#if RSG_PC
	static void DeviceLost();
	static void DeviceReset();
#endif //RSG_PC

	static void SetWarningMessageInUse();

	static eWarningButtonFlags CheckAllInput(bool bHandleSound = true, int iExtraInputOverride = 0);

	static void SetMessage(	const eWarningMessageStyle style,
							const atHashWithStringBank pTextLabel,
							const eWarningButtonFlags iFlags = FE_WARNING_NONE,
							const bool bInsertNumber = false,
							const s32 NumberToInsert = -1,
							const char *pFirstSubString = NULL,
							const char *pSecondSubString = NULL,
							const bool bBlackBackground = true,
							const bool bAllowSpinner = false,
							int errorNumber = 0);

	static void SetMessageWithHeader(	const eWarningMessageStyle style,
										const atHashWithStringBank pTextLabelHeading, 
										const atHashWithStringBank pTextLabelBody,
										const eWarningButtonFlags iFlags = FE_WARNING_NONE,
										const bool bInsertNumber = false,
										const s32 NumberToInsert = -1,
										const char *pFirstSubString = NULL,
										const char *pSecondSubString = NULL,
										const bool bBlackBackground = true,
										const bool bAllowSpinner = false,
										int errorNumber = 0);

	static void SetMessageAndSubtext(		const eWarningMessageStyle style,
											const atHashWithStringBank pTextLabel,
											const atHashWithStringBank pTextLabelSubtext = 0u,
											const eWarningButtonFlags iFlags = FE_WARNING_NONE,
											const bool bInsertNumber = false,
											const s32 NumberToInsert = -1,
											const char *pFirstSubString = NULL,
											const char *pSecondSubString = NULL,
											const bool bBlackBackground = true,
											const bool bAllowSpinner = false,
											int errorNumber = 0);

	static void SetMessageWithHeaderAndSubtext(		const eWarningMessageStyle style,
													const atHashWithStringBank pTextLabelHeading, 
													const atHashWithStringBank pTextLabelBody,
													const atHashWithStringBank pTextLabelSubtext = 0u,
													const eWarningButtonFlags iFlags = FE_WARNING_NONE,
													const bool bInsertNumber = false,
													const s32 NumberToInsert = -1,
													const char *pFirstSubString = NULL,
													const char *pSecondSubString = NULL,
													const bool bBlackBackground = true,
													const bool bAllowSpinner = false,
													int errorNumber = 0);

	static bool SetAndWaitOnMessageScreen(const eWarningMessageStyle style,
											const atHashWithStringBank pText,
											const eWarningButtonFlags iFlags,
											bool bInsertNumberIntoString = false,
											s32 iNumberToInsert = -1,
											eWarningScreenUnpause eUnpause = UNPAUSE_ALWAYS);

	static bool SetMessageOptionItems(  const int iHighlightIndex,
										const char *pNameString,
										const int iCash,
										const int iRp,
										const int iLvl,
										const int iCol);
	
	static void RemoveMessageOptionItems();

	static bool SetMessageOptionHighlight( const int iHighlightIndex );


	static void SetControllerReconnectMessageWithHeader(	const eWarningMessageStyle style,
															const atHashWithStringBank pTextLabelHeading, 
															const atHashWithStringBank pTextLabelBody,
															const eWarningButtonFlags iFlags = FE_WARNING_NONE,
															const bool bInsertNumber = false,
															const s32 NumberToInsert = -1,
															const char *pFirstSubString = NULL,
															const char *pSecondSubString = NULL,
															const bool bBlackBackground = true,
															const bool bAllowSpinner = false,
															int errorNumber = 0);

#if HAS_YT_DISCONNECT_MESSAGE
	static void SetYTConnectionLossMessage(bool display);

	static void BlockYTConnectionLossMessage(bool blockMsg);

	static bool UpdateYTConnectionLossMessage();
#endif

	static void SetFatalReadMessageWithHeader(	const eWarningMessageStyle style,
												const atHashWithStringBank pTextLabelHeading, 
												const atHashWithStringBank pTextLabelBody,
												const eWarningButtonFlags iFlags = FE_WARNING_NONE,
												const bool bInsertNumber = false,
												const s32 NumberToInsert = -1,
												const char *pFirstSubString = NULL,
												const char *pSecondSubString = NULL,
												const bool bBlackBackground = true,
												const bool bAllowSpinner = false,
												int errorNumber = 0);

	static void StoreNewControlState(bool bEnabling);
	static bool IsActive() { return ms_bActiveThisFrame || ms_bSetThisFrame || ms_Movie.IsActive(); }

	static bool CanRender() { return ms_bSetThisFrame; }
	static bool IsMovieStreaming() { return ms_bMovieStreaming; }

	static void Remove() { ms_bActiveThisFrame = false; }

	static bool AllowUnpause() { return ms_bUnpause; }
	static bool IsFlagForButtonActive( InputType eButton, u64 flagsToTest);
	static CScaleformMovieWrapper& GetMovieWrapper() { return ms_Movie; }

	static bool AllowSpinner() { return ms_bAllowSpinner; }

	static void ClearInstructionalButtons(CScaleformMovieWrapper* pOverride = NULL);
	static void SetInstructionalButtons(u64 iFlags, CScaleformMovieWrapper* pOverride = NULL);
	static void UpdateInstructionalButtons();
	static bool CheckInput(int iIndex, bool bHandleSound, int iExtraInputOverride = 0 );
	static bool CheckInput(InputType input, bool bHandleSound, int iExtraInputOverride = 0 );

	static u32 GetActiveMessage();

    static eParsableWarningButtonFlags GetParsableFlagsFromFlag( eWarningButtonFlags const flag );

private:
	static void CreateMovieIfNecessary();

	static void SetMessageInternal(	const eWarningMessageStyle style,
									const atHashWithStringBank pTextLabelHeading, 
									const atHashWithStringBank pTextLabelBody,
									const atHashWithStringBank pTextLabelSubtext,
									const eWarningButtonFlags iFlags, 
									const bool bInsertNumber,
									const s32 NumberToInsert,
									const char *pFirstSubString,
									const char *pSecondSubString,
									const bool bBlackBackground,
									const bool bAllowSpinner,
									int errorNumber);
	
	static int GetButtonMaskIndex(InputType eButton);

	static CScaleformMovieWrapper ms_Movie;
	static u8 ms_returnFromSystemFrameDelay;
	static bool ms_bMovieStreaming;
	static bool ms_bActiveThisFrame;
	static bool ms_bWasActiveLastFrame;
	static bool ms_bInactiveThisFrame;
	static bool ms_bSetThisFrame;
	static bool ms_bSetOptions;
	static bool ms_bAllowSpinner;
	static bool ms_lastInputKeyboard;

	static bank_u32 ms_iPlayerInputDisableDuration;
	static bank_u32 ms_iFrontendInputDisableDuration;

	static char ms_cMessageString[MAX_CHARS_IN_MESSAGE];
	static char ms_cErrorMessage[MAX_CHARS_IN_MESSAGE];
	static u32 ms_uLastBodyPassed;
	static atHashWithStringBank ms_cHeaderTextId;
	static atHashWithStringBank ms_cSubtextTextId;
	static eWarningButtonFlags ms_iFlags;

	static atHashWithStringBank ms_cLastMessageStringHash;
	static atHashWithStringBank ms_cLastHeaderTextId;
	static atHashWithStringBank ms_cLastSubtextTextId;

	static eWarningMessageStyle ms_Style;
	static bool ms_bBlackBackground;
	static bool ms_bUnpause;

	// buttons are indexed by bit value, ie, BIT(0), BIT(1), etc.
	static atArray<CMenuButtonWithSound>	ms_ButtonData;
	static atRangeArray<u64,4>	ms_AffectedButtonMasks; // only 4 for A,B,X,Y (for now?)

	// HACK - Last minute hack solution for controller reconnect messages.
	static bool ms_bIsControllerDisconnectWarningScreenSet;

	static bool ms_bNeedToDisplayYTConnectionError;
	static bool ms_bNeedToRefreshYTConnectionError;
	static bool ms_bIsDisplayingYTConnectionError;
	static bool ms_bBlockYTConnectionError;
};

class CWarningMessage : public datBase
{
public:
	struct Data
	{
		atHashWithStringBank m_TextLabelHeading;
		atHashWithStringBank m_TextLabelBody;
		atHashWithStringBank m_TextLabelSubtext;
		eWarningButtonFlags m_iFlags;
		bool m_bCloseAfterPress;
		bool m_bInsertNumber;
		eWarningMessageStyle m_Style;
		s32 m_NumberToInsert;
		CGxtString m_FirstSubString;
		CGxtString m_SecondSubString;
		datCallback m_acceptPressed;
		datCallback m_declinePressed;
		datCallback m_xPressed;
		datCallback m_yPressed;
		datCallback m_onUpdate;

		Data()
		{
			Clear();
		}

		void Clear()
		{
			m_Style = WARNING_MESSAGE_STANDARD;
			m_TextLabelHeading.Clear();
			m_TextLabelBody.Clear();
			m_TextLabelSubtext.Clear();

			m_iFlags = FE_WARNING_NONE;
			m_bCloseAfterPress = false;

			m_bInsertNumber = false;
			m_NumberToInsert = -1;

			m_FirstSubString.Reset();
			m_SecondSubString.Reset();

			m_acceptPressed = NullCallback;
			m_declinePressed = NullCallback;
			m_xPressed = NullCallback;
			m_yPressed = NullCallback;
			m_onUpdate = NullCallback;
		}

		bool UpdateCallbacks();
	};

	CWarningMessage() {Clear();}

	void Update();
	void Clear();
	
	void SetMessage(const Data& rMessage);
	bool IsActive() const { return m_isActive; }

	void UpdateCallbacks(Data& data);

private:

	bool m_isActive;
	Data m_data;
};

#endif // INC_WARNINGSCREEN_H_

// eof
