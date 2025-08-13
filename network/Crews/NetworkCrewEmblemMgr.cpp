//===========================================================================
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.             
//===========================================================================
#include "NetworkCrewEmblemMgr.h"

//RAGE Headers
#include "system/param.h"
#include "diag/output.h"
#include "file/device.h"
#include "fwscene/stores/txdstore.h" 
#include "grcore/texture.h"
#include "math/amath.h"
#include "rline/clan/rlclan.h"
#include "rline/cloud/rlcloud.h"
#include "rline/rlpresence.h"


#if __BANK
#include "bank/bank.h"
#include "debug/TextureViewer/TextureViewer.h"
#include "debug/TextureViewer/TextureViewerSearch.h"
#endif

//framework
#include "fwsys/timer.h"
#include "fwnet/netchannel.h"
#include "fwrenderer/renderthread.h"

//Game headers
#include "Network/Live/livemanager.h"
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "scene/DownloadableTextureManager.h"

#include "streaming/streamingengine.h" //for streaming allocator

NETWORK_OPTIMISATIONS()

PARAM(netNoClanEmblems, "Disables the clan emblem system");
PARAM(emblemRequestCallstack, "Prints the callstack when an emblem is requested.");


RAGE_DEFINE_SUBCHANNEL(net, clanemblem, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_clanemblem

//===========================================================================

NetworkCrewEmblemMgr::NetworkCrewEmblemMgr() 
: m_bAllowEmblemRequests(true)
{
}

NetworkCrewEmblemMgr::~NetworkCrewEmblemMgr()
{
	Shutdown();
}

void NetworkCrewEmblemMgr::Init()
{
	m_bAllowEmblemRequests = true;

	if(PARAM_netNoClanEmblems.Get())
	{
		m_bAllowEmblemRequests = false;
	}
}

void NetworkCrewEmblemMgr::Shutdown()
{
	// Flush the render thread, we don't want to delete textures out from under it
	gRenderThreadInterface.Flush();

	// Destroy any emblems we had
	EmblemMap::Iterator emblemIter = m_emblemMap.CreateIterator();
	for (emblemIter.Start(); !emblemIter.AtEnd(); ++emblemIter)
	{
		EmblemData* pEmblemData = emblemIter.GetData();

		const strLocalIndex textureDictionarySlot = strLocalIndex(pEmblemData->GetEmblemTXDSlot());
		if(textureDictionarySlot.Get() >= 0)
		{
			if(g_TxdStore.IsValidSlot(textureDictionarySlot))
			{
				g_TxdStore.RemoveRef(textureDictionarySlot, REF_OTHER);
			}
		}

		pEmblemData->Destroy();
		delete pEmblemData;
	}

	m_emblemMap.Kill();

	// Deref any destroyed emblems
	for(int queuedRemove = 0; queuedRemove < m_txdsPendingToDeRef.GetCount(); ++queuedRemove)
	{
		const strLocalIndex slotId = strLocalIndex(m_txdsPendingToDeRef[queuedRemove].m_txdSlot);

		gnetDebug3("Removing delayed emblem deref %d", slotId.Get());
		if(g_TxdStore.IsValidSlot(slotId))
		{
			g_TxdStore.RemoveRef(slotId, REF_OTHER);
		}
	}

	m_txdsPendingToDeRef.Reset();
}

void NetworkCrewEmblemMgr::Update()
{
	EmblemMap::Iterator iter = m_emblemMap.CreateIterator();
	atArray<EmblemDescriptor> toBeRemoved;
	int activeCount = 0;
	for (iter.Start(); !iter.AtEnd(); ++iter)
	{
		EmblemData* pEmblemData = iter.GetData();
		pEmblemData->Update();

		if(pEmblemData->Pending()) // if any one is pending, don't add
		{
			activeCount++;
		}

		if(pEmblemData->ReadyForDelete())
		{
			toBeRemoved.PushAndGrow(iter.GetKey());
		}
	}

	// Now remove any dictionary reference from any previously removed guy
	for(int queuedRemove = 0; queuedRemove < m_txdsPendingToDeRef.GetCount(); ++queuedRemove)
	{
		PendingDeRef& currentCheck = m_txdsPendingToDeRef[queuedRemove];
		if(--currentCheck.m_countdown <= 0)
		{
			gnetDebug3("Removing delayed emblem deref %d", currentCheck.m_txdSlot);
			if(g_TxdStore.IsValidSlot(strLocalIndex(currentCheck.m_txdSlot)))
			{
				g_TxdStore.RemoveRef(strLocalIndex(currentCheck.m_txdSlot), REF_OTHER);
			}
			m_txdsPendingToDeRef.DeleteFast(queuedRemove--);
		}
	}

	//Now remove the guys we found needed to be cleaned up.
	for(int i = 0; i < toBeRemoved.GetCount(); ++i)
	{
		EmblemData** dataPtr = m_emblemMap.Access(toBeRemoved[i]);
		if(dataPtr)
		{
			EmblemData* pData = *dataPtr;
			DeleteAndRemove(toBeRemoved[i], pData);
		}
	}

	//Now start some more.
	int numberToStart = sm_NUMBER_SIMULTANEOUS_EMBLEM_DOWNLOADS - activeCount;
	for (iter.Start(); !iter.AtEnd() && numberToStart > 0; ++iter)
	{
		EmblemData* pEmblemData = iter.GetData();
		if(!pEmblemData->Started())
		{
			if(pEmblemData->StartDownload())
			{
				numberToStart--;
			}
			else //If we failed to start a download, just stop trying.
			{
				gnetDebug1("Failed to start a emblem download, bailing before trying to start any more.  %d left", numberToStart);
				break;
			}
		}
	}
}

void NetworkCrewEmblemMgr::AddUnmodifiedEmblem(const EmblemDescriptor& emblemDesc)
{
	gnetDebug1("Request for Emblem %s return 304, adding to the unmodified list", emblemDesc.GetStr());
	m_unmodifiedEmblemIds.PushAndGrow(emblemDesc);
}

bool NetworkCrewEmblemMgr::IsKnownUnmodifiedEmblem(const EmblemDescriptor& emblemDesc) const
{
	return m_unmodifiedEmblemIds.Find(emblemDesc) >= 0;
}

void NetworkCrewEmblemMgr::DeleteAndRemove(const EmblemDescriptor& emblemDesc, EmblemData* pData)
{
	if(!pData)
	{
		EmblemData** dataPtr = m_emblemMap.Access(emblemDesc);
		if(dataPtr)
		{
			pData = (*dataPtr);
		}
	}

	if(pData)
	{
		const strLocalIndex textureDictionarySlot = strLocalIndex(pData->GetEmblemTXDSlot());
		if(textureDictionarySlot.Get() >= 0)
		{
			if(g_TxdStore.IsValidSlot(textureDictionarySlot))
			{
				const int DEREF_ARRAY_GROW_STEP = 8;

				gnetDebug3("Queued delayed emblem deref %d", textureDictionarySlot.Get());
				PendingDeRef newRemoval(textureDictionarySlot.Get());
				m_txdsPendingToDeRef.PushAndGrow(newRemoval, DEREF_ARRAY_GROW_STEP);
			}
		}

		pData->Destroy();
		delete pData;
	}
	
	gnetDebug3("m_emblemMap.Delete emblem %s",emblemDesc.GetStr());
	m_emblemMap.Delete(emblemDesc);
}

bool NetworkCrewEmblemMgr::HandleNewEmblemRequest( const EmblemDescriptor& emblemDesc )
{
	gnetAssert(m_emblemMap.Access(emblemDesc) == NULL);

	bool success = false;

	if (m_emblemMap.GetNumUsed() < MAX_NUM_EMBLEMS)
	{
		//Add an entry for this request
		EmblemData* pEmblemData = rage_new EmblemData(emblemDesc);
		gnetDebug3("Requesting emblem %s", emblemDesc.GetStr());

		if( gnetVerify(pEmblemData->Init()) )
		{
			m_emblemMap.Insert(emblemDesc, pEmblemData);
			success = true;
		}
		else
		{
			delete pEmblemData;
			success = false;
		}
	}
	else
	{
		gnetDebug3("Failed to requesting emblem %d:%d, ran out of emblems", (int)emblemDesc.m_type, (int)emblemDesc.m_id);
	}

	return success;
}

bool NetworkCrewEmblemMgr::ContentPrivsAreOk()
{
	//First check the local gamer index
	if (!CLiveManager::CheckUserContentPrivileges())
	{
		//gnetDebug3("Emble request blocked because user doesn't have CONTENT PRIVILEGE");
		return false;
	}

	return true;
}

bool NetworkCrewEmblemMgr::RequestEmblem( const EmblemDescriptor& emblemDesc  ASSERT_ONLY(, const char* requesterName) )
{
#if !__NO_OUTPUT
	if(PARAM_emblemRequestCallstack.Get())
	{
		ASSERT_ONLY(gnetDebug1("NetworkCrewEmblemMgr::RequestEmblem %s requested by %s", emblemDesc.GetStr(), requesterName));
		sysStack::PrintStackTrace();
		if (scrThread::GetActiveThread())
		{
			scrThread::GetActiveThread()->PrintStackTrace();
		}
	}
#endif // !__NO_OUTPUT

	if(!m_bAllowEmblemRequests)
	{
		gnetDebug3("Emblem request for %s blocked because !m_bAllowEmblemRequests", emblemDesc.GetStr());
		return false;
	}

	if (!ContentPrivsAreOk())
	{
		gnetDebug3("Emblem request for %s blocked because user doesn't have CONTENT PRIVILEGE", emblemDesc.GetStr());
		return false;
	}

	if (!emblemDesc.IsValid())
	{
		gnetAssertf(0, "NetworkCrewEmblemMgr: Trying to request an invalid emblem");
		return false;
	}

	//Look to see if we already have requested this emblem
	EmblemData** pEmblemDataEntry = m_emblemMap.Access(emblemDesc);

	//No request of this type yet.  Request it!
	if(!pEmblemDataEntry)
	{
		if (HandleNewEmblemRequest(emblemDesc))
		{
			pEmblemDataEntry = m_emblemMap.Access(emblemDesc);
		}
	}

	if (pEmblemDataEntry)
	{
		(*pEmblemDataEntry)->AddRef( ASSERT_ONLY(requesterName) );
		ASSERT_ONLY(gnetDebug3("Reference to emblem %s ++ (%d) requested by %s", emblemDesc.GetStr(), (*pEmblemDataEntry)->GetRefCount(), requesterName);)
		return true;
	}

	return false;
}

void NetworkCrewEmblemMgr::ReleaseEmblem( const EmblemDescriptor& emblemDesc  ASSERT_ONLY(, const char* releaserName) )
{
	if(!m_bAllowEmblemRequests)
	{
		return;
	}

	if (m_emblemMap.GetNumUsed() == 0)
	{
		return;
	}

	EmblemData** pEmblemDataEntry = m_emblemMap.Access(emblemDesc);
	if( gnetVerifyf(pEmblemDataEntry, "No emblem entry found for %s", emblemDesc.GetStr()) )
	{
		(*pEmblemDataEntry)->DecRef( ASSERT_ONLY(releaserName) );
		ASSERT_ONLY(gnetDebug3("Reference to emblem %s -- (%d) released by  %s", emblemDesc.GetStr(), (*pEmblemDataEntry)->GetRefCount(), releaserName);)
	}
}

int NetworkCrewEmblemMgr::GetEmblemTXDSlot( const EmblemDescriptor& emblemDesc )
{
	EmblemData** pEmblemDataEntry = m_emblemMap.Access(emblemDesc);
	if( pEmblemDataEntry && (*pEmblemDataEntry)->Success() ) 
	{
		return (*pEmblemDataEntry)->GetEmblemTXDSlot();
	}

	return -1;
}

bool NetworkCrewEmblemMgr::GetEmblemName( const EmblemDescriptor& emblemDesc, const char*& pOut_Result)
{
	EmblemData** pEmblemDataEntry = m_emblemMap.Access(emblemDesc);
	if( pEmblemDataEntry && (*pEmblemDataEntry)->Success() ) 
	{
		pOut_Result = (*pEmblemDataEntry)->GetEmblemTextureName(false);
		return pOut_Result != nullptr;
	}

	return false;
}

const char* NetworkCrewEmblemMgr::GetEmblemName( const EmblemDescriptor& emblemDesc )
{
	EmblemData** pEmblemDataEntry = m_emblemMap.Access(emblemDesc);
	if( pEmblemDataEntry && (*pEmblemDataEntry)->Success() ) 
	{
		return (*pEmblemDataEntry)->GetEmblemTextureName(false);
	}

	return NULL;
}

int NetworkCrewEmblemMgr::GetNumBytesForEmblemSize( rlClanEmblemSize size, bool bSizeForStreamingHeap )
{
	switch (size)
	{
	case RL_CLAN_EMBLEM_SIZE_32:
		return bSizeForStreamingHeap ? g_rscVirtualLeafSize : 2 * 1024;       // ~2k
	case RL_CLAN_EMBLEM_SIZE_64:
		{
			return bSizeForStreamingHeap ? (int)Max(g_rscVirtualLeafSize, (size_t) 4 * 1024) :  4 * 1024; //~4k
		}
		
	case RL_CLAN_EMBLEM_SIZE_128:
		{
			return bSizeForStreamingHeap ? 32 * 1024 : 22 * 1024; // 22k or nearest power of 2.	
		}
		
	case RL_CLAN_EMBLEM_SIZE_256:
			return bSizeForStreamingHeap ? 128 * 1024 : 65 * 1024; //65k or rounded up to nearest power of 2 (what a waste).

	case RL_CLAN_EMBLEM_SIZE_1024: //yeah, right...
	case RL_CLAN_EMBLEM_NUM_SIZES:
		return 0;
	}

	return 0;
}

#if __BANK
void NetworkCrewEmblemMgr::Bank_InitWidgets( bkBank *bank, const char* mgrName )
{
	char name[64];
	formatf(name, "EmblemManager (%s)", mgrName ? mgrName : "" );

	m_pBankLastEmblemTXDSlotPreview = -1;
	m_bankCrewIdRequest = 724;
	formatf(m_bankEmblemPreviewTextureName, "None");
	formatf(m_bankMemPoolStatusStr, "None");

	bank->PushGroup(name);
	{
		bank->AddText("Current Stats", m_bankMemPoolStatusStr, sizeof(m_bankMemPoolStatusStr), true);
		bank->AddButton("Dump emblem ID refs", datCallback(MFA(NetworkCrewEmblemMgr::Bank_DumpState), (datBase*) this ));
		bank->AddSeparator();
		bank->AddButton("Show Next Emblem Texture", datCallback(MFA(NetworkCrewEmblemMgr::Bank_ShowNextEmblemTexture), (datBase*) this));
		bank->AddText("Emblem Debug Current view", m_bankEmblemPreviewTextureName, sizeof(m_bankEmblemPreviewTextureName), true);
		bank->AddSeparator();
		bank->AddButton("Request Emblem for Local Primary", datCallback(MFA(NetworkCrewEmblemMgr::Bank_RequestLocalPrimary), (datBase*) this));
		bank->AddText("Crew ID to request", &m_bankCrewIdRequest, false);
		bank->AddButton("Request Emblem for Crew Id", datCallback(MFA(NetworkCrewEmblemMgr::Bank_RequestCrewId), (datBase*) this));
		bank->AddButton("Unrequest Emblem for Crew Id", datCallback(MFA(NetworkCrewEmblemMgr::Bank_UnrequestCrewEmblemCrewId), (datBase*) this));
		bank->AddButton("Dump Current BANK request table", datCallback(MFA(NetworkCrewEmblemMgr::Bank_DumpWidgetRequestList), (datBase*) this));
	}
	bank->PopGroup();
}

void NetworkCrewEmblemMgr::Bank_Update()
{
	formatf(m_bankMemPoolStatusStr, sizeof(m_bankMemPoolStatusStr), "Current Number Emblem Entries %d", m_emblemMap.GetNumUsed());
}

void NetworkCrewEmblemMgr::Bank_DumpState()
{

	EmblemMap::Iterator iter = m_emblemMap.CreateIterator();
	for (iter.Start(); !iter.AtEnd(); ++iter)
	{
		EmblemData* pEmblemData = iter.GetData();
		pEmblemData->DebugPrint();
	}
}

void NetworkCrewEmblemMgr::Bank_ShowNextEmblemTexture()
{
	EmblemData* pEmblemData = NULL;

	EmblemMap::Iterator iter = m_emblemMap.CreateIterator();
	iter.Start();

	if ((bool)iter)
	{
		if (m_pBankLastEmblemTXDSlotPreview != -1)
		{
			//Find the one we did before
			for (; !iter.AtEnd(); ++iter)
			{
				pEmblemData = iter.GetData();
				if (pEmblemData->GetEmblemTXDSlot() == m_pBankLastEmblemTXDSlotPreview)
				{
					++iter;
					break;
				}
			}

			//If we didn't find anything, restart.
			if (iter.AtEnd())
			{
				iter.Start();
			}
		}

		int txdSlot = -1;
		//Now look for the next valid emblem texture
		for(;!iter.AtEnd() && txdSlot == -1; ++iter)
		{
			pEmblemData = iter.GetData();
			txdSlot = pEmblemData->GetEmblemTXDSlot();
			if (txdSlot != -1)
			{
				formatf(m_bankEmblemPreviewTextureName, "Emblem %s - %s  [%d]", pEmblemData->GetEmblemStr(), pEmblemData->GetEmblemTextureName(), txdSlot);

				gnetDisplay("Showing Debug view of %s", m_bankEmblemPreviewTextureName);

				//check to see if the slot is already there.
				CTxdRef	ref(AST_TxdStore, txdSlot, INDEX_NONE, "");
				CDebugTextureViewerInterface::SelectTxd(ref, true, true, true);

				m_pBankLastEmblemTXDSlotPreview = txdSlot;
			}
		}
	}
}

void NetworkCrewEmblemMgr::Bank_RequestLocalPrimary()
{
	const rlClanDesc& clan = rlClan::GetPrimaryClan(0);
	if(clan.IsValid())
	{
		EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clan.m_Id, RL_CLAN_EMBLEM_SIZE_128);

		RequestEmblem(emblemDesc  ASSERT_ONLY(, "NetworkCrewEmblemMgr"));
	}
}

