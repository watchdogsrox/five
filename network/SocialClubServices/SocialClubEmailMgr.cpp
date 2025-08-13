//
// SocialClubEmailMgr.cpp
//
// Copyright (C) 2012 Rockstar Games.  All Rights Reserved.
#include "SocialClubEmailMgr.h"

//RAGE
#include "data/rson.h"
#include "system/param.h"

//framework
#include "fwnet/netchannel.h"

//Game
#include "Event/EventGroup.h"
#include "Network/general/NetworkUtil.h"
#include "Network/NetworkInterface.h"
#include "Network/SocialClubServices/GamePresenceEvents.h"
#include "Network/Voice/NetworkVoice.h"
#include "text/TextConversion.h"

NETWORK_OPTIMISATIONS();

RAGE_DEFINE_SUBCHANNEL(net, inbox, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_inbox


// ******************************************************************************************************
// ** CNetworkSCEmail
// ******************************************************************************************************

// constructor used when sending an email
CNetworkSCEmail::CNetworkSCEmail(const char* subject, const char* content)
: m_state(STATE_NONE)
, m_ScriptId(0)
, m_CreatePosixTime(0)
, m_ReadPosixTime(0)
#if USE_EMAIL_IMAGES
, m_numImages(0)
, m_ticksInShown(0)
#endif // USE_EMAIL_IMAGES
, m_Image(0)
{
	m_hFromGamer		= NetworkInterface::GetLocalGamerHandle();
	m_SCId[0]			= '\0';
	safecpy(m_szGamerName, NetworkInterface::GetLocalGamerName());
	safecpy(m_Subject, subject);
	safecpy(m_Content, content);
}

// constructor used when receiving an email
CNetworkSCEmail::CNetworkSCEmail(u32 id, const char* scId, u64 createPosixTime, u64 readPosixTime)
: m_state(STATE_NONE)
, m_ScriptId(id)
, m_CreatePosixTime(createPosixTime)
, m_ReadPosixTime(readPosixTime)
#if USE_EMAIL_IMAGES
, m_numImages(0)
, m_ticksInShown(0)
#endif // USE_EMAIL_IMAGES
, m_Image(0)
{
	safecpy(m_SCId, scId);
	m_szGamerName[0]	= '\0';
	m_Subject[0]		= '\0';
	m_Content[0]		= '\0';
}

CNetworkSCEmail::~CNetworkSCEmail()
{
	Flush();
}

void CNetworkSCEmail::Update()
{
#if USE_EMAIL_IMAGES
	switch(m_state)
	{
	case STATE_NONE:
		{
			if (m_numImages == 0)
			{
				SetState(STATE_RCVD);
			}
		}
		break;
	case STATE_WAITING_FOR_IMAGES:
		{
			bool bAllReady = true;

			for (u32 i=0; i<m_numImages; i++)
			{
				m_images[i].Update();

				if (m_images[i].HasFailed())
				{
					gnetDebug1("EMAIL: Image %d failed to download for email (id: %d, subject: %s)", i, m_ScriptId, m_Subject);
				}
				else if (!m_images[i].IsReady())
				{
					bAllReady = false;
				}
			}

			if (bAllReady)
			{
				gnetDebug1("EMAIL: All images downloaded for email (id: %d, subject: %s)", m_ScriptId, m_Subject);
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
				m_ticksInShown = 0; // ensure we've reset
			}
		}
		break;
	case STATE_CLEANUP:
		{
			ReleaseImages();
			SetState(STATE_NONE);  //Back to none (since we've released our TXD ref).
			m_ticksInShown = 0;	// ensure we've reset
		}
	case STATE_ERROR:
		{
			ReleaseImages();
		}
		break;
	}
#else
	if (m_state == STATE_NONE)
	{
		SetState(STATE_RCVD);
	}
#endif // USE_EMAIL_IMAGES
}


void CNetworkSCEmail::Flush()
{
	gnetDebug1("EMAIL: Flush email (id: %d, subject: %s)", m_ScriptId, m_Subject);

#if USE_EMAIL_IMAGES
	ReleaseImages();
#endif
	m_Subject[0] = '\0';
	m_Content[0] = '\0';
}

#if USE_EMAIL_IMAGES
const char* CNetworkSCEmail::GetImageTxdName(u32 imageNum) const 
{ 
	if (AssertVerify(imageNum < m_numImages)) 
	{
		return m_images[imageNum].GetTxdName(); 
	}

	return '\0';
}

const char* CNetworkSCEmail::GetImageURL(u32 imageNum) const 
{ 
	if (AssertVerify(imageNum < m_numImages)) 
	{
		return m_images[imageNum].GetCloudImagePath();
	}

	return '\0';
}

int CNetworkSCEmail::GetImageTxdSlot(u32 imageNum) const 
{ 
	if (AssertVerify(imageNum < m_numImages)) 
	{
		return m_images[imageNum].GetTextureDictionarySlot(); 
	}

	return -1;
}

void CNetworkSCEmail::RequestImages()
{
	gnetDebug1("EMAIL: Request images for email (id: %d, subject: %s)", m_ScriptId, m_Subject);

	if (m_numImages > 0)
	{
		for (u32 i=0; i<m_numImages; i++)
		{
			m_images[i].Request();
		}

		SetState(STATE_WAITING_FOR_IMAGES);
	}
	else
	{
		SetState(STATE_RCVD);
	}
}
#endif // USE_EMAIL_IMAGES

bool CNetworkSCEmail::Serialise(RsonWriter& rw) const
{
	bool bSuccess = rw.Begin(NULL, NULL);

	bSuccess &= rw.Begin("email", NULL);

	bSuccess &= bSuccess && rw.WriteBinary("gh", &m_hFromGamer, sizeof(rlGamerHandle));

	bSuccess &= bSuccess && rw.WriteString("sb", m_Subject);
	bSuccess &= bSuccess && rw.WriteString("cn", m_Content);

	bSuccess &= bSuccess && rw.End(); // �email�
	bSuccess &= bSuccess && rw.End();

	return bSuccess;
}

bool CNetworkSCEmail::Deserialise(const RsonReader& rr)
{
	int nLength = 0;

	bool bSuccess = true;

	RsonReader emailContents;

	if (rr.GetMember("email", &emailContents))
	{
		// Extra fields for marketing emails
		bool isMarketingEmail = emailContents.ReadInt("i", m_Image);

		if(!isMarketingEmail)
		{
			bSuccess &= bSuccess && emailContents.ReadBinary("gh", &m_hFromGamer, sizeof(rlGamerHandle), &nLength);

			m_hFromGamer.ToUserId(m_szGamerName, EMAIL_MAX_NAME_BUFFER_SIZE);
		}
		else
		{
			bSuccess &= bSuccess && emailContents.ReadString("s", m_szGamerName);
		}

		bSuccess &= bSuccess && emailContents.ReadString("sb", m_Subject);
		bSuccess &= bSuccess && emailContents.ReadString("cn", m_Content);

	}
	else
	{
		gnetAssertf(0, "Trying to deserialise a non-email json");
		bSuccess = false;
	}

	return bSuccess;
}

void CNetworkSCEmail::SetState( State newState )
{
	gnetDebug1("EMAIL: State set to %d for email (id: %d, subject: %s)", newState, m_ScriptId, m_Subject);

	m_state = newState;
}

#if USE_EMAIL_IMAGES
void CNetworkSCEmail::ReleaseImages()
{
	gnetDebug1("EMAIL: Images released for email (id: %d, subject: %s)", m_ScriptId, m_Subject);

	for (u32 i=0; i<m_numImages; i++)
	{
		m_images[i].Clear();
	}
}
#endif // USE_EMAIL_IMAGES

void CNetworkSCEmail::SetSubjectAndContent(sScriptEmailDescriptor* pScriptDescriptor, const char* subject, const char* content)
{
	if (!gnetVerify(pScriptDescriptor))
	{
		gnetError("CNetworkSCEmail::SetSubjectAndContent() - NULL pScriptDescriptor.");
		return;
	}

	for(u32 i = 0; i < sScriptEmailDescriptor::NUM_EMAIL_SUBJECT_TEXTLABELS; i++)
		sysMemSet(&pScriptDescriptor->m_subject[i], 0, sizeof(pScriptDescriptor->m_subject[i])/SCRTYPES_ALIGNMENT-1);

	for(u32 i = 0; i < sScriptEmailDescriptor::NUM_EMAIL_CONTENTS_TEXTLABELS; i++)
		sysMemSet(&pScriptDescriptor->m_contents[i], 0, sizeof(pScriptDescriptor->m_contents[i])/SCRTYPES_ALIGNMENT-1);


	const u32 maxTextSizeInBytes = sizeof(pScriptDescriptor->m_subject[0])/SCRTYPES_ALIGNMENT-1;
	gnetAssert(maxTextSizeInBytes == (sizeof(pScriptDescriptor->m_contents[0])/SCRTYPES_ALIGNMENT-1));

	u32 strByteLength = 0;
	const u32 subjectByteCount = CTextConversion::GetByteCount(subject);

	for(u32 i = 0; i < sScriptEmailDescriptor::NUM_EMAIL_SUBJECT_TEXTLABELS; i++)
	{
		char* copyto = pScriptDescriptor->m_subject[i];
		const char* copyfrom = subject+strByteLength;
		if (strByteLength < subjectByteCount)
		{
			CTextConversion::StrcpyWithCharacterAndByteLimits(copyto, copyfrom, maxTextSizeInBytes, maxTextSizeInBytes);
			const u32 byteCount = CTextConversion::GetByteCount(copyto);
			strByteLength += byteCount;
		}
		else
		{
			break;
		}
	}

	for(u32 i = 0; i < sScriptEmailDescriptor::NUM_EMAIL_SUBJECT_TEXTLABELS; i++)
		gnetDebug1("EMAIL: CNetworkSCEmail::SetSubjectAndContent() - m_subject[%d]='%s'", i, pScriptDescriptor->m_subject[i]);

	strByteLength = 0;
	const u32 contentByteCount = CTextConversion::GetByteCount(content);

	for(u32 i = 0; i < sScriptEmailDescriptor::NUM_EMAIL_CONTENTS_TEXTLABELS; i++)
	{
		char* copyto = pScriptDescriptor->m_contents[i];
		const char* copyfrom = content+strByteLength;
		if (strByteLength < contentByteCount)
		{
			CTextConversion::StrcpyWithCharacterAndByteLimits(copyto, copyfrom, maxTextSizeInBytes, maxTextSizeInBytes);
			const u32 byteCount = CTextConversion::GetByteCount(copyto);
			strByteLength += byteCount;
		}
		else
		{
			break;
		}
	}

	for(u32 i = 0; i < sScriptEmailDescriptor::NUM_EMAIL_CONTENTS_TEXTLABELS; i++)
		gnetDebug1("EMAIL: CNetworkSCEmail::SetSubjectAndContent() - m_contents[%d]='%s'", i, pScriptDescriptor->m_contents[i]);
}

void CNetworkSCEmail::GetSubjectAndContent(sScriptEmailDescriptor* pScriptDescriptor, char* subject, char* content)
{
	if (!gnetVerify(pScriptDescriptor))
	{
		gnetError("CNetworkSCEmail::GetSubjectAndContent() - NULL pScriptDescriptor.");
		return;
	}

	u32 totalByteLength = 0;

	for(u32 i = 0; i < sScriptEmailDescriptor::NUM_EMAIL_SUBJECT_TEXTLABELS; i++)
	{
		const char* currSubject = pScriptDescriptor->m_subject[i];
		const u32 charCount = (u32)CountUtf8Characters(currSubject);
		const u32 byteCount = CTextConversion::GetByteCount(currSubject);

		if (charCount > 0)
		{
			if (gnetVerify((totalByteLength + byteCount) < SIZEOF_SUBJECT-1))
			{
				char* copyto = subject+totalByteLength;
				CTextConversion::StrcpyWithCharacterAndByteLimits(copyto, currSubject, charCount, -1);
				totalByteLength += byteCount;
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
	//Null char termination
	subject[totalByteLength+1] = '\0';
	gnetDebug1("EMAIL: CNetworkSCEmail::GetSubjectAndContent() - size='%u', sb='%s'", totalByteLength, subject);

	totalByteLength = 0;
	for(u32 i = 0; i < sScriptEmailDescriptor::NUM_EMAIL_CONTENTS_TEXTLABELS; i++)
	{
		const char* currContent = pScriptDescriptor->m_contents[i];
		const u32 charCount = (u32)CountUtf8Characters(currContent);
		const u32 byteCount = CTextConversion::GetByteCount(currContent);

		if (charCount > 0)
		{
			if (gnetVerify((totalByteLength + byteCount) < SIZEOF_CONTENT-1))
			{
				char* copyto = content+totalByteLength;
				CTextConversion::StrcpyWithCharacterAndByteLimits(copyto, currContent, charCount, -1);
				totalByteLength += byteCount;
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
	//Null char termination
	content[totalByteLength+1] = '\0';
	gnetDebug1("EMAIL: CNetworkSCEmail::GetSubjectAndContent() - size='%u', ct='%s'", totalByteLength, subject);
}

// ******************************************************************************************************
// ** CEmailImage
// ******************************************************************************************************
#if USE_EMAIL_IMAGES
void CNetworkSCEmail::CEmailImage::Set(const char* txdName, const char* cloudImagePath, int cloudImagePresize)
{
	safecpy(m_txdName, txdName);
	safecpy(m_cloudImagePath, cloudImagePath);
	m_cloudImagePresize = cloudImagePresize;

	if (m_texRequestHandle != CTextureDownloadRequest::INVALID_HANDLE || m_TextureDictionarySlot >= 0)
	{
		ReleaseTXDRef();
	}

	m_texRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	m_TextureDictionarySlot = -1;

	m_state = STATE_NONE;
}

void CNetworkSCEmail::CEmailImage::Request()
{
	if (gnetVerifyf(m_state == STATE_NONE, "Trying to request an email image that has already been requested"))
	{
		// Fill in the descriptor for our request
		CTextureDownloadRequestDesc texRequest;

		texRequest.m_Type = CTextureDownloadRequestDesc::GLOBAL_FILE;
		texRequest.m_CloudFilePath		= m_cloudImagePath;
		texRequest.m_BufferPresize		= m_cloudImagePresize;
		texRequest.m_TxdName			= m_txdName;
		texRequest.m_TextureName		= m_txdName;
		texRequest.m_CloudOnlineService = RL_CLOUD_ONLINE_SERVICE_SC;

		gnetDebug1("EMAIL IMAGE: Requesting image '%s' from '%s'", m_txdName, m_cloudImagePath);

		CTextureDownloadRequest::Status retVal = CTextureDownloadRequest::REQUEST_FAILED;
		retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(m_texRequestHandle, texRequest);

		if(retVal == CTextureDownloadRequest::IN_PROGRESS || retVal == CTextureDownloadRequest::READY_FOR_USER)
		{
			gnetAssert(m_texRequestHandle != CTextureDownloadRequest::INVALID_HANDLE);
	#if !__NO_OUTPUT
			if (retVal == CTextureDownloadRequest::READY_FOR_USER)
				gnetDebug1("EMAIL IMAGE: DTM already has TXD ready (request handle %d)", m_texRequestHandle);
	#endif

			gnetDebug1("EMAIL IMAGE: Request successful (request handle %d)", m_texRequestHandle);

			m_state = STATE_WAITING_FOR_IMAGES;
		} 
		else
		{
			gnetError("EMAIL IMAGE: Request failed");
			m_state = STATE_ERROR;
		}
	}
}

void CNetworkSCEmail::CEmailImage::Update()
{
	if (m_state == STATE_WAITING_FOR_IMAGES && AssertVerify(m_texRequestHandle != CTextureDownloadRequest::INVALID_HANDLE))
	{
		if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(m_texRequestHandle))
		{
			DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_texRequestHandle );
			m_texRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
			
			gnetError("EMAIL IMAGE: Request failed: '%s' from '%s' (request handle %d)", m_txdName, m_cloudImagePath, m_texRequestHandle);

			SetState(STATE_ERROR);
		}
		else if ( DOWNLOADABLETEXTUREMGR.IsRequestReady( m_texRequestHandle ) )
		{
			//Update the slot that the texture was slammed into
			int txdSlot = DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_texRequestHandle);
			if (gnetVerifyf(g_TxdStore.IsValidSlot(txdSlot), "CNetworkSCEmail - failed to get valid txd %s from %s for email (request handle %d, slot %d)", m_txdName, m_cloudImagePath, m_texRequestHandle, txdSlot))
			{
				g_TxdStore.AddRef(txdSlot, REF_OTHER);
				m_TextureDictionarySlot = txdSlot;
				gnetDebug1("EMAIL IMAGE: Image received ('%s' from '%s'). TXD slot %d. Request handle %d.", m_txdName, m_cloudImagePath, txdSlot, m_texRequestHandle);
			}

			DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_texRequestHandle );
			m_texRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;

			SetState(STATE_RCVD);
		}
	}
}

void CNetworkSCEmail::CEmailImage::ReleaseTXDRef()
{
	if (m_texRequestHandle != CTextureDownloadRequest::INVALID_HANDLE)
	{
		gnetDebug1("EMAIL IMAGE: Request released ('%s' from '%s'). Request handle %d.", m_txdName, m_cloudImagePath, m_texRequestHandle);
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_texRequestHandle);
		m_texRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	}

	if (m_TextureDictionarySlot >= 0)
	{
		gnetDebug1("EMAIL IMAGE: TXD '%s' from '%s' has been released", m_txdName, m_cloudImagePath);

		if(g_TxdStore.IsValidSlot(m_TextureDictionarySlot))
		{
			g_TxdStore.RemoveRef(m_TextureDictionarySlot, REF_OTHER);
		}
		m_TextureDictionarySlot = -1;
	}
}

void CNetworkSCEmail::CEmailImage::Clear()
{
	ReleaseTXDRef();

	m_txdName[0] = '\0';
	m_cloudImagePath[0] = '\0';
	m_cloudImagePresize = 0;
	m_state = STATE_NONE;
}

void CNetworkSCEmail::CEmailImage::SetState( State newState )
{
	m_state = newState;
}
#endif // USE_EMAIL_IMAGES

// ******************************************************************************************************
// ** CNetworkEmailMgr
// ******************************************************************************************************
CNetworkEmailMgr& CNetworkEmailMgr::Get()
{
	typedef atSingleton<CNetworkEmailMgr> CNetworkEmailMgr;
	if (!CNetworkEmailMgr::IsInstantiated())
	{
		CNetworkEmailMgr::Instantiate();
	}

	return CNetworkEmailMgr::GetInstance();
}


void CNetworkEmailMgr::Init()
{
	//Register to handle presence events.
	m_presDelegate.Bind(this, &CNetworkEmailMgr::OnPresenceEvent);
	rlPresence::AddDelegate(&m_presDelegate);

	m_requestStatus.Reset();

	m_nextFreeScriptId = 0;
}

void CNetworkEmailMgr::Shutdown()
{
	FlushRetrievedEmails();

	if (m_requestStatus.Pending())
	{
		rlInbox::Cancel(&m_requestStatus);
	}
}

void CNetworkEmailMgr::Update()
{
	if (m_requestStatus.GetStatus() != netStatus::NET_STATUS_NONE && !m_requestStatus.Pending())
	{
		if (m_requestStatus.Succeeded())
		{
			gnetDebug1("EMAIL: Request SUCCEEDED");
			ProcessReceivedEmails();	
		}
		else if (m_requestStatus.Failed() || m_requestStatus.Canceled())
		{
			gnetDebug1("EMAIL: Request FAILED OR CANCELLED");
			//No Retry...it's not that important
		}

		m_requestStatus.Reset();
	}

	for (int i = 0; i < m_emailArray.GetCount(); ++i)
	{
		if (gnetVerify(m_emailArray[i]))
		{
			m_emailArray[i]->Update();
		}
	}
}

void CNetworkEmailMgr::OnPresenceEvent( const rlPresenceEvent* evt )
{
	if ( evt->GetId() == rlPresenceEventScMessage::EVENT_ID() )
	{
		const rlPresenceEventScMessage* pSCEvent = static_cast<const rlPresenceEventScMessage*>(evt);

		if(evt->m_ScMessage->m_Message.IsA<rlScPresenceInboxNewMessage>())
		{
			rlScPresenceInboxNewMessage inbxRcvd;
			if(gnetVerify(inbxRcvd.Import(pSCEvent->m_Message)))
			{
				//check to see if the recipient is the current gamer.
				int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
				rlGamerHandle gh;
				if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && rlPresence::GetGamerHandle(localGamerIndex, &gh))
				{
					if (gh == inbxRcvd.m_Recipient && inbxRcvd.IsTagPresent("gta5email"))
					{
						gnetDebug1("EMAIL MGR: Email event received");

						// trigger an event for script
						GetEventScriptNetworkGroup()->Add(CEventNetworkEmailReceivedEvent());
						
						if (gh.IsValid())
						{
							CNetworkTelemetry::OnNewEmailMessagesReceived(gh);
						}
					}
					else if (gh == inbxRcvd.m_Recipient && inbxRcvd.IsTagPresent("gta5marketing"))
					{
						gnetDebug1("EMAIL MGR: Marketing Email event received");

						// trigger an event for script
						GetEventScriptNetworkGroup()->Add(CEventNetworkMarketingEmailReceivedEvent());
					}
				}
			}
		}
	}
}

const char*   s_postemailTag[] = {"gta5email"};
const char*	  s_validemailTags[] = 
{
	"gta5email",
	"gta5mkt_en",
	"gta5mkt_fr",
	"gta5mkt_ge",
	"gta5mkt_it",
	"gta5mkt_sp",
	"gta5mkt_pt",
	"gta5mkt_pl",
	"gta5mkt_ru",
	"gta5mkt_ko",
	"gta5mkt_ch",
	"gta5mkt_ja",
	"gta5mkt_me",
	"gta5mkt_cn", // simplified chinese
};
const int MAX_VALID_EMAIL_TAGS = NELEM(s_validemailTags);

//Our default email tag is "gta5email".
unsigned s_currentEmailTag = 0;

bool CNetworkEmailMgr::SetCurrentEmailTag(const char* tag)
{
	bool result = false;

	for (int i=0; i<MAX_VALID_EMAIL_TAGS && !result; i++)
	{
		if (strcmp(s_validemailTags[i], tag) == 0)
		{
			s_currentEmailTag = i;
			result = true;
			break;
		}
	}

	gnetAssertf(result, "EMAIL: Invalid tag '%s'.", tag);

	return result;
}

void CNetworkEmailMgr::SendEmail(SendFlag sendFlags, sScriptEmailDescriptor* pScriptDescriptor, const rlGamerHandle* pRecipients, const int numRecipients)
{
	gnetDebug1("EMAIL: Send");

	if (!gnetVerify(pScriptDescriptor))
	{
		return;
	}

	int myGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(myGamerIndex))
	{
		gnetError("Invalid gamer index in SendEventToCommunity");
		return;
	}

	if(!rlPresence::IsOnline(myGamerIndex) || !rlRos::GetCredentials(myGamerIndex).IsValid())
	{
		gnetDebug1("Not able to do SendEmail when player is not online or invalid credentials");
		return;
	}

	char subject[CNetworkSCEmail::SIZEOF_SUBJECT];
	sysMemSet(subject, 0, CNetworkSCEmail::SIZEOF_SUBJECT);

	char content[CNetworkSCEmail::SIZEOF_CONTENT];
	sysMemSet(content, 0, CNetworkSCEmail::SIZEOF_CONTENT);

	CNetworkSCEmail::GetSubjectAndContent(pScriptDescriptor, subject, content);
	Utf8RemoveInvalidSurrogates(subject);
	Utf8RemoveInvalidSurrogates(content);

	gnetDebug1("EMAIL: Subject: %s", subject);
	gnetDebug1("EMAIL: Content: %s", content);

	CNetworkSCEmail newEmail(subject, content);

	char contentBuffer[2048];
	RsonWriter rw(contentBuffer, RSON_FORMAT_JSON);

	if (!newEmail.Serialise(rw))
	{
		gnetAssertf(0, "Failed to export email to buffer");
		return;
	}

	int expirationTimeSecs = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("EMAIL_EXPIRY_TIME", 0xcf695762), CNetworkSCEmail::EMAIL_LIFETIME_IN_SECS);

	gnetDebug1("EMAIL: Expiry Time: %d", expirationTimeSecs);

	if (expirationTimeSecs < 0)
	{
		gnetAssertf(0, "Email expiry time is negative due to tunables. Using default");
		expirationTimeSecs = CNetworkSCEmail::EMAIL_LIFETIME_IN_SECS;
	}

	bool emailSent = false;
	rlGamerHandle singleRecipientHandle;

	//First check if we have a specific list of gamers
	if(sendFlags & SendFlag_Inbox && pRecipients && numRecipients > 0)
	{
#if !__FINAL
		gnetDebug1("EMAIL: Send to '%d' gamers with tag '%s'.", numRecipients, (char *)&s_validemailTags[s_currentEmailTag]);
		ASSERT_ONLY(const bool result =) rlInbox::PostMessage(myGamerIndex, pRecipients, numRecipients, contentBuffer, &s_validemailTags[s_currentEmailTag], 1, (unsigned)expirationTimeSecs);
		ASSERT_ONLY( gnetAssertf(result, "EMAIL: Failed Send to '%d' gamers with tag '%s'.", numRecipients, (char *)&s_validemailTags[s_currentEmailTag]); )
#else
		gnetDebug1("EMAIL: Send to '%d' gamers with tag '%s'.", numRecipients, (char *)s_postemailTag);
		ASSERT_ONLY(const bool result =) rlInbox::PostMessage(myGamerIndex, pRecipients, numRecipients, contentBuffer, s_postemailTag, 1, (unsigned)expirationTimeSecs);
		ASSERT_ONLY( gnetAssertf(result, "EMAIL: Failed Send to '%d' gamers with tag '%s'.", numRecipients, (char *)s_postemailTag); )
#endif // !__FINAL

		emailSent = true;
		if(numRecipients == 1)
			singleRecipientHandle = pRecipients[0];
	}

	if (sendFlags & SendFlag_AllFriends)
	{
#if !__FINAL
		gnetDebug1("EMAIL: Send to All Friends with tag '%s'.", (char *)&s_validemailTags[s_currentEmailTag]);
		ASSERT_ONLY(const bool result =) rlInbox::PostMessageToManyFriends(myGamerIndex, contentBuffer, &s_validemailTags[s_currentEmailTag], 1, (unsigned)expirationTimeSecs);
		ASSERT_ONLY( gnetAssertf(result, "EMAIL: Failed Send to All Friends with tag '%s'.", (char *)&s_validemailTags[s_currentEmailTag]); )
#else
		gnetDebug1("EMAIL: Send to All Friends with tag '%s'.", (char *)s_postemailTag);
		ASSERT_ONLY(const bool result =) rlInbox::PostMessageToManyFriends(myGamerIndex, contentBuffer, s_postemailTag, 1, (unsigned)expirationTimeSecs);
		ASSERT_ONLY( gnetAssertf(result, "EMAIL: Failed Send to All Friends with tag '%s'.", (char *)s_postemailTag); )
#endif // !__FINAL
		emailSent = true;
	}

	if (sendFlags & SendFlag_AllCrew)
	{
		const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(myGamerIndex);
		if(clanDesc.IsValid())
		{
			gnetDebug1("EMAIL: Send to All Crew");
			rlInbox::PostMessageToClan(myGamerIndex, clanDesc.m_Id, contentBuffer, NULL, 0, (unsigned)expirationTimeSecs);
			emailSent = true;
		}
	}

	if (emailSent)
	{
		int textLength = (int)strlen(subject) + (int)strlen(content);
		CNetworkTelemetry::RecordPhoneEmailMessage(singleRecipientHandle, textLength);
	}
}

