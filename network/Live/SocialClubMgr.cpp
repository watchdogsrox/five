//
// SocialClubMgr.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

// rage
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/text.h"
#include "diag/channel.h"
#include "fwnet/netchannel.h"
#include "rline/socialclub/rlsocialclub.h"
#include "system/exec.h"

//game
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/LiveManager.h"
#include "Network/Live/SocialClubMgr.h"
#include "Network/SocialClubUpdates/SocialClubPolicyManager.h"

#include <time.h>

NETWORK_OPTIMISATIONS()

RAGE_DECLARE_SUBCHANNEL(net, sc)
#undef __net_channel
#define __net_channel net_sc

//Helper to compute the age of a user using the local time and the supplied
//dob.
#if SC_COLLECT_AGE
int CalculateAgeFromDob(const int day, const int month, const int year)
{
    //We calculate age simply based on the current day, we don't care about accuracy to the second
    //or anything
    time_t curTime_t = time(NULL);

    //Note: localtime isn't guaranteed to be thread-safe, as it returns a pointer to a shared/static object
    //that gmtime also uses. On the 360 this uses tls so is fine. 
    //We're already calling this in a lot of places though, so should
    //be fine as long as it's only being called from the main thread
    tm curTime = *localtime(&curTime_t);

    // Compute dob years since 1900, dob months since January, since that's what tm uses
    // For the day, we use tm_mday, which is the day of the month (1-31), which is consistent with our day input
    const int dob_day = day;
    const int dob_month = month - 1;
    const int dob_year = year - 1900;

    int age = curTime.tm_year - dob_year;

    if (dob_month > curTime.tm_mon ||
        (dob_month == curTime.tm_mon && dob_day > curTime.tm_mday))
    {
        age--;
    }

    return age;
}
#endif //#if SC_COLLECT_AGE

