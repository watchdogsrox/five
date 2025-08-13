//
// SocialClubNewsStoryMgr.cpp
//
// Copyright (C) 2012 Rockstar Games.  All Rights Reserved.
#include "SocialClubNewsStoryMgr.h"

//RAGE
#include "bank/list.h"
#include "data/rson.h"
#include "system/param.h"

//framework
#include "fwnet/netchannel.h"

//Game
#include "frontend/HudTools.h"
#include "Network/general/NetworkUtil.h"
#include "Network/NetworkInterface.h"
#include "Network/SocialClubServices/GamePresenceEvents.h"

NETWORK_OPTIMISATIONS();
//OPTIMISATIONS_OFF()

#undef __net_channel
#define __net_channel net_newsmgr
RAGE_DEFINE_SUBCHANNEL(net,newsmgr);

#define NEWS_DIRECTION_PREVIOUS	(-1)
#define NEWS_DIRECTION_NEXT (1)

#define NEWS_STYLE_GTA (atHashString("gta", 0x57a4d1d6))
#define DEFAULT_STORY_STYLE (NEWS_STYLE_GTA)

PARAM(noSCNews, "Disable SC News");
PARAM(scnewslang, "force the news to use the giving lang code");
PARAM(scnewsall, "Ignore the expiry and unlock time of the news stories");
PARAM(scnewslocal, "Read a local news file");
PARAM(scprioritynews, "Force priority news on or off");
PARAM(scnewsstorylocal, "Read a local news story file path");
PARAM(scnewsskipdelaysec, "Enforce a skip delay time");

// ******************************************************************************************************
// ** CNetworkSCNewsStoryRequest
// ******************************************************************************************************

CNetworkSCNewsStoryRequest::CNetworkSCNewsStoryRequest( const CNewsItemPresenceEvent& newNewsStory )
	: CloudListener()
	, m_texRequestHandle(CTextureDownloadRequest::INVALID_HANDLE)
	, m_TextureDictionarySlot(-1)
	, m_cloudReqID(-1)
	, m_state(STATE_NONE)
	, m_bFailedLastRequest(false)
	, m_bDoImageRequest(true)
	, m_iImageType(NEWS_IMG_TYPE_PORTRAIT)
	, m_ticksInShown(0)
	, m_style( DEFAULT_STORY_STYLE )
{
	safecpy(m_newsStoryKey, newNewsStory.GetStoryKey());
	m_iPriority = newNewsStory.GetStoryPriority();
	m_iSecondaryPriority = newNewsStory.GetSecondaryPriority();
	m_trackingId = newNewsStory.GetTrackingId();

	m_newsTypes.Reset();
	const CNewsItemPresenceEvent::TypeHashList& typeList = newNewsStory.GetTypeList();
	for(int i = 0; i < typeList.GetCount() && i < m_newsTypes.GetMaxCount(); i++)
	{
		m_newsTypes.Append() = typeList[i];
	}

	// Wherever we show transition news we also show priority news so this
	// simplifies enumerating them.
	if (HasType(NEWS_TYPE_PRIORITY_HASH) && !HasType(NEWS_TYPE_TRANSITION_HASH) && gnetVerify(!m_newsTypes.IsFull()))
	{
		gnetDebug2("Flagging priorty news %s as transition", m_newsStoryKey);
		m_newsTypes.Append() = NEWS_TYPE_TRANSITION_HASH;
	}

	m_newStoryTxdName[0] = '\0';

	for(int i = 0; i < MAX_CLOUD_IMAGES; ++i)
	{
		m_cloudImage[i].Clear();
	}
}

bool CNetworkSCNewsStoryRequest::Start()
{
	if (!gnetVerify(strlen(m_newsStoryKey) > 0))
	{
		gnetError("No key set for news request");
		SetState(STATE_ERROR);
		return false;
	}

	//If we've already requested the cloud info for this new story, we can move along
	bool bNeedToRequestCloud = m_headline.length() == 0 || m_content.length() == 0;
	if (bNeedToRequestCloud)
	{
		char path[128];
		const char* fileType = "json";

		//Get the language for the local player
		const char* lang = NetworkUtils::GetLanguageCode();

#if __BANK
		const char* paramLang = NULL;
		if(PARAM_scnewslang.Get(paramLang) && paramLang != NULL)
		{
			gnetDebug1("PARAM set language to %s", paramLang);
			lang = paramLang;
		}
#endif

#if !__NO_OUTPUT
		const char* localp = nullptr;
		if (PARAM_scnewsstorylocal.Get(localp))
		{
			ReadNewsStoryFromFile(localp, m_newsStoryKey);
			return true;
		}
#endif

		formatf(path, "sc/news/%s/%s.%s", m_newsStoryKey, lang, fileType);
		m_cloudReqID = CloudManager::GetInstance().RequestGetGlobalFile(path, 1024, eRequest_CacheAddAndEncrypt);

		if( m_cloudReqID != -1)
		{
			gnetDebug1("Starting news story request for '%s'",m_newsStoryKey );
			SetState(STATE_WAITING_FOR_CLOUD);
		}
		else
		{
			gnetError("FAILED to start news story request for %s", path);
			SetState(STATE_ERROR);
		}
	}
	//Else, if we've already got the stuff from the cloud, we may just need to request the image
	else if(m_bDoImageRequest && m_TextureDictionarySlot == -1 && strlen(m_cloudImage[m_iImageType].m_cloudImagePath) > 0)
	{
		if(m_bDoImageRequest)
		{
			gnetDebug1("Skipping to image request for '%s' because we already have cloud data", m_newsStoryKey);
			DoImageRequest();
		}
		else
		{
			SetState(STATE_DATA_RCVD);
		}
	}
	else
	{
		gnetDebug1("Starting '%s' but already seem to be ready to go", m_newsStoryKey);
		SetState(STATE_RCVD);
	}
	
	return true;
}

void CNetworkSCNewsStoryRequest::OnCloudEvent( const CloudEvent* pEvent )
{
	switch(pEvent->GetType())
	{
	case CloudEvent::EVENT_REQUEST_FINISHED:
		{
			// grab event data
			const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

			// check if we're interested
			if(pEventData->nRequestID != m_cloudReqID || m_state != STATE_WAITING_FOR_CLOUD)
				return;

			if (pEventData->bDidSucceed)
			{
				gnetDebug2("Received cloud loc string for '%s' of %d bytes", m_newsStoryKey, pEventData->nDataSize);

				m_gb.Init(NULL, datGrowBuffer::NULL_TERMINATE);
				if (pEventData->nDataSize > 0 && gnetVerify(m_gb.Preallocate(pEventData->nDataSize)))
				{
					if(gnetVerify(m_gb.AppendOrFail(pEventData->pData, pEventData->nDataSize)))
					{
						gnetDebug1("SUCCESS recieving news story '%s'", m_newsStoryKey );
						HandleReceivedData();
					}
					else
					{
						gnetError("Fail to fill grow buffer for news story '%s'", m_newsStoryKey);
						SetState(STATE_ERROR);
					}
				}
				else
				{
					gnetError("Cloud request completed with 0 bytes for '%s'", m_newsStoryKey);
					SetState(STATE_ERROR);
				}
				m_cloudReqID = -1;
			}
			else
			{
				gnetError("Request for '%s' failed with error code %d", m_newsStoryKey, pEventData->nResultCode );
				SetState(STATE_ERROR);

				m_cloudReqID = -1;
				m_gb.Clear();
			}
		}
		break;
	default:
		break;
	}
}

void CNetworkSCNewsStoryRequest::Update()
{
	switch(m_state)
	{
	case STATE_NONE:
		{
			// Default idle State
		}
		break;
	case STATE_REQUESTED:
		{
			m_bFailedLastRequest = false;
			gnetDebug1("Request to show news story '%s' has trigger request", m_newsStoryKey);
			m_ticksInShown = 0;	// ensure we've reset
			Start();
		}
		break;
	case STATE_WAITING_FOR_CLOUD:
		{
			//Waiting for the cloud manager to make the callback to OnCloudEvent
		}
		break;
	case STATE_DATA_RCVD:
		{
			//Received json data, but pending a do image request
		}
	case STATE_WAITING_FOR_IMAGE:
		{
			if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(m_texRequestHandle))
			{
				//For now, we'll just treat it as received since we have the text of the article.
				SetState(STATE_RCVD);
			}
			else if ( DOWNLOADABLETEXTUREMGR.IsRequestReady( m_texRequestHandle ) )
			{
				//Update the slot that the texture was slammed into
				strLocalIndex txdSlot = strLocalIndex(DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_texRequestHandle));
				if (gnetVerifyf(g_TxdStore.IsValidSlot(txdSlot), "CNetworkSCNewsStoryRequest - failed to get valid txd for news story '%s' from DTM at slot %d", m_newsStoryKey, txdSlot.Get()))
				{
					g_TxdStore.AddRef(txdSlot, REF_OTHER);
					m_TextureDictionarySlot = txdSlot;
					gnetDebug1("Image for news story '%s' has been received in TXD slot %d", m_newsStoryKey, txdSlot.Get());
				}
				SetState(STATE_RCVD);
			}
		}
		break;
	case STATE_RCVD:
		{
		}
		break;
	case STATE_SHOWN:
		{
			//Wait a bit once we've marked as shown to release all our stuff (which has been copied to scaleform)
			bool bKeepWaiting = ++m_ticksInShown < 30;
			if (!bKeepWaiting)
			{
				SetState(STATE_CLEANUP);
				gnetDebug1("'%s' was shown and now is moving to CLEANUP", m_newsStoryKey);
				m_ticksInShown = 0; // ensure we've reset
			}
		}
		break;
	case STATE_CLEANUP:
		{
			// Ensure that we release the texture download request!
			DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_texRequestHandle );
			m_texRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;

			m_bDoImageRequest = false;
			ReleaseTXDRef();
			gnetDebug1("'%s' is cleaned up and returning to NONE", m_newsStoryKey);
			SetState(STATE_NONE);  //Back to none (since we've released our TXD ref).
			m_ticksInShown = 0;	// ensure we've reset
		}
		break;
	case STATE_ERROR:
		{
			SetState(STATE_CLEANUP);
			m_bFailedLastRequest = true;
		}
		break;
	}
}