void NetworkCrewEmblemMgr::Bank_RequestCrewId()
{
	EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)m_bankCrewIdRequest, RL_CLAN_EMBLEM_SIZE_128);

	if(gnetVerify(RequestEmblem(emblemDesc  ASSERT_ONLY(, "NetworkCrewEmblemMgr"))))
	{
		//Add an entry to the local tracking
		int* pEntry = m_widgetRequests.Access(m_bankCrewIdRequest);
		if (pEntry)
		{
			(*pEntry)++;
		}
		else
		{
			int count = 1;
			m_widgetRequests.Insert(m_bankCrewIdRequest, count);
		}
	}
}

void NetworkCrewEmblemMgr::Bank_UnrequestCrewEmblemCrewId()
{
	//Find the id in our request table to make sure we requested it
	int* pEntry = m_widgetRequests.Access(m_bankCrewIdRequest);
	if (gnetVerify(pEntry))
	{
		EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)m_bankCrewIdRequest, RL_CLAN_EMBLEM_SIZE_128);

		ReleaseEmblem(emblemDesc  ASSERT_ONLY(, "NetworkCrewEmblemMgr"));

		//Clean up our request list
		if (*pEntry == 1)
		{
			//Remove it from the list
			m_widgetRequests.Delete(m_bankCrewIdRequest);
		}
		else
		{
			(*pEntry)--;
		}
	}
}

