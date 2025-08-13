#include "SocialClubInboxManager.h"

//RAGE
#include "system/param.h"


//framework
#include "fwnet/netchannel.h"

#include "network/Live/LiveManager.h"
#include "Network/NetworkInterface.h"

//game
#include "event/EventNetwork.h"
#include "event/EventGroup.h"
#include "Network/Cloud/Tunables.h"
#include "network/SocialClubServices/GamePresenceEventDispatcher.h"
#include "network/SocialClubServices/GamePresenceEvents.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"

NETWORK_OPTIMISATIONS()

#undef __net_channel
#define __net_channel net_inboxMgr
RAGE_DEFINE_SUBCHANNEL(net, inboxMgr);

XPARAM(presevnt_sendtoself);

//////////////////////////////////////////////////////////////////////////
//						CSocialClubInboxMgr
//
//
//////////////////////////////////////////////////////////////////////////
void CSocialClubInboxMgr::Init()
{
	//Register to handle presence events.
	m_presDelegate.Bind(this, &CSocialClubInboxMgr::OnPresenceEvent);
	rlPresence::AddDelegate(&m_presDelegate);

	m_bRefreshInbox = true;
	m_bRefreshNews = true;
	m_numberUnreadMessages = 0;

#if __BANK
	m_bankDoAutoApply = false;
#endif
	
	m_MsgIter.ReleaseResources();

}

void CSocialClubInboxMgr::Shutdown()
{
	rlPresence::RemoveDelegate(&m_presDelegate);

	if (m_netStatus.Pending())
	{
		rlInbox::Cancel(&m_netStatus);
	}
	m_MsgIter.ReleaseResources();
	m_InboxNewsStoryRequest.Shutdown();
}

//Clears out the read messages and packs all the unread messages to the front of the list.
int CSocialClubInboxMgr::ClearOutReadMsgs()
{
	gnetDebug3("ClearOutReadMsgs() with %d messages", m_rcvdMsgs.GetCount() );

	int unreadMsgs = 0;
	for (int iReadIter = 0; iReadIter < m_rcvdMsgs.GetCount(); iReadIter++)
	{
		InboxMessage& currentIterMsg = m_rcvdMsgs[iReadIter];
		//If this message is valid, but not read, just count it for the count
		if (currentIterMsg.IsValid() && !currentIterMsg.IsRead())
		{
			unreadMsgs++;
		}
		//If this message is read (or empty), find the next non-read one and replace it
		else if (!currentIterMsg.IsValid() || currentIterMsg.IsRead())
		{
			//It's read.  We don't care about it no mo!  Clear it!
			m_rcvdMsgs[iReadIter].Clear();

			//Now look for an unread message later in the list to put here.
			//If we get all the way through the list and don't find any valid messages, we're done.
			bool bValidFound = false;
			for(int iUnreadIter = iReadIter+1; iUnreadIter < m_rcvdMsgs.GetCount(); iUnreadIter++)
			{
				//See if this one is unRead
				if (m_rcvdMsgs[iUnreadIter].IsValid())
				{
					bValidFound = true;
					if(!m_rcvdMsgs[iUnreadIter].IsRead())
					{
						//Replace the read one with the unread one (shifting the unread ones toward the front of the list)
						m_rcvdMsgs[iReadIter] = m_rcvdMsgs[iUnreadIter];
						unreadMsgs++;

						//Clear the data in the Unread one we just copied
						m_rcvdMsgs[iUnreadIter].Clear();

						//Stop inner loop
						break;
					}
				}
			}

			//If we went through and didn't find anymore valid messages, then we're done.
			if(!bValidFound)
			{
				break;
			}
		}
	}

	m_rcvdMsgs.SetCount(unreadMsgs);
	m_numberUnreadMessages = unreadMsgs;
	gnetDebug3(" After ClearOutReadMsgs() we currently have %d messages (ALL UNREAD)", m_rcvdMsgs.GetCount() );

	return unreadMsgs;
}