bool CNetworkSCNewsStoryRequest::HandleReceivedData()
{
	//Process the received json object into something.
	if (!gnetVerify(m_gb.Length() > 0))
	{
		SetState(STATE_ERROR);
		return false;
	}


	gnetDebug1("Processing received data for news story '%s'", m_newsStoryKey);

	if ( !gnetVerifyf(RsonReader::ValidateJson((const char*) m_gb.GetBuffer(), m_gb.Length()), "Failed json validation for '%s'", (const char*)m_gb.GetBuffer()) )
	{
		SetState(STATE_ERROR);
		return false;
	}

	RsonReader rr((const char*) m_gb.GetBuffer(), m_gb.Length());

	//Check the date.  If the date on the next story hasn't come yet, then don't show it.
	u64 posixDate = 0;
	if (rr.ReadUns64("date", posixDate))
	{
		u64 currentDate = rlGetPosixTime();
		if (currentDate < posixDate OUTPUT_ONLY(&& !PARAM_scnewsall.Get()))
		{
			gnetDebug1("Date of %" I64FMT "d for '%s' is not valid yet", posixDate, m_newsStoryKey);
			SetState(STATE_ERROR);
			return false;
		}
	}

	// Only progress if we're in a proper state to
	if(m_state == STATE_WAITING_FOR_CLOUD)
	{	
		SetState(STATE_RCVD);
	}

	// An optional override for default title (i.e. GTA Online)
	if (rr.HasMember("title"))
	{
		int strLength = 0;
		const char* value = rr.GetRawMemberValue("title", strLength);
		if (strLength == 0)
		{
			gnetDebug3("No 'title' for %s", m_newsStoryKey);
		}

		m_title.Set(value, strLength, 0, strLength);
	}
	
	if (rr.HasMember("headline"))
	{
		int strLength = 0;
		const char* value = rr.GetRawMemberValue("headline", strLength);
		if (strLength == 0)
		{
			gnetDebug3("No 'headline' for %s", m_newsStoryKey);
			return false;
		}

		m_headline.Set(value, strLength, 0, strLength);
	}

	if (rr.HasMember("subtitle"))
	{
		int strLength = 0;
		const char* value = rr.GetRawMemberValue("subtitle", strLength);
		if (strLength == 0)
		{
			gnetDebug3("No 'subtitle' for %s", m_newsStoryKey);
		}

		SetSubtitle(value, strLength);
	}

	if (rr.HasMember("content"))
	{
		int strLength = 0;
		const char* value = rr.GetRawMemberValue("content", strLength);
		if (strLength == 0)
		{
			gnetDebug3("No 'content' for %s", m_newsStoryKey);
			// Do not return.  A story with empty content is OK because script can now customize the content based on player information
		}

		m_content.Set(value, strLength, 0, strLength);
	}

	if (rr.HasMember("url"))
	{
		int strLength = 0;
		const char* value = rr.GetRawMemberValue("url", strLength);
		if (strLength == 0)
		{
			gnetDebug3("No 'url' for %s", m_newsStoryKey);
		}

		m_url.Set(value, strLength, 0, strLength);
	}

	if( rr.HasMember("style") )
	{
		int strLength = 0;
		const char* value = rr.GetRawMemberValue("style", strLength);
		if (strLength == 0)
		{
			gnetDebug3("No 'style' for %s", m_newsStoryKey);
		}
		else
		{
			atString tmpBuffer;
			tmpBuffer.Set(value, strLength, 0, strLength);
			m_style = atHashString(tmpBuffer.c_str());
		}
	}

	// Only progress if we're in a proper state to
	if(m_state == STATE_WAITING_FOR_CLOUD)
	{
		SetState(STATE_DATA_RCVD);
	}

	//If this news story has an image, we'll need kick over to request it for download
	bool bImageFound = false;
	RsonReader imageRR;
	if (rr.GetMember("image", &imageRR))
	{
		bImageFound = ReadImageData(imageRR, NEWS_IMG_TYPE_PORTRAIT) && m_iImageType == NEWS_IMG_TYPE_PORTRAIT;
	}

	if ( rr.GetMember("landscape", &imageRR) )
	{
		bImageFound = (ReadImageData(imageRR, NEWS_IMG_TYPE_LANDSCAPE) && m_iImageType == NEWS_IMG_TYPE_LANDSCAPE) || bImageFound;
	}

	if (rr.HasMember("extraData"))
	{
		int strLen = 0;
		const char* memberData = rr.GetRawMemberValue("extraData", strLen);
		if(memberData != NULL && strLen > 0)
		{
			m_extraData.Set(memberData, strLen, 0);
		}
	}

	//Start the image request.  It will change the state appropriately.
	if(m_bDoImageRequest && bImageFound)
	{
		DoImageRequest();
	}

	//We've cached all the data off, so clear out our grow buffer.
	m_gb.Clear();

	return true;

}

#define GAMERNAME_TOKEN "GAMER_NAME~"

void CNetworkSCNewsStoryRequest::SetSubtitle(const char* newSubtitle, int newSubtitleLen)
{
	if(gnetVerifyf(newSubtitle, "Trying to set a CNetworkSCNewsStoryRequest NULL title"))
	{
		m_subtitle.Clear();

		const int SIZE_OF_GAMER_NAME_TOKEN = (int)strlen(GAMERNAME_TOKEN);

		// Parse title for gamer name token
		for(int i = 0; i < newSubtitleLen; ++i)
		{
			const char currentChar = newSubtitle[i];
			if(currentChar == FONT_CHAR_TOKEN_DELIMITER)
			{
				if(!strncmp(GAMERNAME_TOKEN, (newSubtitle+i+1), SIZE_OF_GAMER_NAME_TOKEN))
				{
					// Found a match, insert gamer name
					const char* gamerName = NetworkInterface::GetLocalGamerName();
					m_subtitle += gamerName;
					i += SIZE_OF_GAMER_NAME_TOKEN;
				}
				else
				{
					// No match :(
					gnetDebug3("Found token that cannot be parsed %s", newSubtitle);

					// Ignore this token
					while(++i < newSubtitleLen && newSubtitle[i] != FONT_CHAR_TOKEN_DELIMITER);
				}
			}
			else
			{
				m_subtitle += currentChar;
			}
		}
	}
}

bool CNetworkSCNewsStoryRequest::ReadImageData(const RsonReader& imageRR, eNewsImgType iImageType)
{
	if(!imageRR.ReadString("path", m_cloudImage[iImageType].m_cloudImagePath))
	{
		gnetError("No path set in image for '%s'", m_newsStoryKey);
		SetState(STATE_ERROR);
		return false;
	}

	imageRR.ReadString("path", m_cloudImage[iImageType].m_cloudImagePath);

	// names are case-sensitive - "filesize" is correct, but fall back to checking the old name "fileSize" here as well to be safe
	const bool sizeFound = imageRR.ReadInt("filesize", m_cloudImage[iImageType].m_cloudImagePresize);
	if(!sizeFound)
	{
		imageRR.ReadInt("fileSize", m_cloudImage[iImageType].m_cloudImagePresize);
	}

	return strlen(m_cloudImage[iImageType].m_cloudImagePath) > 0;
}

bool CNetworkSCNewsStoryRequest::DoImageRequest()
{
	bool success = false;

	formatf(m_newStoryTxdName, "news_%s_%d", m_newsStoryKey, m_iImageType);

	//@TODO Fix up the image path to see if the image is a 'full path' 
	//or a 'local path' (using the key).
	//@TODO

	// Fill in the descriptor for our request
	m_texRequestDesc.m_Type = CTextureDownloadRequestDesc::GLOBAL_FILE;
	//m_requestDesc.m_GamerIndex		= localGamerIndex;
	m_texRequestDesc.m_CloudFilePath	= m_cloudImage[m_iImageType].m_cloudImagePath;
	m_texRequestDesc.m_BufferPresize	= m_cloudImage[m_iImageType].m_cloudImagePresize;
	m_texRequestDesc.m_TxtAndTextureHash= m_newStoryTxdName;
	m_texRequestDesc.m_CloudOnlineService = RL_CLOUD_ONLINE_SERVICE_SC;

	// Only download the image if it isn't cached locally
	m_texRequestDesc.m_CloudRequestFlags |= eRequestFlags::eRequest_CacheForceCache;

	gnetDebug1("Requesting image for news story '%s' from '%s'", m_newsStoryKey, m_texRequestDesc.m_CloudFilePath);

	CTextureDownloadRequest::Status retVal = CTextureDownloadRequest::REQUEST_FAILED;
	retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(m_texRequestHandle, m_texRequestDesc);

	if(retVal == CTextureDownloadRequest::IN_PROGRESS || retVal == CTextureDownloadRequest::READY_FOR_USER)
	{
		gnetAssert(m_texRequestHandle != CTextureDownloadRequest::INVALID_HANDLE);
#if !__NO_OUTPUT
		if (retVal == CTextureDownloadRequest::READY_FOR_USER)
			gnetDebug1("DTM already has TXD ready for %s with request handle %d", m_newStoryTxdName, m_texRequestHandle);
#endif

		SetState(STATE_WAITING_FOR_IMAGE);
		success = true;
	} 
	else
	{
		gnetError("Failed Request image for news story '%s' (DTM result %d)", m_newsStoryKey, retVal);
	}

	return success;

}

void CNetworkSCNewsStoryRequest::Cancel()
{
	if (m_texRequestHandle != CTextureDownloadRequest::INVALID_HANDLE)
	{
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_texRequestHandle);
		m_texRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	}

	ReleaseTXDRef();

	m_cloudReqID = -1;
	m_gb.Clear();
	m_title.Clear();
	m_headline.Clear();
	m_subtitle.Clear();
	m_content.Clear();
	m_url.Clear();
	m_newStoryTxdName[0] = '\0';
	m_newsStoryKey[0] = '\0';
	m_trackingId = 0;
}

CNetworkSCNewsStoryRequest::~CNetworkSCNewsStoryRequest()
{
	Cancel();
}

const char* CNetworkSCNewsStoryRequest::GetTitle() const
{
	if (!gnetVerify(ReceivedData() || IsIdle()))
	{
		return "";
	}

	return m_title.c_str();
}

const char* CNetworkSCNewsStoryRequest::GetSubtitle() const
{
	if (!gnetVerify(ReceivedData() || IsIdle()))
	{
		return "";
	}

	return m_subtitle.c_str();
}

const char* CNetworkSCNewsStoryRequest::GetContent() const
{
	if (!gnetVerify(ReceivedData() || IsIdle()))
	{
		return "";
	}

	return m_content.c_str();
}

const char* CNetworkSCNewsStoryRequest::GetHeadline() const
{
	if (!gnetVerify(ReceivedData() || IsIdle()))
	{
		return "";
	}

	return m_headline.c_str();
}

const char* CNetworkSCNewsStoryRequest::GetTxdName() const
{
	return m_newStoryTxdName;
}

const char* CNetworkSCNewsStoryRequest::GetURL() const
{
	if (!gnetVerify(m_state != STATE_WAITING_FOR_CLOUD))
	{
		return "";
	}

	return m_url.c_str();
}

atHashString CNetworkSCNewsStoryRequest::GetStyle() const
{
	if (!gnetVerify(ReceivedData() || IsIdle()))
	{
		return DEFAULT_STORY_STYLE;
	}

	return m_style;
}

u32 CNetworkSCNewsStoryRequest::GetTrackingId() const
{
	// No need to assert. This is set in the constructor
	return m_trackingId;
}

bool CNetworkSCNewsStoryRequest::GetExtraNewsData(const char* name, int& value) const
{
	RsonReader extraDataMember;
	if (GetExtraDataMember(name, extraDataMember))
	{
		return extraDataMember.AsInt(value);
	}

	return false;
}

