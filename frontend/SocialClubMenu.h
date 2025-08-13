/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SocialClubMenu.h
// PURPOSE : SocialClubMenu.cpp
// AUTHOR  : James Chagaris
// STARTED : 08/08/2012
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef INC_SOCIALCLUBMENU_H_
#define INC_SOCIALCLUBMENU_H_

// rage
#include "atl/singleton.h"
#include "input/virtualkeyboard.h"
#include "rline/rlpresence.h"
#include "rline/ros/rlroscommon.h"
#include "data/callback.h"
#include "system/criticalsection.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/WarningScreen.h"
#include "Frontend/PauseMenu.h"
#include "Network/Live/SocialClubMgr.h"
#include "Network/SocialClubUpdates/SocialClubPolicyManager.h"
#include "text/TextFile.h"
#include "scene/DownloadableTextureManager.h"

#define DEBUG_SC_MENU (!__FINAL)

#if DEBUG_SC_MENU
	#define DEBUG_SC_MENU_ONLY(_x)	_x
#else // DEBUG_SC_AUTOFILL
	#define DEBUG_SC_MENU_ONLY(_x)
#endif // DEBUG_SC_AUTOFILL

#define KEYBOARD_LABEL_SIZE (40)

#define NICKNAME_CHECK_COOLDOWN_TIME (7000)
#define NICKNAME_CHECKS_ALLOWED_IN_COOLDOWN (3)

class netStatusWithCallback: public netStatus
{
public:
	netStatusWithCallback(datCallback callback = NullCallback) : m_previousStatus(NET_STATUS_NONE), m_callback(callback) {}
	void SetCallback(datCallback callback) {m_callback = callback;}

	void Update()
	{
		if(m_previousStatus != GetStatus())
		{
			m_previousStatus = GetStatus();
			m_callback.Call(this);
		}
	}

	bool HasValidCallback() const {return !(m_callback == NullCallback);}

private:
	StatusCode m_previousStatus;
	datCallback m_callback;
};

class SCTour
{
public:
	SCTour();
	~SCTour() {Shutdown();}

	void Shutdown();

	void StartDownload();
	void UpdateDownload();
	void ReleaseAndCancelTexture();
	bool IsDownloading() const {return m_TextureDictionarySlot == -1 && !m_hasDownloadFailed;}
	bool HasCloudImage() const {return m_hasCloudImage;}

	void SetTag(const char* pTag);
	void SetHeader(const char* pHeader) {m_header.Reset(); m_header.Append(pHeader);}
	void SetBody(const char* pBody) {m_body.Reset(); m_body.Append(pBody);}
	void SetCallout(const char* pCallout) {m_callout.Reset(); m_callout.Append(pCallout);}
	void SetImg(const char* pFullPath);

	u32 GetTagHash() const {return m_tagHash;}
	const char* GetHeader() const {return m_header.GetData();}
	const char* GetBody() const {return m_body.GetData();}
	const char* GetCallout() const {return m_callout.GetData();}
	const char* GetTextureName() const {return m_filename.c_str();}

private:
	bool m_hasCloudImage;
	bool m_hasDownloadFailed;
	u32 m_tagHash;
	strLocalIndex m_TextureDictionarySlot;
	CGxtString m_header;
	CGxtString m_body;
	CGxtString m_callout;
	atString m_filepath; // Cloud path
	atString m_filename; // Local Name, can be whatever
	CTextureDownloadRequestDesc m_requestDesc;
	TextureDownloadRequestHandle m_requestHandle;
};

class SCTourManager : CloudListener
{
public:
	SCTourManager() {Shutdown();}
	~SCTourManager() {Shutdown();}

	void Init(int gamerIndex, const char* pLangCode);
	void Shutdown();
	void Update();

	bool IsDownloading();
	bool HasError() const {return m_hasError;}
	int NumTours() const {return m_tours.size();}
	SCTour& GetTour(int index) {return m_tours[index];}

	const char* GetError() const;
	const char* GetErrorTitle() const;

