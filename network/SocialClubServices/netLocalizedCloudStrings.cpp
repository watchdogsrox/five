//////////////////////////////////////////////////////////////////////////
//
//	Localized cloud string stuff
//
//////////////////////////////////////////////////////////////////////////
#include "netLocalizedCloudStrings.h"

//framework
#include "fwnet/netchannel.h"

#include "Network/General/NetworkUtil.h"

using namespace rage;

#undef __net_channel
#define __net_channel net_cloudstrings
RAGE_DEFINE_SUBCHANNEL(net,cloudstrings);

NETWORK_OPTIMISATIONS()

netLocalizedStringInCloudMgr& netLocalizedStringInCloudMgr::Get()
{
	typedef atSingleton<netLocalizedStringInCloudMgr> netLocalizedStringInCloudMgrSingleton;
	if (!netLocalizedStringInCloudMgrSingleton::IsInstantiated())
	{
		netLocalizedStringInCloudMgrSingleton::Instantiate();
	}

	return netLocalizedStringInCloudMgrSingleton::GetInstance();
}


netLocalizedStringInCloudMgr::netLocalizedStringInCloudMgr()
{

}

netLocalizedStringInCloudMgr::~netLocalizedStringInCloudMgr()
{
	m_reqMap.Kill();
}

bool netLocalizedStringInCloudMgr::ReqestString( const char* key )
{
	//Check in the map to see if he already have an entry for it.
	const netLocalizedStringInCloudRequest* pStringReq = GetEntry(key);
	if (pStringReq)
	{
		gnetDebug1("New cloud string request for '%s' found previous instance in map", key);
		return true;
	}

	//Otherwise, creat a new request
	const char* langCode = NetworkUtils::GetLanguageCode();
	netLocalizedStringInCloudRequest* pNewStringReq = rage_new netLocalizedStringInCloudRequest();
	if (pNewStringReq && pNewStringReq->Start(key, langCode))
	{
		//If it started, add it to the map and pat yourself on the back...
		gnetDebug1("New cloud string request for '%s' has started successfully", key);
		m_reqMap.Insert(atStringHash(key), pNewStringReq);
		return true;
	}

	//Failed!
	//Clean up your leak
	gnetError("FAILED to start new cloud string request for '%s'", key);
	delete pNewStringReq;

	return false;
}

const char* netLocalizedStringInCloudMgr::GetStringForRequest( const char* key ) const
{
	const netLocalizedStringInCloudRequest* pStringReq = GetEntry(key);
	if (gnetVerifyf(pStringReq, "No request for '%s' found", key))
	{
		if (pStringReq->Succeeded())
		{
			return pStringReq->GetString();
		}
	}

	return "";
}

void netLocalizedStringInCloudMgr::ReleaseRequest( const char* key )
{
	netLocalizedStringInCloudRequest* pStringReq = GetEntryNonConst(key);
	if (gnetVerifyf(pStringReq, "No request for '%s' found", key))
	{
		gnetDebug1("Releasing request for '%s'", key);
		if (pStringReq->IsPending())
		{
			pStringReq->Cancel();
		}
		delete pStringReq;
		m_reqMap.Delete(atStringHash(key));
	}
}

bool netLocalizedStringInCloudMgr::IsRequestPending( const char* key ) const
{
	const netLocalizedStringInCloudRequest* pStringReq = GetEntry(key);;
	if (gnetVerifyf(pStringReq, "No request for '%s' found", key))
	{
		return pStringReq->IsPending();
	}

	return false;
}

bool netLocalizedStringInCloudMgr::IsRequestFailed( const char* key ) const
{
	const netLocalizedStringInCloudRequest* pStringReq = GetEntry(key);
	if (gnetVerifyf(pStringReq, "No request for '%s' found", key))
	{
		return pStringReq->Failed();
	}

	return true;
}

const netLocalizedStringInCloudRequest* netLocalizedStringInCloudMgr::GetEntry( const char* key ) const
{
	const netLocalizedStringInCloudRequest* const * pStringReqEntry = m_reqMap.Access(atStringHash(key));
	if (pStringReqEntry)
	{
		return *pStringReqEntry;
	}

	return NULL;
}