bool SocialClubMgr::SocialClubInfo::IsOldEnoughToJoinSC() const
{
#if __WIN32PC
	return true;
#elif SC_COLLECT_AGE
	//date needs to be valid
	if(!IsDateValid(m_Day, m_Month, m_Year))
		return false;

    const int age = CalculateAgeFromDob(m_Day, m_Month, m_Year);

	return age >= JOINABLE_AGE_YEARS;
#else
	return rlGamerInfo::GetAgeGroup(NetworkInterface::GetLocalGamerIndex())>=RL_AGEGROUP_TEEN;
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  SocialClubInfo
///////////////////////////////////////////////////////////////////////////////
bool SocialClubMgr::SocialClubInfo::IsSpammable() const
{
#if __WIN32PC
	return true;
#elif SC_COLLECT_AGE
	//date needs to be valid
	if(!IsDateValid(m_Day, m_Month, m_Year))
		return false;

    const int age = CalculateAgeFromDob(m_Day, m_Month, m_Year);
	
	return age >= SPAMMABLE_AGE_YEARS;
#else
	return rlGamerInfo::GetAgeGroup(NetworkInterface::GetLocalGamerIndex())>RL_AGEGROUP_TEEN;
#endif
}

#if SC_COLLECT_AGE
void SocialClubMgr::SocialClubInfo::SetAge(u8 day, u8 month, u16 year)
{
	m_Day = day;
	m_Month = month;
	m_Year = year;
	formatf(m_BirthDate, RLSC_MAX_DATE_CHARS,"%d/%d/%d",(u32)m_Month,(u32)m_Day,(u32)m_Year);
}
#else
void SocialClubMgr::SocialClubInfo::SetAge(u8 /*day*/, u8 /*month*/, u16 /*year*/)
{
}
#endif //SC_COLLECT_AGE

bool SocialClubMgr::IsYearValid(u16 year)
{
#if SC_COLLECT_AGE
	time_t curTime_t = time(NULL);
	tm curTime = *localtime(&curTime_t);
	return 1900 < year && ((year-1900) <= curTime.tm_year);
#else
	return year <= 1900;
#endif
}

bool SocialClubMgr::IsMonthValid(u8 month)
{
	return 0 < month && month <= 12;
}

bool SocialClubMgr::IsDayValid(u8 day)
{
	return 0 < day && day <= 31;
}

bool SocialClubMgr::IsDayMonthValid(u8 day, u8 month)
{
	if(month==2 && day>29)
	{
		return false;
	}
	//EARLY EVEN MONTHS OR LATE ODD MONTHS? ensure shorter days
	else if((month%2==0)^(month>7) )
	{
		if(day>30)		
			return false;
	}

	return true;
}

bool SocialClubMgr::IsDateValid(u8 day, u8 month, u16 year)
{
	if(!IsDayValid(day) || !IsMonthValid(month) || !IsYearValid(year))
	{
		return false;
	}
	else if(!IsDayMonthValid(day, month))
	{
		return false;
	}
	// Handle Feb and Leap year
	else if(month==2 && day==29)
	{
		if((year - 2012) % 4)
		{
			return false;
		}
	}

	return 0 <= CalculateAgeFromDob(day, month, year);
}

void SocialClubMgr::SocialClubInfo::SetEmail(const char* pEmail)
{
	gnetAssert(StringLength(pEmail) < RLSC_MAX_EMAIL_CHARS);
	safecpy(m_Email, pEmail);
}

void SocialClubMgr::SocialClubInfo::SetPassword(const char* pPassword)
{
	gnetAssert(StringLength(pPassword) < RLSC_MAX_PASSWORD_CHARS);
	safecpy(m_Password, pPassword);
}

void SocialClubMgr::SocialClubInfo::SetNickname(const char* pNickname)
{
	gnetAssert(StringLength(pNickname) < RLSC_MAX_NICKNAME_CHARS);
	safecpy(m_Nickname, pNickname);
}

///////////////////////////////////////////////////////////////////////////////
//  SocialClubMgr
///////////////////////////////////////////////////////////////////////////////
void 
SocialClubMgr::Init()
{

}

void 
SocialClubMgr::Shutdown()
{
	if (m_ScAuth.IsInitialized())
	{
		m_ScAuth.Shutdown();
	}
}

void  
SocialClubMgr::Update(const u32 UNUSED_PARAM(timeStep))
{
	BANK_ONLY(Bank_Update();)

	UpdatePasswordRequirements();

	if (IsIdle())
		return;

	// While the SC Auth component is initialized, is must be updated each frame.
	if (m_ScAuth.IsInitialized())
	{
		m_ScAuth.Update();
	}

	if (!IsPending())
	{
		gnetDebug1("Finished Operation \"%s\"", GetOperationName(m_Operation));

		if (Succeeded())
		{
			ProcessOperationSucceeded();
		}
		else if (Failed())
		{
			ProcessOperationFailed();
		}
		else if (Canceled())
		{
			ProcessOperationCanceled();
		}

		m_onFinishedCallback.Call(&m_MyStatus);

		if (m_ScAuth.IsInitialized())
		{
			m_ScAuth.Shutdown();
		}

		m_Operation = OP_NONE;
		m_MyStatus.Reset();
	}
}

bool 
SocialClubMgr::CreateAccount(const char* countryCode,
							 const char* languageCode,
							 datCallback onFinishedCallback /*= NullCallback*/)
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	m_onFinishedCallback = onFinishedCallback;

	if (gnetVerify(countryCode) && gnetVerify(strlen(countryCode) < MAX_SOCIAL_CLUB_COUNTRY_LEN)
		&& gnetVerify(languageCode) && gnetVerify(strlen(languageCode) < MAX_SOCIAL_CLUB_LANG_LEN)
		&& gnetVerify(strlen(m_SocialClubInfo.GetEmail()) < RLSC_MAX_EMAIL_CHARS)
		&& gnetVerify(strlen(m_SocialClubInfo.GetNickname()) < RLSC_MAX_NICKNAME_CHARS)
		&& gnetVerify(strlen(m_SocialClubInfo.GetPassword()) < RLSC_MAX_PASSWORD_CHARS))
	{
		if (gnetVerify(OP_NONE == m_Operation) 
			&& gnetVerify(IsIdle()))
		{
			m_Operation = OP_CREATE_ACCOUNT;

			gnetDisplay("SocialClubMgr::CreateAccount - creating account with %s", m_SocialClubInfo.GetEmail());

			return AssertVerify(rlSocialClub::CreateAccount(
				localGamerIndex,
				m_SocialClubInfo.m_bSpam, 
				NULL,//avatarUrl
				countryCode,
				m_SocialClubInfo.m_BirthDate,
				m_SocialClubInfo.m_Email,
				false,
				languageCode,
				m_SocialClubInfo.m_Nickname,
				m_SocialClubInfo.m_Password,
				NULL,//phone
				NULL,//zipCode
				&m_MyStatus
				));
		}
	}

	return false;
}