bool CNetworkEmailMgr::RetrieveEmails(u32 firstEmail, u32 numEmails)
{
	//Make sure we're not already busy
	if (m_requestStatus.Pending())
	{
		gnetError("Email retrieval request made while one is in progress");
		return false;
	}

	//check to see if the recipient is the current gamer.
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return false;
	}

	const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);
	if(!cred.IsValid())
	{
		return false;
	}

	FlushRetrievedEmails();

	gnetDebug1("EMAIL: Retrieving '%d' emails from '%d with tag '%s'.", numEmails, firstEmail, (char *)&s_validemailTags[s_currentEmailTag]);

	const bool result = rlInbox::GetMessages(localGamerIndex
											,firstEmail                            //the first email to get.
											,numEmails                             //Get at most this many.
											,&s_validemailTags[s_currentEmailTag]  //tags.
											,1                                     //numTags.
											,&m_MsgIter
											,&m_requestStatus);

	gnetAssertf(result, "EMAIL: Failed to call rlInbox::GetMessages() - Retrieving '%d' emails from '%d with tag '%s'.", numEmails, firstEmail, (char *)&s_validemailTags[s_currentEmailTag]);

	//Always reset back to default tag "gta5email"
	s_currentEmailTag = 0;

	return result;
}

bool CNetworkEmailMgr::GetEmailAtIndex(int index, sScriptEmailDescriptor* pScriptDescriptor)
{
	if (gnetVerify(pScriptDescriptor))
	{
		if (index >= 0 && index < m_emailArray.GetCount())
		{
			CNetworkSCEmail* pEmail = m_emailArray[index];

			if (pEmail)
			{
				pScriptDescriptor->m_id.Int = pEmail->GetScriptId();
				pScriptDescriptor->m_createPosixTime.Int = (int)pEmail->GetCreatePosixTime();

				CNetworkSCEmail::SetSubjectAndContent(pScriptDescriptor, pEmail->GetSubject(), pEmail->GetContent());

				// if there's an image, it's a marketing email
				if(pEmail->GetImage() != 0)
				{
					pScriptDescriptor->m_image.Int = pEmail->GetImage();
				}
				else
				{
					pEmail->GetSender().Export(pScriptDescriptor->m_fromGamerHandle, sizeof(pScriptDescriptor->m_fromGamerHandle));
				}

#if !__NO_OUTPUT
				gnetDebug1("EMAIL: GetEmailAtIndex '%d',  content='%s'.", index,  pEmail->GetContent());

				for (int i=0; i<sScriptEmailDescriptor::NUM_EMAIL_SUBJECT_TEXTLABELS; i++)
				{
					if(CountUtf8Characters(pScriptDescriptor->m_subject[i]) > 0)
					{
						gnetDebug1("m_subject[%i]='%s'.", i, pScriptDescriptor->m_subject[i]);
					}
					else
					{
						break;
					}
				}

				for (int i=0; i<sScriptEmailDescriptor::NUM_EMAIL_CONTENTS_TEXTLABELS; i++)
				{
					if(CountUtf8Characters(pScriptDescriptor->m_contents[i]) > 0)
					{
						gnetDebug1("m_contents[%i]='%s'.", i, pScriptDescriptor->m_contents[i]);
					}
					else
					{
						break;
					}
				}
#endif //!__NO_OUTPUT

				safecpy(pScriptDescriptor->m_fromGamerTag, pEmail->GetSenderName());
				return true;
			}
		}
	}

	return false;
}