int CSocialClubInboxMgr::TransferMessagesFromUpdateBufferAndResort()
{
	//Clean up the big pool to remove items marked as read and moving all the unread
	//messages to the front of the list.
	ClearOutReadMsgs();

	//Check to see if there is anything to transfer.
	if (m_MsgIter.GetNumMessagesRetrieved() == 0)
	{
		gnetDebug3("No new message in update buffer to transfer");
		return 0;
	}

	int numberOfMessagesMoved = 0;

	//In theory, both list are sorted (newest to oldest), so the main buffer has the 
	//oldest at the end.  Replace those with our update buffer
	int openSlotsLeft = m_rcvdMsgs.GetMaxCount() - m_rcvdMsgs.GetCount();

	gnetDebug1("Transfering %d update buffer messages to msg pool with %d open slots", m_MsgIter.GetNumMessagesRetrieved(), openSlotsLeft);

	//If there are more open slots left than updated messages
	int startIndex = -1;
	if(openSlotsLeft >  m_MsgIter.GetNumMessagesRetrieved())
	{
		startIndex = m_rcvdMsgs.GetCount();
		m_rcvdMsgs.SetCount(m_rcvdMsgs.GetCount() +  m_MsgIter.GetNumMessagesRetrieved());
	}
	else //Just add them at the very end, over-writing the least relevant ones
	{
		startIndex = m_rcvdMsgs.GetMaxCount() -  m_MsgIter.GetNumMessagesRetrieved();
		m_rcvdMsgs.SetCount(m_rcvdMsgs.GetMaxCount());
	}

	gnetAssert(startIndex >= 0);
	gnetDebug3("Starting to add from update buffer at index %d for %d new messages", startIndex,  m_MsgIter.GetNumMessagesRetrieved());
	int messagesToMove =  m_MsgIter.GetNumMessagesRetrieved();
	const char* msg = NULL;
	const char* msgId = NULL;
	for (int replacementIter = 0; replacementIter < messagesToMove; replacementIter++)
	{
		InboxMessage &rIter = m_rcvdMsgs[startIndex + replacementIter];


		if(!m_MsgIter.NextMessage(&msg, &msgId, &rIter.m_CreatePosixTime, &rIter.m_ReadPosixTime))
        {
            break;
        }

		safecpy(rIter.m_Contents, msg);
		safecpy(rIter.m_Id, msgId);

		numberOfMessagesMoved++;
	}

	//Now sort the array
	m_rcvdMsgs.QSort(0, -1, CSocialClubInboxMgr::QSortCompare);

	//Update our counts
	m_numberUnreadMessages = m_rcvdMsgs.GetCount();

	return numberOfMessagesMoved;
}

//Compare function to make newer message come before the older one.
int CSocialClubInboxMgr::QSortCompare( const InboxMessage* left, const InboxMessage* right )
{
	return (int)(right->m_CreatePosixTime - left->m_CreatePosixTime);
}

void CSocialClubInboxMgr::OnRefreshReceived()
{
	gnetDebug1("Handling received inbox messages");
	//Now transfer all the unread messages from the update buffer to the new buffer.
	//We're gonna clear out the read messages first.
	int numNewMessages = TransferMessagesFromUpdateBufferAndResort();

	gnetDebug1("Transfered %d unread inbox messages with %d total unread messages",numNewMessages, m_numberUnreadMessages);

	if(numNewMessages > 0)
	{
		gnetDebug1("InboxMsgReceived event sent because %d new message received", numNewMessages);
		CEventNetwork_InboxMsgReceived evt;
		GetEventScriptNetworkGroup()->Add(evt);
	}

	//And we're done.
	m_MsgIter.ReleaseResources();

#if __BANK
	//For testing purposes we can auto apply the new messages.
	if (m_bankDoAutoApply)
	{
		//Start from the oldest new message
		for (int i = 0; i < m_rcvdMsgs.GetCount(); ++i)
		{
			if(!GetIsReadAtIndex(i))
				gnetVerify(DoApplyOnEvent(i));
		}
	}
#endif

}

//Minimize calls to rlGetPosixTime by only calling it once an update loop at most as needed.
u64 CSocialClubInboxMgr::GetUpdatedPosixTime()
{
	if (m_updatedPosixTime == 0)
	{
		m_updatedPosixTime = rlGetPosixTime();
	}

	return m_updatedPosixTime;
}


