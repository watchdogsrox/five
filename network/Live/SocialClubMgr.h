//
// SocialClubMgr.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef SOCIALCLUBMGR_H
#define SOCIALCLUBMGR_H

#include "net/status.h"
#include "rline/socialclub/rlsocialclubcommon.h"
#include "rline/scauth/rlscauth.h"

#define SC_COLLECT_AGE 1
#define SC_COLLECT_AGE_ONLY(x) x

#define SPAMMABLE_AGE_YEARS (18)
//NOTE: SC signup may actually be 14 in certain countries,
//in which case the backend will reject us. We should replace
//this with a purely backend check later once it becomes available
#define JOINABLE_AGE_YEARS (13)

#define MAX_SOCIAL_CLUB_LANG_LEN 5
#define MAX_SOCIAL_CLUB_COUNTRY_LEN 5

namespace rage {
	class bkText;
};

class SocialClubMgr
{
	friend class CLiveManager;
private:
	enum 
	{
		OP_NONE,
		OP_CREATE_ACCOUNT,
		OP_LINK_ACCOUNT,
		OP_UNLINK_ACCOUNT,
		OP_GET_ACCOUNT_INFO,
		OP_GET_PASSWORD_REQS
	};

	struct SocialClubInfo
	{
		friend class SocialClubMgr;

	private:
		char	m_Email		[RLSC_MAX_EMAIL_CHARS];
		char	m_Nickname	[RLSC_MAX_NICKNAME_CHARS];
		char	m_Password	[RLSC_MAX_PASSWORD_CHARS];
		char	m_BirthDate	[RLSC_MAX_DATE_CHARS];
		u8		m_Day		;
		u8		m_Month		;
		u16		m_Year		;

		bool m_bSpam;

	public:
		SocialClubInfo() { Reset(); }
		void Reset()
		{
			m_Email		[0] = 0;
			m_Nickname	[0] = 0;
			m_Password	[0] = 0;
			m_BirthDate	[0]	= 0;
			m_Day			= 0;
			m_Month			= 0;
			m_Year			= 0;
			m_bSpam			= false;
		}

		bool IsOldEnoughToJoinSC() const;

		bool IsSpammable() const;
		void SetSpam(bool bSpam) {m_bSpam = bSpam;}

		void SetAge(u8 day, u8 month, u16 year);

		void SetEmail(const char* pEmail);
		const char* GetEmail() const {return m_Email;}

		void SetPassword(const char* pPassword);
		const char* GetPassword() const {return m_Password;}

		void SetNickname(const char* pNickname);
		const char* GetNickname() const {return m_Nickname;}

	} m_SocialClubInfo;

	netStatus  m_MyStatus;
	u32        m_Operation;

	datCallback m_onFinishedCallback;

	rlScPasswordReqs m_PasswordRequirements;
	u32 m_LastPasswordRequirementCheck;

	bool		m_bLocalInitialLink;

#if __BANK
	bool m_isOnlinePolicyUpToDate;

	// Includes the email, nickname, and status
	char m_BankSCAcountStatus[RLSC_MAX_EMAIL_CHARS + RLSC_MAX_NICKNAME_CHARS + 50];

	bkText* m_pBankAccountStatusWidget;
#endif

public:

	~SocialClubMgr() {}

	void Init();
	void Update(const u32 timeStep);
	void Shutdown();

	static bool IsYearValid(u16 year);
	static bool IsMonthValid(u8 month); 
	static bool IsDayValid(u8 day);
	static bool IsDayMonthValid(u8 day, u8 month);
	static bool IsDateValid(u8 day, u8 month, u16 year);

	//tasks
	bool CreateAccount( const char* countryCode, const char* languageCode, datCallback onFinishedCallback = NullCallback);
	bool LinkAccount(datCallback onFinishedCallback = NullCallback);
	void CancelTask();

	// PURPOSE
	//	Begins a Social Club account link operation. The system web browser will be launched for
	//	scauth.rockstargames.com, in either 'signup' or 'signin' mode.
	// PARAMS
	//	mode - Specify either 'signup' or 'signin' to tweak the page that is loaded.
	//	onFinishedCallback - function to be called upon copmletion
	// Returns
	//	LinkAccountError - Returns ERR_SUCCESS
	bool LinkAccountScAuth(rlScAuth::AuthMode mode, datCallback onFinishedCallback = NullCallback);

	// password requirements
	bool RetrievePasswordRequirements();
	void RetryPasswordRequirements() { m_LastPasswordRequirementCheck = 0; }
	rlScPasswordReqs& GetPasswordRequirements() { return m_PasswordRequirements; }

#if __BANK
	bool UnlinkAccount(datCallback onFinishedCallback = NullCallback);
	bool GetUpToDateToggleState() const {return m_isOnlinePolicyUpToDate;}
#endif

	//accessors
	bool IsLocalGamerAccountInfoValid() const;
	const rlScAccountInfo* GetLocalGamerAccountInfo() const;
	bool IsConnectedToSocialClub() const;
	bool IsOnlinePolicyUpToDate() const;
	const char* GetOperationName(const int operation) const;
	const char*			GetErrorName() const { return GetErrorName(m_MyStatus); }
	static const char*  GetErrorName(const netStatus& erroredStatus);
	static const char*  GetErrorName(int resultCode);

	bool IsLocalInitialLink() { return m_bLocalInitialLink; }
	void ResetLocalInitialLink() { m_bLocalInitialLink = false; }

	bool IsShowingWebBrowser() const { return m_ScAuth.IsInitialized() && m_ScAuth.IsShowingBrowser(); }

	bool IsPending() const { return m_MyStatus.Pending(); }
	bool IsIdle()    const { return (netStatus::NET_STATUS_NONE == m_MyStatus.GetStatus()); }
	bool Succeeded() const { return m_MyStatus.Succeeded(); }
	bool Failed()    const { return m_MyStatus.Failed(); }
	bool Canceled()  const { return m_MyStatus.Canceled(); }

	SocialClubInfo& GetSocialClubInfo() {return m_SocialClubInfo;}

	void SetOnlinePolicyStatus(bool BANK_ONLY(isUpToDate)) {BANK_ONLY(m_isOnlinePolicyUpToDate = isUpToDate;)}

private:
	SocialClubMgr();

	void Clear();

	void ProcessOperationSucceeded();
	void ProcessOperationFailed();
	void ProcessOperationCanceled();
	
	void UpdatePasswordRequirements();

	rlScAuth m_ScAuth;

#if !__FINAL
public:
	static void GetDefaultDevNames(char* szEmail, char* szNick);
	void Debug_AutofillAccountInfo(); // This is used as a debug option in the SC menu
#endif

#if __BANK

public:
	void InitWidgets(bkBank* pBank);
	void Bank_UnlinkAccount();

private:
	void Bank_Update();
	void Bank_CreateAccount();

#endif // __BANK
};


#endif // SOCIALCLUBMGR_H

// eof

