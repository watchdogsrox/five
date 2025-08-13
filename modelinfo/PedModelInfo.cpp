//
// filename:	PedModelInfo.cpp
// description:	Class describing a Pedestrian model
//

// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers
#include "crskeleton/skeleton.h"
#include "grblendshapes/blendshapes_config.h"
#include "fwsys/fileExts.h"
#include "parser/manager.h"
#include "parser/restparserservices.h"
#include "phbound/BoundCapsule.h"
#include "phbound/BoundComposite.h"
#include "phBound/boundcurvedgeom.h"
#include "phBound/support.h"
#include "phcore/MaterialMgr.h"
#include "phcore/phmath.h"
#include "fragmentnm/manager.h"
#include "diag/art_channel.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwdebug/picker.h"
#include "fwmaths/Angle.h"
#include "fwsys/gameskeleton.h"
#include "fwsys/metadatastore.h"
#include "fwscene/stores/clothstore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/TxdStore.h"
#include "fwanimation/expressionsets.h"
#include "fwanimation/facialclipsetgroups.h"
#include "streaming/streamingvisualize.h"

// Game headers
#include "animation/AnimBones.h"
#include "animation/AnimDefines.h"
#include "fragment/typegroup.h"
#include "modelinfo/ModelInfo.h"
#include "modelinfo/ModelInfo_Factories.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/PedModelInfoData_parser.h"
#include "peds/Ped.h"
#include "peds/Ped_Channel.h"
#include "peds/PedCapsule.h"
#include "peds/PedComponentSetInfo.h"
#include "peds/TaskData.h"
#include "peds/PedMoveBlend/PedMoveBlendManager.h"
#include "peds/PedPopulation.h"
#include "peds/rendering/pedVariationPack.h"
#include "physics/gtaArchetype.h"
#include "physics/gtaMaterialManager.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "renderer/HierarchyIds.h"
#include "renderer/MeshBlendManager.h"
#include "scene/DataFileMgr.h"
#include "scene/dlc_channel.h"
#include "scene/texlod.h"
#include "shaders/CustomShaderEffectBase.h"
#include "stats/StatsMgr.h"
#include "stats\StatsInterface.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "system/FileMgr.h"
#include "peds/popcycle.h"
#include "game/ModelIndices.h"
#include "scene/world/GameWorld.h"
#include "Stats/stats_channel.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "stats/StatsMgr.h"
#include "scene/ExtraMetadataMgr.h"
#include "streaming/populationstreaming.h"

#if __BANK
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#endif

AI_OPTIMISATIONS()
PED_OPTIMISATIONS()

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

dev_float CPedModelInfo::ms_fPedRadius = 0.35f;

CPedModelInfo::PersonalityDataList CPedModelInfo::ms_personalities;
fwPtrListSingleLink CPedModelInfo::ms_HDStreamRequests;
atArray<strIndex> CPedModelInfo::ms_residentObjects;
s32 CPedModelInfo::ms_commonTxd = -1;

#if __BANK
bkGroup* CPedModelInfo::ms_bankGroup = NULL;
#endif

PARAM(nopedmetadata, "Disables ped meta data loading.");
PARAM(updatepedmetadata, "Temp command to update the inclusion/exclusion format in ped meta data");


EXT_PF_VALUE_INT(NumWaterSamplesInMemory);

// --- Static Globals -----------------------------------------------------------

const u32 g_headBlendPed = ATSTRINGHASH("mp_headtargets", 0x045f92d79);

const u32 s_HashOfNullString = ATSTRINGHASH("null", 0x03adb3357);

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

#if __DEV
//OPTIMISATIONS_OFF()
#endif

#define HORSE_RAGDOLL_HACK 1

class CPedModelMetaDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile &file)
	{ 
		dlcDebugf3("CPedModelMetaDataFileMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);
		switch(file.m_fileType)
		{
			case CDataFileMgr::PED_METADATA_FILE:
				CPedModelInfo::LoadPedMetaFile(file.m_filename, false, fwFactory::EXTRA);
				break;

			case CDataFileMgr::PED_BOUNDS_FILE:
				CPedCapsuleInfoManager::Instance().Append(file.m_filename);
				break;

			case CDataFileMgr::PED_PERSONALITY_FILE:
				CPedModelInfo::AppendPedPersonalityFile(file.m_filename);
				break;

			case CDataFileMgr::PED_PERCEPTION_FILE:
				CPedPerceptionInfoManager::Shutdown();
				CPedPerceptionInfoManager::Init(file.m_filename);
				break;

			case CDataFileMgr::PED_COMPONENT_SETS_FILE:
				CPedComponentSetManager::AppendExtra(file.m_filename);
				break;

			case CDataFileMgr::PED_TASK_DATA_FILE:
				CTaskDataInfoManager::AppendExtra(file.m_filename);
				break;

			default:
				Errorf("CPedModelMetaDataFileMounter::LoadDataFile Invalid file type! %s [%i]", file.m_filename, file.m_fileType);
				break;
		}
		                      
		return true;
	} 

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) 
	{
		dlcDebugf3("CPedModelMetaDataFileMounter::UnloadDataFile %s type = %d", file.m_filename, file.m_fileType);
		switch(file.m_fileType)
		{
			case CDataFileMgr::PED_COMPONENT_SETS_FILE:
				CPedComponentSetManager::RemoveExtra(file.m_filename);
				break;

			case CDataFileMgr::PED_PERCEPTION_FILE:
				CPedPerceptionInfoManager::Shutdown();
				CPedPerceptionInfoManager::Init("common:/data/pedperception.meta");
				break;

			default:
				break;
		}

	}

} g_PedModelMetaDataFileMounter;


void CPedModelInfo::InitClass(unsigned initMode)
{
	if(initMode == INIT_SESSION)
	{
		CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_METADATA_FILE, &g_PedModelMetaDataFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_BOUNDS_FILE, &g_PedModelMetaDataFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_PERSONALITY_FILE, &g_PedModelMetaDataFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_COMPONENT_SETS_FILE, &g_PedModelMetaDataFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_TASK_DATA_FILE, &g_PedModelMetaDataFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_PERCEPTION_FILE, &g_PedModelMetaDataFileMounter, eDFMI_UnloadLast);

		if (CFileMgr::IsGameInstalled())
		{
			// load head blend data from dedicated dummy ped, the rest is handled when model is loaded
			fwModelId headBlendPed = CModelInfo::GetModelIdFromName(g_headBlendPed);
			if (headBlendPed.IsValid() && !CModelInfo::HaveAssetsLoaded(headBlendPed))
			{
				CModelInfo::RequestAssets(headBlendPed, STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE);
				CStreaming::LoadAllRequestedObjects(true);
			}
		}
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{
		SetupCommonData();
	}
}

void CPedModelInfo::ShutdownClass(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		DeleteCommonData();
        ms_commonTxd = -1;
	}
}

void CPedModelInfo::SetupCommonData()
{
	// load permanently resident texture dictionaries
	for (int i = 0; i < ms_residentObjects.GetCount(); ++i)
	{
		strStreamingEngine::GetInfo().RequestObject(ms_residentObjects[i], STRFLAG_DONTDELETE);
	}

	CStreaming::LoadAllRequestedObjects();

	for (int i = 0; i < ms_residentObjects.GetCount(); ++i)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModuleFromIndex(ms_residentObjects[i]);
		strLocalIndex index = pModule->GetObjectIndex(ms_residentObjects[i]);
		pModule->AddRef(index, REF_OTHER);
		pModule->ClearRequiredFlag(index.Get(), STRFLAG_DONTDELETE);
	}
}		

void CPedModelInfo::DeleteCommonData()
{
	for (int i = 0; i < ms_residentObjects.GetCount(); ++i)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModuleFromIndex(ms_residentObjects[i]);
		pModule->RemoveRef(pModule->GetObjectIndex(ms_residentObjects[i]), REF_OTHER);
	}
	ms_residentObjects.ResetCount();
}

//
// name:		CPedModelInfo::Init
// description:	Initialise class
void CPedModelInfo::Init()
{
	CBaseModelInfo::Init();

	m_LastTimeUsed = 0;
	m_movementClipSet = CLIP_SET_STD_PED;
	m_strafeClipSet = CLIP_SET_ID_INVALID;
	m_movementToStrafeClipSet = CLIP_SET_ID_INVALID;
	m_injuredStrafeClipSet = CLIP_SET_ID_INVALID;
	m_fullBodyDamageClipSet = CLIP_SET_ID_INVALID;
	m_additiveDamageClipSet = CLIP_SET_ID_INVALID;
	m_defaultGestureClipSet = CLIP_SET_ID_INVALID;
	m_SidestepClipSet = CLIP_SET_ID_INVALID;
	m_expressionSet = EXPRESSION_SET_ID_INVALID;
	m_facialClipSetGroupId = FACIAL_CLIP_SET_GROUP_ID_INVALID;
	m_defaultPedType = PEDTYPE_CIVMALE;
	m_drivesWhichCars = 0xffff;
	m_radio1 = -1;
	m_radio2 = -1;
	m_FUpOffset= 0.0f;
	m_RUpOffset= 0.0f;
	m_FFrontOffset= 0.0f;
	m_RFrontOffset= 0.147f;
	m_Race = 0;
	m_AudioPedType = 0;
	m_FirstVoice = 0;
	m_LastVoice = 0;
	m_NextVoice = 0;
	m_PedVoiceGroupHash = 0;
	m_AnimalAudioObjectHash = 0;
	m_MinActivationImpulse = 0.0f;
	m_Stubble = 0.0f;
	m_Sexiness = 0;
	m_Age = 30;
	m_LightFxBoneIdx = 0xffff;
	m_LightMpBoneIdx = 0xffff;

//	m_pRagdollArch = 0;
//	m_physicsDictionaryIndexRagdoll = -1;

	m_pPedVarInfoCollection = NULL;
	m_pSeatInfo = NULL;

	m_pVfxInfo = NULL;

	m_pInvalidRagdollFilter = NULL;

	m_propsFileIndex = -1;
	m_propsFilenameHashcode = 0;
	m_pedCompFileIndex = -1;

	m_CountedAsScenarioPed = false;
	m_NumTimesOnFoot = 0;
	m_NumTimesInCar = 0;
	m_numPedModelRefs = 0;

	m_iAudioGroupCount = 0;

	m_isStreamedGraphics = 0;
	m_AmbulanceShouldRespondTo = 0;
	m_CanRideBikeWithNoHelmet = 0;
	m_CanSpawnInCar = true;
	m_bOnlyBulkyItemVariations = false;

	m_StreamFolder.Clear();

	m_ScenarioPedStreamingSlot = SCENARIO_POP_STREAMING_NORMAL;
	m_DefaultSpawningPreference = DSP_NORMAL;
	m_DefaultRemoveRangeMultiplier = 1.0f;
	m_AllowCloseSpawning = false;
	m_AllowStreamOutAsOldest = true;

	m_PersonalityIdx = -1;

	m_isRequestedAsDriver = 0;
	m_bIsHeadBlendPed = false;

	// HD files stuff
	m_HDDist = -1.f;
	m_HDtxdIdx = -1;

	m_TargetingThreatModifier = 1.0f;
	m_KilledPerceptionRangeModifer = -1.0f;

	m_numAvailableLODs = 0;

	m_NumTimesInReusePool = 0;
}

CPedModelInfo::InitData::InitData()
{
	Init();
}

void CPedModelInfo::InitData::Init()
{
	m_IsStreamedGfx = false;
	m_AmbulanceShouldRespondTo = true;
	m_CanRideBikeWithNoHelmet = false;
	m_CanSpawnInCar = true;
	m_bOnlyBulkyItemVariations = false;
	m_Radio1 = RADIO_GENRE_OFF;
	m_Radio2 = RADIO_GENRE_OFF;
	m_FUpOffset= 0.0f;
	m_RUpOffset= 0.0f;
	m_FFrontOffset= 0.0f;
	m_RFrontOffset= 0.147f;
	m_AbilityType = SAT_NONE;
	m_MinActivationImpulse = 0.0f;
	m_Stubble = 0.0f;
	m_HDDist = -1.f;
	m_SuperlodType = SLOD_HUMAN;
	m_Sexiness = 0;
	m_Age = 0;
	m_ExternallyDrivenDOFs = 0;
	m_TargetingThreatModifier = 1.0f;
	m_KilledPerceptionRangeModifer = -1.0f;
}

CPedModelInfo::PersonalityBravery::PersonalityBravery() : 
m_Flags(0),
m_ThreatResponseUnarmed(),
m_ThreatResponseMelee(),
m_ThreatResponseArmed()
{
}

CPedModelInfo::PersonalityAgility::PersonalityAgility() : 
m_Flags(0),
m_MovementCostModifier(1.f)
{
}

CPedModelInfo::PersonalityCriminality::PersonalityCriminality() : 
m_Flags(0)
{
}

CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets::ClipSets()
: m_MovementClipSetId(CLIP_SET_ID_INVALID)
, m_WeaponClipSetId(CLIP_SET_ID_INVALID)
, m_WeaponClipFilterId(FILTER_ID_INVALID)
, m_UpperBodyShadowExpressionEnabled(false)
, m_UpperBodyFeatheredLeanEnabled(false)
, m_UseWeaponAnimsForGrip(false)
, m_UseLeftHandIk(false)
, m_IdleTransitionBlendOutTime(0.25f)
, m_UnholsterClipDataPtr(NULL)
{
}

void CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets::Clear()
{
	m_MovementClipSetId = CLIP_SET_ID_INVALID;
	m_WeaponClipSetId = CLIP_SET_ID_INVALID;
	m_WeaponClipFilterId = FILTER_ID_INVALID;
	m_UpperBodyShadowExpressionEnabled = false;
	m_UpperBodyFeatheredLeanEnabled = false;
	m_UseWeaponAnimsForGrip = false;
	m_UseLeftHandIk = false;
	m_IdleTransitionBlendOutTime = 0.25f;
	m_IdleTransitions.Reset();
	m_UnholsterClipDataPtr = NULL;
}

bool CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets::IsValid() const
{
	fwClipSet* pMovementClipSet = fwClipSetManager::GetClipSet(m_MovementClipSetId);
	return pMovementClipSet ? true : false;
}

fwMvClipSetId CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets::GetIdleTransitionClipSet() const
{ 
	if(m_IdleTransitions.GetCount() > 0)
	{
		return m_IdleTransitions[fwRandom::GetRandomNumberInRange(0, m_IdleTransitions.GetCount())]; 
	}

	return CLIP_SET_ID_INVALID;
}

fwMvClipId CPedModelInfo::PersonalityMovementModes::MovementMode::ClipSets::GetUnholsterClip(atHashWithStringNotFinal drawingWeapon) const
{
	if(m_UnholsterClipDataPtr)
	{
		for(s32 i = 0; i < m_UnholsterClipDataPtr->m_UnholsterClips.GetCount(); i++)
		{
			const CPedModelInfo::PersonalityMovementModes::sUnholsterClipData::sUnholsterClip& uc = m_UnholsterClipDataPtr->m_UnholsterClips[i];
			for(s32 j = 0; j < uc.m_Weapons.GetCount(); j++)
			{
				if(uc.m_Weapons[j].GetHash() == drawingWeapon.GetHash())
				{
					return uc.m_Clip;
				}
			}
		}
	}

	Assertf(0, "Not found unholster clip for movement mode with clipset [%s], for weapon [%s], UnholsterClipData [%s]", m_MovementClipSetId.TryGetCStr(), drawingWeapon.TryGetCStr(), m_UnholsterClipDataPtr ? m_UnholsterClipDataPtr->m_Name.GetCStr() : "NULL" );
	return CLIP_ID_INVALID;
}

