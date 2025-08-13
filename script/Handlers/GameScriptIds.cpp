//
// name:		GameScriptIds.cpp
// description:	Project specific script ids
// written by:	John Gurney
//
#include "script/handlers/GameScriptIds.h"

#include "fwnet/netlog.h"

#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Objects/object.h"
#include "fwscene/world/WorldLimits.h"
#include "script/script.h"
#include "script/script_brains.h"
#include "script/streamedscripts.h"

NETWORK_OPTIMISATIONS()

void CGameScriptId::Init(const scrThread& scriptThread)
{
	scriptId::Init(scriptThread);

	InitGameScriptId(scriptThread);
}

const char* CGameScriptId::GetLogName() const
{
	static char name[200];

	formatf(name, 200, m_ScriptName);

	if (m_PositionHash != 0)
	{
		safecatf(name, 200, ", Pos(%d)", m_PositionHash);
	}
	
	if (m_InstanceId != -1)
	{
		safecatf(name, 200, ", Inst(%d)", m_InstanceId);
	}

	safecatf(name, 200, ", Time(%d)", m_TimeStamp);

	return name;
}

void CGameScriptId::Read(datBitBuffer &bb)
{
	scriptId::Read(bb);

	bool bHasPosition, bHasInstanceId;

	bb.ReadUns(m_TimeStamp, SIZEOF_TIME_STAMP);

	bb.ReadBool(bHasPosition);

	if (bHasPosition)
		bb.ReadUns(m_PositionHash, SIZEOF_POSITION_HASH);
	else
		m_PositionHash = 0;

	bb.ReadBool(bHasInstanceId);

	if (bHasInstanceId)
	{
		unsigned u;
		bb.ReadUns(u, SIZEOF_INSTANCE_ID);
		m_InstanceId = static_cast<int>(u);
	}
	else
		m_InstanceId = -1;

	GenerateScriptIdHash();
}

void CGameScriptId::Write(datBitBuffer &bb) const
{
	scriptId::Write(bb);

	bool bHasPosition	= (m_PositionHash != 0);
	bool bHasInstanceId	= (m_InstanceId != -1);

	bb.WriteUns(m_TimeStamp, SIZEOF_TIME_STAMP);

	bb.WriteBool(bHasPosition);

	if (bHasPosition)
		bb.WriteUns(m_PositionHash, SIZEOF_POSITION_HASH);

	bb.WriteBool(bHasInstanceId);

	if (bHasInstanceId)
		bb.WriteUns(static_cast<int>(m_InstanceId), SIZEOF_INSTANCE_ID);
}

void CGameScriptId::Log(netLoggingInterface &log)
{
	scriptId::Log(log);

	log.WriteDataValue("Time Stamp", "%d", m_TimeStamp);

	if (m_PositionHash != 0)
	{
		log.WriteDataValue("Position Hash", "%d", m_PositionHash);
	}

	if (m_InstanceId != -1)
	{
		log.WriteDataValue("Instance id", "%d", m_InstanceId);
	}
}

