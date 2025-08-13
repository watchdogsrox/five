// Class header
#include "Peds/PedScriptResource.h" 

// Headers
#include "fwanimation/clipsets.h"
#include "fwanimation/animmanager.h"
#include "modelinfo/PedModelInfo.h"
#include "script/script_channel.h"

////////////////////////////////////////////////////////////////////////////////

CMovementModeScriptResource::CMovementModeScriptResource(atHashWithStringNotFinal MovementModeHash, CPedModelInfo::PersonalityMovementModes::MovementModes Type, ScriptResourceRef Reference)
: m_MovementModeHash(MovementModeHash)
, m_Type(Type)
, m_Reference(Reference)
{
	scriptDisplayf("MovementMode asset requested [%s]", m_MovementModeHash.TryGetCStr());
	RequestAnims();
}

//////////////////////////////////////////////////////////////////////////////

CMovementModeScriptResource::~CMovementModeScriptResource()
{
	m_assetRequester.ClearRequiredFlags(STRFLAG_MISSION_REQUIRED);
}

//////////////////////////////////////////////////////////////////////////////

void CMovementModeScriptResource::RequestAnimsFromClipSet(const fwMvClipSetId& clipSetId)
{
	// Go through all the anim dictionaries in the clip set and request them to be streamed in
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		while(pClipSet)
		{
			if(!Verifyf(m_assetRequester.GetRequestCount() < m_assetRequester.GetRequestMaxCount(), "Asset requester full %s, max num assets %i", m_MovementModeHash.TryGetCStr(), m_assetRequester.GetRequestMaxCount()))
			{
				return;
			}

			s32 animDictIndex = fwAnimManager::FindSlotFromHashKey(pClipSet->GetClipDictionaryName().GetHash()).Get();
			if(Verifyf(animDictIndex>-1, "Can't find clip dictionary [%s]", pClipSet->GetClipDictionaryName().GetCStr()))
			{
				if(m_assetRequester.RequestIndex(animDictIndex, fwAnimManager::GetStreamingModuleId()) == -1)	// make sure we are not duplicating anim requests
				{
					m_assetRequester.PushRequest(animDictIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_MISSION_REQUIRED);		
					scriptDisplayf("-MovementMode asset [%s] requested", pClipSet->GetClipDictionaryName().TryGetCStr());
				}
			}

			// Load the fallbacks aswell if they exist
			pClipSet = fwClipSetManager::GetClipSet(pClipSet->GetFallbackId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

void CMovementModeScriptResource::RequestAnims()
{
	const CPedModelInfo::PersonalityMovementModes* pData = CPedModelInfo::FindPersonalityMovementModes(m_MovementModeHash);
	if(pData)
	{
		s32 iCount = pData->GetMovementModeCount(m_Type);
		for(s32 i = 0; i < iCount; i++)
		{
			const CPedModelInfo::PersonalityMovementModes::MovementMode& am = pData->GetMovementMode(m_Type, i);
			const atArray<CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets>& clipSets = am.GetClipSets();
			for(s32 j = 0; j < clipSets.GetCount(); j++)
			{
				RequestAnimsFromClipSet(clipSets[j].m_MovementClipSetId);
				RequestAnimsFromClipSet(clipSets[j].m_WeaponClipSetId);
				const atArray<fwMvClipSetId>& idleTransitions = clipSets[j].m_IdleTransitions;
				for(s32 k = 0; k < idleTransitions.GetCount(); k++)
				{
					RequestAnimsFromClipSet(idleTransitions[k]);
				}
				RequestAnimsFromClipSet(clipSets[j].m_UnholsterClipSetId);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

CMovementModeScriptResourceManager::Resources CMovementModeScriptResourceManager::ms_Resources;

//////////////////////////////////////////////////////////////////////////////

void CMovementModeScriptResourceManager::Init()
{
}

//////////////////////////////////////////////////////////////////////////////

void CMovementModeScriptResourceManager::Shutdown()
{
	for(s32 i = 0; i < ms_Resources.GetCount(); i++)
	{
		delete ms_Resources[i];
	}
	ms_Resources.Reset();
}

//////////////////////////////////////////////////////////////////////////////

s32 CMovementModeScriptResourceManager::RegisterResource(atHashWithStringNotFinal MovementModeHash, CPedModelInfo::PersonalityMovementModes::MovementModes Type)
{
	s32 iIndex = GetIndex(GetReference(MovementModeHash, Type));
	if(iIndex == -1)
	{
		iIndex = ms_Resources.GetCount();
		ms_Resources.PushAndGrow(rage_new CMovementModeScriptResource(MovementModeHash, Type, GetFreeReference()));
	}
	return ms_Resources[iIndex]->GetReference();
}

//////////////////////////////////////////////////////////////////////////////

void CMovementModeScriptResourceManager::UnregisterResource(ScriptResourceRef Reference)
{
	s32 iIndex = GetIndex(Reference);
	if(iIndex != -1)
	{
		delete ms_Resources[iIndex];
		ms_Resources.DeleteFast(iIndex);
	}
}

//////////////////////////////////////////////////////////////////////////////

CMovementModeScriptResource* CMovementModeScriptResourceManager::GetResource(ScriptResourceRef Reference)
{
	s32 iIndex = GetIndex(Reference);
	if(iIndex != -1)
	{
		return ms_Resources[iIndex];
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

ScriptResourceRef CMovementModeScriptResourceManager::GetReference(atHashWithStringNotFinal MovementModeHash, CPedModelInfo::PersonalityMovementModes::MovementModes Type)
{
	for(s32 i = 0; i < ms_Resources.GetCount(); i++)
	{
		if(MovementModeHash == ms_Resources[i]->GetHash() && Type == ms_Resources[i]->GetType())
		{
			return ms_Resources[i]->GetReference();
		}
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////

ScriptResourceRef CMovementModeScriptResourceManager::GetFreeReference()
{
	ScriptResourceRef BiggestRef = 0;

	s32 iCount = ms_Resources.GetCount();
	for(s32 i = 0; i < iCount; i++)
	{
		if(ms_Resources[i]->GetReference() > BiggestRef)
			BiggestRef = ms_Resources[i]->GetReference();
	}

	for(s32 i = 0; i <= BiggestRef+1; i++)
	{
		s32 j;
		for(j = 0; j < iCount; j++)
			if(i == ms_Resources[j]->GetReference())
				break;

		if(j == iCount)
			return i;
	}

	Assertf(0, "Failed to find a free CMovementModeScriptResourceManager Reference");
	return BiggestRef+1;
}

////////////////////////////////////////////////////////////////////////////// 

s32 CMovementModeScriptResourceManager::GetIndex(ScriptResourceRef Reference)
{
	for(s32 i = 0; i < ms_Resources.GetCount(); i++)
	{
		if(Reference == ms_Resources[i]->GetReference())
		{
			return i;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////////