void CSocialClubInboxMgr::Update()
{
	//Clear out our update posix time every tick.  We'll update it again when we need it.
	m_updatedPosixTime = 0;

	//See if it was pending and is not done
	if (m_netStatus.Succeeded() || m_netStatus.Failed() || m_netStatus.Canceled() )
	{
		//If we succeed;
		if (m_netStatus.Succeeded())
		{
			OnRefreshReceived();

			gnetAssert(m_MsgIter.IsReleased());
		}
		else //Failed or cancelled, make sure we release the iterator
		{
			m_MsgIter.ReleaseResources();
		}

		//Reset to clear out state for next time.
		m_netStatus.Reset();
	}

	m_InboxNewsStoryRequest.Update();

	//Otherwise, if it's not doing anything, and we have a request for refrsh, do it
	if ((m_bRefreshInbox || m_bRefreshNews) && !m_netStatus.Pending() && m_MsgIter.IsReleased())
	{
		if (m_bRefreshInbox)
		{
			DoInboxRefresh();
		}
		else if(m_bRefreshNews && !m_InboxNewsStoryRequest.Pending())
		{
			m_InboxNewsStoryRequest.RequestRefresh();
			m_bRefreshNews = false;
		}
	}

#if __BANK
	Bank_UpdateFakeData();
#endif
}

void CSocialClubInboxMgr::OnPresenceEvent( const rlPresenceEvent* evt )
{
	if ( evt->GetId() == rlPresenceEventScMessage::EVENT_ID() )
	{
		const rlPresenceEventScMessage* pSCEvent = static_cast<const rlPresenceEventScMessage*>(evt);

		//We only process rlScPresenceInboxNewMessage messages here
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
					if (gh == inbxRcvd.m_Recipient)
					{
						if (inbxRcvd.IsTagPresent("gta5"))
						{
							gnetDebug1("New inbox messages ready on server for gamerIndex %d due to presence message", localGamerIndex);
							RequestInboxRefresh();
						}

						if (inbxRcvd.IsTagPresent("news"))
						{
							gnetDebug1("Inbox message with 'news' tag received");
							m_bRefreshNews = true;
						}
					}
				}

			}
		}
	}

	return;
}

void CSocialClubInboxMgr::RequestInboxRefresh()
{
	m_bRefreshInbox = true;
}

void CSocialClubInboxMgr::DoInboxRefresh()
{
	//Make sure we're not already busy
	if (m_netStatus.Pending())
	{
		gnetError("Request to refresh Inbox made while you is in progress");
		return;
	}

	//check to see if the recipient is the current gamer.
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return;
	}

	const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);
	if(!cred.IsValid())
	{
		return;
	}

	//Make use we don't have garbage floating around.  This should have been release before
	if (!gnetVerifyf(m_MsgIter.IsReleased(), "Inbox Msg Iterator hasn't been release prior to requesting more"))
	{
		return;
	}

	const char* inboxMsgTags[] = {"gta5"};
	//If we already have valid messages in the big buffer, we need to use the smaller buffer
	if (m_rcvdMsgs.GetCount() == 0)
	{
		if(gnetVerify(rlInbox::GetUnreadMessages(localGamerIndex, 
			0,								//Start from the top (most recent)
			MAX_INBOX_MSGS,					//Get at most this many
			inboxMsgTags,             //tags
			NELEM(inboxMsgTags),     //numTags
            &m_MsgIter,
			&m_netStatus)))
		{
			m_bRefreshInbox = false;
		}
	}
	else //use the update buffer
	{
		if(gnetVerify(rlInbox::GetUnreadMessages(localGamerIndex, 
			0,						  //Start from the top (most recent)
			MAX_UPDATE_BUFFER_SIZE,	  //Get at most this many
			inboxMsgTags,             //tags
			NELEM(inboxMsgTags),      //numTags
            &m_MsgIter,
			&m_netStatus)))
		{
			m_bRefreshInbox = false;
		}
	}
}

void CSocialClubInboxMgr::SendEventToCommunity( const netGamePresenceEvent& message, SendFlag sendflag )
{
	SendEventToGamers(message, sendflag, NULL, 0);
}