bool CPedModelInfo::PersonalityMovementModes::MovementMode::IsValid(const CPed* UNUSED_PARAM(pPed), u32 uWeaponHash) const
{
	if(m_ClipSets.GetCount() > 0)
	{
		for(s32 i = 0; i < m_Weapons.GetCount(); i++)
		{
			if(m_Weapons[i].GetHash() == uWeaponHash)
			{
				return true;
			}
		}
	}

	return false;
}

void CPedModelInfo::PersonalityMovementModes::MovementMode::PostLoad()
{
	for(s32 i = 0; i < m_ClipSets.GetCount(); i++)
	{
		m_ClipSets[i].m_UnholsterClipDataPtr = CPedModelInfo::FindPersonalityMovementModeUnholsterData(m_ClipSets[i].m_UnholsterClipData);
	}
}

CPedModelInfo::PersonalityMovementModes::PersonalityMovementModes()
{
}

const CPedModelInfo::PersonalityMovementModes::MovementMode* CPedModelInfo::PersonalityMovementModes::FindMovementMode(const CPed* pPed, MovementModes mm, const u32 uWeaponHash, s32& iIndex) const
{
	Assert(pPed);

	s32 iCount = m_MovementModes[mm].GetCount();
	for(s32 i = 0; i < iCount; i++)
	{
		const CPedModelInfo::PersonalityMovementModes::MovementMode& am = m_MovementModes[mm][i];
		if(am.IsValid(pPed, uWeaponHash))
		{
			iIndex = i;
			return &am;
		}
	}

	iIndex = -1;
	return NULL;
}

void CPedModelInfo::PersonalityMovementModes::PostLoad()
{
	for(s32 i = 0; i < MM_Max; i++)
	{
		for(s32 j = 0; j < m_MovementModes[i].GetCount(); j++)
		{
			m_MovementModes[i][j].PostLoad();
		}
	}
}

CPedModelInfo::PersonalityData::PersonalityData()
{
	Init();
}

const s16 CPedModelInfo::FindPersonalityFromHash(u32 hash)
{
	for (s32 i = 0; i < ms_personalities.m_PedPersonalities.GetCount(); ++i)
	{
		if (ms_personalities.m_PedPersonalities[i].GetPersonalityNameHash() == hash)
		{
			return (s16)i;
		}
	}
	return -1;
}

void CPedModelInfo::PersonalityData::Init()
{
	m_DrivingAbilityMin = 5;
	m_DrivingAbilityMax = 5;
	m_DrivingAggressivenessMin = 5;
	m_DrivingAggressivenessMax = 5;
	m_AttackStrengthMin = 0.1f;
	m_AttackStrengthMax = 0.1f;
	m_StaminaEfficiency = 1.0f;
	m_ArmourEfficiency = 1.0f;
	m_HealthRegenEfficiency = 1.0f;
	m_ExplosiveDamageMod = 1.0f;
	m_HandGunDamageMod = 1.0f;
	m_RifleDamageMod = 1.0f;
	m_SmgDamageMod = 1.0f;
	m_PopulationFleeMod = 1.0f;
	m_HotwireRate = 1.0f;
	m_MotivationMin = 0; 
	m_MotivationMax = 0; 
	m_IsMale = true;
	m_IsHuman = true;
	m_ShouldRewardMoneyOnDeath = true;
	m_pCriminality = NULL;
	m_AllowSlowCruisingWithMusic = false;
	m_AllowRoadCrossHurryOnLightChange = true;
	m_MovementModesPtr = NULL;

	m_VehicleTypes.Reset();
}

void CPedModelInfo::PersonalityData::PostLoad()
{
	m_MovementModesPtr = CPedModelInfo::FindPersonalityMovementModes(m_MovementModes);

	// Cache ped personality criminality component
	m_pCriminality = CPedModelInfo::GetPersonalityCriminality(m_Criminality.GetHash());
}

//
// name:		CPedModelInfo::Shutdown
// description:	Shutdown pedmodelinfo class
void CPedModelInfo::Shutdown()
{
	CBaseModelInfo::Shutdown();

	if(m_pSeatInfo)
	{
		delete m_pSeatInfo;
		m_pSeatInfo = NULL;
	}
}

void CPedModelInfo::ShutdownExtra()
{
	if (m_pPedVarInfoCollection)
	{
		atArray<CPedVariationInfo*>& infos = m_pPedVarInfoCollection->GetInfos();

		for (u32 i = 0; i < infos.GetCount(); i++)
		{
			if (infos[i]->GetDlcNameHash() != 0)
			{
				infos.DeleteFast(i);
				i--;
			}
		}
	}
}