bool SocialClubMgr::RetrievePasswordRequirements()
{
	if (gnetVerify(OP_NONE == m_Operation) && gnetVerify(IsIdle()))
	{
		m_Operation = OP_GET_PASSWORD_REQS;

		gnetDisplay("SocialClubMgr::RetrievePasswordRequirements - Begin");
		return AssertVerify(rlSocialClub::GetPasswordRequirements( NetworkInterface::GetLocalGamerIndex(), &m_PasswordRequirements, &m_MyStatus));
	}

	return false;
}

bool SocialClubMgr::LinkAccountScAuth(rlScAuth::AuthMode mode, datCallback onFinishedCallback /*= NullCallback*/)
{
	rtry
	{
		// Setup the callback immediately, it is required for failure handling.
		m_onFinishedCallback = onFinishedCallback;

		// No operation should be in progress
		rverify(OP_NONE == m_Operation, catchall, gnetError("A social club operation is already in progress"));

		// We have to be online with our platform
		rverifyall(NetworkInterface::IsLocalPlayerOnline(false));

		// We shouldn't already be connected to social club
		rcheck(!IsConnectedToSocialClub(), alreadyLinked, gnetDebug1("Already linked to Social Club"));

		// An existing operation should not be in progress
		rverify(!m_ScAuth.IsInitialized(), catchall, gnetError("Sc Auth operation already in progress"));

		// The game should have a valid local gamer index
		const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		rverifyall(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex));

		// Verify that auth initializes correctly.
		rverifyall(m_ScAuth.Init(localGamerIndex, mode, &m_MyStatus));

		// Setup the finished callback and begin auth
		m_Operation = OP_LINK_ACCOUNT;
		return true;
	}
	rcatch(alreadyLinked)
	{
		m_MyStatus.ForceSucceeded();
		m_Operation = OP_LINK_ACCOUNT;
		return true;
	}
	rcatchall
	{
		return false;
	}
}

bool 
SocialClubMgr::LinkAccount(datCallback onFinishedCallback /*= NullCallback*/)
{
	m_onFinishedCallback = onFinishedCallback;

	if	(	gnetVerify(strlen(m_SocialClubInfo.GetEmail()) < RLSC_MAX_EMAIL_CHARS)
		&&	gnetVerify(strlen(m_SocialClubInfo.GetNickname()) < RLSC_MAX_NICKNAME_CHARS)
		&&	gnetVerify(strlen(m_SocialClubInfo.GetPassword()) < RLSC_MAX_PASSWORD_CHARS) )
	{
		if (gnetVerify(OP_NONE == m_Operation) 
			&& gnetVerify(IsIdle()))
		{
#if RLROS_SC_PLATFORM
			return false;
#else
			gnetDisplay("SocialClubMgr::LinkAccount - linking account with %s", m_SocialClubInfo.GetEmail());

			const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
			m_Operation = OP_LINK_ACCOUNT;
			return AssertVerify(rlSocialClub::LinkAccount(localGamerIndex, 
				m_SocialClubInfo.m_Email, m_SocialClubInfo.m_Nickname, m_SocialClubInfo.m_Password, &m_MyStatus));
#endif
		}
	}

	return false;
}

void 
SocialClubMgr::CancelTask()
{
	if (m_ScAuth.IsInitialized())
	{
		m_ScAuth.Shutdown();
		m_MyStatus.SetCanceled();
	}
	else
	{
		if (m_MyStatus.Pending())
			rlSocialClub::Cancel(&m_MyStatus);
	}
}

#if __BANK
bool
SocialClubMgr::UnlinkAccount(datCallback onFinishedCallback /*= NullCallback*/)
{
	m_onFinishedCallback = onFinishedCallback;

#if RLROS_SC_PLATFORM
	return false;
#else
	m_Operation = OP_UNLINK_ACCOUNT;
	return AssertVerify(rlSocialClub::UnlinkAccount(NetworkInterface::GetLocalGamerIndex(), &m_MyStatus));
#endif
}
#endif

