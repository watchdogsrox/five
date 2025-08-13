#ifndef __CREPLAYADVANCEREADER_H__
#define __CREPLAYADVANCEREADER_H__

#include "control/replay/ReplaySupportClasses.h"

#if GTA_REPLAY
class ReplayThreadData;

class CReplayClipScanner
{
public:
	enum flags
	{
		Entity = 1 << 0,
		Event = 1 << 1,
		SingleFrame = 1 << 2,
	};

	CReplayClipScanner(int type, u32 time)
		: m_scannerType(type)
		, m_readAheadTime(time)
		, m_reachedExtent(true)
		, m_expandEnvelope(true)
		, m_pNextScanner(NULL)
	{}

	virtual ~CReplayClipScanner()
	{}

	void				UpdateScan(ReplayThreadData& threadData);
	virtual bool		ProcessResults(const CReplayState&, const u32, bool, int) = 0;
	virtual void		Clear() = 0;

	virtual void		GetBlockDetails(atString&) const = 0;

	virtual bool		UpdateScanInternal(ReplayThreadData&, const CPacketFrame*, const CBlockInfo*, int) = 0;

	int					m_scannerType;
	u32					m_readAheadTime;
	bool				m_reachedExtent;
	bool				m_expandEnvelope;

	CFrameInfo			m_currentPreloadExtent;

	CReplayClipScanner* m_pNextScanner;
};


class ReplayThreadData
{
public:
	ReplayThreadData(int m, const bool& q, const CFrameInfo& ffi, const CFrameInfo& lfi, const CFrameInfo& cfi, CBufferInfo& bi)
		: mode(m)
		, quit(q)
		, firstFrameInfo(ffi)
		, lastFrameInfo(lfi)
		, currentFrameInfo(cfi)
		, resetExtent(true)
		, expandEnvelope(true)
		, flags(0)
		, bufferInfo(bi)
		, pFirstScanner(NULL)
	{}

	void Create(const char* threadName, void (*func)(void*))
	{
		triggerThread = sysIpcCreateSema(0);
		threadEnded = sysIpcCreateSema(0);
		threadDone = sysIpcCreateSema(0);

		threadID = sysIpcCreateThread(func, this, sysIpcMinThreadStackSize, PRIO_NORMAL, threadName);
	}

	void Destroy()
	{
		sysIpcSignalSema(triggerThread);
		sysIpcWaitSema(threadEnded);

		sysIpcWaitThreadExit(threadID);
		threadID = sysIpcThreadIdInvalid;

		sysIpcDeleteSema(triggerThread);
		sysIpcDeleteSema(threadEnded);
		sysIpcDeleteSema(threadDone);
	}

	void TriggerThread()
	{
		sysIpcSignalSema(triggerThread);
	}

	int					mode;
	const bool&			quit;
	const CFrameInfo&	firstFrameInfo;
	const CFrameInfo&	lastFrameInfo;
	const CFrameInfo&	currentFrameInfo;
	CBufferInfo&		bufferInfo;

	sysIpcThreadId		threadID;
	sysIpcSema			triggerThread;
	sysIpcSema			threadDone;
	sysIpcSema			threadEnded;

	bool				resetExtent;
	bool				expandEnvelope;
	
	int					flags;

	CReplayClipScanner* pFirstScanner;
};



class CReplayAdvanceReader
{
public:
	enum
	{
		ePreloadDirFwd = 1,
		ePreloadDirBack = -1,
		ePreloadDisabled = 0,
	};

public:

	CReplayAdvanceReader(const CFrameInfo& firstFrame, const CFrameInfo& lastFrame, CBufferInfo& bufferInfo);
	~CReplayAdvanceReader();

	void				Init();
	void				Shutdown();

	void				AddScanner(CReplayClipScanner* pScanner, int direction);

	void				Kick(const CFrameInfo& currentFrame, bool& resetFwd, bool& resetBack, bool expandEnvelope = true, int flags = 0);

	void				Clear();

	void				WaitForAllScanners();

	bool				HandleResults(int scannerTypes, const CReplayState& replayFlags, const u32 replayTime, bool forceLoading, int flags = 0);

	bool				IsFrameWithinCurrentEnvelope(int scannerTypes, const CFrameInfo& frame);

	bool				HasReachedExtents(int scannerTypes);

	void				GetBlockDetails(char* pStr, bool& err) const;
protected:

	static void			ThreadFunc(void* pData);

	bool				m_exitThreads;
	CFrameInfo			m_currentFrameInfo;

 	ReplayThreadData	m_fwdThreadData;
 	ReplayThreadData	m_backThreadData;

	bool				m_running;
};




#endif // GTA_REPLAY

#endif // __CREPLAYADVANCEREADER_H__
