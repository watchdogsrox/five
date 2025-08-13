//
// Physics/BlendFromNmData.cpp
//
// Copyright (C) 1999-2014 Rockstar Games. All Rights Reserved.
//



#include "BlendFromNmData.h"
#include "BlendFromNmData_parser.h"

#include "file/asset.h"
#include "parser/manager.h"

#include "ai/task/taskchannel.h"
#include "fwanimation/clipsets.h"

#include "task\System\TaskMove.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfo.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfoMetadataMgr.h"

PARAM(validateBlendOutSets, "[NmBlendOutSetManager] Validate blend out clip sets on startup.");

// --- Globals ------------------------------------------------------------------

CNmBlendOutSetManager g_NmBlendOutPoseManager;

const eNmBlendOutSet NMBS_INVALID((u32)0);
const eNmBlendOutSet NMBS_STANDING("NMBS_STANDING",0x623AAD01);
const eNmBlendOutSet NMBS_STANDING_AIMING_CENTRE("NMBS_STANDING_AIMING_CENTRE",0xC782C536);
const eNmBlendOutSet NMBS_STANDING_AIMING_LEFT("NMBS_STANDING_AIMING_LEFT",0x845C1598);
const eNmBlendOutSet NMBS_STANDING_AIMING_RIGHT("NMBS_STANDING_AIMING_RIGHT",0xC68E2703);

const eNmBlendOutSet NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT("NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT",0x8F00B858);
const eNmBlendOutSet NMBS_STANDING_AIMING_LEFT_PISTOL_HURT("NMBS_STANDING_AIMING_LEFT_PISTOL_HURT",0xA92C5426);
const eNmBlendOutSet NMBS_STANDING_AIMING_RIGHT_PISTOL_HURT("NMBS_STANDING_AIMING_RIGHT_PISTOL_HURT",0xCC4EB416);

const eNmBlendOutSet NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT("NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT",0xD312537F);
const eNmBlendOutSet NMBS_STANDING_AIMING_LEFT_RIFLE_HURT("NMBS_STANDING_AIMING_LEFT_RIFLE_HURT",0xCA8C6227);
const eNmBlendOutSet NMBS_STANDING_AIMING_RIGHT_RIFLE_HURT("NMBS_STANDING_AIMING_RIGHT_RIFLE_HURT",0x564F24C0);

const eNmBlendOutSet NMBS_STANDING_AIMING_CENTRE_RIFLE("NMBS_STANDING_AIMING_CENTRE_RIFLE",0xA6A601B6);
const eNmBlendOutSet NMBS_STANDING_AIMING_LEFT_RIFLE("NMBS_STANDING_AIMING_LEFT_RIFLE",0x7E3993FF);
const eNmBlendOutSet NMBS_STANDING_AIMING_RIGHT_RIFLE("NMBS_STANDING_AIMING_RIGHT_RIFLE",0x13DEB269);

const eNmBlendOutSet NMBS_INJURED_STANDING("NMBS_INJURED_STANDING",0x5353C18A); 
const eNmBlendOutSet NMBS_STANDARD_GETUPS("NMBS_STANDARD_GETUPS",0xBD4FA46D);
const eNmBlendOutSet NMBS_STANDARD_GETUPS_FEMALE("NMBS_STANDARD_GETUPS_FEMALE",0x75685DDF);
const eNmBlendOutSet NMBS_STANDARD_CLONE_GETUPS("NMBS_STANDARD_CLONE_GETUPS",0x06f51a49);
const eNmBlendOutSet NMBS_STANDARD_CLONE_GETUPS_FEMALE("NMBS_STANDARD_CLONE_GETUPS_FEMALE",0x2cb7da1b);
const eNmBlendOutSet NMBS_PLAYER_GETUPS("NMBS_PLAYER_GETUPS",0x874F80A);
const eNmBlendOutSet NMBS_PLAYER_MP_GETUPS("NMBS_PLAYER_MP_GETUPS",0xFBCCC2E3);
const eNmBlendOutSet NMBS_PLAYER_MP_CLONE_GETUPS("NMBS_PLAYER_MP_CLONE_GETUPS",0x26dfa508);
const eNmBlendOutSet NMBS_SLOW_GETUPS("NMBS_SLOW_GETUPS",0x427AE27);
const eNmBlendOutSet NMBS_SLOW_GETUPS_FEMALE("NMBS_SLOW_GETUPS_FEMALE",0xE1E497D5);
const eNmBlendOutSet NMBS_FAST_GETUPS("NMBS_FAST_GETUPS",0xA819D6C3);
const eNmBlendOutSet NMBS_FAST_GETUPS_FEMALE("NMBS_FAST_GETUPS_FEMALE",0x9D21B126);
const eNmBlendOutSet NMBS_FAST_CLONE_GETUPS("NMBS_FAST_CLONE_GETUPS",0x4f35058e);
const eNmBlendOutSet NMBS_FAST_CLONE_GETUPS_FEMALE("NMBS_FAST_CLONE_GETUPS_FEMALE",0x24aa7267);