const rlScAccountInfo* 
SocialClubMgr::GetLocalGamerAccountInfo() const
{
	int gamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (RL_IS_VALID_LOCAL_GAMER_INDEX(gamerIndex))
	{
		return &rlRos::GetCredentials(gamerIndex).GetRockstarAccount();
	}

	return NULL;

}

bool SocialClubMgr::IsLocalGamerAccountInfoValid() const
{
	int gamerIndex = NetworkInterface::GetLocalGamerIndex();
	return RL_IS_VALID_LOCAL_GAMER_INDEX(gamerIndex) && rlRos::GetCredentials(gamerIndex).IsValid();
}

bool 
SocialClubMgr::IsConnectedToSocialClub() const
{
	return (NetworkInterface::HasValidRosCredentials() && NetworkInterface::HasValidRockstarId());
}

bool SocialClubMgr::IsOnlinePolicyUpToDate() const
{
#if __BANK
//	Displayf("SocialClubMgr::IsOnlinePolicyUpToDate - m_isOnlinePolicyUpToDate=%d, IsConnectedToSocialClub=%d, IsInstantiated()=%d, IsDownloading=%d, HasError=%d, IsUpToDate=%d", 
//		m_isOnlinePolicyUpToDate,
//		IsConnectedToSocialClub(),
//		SPolicyVersions::IsInstantiated(),
//		SPolicyVersions::IsInstantiated() && SPolicyVersions::GetInstance().IsDownloading(),
//		SPolicyVersions::IsInstantiated() && (SPolicyVersions::GetInstance().GetError() != 0),
//		SPolicyVersions::IsInstantiated() && SPolicyVersions::GetInstance().IsUpToDate());

	if(!m_isOnlinePolicyUpToDate)
	{
		return false;
	}
#endif

	// If we're downloading, lets assume that we are up to date.  If it is out of date, then it'll be caught in a future check, so this will fix itself.
	return !IsConnectedToSocialClub() || !SPolicyVersions::IsInstantiated() || SPolicyVersions::GetInstance().IsDownloading() || SPolicyVersions::GetInstance().GetError() || SPolicyVersions::GetInstance().IsUpToDate();
}

const char*  
SocialClubMgr::GetOperationName(const int operation) const
{
	#define OP_CASE(x) case x: return #x;
	switch (operation)
	{
		OP_CASE(OP_CREATE_ACCOUNT);
		OP_CASE(OP_LINK_ACCOUNT);
		OP_CASE(OP_GET_ACCOUNT_INFO);
		OP_CASE(OP_UNLINK_ACCOUNT);
		OP_CASE(OP_GET_PASSWORD_REQS);
	default:
		return "OP_UNKNOWN";
	}
}

SocialClubMgr::SocialClubMgr()
{
	// Full-screen account linking on PS4
	m_ScAuth.SetUseFullscreen(true);
	m_ScAuth.SetUseSafeRegion(true);
	m_ScAuth.SetHeaderEnabled(false);
	m_ScAuth.SetFooterEnabled(false);

	Clear();
}

void  
SocialClubMgr::Clear()
{
	m_Operation = OP_NONE;
	m_MyStatus.Reset();
	m_bLocalInitialLink = false;
	m_LastPasswordRequirementCheck = 0;
	m_PasswordRequirements.Clear();
	BANK_ONLY(m_isOnlinePolicyUpToDate = true;)
}

