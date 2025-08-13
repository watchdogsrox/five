//
// SocialClubPolicyManager.h
//
// Copyright (C) 2012 Rockstar Games.  All Rights Reserved.
//

#ifndef SOCIALCLUB_POLICY_MANAGER_H
#define SOCIALCLUB_POLICY_MANAGER_H

// rage
#include "rline/ros/rlros.h"
#include "rline/ros/rlroscommon.h"

// game
#include "network/SocialClubUpdates/SocialClubVersionedFile.h"

namespace rage
{
	class bkBank;
};

enum eSocialClubFile
{
	SCF_EULA,
	SCF_TOS,
	SCF_PP,
	SCF_MAX, // The Max for the versioned legal docs.
	SCF_VERSION_NUM = SCF_MAX,
	SCF_POLICY_CHANGE,
	SCF_TOTAL_FILE_COUNT
};

class PolicyVersions // Tracks the versioning.
{
public:
	PolicyVersions();
	~PolicyVersions();

	void Init();
	void Shutdown();
	bool IsInitialized() const {return m_bInitialized;}

	void Update();
	void OnVersionFileDownloaded();
	SocialClubVersionedFile& GetVersionFile(int index) {return m_SCVersionedFiles[index];}
	const SocialClubVersionedFile& GetVersionFile(int index) const {return m_SCVersionedFiles[index];}

	bool IsUpToDate() const;
	bool IsDownloading() const;
	bool IsWaitingForAcceptance() const;
	const char* GetError() const;

	bool HasEverAccepted() const;

	void AcceptPolicy(const bool cloudVersion);
	eSocialClubFile GetNextFileThatNeedsAcceptance() const;
	void ForceWaitingForAcceptance();

#if !__NO_OUTPUT
	static const char* GetSocialClubFileAsString(const eSocialClubFile socialClubFile);
#endif

#if __BANK
	static void InitWidgets(rage::bkBank* pBank);
#endif

private:

	void ParseFile(const char* pFilename, bool fromCloud);

	static const s16 sInvalidAcceptedVersion;
	static const s16 sInvalidOfficialVersion;
	static const s16 sNotFoundOfficialVersion;
	static const char* sSC_VersionFilename;

	netCloudRequestGetTitleFile m_versionFile;
	SocialClubVersionedFile m_SCVersionedFiles[SCF_MAX];
	CSystemTimer m_retryTimer;

	bool m_bInitialized;
	bool m_bHasCloudVersion;
	u8 m_attempts;
};

typedef atSingleton<PolicyVersions> SPolicyVersions;



//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
class SocialClubPolicyManager
{
public:
	SocialClubPolicyManager() {Shutdown();}
	~SocialClubPolicyManager() {Shutdown();}

	void Init(int gamerIndex, const char* pLangCode);
	void Shutdown();
	void Update();

	bool IsInitialised() const {return m_gamerIndex != -1;}
	bool IsDownloading() const;

	// Returns true if all texts have been downloaded from the cloud
	bool AreTextsInSyncWithCloud() const;

	const char* GetError() const;
	const char* GetErrorTitle() const;

	PagedCloudText& GetFile(eSocialClubFile file) {return m_cloudPolicy[file];}
	const PagedCloudText& GetFile(eSocialClubFile file) const {return m_cloudPolicy[file];}
	const PagedCloudText& GetPolicyChangeFile() const {return m_policyChangeFile;}

private:

	int m_gamerIndex;
	const char* m_pLangCode;
	PagedCloudText m_cloudPolicy[SCF_MAX];
	PagedCloudText m_policyChangeFile;
};

#endif  // SOCIALCLUB_POLICY_MANAGER_H