void CSocialClubInboxMgr::SendEventToGamers(  const netGamePresenceEvent& message, SendFlag sendflag, const rlGamerHandle* recipients, const int numRecps)
{
	int myGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(myGamerIndex))
	{
		gnetError("Invalid gamer index in SendEventToCommunity");
		return;
	}

	if(!rlPresence::IsOnline(myGamerIndex) || !rlRos::GetCredentials(myGamerIndex).IsValid())
	{
		gnetDebug1("Not able to do SendEventToCommunity when player is not online or invalid credentials");
		return;
	}

	InboxMessage pubMessageBuffer;
	RsonWriter rw(pubMessageBuffer.m_Contents, RSON_FORMAT_JSON);

	//Since the msg portion of the publish message is going to be an actual JSON object, we need to wrap it in { }
	bool success =	rw.Begin(NULL, NULL) 
		&&	gnetVerify(message.Export(&rw))
		&& rw.End();

	if (!success)
	{
		gnetError("Failed to Export PresenceEvnent %s to buffer", message.GetEventName());
		return;
	}

	int expirationTimeSecs = 30 * 60;  //30 minute expiration for now.

    if(Tunables::GetInstance().Access(ATSTRINGHASH("inbox", 0xfb6abc26), atStringHash(message.GetEventName()), expirationTimeSecs))
	{
		netDebug1("Using tunable expiration for 'inbox.%s' - %d seconds", message.GetEventName(), expirationTimeSecs);
	}

	if (expirationTimeSecs < 0)
	{
		netDebug1("Expiration is negative, meaning we shouldn't send the event.  Could be due to tunables");
		return;
	}

	//All events sent from the game get a game tag.
	const char* gameTitleTag = CGameConfig::Get().GetConfigOnlineServices().m_RosTitleName;
	m_InboxSendTagList.Add(gameTitleTag);

	const char** tagsList = NULL;
	const unsigned tagCount = (unsigned)m_InboxSendTagList.GetCount();
	if (tagCount > 0)
	{
		gnetDebug1("Found %d tags", tagCount);
		tagsList = m_InboxSendTagList.GetPointerList();
	}
	
	//First check if we have a specific list of gamers
	if(recipients != NULL && numRecps > 0)
	{
		gnetDebug1("Sending %s to list of %d gamers", message.GetEventName(), numRecps);
		rlInbox::PostMessage(myGamerIndex, recipients, numRecps, pubMessageBuffer.m_Contents, tagsList, tagCount, (unsigned)expirationTimeSecs);
	}

	if (sendflag & SendFlag_AllFriends)
	{
		gnetDebug1("Sending %s to All Friends", message.GetEventName());
		rlInbox::PostMessageToManyFriends(myGamerIndex, pubMessageBuffer.m_Contents, tagsList, tagCount, (unsigned)expirationTimeSecs);
	}

	if (sendflag & SendFlag_AllCrew)
	{
		const rlClanDesc& clanDesc = rlClan::GetPrimaryClan(myGamerIndex);
		if(clanDesc.IsValid())
		{
			gnetDebug1("Sending %s to All Crew", message.GetEventName());
			rlInbox::PostMessageToClan(myGamerIndex, clanDesc.m_Id, pubMessageBuffer.m_Contents, tagsList, tagCount, (unsigned)expirationTimeSecs);
		}
	}

	if (sendflag & SendFlag_Self || PARAM_presevnt_sendtoself.Get())
	{
		rlGamerHandle gh;
		if(rlPresence::GetGamerHandle(myGamerIndex, &gh))
		{
			gnetDebug1("Sending %s to Self", message.GetEventName());
			//rlScPresenceMessagePublish pubEvent("self", pubMessageBuffer);
			rlInbox::PostMessage(myGamerIndex, &gh, 1, pubMessageBuffer.m_Contents, tagsList, tagCount, (unsigned)expirationTimeSecs);
		}
	}
}

const InboxMessage* CSocialClubInboxMgr::GetMessageAtIndex( int index ) const
{
	if (!gnetVerify(index >= 0 && index < m_rcvdMsgs.GetMaxCount()))
	{
		return NULL;
	}

	//Make sure we're in the range of valid msgs.
	if (index < m_rcvdMsgs.GetCount())
	{
		return &m_rcvdMsgs[index];
	}

	return NULL;
}