netLocalizedStringInCloudRequest* netLocalizedStringInCloudMgr::GetEntryNonConst( const char* key )
{
	netLocalizedStringInCloudRequest** pStringReqEntry = m_reqMap.Access(atStringHash(key));
	if (pStringReqEntry)
	{
		return *pStringReqEntry;
	}

	return NULL;
}

netLocalizedStringInCloudRequest::netLocalizedStringInCloudRequest()
	: CloudListener()
	, m_cloudReqID(-1)
{
	m_status.Reset();
}

netLocalizedStringInCloudRequest::~netLocalizedStringInCloudRequest()
{

}

bool netLocalizedStringInCloudRequest::Start( const char* messageKey, const char* lang )
{
	char path[128];
	const char* fileType = "json";

	//@TODO: IF we're requesting image or for asain language, we need to request a texture using the DTM

	m_status.Reset();

	formatf(path, "sc/msg/%s/%s.%s", messageKey, lang, fileType);
	m_cloudReqID = CloudManager::GetInstance().RequestGetGlobalFile(path, 256, eRequest_CacheAddAndEncrypt);

	OUTPUT_ONLY(safecpy(m_messageKey, messageKey));

	if( m_cloudReqID != -1)
	{
		m_status.SetPending();
	}
	else
	{
		gnetError("FAILED to start cloud string request for %s", path);
		m_status.SetFailed();
	}

	return m_status.Pending();
}

void netLocalizedStringInCloudRequest::OnCloudEvent( const CloudEvent* pEvent )
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

			if (pEventData->bDidSucceed)
			{
				gnetDebug2("Received cloud loc string for '%s' of %d bytes", m_messageKey, pEventData->nDataSize);

				m_gb.Init(NULL, datGrowBuffer::NULL_TERMINATE);
				if (pEventData->nDataSize > 0 && gnetVerify(m_gb.Preallocate(pEventData->nDataSize)))
				{
					if(gnetVerify(m_gb.AppendOrFail(pEventData->pData, pEventData->nDataSize)))
					{
						//Do the string fixup in memory so we have a nice clean string to return when asked.
						if (DoStringFixup())
						{
							gnetDebug1("SUCCESS recieving string '%s'", m_stringPtr);
							m_status.SetSucceeded();
						}
						else
						{
							gnetError("String Fixup for '%s' failed", m_messageKey);
							m_status.SetFailed();
						}
					}
					else
					{
						gnetError("Fail to fill grow buffer for '%s'", m_messageKey);
						m_status.SetFailed();
					}
				}
				else
				{
					gnetError("Cloud request completed with 0 bytes for '%s'", m_messageKey);
					m_status.SetFailed();
				}
				m_cloudReqID = -1;
			}
			else
			{
				gnetError("Request for '%s' failed with error code %d", m_messageKey, pEventData->nResultCode );
				m_status.SetFailed(pEventData->nResultCode);

				m_cloudReqID = -1;
				m_gb.Clear();
			}
		}
		break;
	default:
		break;
	}
}

bool netLocalizedStringInCloudRequest::DoStringFixup()
{
	if(m_gb.Length() > 0)
	{
		//Use the RsonReader to get the value of the member from the buffer.
		char* rawCharPtr = (char*)m_gb.GetBuffer();
		if( gnetVerifyf(RsonReader::ValidateJson(rawCharPtr, m_gb.Length()), "Failed json validation for '%s'", rawCharPtr) )
		{
			RsonReader rr(rawCharPtr, m_gb.Length());
			int strLen = 0;
			m_stringPtr = rr.GetRawMemberValue("s", strLen);

			//Chomp the string off at the length to make a nice clean string
			if (m_stringPtr && strLen > 0)
			{
				//Terminate the raw string in memory. (chomp the ending quote)
				int offsetFromBeginning = int(m_stringPtr - rawCharPtr) + strLen;
				rawCharPtr[offsetFromBeginning] = '\0';

				return true;
			}
		}
	}

	m_stringPtr = NULL;
	return false;
}


void netLocalizedStringInCloudRequest::Cancel()
{
	//Just internal stuff.  The cloud file request will die on it's own.
	if(m_status.Pending())
		m_status.SetCanceled();
}

const char* netLocalizedStringInCloudRequest::GetString() const
{
	if(m_status.Succeeded() && m_stringPtr != NULL)
	{
		return m_stringPtr;
	}

	return "";
}