//
//        name: CPedModelInfo::Init
// description: Initialize the CPedModelInfo from a given set of init data (typically loaded from an xml file)
//
void CPedModelInfo::Init(const InitData & rInitData)
{
	Init();

	strLocalIndex parentTxdSlot = strLocalIndex(FindTxdSlotIndex(rInitData.m_Name.c_str()));
	if(parentTxdSlot == -1)
	{
		parentTxdSlot = GetCommonTxd();
	}
	else if (g_TxdStore.GetParentTxdSlot(parentTxdSlot) == -1)
	{
		g_TxdStore.SetParentTxdSlot(parentTxdSlot, strLocalIndex(GetCommonTxd()));
	}

	// temp hack to prevent ig_ peds from dropping to super LOD too close to camera
	char pedName[100];
	sprintf(pedName, "%s", rInitData.m_Name.c_str());
	strlwr(pedName);
	if (strncmp(pedName,"ig_",3)==0){
		m_bIs_IG_Ped = true;
	} else {
		m_bIs_IG_Ped = false;
	}

	// some peds (streamable gfx ones) have their own fragments - find them
	// (turns out ALL peds are pulling in their fragments for the time being)
	//if (rInitData.m_IsStreamedGfx){
		SetDrawableOrFragmentFile(rInitData.m_Name.c_str(), parentTxdSlot.Get(), true);				// known to be a fragment
	//}

	// Expression set checking.  If you've set an expression set to use, then the specific:
	// rInitData.m_ExpressionDictionaryName and
	// rInitData.m_ExpressionName
	// should both be empty/null.  Expression sets are for peds only at the moment.
	if(rInitData.m_ExpressionSetName && rInitData.m_ExpressionSetName != s_HashOfNullString)
	{
		Assertf(!rInitData.m_ExpressionDictionaryName || rInitData.m_ExpressionDictionaryName == s_HashOfNullString, "Expression Set (%s) should not be specified if expression dictionary is also specified", rInitData.m_ExpressionSetName.GetCStr());
		Assertf(!rInitData.m_ExpressionName || rInitData.m_ExpressionName == s_HashOfNullString, "Expression Set (%s) should not be specified if expression name is also specified", rInitData.m_ExpressionSetName.GetCStr());
	}

	if (!rInitData.m_IsStreamedGfx)
	{
		this->SetPropsFile(rInitData.m_PropsName.c_str());
		this->SetPedComponentFile(rInitData.m_Name.c_str());
	}
	else
		this->SetPropStreamFolder(rInitData.m_PropsName.c_str());

	this->SetPedMetaDataFile(rInitData.m_Name.c_str());
	this->SetClipDictionaryIndex(rInitData.m_ClipDictionaryName);
#if ENABLE_BLENDSHAPES
	this->SetBlendShapeFile(rInitData.m_BlendShapeFileName);
#endif // ENABLE_BLENDSHAPES
	this->SetExpressionDictionaryIndex(rInitData.m_ExpressionDictionaryName);
	this->SetExpressionHash(rInitData.m_ExpressionName);
	this->SetCreatureMetadataFile(rInitData.m_CreatureMetadataName);
	this->SetDecisionMakerHash(rInitData.m_DecisionMakerName);
	this->SetMotionTaskDataSetHash(rInitData.m_MotionTaskDataSetName);
	this->SetDefaultTaskDataSetHash(rInitData.m_DefaultTaskDataSetName);
	this->SetPedCapsuleHash(rInitData.m_PedCapsuleName);
	this->SetLayoutHash(rInitData.m_PedLayoutName);
	this->SetPedComponentSetHash(rInitData.m_PedComponentSetName);
	this->SetPedComponentClothHash(rInitData.m_PedComponentClothName);
	this->SetPedIKSettingsHash(rInitData.m_PedIKSettingsName);
	this->SetTaskDataHash(rInitData.m_TaskDataName);
	this->SetIsStreamedGfx(rInitData.m_IsStreamedGfx);
	m_ScenarioPedStreamingSlot = rInitData.m_ScenarioPopStreamingSlot;
	m_DefaultSpawningPreference = rInitData.m_DefaultSpawningPreference;
	m_DefaultRemoveRangeMultiplier = rInitData.m_DefaultRemoveRangeMultiplier;
	m_AllowCloseSpawning = rInitData.m_AllowCloseSpawning;
	this->SetAmbulanceShouldRespondTo(rInitData.m_AmbulanceShouldRespondTo);
	this->SetSpecialAbilityType(rInitData.m_AbilityType);
	this->SetThermalBehaviour(rInitData.m_ThermalBehaviour);
	this->SetSuperlodType(rInitData.m_SuperlodType);
	this->SetDefaultBrawlingStyleHash(rInitData.m_DefaultBrawlingStyle);
	this->SetDefaultWeaponHash(rInitData.m_DefaultUnarmedWeapon);
	this->SetNavCapabilitiesHash(rInitData.m_NavCapabilitiesName);
	this->SetPerceptionHash(rInitData.m_PerceptionInfo);
	this->SetVfxInfo(rInitData.m_VfxInfoName.GetHash());
	m_AmbientClipsForFlee = rInitData.m_AmbientClipsForFlee;

	Assertf(rInitData.m_RelationshipGroup.GetCStr(), " %s does not have a relationship group defined", rInitData.m_Name.c_str()); 
	this->SetRelationshipGroupHash(rInitData.m_RelationshipGroup);

	// 	Col model is now made when clump is loaded
	//RAGE	this->SetColModel(&CTempColModels::ms_colModelPed1);

	this->SetIsPlayerModel(CPedType::IsPlayerModel(rInitData.m_Pedtype));
	this->SetDefaultPedType(CPedType::FindPedType(rInitData.m_Pedtype));

	this->SetCanRideBikeWithNoHelmet(rInitData.m_CanRideBikeWithNoHelmet);
	this->SetCanSpawnInCar(rInitData.m_CanSpawnInCar);
	this->SetOnlyBulkyItemVariations(rInitData.m_bOnlyBulkyItemVariations);

	//	this->SetDefaultPedStats(CPedStats::GetPedStatType(pedstats));

	// Find and sanity check the requested clip set id
	fwMvClipSetId clipSetId = fwClipSetManager::GetClipSetId(rInitData.m_MovementClipSet);
	Assertf(clipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognized movement clipset", rInitData.m_MovementClipSet.GetCStr());
	this->SetMovementClipSet(clipSetId);
	this->SetMovementClipSets(rInitData.m_MovementClipSets);

	// Find and sanity check the requested strafe clip set id
	if(rInitData.m_StrafeClipSet)
	{
		clipSetId = fwClipSetManager::GetClipSetId(rInitData.m_StrafeClipSet);
		Assertf(clipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognized strafe clipset", rInitData.m_StrafeClipSet.GetCStr());
		this->SetStrafeClipSet(clipSetId);
	}

	if(rInitData.m_MovementToStrafeClipSet)
	{
		clipSetId = fwClipSetManager::GetClipSetId(rInitData.m_MovementToStrafeClipSet);
		Assertf(clipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognized movement to strafe clipset", rInitData.m_MovementToStrafeClipSet.GetCStr());
		this->SetMovementToStrafeClipSet(clipSetId);
	}

	if(rInitData.m_InjuredStrafeClipSet)
	{
		clipSetId = fwClipSetManager::GetClipSetId(rInitData.m_InjuredStrafeClipSet);
		Assertf(clipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognized injured strafe clipset", rInitData.m_InjuredStrafeClipSet.GetCStr());
		this->SetInjuredStrafeClipSet(clipSetId);
	}

	if(rInitData.m_FullBodyDamageClipSet)
	{
		clipSetId = fwClipSetManager::GetClipSetId(rInitData.m_FullBodyDamageClipSet);
		Assertf(clipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognized full body damage clipset", rInitData.m_FullBodyDamageClipSet.GetCStr());
		this->SetFullBodyDamageClipSet(clipSetId);
	}

	if(rInitData.m_AdditiveDamageClipSet)
	{
		clipSetId = fwClipSetManager::GetClipSetId(rInitData.m_AdditiveDamageClipSet);
		Assertf(clipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognized additive damage clipset", rInitData.m_AdditiveDamageClipSet.GetCStr());
		this->SetAdditiveDamageClipSet(clipSetId);
	}

	if(rInitData.m_ExpressionSetName && rInitData.m_ExpressionSetName != s_HashOfNullString)
	{
		fwMvExpressionSetId expressionSetId = fwExpressionSetManager::GetExpressionSetId(rInitData.m_ExpressionSetName);
		Assertf(expressionSetId != EXPRESSION_SET_ID_INVALID, "%s:Unrecognized expression set", rInitData.m_ExpressionSetName.GetCStr());
		this->SetExpressionSet(expressionSetId);
	}

	// Facial Clipset Group
	fwMvFacialClipSetGroupId facialClipSetGroupId = fwFacialClipSetGroupManager::GetFacialClipSetGroupId(rInitData.m_FacialClipsetGroupName);
	//Assertf(facialClipSetGroupId != FACIAL_CLIP_SET_GROUP_ID_INVALID, "%s:Unrecognized facial clipset group", rInitData.m_FacialClipsetGroupName.GetCStr());
	this->SetFacialClipSetGroupId(facialClipSetGroupId);

	// Find and sanity check the requested gesture clip set id
	clipSetId = fwClipSetManager::GetClipSetId(rInitData.m_DefaultGestureClipSet);
	Assertf(clipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognised default gesture clipset", rInitData.m_DefaultGestureClipSet.GetCStr());
	this->SetDefaultGestureClipSet(clipSetId);

	// Find and sanity check the requested viseme clip set id
	clipSetId = fwClipSetManager::GetClipSetId(rInitData.m_DefaultVisemeClipSet);
	Assertf(clipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognised default viseme clipset", rInitData.m_DefaultVisemeClipSet.GetCStr());
	this->SetDefaultVisemeClipSet(clipSetId);

	m_SidestepClipSet = fwClipSetManager::GetClipSetId(rInitData.m_SidestepClipSet);

	this->SetPoseMatcherFile(rInitData.m_PoseMatcherName);
	this->SetPoseMatcherProneFile(rInitData.m_PoseMatcherProneName);
	this->SetGetupSet(rInitData.m_GetupSetHash);

	this->SetRadioStations((s32)rInitData.m_Radio1, (s32)rInitData.m_Radio2);
	this->SetFOffsets(rInitData.m_FUpOffset,rInitData.m_FFrontOffset);
	this->SetROffsets(rInitData.m_RUpOffset,rInitData.m_RFrontOffset);
	this->SetAudioPedType(-1);
	this->SetFirstVoice(-1);
	this->SetLastVoice(-1);
	this->SetPedVoiceGroup(rInitData.m_PedVoiceGroup.GetHash());
	this->SetAnimalAudioObject(rInitData.m_AnimalAudioObject.GetHash());
	this->SetMinActivationImpulse(rInitData.m_MinActivationImpulse);
	this->SetStubble(rInitData.m_Stubble);
	this->SetAge(rInitData.m_Age);
	this->SetSexinessFlags(rInitData.m_Sexiness);
	this->SetMaxPassengersInCar(rInitData.m_MaxPassengersInCar);
	this->SetExternallyDrivenDOFs(rInitData.m_ExternallyDrivenDOFs);
	SetHDDist(rInitData.m_HDDist);
	if (rInitData.m_HDDist > 0.f)
	{
		SetupHDFiles(rInitData.m_Name.c_str());
	}
	this->SetTargetingThreatModifier(rInitData.m_TargetingThreatModifier);
	this->SetKilledPerceptionRangeModifer(rInitData.m_KilledPerceptionRangeModifer);

	//load optional components
	const CPedComponentSetInfo* pComponentSetInfo = CPedComponentSetManager::GetInfo(GetPedComponentSetHash().GetHash());
	if (pComponentSetInfo && pComponentSetInfo->GetIsRidable())
	{
		m_pSeatInfo = rage_new CModelSeatInfo();
		m_pSeatInfo->Reset();
	}

	const atHashString defaultHash("Default",0xE4DF46D5);
	atHashString initHash = defaultHash;
	if (rInitData.m_Personality.GetHash() != 0)
	{
		initHash = rInitData.m_Personality;
	}

	s16 iPersonalityIndex = FindPersonalityFromHash(initHash.GetHash());
	Assertf(iPersonalityIndex != -1, "Ped (%s) has personality (%s) which doesn't exist!", GetModelName(), initHash.GetCStr());
	if( iPersonalityIndex == -1 )
	{
		iPersonalityIndex = FindPersonalityFromHash(defaultHash.GetHash());
		Assertf(iPersonalityIndex != -1, "Ped (%s) falling back to default personality (%s) which doesn't exist!", GetModelName(), defaultHash.GetCStr());
	}
	this->SetPersonalitySettingsIdx(iPersonalityIndex);

	// Check if we don't have a hash set for our combat info and set it to the default if not
	static atHashWithStringNotFinal defaultCombatHash("DEFAULT",0xE4DF46D5);
	u32 uCombatInfoHash = rInitData.m_CombatInfo.GetHash();
	this->SetCombatInfoHash(uCombatInfoHash != 0 ? uCombatInfoHash : defaultCombatHash.GetHash());

	m_bIsHeadBlendPed = rInitData.m_IsHeadBlendPed;

	// Initialise unlocked special ability stats early on so the selector UI has valid data
	if(rInitData.m_AbilityType != SAT_NONE)
	{
		int prefix = -1;
		u32 modelNameHash = GetModelNameHash();
		if( modelNameHash == MI_PLAYERPED_PLAYER_ZERO.GetName() )
		{
			prefix = 0; // Micheal
		}
		else if( modelNameHash == MI_PLAYERPED_PLAYER_ONE.GetName() )
		{
			prefix = 1; // Franklin
		}
		else if( modelNameHash == MI_PLAYERPED_PLAYER_TWO.GetName() )
		{
			prefix = 2; // Trevor
		}
		if(prefix != -1)
		{
			const CSpecialAbilityData* data = CPlayerSpecialAbilityManager::GetSpecialAbilityData(rInitData.m_AbilityType);
			char statName[64];
			formatf(statName, "SP%d_SPECIAL_ABILITY_UNLOCKED", prefix);
			StatId stat(statName);

			float unlockedCapacity = (float)data->initialUnlockedCap;
			if (StatsInterface::IsKeyValid(stat))
			{
				s32 capacityStatValue = (s32)floor(100.f * (unlockedCapacity / data->duration));
				if (StatsInterface::GetIntStat(stat) <= capacityStatValue)
				{
					StatsInterface::SetStatData(stat, capacityStatValue, STATUPDATEFLAG_ASSERTONLINESTATS); // the stat variable wants a percentage [0, 100] not absolute duration
				}
			}
		}
	}
}


//
//        name: CPedModelInfo::InitMasterDrawableData
// description: 
//
void CPedModelInfo::InitMasterDrawableData(u32 modelIdx)
{
	CBaseModelInfo::InitMasterDrawableData(modelIdx);
	InitPedData();
}

void CPedModelInfo::DeleteMasterDrawableData()
{
	if (m_pPedVarInfoCollection)
	{
		delete m_pPedVarInfoCollection;
	}
	m_pPedVarInfoCollection = NULL;

	if (!m_isStreamedGraphics)
	{
		ShutdownLodSkeletonMap();
	}

	CBaseModelInfo::DeleteMasterDrawableData();
}

void CPedModelInfo::InitMasterFragData(u32 modelIdx)
{
	SetHasLoaded(true);
	gtaFragType* pFragType = static_cast<gtaFragType *>(GetFragType());		// lookup from store
	if (!pFragType)
		SetHasLoaded(false);

#if PHYSICS_LOD_GROUP_SHARING
	// - Make sure that Peds sharing physics data point to the fragManager's physicsLODGroup
	if (modelinfoVerifyf(pFragType, "CPedModelInfo::InitMasterFragData: Invalid frag type") && 
		pFragType->GetARTAssetID() >= 0 && !pFragType->GetPhysicsLODGroup())
	{
		CPhysics::ManagedPhysicsLODGroup& managedPhysicsLODGroup = CPhysics::GetManagedPhysicsLODGroup(pFragType->GetARTAssetID());
		pFragType->SetPhysicsLODGroup(managedPhysicsLODGroup.GetPhysicsLODGroup());
		strLocalIndex fragIndex = strLocalIndex(managedPhysicsLODGroup.GetPhysicsLODGroupFragStoreIdx());
		pFragType->CopyEstimatedCacheSizes(g_FragmentStore.Get(fragIndex));

		SetInvalidRagdollFilter(managedPhysicsLODGroup.GetInvalidRagdollFilter());
	}
	else
#endif
	{
		SetInvalidRagdollFilter(CPhysics::CreateInvalidRagdollFilter(*pFragType));
	}

	CBaseModelInfo::InitMasterFragData(modelIdx);
	ConfirmHDFiles();
	InitPedData();
	InitPhys();
}

void CPedModelInfo::DeleteMasterFragData()
{
	if (m_pPedVarInfoCollection)
	{
		delete m_pPedVarInfoCollection;
	}
	m_pPedVarInfoCollection = NULL;

	ShutdownLodSkeletonMap();

	SetInvalidRagdollFilter(NULL);

	CBaseModelInfo::DeleteMasterFragData();
}

void CPedModelInfo::SetPhysics(phArchetypeDamp* pPhysics)
{
	CBaseModelInfo::SetPhysics(pPhysics);
	InitPhys();
}

void CPedModelInfo::InitPedData()
{
	// setup variation information
	if (!m_pPedVarInfoCollection)
	{
		if (Verifyf(GetNumPedMetaDataFiles() > 0, "Ped '%s' has no meta data!", GetModelName()))
		{
			m_pPedVarInfoCollection = rage_new CPedVariationInfoCollection();
			atArray<CPedVariationInfo*>& infos = m_pPedVarInfoCollection->GetInfos();
			infos.Reserve(GetNumPedMetaDataFiles());

			for (s32 i = 0; i < GetNumPedMetaDataFiles(); ++i)
			{
				CPedVariationInfo* newInfo = g_fwMetaDataStore.Get(strLocalIndex(GetPedMetaDataFileIndex(i)))->GetObject<CPedVariationInfo>();

				EXTRAMETADATAMGR.FixupGlobalIndices(this, newInfo, GetVarInfo());
				Assert(newInfo);
				infos.Append() = newInfo;

#if !__FINAL
				if (PARAM_updatepedmetadata.Get())
					infos[infos.GetCount() - 1]->UpdateAndSave(GetModelName());
#endif // !__FINAL

#if __BANK
				if (infos[i])
				{
					char displayName[RAGE_MAX_PATH] = {0};
					formatf(displayName, "peds/%s", g_fwMetaDataStore.GetName(strLocalIndex(GetPedMetaDataFileIndex(i))));
					char fullPath[RAGE_MAX_PATH] = {0};
					formatf(fullPath, "X:\\gta5\\assets\\metadata\\characters\\%s.pso.meta", g_fwMetaDataStore.GetName(strLocalIndex(GetPedMetaDataFileIndex(i))));
					parRestRegisterSingleton(displayName, *infos[i], fullPath);
				}
#endif // __BANK
			}
		}

		const strLocalIndex componentFileIndex = strLocalIndex(GetPedComponentFileIndex());

		// determine the max available LOD level
		if (m_isStreamedGraphics){
			m_numAvailableLODs = 0;
		} else
		{
			m_numAvailableLODs = 0;
			Dwd* pDwd		= g_DwdStore.Get(componentFileIndex);
			Assert(pDwd);
			if (pDwd){
				u32 hashDummy = 0;
				rmcDrawable* pDrawable = CPedVariationPack::ExtractComponent(pDwd, PV_COMP_HEAD, 0, &hashDummy, 0);
				if (pDrawable)
				{
					if (pDrawable->GetLodGroup().ContainsLod(LOD_LOW)) {
						m_numAvailableLODs = 3;
					} else if (pDrawable->GetLodGroup().ContainsLod(LOD_MED)) {
						m_numAvailableLODs = 2;
					} else if (pDrawable->GetLodGroup().ContainsLod(LOD_HIGH)) {
						m_numAvailableLODs = 1;
					} 
				}
			}

			modelinfoAssertf(m_numAvailableLODs > 0, "%s can't determine number of available LOD levels", GetModelName());
		}

		// scan for component drawables
		if (m_pShaderEffectType)
		{
			// shader effect may be already created - so destroy it (will be recreated below):
			this->DestroyCustomShaderEffects();
		}
		modelinfoAssert(m_pShaderEffectType == NULL);

        if (m_pPedVarInfoCollection)
        {
            if(m_isStreamedGraphics)
            {
                m_pShaderEffectType = NULL;
#if LIVE_STREAMING
                if (strStreamingEngine::GetLive().GetEnabled())
                {
                    m_pPedVarInfoCollection->EnumTargetFolder(GetStreamFolder(), this);
                    m_pPedVarInfoCollection->EnumClothTargetFolder(GetStreamFolder());
                }
#endif // LIVE_STREAMING
            }
            else
            {
				m_pShaderEffectType = m_pPedVarInfoCollection->GetShaderEffectType(g_DwdStore.Get( componentFileIndex ), this);
                m_pPedVarInfoCollection->EnumCloth(this);				
            }

			m_pPedVarInfoCollection->m_bIsStreamed = m_isStreamedGraphics;
        }
	}
	
	crSkeletonData *pSkelData = NULL;
	if(GetFragType())
	{
		pSkelData = GetFragType()->GetCommonDrawable()->GetSkeletonData();
	}
	else
	{
		pSkelData = GetDrawable()->GetSkeletonData();
	}
	modelinfoAssert(pSkelData);

	{
		CPedModelInfo* slodMI = NULL;
		switch (GetSuperlodType())
		{
		case SLOD_HUMAN:
			slodMI = (CPedModelInfo*)CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("slod_human",0x3F039CBA), NULL);
            break;
		case SLOD_SMALL_QUADRUPED:
			slodMI = (CPedModelInfo*)CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("slod_small_quadped",0x2D7030F3), NULL);
            break;
		case SLOD_LARGE_QUADRUPED:
			slodMI = (CPedModelInfo*)CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("slod_large_quadped",0x856CFB02), NULL);
            break;
        case SLOD_KEEP_LOWEST:
		case SLOD_NULL:
			break;
		default:
			Assertf(false, "Invalid slod type for ped '%s'", GetModelName());
		}

		if (slodMI && slodMI != this)
		{
			const crSkeletonData* lodSkelData = NULL;
			gtaFragType* fragType = slodMI->GetFragType();
			if(fragType)
			{
				lodSkelData = fragType->GetCommonDrawable()->GetSkeletonData();
			}
			else if (Verifyf(slodMI->GetDrawable(), "Ped superlod '%s' has no drawable! (%s)", slodMI->GetModelName(), GetModelName()))
			{
				lodSkelData = slodMI->GetDrawable()->GetSkeletonData();
			}
			InitLodSkeletonMap(pSkelData, lodSkelData, slodMI);
		}
	}

	if(pSkelData)
	{
		m_LightFxBoneIdx = 0xffff;
		m_LightFxBoneSwitchIdx = 0xffff;

		const crBoneData *pBone0 = pSkelData->FindBoneData("FX_Light");
		if(pBone0)
		{
			Assertf(pBone0->GetIndex() <= 0xffff, "Model: %s, boneIdx=%d.", GetModelName(), pBone0->GetIndex());
			m_LightFxBoneIdx = (u16)pBone0->GetIndex();

			const crBoneData *pBone1 = pSkelData->FindBoneData("FX_Light_Switch");
			if(pBone1)
			{
				Assertf(pBone1->GetIndex() <= 0xffff, "Model: %s, boneIdx=%d.", GetModelName(), pBone1->GetIndex());
				m_LightFxBoneSwitchIdx = (u16)pBone1->GetIndex();
			}
		}


		m_LightMpBoneIdx = 0xffff;

		enum { MP_F_FREEMODE_01 = 0x9C9EFFD8 };	//"mp_f_freemode_01"
		enum { MP_M_FREEMODE_01 = 0x705E61F2 };	//"mp_m_freemode_01"

		const u32 modelNameHash = GetModelNameHash();
		if((modelNameHash == MP_M_FREEMODE_01) || (modelNameHash == MP_F_FREEMODE_01))
		{
			const crBoneData *pBone2 = pSkelData->FindBoneData("Skel_Spine3");
			if(pBone2)
			{
				Assertf(pBone2->GetIndex() <= 0xffff, "Model: %s, boneIdx=%d.", GetModelName(), pBone2->GetIndex());
				m_LightMpBoneIdx = (u16)pBone2->GetIndex();
			}
		}
	}

    if (m_pPedVarInfoCollection)
    {
        m_pPedVarInfoCollection->InitAnchorPoints(pSkelData);

#if LIVE_STREAMING
        if (strStreamingEngine::GetLive().GetEnabled())
        {
            if (GetIsStreamedGfx() && strcmp(GetPropStreamFolder(), "null"))
                m_pPedVarInfoCollection->EnumTargetPropFolder(GetPropStreamFolder(), this);
        }
#endif // LIVE_STREAMING

#if __ASSERT
		if (!GetIsStreamedGfx() && GetPropsFileIndex() != -1)
			artAssertf(m_pPedVarInfoCollection->ValidateProps(GetPropsFileIndex(), 0), "Missing prop file for %s, please set it to 'null' in peds.meta", GetModelName());
#endif
    }

	// Build up a list of Entry/Exit points
	if (m_pSeatInfo)
		InitLayout(pSkelData);

	// register head blend slots
	atHashString pedName(GetStreamFolder());
    if (pedName.GetHash() == g_headBlendPed)
	{
		Assert( m_pPedVarInfoCollection );			// TODO: just couple of lines above there is if(m_pPedVarInfoCollection) ... so the line below will crash if ped variation info is missing
		u32 numHeadDrawables = m_pPedVarInfoCollection->GetMaxNumDrawbls(PV_COMP_HEAD);
		const char*	pedFolder = GetStreamFolder();
		char buf[64];

        Assertf(numHeadDrawables >= MAX_NUM_BLENDS + 1, "We need %d blend head targets, only %d were found!", MAX_NUM_BLENDS + 1, numHeadDrawables);

		// generate the required slot names and register them with the mesh blend manager
		for (u32 i = 0; i < numHeadDrawables; ++i)
		{
			sprintf(buf, "%s/head_%03d_r", pedFolder, i);
			s32 dwdSlotId = g_DwdStore.FindSlot(buf).Get();
			Assertf(dwdSlotId != -1, "Drawable for head blend target couldn't be found: '%s'", buf);

			sprintf(buf, "%s/head_diff_%03d_a_whi", pedFolder, i);
			s32 txdSlotId = g_TxdStore.FindSlot(buf).Get();
			Assertf(txdSlotId != -1, "Texture for head blend target couldn't be found: '%s'", buf);

			sprintf(buf, "%s/feet_diff_%03d_a_whi", pedFolder, i);
			s32 feetTxdSlotId = g_TxdStore.FindSlot(buf).Get();
			Assertf(feetTxdSlotId != -1, "Texture for head blend target couldn't be found: '%s'", buf);

			sprintf(buf, "%s/uppr_diff_%03d_a_whi", pedFolder, i);
			s32 upprTxdSlotId = g_TxdStore.FindSlot(buf).Get();
			Assertf(upprTxdSlotId != -1, "Texture for head blend target couldn't be found: '%s'", buf);

			sprintf(buf, "%s/lowr_diff_%03d_a_whi", pedFolder, i);
			s32 lowrTxdSlotId = g_TxdStore.FindSlot(buf).Get();
			Assertf(lowrTxdSlotId != -1, "Texture for head blend target couldn't be found: '%s'", buf);

			MESHBLENDMANAGER.RegisterPedSlot(dwdSlotId, txdSlotId, feetTxdSlotId, upprTxdSlotId, lowrTxdSlotId);
		}

        MESHBLENDMANAGER.LockSlotRegistering();

		fwModelId headBlendPed = CModelInfo::GetModelIdFromName(g_headBlendPed);
		if (headBlendPed.IsValid())
		{
			CModelInfo::ClearAssetRequiredFlag(headBlendPed, STRFLAG_DONTDELETE);
			CModelInfo::RemoveAssets(headBlendPed);
		}
	}
}


//
// name:		SetPropsFile
// description:	Set props file required when loading this model
void CPedModelInfo::SetPropsFile(const char* pName)
{
	modelinfoAssert( pName );
	if ( stricmp( pName, "null") == 0 )
	{
    	m_propsFileIndex=-1;
		return;
	}

	m_propsFilenameHashcode = atStringHash(pName);

	m_propsFileIndex = g_DwdStore.Register(pName).Get();
	OUTPUT_ONLY(g_DwdStore.SetNoAssignedTxd(strLocalIndex(m_propsFileIndex), true);)

	strLocalIndex txdFileIndex = strLocalIndex(g_TxdStore.Register(pName));
	if (txdFileIndex != -1){
		g_DwdStore.SetParentTxdForSlot(strLocalIndex(m_propsFileIndex), txdFileIndex);
	}
}

//
// name:		SetPedComponentFile
// description:	Set ped component file required when loading this model
void CPedModelInfo::SetPedComponentFile(const char* pName)
{
	modelinfoAssert( pName );
	if ( stricmp( pName, "null") == 0 )
	{
		m_pedCompFileIndex=-1;
		return;
	}
	m_pedCompFileIndex = g_DwdStore.Register(pName).Get();
	OUTPUT_ONLY(g_DwdStore.SetNoAssignedTxd(strLocalIndex(m_pedCompFileIndex), true);)

	strLocalIndex txdFileIndex = strLocalIndex(g_TxdStore.Register(pName));
	if (txdFileIndex != -1){
		g_DwdStore.SetParentTxdForSlot(strLocalIndex(m_pedCompFileIndex), txdFileIndex);
	}
}

//
// name:		SetPedMetaDataFile
// description:	Store ped metadata file index plus additional DLC files.
void CPedModelInfo::SetPedMetaDataFile(const char* pName)
{
	if (PARAM_nopedmetadata.Get())
		return;

	modelinfoAssert( pName );
	if (stricmp(pName, "null") == 0)
	{
		m_pedMetaDataFileIndices.Reset();
		return;
	}

	SetPedMetaDataFile(g_fwMetaDataStore.FindSlot(pName).Get());
}

void CPedModelInfo::SetPedMetaDataFile(s32 index)
{
	if (artVerifyf(index != -1, "No metadata file found for ped '%s'", GetModelName()))
	{
		m_pedMetaDataFileIndices.PushAndGrow(index, 1);
	}
}

void CPedModelInfo::RemovePedMetaDataFile(const char* pName)
{
	RemovePedMetaDataFile(g_fwMetaDataStore.FindSlot(pName).Get());
}

void CPedModelInfo::RemovePedMetaDataFile(s32 index)
{
	if (PARAM_nopedmetadata.Get())
		return;	

	if (artVerifyf(index != -1, "No metadata file found for ped"))
	{
		m_pedMetaDataFileIndices.DeleteMatches(index);
	}
}

void CPedModelInfo::SetMovementClipSets(const atArray<atHashString>& movementClipSets)
{
	s32 iCount = movementClipSets.GetCount();
	m_movementClipSets.Resize(iCount);
	for(s32 i = 0; i < iCount; i++)
	{
		fwMvClipSetId clipSetId = fwClipSetManager::GetClipSetId(movementClipSets[i]);
		Assertf(clipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognized movement clipset", movementClipSets[i].GetCStr());
		m_movementClipSets[i] = clipSetId;
	}
}

fwMvClipSetId CPedModelInfo::GetRandomMovementClipSet(const CPed* pPed) const
{
	if(m_movementClipSets.GetCount() > 0 && pPed)
	{
		return m_movementClipSets[pPed->GetRandomSeed()%m_movementClipSets.GetCount()];
	}
	return m_movementClipSet;
}

fwMvClipSetId CPedModelInfo::GetAppropriateStrafeClipSet(const CPed* pPed) const 
{
	return (pPed && pPed->HasHurtStarted() && m_injuredStrafeClipSet != CLIP_SET_ID_INVALID) ? 
		m_injuredStrafeClipSet : m_strafeClipSet; 
}

/*RAGE
//-------------------------------------------------------------------------
// Create the Hit collision model for a character skeleton
//-------------------------------------------------------------------------
void CPedModelInfo::CreateHitColModelSkinned(crSkeleton* pSkeleton)
{
	CColModel* pColModel = rage_new CColModel();
	pColModel->AllocateData(NUM_PED_COLMODEL_NODES, 0, 0, 0, 0);
	CCollisionData* pHitColData = pColModel->GetCollisionData();
	CColSphere* pColSpheres = pHitColData->m_pSphereArray;

	Vector3 vPos;
	Matrix34 mLocalMat;

	for( s32 i = 0; i < NUM_PED_COLMODEL_NODES; i++ )
	{
		vPos.x = m_pColNodeInfos[i].xOffset;
		vPos.y = m_pColNodeInfos[i].yOffset;
		vPos.z = m_pColNodeInfos[i].zOffset;

		mLocalMat = pSkeleton->GetLocalMtx(CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pSkeleton, (eAnimBoneTag)m_pColNodeInfos[i].boneId));

		mLocalMat.Transform( vPos );

		pColSpheres[i].m_centre = vPos;
		pColSpheres[i].m_radius = m_pColNodeInfos[i].fRadius;

		//pColSpheres[i].m_data.m_nSurfaceType = SURFACE_TYPE_PED;
		pColSpheres[i].m_data.m_nPieceType = m_pColNodeInfos[i].nPieceType;
	}

	mLocalMat = pSkeleton->GetLocalMtx(CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pSkeleton, BONETAG_SPINE1));
	mLocalMat.Transform( vPos );
	pColModel->GetBoundSphere().Set(1.5f, vPos);
	pColModel->GetBoundBox().Set(vPos - Vector3(1.2f, 1.2f, 1.2f), vPos + Vector3(1.2f, 1.2f, 1.2f));
	m_pHitColModel = pColModel;
//	RpHAnimHierarchy *pHierarchy = GetAnimHierarchyFromSkinClump(pClump);
//	RwMatrix *pMatrix = NULL;
//	CColModel* pColModel = new CColModel();
//	CColSphere* pColSpheres;
//	CVector zero(0.0f,0.0f,0.0f);
//	CVector posn;
//	s32 nBone = -1;
////	RwMatrix *pInvertMatrix = RwMatrixCreate();
////	RwMatrix *pResultantMatrix = RwMatrixCreate();
//	RwMatrix *pInvertMatrix = CGame::m_pWorkingMatrix1;
//	RwMatrix *pResultantMatrix = CGame::m_pWorkingMatrix2;
//
//	pColModel->AllocateData(NUM_PED_COLMODEL_NODES, 0, 0, 0, 0);
//	modelinfoAssertf(pHierarchy, "%s:Doesn't have an animation hierarchy", GetModelName());
//
//	RwMatrixInvert(pInvertMatrix, RwFrameGetMatrix(RpClumpGetFrame(pClump)));
//
//	pColSpheres = pColModel->m_pColData->m_pSphereArray;
//
//	for(s32 i=0;i<NUM_PED_COLMODEL_NODES;i++)
//	{
//		// reset position vector
////		posn = zero;
//		posn.x = m_pColNodeInfos[i].xOffset;
//		posn.y = m_pColNodeInfos[i].yOffset;
//		posn.z = m_pColNodeInfos[i].zOffset;
//
//		// reset resulatant matrix
//		RwMatrixCopy(pResultantMatrix, pInvertMatrix);
//
//		// get the node matrix for this node/bone
////		nBone = ConvertPedNode2BoneTag(m_pColNodeInfos[i].boneId);
////		nBone = RpHAnimIDGetIndex(pHierarchy, nBone);
////		pMatrix = (RpHAnimHierarchyGetMatrixArray(pHierarchy) + nBone);
//
//		pMatrix = (RpHAnimHierarchyGetMatrixArray(pHierarchy) + RpHAnimIDGetIndex(pHierarchy, m_pColNodeInfos[i].boneId));
//
//		RwMatrixTransform(pResultantMatrix, pMatrix, rwCOMBINEPRECONCAT);
//		// transform node into ped frame
//		RwV3dTransformPoints(&posn, &posn, 1, pResultantMatrix);
//
//		pColSpheres[i].m_vecCentre = posn;// + CVector(m_pColNodeInfos[i].fHorzOffset,0.0f,m_pColNodeInfos[i].fVertOffset);
//		pColSpheres[i].m_fRadius = m_pColNodeInfos[i].fRadius;
//
//		pColSpheres[i].m_data.m_nSurfaceType = SURFACE_TYPE_PED;
//		pColSpheres[i].m_data.m_nPieceType = m_pColNodeInfos[i].nPieceType;
//	}
//
////	RwMatrixDestroy(pInvertMatrix);
////	RwMatrixDestroy(pResultantMatrix);
//
////	pColModel->m_pSphereArray = pColSpheres;
////	pColModel->m_nNoOfSpheres = NUM_PED_COLMODEL_NODES;
//	pColModel->GetBoundSphere().Set(1.5f, CVector(0.0f, 0.0f, 0.0f));
//	pColModel->GetBoundBox().Set(CVector(-0.5f, -0.5f, -1.2f), CVector(0.5f, 0.5f, 1.2f));
//	pColModel->m_level = LEVEL_GENERIC;
//	m_pHitColModel = pColModel;
//
}
*/

/*RAGE
// Returns a pointer to the peds hit collision model
// which has been animated to match the peds pose in the peds local frame
//
// i.e. all collision sphere coordinates are in local coordinates
CColModel *CPedModelInfo::AnimatePedColModelSkinned(crSkeleton* pSkeleton )
{
	if(m_pHitColModel == NULL)
	{
		CreateHitColModelSkinned(pSkeleton);
		return m_pHitColModel;
	}

	CCollisionData* pHitColData = m_pHitColModel->GetCollisionData();
	Vector3 vPos;
	Matrix34 mLocalMat;

	for( s32 i = 0; i < NUM_PED_COLMODEL_NODES; i++ )
	{
		vPos.x = m_pColNodeInfos[i].xOffset;
		vPos.y = m_pColNodeInfos[i].yOffset;
		vPos.z = m_pColNodeInfos[i].zOffset;

		mLocalMat = pSkeleton->GetLocalMtx(CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pSkeleton, (eAnimBoneTag)m_pColNodeInfos[i].boneId));

		mLocalMat.Transform( vPos );

		pHitColData->m_pSphereArray[i].m_centre = vPos;
	}

	mLocalMat = pSkeleton->GetLocalMtx(CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pSkeleton, BONETAG_SPINE1));
	mLocalMat.Transform( vPos );
	m_pHitColModel->GetBoundSphere().Set(1.5f, vPos);
	m_pHitColModel->GetBoundBox().Set(vPos - Vector3(1.2f, 1.2f, 1.2f), vPos + Vector3(1.2f, 1.2f, 1.2f));

	return m_pHitColModel;

//	CCollisionData* pHitColData = m_pHitColModel->GetCollisionData();
//	RpHAnimHierarchy *pHierarchy = GetAnimHierarchyFromSkinClump(pClump);
//	RwMatrix *pMatrix = NULL;
//	CVector zero(0.0f,0.0f,0.0f);
//	CVector posn;
//	s32 nBone = -1;
////	RwMatrix *pInvertMatrix = RwMatrixCreate();
////	RwMatrix *pResultantMatrix = RwMatrixCreate();
//	RwMatrix *pInvertMatrix = CGame::m_pWorkingMatrix1;
//	RwMatrix *pResultantMatrix = CGame::m_pWorkingMatrix2;
//
//	RwMatrixInvert(pInvertMatrix, RwFrameGetMatrix(RpClumpGetFrame(pClump)));
//
//	for(s32 i=0;i<NUM_PED_COLMODEL_NODES;i++)
//	{
//		// reset position vector
////		posn = zero;
//		posn.x = m_pColNodeInfos[i].xOffset;
//		posn.y = m_pColNodeInfos[i].yOffset;
//		posn.z = m_pColNodeInfos[i].zOffset;
//
//		// reset resulatant matrix
//		RwMatrixCopy(pResultantMatrix, pInvertMatrix);
//
//		// get the node matrix for this node/bone
////		nBone = ConvertPedNode2BoneTag(m_pColNodeInfos[i].frameId);
////		nBone = RpHAnimIDGetIndex(pHierarchy, nBone);
////		pMatrix = (RpHAnimHierarchyGetMatrixArray(pHierarchy) + nBone);
//
//		pMatrix = (RpHAnimHierarchyGetMatrixArray(pHierarchy) + RpHAnimIDGetIndex(pHierarchy, m_pColNodeInfos[i].boneId));
//
//		RwMatrixTransform(pResultantMatrix, pMatrix, rwCOMBINEPRECONCAT);
//
//		// transform node into ped frame
//		RwV3dTransformPoints(&posn, &posn, 1, pResultantMatrix);
//
//		pHitColData->m_pSphereArray[i].m_vecCentre = posn;// + CVector(m_pColNodeInfos[i].fHorzOffset,0.0f,m_pColNodeInfos[i].fVertOffset);
//	}
//
//	posn = zero;
//	RwMatrixCopy(pResultantMatrix, pInvertMatrix);
//	nBone = RpHAnimIDGetIndex(pHierarchy, BONETAG_SPINE1);
//	pMatrix = (RpHAnimHierarchyGetMatrixArray(pHierarchy) + nBone);
//	RwMatrixTransform(pResultantMatrix, pMatrix, rwCOMBINEPRECONCAT);
//	// transform node into ped frame
//	RwV3dTransformPoints(&posn, &posn, 1, pResultantMatrix);
//
//	m_pHitColModel->GetBoundSphere().Set(1.5f, posn);
//	m_pHitColModel->GetBoundBox().Set(posn - CVector(1.2f, 1.2f, 1.2f), posn + CVector(1.2f, 1.2f, 1.2f));
//
////	RwMatrixDestroy(pInvertMatrix);
////	RwMatrixDestroy(pResultantMatrix);
//
}
*/

/*RAGE
// Returns a pointer to the peds hit collision model
// which has been animated to match the peds pose in the world frame
//
// i.e. all collision sphere positions are in world coordinates
CColModel *CPedModelInfo::AnimatePedColModelSkinnedWorld(crSkeleton* pSkeleton)
{
	if(m_pHitColModel == NULL)
	{
		CreateHitColModelSkinned(pSkeleton);
		return m_pHitColModel;
	}

	CCollisionData* pHitColData = m_pHitColModel->GetCollisionData();
	Vector3 vPos;
	Matrix34 mLocalMat;

	for( s32 i = 0; i < NUM_PED_COLMODEL_NODES; i++ )
	{
		vPos.x = m_pColNodeInfos[i].xOffset;
		vPos.y = m_pColNodeInfos[i].yOffset;
		vPos.z = m_pColNodeInfos[i].zOffset;

		mLocalMat = pSkeleton->GetGlobalMtx(CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pSkeleton, (eAnimBoneTag)m_pColNodeInfos[i].boneId));

		mLocalMat.Transform( vPos );

		pHitColData->m_pSphereArray[i].m_centre = vPos;
	}

	mLocalMat = pSkeleton->GetLocalMtx(CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pSkeleton, BONETAG_SPINE1));
	mLocalMat.Transform( vPos );
	m_pHitColModel->GetBoundSphere().Set(1.5f, vPos);
	m_pHitColModel->GetBoundBox().Set(vPos - Vector3(1.2f, 1.2f, 1.2f), vPos + Vector3(1.2f, 1.2f, 1.2f));

	return m_pHitColModel;


//	CCollisionData* pHitColData = m_pHitColModel->GetCollisionData();
//	RpHAnimHierarchy *pHierarchy = GetAnimHierarchyFromSkinClump(pClump);
//	RwMatrix *pMatrix = NULL;
//	CVector zero(0.0f,0.0f,0.0f);
//	CVector posn;
//	s32 nBone = -1;
//
//	for(s32 i=0;i<NUM_PED_COLMODEL_NODES;i++)
//	{
//		// reset position vector
////		posn = zero;
//		posn.x = m_pColNodeInfos[i].xOffset;
//		posn.y = m_pColNodeInfos[i].yOffset;
//		posn.z = m_pColNodeInfos[i].zOffset;
//
//		// get the node matrix for this node/bone
////		nBone = ConvertPedNode2BoneTag(m_pColNodeInfos[i].frameId);
////		nBone = RpHAnimIDGetIndex(pHierarchy, nBone);
////		pMatrix = (RpHAnimHierarchyGetMatrixArray(pHierarchy) + nBone);
//
//		pMatrix = (RpHAnimHierarchyGetMatrixArray(pHierarchy) + RpHAnimIDGetIndex(pHierarchy, m_pColNodeInfos[i].boneId));
//
//		// transform node into world
//		RwV3dTransformPoints(&posn, &posn, 1, pMatrix);
//
//		pHitColData->m_pSphereArray[i].m_vecCentre = posn;// + CVector(m_pColNodeInfos[i].fHorzOffset,0.0f,m_pColNodeInfos[i].fVertOffset);
//	}
//
//	posn = zero;
//	nBone = RpHAnimIDGetIndex(pHierarchy, BONETAG_SPINE1);
//	pMatrix = (RpHAnimHierarchyGetMatrixArray(pHierarchy) + nBone);
//	// transform node into world
//	RwV3dTransformPoints(&posn, &posn, 1, pMatrix);
//
//	m_pHitColModel->GetBoundSphere().Set(1.5f, posn);
//	m_pHitColModel->GetBoundBox().Set(posn - CVector(1.2f, 1.2f, 1.2f), posn + CVector(1.2f, 1.2f, 1.2f));
//
	return m_pHitColModel;
}
*/

void CPedModelInfo::IncrementVoice(void)
{
	if((m_FirstVoice<0) || (m_LastVoice<0))
	{
		m_NextVoice = -1;
		return;
	}

	m_NextVoice++;
	if ((m_NextVoice>m_LastVoice) || (m_NextVoice<m_FirstVoice))
		m_NextVoice = m_FirstVoice;

	return;
}

#define NUM_RAGDOLL_COMPONENTS (21)

void CPedModelInfo::InitPhys()
{
	u32 nRagdollTypeFlags = ArchetypeFlags::GTA_RAGDOLL_TYPE;
	u32 nRagdollIncludeFlags = static_cast<u32>(ArchetypeFlags::GTA_RAGDOLL_INCLUDE_TYPES);
	if(!HasPhysics())
	{
		// create a bound consisting of a capsule
		// (needs to be a composite because capsules are orientated around local y-axis)
		phBoundComposite* pComposite = GeneratePedBound();

		phArchetypeDamp* archetype = rage_new phArchetypeDamp;

		const CBaseCapsuleInfo* pCapsuleInfo = CPedCapsuleInfoManager::GetInfo(GetPedCapsuleHash().GetHash());
		modelinfoAssert(pCapsuleInfo);

		u32 nBoundTypeFlags = ArchetypeFlags::GTA_PED_TYPE;
		u32 nBoundIncludeFlags = ArchetypeFlags::GTA_PED_INCLUDE_TYPES;
		if(pCapsuleInfo->IsQuadruped())
		{
			const CQuadrupedCapsuleInfo* pQuadCapsuleInfo = pCapsuleInfo->GetQuadrupedCapsuleInfo();
			modelinfoAssert(pQuadCapsuleInfo);
			if(pQuadCapsuleInfo->GetUseHorseMapCollision())
			{
				nBoundTypeFlags = ArchetypeFlags::GTA_HORSE_TYPE;
				nRagdollTypeFlags = ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;
				// Remove the include flag which allows the horse capsule to collide with parts of the map tagged as mover
				// and replace this with collisions against the special horse map type.
				nBoundIncludeFlags &= ~ArchetypeFlags::GTA_MAP_TYPE_MOVER;
				nBoundIncludeFlags |= ArchetypeFlags::GTA_MAP_TYPE_HORSE;

				nRagdollIncludeFlags &= ~ArchetypeFlags::GTA_MAP_TYPE_MOVER;
				nRagdollIncludeFlags |= ArchetypeFlags::GTA_MAP_TYPE_HORSE;
			}
		}

		// Peds can detect the stair slope collision bounds.
		nBoundIncludeFlags |= ArchetypeFlags::GTA_STAIR_SLOPE_TYPE;

		// set flags in archetype to say what type of physics object this is
		archetype->SetTypeFlags(nBoundTypeFlags);

		// set flags in archetype to say what type of physics object we wish to collide with
		archetype->SetIncludeFlags(nBoundIncludeFlags);

		Vector3 angInertia = VEC3V_TO_VECTOR3(pComposite->GetComputeAngularInertia(pCapsuleInfo->GetMass()));
		archetype->SetMass(pCapsuleInfo->GetMass());
		archetype->SetAngInertia(angInertia);
		archetype->SetGravityFactor(PED_GRAVITY_FACTOR);
		archetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(PED_DRAG_FACTOR, PED_DRAG_FACTOR, PED_DRAG_FACTOR));
		archetype->SetMaxAngSpeed(PED_MAX_ANG_SPEED);

		archetype->SetBound(pComposite);

		// need to relenquish control of the bounds we just created now that they're added to the archetype
		pComposite->Release();

		// Add a reference to the physics archetype
		// as its loading and unloading is bound to this model
		archetype->AddRef();

		// finally store in modelinfo
		SetPhysics(archetype);

		UpdateBoundingVolumes(*archetype->GetBound());
	}

	if(GetFragType())
	{
#if HORSE_RAGDOLL_HACK
		// Work around for horse ragdoll crash. Need to be remove once we can build correct NM assets
		if (GetFragType()->GetPhysics(0)->GetNumChildren() != NUM_RAGDOLL_COMPONENTS)
		{
			GetFragType()->SetARTAssetID(-1);
		}
#endif // HORSE_RAGDOLL_HACK
#if __ASSERT
		// Assert that non-humans have an art assert ID of -1
		if (GetFragType()->GetPhysics(0)->GetNumChildren() != NUM_RAGDOLL_COMPONENTS)
			Assert(GetFragType()->GetARTAssetID() == -1);

		float highLODRagdollMass = 0.0f;
#endif // __ASSERT

		for(int currentLOD=0; GetFragType()->GetPhysicsLODGroup()->GetLODByIndex(currentLOD); currentLOD++)
		{
			// set flags in archetype to say what type of physics object this is
			GetFragType()->GetPhysics(currentLOD)->GetArchetype()->SetTypeFlags(nRagdollTypeFlags);

			// set flags in archetype to say what type of physics object we wish to collide with
			GetFragType()->GetPhysics(currentLOD)->GetArchetype()->SetIncludeFlags(nRagdollIncludeFlags);

			// Set the material for animals 
			if (GetFragType()->GetARTAssetID() < 0)
			{
				phBound* pBound = GetFragType()->GetPhysics(currentLOD)->GetArchetype()->GetBound();
				if(pBound->GetType()==phBound::COMPOSITE)
				{
					phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pBound);
					for (s32 i=0; i<pBoundComp->GetNumBounds(); i++)
					{
						if (pBoundComp->GetBound(i))
							pBoundComp->GetBound(i)->SetMaterial(PGTAMATERIALMGR->g_idAnimalDefault);
					}
				}
			}

			// set all ped's joint strengths to -1 (infinite) because we can't break apart skinned fragment
			for(int nGroup=0; nGroup < GetFragType()->GetPhysics(currentLOD)->GetNumChildGroups(); nGroup++)
			{
				GetFragType()->GetPhysics(currentLOD)->GetAllGroups()[nGroup]->SetStrength(-1.0f);
			}

			// Set the damaged mass to equal the undamaged mass
			for (s32 i=0; i<GetFragType()->GetPhysics(currentLOD)->GetNumChildren(); i++)
			{
				fragTypeChild* pTypeChild = GetFragType()->GetPhysics(currentLOD)->GetAllChildren()[i];
				pTypeChild->SetDamagedMass(pTypeChild->GetUndamagedMass());
			}

#if __ASSERT
			// Check that all ragdoll LOD's have the same mass
			if (currentLOD == 0)
			{
				highLODRagdollMass = 0.0f;
				for (s32 i=0; i<GetFragType()->GetPhysics(currentLOD)->GetNumChildren(); i++)
				{
					fragTypeChild* pTypeChild = GetFragType()->GetPhysics(currentLOD)->GetAllChildren()[i];
					highLODRagdollMass += pTypeChild->GetUndamagedMass();
				}
			}
			else
			{
				float totalMass = 0.0f;
				for (s32 i=0; i<GetFragType()->GetPhysics(currentLOD)->GetNumChildren(); i++)
				{
					fragTypeChild* pTypeChild = GetFragType()->GetPhysics(currentLOD)->GetAllChildren()[i];
					totalMass += pTypeChild->GetUndamagedMass();
				}
				Assertf(Abs(highLODRagdollMass-totalMass) < 0.15f, "\nHigh ragdoll lod mass is %f, LOD %d ragdoll mass is %f.", highLODRagdollMass, currentLOD, totalMass);
			}

			if(GetFragType()->GetPhysics(currentLOD)->GetNumChildren() == NUM_RAGDOLL_COMPONENTS &&
			   GetFragType()->GetARTAssetID() >= 0)  // this is set at resource time
			{
				ASSERT_ONLY(const CBaseCapsuleInfo* pPedCapsule = CPedCapsuleInfoManager::GetInfo(GetPedCapsuleHash().GetHash());)
				// Check ped has correct ragdoll
				Assertf(pPedCapsule, "%s has no capsule info", GetModelName());
				Assertf(pPedCapsule->GetRagdollType() != RD_CUSTOM, "%s has wrong art asset", GetModelName());
				Assertf(
					(GetFragType()->GetARTAssetID() == NM_ASSET_WILMA && pPedCapsule->GetRagdollType() == RD_FEMALE) ||
					(GetFragType()->GetARTAssetID() == NM_ASSET_WILMA_LARGE && pPedCapsule->GetRagdollType() == RD_FEMALE) ||
					(GetFragType()->GetARTAssetID() == NM_ASSET_FRED && pPedCapsule->GetRagdollType() == RD_MALE) ||
					(GetFragType()->GetARTAssetID() == NM_ASSET_ALIEN && pPedCapsule->GetRagdollType() == RD_MALE) ||
					(GetFragType()->GetARTAssetID() == NM_ASSET_FRED_LARGE && (pPedCapsule->GetRagdollType() == RD_MALE || pPedCapsule->GetRagdollType() == RD_MALE_LARGE)), 
					"%s has wrong art asset", GetModelName());
			}
#endif //__ASSERT	
		}
	}
}

//-------------------------------------------------------------------------
// Builds a list of entry/exit points for peds to use when getting on a ridable ped
//-------------------------------------------------------------------------
void CPedModelInfo::InitLayout( crSkeletonData *pSkelData )
{
	if (m_pSeatInfo) {		
		modelinfoAssert(CVehicleMetadataMgr::GetInstance().GetIsInitialised());

		const CVehicleLayoutInfo* pLayoutInfo = NULL;

		if(m_LayoutHash.GetHash() > 0)
		{
			pLayoutInfo = CVehicleMetadataMgr::GetInstance().GetVehicleLayoutInfo(m_LayoutHash.GetHash());
		}
		pedAssertf(pLayoutInfo,"%s: This ridable ped is missing layout information",GetModelName());
		if(!pLayoutInfo)
		{
			return;
		}
		
		m_pSeatInfo->InitFromLayoutInfo(pLayoutInfo, pSkelData);
	}
}

void CPedModelInfo::SetInvalidRagdollFilter( crFrameFilter *pFrameFilter )
{
	if (m_pInvalidRagdollFilter)
	{
		m_pInvalidRagdollFilter->Release();
	}

	m_pInvalidRagdollFilter = pFrameFilter;

	if (m_pInvalidRagdollFilter)
	{
		m_pInvalidRagdollFilter->AddRef();
	}
}

bool CPedModelInfo::IsUsingFemaleSkeleton() const
{
	if (GetFragType() && (GetFragType()->GetARTAssetID() == NM_ASSET_WILMA || GetFragType()->GetARTAssetID() == NM_ASSET_WILMA_LARGE))
	{
		return true;
	}
	return false;
}

// Name			:	LoadPedPersonalityData
// Purpose		:	Load all pedpersonality data files, including those from the current episodic pack (if any)
// Parameters	:	None
// Returns		:	Nothing
void CPedModelInfo::LoadPedPersonalityData()
{
	// Iterate through the files backwards, so episodic data can override any old data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::PED_PERSONALITY_FILE);
	Assert(pData == DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_PERSONALITY_FILE));
	strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();
	while(DATAFILEMGR.IsValid(pData))
	{
		if(!pData->m_disabled)
		{
			// Read in this particular file
			LoadPedPersonalityFile(pData->m_filename);
		}

		// Get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
		strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();
	}

	PostLoadPedPersonalityData();
}		

void CPedModelInfo::PostLoadPedPersonalityData()
{
	for(s32 i = 0; i < ms_personalities.m_MovementModes.GetCount(); ++i)
	{
		ms_personalities.m_MovementModes[i].PostLoad();
	}

	for(s32 i = 0; i < ms_personalities.m_PedPersonalities.GetCount(); ++i)
	{
		ms_personalities.m_PedPersonalities[i].PostLoad();
	}
}

//
// name:		ReadPedPersonalityFile
// description:	Reads in a single pedpersonality.xml file and assigns the attributes from there to the entries
//				in the PedModelInfo array.
//
void CPedModelInfo::LoadPedPersonalityFile(const char* szFileName)
{
	Printf("loading peds from meta....\n");

	char metadataFilename[RAGE_MAX_PATH];
	formatf(metadataFilename, RAGE_MAX_PATH, "%s.%s", szFileName, META_FILE_EXT);

	psoFile* pPsoFile = psoLoadFile(metadataFilename, PSOLOAD_PREP_FOR_PARSER_LOADING, atFixedBitSet32().Set(psoFile::REQUIRE_CHECKSUM));
	weaponFatalAssertf(pPsoFile, "Failed to load the metadata PSO file (%s)", metadataFilename);

	ASSERT_ONLY(const bool bHasSucceeded = ) psoInitAndLoadObject(*pPsoFile, ms_personalities);
	pedFatalAssertf(bHasSucceeded, "Failed to parse the metadata PSO file (%s)", metadataFilename);

	delete pPsoFile;
}

void CPedModelInfo::PersonalityDataList::AppendToMovementModes(atArray<CPedModelInfo::PersonalityMovementModes>& appendFrom)
{
	atArray<CPedModelInfo::PersonalityMovementModes>& target = CPedModelInfo::ms_personalities.m_MovementModes;
	for(int i=0; i<target.GetCount();i++)
	{
		for(int j=0; j<appendFrom.GetCount();j++)
		{
			if( appendFrom[j].GetNameHash() == target[i].GetNameHash() )
			{
				for(int x = 0; x<CPedModelInfo::PersonalityMovementModes::MM_Max; x++)
				{
					AppendArray(target[i].m_MovementModes[x], appendFrom[j].m_MovementModes[x]);
				}
			}
		}
	}
}
void CPedModelInfo::PersonalityDataList::AppendToUnholsterClips(atArray<CPedModelInfo::PersonalityMovementModes::sUnholsterClipData>& appendFrom)
{
	atArray<CPedModelInfo::PersonalityMovementModes::sUnholsterClipData>& target = CPedModelInfo::ms_personalities.m_MovementModeUnholsterData;
	for(int i=0; i<target.GetCount();i++)
	{
		for(int j=0; j<appendFrom.GetCount();j++)
		{
			if( appendFrom[j].GetNameHash() == target[i].GetNameHash() )
			{
				for(int fromIdx=0; fromIdx<appendFrom[i].m_UnholsterClips.GetCount();fromIdx++)
				{	
					bool isFound = false;
					for(int idx=0; idx < target[j].m_UnholsterClips.GetCount(); idx++)
					{
						if(target[i].m_UnholsterClips[idx].m_Clip == appendFrom[j].m_UnholsterClips[fromIdx].m_Clip)
						{
							AppendArray(target[i].m_UnholsterClips[idx].m_Weapons, appendFrom[j].m_UnholsterClips[fromIdx].m_Weapons);
							isFound = true;
						}
					}
					if(!isFound)
					{						
						target[i].m_UnholsterClips.PushAndGrow(appendFrom[j].m_UnholsterClips[fromIdx]);
					}
				}
			}
		}
	}
}

void CPedModelInfo::AppendPedPersonalityFile(const char* szFileName)
{
	Printf("appending peds pesonality from %s....\n", szFileName);
	PersonalityDataList tempList;
	PARSER.LoadObject(szFileName, NULL, tempList);
	ms_personalities.AppendToMovementModes(tempList.m_MovementModes);
	ms_personalities.AppendToUnholsterClips(tempList.m_MovementModeUnholsterData);
	Displayf("MovementModes count				: %d", tempList.m_MovementModes.GetCount());
	Displayf("CriminalityTypes count			: %d", tempList.m_CriminalityTypes.GetCount());
	Displayf("BraveryTypes count				: %d", tempList.m_BraveryTypes.GetCount());
	Displayf("MovementModeUnholsterData count	: %d", tempList.m_MovementModeUnholsterData.GetCount());
	Displayf("PedPersonalities count				: %d", tempList.m_PedPersonalities.GetCount());


	AppendUniques(ms_personalities.m_MovementModes, tempList.m_MovementModes);
	AppendUniques(ms_personalities.m_CriminalityTypes, tempList.m_CriminalityTypes);
	AppendUniques(ms_personalities.m_BraveryTypes, tempList.m_BraveryTypes);
	AppendUniques(ms_personalities.m_PedPersonalities, tempList.m_PedPersonalities);
	AppendUniques(ms_personalities.m_MovementModeUnholsterData, tempList.m_MovementModeUnholsterData);
}

bool CPedModelInfo::LoadPedMetaFile(const char* szFileName, bool permanent, s32 mapTypeDefIndex)
{
	bool success = false;

	struct CPedModelInfo::InitDataList initDataList;
	const char *ext = (strrchr(szFileName, '.') != NULL) ? NULL : META_FILE_EXT;
	Verifyf(fwPsoStoreLoader::LoadDataIntoObject(szFileName, ext, initDataList), "Failed to load %s", szFileName);

	// load resident txd
    if (CPedModelInfo::GetCommonTxd() == -1)
	{
		if (initDataList.m_residentTxd.GetLength() > 0)
		{
			strLocalIndex txtSlot = g_TxdStore.FindSlot(initDataList.m_residentTxd.c_str());
			if (txtSlot == -1)
			{
				txtSlot = g_TxdStore.AddSlot(initDataList.m_residentTxd.c_str());
			}
			CPedModelInfo::SetCommonTxd(txtSlot.Get());
			CPedModelInfo::AddResidentObject(txtSlot, g_TxdStore.GetStreamingModuleId());
		}
	}
	// add slots for resident anims
	for(int i=0; i<initDataList.m_residentAnims.GetCount(); i++)
	{
		strLocalIndex slot = g_ClipDictionaryStore.FindSlot(initDataList.m_residentAnims[i].c_str());
		if (slot == -1)
		{
			slot = g_ClipDictionaryStore.AddSlot(initDataList.m_residentAnims[i].c_str());
		}
		CPedModelInfo::AddResidentObject(slot, g_ClipDictionaryStore.GetStreamingModuleId());
	}


	// load model infos
	for(int i=0;i<initDataList.m_InitDatas.GetCount();i++)
	{
		CPedModelInfo * pModelInfo = CModelInfo::AddPedModel(initDataList.m_InitDatas[i].m_Name.c_str(), permanent, mapTypeDefIndex);

		if (pModelInfo)
		{
			pModelInfo->Init(initDataList.m_InitDatas[i]);

			if (!permanent)
			{
				CModelInfo::PostLoad(pModelInfo);
			}
		}
	}

	// load txd relationships
	for (s32 i = 0; i < initDataList.m_txdRelationships.GetCount(); ++i)
	{
		{
			strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlot(initDataList.m_txdRelationships[i].m_child.c_str()));

			if(txdSlot == -1)
			{
				txdSlot = g_TxdStore.AddSlot(initDataList.m_txdRelationships[i].m_child.c_str());
			}

			strLocalIndex txdParentSlot = strLocalIndex(g_TxdStore.FindSlot(initDataList.m_txdRelationships[i].m_parent.c_str()));

			if(txdParentSlot == -1)
			{
				txdParentSlot = g_TxdStore.AddSlot(initDataList.m_txdRelationships[i].m_parent.c_str());
			}

			g_TxdStore.SetParentTxdSlot(txdSlot,txdParentSlot);
		}
	}

	// load multi txd relationships
	for (s32 i = 0; i < initDataList.m_multiTxdRelationships.GetCount(); ++i)
	{
		strLocalIndex txdParentSlot = strLocalIndex(g_TxdStore.FindSlot(initDataList.m_multiTxdRelationships[i].m_parent.c_str()));
		if(txdParentSlot == -1)
		{
			txdParentSlot = g_TxdStore.AddSlot(initDataList.m_multiTxdRelationships[i].m_parent.c_str());
		}

		for (s32 f = 0; f < initDataList.m_multiTxdRelationships[i].m_children.GetCount(); ++f)
		{
			strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlot(initDataList.m_multiTxdRelationships[i].m_children[f].c_str()));
			if(txdSlot == -1)
			{
				txdSlot = g_TxdStore.AddSlot(initDataList.m_multiTxdRelationships[i].m_children[f].c_str());
			}

			g_TxdStore.SetParentTxdSlot(txdSlot, txdParentSlot);
		}
	}

	success = true;

	CModelInfo::CopyOutCurrentMPPedMapping();

	return success;
}

// Name			:	LoadPedPerceptionData
// Purpose		:	Load all pedperception data files, including those from the current episodic pack (if any)
// Parameters	:	None
// Returns		:	Nothing
void CPedModelInfo::LoadPedPerceptionData()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::PED_PERCEPTION_FILE);
	Assert(pData == DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_PERCEPTION_FILE));
	while(DATAFILEMGR.IsValid(pData))
	{
		// Read in this particular file
		CPedPerceptionInfoManager::Init(pData->m_filename);

		// Move to the next file
		pData = DATAFILEMGR.GetNextFile(pData);
	}
}