bool CNetworkSCNewsStoryRequest::GetExtraNewsData(const char* name, float& value) const
{
	RsonReader extraDataMember;
	if (GetExtraDataMember(name, extraDataMember))
	{
		return extraDataMember.AsFloat(value);
	}

	return false;
}

bool CNetworkSCNewsStoryRequest::GetExtraNewsData(const char* name, char* str, int maxStr) const
{
	RsonReader extraDataMember;
	if (GetExtraDataMember(name, extraDataMember))
	{
		return extraDataMember.AsString(str,maxStr);
	}

	return false;
}

bool CNetworkSCNewsStoryRequest::GetExtraDataMember(const char* name, RsonReader& rr) const
{
	if(HasExtraNewsData())
	{
		RsonReader extraRR(m_extraData.c_str(), m_extraData.GetLength());
		return extraRR.GetMember(name, &rr);
	}

	return false;
}

bool CNetworkSCNewsStoryRequest::HasExtraDataMember(const char* name) const
{
	if(HasExtraNewsData())
	{
		RsonReader extraRR(m_extraData.c_str(), m_extraData.GetLength());
		return extraRR.HasMember(name);
	}

	return false;
}

void CNetworkSCNewsStoryRequest::Request(bool bDoImageRequest, eNewsImgType iImageType/*=NEWS_IMG_TYPE_PORTRAIT*/)
{
	if(gnetVerifyf(m_state == STATE_NONE, "CNetworkSCNewsStoryRequest - cannot make a request in current state: %d", m_state) &&
		gnetVerifyf(iImageType < MAX_CLOUD_IMAGES && iImageType >= 0, "CNetworkSCNewsStoryRequest - invalid image type requested: %d", iImageType))
	{
		SetState(STATE_REQUESTED);
		m_bDoImageRequest = bDoImageRequest;
		m_iImageType = iImageType;
	}

}

void CNetworkSCNewsStoryRequest::RequestImage(eNewsImgType iImageType)
{
	 // make sure we have received data
	if(gnetVerifyf(m_state == STATE_DATA_RCVD, "CNetworkSCNewsStoryRequest - cannot make image request in current state: %d", m_state) &&
		gnetVerifyf(iImageType < MAX_CLOUD_IMAGES && iImageType >= 0, "CNetworkSCNewsStoryRequest - invalid image type requested: %d", iImageType))
	{
		m_iImageType = iImageType;
		DoImageRequest(); 
	}
}

void CNetworkSCNewsStoryRequest::ReleaseTXDRef()
{
	if (m_TextureDictionarySlot.Get() >= 0)
	{
		if(g_TxdStore.IsValidSlot(m_TextureDictionarySlot))
		{
			gnetDebug1("Releasing '%s' News Story's TXD reference to '%s'", m_newsStoryKey, m_newStoryTxdName);
			g_TxdStore.RemoveRef(m_TextureDictionarySlot, REF_OTHER);
		}
		m_TextureDictionarySlot = strLocalIndex(-1);
	}
}

bool CNetworkSCNewsStoryRequest::HasType( u32 typeHash ) const
{
#if !__FINAL
	//No types is universal.  This will look terribles
	if (m_newsTypes.GetCount() == 0)
	{
		return true;
	}
#endif

	for(int i = 0; i < m_newsTypes.GetCount(); ++i)
	{
		if(m_newsTypes[i] == typeHash)
			return true;
	}

	return false;
}

void CNetworkSCNewsStoryRequest::SetState( State newState )
{
	gnetDebug3("Request '%s' state OLD: %d  NEW: %d ", m_newsStoryKey, m_state, newState);
	m_state = newState;
}

#if !__NO_OUTPUT
void CNetworkSCNewsStoryRequest::ReadNewsStoryFromFile(const char* fpath, const char* key)
{
	char filePath[RAGE_MAX_PATH] = { 0 };
	formatf(filePath, "%s/%s.json", fpath, key);

	const fiDevice* device = nullptr;
	fiHandle handle = fiHandleInvalid;
	char* buffer = nullptr;

	rtry
	{
		device = fiDevice::GetDevice(filePath, true);
		rcheck(device, catchall, gnetError("No device for %s", filePath));

		handle = device->Open(filePath, true);
		rcheck(fiIsValidHandle(handle), catchall, gnetError("News file not found %s", filePath));

		u64 len = device->GetFileSize(filePath);
		buffer = rage_new char[len + 1];
		buffer[len] = 0;

		int num = device->Read(handle, buffer, (int)len);
		rcheck(num == (int)len, catchall, gnetError("Failed to read data for %s", filePath));

		gnetDebug1("Read local news story file %s", filePath);

		m_gb.Init(NULL, datGrowBuffer::NULL_TERMINATE);
		m_gb.Preallocate(num);
		rverify(m_gb.AppendOrFail(buffer, num), catchall, gnetError("Failed to set buffer for %s", filePath));

		HandleReceivedData();
	}
	rcatchall
	{
		SetState(STATE_ERROR);
	}

	if (device != nullptr && fiIsValidHandle(handle))
	{
		device->Close(handle);
	}

	if (buffer != nullptr)
	{
		delete[] buffer;
		buffer = nullptr;
	}
}
#endif

// ******************************************************************************************************
// ** CNetworkNewsStoryMgr
// ******************************************************************************************************
bool CNetworkNewsStoryMgr::AddNewsStoryItem( const CNewsItemPresenceEvent& newNewsStory )
{
	if(PARAM_noSCNews.Get())
	{
		gnetDebug1("Ignoring Add. SC News disabled.");
		return false;
	}

	int iIndexToInsert = 0;
	const char* newsStoryKey = newNewsStory.GetStoryKey();
	//Check to make sure there isn't already a news story with this key already
	for (int i = 0; i < m_requestArray.GetCount(); i++)
	{
		if (strcmp(m_requestArray[i]->GetStoryKey(), newsStoryKey) == 0)
		{ 
			gnetDebug1("News story with key '%s' already found in request list", newsStoryKey);
			return false; 
		}

		// Sort in ascending order
		if(newNewsStory.GetStoryPriority() >= m_requestArray[i]->GetPriority() )
		{
			iIndexToInsert = i + 1;
		}
	}

	//Create a new request and add it to the list
	CNetworkSCNewsStoryRequest* pNewRequest = rage_new CNetworkSCNewsStoryRequest(newNewsStory);
	if (!pNewRequest)
	{
		gnetError("Unable to create request for '%s'", newsStoryKey);
		return false;
	}

 	gnetDebug3("Add news story with key '%s' with priority = %d", newsStoryKey, pNewRequest->GetPriority() );

	if(iIndexToInsert == m_requestArray.GetCount())
	{
		m_requestArray.PushAndGrow(pNewRequest);
	}
	else
	{
		m_requestArray.insert(&m_requestArray[iIndexToInsert], 1, pNewRequest);
	}

	return true;
}

CNetworkNewsStoryMgr& CNetworkNewsStoryMgr::Get()
{
	typedef atSingleton<CNetworkNewsStoryMgr> CNetworkNewsStoryMgr;
	if (!CNetworkNewsStoryMgr::IsInstantiated())
	{
		CNetworkNewsStoryMgr::Instantiate();
	}

	return CNetworkNewsStoryMgr::GetInstance();
}

void CNetworkNewsStoryMgr::Shutdown()
{
	for (int i = 0; i < m_requestArray.GetCount(); ++i)
	{
		m_requestArray[i]->Cancel();
		delete m_requestArray[i];
	}
	m_requestArray.Reset();

	m_CloudRequestCompleted = false;
}


void CNetworkNewsStoryMgr::RequestDownloadCloudFile()
{
	if (PARAM_noSCNews.Get())
	{
		m_CloudRequestCompleted = true;
		return;
	}

	if (m_cloudReqID != INVALID_CLOUD_REQUEST_ID)
	{
		gnetDebug3("News cloud file request for download ignored because download is in progress");
		return;
	}

	m_bRequestCloudFile = true;
}


void CNetworkNewsStoryMgr::DoCloudFileRequest()
{
	if (PARAM_noSCNews.Get())
	{
		return;
	}

	m_bRequestCloudFile = false;  //Mark the reqeust handled, even if there is one in flight.

	if (!gnetVerify(m_cloudReqID == INVALID_CLOUD_REQUEST_ID))
	{
		return;
	}

#if !__NO_OUTPUT
	const char* path = nullptr;
	if (PARAM_scnewslocal.Get(path))
	{
		ReadNewsFromFile(path);
		return;
	}
#endif

	m_cloudReqID = CloudManager::GetInstance().RequestGetTitleFile("news/news.json", 1024, eRequest_CacheAddAndEncrypt);
}

void CNetworkNewsStoryMgr::ParseNews(const char* text, const int len)
{
	//Process a list of news game events.
	RsonReader reader(text, len);

	u64 currentPosixTime = rlGetPosixTime();
	RsonReader iter;
	CNewsItemPresenceEvent newsMsgObject;

	bool success = reader.GetFirstMember(&iter);
	while( success )
	{
		if(newsMsgObject.GetObjectAsType(iter))
		{
			if ((currentPosixTime >= (u64)newsMsgObject.GetActiveDate() &&
			((currentPosixTime <= (u64)newsMsgObject.GetExpireDate()) || newsMsgObject.GetExpireDate() == 0)) OUTPUT_ONLY(|| PARAM_scnewsall.Get()))
			{
				gnetDebug3("Adding news story '%s' from cloud file [%d, %d]", newsMsgObject.GetStoryKey(), newsMsgObject.GetActiveDate(), newsMsgObject.GetExpireDate());
				AddNewsStoryItem(newsMsgObject);
			}
			else
			{
				gnetDebug3("Ignoring cloud news story '%s' due to date [%d, %d] ", newsMsgObject.GetStoryKey(), newsMsgObject.GetActiveDate(), newsMsgObject.GetExpireDate());
			}
		}

		success = iter.GetNextSibling(&iter);
	}

#if !__NO_OUTPUT
	if (HasUnseenPriorityNews())
	{
		gnetDebug1("There are new news the user hasn't seen yet");
	}
#endif
}

bool CNetworkNewsStoryMgr::HasCloudRequestCompleted()
{
	return m_CloudRequestCompleted;
}

#if !__NO_OUTPUT
void CNetworkNewsStoryMgr::ReadNewsFromFile(const char* filePath)
{
	const fiDevice* device = nullptr;
	fiHandle handle = fiHandleInvalid;
	char* buffer = nullptr;

	rtry
	{
		device = fiDevice::GetDevice(filePath, true);
		rcheck(device, catchall, gnetError("No device for %s", filePath));

		handle = device->Open(filePath, true);
		rcheck(fiIsValidHandle(handle), catchall, gnetError("News file not found %s", filePath));

		u64 len = device->GetFileSize(filePath);
		buffer = rage_new char[len + 1];
		buffer[len] = 0;

		int num = device->Read(handle, buffer, (int)len);
		rcheck(num == (int)len, catchall, gnetError("Failed to read data for %s", filePath));

		gnetDebug1("Read local news file");
		ParseNews(buffer, num);
	}
	rcatchall
	{
	}

	if (device != nullptr && fiIsValidHandle(handle))
	{
		device->Close(handle);
	}

	if (buffer != nullptr)
	{
		delete[] buffer;
		buffer = nullptr;
	}
}
#endif