bool CSocialClubInboxMgr::DoApplyOnEvent( int index )
{
	//Get the inbox item at this index.
	const InboxMessage* pMsg = GetMessageAtIndex(index);
	if(pMsg == NULL)
	{
		return false;
	}

	//Push the event through the machine!

	//Check to see if this a message we care about.
	//The contents of our game presence event is packed in msg, so crack that up
	RsonReader msgData(pMsg->m_Contents);
	if (netGamePresenceEvent::IsGamePresEvent(msgData))
	{
		// we don't have sender info for these
		rlScPresenceMessageSender sender;
        const rlGamerInfo* gamerInfo = NetworkInterface::GetActiveGamerInfo();
        PRESENCE_EVENT_DISPATCH.ProcessGameEvent(gamerInfo ? *gamerInfo : rlGamerInfo(), sender, msgData, "inbox");
		return true;
	}
#if __ASSERT
	else
	{
		gnetDebug3("Unhandled Message - %s", pMsg->m_Contents);
	}
#endif

	return false;
}

bool CSocialClubInboxMgr::IsIndexValid( int index ) const
{
	//Get the inbox item at this index.
	const InboxMessage* pMsg = GetMessageAtIndex(index);
	return pMsg == NULL || pMsg->IsValid();
}

bool CSocialClubInboxMgr::GetIsReadAtIndex( int index ) const
{
	//Get the inbox item at this index.
	const InboxMessage* pMsg = GetMessageAtIndex(index);
	if(pMsg == NULL)
	{
		return false;
	}

	return pMsg->IsRead();
}


bool CSocialClubInboxMgr::SetAsReadAtIndex( int index)
{
	if(!gnetVerify(index >= 0 && index < m_rcvdMsgs.GetCount()))
		return false;

	if (m_rcvdMsgs[index].IsValid() && !m_rcvdMsgs[index].IsRead())
	{
		m_rcvdMsgs[index].m_ReadPosixTime = GetUpdatedPosixTime();
		m_numberUnreadMessages--;
		return true;
	}

	return false;
}


const char* CSocialClubInboxMgr::GetEventTypeNameAtIndex( int index, char* outEventName, int strLen ) const
{
	//Get the inbox item at this index.
	const InboxMessage* pMsg = GetMessageAtIndex(index);
	if(pMsg == NULL)
	{
		return "";
	}

	//Do a lookup for the msg type in the table.
	RsonReader msgData(pMsg->m_Contents);
	if (netGamePresenceEvent::IsGamePresEvent(msgData))
	{
		netGamePresenceEvent::PresEventName eventName;
		if (netGamePresenceEvent::GetEventTypeName(msgData, eventName))
		{
			safecpy(outEventName, (const char*)&eventName, strLen);
			return outEventName;
		}
	}

	return NULL;
}

bool GetNestedMemberHelper(const char* name, RsonReader& rr)
{
	//CHECK FOR parent1:parent2:name
	const char* colon = strchr(name, ':');

	//BaseCase with no colon
	if (colon == NULL)
	{
		return rr.GetMember(name, &rr);
	}

	//Else, we need to recurse
	static const int PARENT_MAX_CHARS = 32;
	char parentName[PARENT_MAX_CHARS];

	//Figure out the parent
	int pnamelength = (int)(colon - name);
	if (pnamelength < PARENT_MAX_CHARS)
	{
		//only copy the chars up to the ':' (+1 for the terminator)
		safecpy(parentName, name, pnamelength+1);

		//Look for the parent object
		if (rr.GetMember(parentName, &rr))
		{
			//Get the remaining string (after the colon to recurse)
			const char* nextChunk = colon+1;
			if (gnetVerify(strlen(nextChunk) > 0))
			{
				return GetNestedMemberHelper(nextChunk, rr);
			}
		}
	}

	return false;
}

bool CSocialClubInboxMgr::GetDataValueForMessage( int index, const char* name, int& value ) const
{
	if( const InboxMessage* pMsg = GetMessageAtIndex(index) )
	{
		//Do a lookup for the msg type in the table.
		RsonReader msgData(pMsg->m_Contents);
		if( netGamePresenceEvent::GetEventData(msgData, msgData) )
		{
			if (GetNestedMemberHelper(name, msgData) && msgData.AsInt(value))
			{
				return true;
			}
		}
	}

	return false;
}