//
// name:		AssignPersonalityData
// description:	Assign an array of attributes to the entries in the PedModelInfo array.
//
//void CPedModelInfo::AssignPersonalityData(const CPedModelInfo::PersonalityDataList * UNUSED_PARAM(pPersonalityData))
//{
//	//const int numPersonalities = pPersonalityData->m_PedPersonalities.GetCount();
//	//for(int i=0;i<numPersonalities;i++)
//	//{
//	//	const ConstString & modelName = pPersonalityData->m_PedPersonalities[i].m_Name;
//
//	//	// Try to find the pedmodelinfo with this name.
//	//	u32 ModelIndex = CModelInfo::GetModelIdFromName(modelName.c_str()).GetModelIndex();
//	//	if (!CModelInfo::IsValidModelInfo(ModelIndex))
//	//	{
//	//		modelinfoDisplayf("%s is in pedpersonality.dat but not actually in the game\n", modelName.c_str());
//	//		continue;
//	//	}
//
//	//	// Make sure the model is for a ped.
//	//	if ( GetBaseModelInfo()->GetModelType() != MI_TYPE_PED )
//	//	{
//	//		modelinfoDisplayf("CPedModelInfo::ReadPedPersonalityFile - %s has an entry in pedpersonality.dat but isn't a ped - maybe missing from peds.ide\n", modelName.c_str());
//	//		continue;
//	//	}
//
//	//	// Get the model info from the model index.
//	//	CPedModelInfo *pModelInfo = (CPedModelInfo*)CModelInfo::GetModelInfo (ModelIndex);
//	//	if (!pModelInfo)
//	//	{
//	//		continue;
//	//	}
//
//	//	// Mark that the model type was found in the ped personality file.  Casting away const correctness (treating this flag as mutable)
//	//	const_cast<CPedModelInfo::PersonalityData&>(pPersonalityData->m_PedPersonalities[i].m_PersonalityData).SetFlag(TB_HAS_BEEN_FOUND_IN_PERSONALITY_FILE);
//
//	//	pModelInfo->SetPersonalitySettings( pPersonalityData->m_PedPersonalities[i].m_PersonalityData );
//	//}
//}