void CGameScriptId::InitGameScriptId(const scrThread& scriptThread)
{
	scriptId::InitScriptId(scriptThread);

	const GtaThread& gtaThread = static_cast<const GtaThread&>(scriptThread);

	// use the instance id to store the thread id in SP, so we can have multiple versions of the same script running
	if (gtaThread.bSafeForNetworkGame)
	{
		m_InstanceId = gtaThread.InstanceId;

#ifdef USE_SCRIPT_NAME
		NETWORK_QUITF(m_InstanceId>=-1 && m_InstanceId<(1<<SIZEOF_INSTANCE_ID), "Script %s: instance id is invalid (%d) - should be -1 or less than %d", m_ScriptName, m_InstanceId, (1<<SIZEOF_INSTANCE_ID));
#else
		NETWORK_QUITF(m_InstanceId>=-1 && m_InstanceId<(1<<SIZEOF_INSTANCE_ID), "Instance id is invalid (%d) - should be -1 or less than %d", m_InstanceId, (1<<SIZEOF_INSTANCE_ID));
#endif
	}
	else
	{
		m_InstanceId = gtaThread.GetThreadId()<<SIZEOF_INSTANCE_ID; // we need to shift this out of the range of instance ids set by script, so there is no clash when the thread is registered.
	}

	m_PositionHash = 0;

	// ambient scripts have a position in the world, and we can have multiple scripts of the same
	// type running in the same area. Therefore we need to store this position as a hash in order to identify
	// this script instance
	if (gtaThread.ScriptBrainType == CScriptsForBrains::WORLD_POINT)
	{
		C2dEffect *pWorldPointRunningCurrentScript = &CModelInfo::GetWorldPointStore().GetEntry(fwFactory::GLOBAL, gtaThread.EntityIndexForScriptBrain);

		if (scriptVerify(pWorldPointRunningCurrentScript))
		{
			Vector3 scriptPos;
			pWorldPointRunningCurrentScript->GetPos(scriptPos);

			m_PositionHash = GeneratePositionHash(scriptPos);
		}
	}
	else if (gtaThread.ScriptBrainType == CScriptsForBrains::OBJECT_STREAMED)
	{
		const CObject *pObject = CTheScripts::GetEntityToQueryFromGUID<CObject>(gtaThread.EntityIndexForScriptBrain, 0);
		if (pObject)
		{
			m_PositionHash = GeneratePositionHash(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()));
		}
	}

	GenerateScriptIdHash();
}

void CGameScriptId::SetNameFromHash()
{
	if (AssertVerify(m_ScriptNameHash.IsNotNull()))
	{
		s32 slot = g_StreamedScripts.FindSlotFromHashKey(m_ScriptNameHash).Get();

		if (AssertVerify(slot != -1))
		{
			strncpy(m_ScriptName, g_StreamedScripts.GetScriptName(slot), SCRIPT_NAME_LEN);
		}
	}
}

unsigned CGameScriptId::GeneratePositionHash(const Vector3& position) const
{
	Vector3 pos = position;
	unsigned hash = 0;

	Assertf(pos.z >= -104.0f, "pos.z is %f. It should be >= -104.0f", pos.z);	// allow z within -104..919m range		(4m accuracy)
	Assertf(pos.z <= 919.0f, "pos.z is %f. It should be <= 919.0f", pos.z);	//	The upper limit is (1023 - whatever we add on below)
	Assert(pos.x >= WORLDLIMITS_REP_XMIN);
	Assert(pos.x <= WORLDLIMITS_REP_XMAX);
	Assert(pos.y >= WORLDLIMITS_REP_YMIN);
	Assert(pos.y <= WORLDLIMITS_REP_YMAX);

	pos.y += WORLDLIMITS_REP_YMAX;
	pos.x += WORLDLIMITS_REP_XMAX;

	unsigned	zPart = (((s32)(rage::Floorf(pos.z + 0.5f))) + 104) / 4;
	unsigned	yPart = ((s32)(rage::Floorf(pos.y + 0.5f)));
	unsigned	xPart = ((s32)(rage::Floorf(pos.x + 0.5f)));

	xPart &= 0xfff;	//	xPart and yPart are in the range 0...16384
	yPart &= 0xfff;	//	That would require 15 bits, but we only have 12 bits. I'm going to try ignoring anything above the lower 12 bits.
					//	This means that the hash will be equal for any two positions where the x or y differs by a multiple of 4096
					//	This is to fix GTA5 Bug 1553369 where two cash registers are two metres apart in the y, but ORing the 14th bit of x was causing the hashes to be identical.


	hash = zPart << 24;
	hash = hash | (yPart << 12);
	hash = hash | xPart;

	scriptAssertf(hash != 0,"bad hash");

	return hash;
}

void CGameScriptId::GenerateScriptIdHash()
{
	u32 hash = m_ScriptNameHash.GetHash();

	if (m_InstanceId == -1)
	{
		hash += m_PositionHash;
	}
	else
	{
		hash += m_InstanceId+1;
	}

	// The timestamp is not used because we want to be able to match script ids that have a 0 timestamp with those that have one assigned.
	// Also, we should not have 2 scripts with different non-zero timestamps running at the same time.

	m_scriptIdHash.SetHash(hash);
}
