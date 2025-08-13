//
// NetworkRichPresence.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef RICHPRESENCEMGR_H
#define RICHPRESENCEMGR_H

// Rage headers
#include "atl/dlist.h"
#include "data/base.h"
#include "fwtl/pool.h"
#include "atl/delegate.h"
#include "rline/rl.h"

namespace rage
{

#if RSG_DURANGO 
class rlSchema;
#endif
class netStatus;
class rlGamerInfo;
class rlPresence;
class sysMemAllocator;

#if RSG_DURANGO
//PURPOSE - Wrapper of presence ID and field ID string for the Durango Rich Presence integration with User Stats
class richPresenceFieldStat
{
public:
	richPresenceFieldStat()
	{
		m_FieldStat[0] = '\0';
		m_PresenceId = 0;
	}

	richPresenceFieldStat(int presenceId, const char * stat)
	{
		m_PresenceId = presenceId;
		safecpy(m_FieldStat, stat, RL_PRESENCE_MAX_STAT_SIZE);
	}

	char m_FieldStat[RL_PRESENCE_MAX_STAT_SIZE];
	int m_PresenceId;
};
#endif

//PURPOSE
//  Responsible for setting and queuing rich presence messages when needed.
class richPresenceMgr
{
public:

	//PURPOSE
	//  The ActivatePresenceCallback is a callback that gets called any time the Manager needs to activate a
	//  presence waiting for processing. Callback for rlPresence::SetStatusString( ... )
#if RSG_NP
	typedef atDelegate<bool (const char*)> ActivatePresenceCallback;
#elif RSG_DURANGO
	typedef atDelegate<bool (const rlGamerInfo& , const char * presenceString, netStatus*)> ActivatePresenceCallback;
	typedef atDelegate<bool (const rlGamerInfo& , const richPresenceFieldStat* stats, int numStats )> ActivatePresenceContextCallback;
#else
	typedef atDelegate<bool ()> ActivatePresenceCallback;
#endif // RSG_NP


private:

	//PURPOSE
	// Represents a rich presence message.
	class PresenceMsg
	{
		friend class  richPresenceMgr;

	private:
		//Presence Id
		int  m_Id;

#if RSG_DURANGO
		char m_PresenceStr[RL_PRESENCE_MAX_BUF_SIZE];
		richPresenceFieldStat m_FieldStat[RL_PRESENCE_MAX_FIELDS];
		int m_NumFields;
#endif

		//Presence Id
#if RSG_DURANGO
		int  m_LocalGamerInfo;
		rlSchema*  m_Schema; 
#endif

		//Our presence data that will be sent.
#if RSG_NP
		char m_Data[RL_PRESENCE_MAX_BUF_SIZE];
#endif

		//Pointer to operation status
		netStatus*  m_MyStatus;

	public:
		FW_REGISTER_CLASS_POOL(PresenceMsg);

	public:
#if RSG_NP
		PresenceMsg(const int id, const char* data) : m_Id(id), m_MyStatus(0) 
		{
			Init(data);
		}
#elif RSG_DURANGO
		PresenceMsg(const int id, const int localGamerInfo) : m_Id(id), m_MyStatus(0), m_LocalGamerInfo(localGamerInfo), m_Schema(0)
		{
			Init();
		}
#else
		PresenceMsg(const int id) : m_Id(id), m_MyStatus(0) 
		{
			Init();
		}
#endif

		~PresenceMsg()
		{
			Shutdown();
		}

		bool ActivatePresence(ActivatePresenceCallback& pCB DURANGO_ONLY(, ActivatePresenceContextCallback& cCB));

		bool Pending();
		int GetStatus();

		inline int GetID() {return m_Id;}

#if RSG_DURANGO
		void SetPresenceString(const char * pString);
		bool AddPresenceFieldStat(const richPresenceFieldStat * fieldStat);
#endif

#if RSG_DURANGO
		inline rlSchema*  GetSchemaP() { return m_Schema; }
#endif

	private:
		void Init( ORBIS_ONLY( const char* data ));
		void Shutdown();
	};

private:

	//PURPOSE
	// This is a convenience class for allocating the list nodes from a pool
	class atDPresenceNode : public atDNode<PresenceMsg*, datBase>
	{
	public:
		FW_REGISTER_CLASS_POOL(atDPresenceNode);
	};

	//Maximum number of rich presence message allowed on the list.
	//To Prevent level designer setting presence after presence ...
	static const unsigned MAX_PRESENCE_MSG = 5;

	//TRUE after initializations
	bool m_Initialized;

	//A list of rich presence messages waiting for processing.
	atDList< PresenceMsg*, datBase > m_PresenceList;

	//ActivatePresenceCallback is a callback that gets called any time the Manager needs to activate
	//  a presence msg waiting for processing.
	ActivatePresenceCallback  m_ActivatePresenceCB;

	//Memory allocator
	sysMemAllocator*  m_Allocator;

#if RSG_DURANGO
	//ActivatePresenceContextCallback is a callback that is called when setting the User Event
	// required for presence contexts. 
	ActivatePresenceContextCallback m_ActivatePresenceContextCB;
#endif

public:

	richPresenceMgr()
		: m_Initialized(false)
		, m_ActivatePresenceCB(NULL)
		, m_Allocator(NULL)
#if RSG_DURANGO
		, m_ActivatePresenceContextCB(NULL)
#endif
	{
		m_PresenceList.Empty();
	}
	~richPresenceMgr();

	// PURPOSE
	//  Initializes our pools and sets the callback that gets called for each presence that is activated.
	void Init(ActivatePresenceCallback& pCB DURANGO_ONLY(, ActivatePresenceContextCallback& cCB), sysMemAllocator* allocator);
	void Shutdown(const u32 shutdownMode);

	//PURPOSE
	//  Set a specific presence active. Params depend on the presence type.
	//PARAMS
	//  id         - ID of the presence to set.
	//  fieldIds   - array with all fields ids that must be set for the specific presence.
	//  fieldData  - array with all fields data that must be set for the specific presence.
	//  numFields  - Size of the array's fieldIds and fieldData - the number of fields that the presence has.
#if RSG_DURANGO
	void SetPresenceStatus(const int localGamerInfo, 
						   const int id, 
						   const int* fieldIds, 
						   const DURANGO_ONLY(richPresenceFieldStat*) fieldData, 
						   const unsigned numFields);
#endif

	//PURPOSE
	//  Set a specific presence active. Params depend on the presence type.
	//PARAMS
	//  id        - ID of the presence to set.
	//  data      - Seralized data, only understandable to the game.
	//  dataSize  - Size of data in bytes.
#if RSG_NP
	void SetPresenceStatus(const int id, const char* status);
#endif

	//PURPOSE
	//  Since the set of a gamer's presence data is asynchronous
	//  this method is responsible to deal precisely with that.
	//  3 simple operations:
	//   - Check for pending requests.
	//   - Every DEFAULT_PRESENCE_INTERVAL activate a queued Presence.
	//   - Deletes finished presence messages and frees space in the pool.
	void Update();

	//Returns TRUE when the rich presence manager is initialized
	bool IsInitialized() const { return m_Initialized; }

private:

	//Returns if there are free spaces in the pool or if we have managed to create free spaces.
	// We try to delete all presences that are not pending because the last should always be the one that persists.
	bool CreateFreeSpaces( );
};

} // namespace rage

#endif // RICHPRESENCEMGR_H

// EOF

