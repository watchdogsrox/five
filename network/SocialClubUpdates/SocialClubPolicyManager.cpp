//
// SocialClubPolicyManager.cpp
//
// Copyright (C) 2012 Rockstar Games.  All Rights Reserved.
//

// rage
#include "bank/bank.h"
#include "file/device.h"
#include "fwnet/netchannel.h"
#include "parser/macros.h"
#include "parser/manager.h"
#include "rline/rl.h"
#include "rline/ros/rlros.h"
#include "system/nelem.h"

// game
#include "Network/SocialClubUpdates/SocialClubPolicyManager.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Live/Livemanager.h"
#include "Network/Live/SocialClubMgr.h"

NETWORK_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

RAGE_DEFINE_SUBCHANNEL(net, sc, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_sc


#define PREALLOCATE_VERSION_BUFFER (10*1024)

#if !__FINAL
PARAM(scLegalFileMissing, "simulates missing legal file");
PARAM(scFakeEulaVersion, "Fakes the eula version to test the eula being out of date.");
PARAM(scFakePPVersion, "Fakes the PP version to test the eula being out of date.");
PARAM(scFakeTOSVersion, "Fakes the TOS version to test the eula being out of date.");
PARAM(scFakeLocalEulaVersion, "Fakes the local eula version to test the eula being out of date.");
PARAM(scFakeLocalPPVersion, "Fakes the local PP version to test the eula being out of date.");
PARAM(scFakeLocalTOSVersion, "Fakes the local TOS version to test the eula being out of date.");
#endif

// As long as the accepted version < the current version, we need to update.
const s16	PolicyVersions::sInvalidAcceptedVersion	= -1;
const s16	PolicyVersions::sInvalidOfficialVersion	= 0x7FFF;
const s16	PolicyVersions::sNotFoundOfficialVersion = 1;
const char* PolicyVersions::sSC_VersionFilename		= "legal/version/version_num.xml";

static const char * sSC_PolicyTags[] =
{
	"eula",
	"tos",
	"pp",
	"version_num",
	"policy_change"
};

static CProfileSettings::ProfileSettingId sSC_PolicyProfileIndexs[] =
{
	CProfileSettings::EULA_VERSION,
	CProfileSettings::TOS_VERSION,
	CProfileSettings::PRIVACY_VERSION,
};

static u32 sSC_PolicyStatHashes[] =
{
	ATSTRINGHASH("ACCEPTED_EULA_VERSION",0xf10c586),
	ATSTRINGHASH("ACCEPTED_TOS_VERSION",0x68b862e2),
	ATSTRINGHASH("ACCEPTED_PP_VERSION",0xf03d6ae5),
};

CompileTimeAssert(NELEM(sSC_PolicyTags) ==  SCF_TOTAL_FILE_COUNT);
CompileTimeAssert(NELEM(sSC_PolicyProfileIndexs) ==  SCF_MAX);
CompileTimeAssert(NELEM(sSC_PolicyStatHashes) ==  SCF_MAX);

static bank_u8 sSC_DebugForceDownloadFails[SCF_TOTAL_FILE_COUNT] = {0};

PolicyVersions::PolicyVersions() :
	m_bInitialized(false)
{
	for(int i=0; i<SCF_MAX; ++i)
	{
		m_SCVersionedFiles[i].PreInit(sSC_PolicyTags[i], sSC_PolicyProfileIndexs[i], sSC_PolicyStatHashes[i]);
	}
}

PolicyVersions::~PolicyVersions()
{
	Shutdown();
}

void PolicyVersions::Init()
{
	Shutdown(); //To Clear everything out
	uiDisplayf("PolicyVersions::Init");

	for(int i=0; i<SCF_MAX; ++i)
	{
		m_SCVersionedFiles[i].Init();
		m_SCVersionedFiles[i].SetOfficialLocalVersion(SocialClubVersionedFile::sNotFoundOfficialVersion);
	}

	m_bInitialized = true;

	atString localFilename("platform:/data/");
	localFilename += sSC_VersionFilename;

	ParseFile(localFilename.c_str(), false);
}

void PolicyVersions::Shutdown()
{
	uiDisplayf("PolicyVersions::Shutdown");

	m_attempts = 0;
	m_retryTimer.Zero();
	m_bInitialized = false;
	m_bHasCloudVersion = false;

	for(int i=0; i<SCF_MAX; ++i)
	{
		m_SCVersionedFiles[i].Shutdown();
	}

	m_versionFile.Cancel();
	m_versionFile.Reset();
}

void PolicyVersions::Update()
{
	if(m_bInitialized)
	{
		if(!m_bHasCloudVersion)
		{
			if(m_versionFile.IsIdle() &&
				(m_retryTimer.IsComplete(PagedCloudText::sm_maxTimeoutRetryDelay, false) || !m_retryTimer.IsStarted()))
			{
				uiDisplayf("PolicyVersions::Update - Policy Version file isn't downloaded.");
				m_versionFile.Init(sSC_VersionFilename, 0, NET_HTTP_OPTIONS_NONE, PREALLOCATE_VERSION_BUFFER, datCallback(MFA(PolicyVersions::OnVersionFileDownloaded), (datBase*)this), netCloudRequestMemPolicy::NULL_POLICY);
				m_versionFile.Start();
			}
		}

		m_versionFile.Update();

		for(int i=0; i<SCF_MAX; ++i)
		{
			m_SCVersionedFiles[i].Update();
		}
	}
}

void PolicyVersions::OnVersionFileDownloaded()
{
	if(m_bInitialized)
	{
		bool forceTimeout = false;

#if __BANK
		forceTimeout = m_attempts < sSC_DebugForceDownloadFails[SCF_VERSION_NUM];
#endif

		if(!m_versionFile.DidSucceed() || forceTimeout)
		{
			if((PagedCloudText::IsTimeoutError(static_cast<netHttpStatusCodes>(m_versionFile.GetResultCode())) && m_attempts < PagedCloudText::sm_maxAttempts) ||
				forceTimeout)
			{
				gnetDebug1("PolicyVersions::UpdateVersionFile - Failed to download %s; Download Timeout, resultCode=%d, going to force everything to show.", sSC_VersionFilename, m_versionFile.GetResultCode());
				++m_attempts;
				m_retryTimer.Start();
				m_versionFile.Reset();
			}
			else
			{
				gnetDebug1("PolicyVersions::UpdateVersionFile - Failed to download %s; resultCode=%d, will proceed based off of the local version.", sSC_VersionFilename, m_versionFile.GetResultCode());
				m_versionFile.Reset();
				for (u32 i = 0; i < SCF_MAX; ++i)
				{
					m_SCVersionedFiles[i].SetOfficialCloudVersion(SocialClubVersionedFile::sNotFoundOfficialVersion);
				}
				m_bHasCloudVersion = true; // Make sure we stop acting on stuff.
			}
		}
		else // success
		{
			char bufferFName[64];
			fiDevice::MakeGrowBufferFileName(bufferFName, sizeof(bufferFName), &m_versionFile.GetGrowBuffer());

			for(int i=0; i<SCF_MAX; ++i)
			{
				m_SCVersionedFiles[i].SetOfficialCloudVersion(SocialClubVersionedFile::sNotFoundOfficialVersion);
			}
			ParseFile(bufferFName, true);

			// Make sure we stop acting on stuff.
			m_bHasCloudVersion = true; 
		}

		// reset the file
		m_versionFile.Reset();
	}
#if !__NO_OUTPUT
	else
	{
		gnetDebug3("PolicyVersions::OnVersionFileDownloaded -- Ignoring callback processing because policy versions is not initialized!");
	}
#endif
}

void PolicyVersions::ParseFile(const char* pFilename, bool fromCloud)
{
	//Now load the text file.
	INIT_PARSER;
	{
		parTree* pVersionDataXML = PARSER.LoadTree(pFilename, "");
		if(pVersionDataXML)
		{
			const parTreeNode* pRootNode = pVersionDataXML->GetRoot();
			if(gnetVerify(pRootNode))
			{
				for (u32 i = 0; i < SCF_MAX; ++i)
				{
					float fOfficialVersion = sNotFoundOfficialVersion;

					const parTreeNode* pPolicynode = pRootNode->FindChildWithName(sSC_PolicyTags[i]);
					if (gnetVerifyf(pPolicynode, "PolicyVersions::OnVersionFileDownloaded - Couldn't find the policy %s", pFilename))
					{
						// temp patch for Day0 - the local file has zh-hans in lower-case for Simplified Chinese but this can't be changed at this point
						const bool isCaseSensitive = fromCloud;
						const parTreeNode* pLanguageNode = pPolicynode->FindChildWithName(NetworkUtils::GetLanguageCode(), nullptr, isCaseSensitive);
						if (gnetVerifyf(pLanguageNode, "PolicyVersions::OnVersionFileDownloaded - Couldn't find the language code %s for policy %s", NetworkUtils::GetLanguageCode(), pFilename))
						{
							float versionValue = sInvalidOfficialVersion;
							const char* dataBuffer = pLanguageNode->GetData();
							sscanf(dataBuffer, "%f", &versionValue);
							if(gnetVerifyf(versionValue != sInvalidOfficialVersion, "PolicyVersions::OnVersionFileDownloaded - The file version of %s is the invalid version number %d", pFilename, sInvalidOfficialVersion))
							{
								fOfficialVersion = versionValue;
							}
						}
					}

					if(fromCloud)
					{
#if !__FINAL
						float fHackedOfficialVersion = 0;
						if( (i==SCF_EULA	&& PARAM_scFakeEulaVersion.Get(fHackedOfficialVersion)) || 
							(i==SCF_PP		&& PARAM_scFakePPVersion.Get(fHackedOfficialVersion)) || 
							(i==SCF_TOS		&& PARAM_scFakeTOSVersion.Get(fHackedOfficialVersion)))
						{
							fOfficialVersion = fHackedOfficialVersion;
						}
#endif

						gnetDebug1("PolicyVersions::OnVersionFileDownloaded - file %s has official cloud version of %f", sSC_PolicyTags[i], fOfficialVersion);
						m_SCVersionedFiles[i].SetOfficialCloudVersion(fOfficialVersion);
					}
					else
					{
#if !__FINAL
						float iHackedOfficialVersion = 0;
						if( (i==SCF_EULA	&& PARAM_scFakeLocalEulaVersion.Get(iHackedOfficialVersion)) || 
							(i==SCF_PP		&& PARAM_scFakeLocalPPVersion.Get(iHackedOfficialVersion)) ||
							(i==SCF_TOS		&& PARAM_scFakeLocalTOSVersion.Get(iHackedOfficialVersion)))
						{
							fOfficialVersion = iHackedOfficialVersion;
						}
#endif

						gnetDebug1("PolicyVersions::OnVersionFileDownloaded - file %s has official local version of %d", sSC_PolicyTags[i], (s16)fOfficialVersion);
						m_SCVersionedFiles[i].SetOfficialLocalVersion((s16)fOfficialVersion);
					}
				}
			}

			delete pVersionDataXML;
		}
		else
		{
			gnetDebug1("PolicyVersions::OnVersionFileDownloaded - file %s wasn't found.", pFilename);
		}
	}
	SHUTDOWN_PARSER;
}

bool PolicyVersions::IsDownloading() const
{
	bool bDownloadingFile = false;
	for(int i=0; i<SCF_MAX; ++i)
	{
		if(m_SCVersionedFiles[i].IsDownloading())
		{
			bDownloadingFile = true;
			break;
		}
	}

	// If no versioned file is downloading, set the state to !m_bHasCloudVersion
	if (!bDownloadingFile)
	{
		bDownloadingFile = !m_bHasCloudVersion;
	}

#if !__NO_OUTPUT
	static bool PolicyVersionDownloading = false;
	if (bDownloadingFile != PolicyVersionDownloading)
	{
		gnetDebug3("PolicyVersions::IsDownloading :: state changed to %s", bDownloadingFile ? "TRUE" : "FALSE");
	}
	PolicyVersionDownloading = bDownloadingFile;
#endif //!__NO_OUTPUT

	return bDownloadingFile;
}

bool PolicyVersions::IsUpToDate() const
{
	bool bSuccess = true;

	for(int i=0; i<SCF_MAX; ++i)
	{
		if(!m_SCVersionedFiles[i].IsUpToDate())
		{
			bSuccess = false;
			break;
		}
	}

#if !__NO_OUTPUT
	static bool bWasSuccess = false;
	if (bSuccess != bWasSuccess)
	{
		gnetDebug3("PolicyVersions::IsUpToDate - is now %s", bSuccess ? "TRUE" : "FALSE");
		bWasSuccess = bSuccess;
	}
#endif

	return bSuccess;
}

bool PolicyVersions::IsWaitingForAcceptance() const
{
	for(int i=0; i<SCF_MAX; ++i)
	{
		if(m_SCVersionedFiles[i].IsWaitingForAcceptance())
		{
			return true;
		}
	}

	return false;
}

void PolicyVersions::ForceWaitingForAcceptance()
{
	for(int i=0; i<SCF_MAX; ++i)
	{
		m_SCVersionedFiles[i].ForceWaitingForAcceptance();
	}
}

const char* PolicyVersions::GetError() const
{
	for(int i=0; i<SCF_MAX; ++i)
	{
		if(m_SCVersionedFiles[i].IsError())
		{
			uiDebugf1("GetError :: Error requesting versioned file! File: %s, Error: %s",
				GetSocialClubFileAsString(static_cast<eSocialClubFile>(i)),
				m_SCVersionedFiles[i].GetError());

			return m_SCVersionedFiles[i].GetError();
		}
	}

	return NULL;
}

bool PolicyVersions::HasEverAccepted() const
{
	for(int i=0; i<SCF_MAX; ++i)
	{
		if(m_SCVersionedFiles[i].HasEverAccepted())
		{
			gnetDebug3("PolicyVersions::HasEverAccepted - true");
			return true;
		}
	}

	gnetDebug3("PolicyVersions::HasEverAccepted - false");
	return false;
}

void PolicyVersions::AcceptPolicy(const bool cloudVersion)
{
	for(int i=0; i<SCF_MAX; ++i)
	{
		if(m_SCVersionedFiles[i].IsWaitingForAcceptance())
		{
			m_SCVersionedFiles[i].AcceptVersion(cloudVersion);
		}
	}
}

eSocialClubFile PolicyVersions::GetNextFileThatNeedsAcceptance() const
{
	gnetAssertf(IsWaitingForAcceptance(), "PolicyVersions::GetNextFileThatNeedsAcceptance - This shouldn't be called unless IsWaitingForAcceptance() returns true.");

	for(int i=0; i<SCF_MAX; ++i)
	{
		if(!m_SCVersionedFiles[i].IsUpToDate())
		{
			return static_cast<eSocialClubFile>(i);
		}
	}

	// Might as well return a valid index that won't crash.
	return SCF_PP;
}

#if !__NO_OUTPUT
const char* PolicyVersions::GetSocialClubFileAsString(const eSocialClubFile socialClubFile)
{
	switch(socialClubFile)
	{
	case SCF_EULA: return "SCF_EULA";
	case SCF_TOS: return "SCF_TOS";
	case SCF_PP: return "SCF_PP";
	case SCF_POLICY_CHANGE: return "SCF_POLICY_CHANGE";
	default: return "SCF_INVALID";
	}
}
#endif

#if __BANK
void PolicyVersions::InitWidgets(bkBank* pBank)
{
	if(pBank)
	{
		pBank->PushGroup("Legals");
		for(int i=0; i<SCF_TOTAL_FILE_COUNT; ++i)
		{
			char buffer[100];
			formatf(buffer, sizeof(buffer), "Force Download Timeout %s", sSC_PolicyTags[i]);
			pBank->AddSlider(buffer, &sSC_DebugForceDownloadFails[i], 0, 10, 1);
		}

		pBank->AddSlider("Max Attempts for Timeouts", &PagedCloudText::sm_maxAttempts, 0, 10, 1);
		pBank->AddSlider("Max Timeout Retry Delay", &PagedCloudText::sm_maxTimeoutRetryDelay, 0, 100000, 1);
		pBank->AddSlider("Timeout Retry Delay Variance", &PagedCloudText::sm_timeoutRetryDelayVariance, 0, 100000, 1);

		pBank->PopGroup();
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
void SocialClubPolicyManager::Init(int gamerIndex, const char* pLangCode)
{
	if (IsInitialised())
	{
		gnetAssertf(0, "We've called initialize on SocialClubLegal instance that's alread been initialized.");
		Shutdown();
	}

	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(gamerIndex))
	{
		gnetAssertf(0, "We've tried to initialize our SocialClubLegal with an invalid gamerIndx %d", gamerIndex);
		return;
	}

#if !__FINAL
	//simulate missing file
	u32 iMissingFile;
	if(PARAM_scLegalFileMissing.Get(iMissingFile) && AssertVerify(iMissingFile<SCF_MAX))
		sSC_PolicyTags[iMissingFile] = "JUNK";
#endif

	m_gamerIndex = gamerIndex;
	m_pLangCode = pLangCode;


	bool policyUpdateNeedsCloudVersion = false;

	for(int i=0; i<SCF_MAX; ++i)
	{
		atString policyFilename;

		policyFilename = "legal/";
		policyFilename += sSC_PolicyTags[i];
		policyFilename += "/";
		policyFilename += sSC_PolicyTags[i];
		policyFilename += "_";
		policyFilename += m_pLangCode;
		policyFilename += ".xml";

		bool fileNeedsCloudUpdate = SPolicyVersions::GetInstance().GetVersionFile(i).NeedsCloudVersion();
		policyUpdateNeedsCloudVersion |= fileNeedsCloudUpdate;

		m_cloudPolicy[i].Init(policyFilename, fileNeedsCloudUpdate BANK_ONLY(, sSC_DebugForceDownloadFails[i]));
	}

	
	{
		atString policyFilename;
		policyFilename = "legal/policychange/policy_changed_";
		policyFilename += m_pLangCode;
		policyFilename += ".xml";

		m_policyChangeFile.Init(policyFilename, policyUpdateNeedsCloudVersion BANK_ONLY(, sSC_DebugForceDownloadFails[SCF_POLICY_CHANGE]));
	}
}

void SocialClubPolicyManager::Shutdown()
{
	m_gamerIndex = -1;

	for(int i=0; i<SCF_MAX; ++i)
	{
		m_cloudPolicy[i].Shutdown();
	}

	m_policyChangeFile.Shutdown();
}

void SocialClubPolicyManager::Update()
{
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(m_gamerIndex))
	{
		return;
	}

	for(int i=0; i<SCF_MAX; ++i)
	{
		m_cloudPolicy[i].Update();
	}

	m_policyChangeFile.Update();
}

bool SocialClubPolicyManager::IsDownloading() const
{
	if(SPolicyVersions::GetInstance().GetError())
	{
		return false;
	}

	for(int i=0; i<SCF_MAX; ++i)
	{
		if(m_cloudPolicy[i].IsPending())
		{
			return true;
		}
	}

	if(m_policyChangeFile.IsPending())
	{
		return true;
	}

	return false;
}

bool SocialClubPolicyManager::AreTextsInSyncWithCloud() const
{
	for (int i = 0; i < SCF_MAX; ++i)
	{
		if (!m_cloudPolicy[i].DidSucceed() || m_cloudPolicy[i].IsLocalFile())
		{
			return false;
		}
	}

	return true;
}

const char* SocialClubPolicyManager::GetError() const
{
	for(int i=0; i<SCF_MAX; ++i)
	{
		if(!m_cloudPolicy[i].DidSucceed() && !m_cloudPolicy[i].IsPending())
		{
			uiDebugf1("GetError :: Error requesting policy file! Name: %s, Code: %d", m_cloudPolicy[i].GetFilename(), m_cloudPolicy[i].GetResultCode());
			return SocialClubMgr::GetErrorName(m_cloudPolicy[i].GetResultCode());
		}
	}

	if(!m_policyChangeFile.DidSucceed() && !m_policyChangeFile.IsPending())
	{
		uiDebugf1("GetError :: Error requesting policy change file! Code: %d", m_policyChangeFile.GetResultCode());
		return SocialClubMgr::GetErrorName(m_policyChangeFile.GetResultCode());
	}

	return SPolicyVersions::GetInstance().GetError();
}

const char* SocialClubPolicyManager::GetErrorTitle() const
{
	if(rlRos::GetCredentials(m_gamerIndex).HasPrivilege(RLROS_PRIVILEGEID_DEVELOPER))
	{
		for(int i=0; i<SCF_MAX; ++i)
		{
			if(SPolicyVersions::GetInstance().GetVersionFile(i).IsError())
			{
				return "DEV_SC_POLICY_FAILED";
			}
		}
	}

	return "SC_WARNING_TITLE";
}


// eof