void NetworkCrewEmblemMgr::Bank_DumpWidgetRequestList()
{
	WidgetEmblemRequestMap::Iterator iter = m_widgetRequests.CreateIterator();

	//Find the one we did before
	for (iter.Start(); !iter.AtEnd(); ++iter)
	{
		int count = (*iter);
		gnetDebug1("%d - %d", iter.GetKey(), count);
	}
}

void NetworkCrewEmblemMgr::EmblemData::DebugPrint()
{
	gnetDisplay("----- Emblem %s -----", m_emblemDescriptor.GetStr() );
	switch (m_reqState)
	{
	case STATE_NONE:
		gnetDisplay("   Receive State: NONE");	
		break;
	case STATE_RECEIVING:
		gnetDisplay("   Receive State: STATE_RECEIVING");
		break;
	case STATE_RECVD_NOTHING:
		gnetDisplay("	Receive State: STATE_RECVD_NOTHING");
		break;
	case STATE_RECVD:
		gnetDisplay("   Receive State: STATE_RECVD");
		gnetDisplay("		TXDslot %d", m_TextureDictionarySlot);
		gnetDisplay("		TXDName %s", m_cTextureName);
		gnetDisplay("		RefCount: %d", m_refCount);
		break;
	case STATE_FAILED_RETRY_WAIT:
		gnetDisplay("	Receive State: STATE_FAILED_RETRY_WAIT");
		gnetDisplay("		Next retry %d, num tries %d", m_iNextRetryTime, m_numRetries);
		break;
	case STATE_ERROR:
		gnetDisplay("	Receive State: STATE_ERROR");
		gnetDisplay("		Next retry %d, num tries %d", m_iNextRetryTime, m_numRetries);
		break;
	}
}