	int GetStartingTour(u32 tourHash);

private:

	void OnCloudEvent(const CloudEvent* pEvent);

	bool m_downloadComplete;
	bool m_hasError;
	int m_gamerIndex;
	int m_resultCode;
	atArray<SCTour> m_tours;
	atString m_tourFilename;

	CloudRequestID m_fileRequestId;
};

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
class BaseSocialClubMenu
{
public:
	static inline bool IsActive() {return sm_isActive;}

	static bool IsFlaggedForDeletion() {return sm_flaggedForDeletion;}
	static void FlagForDeletion();
	
	static CScaleformMovieWrapper* GetButtonWrapper();


protected:
	enum eButtonFlags
	{
		BUTTON_NONE = 0,
		BUTTON_SELECT = BIT(0),
		BUTTON_BACK = BIT(1),
		BUTTON_GOTO_SIGNIN = BIT(2),
		BUTTON_CANCEL = BIT(3),
		BUTTON_EXIT = BIT(4),
		BUTTON_SCROLL_UPDOWN = BIT(5),
		BUTTON_LR_TAB = BIT(6),
		BUTTON_READ = BIT(7),
		BUTTON_ACCEPT = BIT(8),
		BUTTON_SUGGEST_NICKNAME = BIT(9),
		BUTTON_UPLOAD = BIT(10),

		// The below only does anything if DEBUG_SC_MENU is true.
		BUTTON_AUTOFILL = BIT(29),
	};

	enum eOnlinePolicyOptions
	{
		OPO_EULA,
		OPO_PP,
		OPO_TOS,
		OPO_ACCEPT,
		OPO_SUBMIT,
		OPO_MAX
	};

	enum eScrollOptions
	{
		SO_UP,
		SO_DOWN,
		SO_BOTTOM,
		SO_TOP,
		SO_STATIC
	};

	BaseSocialClubMenu();
	virtual ~BaseSocialClubMenu();

	virtual void Shutdown();
	virtual void Update();

	void DelayButtonCheck();
	void SetButtons(u32 buttonFlags);
	void AddInstructionalButton(CMenuButtonList& buttonList, InputGroup const inputGroup, const char* locLabel);
	void AddInstructionalButton(CMenuButtonList& buttonList, InputType const inputType, const char* locLabel);

	CScaleformMovieWrapper::Param BuildLocTextParam(atHashWithStringBank aLabel, bool html = true);
	CScaleformMovieWrapper::Param BuildLocTextParam(const char* pLabel, bool html = true);
	CScaleformMovieWrapper::Param BuildNonLocTextParam(const char* pText, bool html = true);
	void CallMovieWithPagedText(const char* pMovie, const PagedCloudText& str);

	bool UpdateOption(int& value, int maxValue, const bool* pValidOptions = NULL);
	virtual void UpdateError();

	void SetBusySpinner(bool isVisible);
	void AcceptPolicyVersions(bool cloudVersion);

	void CheckScroll();
	bool CheckInput(eFRONTEND_INPUT input, bool playSound = true);

	static bool sm_flaggedForDeletion;
	static bool sm_isActive;
	static u8 sm_framesUntilDeletion;

	bool m_acceptPolicies;
	u8 m_showDelayedButtonPrompts;
	eButtonFlags m_pendingButtons;
	eOnlinePolicyOptions m_opOption;

	CSystemTimer m_delayTimer;

	atQueue<u32, NICKNAME_CHECKS_ALLOWED_IN_COOLDOWN> m_nicknameRequestTimes;

	CScaleformMovieWrapper m_scMovie;
	CScaleformMovieWrapper m_buttonMovie;
	
	CWarningMessage m_errorMessage;
	CWarningMessage::Data m_errorMessageData;

	u32 m_buttonFlags;
	bool m_hasPendingNicknameCheck;