//
//#if __DEV
////
//// name:		MakePersonalityData
//// description:	Create an PedModelInfo array based on the current model infos.
//// notes:		Uses atArray<>::Grow to create the array, code is for DEV only
////
//CPedModelInfo::PersonalityDataList * CPedModelInfo::MakePersonalityData()
//{
//	CPedModelInfo::PersonalityDataList * pData = rage_new CPedModelInfo::PersonalityDataList();
//	fwArchetypeFactory<CPedModelInfo>& modelInfoStore = CModelInfo::GetPedModelInfoStore();
//	const s32 numItems = modelInfoStore.GetItemsUsed();
//
//	for(s32 i=0; i<numItems; i++) 
//	{
//		const CPedModelInfo & modelInfo = modelInfoStore[i];
//		if (modelInfo.m_PersonalitySettings.GetFlag(TB_HAS_BEEN_FOUND_IN_PERSONALITY_FILE))
//		{
//			PersonalityDataWrapped & personalityDataWrapper = pData->m_PedPersonalities.Grow(64);
//			personalityDataWrapper.m_Name = modelInfo.GetModelName();
//			personalityDataWrapper.m_PersonalityData = modelInfo.m_PersonalitySettings;
//		}
//	}
//	return pData;
//}
//#endif // __DEV


