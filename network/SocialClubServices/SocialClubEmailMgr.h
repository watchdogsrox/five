//
// filename:	SocialClubEmailMgr.h
// description:	
//

#ifndef INC_SOCIALCLUBEMAILMGR_H_
#define INC_SOCIALCLUBEMAILMGR_H_

#include "data/growbuffer.h"
#include "atl/string.h"

#include "event/EventNetwork.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "network/SocialClubServices/netGamePresenceEvent.h"
#include "scene/DownloadableTextureManager.h"

#include "rline/inbox/rlinbox.h"

class CEmailPresenceEvent;

// email images currently disabled as they are not used
#define USE_EMAIL_IMAGES 0

// social club message id
typedef char SocialClubID[RL_INBOX_ID_MAX_LENGTH];

struct sScriptEmailDescriptor
{
	static const unsigned NUM_EMAIL_SUBJECT_TEXTLABELS = 2;
	static const unsigned NUM_EMAIL_CONTENTS_TEXTLABELS = 16;

	scrValue			m_id;
	scrValue			m_createPosixTime;
	scrTextLabel31		m_fromGamerTag;
	scrGamerHandle		m_fromGamerHandle;
	scrValue			m_nSubjectArrayPadding;  // an additional padding because arrays in script have a leading int
	scrTextLabel63		m_subject[NUM_EMAIL_SUBJECT_TEXTLABELS];
	scrValue			m_nContentArrayPadding;  // an additional padding because arrays in script have a leading int
	scrTextLabel63		m_contents[NUM_EMAIL_CONTENTS_TEXTLABELS];
	scrValue			m_image;
};

class CNetworkSCEmail 
{
public:

	static const int SIZEOF_SUBJECT = 128;
	static const int SIZEOF_CONTENT = 768;
	static const int EMAIL_LIFETIME_IN_SECS = 30 * 24 * 60 * 60; // 30 days
	static const int MAX_NUM_EMAIL_IMAGES = 5;

	enum State {
		STATE_NONE,
		STATE_WAITING_FOR_IMAGES,
		STATE_RCVD,
		STATE_SHOWN,
		STATE_CLEANUP,
		STATE_ERROR
	};

public:

	CNetworkSCEmail(const char* subject, const char* content);							// constructor used when sending an email			
	CNetworkSCEmail(u32 id, const char* scId, u64 createPosixTime, u64 readPosixTime);	// constructor used when receiving an email
	~CNetworkSCEmail();

	void Update();

	void Flush();

	bool IsIdle() const		  { return m_state == STATE_NONE; }
	bool Pending() const	  { return m_state == STATE_WAITING_FOR_IMAGES; }
	bool Success() const	  { return m_state == STATE_RCVD; }
	bool Failed() const		  { return m_state == STATE_ERROR; }

	bool IsReady() const { return Success(); }

	const int GetScriptId() const			{ return m_ScriptId; }
	const char* GetSCId() const				{ return m_SCId; }
	const rlGamerHandle& GetSender() const	{ return m_hFromGamer; }
	const char* GetSenderName() const		{ return m_szGamerName; }
	const char* GetSubject() const			{ return m_Subject; }
	const char* GetContent() const			{ return m_Content; }
	u64 GetCreatePosixTime() const			{ return m_CreatePosixTime; }
	u64 GetReadTime() const					{ return m_ReadPosixTime; }
	int GetImage() const					{ return m_Image; }

#if USE_EMAIL_IMAGES
	u32 GetNumImages()	const				{ return m_numImages; }

	const char* GetImageTxdName(u32 imageNum) const;
	const char* GetImageURL(u32 imageNum) const;
	int GetImageTxdSlot(u32 imageNum) const;

	void RequestImages();

	void MarkAsShown()	  { SetState(STATE_SHOWN); }
	void MarkForCleanup() { SetState(STATE_CLEANUP); }
#endif // USE_EMAIL_IMAGES

	bool Serialise(RsonWriter& rw) const;
	bool Deserialise(const RsonReader& rr);

	static void SetSubjectAndContent(sScriptEmailDescriptor* pScriptDescriptor, const char* subject, const char* content);
	static void GetSubjectAndContent(sScriptEmailDescriptor* pScriptDescriptor, char* subject, char* content);

protected:

