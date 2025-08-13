#include "AnimManager.h"

// Rage headers
#include "fwanimation/animmanager.h"
#include "fwscene/stores/framefilterdictionarystore.h"
#include "fwsys/gameskeleton.h"
#include "system/bootmgr.h"

// Gta headers
#include "animation/AnimBones.h"
#include "animation/AnimDefines.h"
#include "animation/CreatureMetadata.h"

#include "scene/DynamicEntity.h"
#include "animation/Move.h"
#include "peds/Ped.h"

#if __BANK
#include "animation/debug/AnimViewer.h"
#include "animation/debug/AnimDebug.h"
#endif //__BANK

CompileTimeAssert(sizeof(fwMvClipSetId) == 4);
CompileTimeAssert(sizeof(fwMvClipId) == 4);

ANIM_OPTIMISATIONS()

//RAGE_DEFINE_CHANNEL(anim)

//////////////////////////////////////////////////////////////////////////
/*

*/

// --- CGtaAnimManager --------------------------------------------------------------------------------------

void CGtaAnimManager::InitClass()
{
	Assert(!g_AnimManager);
	g_AnimManager = rage_new CGtaAnimManager();

#if !__FINAL
	if (!sysBootManager::IsPackagedBuild())
	{
		ClipUserData clipUserData;

		ASSET.PushFolder("common:/non_final/anim");

		PARSER.LoadObject("ClipUserData", "meta", clipUserData);

		ASSET.PopFolder();
	}
#endif // !__FINAL
}

//////////////////////////////////////////////////////////////////////////

void CGtaAnimManager::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
	    USE_MEMBUCKET(MEMBUCKET_ANIMATION);

		CGtaAnimManager::InitClass();
		fwAnimManager::InitCore();
	    CDynamicEntity::InitClass();
	    CPedBoneTagConvertor::Init();
	}
    else if(initMode == INIT_BEFORE_MAP_LOADED)
    {
		fwAnimManager::InitBeforeMap();
    }
	else if(initMode == INIT_SESSION)
	{
#if __BANK
		CDebugClipDictionary::InitLevel();
#endif
	}
}

//////////////////////////////////////////////////////////////////////////

void CGtaAnimManager::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
	    CPedBoneTagConvertor::Shutdown();
	    CDynamicEntity::ShutdownClass();
		fwAnimManager::ShutdownCore();
		CGtaAnimManager::ShutdownClass();
    }
    else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
#if __BANK
		CDebugClipDictionary::ShutdownLevel();
#endif
		fwAnimManager::ShutdownSession();
    }
}

//////////////////////////////////////////////////////////////////////////

crFrameFilterMultiWeight* CGtaAnimManager::FindFrameFilter(const atHashWithStringNotFinal &hashString, const CPed* pPed)
{
	if(pPed && pPed->GetSkeleton())
	{
		crFrameFilter *frameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(hashString));
		if(frameFilter)
		{
			crFrameFilterMultiWeight *frameFilterMultiWeight = frameFilter->DynamicCast<crFrameFilterMultiWeight>();
			if(frameFilterMultiWeight)
			{
				return frameFilterMultiWeight;
			}
			else
			{
#if !__FINAL
				animDisplayf("FrameFilter %s is not a FrameFilterMultiWeight", hashString.GetCStr());
#endif // !__FINAL
			}
		}
		else
		{
#if !__FINAL
			if(hashString.GetHash() != BONEMASK_ALL)
			{
				animDisplayf("Missing FrameFilter %s", hashString.GetCStr());
			}
#endif // !__FINAL
		}
	}
	else
	{
#if !__FINAL
		animDisplayf("Unable to load FrameFilter %s, MotionTree has no skeleton", hashString.GetCStr());
#endif // !__FINAL
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

fwAnimDirector *CGtaAnimManager::GetAnimDirectorBeingViewed() const
{
#if __BANK
	if (CAnimViewer::m_pDynamicEntity)
	{
		return CAnimViewer::m_pDynamicEntity->GetAnimDirector();
	}
#endif // __BANK

	return NULL;
}

//////////////////////////////////////////////////////////////////////////


