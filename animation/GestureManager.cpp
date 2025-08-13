#include "GestureManager.h"

// Rage headers

#include "fwanimation/anim_channel.h"

// Gta headers

#include "streaming/PrioritizedClipSetStreamer.h"

ANIM_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////

RAGE_DEFINE_CHANNEL(gesture)

//////////////////////////////////////////////////////////////////////////

CGestureManager *g_pGestureManager = NULL;

//////////////////////////////////////////////////////////////////////////

void CGestureManager::RequestVehicleGestureClipSet(fwMvClipSetId clipSetId)
{
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
		CPrioritizedClipSetRequestManager::GetInstance().Request(CPrioritizedClipSetRequestManager::RC_GestureVehicle, clipSetId, SP_Low, 1.0f, false);
	}
}

//////////////////////////////////////////////////////////////////////////

bool CGestureManager::CanUseVehicleGestureClipSet(fwMvClipSetId clipSetId) const
{
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
		return CPrioritizedClipSetRequestManager::GetInstance().IsLoaded(clipSetId) && !CPrioritizedClipSetRequestManager::GetInstance().ShouldClipSetBeStreamedOut(CPrioritizedClipSetRequestManager::RC_GestureVehicle, clipSetId);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

void CGestureManager::RequestScenarioGestureClipSet(fwMvClipSetId clipSetId)
{
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
		CPrioritizedClipSetRequestManager::GetInstance().Request(CPrioritizedClipSetRequestManager::RC_GestureScenario, clipSetId, SP_Low, 1.0f);
	}
}

//////////////////////////////////////////////////////////////////////////

bool CGestureManager::CanUseScenarioGestureClipSet(fwMvClipSetId clipSetId) const
{
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
		return CPrioritizedClipSetRequestManager::GetInstance().IsLoaded(clipSetId) && !CPrioritizedClipSetRequestManager::GetInstance().ShouldClipSetBeStreamedOut(CPrioritizedClipSetRequestManager::RC_GestureScenario, clipSetId);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL

bool ClipDictionaryIterator(crClip *pClip, crClipDictionary::ClipKey /*key*/, void * /*pData*/)
{
	if(pClip)
	{
		/* Clean up clip name */
		atString clipName(pClip->GetName());
		clipName.Replace("pack:/", "");
		clipName.Replace(".clip", "");

		/* Put clip name in hash string table */
		atHashString temp(clipName.c_str());
		temp.Clear();
	}

	return true;
}

#endif // !__FINAL

void CGestureManager::Init(unsigned initMode)
{
	if(initMode == INIT_SESSION)
	{
		if(gestureVerifyf(!g_pGestureManager, "GestureManager already exists!"))
		{
			g_pGestureManager = rage_new CGestureManager();
		}

#if !__FINAL

		/* Add all the gesture clip names to the hash string table */

		const char *szGesturePrefix = "gestures@";
		const size_t uGesturePrefixLength = strlen(szGesturePrefix);

		/* Iterate through clip dictionaries */
		for(int i = 0; i < g_ClipDictionaryStore.GetNumUsedSlots(); i ++)
		{
			const char *szClipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(i));
			if(szClipDictionaryName && szClipDictionaryName[0] != '\0')
			{
				/* Does clip dictionary name start with the gesture prefix? */
				if(strncmp(szClipDictionaryName, szGesturePrefix, uGesturePrefixLength) == 0)
				{
					/* Put clip dictionary name in hash string table */
					atHashString temp(szClipDictionaryName);
					temp.Clear();

					crClipDictionary *pClipDictionary = g_ClipDictionaryStore.Get(strLocalIndex(i));
					if(pClipDictionary)
					{
						/* Iterate through clips */
						pClipDictionary->ForAll(ClipDictionaryIterator, NULL);
					}
				}
			}
		}

#endif // !__FINAL
	}
}

//////////////////////////////////////////////////////////////////////////

void CGestureManager::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_SESSION)
	{
		if(gestureVerifyf(g_pGestureManager, "GestureManager doesn't exist!"))
		{
			delete g_pGestureManager; g_pGestureManager = NULL;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CGestureManager::~CGestureManager()
{
}

//////////////////////////////////////////////////////////////////////////

CGestureManager::CGestureManager()
{
}

//////////////////////////////////////////////////////////////////////////