// Mini hash to store as unsigned together with the impression count
unsigned StoryKeyToProfileHash(const char* storyKey)
{
	return atStringHash(storyKey) & 0xfffffff0;
}

int GetRequiredImpressions(unsigned /*hash*/)
{
	// This isn't great because there's a good chance the tunable isn't downloaded yet but for now it'll have to do.
	// Can be changed once we added a field to the post.
	return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NEWS_IMPRESSIONS", 0xD0C9083B), 1);
}

// Save the impression of a post
bool StoreImpression(int idx, const unsigned story, const int count)
{
	CProfileSettings& settings = CProfileSettings::GetInstance();

	if (!settings.AreSettingsValid())
	{
		return false;
	}

	const unsigned num = (CProfileSettings::SC_PRIO_SEEN_LAST - CProfileSettings::SC_PRIO_SEEN_FIRST) + 1;
	idx = idx < 0 ? 0 : (idx % num);

	unsigned countval = rage::Min(count, 0xf);
	unsigned guidval = (story & 0xfffffff0);
	unsigned combi = guidval | countval;

	settings.Set(static_cast<CProfileSettings::ProfileSettingId>(CProfileSettings::SC_PRIO_SEEN_FIRST + idx), static_cast<int>(combi));

	return true;
}

// Get the impression of an index
bool GetImpression(int idx, unsigned& story, int& count)
{
	CProfileSettings& settings = CProfileSettings::GetInstance();

	if (!settings.AreSettingsValid())
	{
		return false;
	}

	const unsigned num = (CProfileSettings::SC_PRIO_SEEN_LAST - CProfileSettings::SC_PRIO_SEEN_FIRST) + 1;
	idx = idx < 0 ? 0 : (idx % num);

	const unsigned a = static_cast<unsigned>(settings.GetInt(static_cast<CProfileSettings::ProfileSettingId>(CProfileSettings::SC_PRIO_SEEN_FIRST + idx)));

	story = (a & 0xfffffff0);
	count = static_cast<int>(static_cast<u32>(a) & 0xf);

	return true;
}

bool CNetworkNewsStoryMgr::HasUnseenPriorityNews() const
{
#if !__NO_OUTPUT
	int val = 0;
	if (PARAM_scprioritynews.Get(val))
	{
		if (val == 0)
		{
			return false;
		}

		const int prio = (int)NEWS_TYPE_PRIORITY_HASH;
		return (GetNumStories(prio) > 0); // scprioritynews=1 still expects there is any priority news
	}
#endif

	const int num = GetNumStories();

	for (int i = 0; i < num; ++i)
	{
		const CNetworkSCNewsStoryRequest * const story = GetStoryAtIndex(i);

		if (story == nullptr || !IsPriorityNews(*story))
		{
			continue;
		}

		if (!HasSeenNewsStory(*story))
		{
			return true;
		}
	}

	return false;
}

bool CNetworkNewsStoryMgr::IsPriorityNews(const CNetworkSCNewsStoryRequest& news)
{
	return news.HasType(NEWS_TYPE_PRIORITY_HASH);
}

void CNetworkNewsStoryMgr::NewsStorySeen(const CNetworkSCNewsStoryRequest& news)
{
	CProfileSettings& settings = CProfileSettings::GetInstance();

	if (!settings.AreSettingsValid())
	{
		return;
	}

	if (!IsPriorityNews(news))
	{
		return; // No need to store non-priority news
	}

	int hash = StoryKeyToProfileHash(news.GetStoryKey());
	int requiredImpressions = GetRequiredImpressions(hash);
	
	if (requiredImpressions == 0)
	{
		gnetDebug3("News - It's a 'show always'. key[%s]", news.GetStoryKey());
		return;
	}

	int slotIdx = -1;
	int impressionCount = GetNewsStoryImpressionCount(hash, slotIdx);
	if (impressionCount >= requiredImpressions)
	{
		gnetDebug3("News already seen [%d] times. key[%s]", impressionCount, news.GetStoryKey());
		return;
	}

	const unsigned num = (CProfileSettings::SC_PRIO_SEEN_LAST - CProfileSettings::SC_PRIO_SEEN_FIRST) + 1;

	unsigned idx = 0;

	if (slotIdx < 0) // find any empty or no longer needed slot
	{
		idx = static_cast<unsigned>(settings.GetInt(CProfileSettings::SC_PRIO_SEEN_NEXT_SLOT)) % num;

		// There's at most 8 so this double loop is fine. Used to avoid overwriting old posts.
		for (unsigned i = 0; i < num; ++i)
		{
			bool slotUsed = false;
			unsigned impressionHash = 0;
			int count = 0;

			GetImpression(i, impressionHash, count);

			if (impressionHash == 0)
			{
				break;
			}

			for (unsigned j = 0; j < GetNumStories(); ++j)
			{
				unsigned osh = StoryKeyToProfileHash(GetStoryAtIndex(j)->GetStoryKey());

				if (impressionHash == osh)
				{
					slotUsed = true; // The slot is occupied by an existing post
					break;
				}
			}

			if (!slotUsed)
			{
				break;
			}

			idx = (idx + 1) % num;
		}
	}
	else
	{
		idx = slotIdx;
	}

	gnetDebug1("News Seen Tracked key[%s] count[%u] idx[%u]", news.GetStoryKey(), impressionCount + 1, idx);

	if (StoreImpression(idx, hash, impressionCount + 1))
	{
		settings.Set(CProfileSettings::SC_PRIO_SEEN_NEXT_SLOT, static_cast<int>((idx + 1) % num));
	}
}

bool CNetworkNewsStoryMgr::HasSeenNewsStory(const CNetworkSCNewsStoryRequest& news) const
{
	unsigned hash = StoryKeyToProfileHash(news.GetStoryKey());
	const s64 requiredImpressions = GetRequiredImpressions(hash);

	if (requiredImpressions == 0)
	{
		return true;
	}

	if (!IsPriorityNews(news))
	{
		return true; // For now all non-priority news count as seen
	}

	int slotIdx = -1;
	const int impressionCount = GetNewsStoryImpressionCount(hash, slotIdx);

	return impressionCount >= requiredImpressions;
}

int CNetworkNewsStoryMgr::GetNewsStoryImpressionCount(unsigned hash, int& slotIdx) const
{
	slotIdx = -1;

	const CProfileSettings& settings = CProfileSettings::GetInstance();

	if (!settings.AreSettingsValid())
	{
		return 0;
	}

	const unsigned num = (CProfileSettings::SC_PRIO_SEEN_LAST - CProfileSettings::SC_PRIO_SEEN_FIRST) + 1;

	for (unsigned i = 0; i < num; ++i)
	{
		unsigned impressionHash = 0;
		int count = 0;

		if (GetImpression(i, impressionHash, count) && hash == impressionHash)
		{
			slotIdx = i;
			return count;
		}
	}

	return 0;
}

unsigned CNetworkNewsStoryMgr::GetNewsSkipDelaySec()
{
#if !__NO_OUTPUT
	unsigned delaySec = 0;

	if (PARAM_scnewsskipdelaysec.Get(delaySec))
	{
		return delaySec;
	}
#endif

	if (!HasUnseenPriorityNews())
	{
		// The user has already seen all news once so don't bother him
		return 0;
	}

	// This is what we decided to do. This may not be flexible enough but for now it will do.
	return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NEWS_SKIP_DELAY_SEC", 0x74395B5A), 0);
}

void CNetworkNewsStoryMgr::OnCloudEvent( const CloudEvent* pEvent )
{
	switch(pEvent->GetType())
	{
	case CloudEvent::EVENT_REQUEST_FINISHED:
		{
			// grab event data
			const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

			// check if we're interested
			if(pEventData->nRequestID != m_cloudReqID)
				return;

			if (pEventData->bDidSucceed && pEventData->nDataSize > 10)
			{
				gnetDebug2("Received cloud news set from cloud file");
				ParseNews((const char*)pEventData->pData, pEventData->nDataSize);
			}

			m_CloudRequestCompleted = true;
			m_cloudReqID = -1;
		}
		break;
	case CloudEvent::EVENT_AVAILABILITY_CHANGED:
		{
			const CloudEvent::sAvailabilityChangedEvent* pEventData = pEvent->GetAvailabilityChangedData();
			if (pEventData && pEventData->bIsAvailable)
			{
				RequestDownloadCloudFile();
			}
		}
		break;
	}
}


void CNetworkNewsStoryMgr::Update()
{
	if (PARAM_noSCNews.Get())
	{
		return;
	}

	if (m_bRequestCloudFile)
	{
		DoCloudFileRequest();
	}

	for (int i = 0; i < m_requestArray.GetCount(); ++i)
	{
		CNetworkSCNewsStoryRequest* pUpdate = m_requestArray[i];
		pUpdate->Update();
	}

	//Clean up NULLs
	for (int removeIter = m_requestArray.GetCount() - 1; removeIter >= 0; removeIter--)
	{
		if (m_requestArray[removeIter] == NULL)
		{
			//Delete is from the list (but RETAIN the order)
			m_requestArray.Delete(removeIter);
		}
	}
}

int CNetworkNewsStoryMgr::GetIndexOfNextByType(int startIndex, u32 typeHash, int direction, bool bIdleOnly ) const
{
	int numNewsStories = m_requestArray.GetCount();
	if (numNewsStories == 0)
	{
		return -1;
	}
	
	gnetAssert(direction == 1 || direction == -1);
	for(int i = 0; i < numNewsStories; i++)
	{
		int indexToCheck = (startIndex + (i * direction) + direction) % numNewsStories;
		
		if(indexToCheck < 0)
			indexToCheck = numNewsStories + indexToCheck;
		
		//Insanity handling so we don't crash 
		if(!gnetVerify(indexToCheck >= 0 && indexToCheck < m_requestArray.GetCount()))
		{
			return -1;
		}

		//Check to make sure the type is correct.
		bool bIsCorrectType = (m_requestArray[indexToCheck]->HasType(typeHash) || typeHash == 0);
		
		//If we only care about idle ones, check if it's idle.
		bool bIsStateGood = !bIdleOnly || m_requestArray[indexToCheck]->IsIdle();
		
		if ( bIsCorrectType && bIsStateGood)
		{
			return indexToCheck;
		}
	}

	return -1;
}

int CNetworkNewsStoryMgr::GetNumStories(int newsTypeHash) const
{
	int iNumStories = 0;

	for(int i = 0; i < GetNumStories(); ++i)
	{
		if(GetStoryAtIndex(i)->HasType(newsTypeHash))
		{
			++iNumStories;
		}
	}

	return iNumStories;
}

int CNetworkNewsStoryMgr::GetIndexByType(int newsTypeHash, int iRequestIndex)
{
	int iStoryCount = 0;

	for(int i = 0; i < GetNumStories(); ++i)
	{
		if(i == iRequestIndex)
		{
			break;
		}
		else if(GetStoryAtIndex(i)->HasType(newsTypeHash))
		{
			++iStoryCount;
		}
	}

	return iStoryCount;
}

