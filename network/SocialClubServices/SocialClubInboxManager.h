//
// filename:	SocialClubInboxManager.h
// description:	
//
#include "atl/array.h"
#include "atl/singleton.h"
#include "data/base.h"
#include "data/rson.h"
#include "net/status.h"
#include "rline/inbox/rlinbox.h"
#include "rline/rlpresence.h"

#include "Network/SocialClubServices/netGamePresenceEvent.h"

namespace rage 
{
	class rlPresenceEvent;
}

#ifndef INC_SOCIALCLUBINBOXMANAGER_H_
#define INC_SOCIALCLUBINBOXMANAGER_H_

class InboxMessage
{
public:

    InboxMessage()
    {
		Clear();
    }

	void Clear()
	{
		m_CreatePosixTime = 0;
		m_ReadPosixTime = 0;
		m_Id[0] = '\0';
		m_Contents[0] = '\0';
	}

	bool IsValid() const { return m_CreatePosixTime > 0; }
	bool IsRead() const { return m_ReadPosixTime > 0; }

	//message id
	char m_Id[RL_INBOX_ID_MAX_LENGTH];
    //Posix time at which the message was created.
    u64 m_CreatePosixTime;
    //Posix time at which the message was read.
    u64 m_ReadPosixTime;
    //The contents of the message - a null terminated string.
    char m_Contents[RL_INBOX_MAX_CONTENT_LENGTH];
};

class InboxNewsStoryRequest
{
public:
	InboxNewsStoryRequest() 
		: m_bRefreshRequested(false)
	{

	}

	static const int MAX_NEWS_STORIES_INBOX = 10; //Try to make this match MAX_UPDATE_BUFFER_SIZE below to minimize template code bloat.
	void RequestRefresh() { m_bRefreshRequested = true; }
	void Update();
	bool Pending() const { return m_status.Pending(); }
	void Shutdown();

protected:
	void ProcessReceivedInboxItems();
	bool RefreshNewsStories();
	netStatus m_status;
	atFixedArray<InboxMessage,MAX_NEWS_STORIES_INBOX> m_rcvdMsgs;
	rlInboxMessageIterator m_MsgIter;
	bool m_bRefreshRequested;
};

class CSocialClubInboxMgr : public datBase
{
public:

	CSocialClubInboxMgr()
	{
	}

	void Init();
	void Shutdown();
	void Update();

	void OnPresenceEvent(const rlPresenceEvent* event);
	void RequestInboxRefresh();

	u32 GetTotalNumberOfMessages() const { return m_rcvdMsgs.GetCount(); }
	u32 GetNumberOfNewMessages() const { return GetTotalNumberOfMessages(); }

	bool GetIsReadAtIndex(int index) const;
	bool SetAsReadAtIndex(int index);
	bool IsIndexValid(int index) const;

	const char* GetEventTypeNameAtIndex(int index, char* outEventName, int strLen) const;
	template<int SIZE>
	const char* GetEventTypeNameAtIndex(int index, char (&buf)[SIZE]) const
	{
		return GetEventTypeNameAtIndex(index, buf, SIZE );
	}

	bool GetDataValueForMessage(int index, const char* name, int& value ) const;
	bool GetDataValueForMessage(int index, const char* name, float& value ) const;
	bool GetDataValueForMessage(int index, const char* name, char* value, int strLen ) const;
	template<int SIZE>
	bool GetDataValueForMessage(int index, const char* name, char (&buf)[SIZE]) const
	{
		return GetDataValueForMessage(index, name, buf, SIZE);
	}

	bool DoApplyOnEvent(int index);

	void SendEventToCommunity(const netGamePresenceEvent& message, SendFlag sendflag = (SendFlag_Inbox | SendFlag_Self));
	void SendEventToGamers(const netGamePresenceEvent& message, SendFlag sendflag, const rlGamerHandle* recipients, const int numRecps);

	template<typename T>
	bool GetObjectAsType(int index, T& object)
	{
		const InboxMessage* pMsg = GetMessageAtIndex(index);
		if(pMsg == NULL)
		{
			return false;
		}

		RsonReader msgData(pMsg->m_Contents);
		return object.GetObjectAsType(msgData);
	}

#if __BANK
	void AddWidgets(bkBank& bank);
#endif

	void AddTagToList(const char* newTag);
	void ClearTagList();

private:

	bool RefreshInProgress() const { return m_netStatus.Pending(); }

	const InboxMessage* GetMessageAtIndex(int index) const;

	void DoInboxRefresh();
	void OnRefreshReceived();

	int ClearOutReadMsgs();
	int TransferMessagesFromUpdateBufferAndResort();

	rlPresence::Delegate m_presDelegate;

	bool m_bRefreshInbox;
	bool m_bRefreshNews;

	static int QSortCompare(const InboxMessage* left, const InboxMessage* right);

	static const unsigned MAX_INBOX_MSGS = 100;
	static const unsigned MAX_UPDATE_BUFFER_SIZE = 10;

	atFixedArray<InboxMessage,MAX_INBOX_MSGS> m_rcvdMsgs;
	rlInboxMessageIterator m_MsgIter;

	unsigned m_numberUnreadMessages;

	u64 m_updatedPosixTime;
	u64 GetUpdatedPosixTime();

	InboxNewsStoryRequest m_InboxNewsStoryRequest;

	netStatus m_netStatus;

	rlInboxTagList m_InboxSendTagList;
	

#if __BANK
	void Bahk_DebugDumpBuffers();
	void Bank_LoadFakeData();

	class FakeInboxDataDef : public InboxMessage
	{
	public:
		FakeInboxDataDef() : InboxMessage() {}
		void Set(u64 timestamp, const char* msg);
		void DebugPrint();

		static int QSortCompare(const FakeInboxDataDef* left, const FakeInboxDataDef* right);
	};

	void Bank_UpdateFakeData();
	void Bank_AddFakeMessageToRecievedArray(const InboxMessage& newMsg);
	void Bank_ClearAllMessages();

	void Bank_MarkNextUnreadMessageAsRead();
	void Bank_CheckGetValueString();

	int	 m_bankIntVariableIndex;
	char m_bankCheckVariableName[32];
	char m_bankFakeFileName[128];
	atArray<FakeInboxDataDef> m_fakeInboxMsgArray;
	bool m_bankDoAutoApply;
#endif
};
typedef atSingleton<CSocialClubInboxMgr> CSocialClubInboxMgrSingleton;
#define SC_INBOX_MGR CSocialClubInboxMgrSingleton::GetInstance()

#endif // !INC_SOCIALCLUBINBOXMANAGER_H_