#endif //__BANK


//////////////////////////////////////////////////////////////////////////
//
//	clanEmblemData - manages requests
//
//////////////////////////////////////////////////////////////////////////
bool NetworkCrewEmblemMgr::EmblemData::Init()
{
	// Make sure the clan emblem indicates that it's ready to download
	// The clan emblem should be idle when this is called
	if (gnetVerify(m_emblemDescriptor.IsValid() && m_TextureDictionarySlot == -1))
	{
		gnetAssertf(!Started(), "Trying to init a EmblemData that is not idle");
		m_reqState = STATE_NONE;
		m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
		m_TextureDictionarySlot = -1;
		return true;
	}
	else
	{
		m_reqState = STATE_ERROR;
		return false;
	}
}

void NetworkCrewEmblemMgr::EmblemData::CreateTextureCacheName()
{
	if (m_emblemDescriptor.m_type == EmblemDescriptor::EMBLEM_CLAN)
	{
		formatf(m_cTextureName, sizeof(m_cTextureName), "ClanTexture%d_%d", (int) m_emblemDescriptor.m_id, (int) m_emblemDescriptor.m_size);
	}
	else
	{
		formatf(m_cTextureName, sizeof(m_cTextureName), "TournamentTexture%d_%d", (int) m_emblemDescriptor.m_id, (int) m_emblemDescriptor.m_size);
	}
}

