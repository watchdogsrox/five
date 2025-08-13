//
// SocialClubVersionedFile.h
//
// Copyright (C) 2012 Rockstar Games.  All Rights Reserved.
//

#ifndef SOCIALCLUB_VERSIONED_FILE_H
#define SOCIALCLUB_VERSIONED_FILE_H

// rage
#include "fwnet/netCloudFileDownloadHelper.h"
#include "system/timer.h"

// game
#include "Frontend/ProfileSettings.h"
#include "Text/PagedText.h"

class SocialClubVersionedFile
{
	typedef CProfileSettings::ProfileSettingId SettingId;

	enum eStates
	{
		STATE_IDLE,
		STATE_WAITING_ON_OFFICIAL_VERSION,
		STATE_WAITING_FOR_ACCEPTED_CLOUD_VERSION,
		STATE_WAITING_FOR_POLICY_ACCEPTANCE,
		STATE_UP_TO_DATE,
		STATE_ERROR
	};

public:
	SocialClubVersionedFile();
	~SocialClubVersionedFile();

	void PreInit(const char* pPolicyTag, SettingId settingId, u32 statHash);
	void Init();
	void Shutdown();
	void Update();

	bool IsInitialized() const {return m_state != STATE_IDLE;}
	bool IsWaitingForAcceptance() const {return m_state == STATE_WAITING_FOR_POLICY_ACCEPTANCE;}
	bool IsUpToDate() const {return m_state == STATE_UP_TO_DATE;}
	bool IsDownloading() const {return m_state < STATE_WAITING_FOR_POLICY_ACCEPTANCE;}
	bool IsError() const {return m_state == STATE_ERROR;}
	
	void ForceWaitingForAcceptance() {m_state = STATE_WAITING_FOR_POLICY_ACCEPTANCE;}

	const char* GetError() const;

	// The cloud version now has precedence but we can fall back to the local file if needed.
	bool NeedsCloudVersion() const { return true; }
	void SetOfficialCloudVersion(float officialCloudVersion) {m_officialCloudVersion = officialCloudVersion;}
	void SetOfficialLocalVersion(s16 officialLocalVersion) {m_officialLocalVersion = officialLocalVersion;}
	float GetOfficialVersion() const {return (m_officialCloudVersion < m_officialLocalVersion) ? m_officialLocalVersion : m_officialCloudVersion;}
	void AcceptVersion(const bool isCloudVersion);

	bool HasEverAccepted() const;

	static const s16 sInvalidOfficialVersion;
	static const s16 sNotFoundOfficialVersion;

private:

	void UpdateWaitingOnOfficialVersion();
	void UpdateWaitingForAcceptedCloudVersion();
	void UpdateWaitingForPolicyAcceptance();

	void GoToWaitingOnOfficialVersion();
	void GoToWaitingForAcceptedCloudVersion();
	void GoToWaitingForPolicyAcceptance();
	void GoToComplete();
	void GoToError();

	s16 GetAcceptedVersion(bool* pOutStatsValid = NULL) const;

	static const s16 sInvalidAcceptedVersion;

	bool							m_areStatsValid;
	eStates							m_state;

	float							m_officialCloudVersion;
	s16								m_officialLocalVersion;
	s16								m_acceptedCloudVersion;
	s16								m_acceptedLocalVersion;
	netStatus						m_rlscNetStatus;
	netStatus						m_rlscNetFFStatus; // We dont use the status above for the rlSocialClub::AcceptLegalPolicy task as it's "Fire and forget"

	const char*						m_pPolicyTag;
	SettingId						m_settingId;
	u32								m_statHash;
	PagedCloudText					m_cloudPolicy;
	sysTimer						m_InvalidStatReadTimer;
};

#endif  // SOCIALCLUB_VERSIONED_FILE_H

