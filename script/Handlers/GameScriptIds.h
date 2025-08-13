//
// name:		GameScriptIds.h
// description:	Project specific script ids
// written by:	John Gurney
//

#ifndef GAME_SCRIPT_IDS_H
#define GAME_SCRIPT_IDS_H

#include "fwscript/scriptid.h"
#include "fwscript/scriptobjinfo.h"
#include "fwtl/pool.h"

class GtaThread;

// ================================================================================================================
// CGameScriptId: used to identify an instance of a running script over the network
// ================================================================================================================

class CGameScriptId : public scriptId
{
public:

	static const unsigned SIZEOF_TIME_STAMP		= 32;
	static const unsigned SIZEOF_POSITION_HASH	= 32;
	static const unsigned SIZEOF_INSTANCE_ID	= 8;

public:

	CGameScriptId() 
	: m_PositionHash(0)
	, m_InstanceId(-1)
	, m_TimeStamp(0) 
	{
	}

	CGameScriptId(const scrThread& scriptThread)
	: m_PositionHash(0)
	, m_InstanceId(-1)
	, m_TimeStamp(0)
	{
		InitGameScriptId(scriptThread);
	}

	CGameScriptId(const char* scriptName, int instanceId, unsigned positionHash = 0)
	: scriptId(scriptName)
	, m_PositionHash(positionHash)
	, m_InstanceId(instanceId)
	, m_TimeStamp(0)
	{
		GenerateScriptIdHash();
	}

	CGameScriptId(const char* scriptName, Vector3& position)
		: scriptId(scriptName)
		, m_PositionHash(GeneratePositionHash(position))
		, m_InstanceId(-1)
		, m_TimeStamp(0)
	{
		GenerateScriptIdHash();
	}

	CGameScriptId(const char* scriptName)
		: scriptId(scriptName)
		, m_PositionHash(0)
		, m_InstanceId(-1)
		, m_TimeStamp(0)
	{
		GenerateScriptIdHash();
	}

	// Initialises the script id by setting the hash directly. In this case the name is not available.
	CGameScriptId(const atHashString& scriptHash, int instanceId, unsigned positionHash = 0)
		: scriptId(scriptHash)
		, m_PositionHash(positionHash)
		, m_InstanceId(instanceId)
		, m_TimeStamp(0)
	{
		GenerateScriptIdHash();
	}

	CGameScriptId(const CGameScriptId& other) 
	: scriptId(static_cast<const scriptId&>(other))
	, m_PositionHash(other.m_PositionHash)
	, m_InstanceId(other.m_InstanceId)
	, m_TimeStamp(other.m_TimeStamp)
	{ 
		GenerateScriptIdHash();
	}

	CGameScriptId(const scriptId& other) 
	: scriptId(other)
	, m_PositionHash(0)
	, m_InstanceId(-1)
	, m_TimeStamp(0)
	{ 
		const CGameScriptId* gameScriptId = SafeCast(const CGameScriptId, &other);

		if (gameScriptId)
		{
			m_scriptIdHash  = gameScriptId->m_scriptIdHash;
			m_PositionHash	= gameScriptId->m_PositionHash;
			m_InstanceId	= gameScriptId->m_InstanceId;
			m_TimeStamp		= gameScriptId->m_TimeStamp;
		}
	}

	CGameScriptId(const scriptIdBase& other) 
	: scriptId(other)
	, m_PositionHash(0)
	, m_InstanceId(-1)
	, m_TimeStamp(0)
	{ 
		const CGameScriptId* gameScriptId = SafeCast(const CGameScriptId, &other);

		if (gameScriptId)
		{
			m_scriptIdHash  = gameScriptId->m_scriptIdHash;
			m_PositionHash	= gameScriptId->m_PositionHash;
			m_InstanceId	= gameScriptId->m_InstanceId;
			m_TimeStamp		= gameScriptId->m_TimeStamp;
		}
	}

	virtual ~CGameScriptId() {}