bool NetworkCrewEmblemMgr::EmblemData::StartDownload()
{
	bool success = false;

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!gnetVerify(m_emblemDescriptor.m_id > 0 && RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex)))
	{
		gnetDebug1("clanEmblemData::StartDownload failed to start for emblem %s due to invalid clan or gamer index", m_emblemDescriptor.GetStr());
		m_reqState = STATE_ERROR;
		return success;
	}

	// Is this texture already available for us?
	CreateTextureCacheName();

	//Construct the path name for the crew / tournament texture
	char emblemCloudPath[64];

	if (m_emblemDescriptor.m_type == EmblemDescriptor::EMBLEM_CLAN)
	{
		rlClan::GetCrewEmblemCloudPath((rlClanEmblemSize)m_emblemDescriptor.m_size, emblemCloudPath, sizeof(emblemCloudPath));

		// Fill in the descriptor for our request
		m_requestDesc.m_Type = CTextureDownloadRequestDesc::CREW_FILE;
		m_requestDesc.m_CloudMemberId.Reset(m_emblemDescriptor.m_id);

	}
	else
	{
		// tournament emblems:
		static const char* emblemSizeStrs[] = { "32", "64", "128", "256", "1024" };
		CompileTimeAssert(COUNTOF(emblemSizeStrs) == RL_CLAN_EMBLEM_NUM_SIZES);	

		formatf(emblemCloudPath, sizeof(emblemCloudPath), "test/emblem_%d_%s.dds", m_emblemDescriptor.m_id, emblemSizeStrs[m_emblemDescriptor.m_size] );
	
		m_requestDesc.m_Type = CTextureDownloadRequestDesc::TITLE_FILE;
	}

	m_requestDesc.m_GamerIndex		= localGamerIndex;
	m_requestDesc.m_CloudFilePath	= emblemCloudPath;
	m_requestDesc.m_BufferPresize	= GetNumBytesForEmblemSize((rlClanEmblemSize)m_emblemDescriptor.m_size, true);
	m_requestDesc.m_TxtAndTextureHash = m_cTextureName;
	m_requestDesc.m_CloudOnlineService = RL_CLOUD_ONLINE_SERVICE_SC;
	m_requestDesc.m_CloudRequestFlags = NETWORK_CLAN_INST.GetCrewEmblemMgr().IsKnownUnmodifiedEmblem(m_emblemDescriptor) ? eRequest_CacheForceCache : eRequest_CacheNone;

	CTextureDownloadRequest::Status retVal = CTextureDownloadRequest::REQUEST_FAILED;
	retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(m_requestHandle, m_requestDesc);

	if(retVal == CTextureDownloadRequest::IN_PROGRESS || retVal == CTextureDownloadRequest::READY_FOR_USER)
	{
		gnetAssert(m_requestHandle != CTextureDownloadRequest::INVALID_HANDLE);
#if !__NO_OUTPUT
		if (retVal == CTextureDownloadRequest::READY_FOR_USER)
		{
			gnetDebug1("DTM already has TXD ready for %s with request handle %d", m_cTextureName, m_requestHandle);
		}
#endif

		m_reqState = STATE_RECEIVING;
		success = true;
	} 
	else
	{
		gnetError("Failed Request for emblem %s (DTM result %d)", m_emblemDescriptor.GetStr(), retVal);
		m_reqState = STATE_ERROR;
		success = false;	
	}

	return success;
}