bool CSocialClubInboxMgr::GetDataValueForMessage( int index, const char* name, float& value ) const
{
	if( const InboxMessage* pMsg = GetMessageAtIndex(index) )
	{
		//Do a lookup for the msg type in the table.
		RsonReader msgData(pMsg->m_Contents);
		if( netGamePresenceEvent::GetEventData(msgData, msgData) )
		{
			if (GetNestedMemberHelper(name, msgData) && msgData.AsFloat(value))
			{
				return true;
			}
		}
	}

	return false;
}

bool CSocialClubInboxMgr::GetDataValueForMessage( int index, const char* name, char* value, int strLen ) const
{
	if( const InboxMessage* pMsg = GetMessageAtIndex(index) )
	{
		//Do a lookup for the msg type in the table.	
		RsonReader msgData(pMsg->m_Contents);
		if( netGamePresenceEvent::GetEventData(msgData, msgData) )
		{
			if (GetNestedMemberHelper(name, msgData) && msgData.AsString(value, strLen))
			{
				return true;
			}
		}
	}

	return false;
}

#if __BANK
void CSocialClubInboxMgr::AddWidgets( bkBank& bank )
{
	safecpy(m_bankFakeFileName, "X:/fakeInbox.csv");
	m_bankIntVariableIndex = 0;
	safecpy(m_bankCheckVariableName, "msg");

	bank.PushGroup("CSocialClubInboxMgr");
		bank.AddButton("Clear current list", datCallback(MFA(CSocialClubInboxMgr::Bank_ClearAllMessages), (datBase*) this));
		bank.AddToggle("Request Inbox Refresh", &m_bRefreshInbox);
		bank.AddButton("Dump Buffers", datCallback(MFA(CSocialClubInboxMgr::Bahk_DebugDumpBuffers), (datBase*) this));
		bank.AddSeparator();
		bank.AddText("Fake File Path", m_bankFakeFileName, sizeof(m_bankFakeFileName), false);
		bank.AddButton("Load Fake Data", datCallback(MFA(CSocialClubInboxMgr::Bank_LoadFakeData), (datBase*) this));
		bank.AddToggle("Do Auto Apply", &m_bankDoAutoApply);
		bank.AddSeparator();
		bank.AddButton("Mark Next As Read", datCallback(MFA(CSocialClubInboxMgr::Bank_MarkNextUnreadMessageAsRead), (datBase*) this));
		bank.AddSeparator();
		bank.AddText("Message Index", &m_bankIntVariableIndex, false);
		bank.AddText("Member Name", m_bankCheckVariableName, sizeof(m_bankCheckVariableName), false);
		bank.AddButton("Request Member Check", datCallback(MFA(CSocialClubInboxMgr::Bank_CheckGetValueString), (datBase*) this));
	bank.PopGroup();

}

void CSocialClubInboxMgr::Bank_CheckGetValueString()
{
	if (IsIndexValid(m_bankIntVariableIndex))
	{
		char variableValue[32];
		if (GetDataValueForMessage(m_bankIntVariableIndex, m_bankCheckVariableName, variableValue))
		{
			gnetDisplay("Message[%d] - %s : '%s'", m_bankIntVariableIndex, m_bankCheckVariableName, variableValue);
		}
		else
		{
			gnetDisplay("No variable names %s", m_bankCheckVariableName);
		}
	}
	else
	{
		gnetDisplay("Index %d i not valid", m_bankIntVariableIndex);
	}
}


