#ifndef __INC_AMBIENT_ANIM_MANAGER_H__
#define __INC_AMBIENT_ANIM_MANAGER_H__

// Rage headers
#include "string/stringhash.h"

// Framework headers
#include "ai/aichannel.h"
#include "streaming/streamingmodule.h"

// GTA defines
#include "modelInfo/modelInfo.h"
#include "Vehicles/VehicleDefines.h"
#include "AI/Ambient/ConditionalAnims.h"

// Forward Declarations
class CCachedState;
class CDynamicEntity;
class CPed;
class CConditionalAnims;
class CConditionalAnimsGroup;

namespace rage {
	class fwClipSet;
}



enum
{
	SF_AddPedRandomTimeOfDayDelay = (1<<0),
	SF_LowThreatClips	= (1<<8),
	SF_HighThreatClips	= (1<<9),
	SF_IgnoreHasPropCheck = (1<<10),
	SF_IgnoreTime		= (1<<11),
	SF_IgnoreWeather	= (1<<12),
};


//-------------------------------------------------------------------------
// Class responsible for loading ambient clips and deciding 
// which clips a ped can use at any particular time.
//-------------------------------------------------------------------------
class CAmbientAnimationManager
{
	#define MAX_SIZE_PROP_SET 16
	typedef struct
	{
		float						fProbabilities;
		s32							iNumPedsNearbyWithProp;
		s32							iPropSetsUses[MAX_SIZE_PROP_SET];
		u32							iIndex; // Which ConditionalAnim (in the group being searched) this came from 
		const CConditionalAnims*	pAmbientSet;
	} 
	sPropSearchItem;
	
	#define MAX_SEARCH_ITEMS 64
	typedef struct
	{
		sPropSearchItem searchItems[MAX_SEARCH_ITEMS];
		u32 numSearchItems;
		float totalProbability;
		bool foundAnySimilarProps;
	} 
	sPropSearchInfo;

	typedef struct
	{
		float						fProbabilities;
		u32							iIndex; // Which ConditionalAnim (in the group being searched) this came from 
		const CConditionalAnims*	pAmbientSet;
	} 
	sAmbientManagerSearchResult;

	typedef struct
	{
		sAmbientManagerSearchResult searchItems[MAX_SEARCH_ITEMS];
		u32 numSearchItems;
		float totalProbability;

		void RemoveFast(int index) 
		{ 
			ArrayAssert(index >=0 && index < numSearchItems);
			totalProbability -= searchItems[index].fProbabilities; 
			searchItems[index] = searchItems[numSearchItems-1]; 
			--numSearchItems;
		}
	} 
	sSearchInfo;

public:
#if !__FINAL
	friend class				CPedDebugVisualiser;
//	static void					TestAllAmbientDictionaries();
#endif

	static bool					ms_bInitialised;

    static void					Init(unsigned initMode);
	static void					Shutdown(unsigned shutdownMode);
	static bool					RandomlyChoosePropForSpawningPed(  CScenarioCondition::sScenarioConditionData& conditionData, const CConditionalAnimsGroup * pClips, u32& uiPropHashOut, int& animIndexWithinGroupOut, int conditionalAnimsChosen);
	static s32					GetAmbientClipPriority			( CPed& ped );
	static bool					ChooseConditionalAnimations( CScenarioCondition::sScenarioConditionData& conditionData, s32& iChosenPriority, const CConditionalAnimsGroup * pConditionalAnimsGroup, s32 & ConditionalAnimsChosen, CConditionalAnims::eAnimType clipType );
	static bool					ChooseConditionalAnimationsConsiderNearbyPeds( CScenarioCondition::sScenarioConditionData& conditionData, s32& iChosenPriority, const CConditionalAnimsGroup * pConditionalAnimsGroup, s32 & ConditionalAnimsChosen, CConditionalAnims::eAnimType clipType );
	static const CAmbientPropModelSet* GetPropSetFromHash(u32 hash);

private:

#if __DEV
	static fwClipSet*			AddClipsToClipSet				( const char* szClipSetName, const char* szDictName, char* pToken, char* seps );
	static fwClipSet*			AddFullDictionaryClipSet		( const char* szDictName );
#endif
	static float				AdjustPriorityForPed			( const CDynamicEntity& dynamicEntity, float iPriority );

	static void					AddClipsToPropSearch( CScenarioCondition::sScenarioConditionData& conditionData, sPropSearchInfo & searchInfo, const CConditionalAnimsGroup & ConditionalAnims, int conditionalAnimsChosen );

	static void FillSearchInfoForConditionalAnimsGroup(CScenarioCondition::sScenarioConditionData& conditionData, const CConditionalAnimsGroup* pConditionalAnimsGroup, CConditionalAnims::eAnimType clipType, sSearchInfo& searchInfoOut);
};

#endif // __INC_AMBIENT_ANIM_MANAGER_H__