void CNetworkNewsStoryMgr::Init()
{
}

CNetworkSCNewsStoryRequest* CNetworkNewsStoryMgr::GetStoryAtIndex(int index) const
{
	if(index < m_requestArray.GetCount() && index >= 0)
	{
		CNetworkSCNewsStoryRequest* pNewsItem = m_requestArray[index];
		return pNewsItem;
	}

	return NULL;
}

// ******************************************************************************************************
// ** Reading Utils
// ******************************************************************************************************
#define AVG_ADULT_READING_WPM	(250)										// Average Adult Reading WPM is between 250-300
#define AVG_ADULT_READING_MSPW	(60.0f / AVG_ADULT_READING_WPM * 1000)		// MSPW -- Milliseconds Per Word

u32 ReadingUtils::GetWordCount(const char* cText)
{
	u32 uWordCount = 1;

	if(AssertVerify(cText))
	{
		bool bLastCharWasSpace = false;

		while(*cText)
		{
			if(*cText == ' ')
			{
				// Don't process multiple spaces in a row as multiple words
				if(!bLastCharWasSpace)
				{
					++uWordCount;
				}
			}
			else
			{
				bLastCharWasSpace = false;
			}

			++cText;
		}
	}

	return uWordCount;
}

u32 ReadingUtils::GetAverageAdultTimeToRead(const char* cText)
{
	return (u32)(GetWordCount(cText) * AVG_ADULT_READING_MSPW);
}

// ******************************************************************************************************
// ** CNetworkTransitionNewsController
// ******************************************************************************************************
CNetworkTransitionNewsController& CNetworkTransitionNewsController::Get()
{
	typedef atSingleton<CNetworkTransitionNewsController> CNetworkTransitionNewsController;
	if (!CNetworkTransitionNewsController::IsInstantiated())
	{
		CNetworkTransitionNewsController::Instantiate();
	}

	return CNetworkTransitionNewsController::GetInstance();
}

bool CNetworkTransitionNewsController::SkipCurrentNewsItem()
{
	if( !ShouldAutoCycleStories() )
	{
		// Hide the currently displaying news item only if we're not cycling news
		ClearNewsDisplay();
	}
	else
	{
		ClearPendingStory();
		m_iPendingRequestID = -1;
	}

	// Start loading the next news item
	return ReadyNextRequest(true);
}

void CNetworkTransitionNewsController::UpdateDisplayConfig()
{
	CScaleformMgr::SetMovieDisplayConfig(m_newsMovieID, eSCALEFORM_BASE_CLASS::SF_BASE_CLASS_GENERIC, CScaleformMgr::SDC::UseFakeSafeZoneOnBootup);
	CScaleformMgr::UpdateMovieParams(m_newsMovieID, CScaleformMgr::GetRequiredScaleMode(m_newsMovieID, true));
}

void CNetworkTransitionNewsController::SetAlignment(bool bAlignRight)
{
	if (CScaleformMgr::BeginMethod(m_newsMovieID, SF_BASE_CLASS_GENERIC, "SET_ALIGNMENT_TYPE"))
	{
		CScaleformMgr::AddParamBool(bAlignRight);
		CScaleformMgr::EndMethod();
	}
}

u32 CNetworkTransitionNewsController::GetSeenNewsMask() const
{
	return m_seenNewsMask;
}

void CNetworkTransitionNewsController::ClearSeenNewsMask()
{
	m_seenNewsMask = 0;
}

u32 CNetworkTransitionNewsController::GetActiveNewsType() const
{
	return IsPriorityMode() ? NEWS_TYPE_PRIORITY_HASH : NEWS_TYPE_TRANSITION_HASH;
}

bool CNetworkTransitionNewsController::ReadyNextRequest( bool const requestShow )
{
	bool const c_result = ReadyRequest( 1 );
	m_bRequestShow = requestShow;
	return c_result;
}

bool CNetworkTransitionNewsController::ReadyPreviousRequest(bool const requestShow)
	{
    bool const c_result = ReadyRequest( -1 );
    m_bRequestShow = requestShow;
    return c_result;
	}

bool CNetworkTransitionNewsController::ReadyRequest( int const delta )
{
    bool result = false;

    if ( GetNewsItemCount() != 0)
    {
        if(!HasPendingStory())
        {
            m_requestDelta = delta;
        }

        m_bRequestShow = false;
        result = true;
	}

    return result;
}

int CNetworkTransitionNewsController::GetNewsItemCount() const
{
    int const c_itemCount = CNetworkNewsStoryMgr::Get().GetNumStories(GetActiveNewsType());
    return c_itemCount;
}

bool CNetworkTransitionNewsController::InitControl( s32 const movieID, eMode const interactivityMode )
{
	bool bSuccess = false;
	
	uiDebugf3("Initializing Transition News Controller");
    if( uiVerifyf( m_newsMovieID == INVALID_MOVIE_ID, "News movie already in use" ) )
    {
        //  Only start news if we've got a valid movie
        if( uiVerify(CScaleformMgr::IsMovieActive(movieID) ) )
        {
            m_interactivityMode = interactivityMode;

            int numTransitionNews = CNetworkNewsStoryMgr::Get().GetNumStories(GetActiveNewsType());
            if(numTransitionNews > 0)
            {
                m_newsMovieID = movieID;

                if (!IsLegacyMode())
                {
                    // This reset skips some of the frequency checks that would cause sorting issues on the landing page
                    ResetPrioritizedNews();
                }

                UpdatePrioritizedNewsList();

                if( IsLegacyMode() )
                {
                    // Immediately kick off first request if we haven't made one yet
                    if(!HasPendingStory())
                    {
                        ReadyNextRequest(true);
                    }
                    else	// We've got a story queued, request to show what we have queued
                    {
                        m_bRequestShow = true;
                    }
                }
                else
                {
                    // Request all items upfront in non-legacy modes
                    ReadyAllRequests();
                    ReadyNextRequest( true );
                }

                m_uLastDisplayedTimeMs = fwTimer::GetSystemTimeInMilliseconds();

                // Setup number of tabs
                if(CScaleformMgr::BeginMethod(m_newsMovieID, SF_BASE_CLASS_GENERIC, "SETUP_TABS"))
                {
                    // Legacy GTAV code only set this in the loading screen context
                    bool const c_alignRight = m_interactivityMode == MODE_CODE_LOADING_SCREEN && CHudTools::GetWideScreen();

                    CScaleformMgr::AddParamInt(numTransitionNews);
                    CScaleformMgr::AddParamBool( c_alignRight );
                    CScaleformMgr::EndMethod(true);	// Invoke this instantly to avoid situations like url:bugstar:1952936, where functions may somehow occur out of order during bootup and the render thread is but a fetus in the womb

                    bSuccess = true;
                }

                SetPaused(false);
                m_bIsActive = true;
            }
        }
    }

	return bSuccess;
}

void CNetworkTransitionNewsController::SetPaused(bool bPause)
{
	if(m_bIsPaused != bPause)
	{
		// If we're active, fade/in out the news
		if(IsActive())
		{
			CScaleformMgr::CallMethod(m_newsMovieID, SF_BASE_CLASS_GENERIC, bPause ? "FADE_OUT_BIGFEED" : "FADE_IN_BIGFEED");
		}
		m_bIsPaused = bPause;
	}
}

void CNetworkTransitionNewsController::MarkPendingStoryAsShown()
{
	if(GetPendingStory())
		GetPendingStory()->MarkAsShown(); // Mark as shown, prepare it for cleanup
}

void CNetworkTransitionNewsController::ClearPendingStory()
{
	if(GetPendingStory())
		GetPendingStory()->MarkForCleanup(); // Clean up immediately
}

int CNetworkTransitionNewsController::GetNextIDByPriority()
{
	return GetIDByPriority( 1 );
}

int CNetworkTransitionNewsController::GetPreviousIDByPriority()
{
    return GetIDByPriority( -1 );
}

int CNetworkTransitionNewsController::GetIDByPriority(int const delta) 
{
    int id = m_iLastPriorityID + delta;

    if( m_iLastPriorityID != id )
    {
        int const c_newsItemCount = m_prioritizedNews.GetCount();
        bool const c_hasNoItems = c_newsItemCount == 0;
        bool const c_isOverflowingEnd = id >= c_newsItemCount;

        // We only want the list regeneration in legacy modes.
        // In modern modes, we generate the list once and the player can only iterate through
        // those
        if( c_hasNoItems || ( c_isOverflowingEnd && IsLegacyMode() ) )
        {
            BANK_ONLY( Bank_ClearPrioritizedNewsListDisplay() );

            m_prioritizedNews.GenerateNextList();
            m_iLastPriorityID = id = 0;

            BANK_ONLY( Bank_FillPrioritizedNewsListDisplay() );
        }
        else
        {
            id = rage::Clamp( id, 0, c_newsItemCount-1 );
        }
	}

    return id;
}

void CNetworkTransitionNewsController::Update()
{
	if(!IsPaused())
	{
		if(m_uLastDisplayedTimeMs != 0 && ShouldAutoCycleStories() && (m_uLastDisplayedTimeMs + m_uStoryOnscreenDuration <= fwTimer::GetSystemTimeInMilliseconds()) )
		{
			const CNetworkSCNewsStoryRequest* prev = GetLastShownStory();

			if (prev == nullptr || prev->IsIdle())
			{
				ReadyNextRequest(true);
				m_uLastDisplayedTimeMs = 0;
			}
		}

		if( m_requestDelta != 0 )
		{
			//Make a request for the next story (get the index for the next idle one)
			m_iLastPriorityID = GetIDByPriority( m_requestDelta );
			int nextRequestID = m_prioritizedNews.GetItemUIDAt(m_iLastPriorityID);

#if __BANK
			if(ms_iDebugStoryRequestID != -1)
			{
				// Override the next request with the current debug request ID
				nextRequestID = ms_iDebugStoryRequestID;
			}
#endif

			// Don't try and re-request the same pending request
			if(nextRequestID != m_iPendingRequestID && nextRequestID != m_iLastShownID)
			{

				if(HasPendingStory())
				{
					ClearPendingStory(); // Clear previous pending story
				}

				MakeStoryRequestAt(nextRequestID);
			}

			m_requestDelta = 0;
		}
		else if(HasValidNewsMovie() && m_bRequestShow && IsPendingStoryReady())
		{
			CNetworkSCNewsStoryRequest* newsItem = GetPendingStory();
			if( ShouldSkipScriptNews() && IsNewsStoryScriptDriven(newsItem))
			{
				SkipCurrentNewsItem();
			}
			else
			{
				PostNews(newsItem, true);

				// Set the on screen duration of the story just posted
				m_uStoryOnscreenDuration = MAX((ReadingUtils::GetAverageAdultTimeToRead(newsItem->GetHeadline()) + 
												ReadingUtils::GetAverageAdultTimeToRead(newsItem->GetSubtitle()) + 
												ReadingUtils::GetAverageAdultTimeToRead(newsItem->GetContent())), TRANSITION_STORY_MINIMUM_ONSCREEN_DURATION);

				MarkPendingStoryAsShown(); // Mark as shown so the texture is cleaned up

				m_iPendingRequestID = -1;

				if(ShouldAutoCycleStories())
				{	
					ReadyNextRequest(false);	// Queue up next story
				}
			}
		}
	}
}