void CSocialClubInboxMgr::Bank_LoadFakeData()
{
	//Load some data from a cvs to create some fake data.
	fiStream* csvFile = ASSET.Open(m_bankFakeFileName, "");

	if (csvFile)
	{
		char inputLine[512];
		int offset = 0;
		u64 currentTime = rlGetPosixTime();
		int read = 0;
		int numAdded = 0;
		gnetDisplay("Adding fake inbox MSGs");
		do 
		{
			read = fgetline(inputLine, sizeof(inputLine), csvFile);
			if (read > 0)
			{
				//process the csv line
				// <OFFSET>,<JSON OBJECT>
				char* comma = strchr(inputLine, ',');

				//Split the string at the comman
				*comma = '\0';

				if (comma)
				{
					//atoi the first half
					offset = atoi(inputLine);
					FakeInboxDataDef& newFake = m_fakeInboxMsgArray.Grow();
					newFake.Set(currentTime + offset, comma+1);
					newFake.DebugPrint();
					numAdded++;
				}
			}
		} while (read > 0);

		csvFile->Close();

		//If we added some, sort the array from least to greatest.
		if (numAdded > 0)
		{
			m_fakeInboxMsgArray.QSort(0, -1, FakeInboxDataDef::QSortCompare);
		}
	}
}

void CSocialClubInboxMgr::FakeInboxDataDef::Set( u64 timestamp, const char* msg)
{
	m_CreatePosixTime = timestamp;
	safecpy(m_Contents, msg);
}

void CSocialClubInboxMgr::FakeInboxDataDef::DebugPrint()
{
	gnetDisplay("%d - %s", (int)m_CreatePosixTime, m_Contents);
}

int CSocialClubInboxMgr::FakeInboxDataDef::QSortCompare( const FakeInboxDataDef* left, const FakeInboxDataDef* right )
{
	return (int)(right->m_CreatePosixTime - left->m_CreatePosixTime);
}


void CSocialClubInboxMgr::Bank_UpdateFakeData()
{
	if(m_netStatus.Pending() || m_bRefreshInbox)
	{
		return;
	}


	//Zip through the data and look for any fakes that are ready, staring from the end.
	u64 currentTime = rlGetPosixTime();
	for (int i = m_fakeInboxMsgArray.GetCount() - 1; i >= 0; --i )
	{
		FakeInboxDataDef& rFakeData = m_fakeInboxMsgArray[i];
		if (rFakeData.m_CreatePosixTime < currentTime)
		{
			//Send this as a fake new Inbox msg.
			Bank_AddFakeMessageToRecievedArray(rFakeData);

			//Now remove it from the list
			m_fakeInboxMsgArray.Delete(i);
		}
	}
}

void CSocialClubInboxMgr::Bank_AddFakeMessageToRecievedArray( const InboxMessage& /*newMsg*/ )
{
	//Add it to the update buffer to get transferred.
// 	if (!m_msgUpdateBuffer.IsFull())
// 	{
// 		m_msgUpdateBuffer[m_msgUpdateBuffer.m_rcvdMsgCount] = newMsg;
// 		m_msgUpdateBuffer.m_rcvdMsgCount++;
// 		m_msgUpdateBuffer.m_totalMsgCount++;
// 	}

	//Then force the netStatus to Succeeed if it's not already
	if(!m_netStatus.Succeeded())
		m_netStatus.ForceSucceeded(0);
}

void CSocialClubInboxMgr::Bank_ClearAllMessages()
{
	if(m_netStatus.Pending() || m_bRefreshInbox)
	{
		return;
	}

	gnetDisplay("Force clearing msg queue");

	m_rcvdMsgs.SetCount(0);
}

void CSocialClubInboxMgr::Bahk_DebugDumpBuffers()
{
	gnetDebug1("---- Received pool [Rcv:%d  ]------------", m_rcvdMsgs.GetCount());
	InboxMessage* pRawRcvArray = m_rcvdMsgs.GetElements();
	gnetDebug1("#, createTime, Read?, MSG");
	for (int i = 0; i < m_rcvdMsgs.GetMaxCount(); i++)
	{
		InboxMessage& rMsg = pRawRcvArray[i];
		if (rMsg.IsValid())
		{
			gnetDebug1("%d, %d, %s, %s", i, (int)rMsg.m_CreatePosixTime, rMsg.IsRead() ? "READ" :"UNREAD", rMsg.m_Contents);
		}
	}
}