const eNmBlendOutSet NMBS_ACTION_MODE_GETUPS("NMBS_ACTION_MODE_GETUPS",0x684af351);
const eNmBlendOutSet NMBS_ACTION_MODE_CLONE_GETUPS("NMBS_ACTION_MODE_CLONE_GETUPS",0xde65acaa);
const eNmBlendOutSet NMBS_PLAYER_ACTION_MODE_GETUPS("NMBS_PLAYER_ACTION_MODE_GETUPS",0xe51e47f7);
const eNmBlendOutSet NMBS_PLAYER_MP_ACTION_MODE_GETUPS("NMBS_PLAYER_MP_ACTION_MODE_GETUPS",0x974d873c);
const eNmBlendOutSet NMBS_PLAYER_MP_ACTION_MODE_CLONE_GETUPS("NMBS_PLAYER_MP_ACTION_MODE_CLONE_GETUPS",0x9a269bfd);

const eNmBlendOutSet NMBS_MELEE_STANDING("NMBS_MELEE_STANDING",0x66a10718);
const eNmBlendOutSet NMBS_MELEE_GETUPS("NMBS_MELEE_GETUPS",0xc69eb071);

const eNmBlendOutSet NMBS_ARMED_1HANDED_GETUPS("NMBS_ARMED_1HANDED_GETUPS",0x36EA7085);
const eNmBlendOutSet NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND("NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND",0xF5A377CF);
const eNmBlendOutSet NMBS_ARMED_2HANDED_GETUPS("NMBS_ARMED_2HANDED_GETUPS",0x6DF632E);
const eNmBlendOutSet NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND("NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND",0x8C350968);
const eNmBlendOutSet NMBS_INJURED_GETUPS("NMBS_INJURED_GETUPS",0x9EAF445);
const eNmBlendOutSet NMBS_INJURED_ARMED_GETUPS("NMBS_INJURED_ARMED_GETUPS",0x21D833F3);
const eNmBlendOutSet NMBS_INJURED_PLAYER_GETUPS("NMBS_INJURED_PLAYER_GETUPS",0x4a2d21c0);
const eNmBlendOutSet NMBS_INJURED_PLAYER_MP_GETUPS("NMBS_INJURED_PLAYER_MP_GETUPS",0x2c4505ec);
const eNmBlendOutSet NMBS_INJURED_PLAYER_MP_CLONE_GETUPS("NMBS_INJURED_PLAYER_MP_CLONE_GETUPS",0xf316f647);
const eNmBlendOutSet NMBS_CUFFED_GETUPS("NMBS_CUFFED_GETUPS",0xD8121FB1);
const eNmBlendOutSet NMBS_WRITHE("NMBS_WRITHE",0xDD7820AB);
const eNmBlendOutSet NMBS_DRUNK_GETUPS("NMBS_DRUNK_GETUPS",0x941C3847);
const eNmBlendOutSet NMBS_PANIC_FLEE("NMBS_PANIC_FLEE",0x82DCD527);
const eNmBlendOutSet NMBS_PANIC_FLEE_INJURED("NMBS_PANIC_FLEE_INJURED",0x432BF807);
const eNmBlendOutSet NMBS_ARREST_GETUPS("NMBS_ARREST_GETUPS",0xDAA031EC);
const eNmBlendOutSet NMBS_FULL_ARREST_GETUPS("NMBS_FULL_ARREST_GETUPS",0x92E9D987);

