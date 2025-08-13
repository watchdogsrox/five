//
// name:        NetworkDebugMsgs.h
// description: Messages used for network debugging
// written by:  John Gurney
//

#ifndef NETWORK_DEBUG_MSGS_H
#define NETWORK_DEBUG_MSGS_H

// rage headers
#include "data/bitbuffer.h"
#include "net/message.h"
#include "fwnet/netchannel.h"
#include "string/string.h"

namespace rage
{
#if !__FINAL
//PURPOSE
// Sent when a player is trying to start a new script
class msgDebugTimeScale
{
public:

	NET_MESSAGE_DECL(msgDebugTimeScale, MSG_DEBUG_TIME_SCALE);

	msgDebugTimeScale() :
	m_Scale(1.0f)
	{
	}

	msgDebugTimeScale(float scale) : 
	m_Scale(scale)
	{ 
	}

	NET_MESSAGE_SER(bb, message)
	{
		return bb.SerFloat(message.m_Scale);
	}

	int GetMessageDataBitSize() const
	{
		return (sizeof(m_Scale));
	}

	float m_Scale;
};

class msgDebugTimePause
{
public:

	NET_MESSAGE_DECL(msgDebugTimePause, MSG_DEBUG_TIME_PAUSE);

	msgDebugTimePause() : 
	m_Paused(false),
	m_ScriptsPaused(false),
	m_SingleStep(false)
	{
	}

	msgDebugTimePause(bool paused, bool scriptsPaused, bool singleStep) :
	m_Paused(paused),
	m_ScriptsPaused(scriptsPaused),
	m_SingleStep(singleStep)
	{
	}

	NET_MESSAGE_SER(bb, message)
	{
		return bb.SerBool(message.m_Paused) && bb.SerBool(message.m_ScriptsPaused) && bb.SerBool(message.m_SingleStep);
	}

	int GetMessageDataBitSize() const
	{
		return 3;
	}

	bool m_Paused;
	bool m_ScriptsPaused;
	bool m_SingleStep;
};
#endif // !__FINAL

#if __BANK

class msgDebugNoTimeouts
{
public:

	NET_MESSAGE_DECL(msgDebugNoTimeouts, MSG_DEBUG_NO_TIMEOUTS);

	msgDebugNoTimeouts() : m_NoTimeouts(false) { }
	msgDebugNoTimeouts(bool noTimeout) : m_NoTimeouts(noTimeout) { }

	NET_MESSAGE_SER(bb, message) { return bb.SerBool(message.m_NoTimeouts); }

	int GetMessageDataBitSize() const { return 1; }

	bool m_NoTimeouts;
};

class msgDebugAddFailMark
{
public:

	static const unsigned MAX_FAIL_MSG_SIZE = 90;

	NET_MESSAGE_DECL(msgDebugAddFailMark, MSG_DEBUG_ADD_FAIL_MARK);

	msgDebugAddFailMark() : m_bVerboseFailmark(false) { }
	msgDebugAddFailMark(const char* failMarkMsg, const bool bVerboseFailmark) 
	: m_bVerboseFailmark(bVerboseFailmark)
	{
		gnetAssert(strlen(failMarkMsg) < MAX_FAIL_MSG_SIZE);
		formatf(m_FailMarkMsg, MAX_FAIL_MSG_SIZE, failMarkMsg);
	}

	NET_MESSAGE_SER(bb, message) { return bb.SerStr(message.m_FailMarkMsg, MAX_FAIL_MSG_SIZE) && bb.SerBool(message.m_bVerboseFailmark); }

	int GetMessageDataBitSize() const { return MAX_FAIL_MSG_SIZE; }

	char m_FailMarkMsg[MAX_FAIL_MSG_SIZE];
	bool m_bVerboseFailmark;
};

class msgSyncDRDisplay
{
public:
	int m_iNetTime;
	float m_fZoom;
	float m_fOffset;
	bool m_bDisplayOn;

	NET_MESSAGE_DECL(msgSyncDRDisplay, MSG_SYNC_DR_DISPLAY);

	bool operator!=(const msgSyncDRDisplay &rOther)
	{
		return	(m_iNetTime != rOther.m_iNetTime)
			||	(m_fZoom != rOther.m_fZoom)
			||	(m_fOffset != rOther.m_fOffset)
			||	(m_bDisplayOn != rOther.m_bDisplayOn);
	}

	void operator=(const msgSyncDRDisplay &rOther)
	{
		m_iNetTime	 = rOther.m_iNetTime;
		m_fZoom		 = rOther.m_fZoom;
		m_fOffset	 = rOther.m_fOffset;
		m_bDisplayOn = rOther.m_bDisplayOn;
	}

	msgSyncDRDisplay()
		:m_iNetTime(0)
		,m_fZoom(0.0f)
		,m_fOffset(0.0f)
		,m_bDisplayOn(false)
	{ }
	
	msgSyncDRDisplay(u32 iNetTime, bool bDisplayOn, float fZoom, float fOffset) 
		:m_iNetTime(iNetTime)
		,m_fZoom(fZoom)
		,m_fOffset(fOffset)
		,m_bDisplayOn(bDisplayOn)
	{
	}

	NET_MESSAGE_SER(bb, message) { return bb.SerInt(message.m_iNetTime, 32) && bb.SerFloat(message.m_fZoom) && bb.SerFloat(message.m_fOffset) && bb.SerBool(message.m_bDisplayOn); }
};
#endif // __BANK


} // namespace rage

#endif  // NETWORK_DEBUG_MSGS_H