	KEYBOARD_MOUSE_ONLY( bool m_bLastInputWasKeyboard; )
};

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
class SocialClubLegalsMenu: public BaseSocialClubMenu
{
	enum eSCLegalStates
	{
		SC_IDLE,
		SC_DOWNLOADING,
		SC_ONLINE_POLICIES,
		SC_EULA,
		SC_PP,
		SC_TOS,
		SC_DECLINE_WARNING,
		SC_DOWNLOAD_ERROR,
		SC_ERROR
	};

public:
	SocialClubLegalsMenu();
	~SocialClubLegalsMenu();

	void Init();
	virtual void Shutdown();
	void Update();
	void Render();
	static void RenderWrapper();

	bool IsIdle() const {return m_state == SC_IDLE;}

	void UpdateDownloading();
	void UpdateOnlinePolicies();
	void UpdateDocs();
	void UpdateDownloadError();

	KEYBOARD_MOUSE_ONLY ( void UpdateMouseEvent(const GFxValue* args); )

	void UpdatePolicyHighlight();
	void SelectCurrentOption();

	void GoToDownloading();
	void GoToOnlinePolicies();
	void GoToEula();
	void GoToPrivacyPolicy();
	void GoToTOS();
	void GoToDeclineWarning();
	void GoToDownloadError();

	void GoToState(eSCLegalStates newState);

	void AcceptBackoutWarning();
	void DeclineBackoutWarning();

private:
	eSCLegalStates m_state;

	SocialClubPolicyManager m_policyManager;
};

typedef atSingleton<SocialClubLegalsMenu> SSocialClubLegalsMenu;

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
class SocialClubMenu: public BaseSocialClubMenu
{
	enum eMiscSCValues
	{
		MISCSC_MAX_ALT_NICKNAMES = 5,
	};

	enum eSocialClubStates
	{
		SC_IDLE,
		SC_DOWNLOADING,
		SC_WELCOME,
#if SC_COLLECT_AGE
		SC_DOB,
#endif
		SC_ONLINE_POLICIES,
		SC_EULA,
		SC_PP,
		SC_TOS,
		SC_SIGNUP,
		SC_SIGNUP_CONFIRM,
		SC_SIGNING_UP,
		SC_SIGNUP_DONE,
		SC_SIGNIN,
		SC_SIGNING_IN,
		SC_SIGNIN_DONE,
		SC_RESET_PASSWORD,
		SC_RESETTING_PASSWORD,
		SC_RESET_PASSWORD_DONE,
		SC_POLICY_UPDATE_ACCEPTED,
		SC_ERROR,

		// scauth
		SC_AUTH_SIGNUP,
		SC_AUTH_SIGNUP_DONE,
		SC_AUTH_SIGNIN,
		SC_AUTH_SIGNIN_DONE
	};

	enum eWelcomOptions
	{
		WO_SIGNUP,
		WO_SIGNIN,
		WO_MAX
	};

	enum eDOBOptions
	{
		DOBO_START_DATE,
		DOBO_MONTH = DOBO_START_DATE,
		DOBO_DAY,
		DOBO_YEAR,
		DOBO_SUBMIT,
		DOBO_MAX
	};

	enum eOnlinePolicyOptions
	{
		OPO_EULA,
		OPO_PP,
		OPO_TOS,
		OPO_ACCEPT,
		OPO_SUBMIT,
		OPO_MAX
	};

	enum eSignUpOptions
	{
		SUO_NICKNAME,
		SUO_EMAIL,
		SUO_PASSWORD,
		SUO_SPAM,
		SUO_SUBMIT,
		SUO_MAX
	};

	enum eSignInOptions
	{
		SIO_NICK_EMAIL,
		SIO_PASSWORD,
		SIO_SUBMIT,
		SIO_RESET_PASSWORD,
		SIO_MAX
	};

	enum eResetPasswordOptions
	{
		RPO_EMAIL,
		RPO_SUBMIT,
		RPO_MAX
	};

	enum eVerifyStates
	{
		VS_STATUS_QUO = -1,
		VS_NONE,
		VS_BUSY,
		VS_ERROR,
		VS_VALID
	};

public:
	SocialClubMenu();
	~SocialClubMenu();