void CPedModelInfo::ClearCountedAsScenarioPed()
{
	SetCountedAsScenarioPed(false);
}


void CPedModelInfo::AddAudioGroupMap( s32 iHeadComponent, u32 iAudioHash )
{
	if( m_iAudioGroupCount >= MAX_AUDIO_GROUP_MAPS )
	{
		modelinfoAssertf(0, "AddAudioGroupMap out of audio groups");
		return;
	}

	m_audioGroupMaps[m_iAudioGroupCount].m_audioGroupHash = iAudioHash;
	m_audioGroupMaps[m_iAudioGroupCount].m_iHeadComponent = iHeadComponent;
	++m_iAudioGroupCount;
}

bool CPedModelInfo::GetAudioGroupForHeadComponent( s32 iHeadComponent, u32& iOutAudioHash )
{
	for( s32 i = 0; i < m_iAudioGroupCount; i++ )
	{
		if( m_audioGroupMaps[i].m_iHeadComponent == iHeadComponent )
		{
			iOutAudioHash = m_audioGroupMaps[i].m_audioGroupHash;
			return true;
		}
	}
	return false;
}

/*
void CPedModelInfo::SetRagdollPhysics(phArchetype *pArch, s32 dictIndex)
{
	m_pRagdollArch = pArch;
	if(m_pRagdollArch != NULL)
	{
		m_physicsDictionaryIndexRagdoll = dictIndex;
	}
	else
		m_physicsDictionaryIndexRagdoll = -1;
}*/

static dev_float PED_BUOYANCY_MULT = 1.3f;
static dev_float PED_DRAG_MULT_XY = 0.5f;
static dev_float PED_DRAG_MULT_ZUP = 0.5f;
static dev_float PED_DRAG_MULT_ZDOWN = 0.5f;
static dev_float PED_SAMPLE_OFFSET_Z = 0.25f;
static dev_float PED_SAMPLE_SIZE = PED_HUMAN_HEAD_HEIGHT / 2.0f;
static dev_float QUADRUPED_SAMPLE_SIZE = 0.3f;
static dev_float QUADRUPED_FX_SAMPLE_SIZE = 0.5f;
static dev_float PED_FX_SAMPLE_OFFSET_Z = -0.15f;
static dev_float PED_FX_SAMPLE_SIZE = 0.9f;
static dev_float RAGDOLL_SAMPLE_SIZE = 0.1f;

//static float aRagdollModifiers[PED_WATER_SAMPLES_RAGDOLL] =
//{
//	1.02f,	// 0:  Buttocks
//	1.05f,	// 1:  Thigh_left
//	1.05f,	// 2:  Shin_left
//	0.9f,	// 3:  Foot_left
//	1.05f,	// 4:  Thigh_right
//	1.05f,	// 5:  Shin_right
//	0.9f,	// 6:  Foot_right
//	1.05f,	// 7:  Spine0
//	1.05f,	// 8:  Spine1
//	1.05f,	// 9:  Spine2
//	1.05f,	// 10: Spine3
//	1.05f,	// 11: Clavicle_Left
//	1.0f,	// 12: Upper_Arm_Left
//	1.0f,	// 13: Lower_Arm_Left
//	0.95f,	// 14: Hand_Left
//	1.05f,	// 15: Clavicle_Right
//	1.0f,	// 16: Upper_Arm_Right
//	1.0f,	// 17: Lower_Arm_Right
//	0.95f,	// 18: Hand_Right
//	1.0f,	// 19: Neck
//	1.0f	// 20: Head
//};