void  
SocialClubMgr::ProcessOperationSucceeded()
{
	gnetDebug1(".... \"%s\" Succeeded.", GetOperationName(m_Operation));

	switch (m_Operation)
	{
	case OP_GET_ACCOUNT_INFO:
		break;
	case OP_CREATE_ACCOUNT:
		break;
	case OP_LINK_ACCOUNT:

		//Now that we are manually linked - set a boolean that is used when clan data is synced - if after the clan data is synced the player has a clan and a manual link was just performed then need to go to character selection (lavalley)
		gnetDebug1("SocialClubMgr::ProcessOperationSucceeded--OP_LINK_ACCOUNT");
		m_bLocalInitialLink = true;

		// Linked social club accout - set stat
		//	SETTING_OPTION(ROS_SC_REGISTERED_IN_GAME, 0x00a6c4f78)
		//KeyedVariable* linkedInGame = SettingManager::GetInstance().Add(ATSTRINGHASH("ROS_SC_REGISTERED_IN_GAME", 0x00a6c4f78),KEYED_VARIABLE_TYPE_INT);
		//if(linkedInGame)
		//{
		//	linkedInGame->SetFile(false);
		//	linkedInGame->SetROS();
		//	SettingInterface::SetInt(1, ATSTRINGHASH("ROS_SC_REGISTERED_IN_GAME", 0x00a6c4f78));
		//}
		//// We're now linked, set this stat as well
		//SettingInterface::SetInt(1, Settings::ROS_SC_IS_LINKED);

		GetEventScriptNetworkGroup()->Add(CEventNetworkSocialClubAccountLinked());

		break;
	case OP_GET_PASSWORD_REQS:
		break;
	case OP_UNLINK_ACCOUNT:
		break;
	}
}
void  
SocialClubMgr::ProcessOperationFailed()
{
	gnetWarning(".... \"%s\" Failed.", GetOperationName(m_Operation));
	gnetWarning(".... Reason: \"%s\"", GetErrorName());

	switch (m_Operation)
	{
	case OP_GET_ACCOUNT_INFO:
		break;
	case OP_CREATE_ACCOUNT:
	case OP_LINK_ACCOUNT:	
		break;
	case OP_UNLINK_ACCOUNT:
		break;
	case OP_GET_PASSWORD_REQS:
		break;
	}
}

void  
SocialClubMgr::ProcessOperationCanceled()
{
	gnetWarning(".... \"%s\" Canceled.", GetOperationName(m_Operation));

	switch (m_Operation)
	{
	case OP_GET_ACCOUNT_INFO:
	case OP_CREATE_ACCOUNT:  
	case OP_LINK_ACCOUNT: 
	case OP_UNLINK_ACCOUNT:
	case OP_GET_PASSWORD_REQS:
		break;
	}
}

void
SocialClubMgr::UpdatePasswordRequirements()
{
	// we have valid password requirements, early out
	if (m_PasswordRequirements.bValid)
		return;

	// if no valid ROS creds, early out
	if (!NetworkInterface::HasValidRosCredentials())
		return;

	// a task is in progress, early out
	if (!IsIdle())
		return;

	// 5 min interval between failed password requirement checks
	const int PASSWORD_REQUIREMENT_RETRY_TIMER (5 * 60 * 1000);
	if (m_LastPasswordRequirementCheck == 0 || sysTimer::HasElapsedIntervalMs(m_LastPasswordRequirementCheck, PASSWORD_REQUIREMENT_RETRY_TIMER))
	{
		RetrievePasswordRequirements();
		m_LastPasswordRequirementCheck = sysTimer::GetSystemMsTime();
	}
}

const char*
SocialClubMgr::GetErrorName(const netStatus& erroredStatus)
{
	return GetErrorName(erroredStatus.GetResultCode());
}