#define EMBLEM_RETRY_MS 30 * 1000
void NetworkCrewEmblemMgr::EmblemData::Update()
{
	switch(m_reqState)
	{
	case STATE_NONE:
	case STATE_RECVD:
	case STATE_RECVD_NOTHING:
	case STATE_ERROR:
		break;
	case STATE_RECEIVING:
		{
			if (!gnetVerify(m_requestHandle != CTextureDownloadRequest::INVALID_HANDLE))
			{
				m_reqState = STATE_ERROR;
				return;
			}

			if ( !DOWNLOADABLETEXTUREMGR.IsRequestPending(m_requestHandle) )
			{
				if (DOWNLOADABLETEXTUREMGR.HasRequestFailed(m_requestHandle))
				{
					//Handle Failure
					//If the failure was because there was nothign to get (404), act like it's received.
					const int failureResultCode = DOWNLOADABLETEXTUREMGR.GetTextureDownloadRequestResultCode(m_requestHandle);

					if ( failureResultCode == 404)
					{
						gnetDebug1("Error 404 for emblem for %s", m_emblemDescriptor.GetStr());
						m_reqState = STATE_RECVD_NOTHING;
					}
					//We allow a retry if the request failed because, but not failed because the file wasn't there (error 404)
					else if(m_numRetries < 3 )
					{
						//Set how long we should wait to do the retry.
						m_iNextRetryTime = fwTimer::GetSystemTimeInMilliseconds() + EMBLEM_RETRY_MS;
						m_numRetries++;
						m_reqState = STATE_FAILED_RETRY_WAIT;
					}
					else
					{
						gnetDebug1("Failed to receive emblem %s after %d tries", m_emblemDescriptor.GetStr(), m_numRetries+1);
						m_reqState = STATE_ERROR;			
					}

					//Regardless, forget about our request, because it's dead to us.
					DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_requestHandle );
					m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
				}
				else if ( DOWNLOADABLETEXTUREMGR.IsRequestReady( m_requestHandle ) )
				{
					// If the request 304'd, add that emblem descriptor to our list of known unmodified emblems.
					int resultCode = DOWNLOADABLETEXTUREMGR.GetTextureDownloadRequestResultCode(m_requestHandle);
					if (resultCode == NET_HTTPSTATUS_NOT_MODIFIED)
					{
						NETWORK_CLAN_INST.GetCrewEmblemMgr().AddUnmodifiedEmblem(m_emblemDescriptor);
					}

					//Update the slot that the texture was slammed into
					strLocalIndex txdSlot = strLocalIndex(DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_requestHandle));
					if (gnetVerifyf(g_TxdStore.IsValidSlot(txdSlot), "NetworkCrewEmblemMgr::clanEmblemData::Update() - failed to get valid txd with name %s from DTM at slot %d", m_cTextureName, txdSlot.Get()))
					{
						g_TxdStore.AddRef(txdSlot, REF_OTHER);
						m_TextureDictionarySlot = txdSlot.Get();
						gnetDebug1("Emblem %s has been received in TXD slot %d", m_emblemDescriptor.GetStr(), txdSlot.Get());
					}

					DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_requestHandle );
					m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;

					m_reqState = STATE_RECVD;
				}
				else
				{
					gnetError("DTM request is no longer active.  Somethign has gone wrong");
					m_reqState = STATE_ERROR;

					DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_requestHandle );
					m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
				}
			}
		}
		break;
	case STATE_FAILED_RETRY_WAIT:
		if (m_iNextRetryTime > 0)
		{
			if( fwTimer::GetSystemTimeInMilliseconds() > m_iNextRetryTime )
			{
				//Set it back into the initial not-Started state so it will get picked up to start again.
				m_reqState = STATE_NONE;
				m_iNextRetryTime = 0;
			}
		}
		else
		{
			m_reqState = STATE_ERROR;
		}
		break;
	}
}


