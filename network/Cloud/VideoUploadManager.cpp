#include "Network/Cloud/VideoUploadManager.h"

#if defined(GTA_REPLAY) && GTA_REPLAY 

#include "control/replay/file/ReplayFileManager.h"

#include "network/NetworkInterface.h"

#include "Network/Live/livemanager.h"

#if RSG_PC
#include "rline/ros/rlros.h"
#endif

CVideoUploadTunablesListener* CVideoUploadManager::m_pTunablesListener = NULL;

void CVideoUploadTunablesListener::OnTunablesRead()
{
	VideoUploadManager::GetInstance().UpdateTunables();
}


CVideoUploadManager::CVideoUploadManager()
{

}

CVideoUploadManager::~CVideoUploadManager()
{

}

void CVideoUploadManager::Init()
{
	m_localGamerIndex = -1;

	m_userID = 0;

	m_serverKillSwitch = false;
	m_enableYoutubeUploads = false;
	m_enableYoutubePublic = false;
	m_enableAlbumAdvert = false;
	m_advertTrackId = 0;
	m_enableResetAuthOn401 = true;
	m_uploadCurrentlyEnabled = false;
	m_hasCorrectPrivileges = true;
}

void CVideoUploadManager::Shutdown()
{
	if (m_pTunablesListener)
	{
		delete m_pTunablesListener;
		m_pTunablesListener = NULL;
	}
}

void CVideoUploadManager::Update()
{
	if (!m_pTunablesListener && Tunables::IsInstantiated())
	{
		m_pTunablesListener = rage_new CVideoUploadTunablesListener();
		SetInitialTunables();
	}

	RefreshUploadEnabledFlag();
}

bool CVideoUploadManager::UpdateUsers()
{
	m_localGamerIndex = NetworkInterface::GetLocalGamerIndex();

#if RSG_DURANGO
	u64 newUserId = 0;
	if (CLiveManager::IsOnline())
	{
		newUserId = (m_localGamerIndex >= 0) ? NetworkInterface::GetLocalGamerHandle().GetXuid() : 0;
	}
#elif RSG_ORBIS
	s32 newUserId = 0;
	if (CLiveManager::IsOnline())
	{
		newUserId = (m_localGamerIndex >= 0) ? static_cast<s32>( ReplayFileManager::GetCurrentUserId(m_localGamerIndex) ) : 0;
	}
#else
	s64 newUserId = 0;
	if (CLiveManager::IsOnline())
	{
		const rlRosCredentials & cred = rlRos::GetCredentials(m_localGamerIndex);
		if( cred.IsValid() && cred.GetRockstarId() != 0 )
		{
			newUserId = cred.GetRockstarId();
		}
	}
#endif

	if (newUserId != m_userID)
	{
		m_userID = newUserId;
		return true;
	}

	return false;
}

bool CVideoUploadManager::GatherHeaderData(const char* sourcePathAndFileName, u32 duration, const char* createdDate, const char* displayName)
{
	m_sourcePathAndFileName = sourcePathAndFileName;
	m_sourcePathAndFileName.Replace("\\", "/");

	m_displayName = displayName;

	fiStream* pStream = ASSET.Open(m_sourcePathAndFileName,"");
	if (!pStream)
		return false;

	m_sourceCreatedDate = createdDate;

	m_videoSize = pStream->Size64();
	pStream->Close();

	m_videoDurationMs = duration;

	return true;
}

void CVideoUploadManager::ResetCoreValues()
{
	m_videoSize = 0;
	m_videoDurationMs = 0;
}

void CVideoUploadManager::ReadTunables()
{
#if ENABLE_VIDEO_UPLOAD_THROTTLES
	m_maxUploadSpeed = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MAX_VIDEO_UPLOAD_SPEED", 0x4b968c82), MaxUploadSpeed);
	m_maxBlockSize = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MAX_VIDEO_BLOCK_SIZE", 0x3c59c32e), MaxBlockSize);
#endif

	m_enableYoutubeUploads = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("YOUTUBE_ENABLED", 0xf84065d0), false);
	m_enableYoutubePublic = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("YOUTUBE_PUBLIC", 0xa3e6a02b), false);
	m_enableAlbumAdvert = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("YOUTUBE_ALBUM_AD", 0xde3ccd32), false);
	m_advertTrackId = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("YOUTUBE_ALBUM_AD_TRACKID", 0x778d8a4e), 0);
	m_serverKillSwitch = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("UPLOAD_KILLSWITCH", 0xfb58c812), false);

	// default to true, only want to stop this if issues occur
	m_enableResetAuthOn401 = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("UPLOAD_AUTHFAIL_RESET", 0xef910fd), true);  

	RefreshUploadEnabledFlag();
}

void CVideoUploadManager::SetInitialTunables()
{
	ReadTunables();

#if ENABLE_VIDEO_UPLOAD_THROTTLES
	// throttle speed won't have been changed by game/user yet, so just whack it to the new max
	SetIdealUploadSpeed(m_maxUploadSpeed); //kb/s
#endif
}

void CVideoUploadManager::UpdateTunables()
{
	ReadTunables();

#if ENABLE_VIDEO_UPLOAD_THROTTLES
	if (m_maxUploadSpeed < m_throttleSpeed)
		SetIdealUploadSpeed(m_maxUploadSpeed); //kb/s
#endif
}

void CVideoUploadManager::RefreshUploadEnabledFlag()
{
	// pre-check of things that won't often change, but tunables could break
	
	// no upload in china
	// just do PC for now, as facebook is ... we can expand out later
#if RSG_PC
	// keep checking, it's how the facebook code does it
	if(m_enableYoutubeUploads)
	{
		rlRosGeoLocInfo geoLocInfo;
		if(rlRos::GetGeoLocInfo(&geoLocInfo))
		{
			if (stricmp(geoLocInfo.m_CountryCode,"CN") == 0)
			{
				m_enableYoutubeUploads = false;
			}
		}
	}
#endif

	// enableYoutubeUploads is more of an initial is available
	m_uploadCurrentlyEnabled = m_enableYoutubeUploads && CLiveManager::HasValidRosCredentials();

	// but we also have to check for various other things that could change more rapidly like privileges 
	if (m_uploadCurrentlyEnabled)
	{
		// check to see if network sharing privileges are allowed
		m_hasCorrectPrivileges = CLiveManager::GetSocialNetworkingSharingPrivileges();

		// B*2429351 - XB1 - want to inform user about privileges on upload attempt, rather than disable entirely
#if !RSG_DURANGO
		m_uploadCurrentlyEnabled = m_hasCorrectPrivileges;
#endif

		// upload to YT isn't allowed on any PS SubAccount
#if RSG_ORBIS
		if (m_uploadCurrentlyEnabled)
		{
			m_uploadCurrentlyEnabled = !CLiveManager::IsSubAccount();
		}
#endif
	}
}

#endif //  defined(GTA_REPLAY) && GTA_REPLAY 