	static void Open( bool const isPolicyMenu = false, bool const ignoreLoadingScreens = false );
	static void Close();
	static void UpdateWrapper();
	static void RenderWrapper();
	static void CacheOffPMState();

	void Init();
	virtual void Shutdown();
	void Update();
	void Render();
	bool IsIdle() const {return m_state == SC_IDLE;}
    static bool IsIdleWrapper();

	KEYBOARD_MOUSE_ONLY ( void CheckMouseEvent(); )

	void UpdatePolicyHighlight();
	void SelectCurrentOption();

	static void OnPresenceEvent(const rlPresenceEvent* evt);
	static void OnRosEvent(const rlRosEvent& evt);

	static void LockRender() {sm_renderingSection.Lock();}
	static void UnlockRender() {sm_renderingSection.Unlock();}

	static void LockNetwork() {sm_networkSection.Lock();}
	static void UnlockNetwork() {sm_networkSection.Unlock();}

	static void SetTourHash(u32 tourHash) {sm_tourHash = tourHash;}

	void SetGotCredsResult() {m_gotCredsResult = true;}

private:

	// State update functions
	void UpdateDownloading();
	void UpdateWelcome();
	SC_COLLECT_AGE_ONLY(void UpdateDOB();)
	void UpdateOnlinePolicies();
	void UpdateEula();
	void UpdatePrivacyPolicy();
	void UpdateTOS();
	void UpdateSignUp();
	void UpdateSignUpConfirm();
	void UpdateSigningUp();
	void UpdateSignUpDone();
	void UpdateSignIn();
	void UpdateSigningIn();
	void UpdateSignInDone();
	void UpdateResetPassword();
	void UpdateResettingPassword();
	void UpdateResetPasswordDone();
	void UpdatePolicyUpdateSummary();
	void UpdatePolicyUpdateAccepted();
	void UpdateScAuthSignup();
	void UpdateScAuthSignupDone();
	void UpdateScAuthSignin();
	void UpdateScAuthSigninDone();
	virtual void UpdateError();

	void CheckPolicyUpdate();
	void CheckNextPrev();
	void CheckAutoFill();

	// Go to state functions
	void GoToDownloading();
	void GoToWelcome();
	SC_COLLECT_AGE_ONLY(void GoToDOB();)
	void GoToOnlinePolicies();
	void GoToEula();
	void GoToPrivacyPolicy();
	void GoToTOS();
	void GoToSignUp();
	void GoToSigningUp();
	void GoToSignUpConfirm();
	void GoToSignUpDone();
	void GoToSignIn();
	void GoToSigningIn();
	void GoToSignInDone();
	void GoToResetPassword();
	void GoToResettingPassword();
	void GoToResetPasswordDone();
	void GoToPolicyUpdateAccepted();
	void GoToScAuthSignup();
	void GoToScAuthSignUpDone();
	void GoToScAuthSignin();
	void GoToScAuthSigninDone();
	void GoToError(const CWarningMessage::Data& rMessage);

	void GoToNext();
	void GoToPrevious();
	void GoToState(eSocialClubStates newState);

	void SetWelcomeTab(s8 welcomeTab);

	void SetupEmailKeyboard();
	void SetupNicknameKeyboard();
	void SetupNickEmailKeyboard();
	void SetupPasswordKeyboard();
	void SetupDateKeyboard(const char* pTitle, const char* pInitialStr, int maxLength);
	void SetupKeyboardHelper(const char* pTitle, const char* pInitialStr, ioVirtualKeyboard::eTextType keyboardType, int maxLength = -1, bool textEvaluated = false); // Default/Max size for maxLength is ioVirtualKeyboard::MAX_STRING_LENGTH

	void FillOutEmailFromKeyboard();
	void FillOutNicknameFromKeyboard();
	void FillOutNickEmailFromKeyboard();
	void FillOutPasswordFromKeyboard();
	void FillOutDateFromKeyboard();