#if CNC_MODE_ENABLED
const eNmBlendOutSet NMBS_INCAPACITATED_GETUPS("NMBS_INCAPACITATED_GETUPS", 0x766BBE81);
const eNmBlendOutSet NMBS_INCAPACITATED_GETUPS_02("NMBS_INCAPACITATED_GETUPS_02", 0x336F5793);
#endif

const eNmBlendOutSet NMBS_UNDERWATER_GETUPS("NMBS_UNDERWATER_GETUPS",0xF1ECD044);
const eNmBlendOutSet NMBS_SWIMMING_GETUPS("NMBS_SWIMMING_GETUPS",0x7CF94B99);
const eNmBlendOutSet NMBS_DIVING_GETUPS("NMBS_DIVING_GETUPS",0x7FA67977);
const eNmBlendOutSet NMBS_SKYDIVE_GETUPS("NMBS_SKYDIVE_GETUPS",0x862D45BA);
const eNmBlendOutSet NMBS_DEATH_HUMAN("NMBS_DEATH_HUMAN",0xD9AC657);
const eNmBlendOutSet NMBS_DEATH_HUMAN_OFFSCREEN("NMBS_DEATH_HUMAN_OFFSCREEN",0xcf5979be);
const eNmBlendOutSet NMBS_DEAD_FALL("NMBS_DEAD_FALL",0x7e1395d1);
const eNmBlendOutSet NMBS_CRAWL_GETUPS("NMBS_CRAWL_GETUPS",0xB70C7B8E);
#if FPS_MODE_SUPPORTED
const eNmBlendOutSet NMBS_FIRST_PERSON_GETUPS("NMBS_FIRST_PERSON_GETUPS",0x36A29E60);
const eNmBlendOutSet NMBS_FIRST_PERSON_ACTION_MODE_GETUPS("NMBS_FIRST_PERSON_ACTION_MODE_GETUPS",0x4296B4AD);
const eNmBlendOutSet NMBS_INJURED_FIRST_PERSON_GETUPS("NMBS_INJURED_FIRST_PERSON_GETUPS",0xE543B35);
const eNmBlendOutSet NMBS_FIRST_PERSON_MP_GETUPS("NMBS_FIRST_PERSON_MP_GETUPS",0x591D578D);
const eNmBlendOutSet NMBS_FIRST_PERSON_MP_ACTION_MODE_GETUPS("NMBS_FIRST_PERSON_MP_ACTION_MODE_GETUPS",0x6318D097);
const eNmBlendOutSet NMBS_INJURED_FIRST_PERSON_MP_GETUPS("NMBS_INJURED_FIRST_PERSON_MP_GETUPS",0x27569A6);
#endif

//////////////////////////////////////////////
// class CNmBlendOutItem
//////////////////////////////////////////////

CNmBlendOutItem::CNmBlendOutItem()
: m_type(NMBT_NONE)
{
}

CNmBlendOutItem::~CNmBlendOutItem()
{
}

//////////////////////////////////////////////
// class CNmBlendOutPoseItem
//////////////////////////////////////////////

CNmBlendOutPoseItem::CNmBlendOutPoseItem()
: m_minPlaybackRate(1.0f)
, m_maxPlaybackRate(1.0f)
, m_earlyOutPhase(1.0f)
, m_movementBreakOutPhase(1.0f)
, m_turnBreakOutPhase(1.0f)
, m_ragdollFrameBlendDuration(0.25f)
, m_no180Blend(false)
, m_looping(false)
{
}

CNmBlendOutPoseItem::~CNmBlendOutPoseItem()
{
}

//////////////////////////////////////////////
// class CNmBlendOutMotionStateItem
//////////////////////////////////////////////

CNmBlendOutMotionStateItem::CNmBlendOutMotionStateItem()
: m_motionState(CPedMotionStates::MotionState_None)
{
}

