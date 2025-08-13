#include "AIHandlingInfo.h"
#include "AIHandlingInfo_parser.h"

#include "bank\bank.h"
#include "fwsys\gameskeleton.h"
#include "parser\manager.h"
    
// Instantiate pools
FW_INSTANTIATE_CLASS_POOL(CAIHandlingInfo, CAIHandlingInfo::ms_iMaxAIHandlingInfos, atHashString("CAIHandlingInfo",0x4fce10c));
FW_INSTANTIATE_CLASS_POOL(CAICurvePoint, CAICurvePoint::ms_iMaxAICurvePoints, atHashString("CAICurvePoint",0x7cba2292));

CAICurvePoint::CAICurvePoint()
: m_Angle(0.0f)
, m_Speed(0.0f)
{

}

CAICurvePoint::~CAICurvePoint()
{

}
//////////////////////////////////////////////
// class AIHandlingInfo
//////////////////////////////////////////////
    
CAIHandlingInfo::CAIHandlingInfo()
: m_MinBrakeDistance(20.0f)
, m_MaxBrakeDistance(60.0f)
, m_MaxSpeedAtBrakeDistance(40.0f)
, m_AbsoluteMinSpeed(6.0f)
{
}

CAIHandlingInfo::~CAIHandlingInfo()
{
}

    
//////////////////////////////////////////////
// class CAIHandlingInfoMgr
//////////////////////////////////////////////
    
CAIHandlingInfoMgr::CAIHandlingInfoMgr()
{
}

CAIHandlingInfoMgr::~CAIHandlingInfoMgr()
{
	Reset();
}

////////////////////////////////////////////////////////////////////////////////

CAIHandlingInfoMgr CAIHandlingInfoMgr::ms_Instance;

////////////////////////////////////////////////////////////////////////////////

void CAIHandlingInfoMgr::Init(unsigned initMode)
{
	if(initMode == INIT_BEFORE_MAP_LOADED)
	{	
		// Initialise pools
		CAICurvePoint::InitPool( MEMBUCKET_GAMEPLAY );
		CAIHandlingInfo::InitPool( MEMBUCKET_GAMEPLAY );
		// Load
		Load();
	}
}

////////////////////////////////////////////////////////////////////////////////

void CAIHandlingInfoMgr::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		Reset();

		// Now the pools shouldn't be referenced anywhere, so shut them down
		CAIHandlingInfo::ShutdownPool();
		CAICurvePoint::ShutdownPool();
	}
}

////////////////////////////////////////////////////////////////////////////////

void CAIHandlingInfoMgr::Reset()
{
	// Delete each entry from each array
	for (s32 i=0; i<ms_Instance.m_AIHandlingInfos.GetCount(); i++)
	{
		if (ms_Instance.m_AIHandlingInfos[i])
		{
			delete ms_Instance.m_AIHandlingInfos[i];
		}
	}
	ms_Instance.m_AIHandlingInfos.Reset();
}


void CAIHandlingInfoMgr::Load()
{
	// Delete any existing data
	Reset();

	// Load
	PARSER.LoadObject("common:/data/ai/VehicleaiHandlingInfo", "meta", ms_Instance);
}

const CAIHandlingInfo* CAIHandlingInfoMgr::GetAIHandlingInfoByHash( u32 uHash )
{
	for (s32 i = 0; i < ms_Instance.m_AIHandlingInfos.GetCount(); i++)
	{
		if (uHash == ms_Instance.m_AIHandlingInfos[i]->GetName().GetHash())
		{
			return ms_Instance.m_AIHandlingInfos[i];
		}
	}
	return NULL;

}

////////////////////////////////////////////////////////////////////////////////

#if __BANK

////////////////////////////////////////////////////////////////////////////////

void CAIHandlingInfoMgr::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/ai/VehicleaiHandlingInfo", "meta", &ms_Instance, parManager::XML), "Failed to save ai vehicle info infos");
}

////////////////////////////////////////////////////////////////////////////////

void CAIHandlingInfoMgr::AddWidgets(bkBank& bank)
{
	bank.PushGroup("AI Handling Metadata");

	// Load/Save
	bank.AddButton("Load", Load);
	bank.AddButton("Save", Save);

	// Combat Infos
	bank.PushGroup("AI handling Infos");
	for(s32 i = 0; i < ms_Instance.m_AIHandlingInfos.GetCount(); i++)
	{
		bank.PushGroup(ms_Instance.m_AIHandlingInfos[i]->GetName().GetCStr());
		PARSER.AddWidgets(bank, ms_Instance.m_AIHandlingInfos[i]);
		bank.PopGroup();
	}
	bank.PopGroup(); // "AI Handling Infos"

	bank.PopGroup(); // "AI Handling Metadata"
}
#endif // __BANK