void CPedModelInfo::InitWaterSamples()
{
#if __DEV
	if(CBaseModelInfo::ms_bPrintWaterSampleEvents)
	{
		modelinfoDisplayf("[Buoyancy] Creating water samples for ped %s",GetModelName());
	}
#endif

	// Check we haven't done this before
	modelinfoAssertf(m_pBuoyancyInfo == NULL,"InitWaterSamples() called when water samples already exist");
	if(m_pBuoyancyInfo != NULL)
	{
		return;
	}

	// Create buoyancy info
	m_pBuoyancyInfo = rage_new CBuoyancyInfo();
	modelinfoAssert(m_pBuoyancyInfo);
	modelinfoAssert(GetFragType());
	const CBaseCapsuleInfo* pCapsuleInfo = CPedCapsuleInfoManager::GetInfo(GetPedCapsuleHash().GetHash());
	SetupPedBuoyancyInfo(*m_pBuoyancyInfo, pCapsuleInfo, *GetFragType(), false);
}

void CPedModelInfo::SetupPedBuoyancyInfo(CBuoyancyInfo &BuoyancyInfo, const CBaseCapsuleInfo* pCapsuleInfo, const fragType &FragType, bool bIsWeightless)
{
	int numChildren = FragType.GetPhysics(0)->GetNumChildren();
	int totalWaterSamples = PED_WATER_SAMPLES_STD + numChildren;

	modelinfoAssert(BuoyancyInfo.m_WaterSamples == NULL);
	if(BuoyancyInfo.m_WaterSamples == NULL)
	{
		BuoyancyInfo.m_WaterSamples = rage_new CWaterSample[totalWaterSamples];
#if __DEV
		ms_nNumWaterSamplesInMemory += totalWaterSamples;
		PF_SET(NumWaterSamplesInMemory, ms_nNumWaterSamplesInMemory);
#endif // __DEV
		pedAssertf(BuoyancyInfo.m_WaterSamples,"Failed to create ped water samples");
	}
	

	// ---animated samples---

	// Sample 0 is used for the FX. Forces are not applied.
	const float fPhysicsSampleSize = pCapsuleInfo->IsQuadruped() ? QUADRUPED_SAMPLE_SIZE : PED_SAMPLE_SIZE;
	const float fFXSampleSize = pCapsuleInfo->IsQuadruped() ? QUADRUPED_FX_SAMPLE_SIZE : PED_FX_SAMPLE_SIZE;
	BuoyancyInfo.m_WaterSamples[0].m_vSampleOffset.Set(0.0f, PED_FX_SAMPLE_OFFSET_Z, 0.0f);
	BuoyancyInfo.m_WaterSamples[0].m_fSize = fFXSampleSize;
	BuoyancyInfo.m_WaterSamples[0].m_fBuoyancyMult = 0.0f / fPhysicsSampleSize;

	// Sample 1 is used for actual physics. FX are disabled.
	BuoyancyInfo.m_WaterSamples[1].m_vSampleOffset.Set(0.0f,0.0f,pCapsuleInfo->IsBird() ? 0.0f : PED_SAMPLE_OFFSET_Z);
	BuoyancyInfo.m_WaterSamples[1].m_fSize = fPhysicsSampleSize;
	float fAnimatedBuoyancyMult = bIsWeightless ? 1.0f : PED_BUOYANCY_MULT;
	BuoyancyInfo.m_WaterSamples[1].m_fBuoyancyMult = fAnimatedBuoyancyMult / fPhysicsSampleSize;
	// The animated capsule's buoyancy sample offset is wrong for quadrupeds.
	modelinfoAssert(pCapsuleInfo);
	if(pCapsuleInfo->IsQuadruped())
	{
		BuoyancyInfo.m_WaterSamples[1].m_vSampleOffset.Set(0.0f,0.0f,0.0f);
	}

	// ---ragdoll samples---

	int sampleIndex = WS_PED_RAGDOLL_SAMPLE_START;
	for (int childIndex = 0; childIndex < numChildren; ++childIndex)
	{
		float fSize = 0.0f;
		fSize = RAGDOLL_SAMPLE_SIZE;

		// Buoyancy mult as fraction of average part mass (fTotalRagdollMass / numChildren)
		float fBuoyancyMult = 1.0f;

		modelinfoAssert(sampleIndex < totalWaterSamples);
		BuoyancyInfo.m_WaterSamples[sampleIndex].m_fSize = fSize;
		BuoyancyInfo.m_WaterSamples[sampleIndex].m_fBuoyancyMult = fBuoyancyMult;
		BuoyancyInfo.m_WaterSamples[sampleIndex].m_nComponent = static_cast<u16>(childIndex);

		sampleIndex++;
	}

	BuoyancyInfo.m_fBuoyancyConstant = -GRAVITY;
	BuoyancyInfo.m_fDragMultXY =  PED_DRAG_MULT_XY * -GRAVITY / fPhysicsSampleSize;
	BuoyancyInfo.m_fDragMultZUp =  PED_DRAG_MULT_ZUP * -GRAVITY / fPhysicsSampleSize;
	BuoyancyInfo.m_fDragMultZDown =  PED_DRAG_MULT_ZDOWN * -GRAVITY / fPhysicsSampleSize;

	// might want to use rudder force to provide directional control for swimming peds
	BuoyancyInfo.m_fKeelMult = 0.0f;
	BuoyancyInfo.m_nNumWaterSamples = (s16)totalWaterSamples;
}

#if __DEV
void CPedModelInfo::InitPedBuoyancyWidget()
{
	//bkBank* physBank = BANKMGR.FindBank("Physics");
	//physBank->PushGroup("Ped Buoyancy Samples",false);
	//for(int i = 0; i < PED_WATER_SAMPLES_RAGDOLL; i++)
	//{
	//	char sSliderName[20];
	//	sprintf(sSliderName,"Sample no. %i",i);
	//	physBank->AddSlider(sSliderName,&aRagdollModifiers[i],0.0f,2.0f,0.01f);
	//}
	//physBank->PopGroup();
}
#endif

phBoundComposite* CPedModelInfo::GeneratePedBound()
{
	// create a bound consisting of a capsule
	// (needs to be a composite because capsules are orientated around local y-axis)	
	const CBaseCapsuleInfo* pCapsuleInfo = CPedCapsuleInfoManager::GetInfo(GetPedCapsuleHash().GetHash());
	if (Verifyf(pCapsuleInfo, "Failed to find capsule type %s for ped %s", GetPedCapsuleHash().GetCStr(), GetModelName()))
	{
		return pCapsuleInfo->GeneratePedBound(*this);
	}
	return 0;
}

float CPedModelInfo::GetQuadrupedProbeDepth() const
{
	return m_ProbeDepth;
}


const Vector3& CPedModelInfo::GetQuadrupedProbeTopPosition(int iCapsule) const
{
	modelinfoAssert(iCapsule == 1 || iCapsule == 2);
	return m_ProbeTopPosition[iCapsule-1];
}

void CPedModelInfo::SetQuadrupedProbeDepth(float fDepth)
{
	m_ProbeDepth = fDepth;
}


void CPedModelInfo::SetQuadrupedProbeTopPosition(int iCapsule, const Vector3 &rPos)
{
	modelinfoAssert(iCapsule == 1 || iCapsule == 2);
	m_ProbeTopPosition[iCapsule-1] = rPos;
}

#if __BANK
void CPedModelInfo::PedDataChangedCB()
{
	// Go through all loaded ped model infos and resize their bounds
	// Then resize all peds boudns

	fwArchetypeDynamicFactory<CPedModelInfo>& gPedModelStore = CModelInfo::GetPedModelInfoStore();
	
	gPedModelStore.ForAllItemsUsed(&CPedModelInfo::RefreshPedBoundsCB);

	CPed::Pool* pPedPool= CPed::GetPool();
	CPed* pPed;
	s32 i=pPedPool->GetSize();
	while(i--)
	{
		pPed = pPedPool->GetSlot(i);

		if(pPed)
		{
			const float fOldRadius = pPed->GetAnimatedInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();

			CPedModelInfo* pModelInfo = pPed->GetPedModelInfo();
			Assert(pModelInfo);
			pModelInfo->RefreshPedBound(pPed->GetAnimatedInst()->GetArchetype()->GetBound());

			const float fNewRadius = pPed->GetAnimatedInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();
			if(fOldRadius != fNewRadius && CPhysics::GetLevel()->IsInLevel(pPed->GetAnimatedInst()->GetLevelIndex()))
			{
				CPhysics::GetLevel()->UpdateObjectLocationAndRadius(pPed->GetAnimatedInst()->GetLevelIndex(),(Mat34V_Ptr)(NULL));
			}
		}
	}
}

void CPedModelInfo::RefreshPedBoundsCB()
{
	if(HasPhysics() && GetPhysics())
	{
		RefreshPedBound(GetPhysics()->GetBound());
	}
}

void CPedModelInfo::RefreshPedBound(phBound* pBound)
{
	phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>(pBound);
	modelinfoAssert(pCompositeBound->GetType() == phBound::COMPOSITE);

	const CBaseCapsuleInfo* pCapsuleInfo = CPedCapsuleInfoManager::GetInfo(GetPedCapsuleHash().GetHash());
	modelinfoAssert(pCapsuleInfo);
	if (pCapsuleInfo)
	{
		pCapsuleInfo->GeneratePedBound(*this, pCompositeBound);
	}

	pCompositeBound->CalculateCompositeExtents();
	pCompositeBound->UpdateBvh(true);
}

#endif

//
// name:		SetMotionTaskDataSetHash
// description: Sets the motion task data set hash used by this model
void CPedModelInfo::SetMotionTaskDataSetHash(const atHashString& motionTaskDataSet)
{
	if(motionTaskDataSet.GetHash() == 0 || (motionTaskDataSet.GetHash() == s_HashOfNullString))
	{
		m_motionTaskDataSetHash = atHashString::Null();
		return;
	}
	m_motionTaskDataSetHash = motionTaskDataSet;
}

//
// name:		SetDefaultTaskDataSetHash
// description: Sets the default task data set hash used by this model
void CPedModelInfo::SetDefaultTaskDataSetHash(const atHashString& defaultTaskDataSet)
{
	if(defaultTaskDataSet.GetHash() == 0 || (defaultTaskDataSet.GetHash() == s_HashOfNullString))
	{
		m_defaultTaskDataSetHash = atHashString::Null();
		return;
	}
	m_defaultTaskDataSetHash = defaultTaskDataSet;
}

//
// name:		SetPedCapsuleHash
// description:	Set the ped capsule hash name used by this model
void CPedModelInfo::SetPedCapsuleHash(const atHashString& pedCapsule)
{
	if(pedCapsule.GetHash() == 0 || (pedCapsule.GetHash() == s_HashOfNullString))
	{
		m_pedCapsuleHash = atHashString::Null();
		return;
	}
	m_pedCapsuleHash = pedCapsule;
}

//
// name:		SetLayoutHash
// description:	Set the ped layout hash name used by this model
void CPedModelInfo::SetLayoutHash(const atHashString& pedLayoutName)
{
	if(pedLayoutName.GetHash() == 0 || (pedLayoutName.GetHash() == s_HashOfNullString))
	{
		m_LayoutHash = atHashString::Null();
		return;
	}
	m_LayoutHash = pedLayoutName;
}

//
// name:		GetLayoutInfo
// description:	Get the Layout info for this model if applicable
const CVehicleLayoutInfo* CPedModelInfo::GetLayoutInfo() const
{
	if (m_pSeatInfo) 
	{		
		if(m_LayoutHash.GetHash() > 0)
		{
			return CVehicleMetadataMgr::GetInstance().GetVehicleLayoutInfo(m_LayoutHash.GetHash());
		}

	}

	return NULL;
}

void CPedModelInfo::SetPedComponentClothHash(const atHashString& pedComponentClothName)
{
	if(pedComponentClothName.GetHash() == 0 || (pedComponentClothName.GetHash() == s_HashOfNullString))
	{
		m_pedComponentClothHash = atHashString::Null();
		return;
	}
	m_pedComponentClothHash = pedComponentClothName;
}

//
// name:		SetPedComponentSetHash
// description:	Set the ped component set hash name used by this model
void CPedModelInfo::SetPedComponentSetHash(const atHashString& pedComponentSetName)
{
	if(pedComponentSetName.GetHash() == 0 || (pedComponentSetName.GetHash() == s_HashOfNullString))
	{
		m_pedComponentSetHash = atHashString::Null();
		return;
	}
	m_pedComponentSetHash = pedComponentSetName;
}


//
// name:		SetPedIKSettingsHash
// description:	Set the ped IK settings hash name used by this model
void CPedModelInfo::SetPedIKSettingsHash(const atHashString& pedIKSettingsName)
{
	if(pedIKSettingsName.GetHash() == 0 || (pedIKSettingsName.GetHash() == s_HashOfNullString))
	{
		m_pedIKSettingsHash = atHashString::Null();
		return;
	}
	m_pedIKSettingsHash = pedIKSettingsName;
}


//
// name:		SetTaskFlagsHash
// description:	Set the ped IK settings set hash name used by this model
void CPedModelInfo::SetTaskDataHash(const atHashString& taskDataName)
{
	if(taskDataName.GetHash() == 0 || (taskDataName.GetHash() == s_HashOfNullString))
	{
		m_TaskDataHash = atHashString::Null();
		return;
	}
	m_TaskDataHash = taskDataName;
}


//
// name:		SetDecisionMakerHash
// description:	Set the decision maker hash name used by this model
void CPedModelInfo::SetDecisionMakerHash(const atHashString& decisionMakerName)
{
	if(decisionMakerName.GetHash() == 0 || (decisionMakerName.GetHash() == s_HashOfNullString))
	{
		m_decisionMakerHash = atHashString::Null();
		return;
	}
	m_decisionMakerHash = decisionMakerName;
}


//
// name:		SetRelationshipGroupHash
// description:	Set the decision maker hash name used by this model
void CPedModelInfo::SetRelationshipGroupHash(const atHashString& relationshipGroupName)
{
	if(relationshipGroupName.GetHash() == 0 || (relationshipGroupName.GetHash() == s_HashOfNullString))
	{
		m_relationshipGroupHash = atHashString::Null();
		return;
	}
	m_relationshipGroupHash = relationshipGroupName;
}

void CPedModelInfo::SetNavCapabilitiesHash(const atHashString& navCapabilitiesName)
{
	if (navCapabilitiesName.GetHash() == 0 || (navCapabilitiesName.GetHash() == s_HashOfNullString))
	{
		m_navCapabilitiesHash = atHashString::Null();
		return;
	}
	m_navCapabilitiesHash = navCapabilitiesName;
}

void CPedModelInfo::SetPerceptionHash(const atHashString& perceptionHashString)
{
	if (perceptionHashString.GetHash() == 0 || (perceptionHashString.GetHash() == s_HashOfNullString))
	{
		m_perceptionHash = atHashString::Null();
		return;
	}
	m_perceptionHash = perceptionHashString;
}

void CPedModelInfo::SetDefaultBrawlingStyleHash(const atHashString& brawlingStyle)
{ 
	if (brawlingStyle.GetHash() == 0 || (brawlingStyle.GetHash() == s_HashOfNullString))
	{
		m_DefaultBrawlingStyle = atHashString::Null();
		return;
	}
	m_DefaultBrawlingStyle = brawlingStyle;
}