const char*
SocialClubMgr::GetErrorName(int resultCode)
{
	switch (resultCode)
	{
	case RLSC_ERROR_UNEXPECTED_RESULT			: return "SOCIAL_CLUB_UNKNOWN";
	case RLSC_ERROR_NONE                        : break;//Success    

		//32  X----X----X----X----X----X----X| TMS: | marks the NULL terminator in a 32 char string.
	case RLSC_ERROR_PLATFORM_PRIVILEGE			: return "SC_ERROR_ONLINEPRIVILEGES";    
	case RLSC_ERROR_ALREADYEXISTS_EMAILORNICKNAME:return "SC_ERROR_EXISTS_EMAILNICKNAME";
	case RLSC_ERROR_ALREADYEXISTS_EMAIL			: return "SC_ERROR_ALREADYEXISTS_EMAIL"; 
	case RLSC_ERROR_ALREADYEXISTS_NICKNAME	   	: return "SC_ERROR_ALREADYEXISTS_NICK";  

		//32  X----X----X----X----X----X----X| TMS: | marks the NULL terminator in a 32 char string.
	case RLSC_ERROR_AUTHENTICATIONFAILED_PASSWORD:return "SC_ERROR_AUTH_FAILED_PW";
	case RLSC_ERROR_AUTHENTICATIONFAILED_TICKET	: return "SC_ERROR_AUTH_FAILED_TICKET";
	case RLSC_ERROR_AUTHENTICATIONFAILED_LOGINATTEMPTS: return "SC_ERROR_AUTH_FAILED_LA";
	case RLSC_ERROR_AUTHENTICATIONFAILED_MFA : return "SC_ERROR_AUTH_FAILED_MFA";

		//32  X----X----X----X----X----X----X|
	case RLSC_ERROR_DOESNOTEXIST_PLAYERACCOUNT	: return "SC_ERROR_EXIST_PLAYER_ACCT";
	case RLSC_ERROR_DOESNOTEXIST_ROCKSTARACCOUNT: return "SC_ERROR_EXIST_ROCKSTAR_ACCT";   //No SC account (aka RockstarAccount)

		//32  X----X----X----X----X----X----X| TMS: | marks the NULL terminator in a 32 char string.
	case RLSC_ERROR_INVALIDARGUMENT_COUNTRYCODE	: return "SC_ERROR_INVALID_COUNTRY";
	case RLSC_ERROR_INVALIDARGUMENT_EMAIL		: return "SC_ERROR_INVALID_EMAIL";
	case RLSC_ERROR_INVALIDARGUMENT_NICKNAME	: return "SC_ERROR_INVALID_NICKNAME";
	case RLSC_ERROR_INVALIDARGUMENT_PASSWORD	: return "SC_ERROR_INVALID_PASSWORD";
	case RLSC_ERROR_INVALIDARGUMENT_ZIPCODE		: return "SC_ERROR_INVALID_ZIPCODE";
	case RLSC_ERROR_INVALIDARGUMENT_ROCKSTARID	: return "SC_ERROR_INVALID_ROCKSTARID";

		//32  X----X----X----X----X----X----X| TMS: | marks the NULL terminator in a 32 char string.
	case RLSC_ERROR_OUTOFRANGE_AGE				: return "SC_ERROR_OUTOFRANGE_AGE";
	case RLSC_ERROR_OUTOFRANGE_AGELOCALE		: return "SC_ERROR_OUTOFRANGE_AGELOCALE";
	case RLSC_ERROR_OUTOFRANGE_NICKNAMELENGTH	: return "SC_ERROR_OUTOFRANGE_NAMELEN";	
	case RLSC_ERROR_OUTOFRANGE_PASSWORDLENGTH	: return "SC_ERROR_OUTOFRANGE_PWLEN";
	case RLSC_ERROR_PROFANE_NICKNAME			: return "SC_ERROR_PROFANE_NICKNAME";

	case RLSC_ERROR_DOESNOTEXIST_ACHIEVEMENT			: return "SC_ERROR_INVALID_ACHIEVEMENT";
	case RLSC_ERROR_INVALIDARGUMENT_AVATARURL			: return "SC_ERROR_INVALID_AVATARURL";
	case RLSC_ERROR_INVALIDARGUMENT_DOB					: return "SC_ERROR_INVALID_DOB";
	case RLSC_ERROR_INVALIDARGUMENT_PHONE				: return "SC_ERROR_INVALID_PHONE";
	case RLSC_ERROR_NOTALLOWED_MAXEMAILSREACHED			: return "SC_ERROR_MAX_MAIL";
	case RLSC_ERROR_SYSTEMERROR_SENDMAIL				: return "SC_ERROR_SEND_MAIL";
	case RLSC_ERROR_PROFANE_TEXT						: return "RLSC_ERROR_PROFANE_TEXT";
	case RLSC_ERROR_NOTALLOWED_PRIVACY					: return "SC_ERROR_PRIV";
	}

	return "SOCIAL_CLUB_UNKNOWN";
}

#if !__FINAL
void 
SocialClubMgr::GetDefaultDevNames( char* szEmail, char* szNick )
{
	char localUserName[64];
	u64 posixTime = (rlGetPosixTime() << 32);
	int timeStamp = (*((int*)(&posixTime))) % 9999;

	const rlGamerInfo* pGamerInfo = NetworkInterface::GetActiveGamerInfo();
	formatf(localUserName, "%s", pGamerInfo ? pGamerInfo->GetName() : "none");
	sysGetEnv("USERNAME",localUserName,sizeof(localUserName));

	formatf(szEmail, RLSC_MAX_EMAIL_CHARS, "%s_%4.4d.gta5@rockfoo.com", localUserName, timeStamp);
	formatf(szNick, RLSC_MAX_NICKNAME_CHARS, "%s_%4.4d", localUserName, timeStamp );
}