bool CNetworkTransitionNewsController::IsNewsStoryScriptDriven(const CNetworkSCNewsStoryRequest* newsItem) const
{
	return newsItem && newsItem->HasExtraDataMember("scriptDisplay");
}

void CNetworkTransitionNewsController::ReadyAllRequests()
{
    int count = m_prioritizedNews.GetCount();
    if( count == 0 )
    {
        m_prioritizedNews.GenerateNextList();
        count = m_prioritizedNews.GetCount();
    }

    // Done in reverse so when the data is displayed we show the last loaded one which is the first one in the list
    for( int index = count - 1; index >= 0; --index )
    {
        const int itemId = m_prioritizedNews.GetItemUIDAt(index);
        MakeStoryRequestAt( itemId );
    }
}

void CNetworkTransitionNewsController::ResetPrioritizedNews()
{
    m_iLastPriorityID = 0;
    m_prioritizedNews.Clear();
}

void CNetworkTransitionNewsController::UpdatePrioritizedNewsList()
{
	// Update our prioritized news entries in case there have been any late additions to the news system
	CNetworkNewsStoryMgr& newsMgr = CNetworkNewsStoryMgr::Get();
	int const c_count = newsMgr.GetNumStories();
	u32 const c_newsType = GetActiveNewsType();

	for(int i = 0; i < c_count; ++i)
	{
		if(!m_prioritizedNews.HasEntry(i))
		{
			CNetworkSCNewsStoryRequest* newsItem = newsMgr.GetStoryAtIndex(i);
			if(newsItem->HasType(c_newsType))
			{
				int const c_priority =  newsItem->GetPriority();
				int const c_secondPriority = newsItem->GetSecondaryPriority();

				if( m_prioritizedNews.AddEntry(i, c_priority, c_secondPriority ) )
				{
					// We've already generated a list, let's update it
					if(m_prioritizedNews.GetCount() > 0)
					{
						m_prioritizedNews.InsertEntryIntoList(i, m_iLastPriorityID);
					}
				}
		    }
	    }
    }
}

bool CNetworkTransitionNewsController::MakeStoryRequestAt(int iRequestedID)
{
	bool bRequestSuccess = false;
	if(iRequestedID >= 0)
	{
		CNetworkSCNewsStoryRequest* newsItem = CNetworkNewsStoryMgr::Get().GetStoryAtIndex(iRequestedID);
		if(newsItem)
		{
            if( newsItem->IsIdle() )
            {
                newsItem->Request(true);
            }

            m_iPendingRequestID = iRequestedID;
            m_iLastRequestedID = iRequestedID;
			bRequestSuccess = true;
		}
	}

	return bRequestSuccess;
}

bool CNetworkTransitionNewsController::PostNews(const CNetworkSCNewsStoryRequest* pNewsItem, bool bPostImg)
{
	if(CScaleformMgr::IsMovieActive(m_newsMovieID))
	{
		// Send data to scaleform
		if(CScaleformMgr::BeginMethod(m_newsMovieID, SF_BASE_CLASS_GENERIC, "SET_BIGFEED_INFO"))
		{
			CScaleformMgr::AddParamString(pNewsItem->GetHeadline());
			CScaleformMgr::AddParamString(pNewsItem->GetContent(), false);
			CScaleformMgr::AddParamInt(0);	// selected tab index -- deprecated

            char const * const c_txdName = bPostImg ? pNewsItem->GetTxdName() : "";
			CScaleformMgr::AddParamString( c_txdName );
            CScaleformMgr::AddParamString( c_txdName );

			CScaleformMgr::AddParamString(pNewsItem->GetSubtitle());
            CScaleformMgr::AddParamString( "" ); // url -- deprecated
			CScaleformMgr::AddParamString(pNewsItem->GetTitle());
			CScaleformMgr::AddParamInt(static_cast<int>(pNewsItem->GetStyle().GetHash()));
			CScaleformMgr::EndMethod();

			m_iLastShownID = m_iPendingRequestID;
			m_uLastDisplayedTimeMs = fwTimer::GetSystemTimeInMilliseconds();

			// We only count transition news. That also covers priority.
			if (pNewsItem->HasType(NEWS_TYPE_TRANSITION_HASH))
			{
				// We track the seen bits as listed in m_prioritizedNews, not as in the full list.
				for (int i=0; i < m_prioritizedNews.GetCount(); ++i)
				{
					const int newsId = m_prioritizedNews.GetItemUIDAt(i);

					if (newsId == m_iLastShownID)
					{
						m_seenNewsMask |= (1u << (u32)i);
						break;
					}
				}
			}

			if (!IsLegacyMode())
			{
				CNetworkNewsStoryMgr::Get().NewsStorySeen(*pNewsItem);
			}

			m_bIsDisplayingNews = true;

			return true;

		}
	}

	uiDebugf3("Failed to post transition news story at index %d", m_iPendingRequestID);

	return false;
}

void CNetworkTransitionNewsController::ClearNewsDisplay()
{
	if(CScaleformMgr::IsMovieActive(m_newsMovieID))
	{
		CScaleformMgr::CallMethod(m_newsMovieID, SF_BASE_CLASS_GENERIC, "HIDE_BIGFEED_INFO");
	}
}

void CNetworkTransitionNewsController::Reset()
{
	uiDebugf3("CNetworkTransitionNewsController::Reset -- Resetting Transition News Controller");

	if(HasValidNewsMovie() && CScaleformMgr::IsMovieActive(m_newsMovieID))
	{
		// If this isn't hit, that means script called EndNews() without calling BeginNews() or has unloaded the movie already before calling this
		CScaleformMgr::CallMethod(m_newsMovieID, SF_BASE_CLASS_GENERIC, "END_BIGFEED");
	}

	if(HasPendingStory())
	{
		ClearPendingStory();
	}

	m_bIsActive = false;
	m_requestDelta = 0;
	m_bRequestShow = false;
	m_bIsDisplayingNews = false;
	m_iPendingRequestID = -1;
	m_iLastShownID = -1;
	m_newsMovieID = SF_INVALID_MOVIE;
	m_uLastDisplayedTimeMs = 0;
	m_uStoryOnscreenDuration = 0;
}

CNetworkSCNewsStoryRequest* CNetworkTransitionNewsController::GetPendingStory()
{
	return CNetworkNewsStoryMgr::Get().GetStoryAtIndex(m_iPendingRequestID);
}

CNetworkSCNewsStoryRequest* CNetworkTransitionNewsController::GetLastShownStory()
{
	return CNetworkNewsStoryMgr::Get().GetStoryAtIndex(m_iLastShownID);
}

bool CNetworkTransitionNewsController::IsPendingStoryDataRcvd()
{
	return HasPendingStory() && GetPendingStory()->ReceivedData();
}

bool CNetworkTransitionNewsController::IsPendingStoryReady()
{
	return HasPendingStory() && GetPendingStory()->IsReady();
}

bool CNetworkTransitionNewsController::HasPendingStoryFailed()
{
	return HasPendingStory() && GetPendingStory()->Failed();
}

bool CNetworkTransitionNewsController::HasPendingStory()
{
	return m_iPendingRequestID != -1 && GetPendingStory();
}

bool CNetworkTransitionNewsController::CurrentNewsItemHasExtraData()
{
	CNetworkSCNewsStoryRequest* pLastShownNewsItem = GetLastShownStory();
	return pLastShownNewsItem && pLastShownNewsItem->HasExtraNewsData();
}

bool CNetworkTransitionNewsController::GetCurrentNewsItemExtraData(const char* name, int& value)
{
	CNetworkSCNewsStoryRequest* pLastShownNewsItem = GetLastShownStory();
	return pLastShownNewsItem && pLastShownNewsItem->GetExtraNewsData(name, value);
}

bool CNetworkTransitionNewsController::GetCurrentNewsItemExtraData(const char* name, float& value)
{
	CNetworkSCNewsStoryRequest* pLastShownNewsItem = GetLastShownStory();
	return pLastShownNewsItem && pLastShownNewsItem->GetExtraNewsData(name, value);
}

bool CNetworkTransitionNewsController::GetCurrentNewsItemExtraData(const char* name, char* str, int maxStr)
{
	CNetworkSCNewsStoryRequest* pLastShownNewsItem = GetLastShownStory();
	return pLastShownNewsItem && pLastShownNewsItem->GetExtraNewsData(name, str, maxStr);
}

const char* CNetworkTransitionNewsController::GetCurrentNewsItemUrl()
{
    CNetworkSCNewsStoryRequest* pLastShownNewsItem = GetLastShownStory();
    return pLastShownNewsItem ? pLastShownNewsItem->GetURL() : "";
}

u32 CNetworkTransitionNewsController::GetCurrentNewsItemTrackingId()
{
    CNetworkSCNewsStoryRequest* pLastShownNewsItem = GetLastShownStory();
    return pLastShownNewsItem ? pLastShownNewsItem->GetTrackingId() : 0;
}

bool CNetworkTransitionNewsController::HasCurrentNewsItemDataOrIdle()
{
    CNetworkSCNewsStoryRequest* pLastShownNewsItem = GetLastShownStory();
    return pLastShownNewsItem ? (pLastShownNewsItem->ReceivedData() || pLastShownNewsItem->IsIdle()) : false;
}

// ******************************************************************************************************
// ** PrioritizedList
// ******************************************************************************************************

bool PrioritizedList::AddEntry( int uID, int iPriority, int iSecondaryPriority )
{
	const int INVALID_SECONDARY_PRIORITY = -1;
	int iIndexToInsert = 0;

	// Check if we've already added the item, and at the same time begin insertion sort
	for(int i = 0; i < entries.GetCount(); ++i)
	{
		if(!gnetVerifyf(entries[i].uID != uID, "PrioritizedList::AddEntry -- Attempting to add item uID %d that already exists in this prioritized list", uID))
		{
			return false;
		}

		// Sort in ascending order
		if(iPriority >= entries[i].iPriority)
		{
			iIndexToInsert = i + 1;
		}
	}

	// Do a secondary sort by secondary priority if it exists
	if(iSecondaryPriority != INVALID_SECONDARY_PRIORITY)
	{
		for(int i = 0; i < entries.GetCount(); ++i)
		{
			if(iPriority == entries[i].iPriority)
			{
				if(entries[i].iSecondaryPriority == INVALID_SECONDARY_PRIORITY)
				{
					iIndexToInsert = i;
				}
				else
				{
					if(iSecondaryPriority >= entries[i].iSecondaryPriority)
					{
						iIndexToInsert = i + 1;
					}
					else
					{
						iIndexToInsert = i;
					}
				}
			}

			if(entries[i].iPriority > iPriority)
			{
				break;
			}
		}
	}

	PriorityItem item;
	item.uID = uID;
	item.iPriority = iPriority;
	item.iCounter = 0;
	item.iSecondaryPriority = iSecondaryPriority;

	if(iIndexToInsert == entries.GetCount())
	{
		entries.PushAndGrow(item);
	}
	else
	{
		entries.insert(&entries[iIndexToInsert], 1, item);
	}

	return true;
}