void NetworkCrewEmblemMgr::EmblemData::Destroy()
{
	if (m_requestHandle != CTextureDownloadRequest::INVALID_HANDLE)
	{
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_requestHandle);
		m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
	}

	m_TextureDictionarySlot = -1;
}

 int NetworkCrewEmblemMgr::EmblemData::GetEmblemTXDSlot() const
 {
 	if( Success() && g_TxdStore.IsValidSlot(strLocalIndex(m_TextureDictionarySlot)))
 	{
 		return m_TextureDictionarySlot;
 	}
 
 	return -1;
 }

const char* NetworkCrewEmblemMgr::EmblemData::GetEmblemTextureName(bool bReturnNameEvenIfEmpty /* = true */)
{
	if( !bReturnNameEvenIfEmpty && m_reqState == STATE_RECVD_NOTHING )
		return NULL;

	return m_cTextureName;
}

void NetworkCrewEmblemMgr::EmblemData::AddRef( ASSERT_ONLY(const char* requesterName) )
{
	m_refCount++;
#if __ASSERT
	const atString key=atString(requesterName);
	if(!m_refCountOwners.Access(key))
	{
		m_refCountOwners.Insert(key, 0);
	}
	(*(m_refCountOwners.Access(key)))++;
#endif
}

void NetworkCrewEmblemMgr::EmblemData::DecRef( ASSERT_ONLY(const char* releaserName) )
{
#if __ASSERT
	const atString key=atString(releaserName);
	gnetAssertf(m_refCountOwners.Access(key), "RefCount error detected ! %s is releasing an emblem that he has never referenced.", releaserName);
	if(m_refCountOwners.Access(key))
	{
		gnetAssertf( (*(m_refCountOwners.Access(key)))>0 , "RefCount error detected ! %s is releasing an emblem it has no ref on anymore.", releaserName);
		if((*(m_refCountOwners.Access(key)))==0)
		{
			for(atMap<atString, u32>::Iterator It = m_refCountOwners.CreateIterator(); !It.AtEnd(); It.Next())
			{
				gnetDebug1("	RefCountOwner %s : %d", It.GetKey().c_str(), It.GetData());
			}
		}
	}
	else
	{
		for(atMap<atString, u32>::Iterator It = m_refCountOwners.CreateIterator(); !It.AtEnd(); It.Next())
		{
			gnetDebug1("	RefCountOwner %s : %d", It.GetKey().c_str(), It.GetData());
		}
	}

#endif
	if(AssertVerify(m_refCount > 0)) 
	{
		m_refCount--;
#if __ASSERT
		if(m_refCountOwners.Access(key))
		{
			(*(m_refCountOwners.Access(key)))--;
		}
#endif
	}
}


