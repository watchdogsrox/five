#ifndef _REPLAYTRACKINGINFO_H_
#define _REPLAYTRACKINGINFO_H_


#if GTA_REPLAY

class CEntity;
class CBlockInfo;

#define MAX_EXPIRY_INFOS 10

class ExpiryInfo
{
public:
	ExpiryInfo() { Reset(); }

	void Init(const int packetID) 
	{
		m_PacketID = packetID;
		Add();
	}

	void Add(){ ++m_Count; }

	bool IsInUse() const { return m_PacketID != -1; }

	int GetPacketID() const { return m_PacketID; }

	bool HasExpired() const
	{
		if( m_Count > 1 )
		{
			--m_Count;

			return true;
		}

		return false;
	}

	int	GetCount() const	{ return m_Count; }

	void Reset()
	{
		m_Count = 0;
		m_PacketID = -1;
	}

private:
	ExpiryInfo(const ExpiryInfo&);
	ExpiryInfo& operator = (const ExpiryInfo&);

	int m_PacketID;
	mutable int m_Count;
};


class CTrackedEventInfoBase
{
public:
	virtual		~CTrackedEventInfoBase()	{}
	virtual		void	reset() = 0;

	void				SetPlaybackFlags(const CReplayState& f)		{	playbackFlags = f;		}
	const CReplayState&	GetPlaybackFlags() const					{	return playbackFlags;	}

	void				SetStale(bool b)							{	if(b)	reset(); isStale = b; 	}
	bool				IsStale() const								{	return isStale;			}
	void				SetStarted(bool b)							{	isStarted = b;			}
	void				SetInterp(bool b)							{	isInterp = b;			}
	bool				IsInterp() const							{	return isInterp;		}

	ExpiryInfo*			GetFreeExpiryInfo()							
	{
		for(int i = 0; i < MAX_EXPIRY_INFOS; i++)
		{
			ExpiryInfo& info = m_ExpirePrevious[i];
			if( !info.IsInUse() )
			{
				return &info;
			}
		}

		replayAssertf(false, "Tracked event with ID %u ran out of expiry infos! Increase MAX_EXPIRY_INFOS if you need more.", trackingID);

		return NULL;
	}

	const ExpiryInfo*			GetExpiryInfo(int packetID) const
	{
		for(int i = 0; i < MAX_EXPIRY_INFOS; i++)
		{
			const ExpiryInfo& info = m_ExpirePrevious[i];

			if( info.GetPacketID() == packetID )
			{
				return &info;
			}
		}

		return NULL;
	}

	ExpiryInfo*			GetExpiryInfo(int packetID)
	{
		for(int i = 0; i < MAX_EXPIRY_INFOS; i++)
		{
			ExpiryInfo& info = m_ExpirePrevious[i];

			if( info.GetPacketID() == packetID )
			{
				return &info;
			}
		}
		
		return NULL;
	}

	void ResetExpireInfos()
	{
		for(int i = 0; i < MAX_EXPIRY_INFOS; ++i)
		{
			ExpiryInfo& info = m_ExpirePrevious[i];

			info.Reset();
		}

		allowSetStale = true;
	}

	void				AllowSetStale(bool b)				{   allowSetStale = b; }
	bool				CanSetStale()						{   return allowSetStale; }


	CEntity*	pEntity[2];
	s16			trackingID;
	bool		storedInScratch;

	CBlockInfo*	pBlockOfPrevPacket;
	u32			offsetOfPrevPacket;

	bool		updatedThisFrame;

	CReplayState		playbackFlags;

	bool		isStale;
	bool		isStarted;
	bool		isInterp;

	ExpiryInfo	m_ExpirePrevious[MAX_EXPIRY_INFOS];

	bool		allowSetStale;

#if __ASSERT
	eReplayPacketId	m_PacketId;
#endif //__ASSERT

protected:
	CTrackedEventInfoBase()
	{}

	CTrackedEventInfoBase(const CTrackedEventInfoBase&){}
};

template<typename TYPE>
class CTrackedEventInfo : public CTrackedEventInfoBase
{
public:
	CTrackedEventInfo()
	{
		reset();
	}

	explicit CTrackedEventInfo(const TYPE& pEffect)
	{
		reset();
		m_pEffect[0] = pEffect;
	}

	explicit CTrackedEventInfo(const TYPE& pEffect0, const TYPE& pEffect1)
	{
		reset();
		m_pEffect[0] = pEffect0;
		m_pEffect[1] = pEffect1;
	}

	explicit CTrackedEventInfo(const TYPE& pEffect0, const TYPE& pEffect1, const TYPE& pEffect2)
	{
		reset();
		m_pEffect[0] = pEffect0;
		m_pEffect[1] = pEffect1;
		m_pEffect[2] = pEffect2;
	}

	void		reset()
	{
		pEntity[0] = pEntity[1]	= NULL;
		trackingID			= -1;
		m_pEffect[0]		= m_pEffect[1] = m_pEffect[2] = 0;
		storedInScratch		= false;
		pBlockOfPrevPacket	= NULL;
		offsetOfPrevPacket	= (u32)-1;
		updatedThisFrame	= false;
		playbackFlags		= CReplayState(0);
		isStale				= false;
		isStarted			= true;
		isInterp			= false;
		preloadOffsetTime   = 0;
		isFirstFrame		= false;
		ResetExpireInfos();

#if __ASSERT
		m_PacketId			= PACKETID_INVALID;
#endif		
	}

	bool		isValid() const
	{
		return trackingID != -1;
	}

	void		set(const CTrackedEventInfo& other, eReplayPacketId ASSERT_ONLY(packetID) )
	{
		pEntity[0] = other.pEntity[0];
		pEntity[1] = other.pEntity[1];
		m_pEffect[0] = other.m_pEffect[0];
		m_pEffect[1] = other.m_pEffect[1];
		m_pEffect[2] = other.m_pEffect[2];

#if __ASSERT
		m_PacketId = packetID;
#endif //__ASSERT
	}

	bool		isAlive() const	{	return m_pEffect[0] != 0 || m_pEffect[1] != 0 || m_pEffect[2] != 0;	}

	bool		IsEffectAlive(int i) const	{	return m_pEffect[i] != 0;	}

	bool		operator==(const CTrackedEventInfo& rhs) const
	{
		return (m_pEffect[0] != 0 && m_pEffect[0] == rhs.m_pEffect[0]) || (trackingID != -1 && trackingID == rhs.trackingID);
	}

	const u32			GetReplayTime() const						{	return replayTime;		}
	void				SetReplayTime(const u32 time)				{	replayTime = time;		}

	const s32			GetPreloadOffsetTime() const				{	return preloadOffsetTime;		}
	void				SetPreloadOffsetTime(const s32 time)		{	preloadOffsetTime = time;		}

	void				SetIsFirstFrame(bool b)						{	isFirstFrame = b;		}
	bool				GetIsFirstFrame() const						{	return isFirstFrame;	}

	TYPE		m_pEffect[3];
	u32			replayTime;
	s32			preloadOffsetTime;
	bool		isFirstFrame;
};

#endif // GTA_REPLAY

#endif // _REPLAYTRACKINGINFO_H_