void CSocialClubInboxMgr::Bank_MarkNextUnreadMessageAsRead()
{
	//Find the next unread message
	for(int i = 0; i < m_rcvdMsgs.GetCount(); i++)
	{
		if (!m_rcvdMsgs[i].IsValid())
		{
			gnetDisplay("No Unread message found in %d messages", i);
			return;
		}
		else if (!m_rcvdMsgs[i].IsRead())
		{
			gnetDisplay("Marking %d as Read", i);
			SetAsReadAtIndex(i);
			return;
		}
	}

	gnetDisplay("No unread messages found");
}


#endif  //BANK

void CSocialClubInboxMgr::AddTagToList( const char* inNewTag )
{
	m_InboxSendTagList.Add(inNewTag);
}

void CSocialClubInboxMgr::ClearTagList()
{
	m_InboxSendTagList.Clear();
}

const char* newsStoryTags[] = {"news"};
bool InboxNewsStoryRequest::RefreshNewsStories()
{
	//Make sure we're not already busy
	if (m_status.Pending())
	{
		gnetError("Request to refresh Inbox made while you is in progress");
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

	gnetDebug1("NEWSINBOX: Requesting refresh");
	gnetVerify(rlInbox::GetMessages(localGamerIndex, 
		0,								//Start from the top (most recent)
		m_rcvdMsgs.GetMaxCount(),		//Get at most this many
		newsStoryTags,					//tags
		NELEM(newsStoryTags),			//numTags
        &m_MsgIter,
		&m_status));

	return true;
}

void InboxNewsStoryRequest::Update()
{
	if (m_status.GetStatus() == netStatus::NET_STATUS_NONE)
	{
		if (m_bRefreshRequested)
		{
			if( RefreshNewsStories() )
				m_bRefreshRequested = false;
		}
	}
	else if (!m_status.Pending())
	{
		if (m_status.Succeeded())
		{
			gnetDebug1("NEWSINBOX: Refresh SUCCESS");
			ProcessReceivedInboxItems();	
		}
		else if (m_status.Failed() || m_status.Canceled())
		{
			gnetDebug1("NEWSINBOX: Refresh FAILED OR CANCELLED");
			//No Retry...it's not that important
		}

		m_status.Reset();
	}

	return;
}

void InboxNewsStoryRequest::ProcessReceivedInboxItems()
{
	gnetDebug1("NEWSINBOX: ProcessReceivedInboxItems received %d items", m_rcvdMsgs.GetCount());
	CNewsItemPresenceEvent newsMsgObject;
	u64 currentPosixTime = rlGetPosixTime();

	m_rcvdMsgs.Reset();
	int numMessage = m_MsgIter.GetNumMessagesRetrieved();
	
	if (numMessage > m_rcvdMsgs.GetMaxCount())
	{
		numMessage = m_rcvdMsgs.GetMaxCount();
	}

	m_rcvdMsgs.SetCount(numMessage);
	
	const char* msg;
	const char* msgId;
    for(int i = 0; i < m_rcvdMsgs.GetCount(); ++i)
    {
		InboxMessage& newMsg = m_rcvdMsgs[i];
        if(!m_MsgIter.NextMessage(&msg, &msgId, &newMsg.m_CreatePosixTime, &newMsg.m_ReadPosixTime))
        {
            break;
        }

        safecpy(newMsg.m_Contents, msg);
        safecpy(newMsg.m_Id, msgId);
    }

    //Release memory
    m_MsgIter.ReleaseResources();

	//Iterate through the news messages and send them to the news system
	for (int i = 0; i < m_rcvdMsgs.GetCount(); i++)
	{
		InboxMessage& rMsg = m_rcvdMsgs[i];
		if (gnetVerifyf(RsonReader::ValidateJson(rMsg.m_Contents, istrlen(rMsg.m_Contents)), "Failed json validation for '%s'", rMsg.m_Contents))
		{
			//Do a lookup for the msg type in the table.
			RsonReader msgData(rMsg.m_Contents);
			if (newsMsgObject.GetObjectAsType(msgData))
			{
				if((u64)newsMsgObject.GetActiveDate() < currentPosixTime)
					CNetworkNewsStoryMgr::Get().AddNewsStoryItem(newsMsgObject);
			}
		}
	}
}

void InboxNewsStoryRequest::Shutdown()
{
	if (m_status.Pending())
	{
		rlInbox::Cancel(&m_status);
	}
	m_MsgIter.ReleaseResources();
}