void CPedModelInfo::SetDefaultWeaponHash(const atHashString& weaponType)
{
	if (weaponType.GetHash() == 0 || (weaponType.GetHash() == s_HashOfNullString))
	{
		m_DefaultUnarmedWeapon = atHashString::Null();
		return;
	}
	m_DefaultUnarmedWeapon = weaponType;
}

const CPedModelInfo::PersonalityBravery* CPedModelInfo::GetPersonalityBravery(u32 hash)
{
	for (s32 i = 0; i < ms_personalities.m_BraveryTypes.GetCount(); ++i)
	{
		if (ms_personalities.m_BraveryTypes[i].m_Name.GetHash() == hash)
			return &ms_personalities.m_BraveryTypes[i];
	}

	return NULL;	
}

CPedModelInfo::PersonalityCriminality* CPedModelInfo::GetPersonalityCriminality(u32 hash)
{
    for (s32 i = 0; i < ms_personalities.m_CriminalityTypes.GetCount(); ++i)
    {
        if (ms_personalities.m_CriminalityTypes[i].m_Name.GetHash() == hash)
            return &ms_personalities.m_CriminalityTypes[i];
    }

    return NULL;	
}

const CPedModelInfo::PersonalityMovementModes* CPedModelInfo::FindPersonalityMovementModes(u32 hash)
{
	for (s32 i = 0; i < ms_personalities.m_MovementModes.GetCount(); ++i)
	{
		if (ms_personalities.m_MovementModes[i].GetNameHash() == hash)
			return &ms_personalities.m_MovementModes[i];
	}

	return NULL;	
}

const CPedModelInfo::PersonalityMovementModes::sUnholsterClipData* CPedModelInfo::FindPersonalityMovementModeUnholsterData(u32 hash)
{
	for (s32 i = 0; i < ms_personalities.m_MovementModeUnholsterData.GetCount(); ++i)
	{
		if (ms_personalities.m_MovementModeUnholsterData[i].m_Name.GetHash() == hash)
			return &ms_personalities.m_MovementModeUnholsterData[i];
	}

	return NULL;	
}

#if __BANK
void CPedModelInfo::SaveCallback()
{
	const char* sPedPersonalityMetadataAssetsSubPath = "export/data/metadata/pedpersonality.pso.meta";
	char filename[RAGE_MAX_PATH];
	formatf(filename, RAGE_MAX_PATH, "%s%s", CFileMgr::GetAssetsFolder(), sPedPersonalityMetadataAssetsSubPath);
	Verifyf(PARSER.SaveObject(filename, "", &ms_personalities, parManager::XML), "Failed to save personality info file:%s", filename);
}

void CPedModelInfo::AddNewPersonalityCallback()
{
	ms_personalities.m_PedPersonalities.Grow();
	RefreshWidgets();
}

void CPedModelInfo::AddNewBraveryCallback()
{
	ms_personalities.m_BraveryTypes.Grow();
	RefreshWidgets();
}

void CPedModelInfo::AddNewCriminalityCallback()
{
    ms_personalities.m_CriminalityTypes.Grow();
    RefreshWidgets();
}

void CPedModelInfo::RefreshWidgets()
{
	// delete the old widgets
	bkBank* rootBank = BANKMGR.FindBank("Peds");
	if (rootBank && ms_bankGroup)
		rootBank->DeleteGroup(*ms_bankGroup);
	ms_bankGroup = NULL;

	AddWidgets(*rootBank);
}

void CPedModelInfo::RemovePersonalityCallback(CallbackData obj, CallbackData UNUSED_PARAM(clientData))
{
	s32 idx = (s32)(size_t)obj;
	ms_personalities.m_PedPersonalities.Delete(idx);
	RefreshWidgets();
}

void CPedModelInfo::RemoveBraveryCallback(CallbackData obj, CallbackData UNUSED_PARAM(clientData))
{
	s32 idx = (s32)(size_t)obj;
	ms_personalities.m_BraveryTypes.Delete(idx);
	RefreshWidgets();
}

void CPedModelInfo::RemoveCriminalityCallback(CallbackData obj, CallbackData UNUSED_PARAM(clientData))
{
    s32 idx = (s32)(size_t)obj;
    ms_personalities.m_CriminalityTypes.Delete(idx);
    RefreshWidgets();
}


void CPedModelInfo::AddWidgets(bkBank& bank)
{
	ms_bankGroup = bank.AddGroup("Ped Personalities", true);
	bank.SetCurrentGroup(*ms_bankGroup);

	bank.AddButton("Save", SaveCallback);

    bank.AddButton("Add new Criminality", AddNewCriminalityCallback);
    s32 criminalityCount = ms_personalities.m_CriminalityTypes.GetCount();
    if (criminalityCount > 0)
    {
        bank.PushGroup("Criminality types");
        for (s32 i = 0; i < criminalityCount; ++i)
        {
            bank.PushGroup(ms_personalities.m_CriminalityTypes[i].m_Name.GetCStr());
            PARSER.AddWidgets(bank, &ms_personalities.m_CriminalityTypes[i]);
            bank.AddButton("Remove", datCallback(CFA2(CPedModelInfo::RemoveCriminalityCallback), CallbackData((size_t)i)));
            bank.PopGroup();
        }
        bank.PopGroup();
    }

	bank.AddButton("Add new bravery", AddNewBraveryCallback);
	s32 braveryCount = ms_personalities.m_BraveryTypes.GetCount();
	if (braveryCount > 0)
	{
		bank.PushGroup("Bravery types");
		for (s32 i = 0; i < braveryCount; ++i)
		{
			bank.PushGroup(ms_personalities.m_BraveryTypes[i].m_Name.GetCStr());
			PARSER.AddWidgets(bank, &ms_personalities.m_BraveryTypes[i]);
			bank.AddButton("Remove", datCallback(CFA2(CPedModelInfo::RemoveBraveryCallback), CallbackData((size_t)i)));
			bank.PopGroup();
		}
		bank.PopGroup();
	}

	bank.AddButton("Add new personality", AddNewPersonalityCallback);
	s32 personalityCount = ms_personalities.m_PedPersonalities.GetCount();
	if (personalityCount > 0)
	{
		bank.PushGroup("Personalities");
		for (s32 i = 0; i < personalityCount; ++i)
		{
			bank.PushGroup(ms_personalities.m_PedPersonalities[i].GetPersonalityName());
			PARSER.AddWidgets(bank, &ms_personalities.m_PedPersonalities[i]);
			bank.AddButton("Remove", datCallback(CFA2(CPedModelInfo::RemovePersonalityCallback), CallbackData((size_t)i)));
			bank.PopGroup();
		}
		bank.PopGroup();
	}

	bank.UnSetCurrentGroup(*ms_bankGroup);

	bank.PushGroup("Instance data");
	bank.AddButton("Dump instance data", DumpPedInstanceDataCB);
	bank.AddButton("Dump average pack size", CModelInfo::DumpAveragePedSize);
	bank.PopGroup();
}
#endif // __BANK

// ---- HD streaming stuff ----
void CPedModelInfo::SetupHDFiles(const char* pName)
{
	if (pName == NULL || strcmp(pName, "NULL") == 0 || strcmp(pName, "null") == 0 || GetIsStreamedGfx())
	{
		return;
	}

	if (!(GetHDDist() > 0.f))
	{
		return;
	}

	char HDName[255];
	u32 size = ustrlen(pName);

	if (size > 250)
	{
		return;
	}

	strncpy(HDName, pName, size);

	HDName[size] = '+';
	HDName[size+1] = 'h';
	HDName[size+2] = 'i';
	HDName[size+3] = '\0';

	m_HDtxdIdx = g_TxdStore.FindSlot(HDName).Get();

	s32 parentTdxIndex = GetAssetParentTxdIndex();
	if (parentTdxIndex == -1)
	{
		Warningf("Ped '%s' parent texture dictionary not found! Disabling this type for HD.", GetModelName());
		SetHDDist(-1.f);
	}
	else if (m_HDtxdIdx == -1) 
	{
		Warningf("Ped HD assets for '%s' not found! Disabling this type for HD.", HDName);
        SetHDDist(-1.f);
	}
	else
	{
		CTexLod::StoreHDMapping(STORE_ASSET_TXD, parentTdxIndex, m_HDtxdIdx);
		SetIsHDTxdCapable(true);
	}
}

void CPedModelInfo::ConfirmHDFiles()
{
	if (GetHasHDFiles())
	{
		// verify existence of HD files
		if (GetIsStreamedGfx() || g_TxdStore.IsObjectInImage(strLocalIndex(m_HDtxdIdx)))
		{
			return;
		}

		SetHDDist(-1.0f);
	}
}

bool CPedModelInfo::GetAreHDFilesLoaded() const
{
	if (!GetHasHDFiles())
		return false;

	strLocalIndex hdTxd = strLocalIndex(GetHDTxdIndex());
	if (hdTxd == -1)
		return false;

	return g_TxdStore.GetIsBoundHD(hdTxd);
}

bool CPedModelInfo::HasHelmetProp() const
{
    if (!GetVarInfo())
        return false;
	return GetVarInfo()->HasHelmetProp();
}

void CPedModelInfo::AddPedInstance(CPed* ped)
{
	if (!ped)
		return;

	s32 poolIndex = CPed::GetPool()->GetJustIndex(ped);
	if (Verifyf(poolIndex > -1 && poolIndex < 256, "Bad ped pool index, do we have too many peds?"))
	{
		u8 truncIndex = (u8)(((u32)poolIndex) & 0xff);
		Assert((s32)truncIndex == poolIndex);
#if __DEV
		for (s32 i = 0; i < m_pedInstances.GetCount(); ++i)
		{
			Assertf(m_pedInstances[i] != truncIndex, "Stored ped instance already in list! index: %d - stored index: %d - truncIndex: %d - 0x%p", i, m_pedInstances[i], truncIndex, ped);
		}
#endif
		m_pedInstances.PushAndGrow(truncIndex);
	}
}

void CPedModelInfo::RemovePedInstance(CPed* ped)
{
	if (!ped)
		return;

	s32 poolIndex = CPed::GetPool()->GetJustIndex(ped);
	if (Verifyf(poolIndex > -1 && poolIndex < 256, "Bad ped pool index, do we have too many peds?"))
	{
		u8 truncIndex = (u8)(((u32)poolIndex) & 0xff);
		Assert((s32)truncIndex == poolIndex);
		for (s32 i = 0; i < m_pedInstances.GetCount(); ++i)
		{
			if (m_pedInstances[i] == truncIndex)
			{
				m_pedInstances.DeleteFast(i);
                break;
			}
		}
	}
}

CPed* CPedModelInfo::GetPedInstance(u32 index)
{
	if (Verifyf(index < m_pedInstances.GetCount(), "CPedModelInfo::GetPedInstances - index out of range"))
	{
		s32 poolIndex = (s32)m_pedInstances[index];
		return CPed::GetPool()->GetSlot(poolIndex);
	}

	return NULL;
}

float CPedModelInfo::GetDistanceSqrToClosestInstance(const Vector3& pos)
{
	float minDist = 999999.f;

	const u32 numInsts = GetNumPedInstances();
	for (s32 i = 0; i < numInsts; ++i)
	{
		CPed* ped = GetPedInstance(i);
		if (ped)
		{
			if(!ped->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool)) // don't count peds in the reuse pool
			{
				float dist = DistSquared(ped->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(pos)).Getf();
				minDist = dist < minDist ? dist : minDist;
			}
		}
	}	
	
	// account for peds in the spawning queues
	float minScheduledDist = CPedPopulation::GetDistanceSqrToClosestScheduledPedWithModelId(pos, GetStreamSlot().Get());
	
	if(minScheduledDist < minDist)
	{
		// there is a scheduled ped closer to this position
		return minScheduledDist;
	}

	return minDist;
}

#if __BANK
void CPedModelInfo::DumpPedInstanceDataCB()
{
	CEntity* entity = (CEntity*)g_PickerManager.GetSelectedEntity();
	if (!entity || !entity->GetIsTypePed())
	{
		Displayf("No selected ped\n");
		return;
	}

	CPed* selectedPed = (CPed*)entity;
	CPedModelInfo* pmi = selectedPed->GetPedModelInfo();
	Assert(pmi);

	Displayf("**** Ped instance data for '%s' (%d / %d) ****\n", pmi->GetModelName(), pmi->GetNumPedInstances(), CPed::GetPool()->GetNoOfUsedSpaces());
	for (s32 i = 0; i < pmi->GetNumPedInstances(); ++i)
	{
		CPed* ped = pmi->GetPedInstance(i);
		if (ped)
			Displayf("** Ped %d - 0x%p - (%.2f, %.2f, %.2f) **\n", i, ped, ped->GetTransform().GetPosition().GetXf(), ped->GetTransform().GetPosition().GetYf(), ped->GetTransform().GetPosition().GetZf());
		else
			Displayf("** Ped %d - null ped! **\n", i);
	}
}

atArray<sDebugSize> CPedModelInfo::ms_pedSizes;
void CPedModelInfo::DumpAveragePedSize()
{
    // skip streamed peds for now, they can skew the numbers and aren't used as ambient peds anyway
    if (GetIsStreamedGfx() || m_bIs_IG_Ped)
        return;

	fwModelId modelId = CModelInfo::GetModelIdFromName(GetModelName());
	if (modelId.IsValid())
	{
		if (!CModelInfo::HaveAssetsLoaded(modelId))
		{
			CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD);
			CStreaming::LoadAllRequestedObjects();
		}
	}

    u32 virtualSize  = 0;
    u32 physicalSize = 0;
    CModelInfo::GetObjectAndDependenciesSizes(CModelInfo::GetModelIdFromName(GetModelName()), virtualSize, physicalSize, CPedModelInfo::GetResidentObjects().GetElements(), CPedModelInfo::GetResidentObjects().GetCount(), true);

    u32 virtualSizeHd = 0;
    u32 physicalSizeHd = 0;
    if (GetHDTxdIndex() != -1)
    {
        virtualSizeHd = CStreaming::GetObjectVirtualSize(strLocalIndex(GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
        physicalSizeHd = CStreaming::GetObjectPhysicalSize(strLocalIndex(GetHDTxdIndex()), g_TxdStore.GetStreamingModuleId());
    }

    sDebugSize data;
    data.main = virtualSize;
    data.vram = physicalSize;
    data.mainHd = virtualSizeHd;
    data.vramHd = physicalSizeHd;
    data.name = GetModelName();

    ms_pedSizes.PushAndGrow(data);
}
#endif

// Singleton manager instance
CPedPerceptionInfoManager CPedPerceptionInfoManager::m_PedPerceptionInfoManagerInstance;

// Init by loading the given filename
void CPedPerceptionInfoManager::Init(const char* pFileName)
{
	Load(pFileName);
}

// Reset to delete all the data.
void CPedPerceptionInfoManager::Shutdown()
{
	// Delete
	Reset();
}

// Delete all the existing data, then instruct the parser system to load the meta file
void CPedPerceptionInfoManager::Load(const char* pFileName)
{
	//CPedPopulation::RemoveAllPedsWithException(NULL, CPedPopulation::PMR_CanBeDeleted);

	Reset();

	// Load
	PARSER.LoadObject(pFileName, "meta", m_PedPerceptionInfoManagerInstance);

}

// Reset all data in the array.
void CPedPerceptionInfoManager::Reset()
{
	m_PedPerceptionInfoManagerInstance.m_aPedPerceptionInfoData.Reset();
}