void
SocialClubMgr::Debug_AutofillAccountInfo()
{
	SocialClubInfo& rSocialClubInfo = CLiveManager::GetSocialClubMgr().GetSocialClubInfo();

	sysMemSet(rSocialClubInfo.m_Email, 0, RLSC_MAX_EMAIL_CHARS);
	sysMemSet(rSocialClubInfo.m_Nickname, 0, RLSC_MAX_NICKNAME_CHARS);
	GetDefaultDevNames(rSocialClubInfo.m_Email, rSocialClubInfo.m_Nickname );

	formatf(rSocialClubInfo.m_BirthDate, "7/28/1980");
	formatf(rSocialClubInfo.m_Password, RLSC_MAX_PASSWORD_CHARS, "Password1");
}
#endif

#if __BANK
void  
SocialClubMgr::InitWidgets(bkBank* bank)
{
	SocialClubInfo& rSocialClubInfo = CLiveManager::GetSocialClubMgr().GetSocialClubInfo();

	m_pBankAccountStatusWidget = NULL;
	formatf(m_BankSCAcountStatus, "NOT VALID");

	bank->PushGroup("Social Club");
	m_pBankAccountStatusWidget = bank->AddText("Current Account Status:",m_BankSCAcountStatus, NELEM(m_BankSCAcountStatus));
	bank->PushGroup("Account Creation");
	bank->AddButton("Autofill", datCallback(MFA(SocialClubMgr::Debug_AutofillAccountInfo), (datBase*)this));
	bank->AddText( "E-Mail", rSocialClubInfo.m_Email, RLSC_MAX_EMAIL_CHARS);
	bank->AddText( "Nickname", rSocialClubInfo.m_Nickname, RLSC_MAX_NICKNAME_CHARS);
	bank->AddText( "Password", rSocialClubInfo.m_Password, RLSC_MAX_PASSWORD_CHARS);
	bank->AddButton("Create Account", datCallback(MFA(SocialClubMgr::Bank_CreateAccount), (datBase*)this));
	bank->AddButton("Unlink Account", datCallback(MFA(SocialClubMgr::Bank_UnlinkAccount), (datBase*)this));
	bank->AddToggle("Is Online Policy Up To Date?", &m_isOnlinePolicyUpToDate);
	bank->PopGroup();

	PolicyVersions::InitWidgets(bank);

	bank->PopGroup();

}

void  
SocialClubMgr::Bank_Update()
{
	if(m_pBankAccountStatusWidget)
	{
		if (IsConnectedToSocialClub())
		{
			if(IsLocalGamerAccountInfoValid())
			{
				const rlScAccountInfo* pInfo = GetLocalGamerAccountInfo();
				formatf(m_BankSCAcountStatus, "CONNECTED: %s %s", pInfo->m_Email, pInfo->m_Nickname);
			}
			else
			{
				formatf(m_BankSCAcountStatus, "CONNECTED: Info Invalid");
			}

			m_pBankAccountStatusWidget->SetFillColor("NamedColor:Green");	
		}
		else if(m_MyStatus.Failed())
		{
			formatf(m_BankSCAcountStatus, "Error: %s", GetErrorName());
			m_pBankAccountStatusWidget->SetFillColor("NamedColor:Red");
		}
		else if(m_MyStatus.Pending())
		{
			formatf(m_BankSCAcountStatus, "Pending");
			m_pBankAccountStatusWidget->SetFillColor("NamedColor:Yellow");
		}
	}
}

void  
SocialClubMgr::Bank_CreateAccount()
{
	CLiveManager::GetSocialClubMgr().CreateAccount("US", "EN");
}

void
SocialClubMgr::Bank_UnlinkAccount()
{
	gnetVerifyf(CLiveManager::GetSocialClubMgr().UnlinkAccount(), "SocialClubMgr::Bank_UnlinkAccount - Failed to unlink the account.");
}

#endif // __BANK

// eof

