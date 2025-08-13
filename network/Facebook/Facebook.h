//
// Facebook.h
//
// Copyright (C) 2013 Rockstar Games.  All Rights Reserved.
//
// Exposes facebook posting functionality to the game via rage.
// url:bugstar:982616 Integrate Rage facebook sharing into game.

#ifndef NETWORK_FACEBOOK_H
#define NETWORK_FACEBOOK_H

#include "rline/facebook/rlfacebookcommon.h"
#include "rline/facebook/rlfacebook.h"

#if RL_FACEBOOK_ENABLED

#define FACEBOOK_WORKBUFFER_SIZE	(1024*8)
#define FACEBOOK_POST_ID_LEN		64

#define FACEBOOK_MAX_POSTACTION		32
#define FACEBOOK_MAX_OBJNAME		32
#define FACEBOOK_MAX_OBJTARGETDATA	256
#define FACEBOOK_MAX_EXTRAPARAMS	256

enum eFbPhase
{
	FACEBOOK_PHASE_IDLE=0,

	FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_INIT,
	FACEBOOK_PHASE_GETACCCESSTOKEN_SILENT_WAIT,

	FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_BEFORE,

	FACEBOOK_PHASE_GETACCCESSTOKEN_INIT,
	FACEBOOK_PHASE_GETACCESSTOKEN_WAIT,

	FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER,
	FACEBOOK_PHASE_GETACCESSTOKEN_USER_INPUT_AFTER_DELAY,

	FACEBOOK_PHASE_POST_INIT,
	FACEBOOK_PHASE_POST_WAIT,

	FACEBOOK_PHASE_POST_BOUGHT_GTA_INIT,
	FACEBOOK_PHASE_POST_BOUGHT_GTA_WAIT,

	FACEBOOK_PHASE_PRE_CLEANUP,

	FACEBOOK_PHASE_CLEANUP
};

// script: MILESTONE_ID
enum eMileStoneId
{
	FACEBOOK_MILESTONE_CHECKLIST=0,	// 100% Complete game.
	FACEBOOK_MILESTONE_OVERVIEW,	// 100% Complete story mission.
	FACEBOOK_MILESTONE_VEHICLES,	// Driven all cars.
	FACEBOOK_MILESTONE_PROPERTIES,	// Bought all properties.
	FACEBOOK_MILESTONE_PSYCH,		// Future: psych report (TBD)
	FACEBOOK_MILESTONE_MAPREVEAL,	// Map revealed.
	FACEBOOK_MILESTONE_PROLOGUE,	// Prologue completed

	FACEBOOK_MILESTONE_COUNT
};

enum eFbError
{
	FACEBOOK_ERROR_OK=0,
	FACEBOOK_ERROR_GETACESSTOKEN,
	FACEBOOK_ERROR_POST
};

struct fbkMilestone
{
	eMileStoneId Id;
	const char* Name;
};

using namespace rage;

#if __BANK
namespace rage
{
	class bkBank;
}

#define FACEBOOK_BANK_MAX_PHASE				64
#define FACEBOOK_BANK_MAX_LASTERROR			64
#define FACEBOOK_BANK_MAX_ID_LEN			64
#define FACEBOOK_BANK_MAX_POSTACTION		32
#define FACEBOOK_BANK_MAX_OBJNAME			32
#define FACEBOOK_BANK_MAX_OBJTARGETDATA		256
#define FACEBOOK_BANK_MAX_EXTRAPARAMS		256
#define FACEBOOK_BANK_MAX_PARAM				64
#define FACEBOOK_BANK_MAX_ACCESSTOKEN_STATE	128
#define FACEBOOK_BANK_MAX_CANREPORT_STATE	128

#endif

class CFacebook: public datBase
{
public:
	
	CFacebook();
	~CFacebook();
	
	void Init();
	void Shutdown();
	void Update();

	bool		IsBusy() const;
	bool		DidSucceed() const;
	eFbError	GetLastError() const;
	rlFbGetTokenResult	GetLastResult() const;
	void		EnableGUI( bool setEnabled );
	void		EnableAutoGetAccessToken( bool setEnabled );	// default disabled as the frontend menu can retrieve the access token too.
	bool		HasAccessToken() const;

	bool		CanReportToFacebook();		// Used to determine if the "Post to Facebook" option is displayed.
#if !__NO_OUTPUT
	const char* GetNoReportToFacebookReason();
#endif

	void		TryPostBoughtGame();

	bool		IsOnline();
	bool		HasSocialClubAccount();
	bool		IsProfileSettingEnabled();
	bool		HasProfileSettingHint();
	void		SetProfileSettingHint(bool bHintEnabled);
	void		OutputProfileSettings();
	
	const char*	GetLastPostId() const;
	bool		GetAccessToken( void );

	bool		PostPlayMission( const char* missionId, int xpEarned, int rank );
	bool		PostLikeMission( const char* missionId );
	bool		PostPublishMission( const char* missionId );
	bool		PostPublishPhoto( const char* screenshotId );