bool PrioritizedList::HasEntry( int uID ) const
{
	for(int i = 0; i < entries.GetCount(); ++i)
	{
		if(entries[i].uID == uID)
		{
			return true;
		}
	}

	return false;
}

PrioritizedList::PriorityItem* PrioritizedList::GetEntry(int uID)
{
	for(int i = 0; i < entries.GetCount(); ++i)
	{
		if(entries[i].uID == uID)
		{
			return &entries[i];
		}
	}

	return NULL;
}

int PrioritizedList::GetItemUIDAt( int iIndex ) const
{
	if(gnetVerifyf(iIndex >= 0 && iIndex < generatedList.GetCount(), "PrioritizedList::GetItem -- Index %d out of range.  List contains %d elements", iIndex, entries.GetCount()))
	{
		return generatedList[iIndex];
	}

	return -1;
}

void PrioritizedList::SetPriorityFrequency( int iPriority, u8 iFrequency )
{
	frequencies[iPriority] = iFrequency;
}

void PrioritizedList::RefreshListForNewEntries()
{
	if(gnetVerifyf(entries.GetCount(), "PrioritizedList::RefreshListForNewEntries -- Attempting to RefreshListForNewEntries() but no items have been added via PrioritizedList::AddEntry()"))
	{
		// Clear list
		generatedList.Reset();

		for(int i = 0; i < entries.GetCount(); ++i)
		{
			int iFrequency = GetFrequency(entries[i].iPriority);

			// Only new entries contain a counter value of 0
			if(entries[i].iCounter == 0)
			{
				entries[i].iCounter = iFrequency;
			}

			if(entries[i].iCounter == iFrequency)
			{
				uiDisplayf("PrioritizedList::RefreshListForNewEntries -- Adding item uID = %d with priority = %d", entries[i].uID, entries[i].iPriority);
				// Add item to our final refreshed generated list
				generatedList.PushAndGrow(entries[i].uID);
			}
		}
	}
}

void PrioritizedList::GenerateNextList()
{
	uiDisplayf("PrioritizedList::GenerateNextList -- Generating Prioritized List");

	if(gnetVerifyf(entries.GetCount(), "PrioritizedList::GenerateNextList -- Attempting to GenerateNextList() but no items have been added via PrioritizedList::AddEntry()"))
	{
		// Clear previous list
		generatedList.Reset();

		while(generatedList.GetCount() == 0)
		{
			for(int i = 0; i < entries.GetCount(); ++i)
			{
				if(--entries[i].iCounter <= 0)
				{
					uiDisplayf("PrioritizedList::GenerateNextList -- Adding item uID = %d with priority = %d", entries[i].uID, entries[i].iPriority);
					// Reset counter
					entries[i].iCounter = GetFrequency(entries[i].iPriority);
					// Add item to our final generated list
					generatedList.PushAndGrow(entries[i].uID);
				}
			}
		}
	}
}

void PrioritizedList::InsertEntryIntoList(int uID, int iIndexToInsertAfter)
{
	if(gnetVerifyf(HasEntry(uID), "PrioritizedList::InsertEntryIntoList -- Attempting to insert entry that does not exist"))
	{
		PriorityItem* newEntry = GetEntry(uID);

		if(newEntry)
		{
			for(int i = iIndexToInsertAfter + 1; i < GetCount(); ++i)
			{
				PriorityItem* currEntry = GetEntry(generatedList[i]);
				if(currEntry && currEntry->iPriority <= newEntry->iPriority)
				{
					uiDebugf3("Inserting uid = %d after specified index %d into position %d", uID, iIndexToInsertAfter, i);
					generatedList.insert(&generatedList[i], 1, newEntry->uID);
					return;
				}
			}
			// We haven't found somewhere to place the new item, add item to the end of the list
			generatedList.PushAndGrow(newEntry->uID);
		}
	}
}

u8 PrioritizedList::GetFrequency( int iPriority )
{
	return frequencies[iPriority];
}

void PrioritizedList::Clear()
{
    generatedList.clear();
    entries.clear();
}

// ******************************************************************************************************
// ** CNetworkPauseMenuNewsController
// ******************************************************************************************************

CNetworkPauseMenuNewsController& CNetworkPauseMenuNewsController::Get()
{
	typedef atSingleton<CNetworkPauseMenuNewsController> CNetworkPauseMenuNewsController;
	if (!CNetworkPauseMenuNewsController::IsInstantiated())
	{
		CNetworkPauseMenuNewsController::Instantiate();
	}

	return CNetworkPauseMenuNewsController::GetInstance();
}

bool CNetworkPauseMenuNewsController::InitControl(u32 newsTypeHash)
{
	m_newsTypeHash = newsTypeHash;

	if(m_newsTypeHash == NEWS_TYPE_NEWSWIRE_HASH)
	{
		m_imageTypeToRequest = NEWS_IMG_TYPE_LANDSCAPE;
	}
	else if(m_newsTypeHash == NEWS_TYPE_STORE_HASH || m_newsTypeHash == NEWS_TYPE_ROCKSTAR_EDITOR_HASH
		|| m_newsTypeHash == NEWS_TYPE_STARTERPACK_OWNED_HASH || m_newsTypeHash == NEWS_TYPE_STARTERPACK_NOTOWNED_HASH)
	{
		m_imageTypeToRequest = NEWS_IMG_TYPE_PORTRAIT;
	}

	return RequestStory(1); // Kick off request immediately
}

bool CNetworkPauseMenuNewsController::RequestStory(int iDirection)
{
	int nextRequestID = CNetworkNewsStoryMgr::Get().GetIndexOfNextByType(m_iLastRequestedID, m_newsTypeHash, iDirection);

	if(nextRequestID != m_iLastRequestedID)
	{
		ClearPendingStory();
		return MakeStoryRequestAt(nextRequestID);
	}

	return false;
}

bool CNetworkPauseMenuNewsController::MakeStoryRequestAt(int iRequestedID)
{
	bool bRequestSuccess = false;
	if(iRequestedID >= 0)
	{
		CNetworkSCNewsStoryRequest* newsItem = CNetworkNewsStoryMgr::Get().GetStoryAtIndex(iRequestedID);
		if(newsItem)
		{
			newsItem->Request(true, m_imageTypeToRequest);

			m_iLastRequestedID = iRequestedID;
			
			bRequestSuccess = true;
		}
	}

	return bRequestSuccess;
}

bool CNetworkPauseMenuNewsController::IsPendingStoryReady() const
{
	return HasPendingStory() && GetPendingStory()->IsReady();
}

bool CNetworkPauseMenuNewsController::HasPendingStoryFailed()
{
	return HasPendingStory() && (GetPendingStory()->Failed());
}

int CNetworkPauseMenuNewsController::GetPendingStoryIndex()
{
	int iPendingStoryIndex = 0;
	if(HasPendingStory())
	{
		iPendingStoryIndex = CNetworkNewsStoryMgr::Get().GetIndexByType(m_newsTypeHash, m_iLastRequestedID);
	}

	return iPendingStoryIndex;
}

int CNetworkPauseMenuNewsController::GetNumStories()
{
	return CNetworkNewsStoryMgr::Get().GetNumStories(m_newsTypeHash);
}

bool CNetworkPauseMenuNewsController::IsPendingStoryDataRcvd()
{
	return HasPendingStory() && GetPendingStory()->ReceivedData();
}


bool CNetworkPauseMenuNewsController::HasPendingStory() const
{
	return m_iLastRequestedID != -1 && GetPendingStory();
}

CNetworkSCNewsStoryRequest* CNetworkPauseMenuNewsController::GetPendingStory() const
{
	return CNetworkNewsStoryMgr::Get().GetStoryAtIndex(m_iLastRequestedID);
}

void CNetworkPauseMenuNewsController::Reset()
{
	uiDebugf3("Resetting Pause Menu News Controller");

	ClearPendingStory();
	m_iLastRequestedID = -1;
}

void CNetworkPauseMenuNewsController::MarkPendingStoryAsShown()
{
	if(HasPendingStory())
	{
		GetPendingStory()->MarkAsShown();
	}
}

void CNetworkPauseMenuNewsController::ClearPendingStory()
{
	if(HasPendingStory())
	{
		GetPendingStory()->MarkForCleanup(); // Mark for cleanup immediately
	}
}

// ******************************************************************************************************
// ** RAG Widgets
// ******************************************************************************************************
#if __BANK
void CNetworkNewsStoryMgr::Empty()
{
	for (int i = 0; i < m_requestArray.GetCount(); i++)
	{
		if (m_requestArray[i])
		{
			m_requestArray[i]->Cancel();
			delete m_requestArray[i];
			m_requestArray[i] = NULL;
		}
	}

	m_requestArray.Reset();
}

char bank_NewsStoryAddItemKey[32] = "test";

void Bank_AddItem()
{
	CNewsItemPresenceEvent evt;
	CNewsItemPresenceEvent::TypeHashList typelist;
	typelist.Append() = "ticker";
	evt.Set(bank_NewsStoryAddItemKey, typelist);

	CNetworkNewsStoryMgr::Get().AddNewsStoryItem(evt);
}

void Bank_AddItemNewswire()
{
	CNewsItemPresenceEvent evt;
	CNewsItemPresenceEvent::TypeHashList typelist;
	typelist.Append() = "newswire";
	evt.Set(bank_NewsStoryAddItemKey, typelist);

	CNetworkNewsStoryMgr::Get().AddNewsStoryItem(evt);
}

void Bank_AddItemStore()
{
	CNewsItemPresenceEvent evt;
	CNewsItemPresenceEvent::TypeHashList typelist;
	typelist.Append() = "store";
	evt.Set(bank_NewsStoryAddItemKey, typelist);

	CNetworkNewsStoryMgr::Get().AddNewsStoryItem(evt);
}

void Bank_AddItemTransition()
{
    CNewsItemPresenceEvent evt;
    CNewsItemPresenceEvent::TypeHashList typelist;
    typelist.Append() = NEWS_TYPE_TRANSITION_HASH;
    evt.Set(bank_NewsStoryAddItemKey, typelist);

    CNetworkNewsStoryMgr::Get().AddNewsStoryItem(evt);
}

void Bank_AddItemPriority()
{
    CNewsItemPresenceEvent evt;
    CNewsItemPresenceEvent::TypeHashList typelist;
    typelist.Append() = NEWS_TYPE_PRIORITY_HASH;
    evt.Set(bank_NewsStoryAddItemKey, typelist);

    CNetworkNewsStoryMgr::Get().AddNewsStoryItem(evt);
}

void Bank_Clear()
{
	CNetworkNewsStoryMgr::Get().Empty();
}