	unsigned	GetTimeStamp() const			{ return m_TimeStamp; }
	void		SetTimeStamp(unsigned time)		{ gnetAssert(m_TimeStamp == 0); m_TimeStamp = time;}
	void		ResetTimestamp()				{ m_TimeStamp = 0; }
	int			GetInstanceId() const			{ return m_InstanceId;}
	void		SetInstanceId(int id) 			{ m_InstanceId = id; GenerateScriptIdHash(); }
	unsigned	GetPositionHash() const			{ return m_PositionHash;}

	virtual const atHashValue GetScriptNameHash() const { Assert(m_ScriptNameHash.IsNotNull()); return m_ScriptNameHash; }
	virtual atHashValue GetScriptIdHash() const { Assert(m_scriptIdHash.IsNotNull()); return m_scriptIdHash; }

	virtual void Init(const scrThread& scriptThread);

	virtual const char* GetLogName() const;

	virtual void Read(datBitBuffer &bb);
	virtual void Write(datBitBuffer &bb) const;

	virtual unsigned GetSizeOfMsgData() const
	{
		unsigned size = scriptId::GetSizeOfMsgData();

		size += SIZEOF_TIME_STAMP;

		size++;

		if (m_PositionHash != 0)
			size += SIZEOF_POSITION_HASH;

		size++;

		if (m_InstanceId != -1)
			size += SIZEOF_INSTANCE_ID;

		return size;
	}

	virtual unsigned GetMaximumSizeOfMsgData() const
	{
		unsigned size = scriptId::GetMaximumSizeOfMsgData();

		size += SIZEOF_TIME_STAMP;

		size++;
		size += SIZEOF_POSITION_HASH;

		size++;
		size += SIZEOF_INSTANCE_ID;

		return size;
	}

	virtual void Log(netLoggingInterface &log);

	// checks 2 script ids for equivalence using the script name only
	virtual bool EqualivalentScriptName(const scriptIdBase& other) const 
	{
		return scriptId::Equals(other);
	}

	void InitGameScriptId(const scrThread& scriptThread);

protected:

	virtual void From(const scriptIdBase& other)
	{
		scriptId::From(other);

		const CGameScriptId* gameScriptId = SafeCast(const CGameScriptId, &other);

		if (gameScriptId)
		{
			m_scriptIdHash  = gameScriptId->m_scriptIdHash;
			m_PositionHash	= gameScriptId->m_PositionHash;
			m_InstanceId	= gameScriptId->m_InstanceId;
			m_TimeStamp		= gameScriptId->m_TimeStamp;
		}
	}

	virtual bool Equals(const scriptIdBase& other) const 
	{
		if (scriptId::Equals(other))
		{
			const CGameScriptId* gameScriptId = SafeCast(const CGameScriptId, &other);

			if (gameScriptId)
			{
				return (m_PositionHash == gameScriptId->m_PositionHash && 
						m_InstanceId == gameScriptId->m_InstanceId && 
						(m_TimeStamp == 0 || gameScriptId->m_TimeStamp == 0 || m_TimeStamp == gameScriptId->m_TimeStamp));
			}
		}

		return false;
	}

	virtual void SetNameFromHash();

	unsigned GeneratePositionHash(const Vector3& position) const;

	void GenerateScriptIdHash();

protected:

	unsigned		m_TimeStamp;				// the time this script started, set by the host
	unsigned		m_PositionHash;				// a hash generated from the script's position (only used by ambient scripts)
	int				m_InstanceId;				// an id that is used to distinguish between different instances of the same script	
	atHashValue		m_scriptIdHash;				// the hash identifying this id
};

// ================================================================================================================
// CGameScriptObjInfo: used to identify an object registered with a script handler.
// ================================================================================================================

class CGameScriptObjInfo : public scriptObjInfo<CGameScriptId>
{
public:

	CGameScriptObjInfo(const CGameScriptId &scrId, const ScriptObjectId scrObjId, const HostToken hostToken) : 
	  scriptObjInfo<CGameScriptId>(scrId, scrObjId, hostToken)
	{
	}

	CGameScriptObjInfo(const scriptObjInfoBase &other) : 
	scriptObjInfo<CGameScriptId>(other)
	{
	}

	CGameScriptObjInfo()
	{
	}
};

#endif  // GAME_SCRIPT_IDS_H