	bool		PostTakePhoto( const char* screenshotId );
	bool		PostCompleteHeist( const char* heistId, int cashEarned, int xpEarned );
	bool		PostCompleteMilestone( eMileStoneId milestoneId );
	bool		PostCreateCharacter();
	bool		PostRaw( const char* action, const char* objName, const char* objTargetData, const char* extraParams );

	const char*	GetPlayerCharacter();

#if RSG_PC
	void		UpdateKillswitch();
#endif

	void		InitTunables();

#if __BANK
	void InitWidgets( bkBank& bank );
#endif

private:

	eFbPhase	m_StartPhase;
	eFbPhase	m_Phase;
	eFbError	m_LastError;
	int			m_LastResult;
	netStatus	m_Status;
	
	bool		m_LastOnlineState;
	bool		m_LastSocialSharingPrivileges;
	char		m_PostId[FACEBOOK_POST_ID_LEN];

	char		m_PostAction[FACEBOOK_MAX_POSTACTION];				// Action (e.g. share,take,like)
	char		m_PostObjName[FACEBOOK_MAX_OBJNAME];				// Object of the action (e.g. screenshot,mission)
	char		m_PostObjTargetData[FACEBOOK_MAX_OBJTARGETDATA];	// Permalink to the object that facebook can access (http://....), or for UGC types like gta5mission, gta5missionplaylist, or screenshots, pass in the UGC Id of the object.
	char		m_PostObjExtraParams[FACEBOOK_MAX_EXTRAPARAMS];		// Any extra params to pass as part of the facebook action. Format: "key=value&key2=value2&key3=value3". Doesn't need to be url-escaped.

	u32			m_FacebookSuccessTimer;
	u32			m_uDelayStartTime;

#if RSG_PC
	bool		m_bAllowFacebookKillswitch;
#endif

	void		UpdateOnlineStateIdle();
	
	bool		ShouldPostBoughtGame();
	void		SetPostedBoughtGame();
	const char* TranslateMilestoneName( eMileStoneId milestoneId );

	void		SetPhase(eFbPhase e);

	void		EnableBusySpinner(const char* source);
	void		DisableBusySpinner();

#if __BANK
	void		Bank_Update();
#endif

protected:
	
#if __BANK

	static bool	sm_BankEnableGUI;
	static bool sm_BankEnableAutoGetAccessToken;
	static char	sm_BankPhase[FACEBOOK_BANK_MAX_PHASE];
	static char	sm_BankLastError[FACEBOOK_BANK_MAX_LASTERROR];
	static char sm_BankOutPostId[FACEBOOK_BANK_MAX_ID_LEN];
	static char sm_BankAccessTokenState[FACEBOOK_BANK_MAX_ACCESSTOKEN_STATE];
	static char sm_BankCanReportState[FACEBOOK_BANK_MAX_CANREPORT_STATE];
	
	static char sm_BankPostAction[FACEBOOK_BANK_MAX_POSTACTION];
	static char sm_BankPostObjName[FACEBOOK_BANK_MAX_OBJNAME];
	static char sm_BankPostObjTargetData[FACEBOOK_BANK_MAX_OBJTARGETDATA];
	static char sm_BankPostObjExtraParams[FACEBOOK_BANK_MAX_EXTRAPARAMS];

	static char sm_BankPostPublishPhotoA[FACEBOOK_BANK_MAX_PARAM];
	static char sm_BankPostPlayMissionA[FACEBOOK_BANK_MAX_PARAM];
	static char sm_BankPostPlayMissionB[FACEBOOK_BANK_MAX_PARAM];
	static char sm_BankPostPlayMissionC[FACEBOOK_BANK_MAX_PARAM];
	static char sm_BankPostLikeMissionA[FACEBOOK_BANK_MAX_PARAM];
	static char sm_BankPostPublishMissionA[FACEBOOK_BANK_MAX_PARAM];

	static char sm_BankPostCompleteHeistA[FACEBOOK_BANK_MAX_PARAM];
	static char sm_BankPostCompleteHeistB[FACEBOOK_BANK_MAX_PARAM];
	static char sm_BankPostCompleteHeistC[FACEBOOK_BANK_MAX_PARAM];

	static char sm_BankPostCompleteMilestoneA[FACEBOOK_BANK_MAX_PARAM];

#if RSG_PC
	static bool sm_BankKillswitch;
#endif

	void		Bank_PostRaw();
	void		Bank_PostPlayMission();
	void		Bank_PostLikeMission();
	void		Bank_PostPublishMission();
	void		Bank_PostPublishPhoto();
	void		Bank_PostCompleteHeist();
	void		Bank_PostCompleteMilestone();
	void		Bank_PostCreateCharacter();
	void		Bank_LinkToFacebook();

#endif

};

#endif // RL_FACEBOOK_ENABLED
#endif // NETWORK_FACEBOOK_H