bool NetworkCrewEmblemMgr::RequestPending( const EmblemDescriptor& emblemDesc ) const
{
	 EmblemData *const * pEmblemDataEntry = m_emblemMap.Access(emblemDesc);
	 if( pEmblemDataEntry )
	 {
		 return (*pEmblemDataEntry)->Pending();
	 }

	 return false;
}

bool NetworkCrewEmblemMgr::RequestSucceeded( const EmblemDescriptor& emblemDesc ) const
{
	EmblemData *const * pEmblemDataEntry = m_emblemMap.Access(emblemDesc);
	if( pEmblemDataEntry )
	{
		return (*pEmblemDataEntry)->Success();
	}

	return false;
}

bool NetworkCrewEmblemMgr::RequestFailed( const EmblemDescriptor& emblemDesc ) const
{
	EmblemData *const * pEmblemDataEntry = m_emblemMap.Access(emblemDesc);
	if( pEmblemDataEntry )
	{
		return (*pEmblemDataEntry)->Failed();
	}

	return true;
}

// TODO: RequestPending, RequestSucceeded, RequestFailed and RequestRetrying are all the same! Someone refactor them in next project
bool NetworkCrewEmblemMgr::RequestRetrying( const EmblemDescriptor& emblemDesc ) const
{
	EmblemData *const * pEmblemDataEntry = m_emblemMap.Access(emblemDesc);
	if( pEmblemDataEntry )
	{
		return (*pEmblemDataEntry)->IsRetrying();
	}

	return true;
}