void CNetworkEmailMgr::DeleteEmails(int* scriptEmailIds, int numIds)
{
	SocialClubID scIds[MAX_EMAILS_INBOX];
	u32 numSCIds = 0;

	for (int id=0; id<numIds; id++)
	{
		bool bFound = false;

		for (int e=0; e<m_emailArray.GetCount(); e++)
		{
			CNetworkSCEmail* pEmail = m_emailArray[e];

			if (pEmail && scriptEmailIds[id] == pEmail->GetScriptId())
			{
				gnetDebug1("EMAIL MGR: Delete email (id: %d, sc id: %s, subject: %s)", pEmail->GetScriptId(), pEmail->GetSCId(), pEmail->GetSubject());

				if (gnetVerify(numSCIds<MAX_EMAILS_INBOX))
				{
					safecpy(scIds[numSCIds++], pEmail->GetSCId());
				}

				delete pEmail;
				m_emailArray[e] = NULL;
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			gnetAssertf(0, "Failed to find email to delete with id %d", scriptEmailIds[id]);
		}
	}

	//Clean up NULLs
	for (int removeIter = m_emailArray.GetCount() - 1; removeIter >= 0; removeIter--)
	{
		if (m_emailArray[removeIter] == NULL)
		{
			//Delete is from the list (but RETAIN the order)
			m_emailArray.Delete(removeIter);
		}
	}

	if (numSCIds > 0)
	{
		DeleteSCEmails(scIds, numSCIds);
	}
}

void CNetworkEmailMgr::DeleteAllRetrievedEmails()
{
	int ids[MAX_EMAILS_INBOX];
	u32 numIds = 0;

	for (int e=0; e<m_emailArray.GetCount(); e++)
	{
		CNetworkSCEmail* pEmail = m_emailArray[e];

		if (pEmail)
		{
			ids[numIds++] = pEmail->GetScriptId();
		}
	}

	if (numIds > 0)
	{
		DeleteEmails(ids, numIds);
	}
}

void CNetworkEmailMgr::ProcessReceivedEmails()
{
	const char* msg;
	const char* msgId;
	u64 createPosixTime, readPosixTime;

	gnetAssert(m_emailArray.GetCount() == 0);

	gnetDebug1("EMAIL MGR: Process received emails");

	while (m_MsgIter.NextMessage(&msg, &msgId, &createPosixTime, &readPosixTime))
	{
		bool bDelete = false;

		gnetDebug1("EMAIL MGR: Process email with SC id %s)", msgId);

		if (gnetVerifyf(RsonReader::ValidateJson(msg, istrlen(msg)), "Failed json validation for email '%s'", msg))
		{
			//Create a new request and add it to the list
			CNetworkSCEmail* pNewEmail = rage_new CNetworkSCEmail(++m_nextFreeScriptId, msgId, createPosixTime, readPosixTime);
			if (!pNewEmail)
			{
				gnetAssertf(0, "Unable to create CNetworkSCEmail");
				return;
			}

			RsonReader msgData(msg, istrlen(msg));

			if (pNewEmail->Deserialise(msgData))
			{
				gnetDebug1("EMAIL MGR: New email added from %s (id: %d, SC id: %s)", pNewEmail->GetSenderName(), pNewEmail->GetScriptId(), pNewEmail->GetSCId());
				gnetDebug1("EMAIL MGR: Subject: %s", pNewEmail->GetSubject());
				gnetDebug1("EMAIL MGR: Content: %s", pNewEmail->GetContent());

				if(pNewEmail->GetImage() != 0)
				{
					gnetDebug1("EMAIL MGR: Image: %d", pNewEmail->GetImage());
				}

#if RSG_DURANGO
				// Xbox XR [XR-015] requires blocking of all communication (including text) when received
				// from a mute player
				// we can only make this check once we have the gamer handle of the sender
				if(NetworkInterface::GetVoice().IsInMuteList(pNewEmail->GetSender()))
				{
					gnetDebug1("EMAIL MGR: Rejecting, Sender in Mute List");
					bDelete = true;
				}
				else
#endif
				{
				
					m_emailArray.PushAndGrow(pNewEmail);
				}
			}
			else
			{
				bDelete = true;
			}
		}
		else
		{
			bDelete = true;
		}

		if (bDelete)
		{
			DeleteSCEmails((const SocialClubID*)msgId, 1);
		}
	}

	//Release memory
	m_MsgIter.ReleaseResources();
}

void CNetworkEmailMgr::FlushRetrievedEmails()
{
	gnetDebug1("EMAIL MGR: FlushRetrievedEmails");

	for (int i = 0; i < m_emailArray.GetCount(); ++i)
	{
		m_emailArray[i]->Flush();
		delete m_emailArray[i];
	}

	m_emailArray.Reset();

	m_MsgIter.ReleaseResources();
}

void CNetworkEmailMgr::DeleteSCEmails(const SocialClubID* scEmailIds, u32 numIds)
{
	int myGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(myGamerIndex))
	{
		gnetError("Invalid gamer index in CNetworkEmailMgr::DeleteSCEmails");
		return;
	}

	if(!rlPresence::IsOnline(myGamerIndex) || !rlRos::GetCredentials(myGamerIndex).IsValid())
	{
		gnetDebug1("Not able to do CNetworkEmailMgr::DeleteSCEmails when player is not online or invalid credentials");
		return;
	}

	const char* messageIds[MAX_EMAILS_INBOX];
	
	for (int id=0; id<numIds; id++)
	{
		messageIds[id] = (const char*)scEmailIds[id];
	}

	rlInbox::DeleteMessages(myGamerIndex, messageIds, numIds);
}