	void SetState(State newState);
#if USE_EMAIL_IMAGES
	void ReleaseImages();
#endif
	State m_state;

	// a local id assigned by the email manager, to be used by script
	int m_ScriptId;

	// the global Social Club id
	SocialClubID m_SCId;

	//Posix time at which the email was created.
	u64 m_CreatePosixTime;

	//Posix time at which the email was read.
	u64 m_ReadPosixTime;

	// The gamer handle of the player who sent the email
	rlGamerHandle m_hFromGamer;

	// The gamer name the player who sent the email
	static const int EMAIL_MAX_NAME_BUFFER_SIZE = 32;
	char m_szGamerName[EMAIL_MAX_NAME_BUFFER_SIZE];

	// The email subject and content
	char m_Subject[SIZEOF_SUBJECT];
	char m_Content[SIZEOF_CONTENT];
	int m_Image;

#if USE_EMAIL_IMAGES
	class CEmailImage
	{
	public:

		CEmailImage()  
		  : m_texRequestHandle(CTextureDownloadRequest::INVALID_HANDLE)
		  , m_TextureDictionarySlot(-1)
		  , m_state(STATE_NONE)
		{
			m_txdName[0] = '\0';
			m_cloudImagePath[0] = '\0';
			m_cloudImagePresize = 0;
		}

		void Set(const char* txdName, const char* cloudImagePath, int cloudImagePresize);
		void Request();
		void Update();
		void ReleaseTXDRef();
		void Clear();

		bool IsReady() const	{ return m_state == STATE_RCVD; }
		bool IsPending() const	{ return m_state == STATE_WAITING_FOR_IMAGES; }
		bool HasFailed() const	{ return m_state == STATE_ERROR; }

		const char* GetTxdName() const { return m_txdName; }
		const char* GetCloudImagePath() const { return m_cloudImagePath; }
		int GetTextureDictionarySlot() const { return m_TextureDictionarySlot; }

	private:

		void SetState(State newState);

		static const unsigned SIZEOF_TXD_NAME = 64;
		static const unsigned SIZEOF_CLOUD_PATH = 128;

		char	m_txdName[SIZEOF_TXD_NAME];
		char	m_cloudImagePath[SIZEOF_CLOUD_PATH];
		int		m_cloudImagePresize;

		TextureDownloadRequestHandle m_texRequestHandle;
		int m_TextureDictionarySlot;

		State m_state;
	};

	CEmailImage m_images[MAX_NUM_EMAIL_IMAGES];
	u32 m_numImages;
	
	u32 m_ticksInShown;
#endif // USE_EMAIL_IMAGES
};

class CNetworkEmailMgr 
{
public:

	static const int MAX_EMAILS_INBOX = 10;

public:
	CNetworkEmailMgr() 
	{}

	~CNetworkEmailMgr() 
	{
		Shutdown();
	}

	static CNetworkEmailMgr& Get();

	void Init();
	void Shutdown();
	void Update();

	void OnPresenceEvent(const rlPresenceEvent* event);

	bool SetCurrentEmailTag(const char* tag);
	void SendEmail(SendFlag sendFlags, sScriptEmailDescriptor* pScriptDescriptor, const rlGamerHandle* pRecipients, const int numRecipients);

	bool RetrieveEmails(u32 firstEmail, u32 numEmails);

	bool IsRetrievalInProgress() { return m_requestStatus.Pending(); }
	int GetRetrievalStatus() const { return m_requestStatus.GetStatus(); }

	int GetNumRetrievedEmails() const { return m_emailArray.GetCount(); }

	bool GetEmailAtIndex(int index, sScriptEmailDescriptor* pScriptDescriptor);

	void DeleteEmails(int* scriptEmailIds, int numIds);

	void DeleteAllRetrievedEmails();

#if __BANK
	void InitWidgets(bkBank* bank);
#endif

private:

	void ProcessReceivedEmails();
	void FlushRetrievedEmails();
	void DeleteSCEmails(const SocialClubID* scEmailIds, u32 numIds);

	atArray<CNetworkSCEmail*> m_emailArray;

	rlPresence::Delegate m_presDelegate;

	rlInboxMessageIterator m_MsgIter;

	netStatus m_requestStatus;

	int m_nextFreeScriptId;
};

#endif // !INC_SOCIALCLUBEMAILMGR_H_