#include "GamePresenceEvents.h"
void Bank_SendToInbox()
{
	CNewsItemPresenceEvent::Trigger_Debug(bank_NewsStoryAddItemKey, SendFlag_Inbox | SendFlag_Self);
}

void Bank_ClearNewsStoriesSeen()
{
	const unsigned num = (CProfileSettings::SC_PRIO_SEEN_LAST - CProfileSettings::SC_PRIO_SEEN_FIRST) + 1;

	for (int i = 0; i < num; ++i)
	{
		StoreImpression(i, 0, 0);
	}
}

void Bank_RequestPauseStoryStore()
{
	CNetworkPauseMenuNewsController::Get().InitControl(NEWS_TYPE_STORE_HASH);
}

void Bank_CyclePauseStory()
{
	CNetworkPauseMenuNewsController::Get().RequestStory(1);
}

void Bank_ClearPauseStory()
{
	CNetworkPauseMenuNewsController::Get().Reset();
}

static char s_StoryTitle[64] = {0};
static char s_StorySubtitle[64] = { 0 };
static char s_StoryContent[64] = { 0 };
static char s_StoryUrl[64] = { 0 };
static char s_StoryHeadline[64] = { 0 };
static char s_StoryTexture[64] = { 0 };
static int s_StoryCount = 0;

void Bank_ReadPauseStory()
{
	CNetworkPauseMenuNewsController& newsController = CNetworkPauseMenuNewsController::Get();
	if (newsController.IsPendingStoryReady())
	{
		CNetworkSCNewsStoryRequest* newsStory = newsController.GetPendingStory();
		safecpy(s_StoryTitle, newsStory->GetTitle());
		safecpy(s_StorySubtitle, newsStory->GetSubtitle());
		safecpy(s_StoryContent, newsStory->GetContent());
		safecpy(s_StoryUrl, newsStory->GetURL());
		safecpy(s_StoryHeadline, newsStory->GetHeadline());
		safecpy(s_StoryTexture, newsStory->GetTxdName());

		newsStory->MarkAsShown();
	}
	else
	{
		safecpy(s_StoryTitle, "not ready");
		s_StorySubtitle[0] = '\0';
		s_StoryContent[0] = '\0';
		s_StoryUrl[0] = '\0';
		s_StoryHeadline[0] = '\0';
		s_StoryTexture[0] = '\0';
	}

	s_StoryCount = CNetworkPauseMenuNewsController::Get().GetNumStories();
}

void CNetworkNewsStoryMgr::InitWidgets( bkBank* pBank )
{
	pBank->PushGroup( "Social Club News " );
	{
		pBank->AddText("Story Key", bank_NewsStoryAddItemKey, sizeof(bank_NewsStoryAddItemKey), false);
		pBank->AddButton("Add Item (as ticker)", CFA(Bank_AddItem));
		pBank->AddButton("Add Item (as newswire)", CFA(Bank_AddItemNewswire));
		pBank->AddButton("Add Item (as store)", CFA(Bank_AddItemStore));
        pBank->AddButton("Add Item (as transition)", CFA(Bank_AddItemTransition));
		pBank->AddButton("Add Item (as priority)", CFA(Bank_AddItemPriority));
		pBank->AddButton("Send to inbox", CFA(Bank_SendToInbox));
		pBank->AddSeparator();
		pBank->AddButton("Clear Stories", CFA(Bank_Clear));
		pBank->AddSeparator();
		pBank->AddButton("Clear Stories Seen (impressions)", CFA(Bank_ClearNewsStoriesSeen));
		pBank->AddSeparator();
		pBank->AddButton("Request Pause Store Story", CFA(Bank_RequestPauseStoryStore));
		pBank->AddButton("Read Pause Story", CFA(Bank_ReadPauseStory));
		pBank->AddButton("Cycle Pause Story", CFA(Bank_CyclePauseStory));
		pBank->AddButton("Reset Pause Story", CFA(Bank_ClearPauseStory));
		pBank->AddText("Title", s_StoryTitle, sizeof(s_StoryTitle), true);
		pBank->AddText("Subtitle", s_StorySubtitle, sizeof(s_StorySubtitle), true);
		pBank->AddText("Content", s_StoryContent, sizeof(s_StoryContent), true);
		pBank->AddText("Url", s_StoryUrl, sizeof(s_StoryUrl), true);
		pBank->AddText("Headline", s_StoryHeadline, sizeof(s_StoryHeadline), true);
		pBank->AddText("Texture", s_StoryTexture, sizeof(s_StoryTexture), true);
		pBank->AddText("Num Stories", &s_StoryCount, true);
	}
	pBank->PopGroup();
}

s32 CNetworkTransitionNewsController::ms_iDebugNewsMovieID = SF_INVALID_MOVIE;
bkList* CNetworkTransitionNewsController::ms_pFullNewsList = NULL;
bkList* CNetworkTransitionNewsController::ms_pPrioritizedNewsList = NULL;
int CNetworkTransitionNewsController::ms_iDebugStoryRequestID = -1;

void CNetworkTransitionNewsController::Bank_LoadDebugNewsMovie()
{
	ms_iDebugNewsMovieID = CScaleformMgr::CreateMovieAndWaitForLoad("GTAV_ONLINE", Vector2(0.0, 0.0), Vector2(1.0, 1.0));
}

void CNetworkTransitionNewsController::Bank_StopNewsDisplay()
{
	if(CNetworkTransitionNewsController::Get().IsActive())
	{
		// Reset control
		CNetworkTransitionNewsController::Get().Reset();
	}

	if(ms_iDebugNewsMovieID != SF_INVALID_MOVIE)
	{
		// Unload movie
		CScaleformMgr::RequestRemoveMovie(ms_iDebugNewsMovieID);
		ms_iDebugNewsMovieID = SF_INVALID_MOVIE;
	}

	ms_iDebugStoryRequestID = -1;
}

void CNetworkTransitionNewsController::Bank_BeginNewsCycle()
{
	if(CNetworkTransitionNewsController::Get().IsActive())
	{
		Bank_StopNewsDisplay();
	}

	// Load movie
	Bank_LoadDebugNewsMovie();
	// Give control
	CNetworkTransitionNewsController::Get().InitControl( ms_iDebugNewsMovieID, CNetworkTransitionNewsController::MODE_CODE_LOADING_SCREEN );
}

void CNetworkTransitionNewsController::Bank_RenderDebugMovie()
{
	if(CScaleformMgr::IsMovieActive(ms_iDebugNewsMovieID))
	{
		CScaleformMgr::RenderMovie(ms_iDebugNewsMovieID);
	}
}

void CNetworkTransitionNewsController::Bank_FillCompleteNewsList()
{
	if(ms_pFullNewsList)
	{
		CNetworkNewsStoryMgr& newsMgr = CNetworkNewsStoryMgr::Get();
		int newsCount = newsMgr.GetNumStories(NEWS_TYPE_TRANSITION_HASH);

		for(int i = 0; i < newsCount; ++i)
		{
			int requestID = newsMgr.GetIndexByType(NEWS_TYPE_TRANSITION_HASH, i);
			const CNetworkSCNewsStoryRequest* newsItem = newsMgr.GetStoryAtIndex(requestID);

			ms_pFullNewsList->AddItem(requestID, 0, newsItem->GetStoryKey());
			ms_pFullNewsList->AddItem(requestID, 1, newsItem->GetPriority());
		}
	}
}

void CNetworkTransitionNewsController::Bank_FillPrioritizedNewsListDisplay()
{
	if(ms_pPrioritizedNewsList)
	{
		for(int i = 0; i < m_prioritizedNews.GetCount(); ++i)
		{
			int requestID = m_prioritizedNews.GetItemUIDAt(i);
			const CNetworkSCNewsStoryRequest* newsItem = CNetworkNewsStoryMgr::Get().GetStoryAtIndex(requestID);

			ms_pPrioritizedNewsList->AddItem(requestID, 0, i);
			ms_pPrioritizedNewsList->AddItem(requestID, 1, newsItem->GetStoryKey());
			ms_pPrioritizedNewsList->AddItem(requestID, 2, newsItem->GetPriority());			
		}
	}
}

void CNetworkTransitionNewsController::Bank_ClearPrioritizedNewsListDisplay()
{
	if(ms_pPrioritizedNewsList)
	{
		for(int i = 0; i < m_prioritizedNews.GetCount(); ++i)
		{
			int requestID = m_prioritizedNews.GetItemUIDAt(i);
			ms_pPrioritizedNewsList->RemoveItem(requestID);
		}
	}
}

void CNetworkTransitionNewsController::Bank_ShowNewsItem(s32 key)
{
	if(!CNetworkTransitionNewsController::Get().IsActive())
	{
		Bank_LoadDebugNewsMovie();
		CNetworkTransitionNewsController::Get().InitControl( ms_iDebugNewsMovieID, CNetworkTransitionNewsController::MODE_CODE_INTERACTIVE );
	}

	// Find the request ID with this key
	ms_iDebugStoryRequestID = key;
	CNetworkTransitionNewsController::Get().ReadyNextRequest(true);
}

void CNetworkTransitionNewsController::Bank_CreateTransitionNewsWidgets()
{
	bkBank* pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);
	if( pBank)
	{
		pBank->PushGroup( "Transition News Controller " );
		{
			pBank->AddButton("Begin Cycling News", &Bank_BeginNewsCycle);
			pBank->AddButton("Stop News Display", &Bank_StopNewsDisplay);


			bkList::ClickItemFuncType newsItemClickCB;
			newsItemClickCB.Reset<&CNetworkTransitionNewsController::Bank_ShowNewsItem>();

			pBank->PushGroup( "All Transition News " );
			{
				ms_pFullNewsList = pBank->AddList("All Transition News");
				ms_pFullNewsList->AddColumnHeader(0, "Story Key", bkList::STRING);
				ms_pFullNewsList->AddColumnHeader(1, "Priority", bkList::INT);
				ms_pFullNewsList->SetSingleClickItemFunc(newsItemClickCB);
			}
			pBank->PopGroup();

			pBank->PushGroup( "Current Prioritized Transition News List " );
			{
				ms_pPrioritizedNewsList = pBank->AddList("Current Prioritized Transition News List");
				ms_pPrioritizedNewsList->AddColumnHeader(0, "Display Order", bkList::INT);
				ms_pPrioritizedNewsList->AddColumnHeader(1, "Story Key", bkList::STRING);
				ms_pPrioritizedNewsList->AddColumnHeader(2, "Priority", bkList::INT);
				ms_pPrioritizedNewsList->SetSingleClickItemFunc(newsItemClickCB);
			}
			pBank->PopGroup();

			CNetworkTransitionNewsController& transitionNewsCtrlr = CNetworkTransitionNewsController::Get();
			transitionNewsCtrlr.Bank_FillCompleteNewsList();
			transitionNewsCtrlr.Bank_FillPrioritizedNewsListDisplay();
		}
		pBank->PopGroup();
	}
}

void CNetworkTransitionNewsController::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);
	if( pBank)
	{
		pBank->AddButton("Create Transition News Widgets", &Bank_CreateTransitionNewsWidgets);
	}
}
#endif