#if __BANK
char sBank_Email_Subject[CNetworkSCEmail::SIZEOF_SUBJECT];
char sBank_Email_Contents[CNetworkSCEmail::SIZEOF_CONTENT];
int sBank_FirstEmail;
int sBank_NumEmails;
int sBank_EmailToDelete;

void Bank_EmailSend()
{
	sScriptEmailDescriptor scriptDescriptor;

	CNetworkSCEmail::SetSubjectAndContent(&scriptDescriptor, sBank_Email_Subject, sBank_Email_Contents);

	CNetworkEmailMgr::Get().SendEmail(SendFlag_Inbox, &scriptDescriptor, &NetworkInterface::GetLocalGamerHandle(), 1);
}

void Bank_EmailRetrieve()
{
	CNetworkEmailMgr::Get().RetrieveEmails(sBank_FirstEmail, sBank_NumEmails);
}

void Bank_EmailDelete()
{
	CNetworkEmailMgr::Get().DeleteEmails(&sBank_EmailToDelete, 1);
}

void Bank_EmailDeleteAll()
{
	CNetworkEmailMgr::Get().DeleteAllRetrievedEmails();
}

void CNetworkEmailMgr::InitWidgets( bkBank* pBank )
{
	pBank->PushGroup( "Social Club Email" );
	{
		/*
		sBank_Email_Subject[0] = '\0';
		sBank_Email_Contents[0] = '\0';

		for (u32 i=0; i<CNetworkSCEmail::SIZEOF_SUBJECT-1; i++)
		{
			strcat(sBank_Email_Subject, "X");
		}
		sBank_Email_Subject[CNetworkSCEmail::SIZEOF_SUBJECT-1] = '\0';

		for (u32 i=0; i<CNetworkSCEmail::SIZEOF_CONTENT-1; i++)
		{
			strcat(sBank_Email_Contents, "Q");
		}
		sBank_Email_Contents[CNetworkSCEmail::SIZEOF_CONTENT-1] = '\0';
		*/

		safecpy(sBank_Email_Subject, "Hello");
		safecpy(sBank_Email_Contents, "Blah blah blah");

		sBank_FirstEmail = 0;
		sBank_NumEmails = MAX_EMAILS_INBOX;

		pBank->AddText("Email subject", sBank_Email_Subject, sizeof(sBank_Email_Subject), false);
		pBank->AddText("Email contents", sBank_Email_Contents, sizeof(sBank_Email_Contents), false);
		pBank->AddButton("Send email", CFA(Bank_EmailSend));
		pBank->AddText("First email to retrieve", &sBank_FirstEmail);
		pBank->AddSlider("Num emails to retrieve", &sBank_NumEmails, 1, MAX_EMAILS_INBOX, 1);
		pBank->AddButton("Retrieve emails", CFA(Bank_EmailRetrieve));
		pBank->AddText("Email to delete", &sBank_EmailToDelete);
		pBank->AddButton("Delete email", CFA(Bank_EmailDelete));
		pBank->AddButton("Delete all emails", CFA(Bank_EmailDeleteAll));
	}
	pBank->PopGroup();
}
#endif // __BANK	