	void FillOutEmail(const char* pEmail, bool canResetToDefault = true);
	void FillOutNickname(const char* pNickname, bool canResetToDefault = true);
	void FillOutPassword(const char* pPassword);

	void ValidateNickname(const char* pNickname);

	void SuggestNickname(bool isAutoSuggest);

#if DEBUG_SC_MENU
	void Debug_AutofillAccountInfo();
#endif

	// Errors will kick players out of the menu completely, warnings will just display a message.
	void SetGenericWarning(const char* pText, const char* pTitle = "SC_WARNING_TITLE");
	void SetGenericWarning(const char* pText, eSocialClubStates returnState, const char* pTitle = "SC_WARNING_TITLE");
	void SetGenericError(const char* pText, const char* pTitle = "SC_ERROR_TITLE");

	void CheckSignUpNickEmailVerification(CallbackData function, void* pData);
	void CheckSignInNickEmailVerification(CallbackData function, void* pData);
	void CheckPasswordVerification(CallbackData function, void* pData);
	void CheckResetPasswordVerification(CallbackData function, void* pData);

	void UpdateSignUpSubmitState();
	void UpdateSignInSubmitState();
	void UpdateResetPasswordSubmitState();

	void AccountCreationCallback(void* pData);
	void AccountLinkCallback(void* pData);
	void AccountResetPasswordCallback(void* pData);
	void SuggestNicknameCallback();
	void AutoSuggestNicknameCallback();
	void ScAuthSignupCallback(void* pData);
	void ScAuthSigninCallback(void* pData);


	static bool sm_isTooYoung;
	static bool sm_wasPMOpen;
	static bool sm_shouldUnpauseGameplay;
	static u32 sm_currentTourHash;
	static u32 sm_tourHash;
	static eFRONTEND_MENU_VERSION sm_currentMenuVersion;
	static eMenuScreen sm_currentMenuScreen;

	bool m_isKeyboardOpen;
	bool m_allowEmailSpam;
	bool m_isSpammable;
	bool m_acceptPolicies;
	bool m_isInSignInFlow;
	bool m_isPolicyUpdateFlow;
	bool m_gotCredsResult;
	s8 m_welcomeTab;
	u8 m_day;
	u8 m_month;
	u16 m_year;

	eSocialClubStates m_state;
	eWelcomOptions m_wOptions;
	eDOBOptions m_dobOption;
	eOnlinePolicyOptions m_opOption;
	eSignUpOptions m_suOption;
	eSignInOptions m_siOption;
	eResetPasswordOptions m_rpOption;

	SocialClubPolicyManager m_policyManager;
	SCTourManager m_tourManager;

	char m_dayBuffer[3];
	char m_monthBuffer[3];
	char m_yearBuffer[5];
	char m_originalNickname[RLSC_MAX_NICKNAME_CHARS];
	char16 m_keyboardLabel[KEYBOARD_LABEL_SIZE];
	char16 m_keyboardInitialStr[ioVirtualKeyboard::MAX_STRING_LENGTH_FOR_VIRTUAL_KEYBOARD];
	ioVirtualKeyboard::Params m_keyboardParams;
	
	datCallback m_keyboardCallback;

	netStatusWithCallback  m_emailStatus;
	netStatusWithCallback  m_nicknameStatus;
	netStatusWithCallback  m_alternateNicknameStatus;
    netStatusWithCallback  m_passwordStatus;
	netStatusWithCallback  m_netStatus;

	char m_alternateNicknames[MISCSC_MAX_ALT_NICKNAMES][RLSC_MAX_NICKNAME_CHARS];
	u32 m_altCount;
	s32 m_currAlt;

    rlScPasswordReqs m_passwordReqs;
	CSystemTimer m_delayTimer;

	static sysCriticalSectionToken sm_renderingSection;
	static sysCriticalSectionToken sm_networkSection;
};

typedef atSingleton<SocialClubMenu> SSocialClubMenu;

#endif // INC_SOCIALCLUBMENU_H_

// eof