CNmBlendOutMotionStateItem::~CNmBlendOutMotionStateItem()
{
}


//////////////////////////////////////////////
// class CNmBlendOutBlendItem
//////////////////////////////////////////////

CNmBlendOutBlendItem::CNmBlendOutBlendItem()
: m_blendDuration(0.25f)
, m_tagSync(false)
{
}

CNmBlendOutBlendItem::~CNmBlendOutBlendItem()
{
}

//////////////////////////////////////////////
// class CNmBlendOutReactionItem
//////////////////////////////////////////////

CNmBlendOutReactionItem::CNmBlendOutReactionItem()
: m_doReactionChance(1.0f)
{
}

CNmBlendOutReactionItem::~CNmBlendOutReactionItem()
{
}

//////////////////////////////////////////////
// class CNmBlendOutPoseSet
//////////////////////////////////////////////
    
CNmBlendOutSet::CNmBlendOutSet()
: m_ControlFlags(BOSF_AllowRootSlopeFixup),
m_fallbackSet((u32)0)
{
}

CNmBlendOutSet::~CNmBlendOutSet()
{
    // Deleting m_items's contents
    for(int index1 = 0; index1 < m_items.GetCount(); index1++) 
    {
        delete m_items[index1];
    }
}

CNmBlendOutPoseItem* CNmBlendOutSet::FindPoseItem(fwMvClipSetId clipSet, fwMvClipId clip)
{
	for (s32 j=0; j<m_items.GetCount(); j++)
	{
		if ( m_items[j]->IsPoseItem() )
		{
			CNmBlendOutPoseItem* pPoseItem = static_cast<CNmBlendOutPoseItem*>(m_items[j]);
			if (pPoseItem->m_clipSet.GetHash()==clipSet.GetHash() && pPoseItem->m_clip.GetHash()==clip.GetHash())
			{
				return pPoseItem;
			}
		}
	}

	if (HasFallbackSet())
	{
		CNmBlendOutSet* pFallbackSet = CNmBlendOutSetManager::GetBlendOutSet(GetFallbackSet());
		return pFallbackSet->FindPoseItem(clipSet, clip);
	}

	return NULL;
}

    
//////////////////////////////////////////////
// class CNmBlendOutPoseManager
//////////////////////////////////////////////
    
CNmBlendOutSetManager::CNmBlendOutSetManager()
{
}

CNmBlendOutSetManager::~CNmBlendOutSetManager()
{
}



//////////////////////////////////////////////////////////////////////////

CNmBlendOutSetList::CNmBlendOutSetList()
: m_pMoveTask(NULL)
{
	//nothing to see here
}

//////////////////////////////////////////////////////////////////////////

CNmBlendOutSetList::~CNmBlendOutSetList()
{
	Reset();
}

//////////////////////////////////////////////////////////////////////////

void CNmBlendOutSetList::Reset()
{
	m_sets.Reset();
	m_filter.Reset();
	m_Target.Clear();
	if (m_pMoveTask)
	{
		delete m_pMoveTask;
		m_pMoveTask = NULL;
	}	
}

//////////////////////////////////////////////////////////////////////////

void CNmBlendOutSetList::Add( eNmBlendOutSet set, float bias)
{
	CNmBlendOutSet* pSet = CNmBlendOutSetManager::GetBlendOutSet(set);

	animDebugf2("[%d:%d] CNmBlendOutSetList::Add - %s", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount(), set.TryGetCStr());

	if (taskVerifyf(pSet, "Blend out set %s does not exist in metadata!", CNmBlendOutSetManager::GetBlendOutSetName(set)))
	{
		if (m_sets.Find(set)<0)
		{
			m_sets.PushAndGrow(set);

			// run over the pose set and add the filter for 
			// each for the included items
			for (s32 i=0; i<pSet->m_items.GetCount(); i++)
			{
				if (pSet->m_items[i]->ShouldAddToPointCloud())
				{
					m_filter.AddKey(pSet->m_items[i]->GetClipSet(), pSet->m_items[i]->GetClip(), bias);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CNmBlendOutSetList::AddWithFallback( eNmBlendOutSet set, eNmBlendOutSet fallbackSet)
{
	taskAssertf(CNmBlendOutSetManager::IsBlendOutSetStreamedIn(fallbackSet), "Fallback nm blend out pose set '%s' is not streamed in!", fallbackSet.GetCStr());
	bool bSetStreamed = CNmBlendOutSetManager::IsBlendOutSetStreamedIn(set);
#if !__NO_OUTPUT
	if (!bSetStreamed)
	{
		animWarningf("[%d:%d] CNmBlendOutSetList::AddWithFallback - Desired set %s is not streamed in. Using fallback set %s", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount(), set.TryGetCStr(), fallbackSet.TryGetCStr());
	}
#endif //!__NO_OUTPUT
	Add( bSetStreamed ? set : fallbackSet);
}

#if __ASSERT
//////////////////////////////////////////////////////////////////////////

atString CNmBlendOutSetList::DebugDump() const
{
	atVarString outString("%d blend out sets with %d filter keys.\n", m_sets.GetCount(), m_filter.GetKeyCount());

	if (m_sets.GetCount()>0)
	{
		outString+="Sets: ";
		for (s32 i=0; i<m_sets.GetCount(); i++)
		{
			outString+= m_sets[i].GetCStr();
			outString+= "\n";
		}
	}

	return outString;
}

#endif //__ASSERT

//////////////////////////////////////////////////////////////////////////

CNmBlendOutPoseItem* CNmBlendOutSetList::FindPoseItem(fwMvClipSetId clipSet, fwMvClipId clip, eNmBlendOutSet& outSet)
{
	// find the set and item we matched to (only search the list of sets we asked to match against)
	for (s32 i=0; i<m_sets.GetCount(); i++)
	{
		CNmBlendOutSet* pSet = CNmBlendOutSetManager::GetBlendOutSet(m_sets[i]);

		if (pSet)
		{
			CNmBlendOutPoseItem* pItem = pSet->FindPoseItem(clipSet, clip);
			if (pItem && pItem->ShouldAddToPointCloud())
			{
				outSet = m_sets[i];
				return pItem;
			}
		}
		else
		{
			taskAssertf(pSet, "Unable to find matched blend out set for clip '%s', clip set '%s'!", clip.TryGetCStr() ? clip.GetCStr() : "UNKNOWN",  clipSet.TryGetCStr() ? clipSet.GetCStr() : "UNKNOWN");
		}
	}

	return NULL;
}

#if __ASSERT
void CNmBlendOutSetManager::ValidateBlendOutSets()
{
	if(!PARAM_validateBlendOutSets.Get())
	{
		return;
	}

	bool bFoundProblems = false;
	for (int i = 0; i < g_NmBlendOutPoseManager.m_sets.GetCount(); ++i)
	{
		const atHashString * pKey = g_NmBlendOutPoseManager.m_sets.GetKey(i);
		CNmBlendOutSet ** ppPoseSet = g_NmBlendOutPoseManager.m_sets.GetItem(i);
		if (pKey && ppPoseSet)
		{
			const CNmBlendOutSet * pPoseSet = *ppPoseSet;
			const char * pName = pKey->GetCStr();
			for (int j = 0; j < pPoseSet->m_items.GetCount(); ++j)
			{
				atHashString  nItem = pPoseSet->m_items[j]->m_id;
				if (pPoseSet->m_items[j]->HasClipSet())
				{
					fwMvClipSetId clipSet = pPoseSet->m_items[j]->GetClipSet();
					if (pPoseSet->m_items[j]->m_type == NMBT_AIMFROMGROUND)
					{
						if (CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByHash(clipSet.GetHash()) == NULL)
						{
							taskDisplayf("NM BLENDOUTSETS METADATA ERROR: ScriptedGunTaskInfo: %s (%u) does not exist for NmBlendOutSet: %s (%u) For Item: %s (%u).  This should probably be added to scriptedguntasks.meta or removed from the blend out sets file.", clipSet.GetCStr(), clipSet.GetHash(), pName, pKey->GetHash(), nItem.GetCStr(), nItem.GetHash());
							bFoundProblems = true;
						}
						else
						{
							taskDebugf3("NM BLENDOUTSETS METADATA OK: NmBlendOutSet: %s (%u) Item: %s (%u) ScriptedGunTaskInfo: %s (%u)", pName, pKey->GetHash(), nItem.GetCStr(), nItem.GetHash(), clipSet.GetCStr(), clipSet.GetHash());
						}
					}
					else
					{
						fwMvClipId    clip	  = pPoseSet->m_items[j]->GetClip();
						fwClipSet *	  pClipSet = fwClipSetManager::GetClipSet(clipSet);
						if (!pClipSet)
						{
							taskDisplayf("NM BLENDOUTSETS METADATA ERROR: ClipSet: %s (%u) does not exist for NmBlendOutSet: %s (%u) For Item: %s (%u).  This should probably be added to clip_sets.xml or removed from the blend out sets file.", clipSet.GetCStr(), clipSet.GetHash(), pName, pKey->GetHash(), nItem.GetCStr(), nItem.GetHash());
							bFoundProblems = true;
						}
						else
						{
							if (pClipSet->GetClipDictionary(false))
							{
								const crClip * pClip = pClipSet->GetClip(clip, CRO_NONE);
								if (!pClip)
								{
									taskDisplayf("NM BLENDOUTSETS METADATA ERROR: Clip: %s (%u) does not exist in clipset: %s (%u) in NmBlendOutSet: %s (%u).  For Item: %s (%u).  This should probably be added to clip_sets.xml or removed from the blend out sets file.", clip.GetCStr(), clip.GetHash(), clipSet.GetCStr(), clipSet.GetHash(), pName, pKey->GetHash(), nItem.GetCStr(), nItem.GetHash());
									bFoundProblems = true;
								}
								else
								{
									taskDebugf3("NM BLENDOUTSETS METADATA OK: NmBlendOutSet: %s (%u) Item: %s (%u) ClipSet: %s (%u) Clip: %s (%u)", pName, pKey->GetHash(), nItem.GetCStr(), nItem.GetHash(), clipSet.GetCStr(), clipSet.GetHash(), clip.GetCStr(), clip.GetHash());
								}
							}
							else
							{
								taskDebugf3("NM BLENDOUTSETS METADATA skipped individual clip checks on: NmBlendOutSet: %s Item: %s (%u) (%u) ClipSet: %s (%u) ", pName, nItem.GetCStr(), pKey->GetHash(), nItem.GetHash(), clipSet.GetCStr(), clipSet.GetHash());
							}
						}
					}
				}
				else
				{
					taskDebugf3("NM BLENDOUTSETS METADATA OK: NmBlendOutSet: %s (%u) Item: %s (%u) (Has No ClipSet)", pName, pKey->GetHash(), nItem.GetCStr(), nItem.GetHash());
				}
			}
		}
	}
	taskAssertf(!bFoundProblems, "Found Problems with BlendOutSets Metadata.  See Output for Details!  Search for NM BLENDOUTSETS METADATA ERROR after loading into game.");
}
#endif // __ASSERT
//////////////////////////////////////////////////////////////////////////

bool CNmBlendOutSetManager::RequestBlendOutSet( eNmBlendOutSet set )
{
	//grab the set from the manager
	CNmBlendOutSet* poseSet = GetBlendOutSet(set);
	bool allLoaded = true;

	if (poseSet)
	{
		// run over the pose set and request the anim set for each entry
		for (s32 i=0; i<poseSet->m_items.GetCount(); i++)
		{
			fwMvClipSetId setId;

			// Aim from ground items store a scripted gun task metadata hash in their clip set id.
			// grab the scripted gun task metadata and get the clip set from it.
			if (poseSet->m_items[i]->m_type==NMBT_AIMFROMGROUND)
			{
				const CScriptedGunTaskInfo* pScriptedGunTaskInfo = CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByHash(poseSet->m_items[i]->GetClipSet().GetHash());
				if (pScriptedGunTaskInfo)
				{
					setId.SetHash(pScriptedGunTaskInfo->GetClipSet().GetHash());
				}
			}
			else if (poseSet->m_items[i]->HasClipSet())
			{
				// Get the clip set id directly from the pose set
				setId.SetHash(poseSet->m_items[i]->GetClipSet().GetHash());
			}

			if (setId.GetHash()!= 0 && !fwClipSetManager::Request_DEPRECATED(setId))
			{
				allLoaded = false;
			}
		}

		if (poseSet->HasFallbackSet())
		{
			bool fallbackLoaded = RequestBlendOutSet(poseSet->GetFallbackSet());
			allLoaded &= fallbackLoaded;
		}
	}
	else
	{
		taskAssertf(poseSet, "RequestBlendOutSet - Blend out set %s does not exist in metadata!", GetBlendOutSetName(set));
		allLoaded = false;
	}

	return allLoaded;
}

void CNmBlendOutSetManager::AddRefBlendOutSet( eNmBlendOutSet set )
{
	//grab the set from the manager
	CNmBlendOutSet* poseSet = GetBlendOutSet(set);

	if (poseSet)
	{
		// run over the pose set and request the anim set for each entry
		for (s32 i=0; i<poseSet->m_items.GetCount(); i++)
		{
			fwMvClipSetId setId;

			// Aim from ground items store a scripted gun task metadata hash in their clip set id.
			// grab the scripted gun task metadata and get the clip set from it.
			if (poseSet->m_items[i]->m_type==NMBT_AIMFROMGROUND)
			{
				const CScriptedGunTaskInfo* pScriptedGunTaskInfo = CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByHash(poseSet->m_items[i]->GetClipSet().GetHash());
				if (pScriptedGunTaskInfo)
				{
					setId.SetHash(pScriptedGunTaskInfo->GetClipSet().GetHash());
				}
			}
			else if (poseSet->m_items[i]->HasClipSet())
			{
				// Get the clip set id directly from the pose set
				setId.SetHash(poseSet->m_items[i]->GetClipSet().GetHash());
			}

			if (setId.GetHash()!= 0 )
			{
				fwClipSetManager::AddRef_DEPRECATED(setId);
			}
		}

		if (poseSet->HasFallbackSet())
		{
			AddRefBlendOutSet(poseSet->GetFallbackSet());
		}
	}
	else
	{
		taskAssertf(poseSet, "AddRefBlendOutSet - Blend out set %s does not exist in metadata!", GetBlendOutSetName(set));
	}
}

void CNmBlendOutSetManager::ReleaseBlendOutSet( eNmBlendOutSet set )
{
	//grab the set from the manager
	CNmBlendOutSet* poseSet = GetBlendOutSet(set);

	if (poseSet)
	{
		// run over the pose set and request the anim set for each entry
		for (s32 i=0; i<poseSet->m_items.GetCount(); i++)
		{
			fwMvClipSetId setId;

			// Aim from ground items store a scripted gun task metadata hash in their clip set id.
			// grab the scripted gun task metadata and get the clip set from it.
			if (poseSet->m_items[i]->m_type==NMBT_AIMFROMGROUND)
			{
				const CScriptedGunTaskInfo* pScriptedGunTaskInfo = CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByHash(poseSet->m_items[i]->GetClipSet().GetHash());
				if (pScriptedGunTaskInfo)
				{
					setId.SetHash(pScriptedGunTaskInfo->GetClipSet().GetHash());
				}
			}
			else if (poseSet->m_items[i]->HasClipSet())
			{
				// Get the clip set id directly from the pose set
				setId.SetHash(poseSet->m_items[i]->GetClipSet().GetHash());
			}

			if (setId.GetHash()!= 0)
			{
				fwClipSetManager::Release_DEPRECATED(setId);
			}
		}

		if (poseSet->HasFallbackSet())
		{
			ReleaseBlendOutSet(poseSet->GetFallbackSet());
		}
	}
	else
	{
		taskAssertf(poseSet, "ReleaseBlendOutSet - Blend out set %s does not exist in metadata!", GetBlendOutSetName(set));
	}
}

bool CNmBlendOutSetManager::IsBlendOutSetStreamedIn( eNmBlendOutSet set )
{
	//grab the set from the manager
	CNmBlendOutSet* poseSet = GetBlendOutSet(set);

	if (poseSet)
	{
		// run over the pose set and check if the anim set for each entry is streamed in
		for (s32 i=0; i<poseSet->m_items.GetCount(); i++)
		{
			fwMvClipSetId setId;

			// Aim from ground items store a scripted gun task metadata hash in their clip set id.
			// grab the scripted gun task metadata and get the clip set from it.
			if (poseSet->m_items[i]->m_type==NMBT_AIMFROMGROUND)
			{
				const CScriptedGunTaskInfo* pScriptedGunTaskInfo = CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByHash(poseSet->m_items[i]->GetClipSet().GetHash());
				if (pScriptedGunTaskInfo)
				{
					setId.SetHash(pScriptedGunTaskInfo->GetClipSet().GetHash());
				}
			}
			else if (poseSet->m_items[i]->HasClipSet())
			{
				// Get the clip set id directly from the pose set
				setId.SetHash(poseSet->m_items[i]->GetClipSet().GetHash());
			}

			if (setId.GetHash()!= 0)
			{
				if (!fwClipSetManager::IsStreamedIn_DEPRECATED(setId))
				{
					return false;
				}
			}
		}

		if (poseSet->HasFallbackSet())
		{
			bool fallbackLoaded = IsBlendOutSetStreamedIn(poseSet->GetFallbackSet());
			if (!fallbackLoaded)
			{
				return false;
			}
		}
	}
	else
	{
		taskAssertf(poseSet, "IsBlendOutSetStreamedIn - Blend out set %s does not exist in metadata!", GetBlendOutSetName(set));
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

CNmBlendOutItem* CNmBlendOutSetManager::FindItem(eNmBlendOutSet set, u32 key)
{
	//grab the set from the manager
	CNmBlendOutSet* poseSet = GetBlendOutSet(set);

	if (poseSet)
	{
		// run over the pose set and add the filter for 
		// each for the included items
		for (s32 i=0; i<poseSet->m_items.GetCount(); i++)
		{
			if (poseSet->m_items[i]->m_id==key)
			{
				return poseSet->m_items[i];
			}
		}

		// no match yet, try the fallback set
		if (poseSet->HasFallbackSet())
		{
			return FindItem(poseSet->GetFallbackSet(), key);
		}
	}
	else
	{
		taskAssertf(poseSet, "FindPoseItem - Blend out set %s does not exist in metadata!", GetBlendOutSetName(set));
		return NULL;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

const char *g_blendOutDataSetDirectory = "common:/data/anim/pose_databases";

//////////////////////////////////////////////////////////////////////////

void CNmBlendOutSetManager::Load()
{
	//ASSET.PushFolder(g_blendOutDataSetDirectory);
	PARSER.LoadObject("common:/data/anim/pose_databases/nm_blend_out_sets", "meta", g_NmBlendOutPoseManager);
	//ASSET.PopFolder();
}

bool CNmBlendOutSetManager::Append(const char *fname)
{
	CNmBlendOutSetManager temp_inst;
	bool bResult = PARSER.LoadObject(fname, NULL, temp_inst);
	Assertf(bResult, "Load %s failed!\n", fname);
	AppendBinMap(g_NmBlendOutPoseManager.m_sets, temp_inst.m_sets, AppendBinMapFreeObjectPtr<CNmBlendOutSet>);
	return bResult;
}

//////////////////////////////////////////////////////////////////////////

void CNmBlendOutSetManager::Shutdown()
{
	g_NmBlendOutPoseManager.m_sets.Reset();
}


//////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CNmBlendOutSetManager::Save()
{
	ASSET.PushFolder(g_blendOutDataSetDirectory);
	PARSER.SaveObject("nm_blend_out_sets", "meta", &g_NmBlendOutPoseManager);
	ASSET.PopFolder();
}
#endif //!__FINAL
