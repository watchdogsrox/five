// 
// camera/system/CameraFactory.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "camera/system/CameraFactory.h"

#include "bank/bkmgr.h"
#include "fwsys/fileExts.h"
#include "grcore/debugdraw.h"
#include "parser/manager.h"
#include "parser/psofile.h"
#include "parser/restparserservices.h"
#include "parser/structure.h"

#include "fwutil/xmacro.h"

#include "camera/CamInterface.h"
#include "camera/animscene/AnimSceneDirector.h"
#include "camera/cinematic/camera/animated/CinematicAnimatedCamera.h"
#include "camera/cinematic/camera/tracking/CinematicCamManCamera.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/tracking/CinematicHeliChaseCamera.h"
#include "camera/cinematic/camera/tracking/CinematicIdleCamera.h"
#include "camera/cinematic/camera/tracking/CinematicFirstPersonIdleCamera.h"
#include "camera/cinematic/camera/tracking/CinematicGroupCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedPartCamera.h"
#include "camera/cinematic/camera/tracking/CinematicPedCloseUpCamera.h"
#include "camera/cinematic/camera/tracking/CinematicStuntCamera.h"
#include "camera/cinematic/camera/tracking/CinematicTrainTrackCamera.h"
#include "camera/cinematic/camera/tracking/CinematicTwoShotCamera.h"
#include "camera/cinematic/camera/tracking/CinematicVehicleTrackingCamera.h"
#include "camera/cinematic/camera/orbit/CinematicVehicleOrbitCamera.h"
#include "camera/cinematic/camera/tracking/CinematicPositionCamera.h"
#include "camera/cinematic/camera/tracking/CinematicWaterCrashCamera.h"
#include "camera/cinematic/context/BaseCinematicContext.h"
#include "camera/cinematic/context/CinematicBustedContext.h"
#include "camera/cinematic/context/CinematicFallFromHeliContext.h"
#include "camera/cinematic/context/CinematicOnFootIdleContext.h"
#include "camera/cinematic/context/CinematicInVehicleContext.h"
#include "camera/cinematic/context/CinematicInVehicleWantedContext.h"
#include "camera/cinematic/context/CinematicInVehicleFirstPersonContext.h"
#include "camera/cinematic/context/CinematicInVehicleOverriddenFirstPersonContext.h"
#include "camera/cinematic/context/CinematicMissileKillContext.h"
#include "camera/cinematic/context/CinematicWaterCrashContext.h"
#include "camera/cinematic/context/CinematicOnFootAssistedAimingContext.h"
#include "camera/cinematic/context/CinematicOnFootMeleeContext.h"
#include "camera/cinematic/context/CinematicParachuteContext.h"
#include "camera/cinematic/context/CinematicScriptContext.h"
#include "camera/cinematic/context/CinematicStuntJumpContext.h"
#include "camera/cinematic/context/CinematicOnFootSpectatingContext.h"
#include "camera/cinematic/context/CinematicSpectatorNewsChannelContext.h"
#include "camera/cinematic/shot/CinematicBustedShot.h"
#include "camera/cinematic/shot/CinematicInVehicleGroupShot.h"
#include "camera/cinematic/shot/CinematicInVehicleShot.h"
#include "camera/cinematic/shot/CinematicInVehicleWantedShot.h"
#include "camera/cinematic/shot/CinematicFallFromHeliShot.h"
#include "camera/cinematic/shot/CinematicOnFootAssistedAimingShot.h"
#include "camera/cinematic/shot/CinematicOnFootIdleShot.h"
#include "camera/cinematic/shot/CinematicOnFootFirstPersonIdleShot.h"
#include "camera/cinematic/shot/CinematicOnFootMeleeShot.h"
#include "camera/cinematic/shot/CinematicOnFootSpectatingShot.h"
#include "camera/cinematic/shot/CinematicMissileKillShot.h"
#include "camera/cinematic/shot/CinematicWaterCrashShot.h"
#include "camera/cinematic/shot/CinematicParachuteShot.h"
#include "camera/cinematic/shot/CinematicScriptShot.h"
#include "camera/cinematic/shot/CinematicStuntJumpShot.h"
#include "camera/cinematic/shot/CinematicTrainShot.h"
#include "camera/cutscene/AnimatedCamera.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/debug/FreeCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonPedAimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/aim/FirstPersonHeadTrackingAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAimInCoverCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedMeleeAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonVehicleAimCamera.h"
#include "camera/gameplay/follow/FollowObjectCamera.h"
#include "camera/gameplay/follow/FollowParachuteCamera.h"
#include "camera/gameplay/follow/FollowPedCamera.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/gameplay/follow/TableGamesCamera.h"
#include "camera/helpers/AnimatedFrameShaker.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/CatchUpHelper.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/helpers/FrameShaker.h"
#include "camera/helpers/HintHelper.h"
#include "camera/helpers/InconsistentBehaviourZoomHelper.h"
#include "camera/helpers/LookAheadHelper.h"
#include "camera/helpers/LookAtDampingHelper.h"
#include "camera/helpers/NearClipScanner.h"
#include "camera/helpers/Oscillator.h"
#include "camera/helpers/SplineNode.h"
#include "camera/helpers/SpringMount.h"
#include "camera/helpers/StickyAimHelper.h"
#include "camera/helpers/ThirdPersonFrameInterpolator.h"
#include "camera/helpers/switch/BaseSwitchHelper.h"
#include "camera/helpers/switch/LongSwoopSwitchHelper.h"
#include "camera/helpers/switch/ShortRotationSwitchHelper.h"
#include "camera/helpers/switch/ShortTranslationSwitchHelper.h"
#include "camera/helpers/switch/ShortZoomInOutSwitchHelper.h"
#include "camera/helpers/switch/ShortZoomToHeadSwitchHelper.h"
#include "camera/marketing/MarketingAToBCamera.h"
#include "camera/marketing/MarketingDirector.h"
#include "camera/marketing/MarketingFreeCamera.h"
#include "camera/marketing/MarketingMountedCamera.h"
#include "camera/marketing/MarketingOrbitCamera.h"
#include "camera/marketing/MarketingStickyCamera.h"
#include "camera/replay/ReplayRecordedCamera.h"
#include "camera/replay/ReplayPresetCamera.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/scripted/ScriptedCamera.h"
#include "camera/scripted/ScriptedFlyCamera.h"
#include "camera/scripted/BaseSplineCamera.h"
#include "camera/scripted/RoundedSplineCamera.h"
#include "camera/scripted/SmoothedSplineCamera.h"
#include "camera/scripted/TimedSplineCamera.h"
#include "camera/scripted/CustomTimedSplineCamera.h"
#include "camera/switch/SwitchCamera.h"
#include "camera/switch/SwitchDirector.h"
#include "camera/syncedscene/SyncedSceneDirector.h"
#include "camera/system/CameraMetadata.h"
#include "control/replay/ReplaySettings.h"
#include "camera/replay/ReplayDirector.h"
#include "system/FileMgr.h"


CAMERA_OPTIMISATIONS()

//
//Size the object pools:
//
const u32 g_MaxNumCameras				= 27;
FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(camBaseCamera, g_MaxNumCameras, 0.61f, atHashString("camBaseCamera",0xfe12ce88), camFactory::GetMaxCameraSize());

const u32 g_MaxNumSwitchHelpers			= 1;
FW_INSTANTIATE_BASECLASS_POOL(camBaseSwitchHelper, g_MaxNumSwitchHelpers, atHashString("camBaseSwitchHelper",0x7fb0c46b), camFactory::GetMaxSwitchHelperSize());

#if GTA_REPLAY
const u32 g_MaxNumCollisionHelpers		= 4;
#else
const u32 g_MaxNumCollisionHelpers		= 2;
#endif
FW_INSTANTIATE_CLASS_POOL(camCollision, g_MaxNumCollisionHelpers, atHashString("camCollision",0x1eb7aae5));

const u32 g_MaxNumControlHelpers		= 23;
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(camControlHelper, g_MaxNumControlHelpers, 0.68f, atHashString("camControlHelper",0xd026e9a8));

const u32 g_MaxNumFrameShakers			= 16;
FW_INSTANTIATE_BASECLASS_POOL(camBaseFrameShaker, g_MaxNumFrameShakers, atHashString("camBaseFrameShaker",0xd7946dab), camFactory::GetMaxFrameShakerSize());

//NOTE: Envelopes are used by both camera frame shakers and cameras, so ensure that enough are allocated for both uses.
const u32 g_MaxNumEnvelopes				= 90;
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(camEnvelope, g_MaxNumEnvelopes, 0.45f, atHashString("camEnvelope",0xc93e3d5));

const u32 g_MaxNumOscillators			= 90;
FW_INSTANTIATE_CLASS_POOL(camOscillator, g_MaxNumOscillators, atHashString("camOscillator",0xfe8e27d9));

const u32 g_MaxNumSpringMounts			= 4;
FW_INSTANTIATE_CLASS_POOL(camSpringMount, g_MaxNumSpringMounts, atHashString("camSpringMount",0x3d211eca));

const u32 g_MaxNumFrameInterpolators	= 12;
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(camFrameInterpolator, g_MaxNumFrameInterpolators, 0.69f, atHashString("camFrameInterpolator",0xe3607a47));

const u32 g_MaxNumThirdPersonFrameInterpolators	= 2;
FW_INSTANTIATE_CLASS_POOL(camThirdPersonFrameInterpolator, g_MaxNumThirdPersonFrameInterpolators, atHashString("camThirdPersonFrameInterpolator",0x6e46cc34));

const u32 g_MaxNumSplineNodes			= 20;
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(camSplineNode, g_MaxNumSplineNodes, 0.68f, atHashString("camSplineNode",0x24836b53));

const u32 g_MaxNumHintHelpers			= 2;
FW_INSTANTIATE_CLASS_POOL(camHintHelper, g_MaxNumHintHelpers, atHashString("camHintHelper",0xcebe5a3a));

const u32 g_MaxNumInconsistentBehaviourZoomHelpers = 2;
FW_INSTANTIATE_CLASS_POOL(camInconsistentBehaviourZoomHelper, g_MaxNumInconsistentBehaviourZoomHelpers, atHashString("camInconsistentBehaviourZoomHelper",0xdaa0a9e0));

const u32 g_MaxNumCatchUpHelpers		= 2;
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(camCatchUpHelper, g_MaxNumCatchUpHelpers, 0.38f, atHashString("camCatchUpHelper",0x504fe2f5));

const u32 g_MaxNumLookAheadHelpers		= 2;
FW_INSTANTIATE_CLASS_POOL(camLookAheadHelper, g_MaxNumLookAheadHelpers, atHashString("camLookAheadHelper",0x74ab6150));

const u32 g_MaxNumLookAtDampingHelpers	= 4;
FW_INSTANTIATE_CLASS_POOL(camLookAtDampingHelper, g_MaxNumLookAtDampingHelpers, atHashString("camLookAtDampingHelper",0xb442603d));

const u32 g_MaxNumStickyAimHelpers		= 2;
FW_INSTANTIATE_CLASS_POOL(camStickyAimHelper, g_MaxNumStickyAimHelpers, atHashString("camStickyAimHelper",0xeb511f85));

#define MAP_FACTORY_HELPER(_MetadataClass, _Class)			(ms_FactoryHelperMap[_MetadataClass::parser_GetStaticStructure()->GetNameHash()] = \
																rage_new camFactoryHelperPooled<_Class>)
#define MAP_FACTORY_HELPER_NO_POOL(_MetadataClass, _Class)	(ms_FactoryHelperMap[_MetadataClass::parser_GetStaticStructure()->GetNameHash()] = \
																rage_new camFactoryHelper<_Class>)

#define CHECK_CLASS_SIZE(_T, _MaxSize, _LargestType)	if(sizeof(_T) > _MaxSize) { _MaxSize = sizeof(_T); OUTPUT_ONLY(_LargestType = #_T;) }

#if __BANK

#if PARSER_ALL_METADATA_HAS_NAMES
bkGroup* camFactory::ms_MetadataWidgetGroup = NULL;
#endif // PARSER_ALL_METADATA_HAS_NAMES

PARAM(rawCameraMetadata, "[camFactory] Load the raw XML camera metadata from the assets folder if no alternative path specified");
const char* g_CameraMetadataAssetsSubPath = "export/data/metadata/cameras.pso.meta";

#endif // __BANK

camMetadataStore* camFactory::ms_MetadataStore = NULL;

atMap<u32, camBaseObjectMetadata*>	camFactory::ms_MetadataMap;
atMap<u32, camFactoryHelperBase*>	camFactory::ms_FactoryHelperMap;


void camFactoryHelperBase::EnsureObjectClassIdIsValid(camBaseObject*& object, const camBaseObjectMetadata& ASSERT_ONLY(metadata),
	const atHashWithStringNotFinal& classIdToVerify)
{
	if(object && !cameraVerifyf(object->GetIsClassId(classIdToVerify.GetHash()),
		"Camera metadata %s (hash: %u) has generated an object of an unexpected type (hash: %u - expected class name: %s, hash: %u)",
		SAFE_CSTRING(metadata.m_Name.GetCStr()), metadata.m_Name.GetHash(), object->GetClassId().GetHash(), SAFE_CSTRING(classIdToVerify.GetCStr()),
		classIdToVerify.GetHash()))
	{
		delete object;
		object = NULL;
	}
}

bool camFactoryHelperBase::VerifyObjectPool(fwBasePool* pool, const camBaseObjectMetadata& ASSERT_ONLY(metadata),
	const atHashWithStringNotFinal& ASSERT_ONLY(classId)) const
{
	ASSERT_ONLY(const char* className		= SAFE_CSTRING(classId.GetCStr());)
	ASSERT_ONLY(const char* metadataName	= SAFE_CSTRING(metadata.m_Name.GetCStr());)

	bool isPoolValid	= cameraVerifyf((pool != NULL), "Cannot create a %s camera object (%s) as its pool has not been allocated", className,
							metadataName);
	if(isPoolValid)
	{
		isPoolValid		= cameraVerifyf(pool->GetNoOfFreeSpaces() > 0, "Cannot create a %s camera object (%s) as its pool is full (%d slots)",
							className, metadataName, pool->GetSize());
#if __DEV
		if(!isPoolValid)
		{
			camFactory::DebugDumpCreateObjectCallStack(pool);
		}
#endif // __DEV
	}

	return isPoolValid;
}

void camFactory::InitPools()
{
	camBaseCamera::InitPool( MEMBUCKET_GAMEPLAY );
	camBaseSwitchHelper::InitPool( MEMBUCKET_GAMEPLAY );
	camCatchUpHelper::InitPool( MEMBUCKET_GAMEPLAY );
	camCollision::InitPool( MEMBUCKET_GAMEPLAY );
	camControlHelper::InitPool( MEMBUCKET_GAMEPLAY );
	camEnvelope::InitPool( MEMBUCKET_GAMEPLAY );
	camFrameInterpolator::InitPool( MEMBUCKET_GAMEPLAY );
	camBaseFrameShaker::InitPool( MEMBUCKET_GAMEPLAY );
	camHintHelper::InitPool( MEMBUCKET_GAMEPLAY );
	camInconsistentBehaviourZoomHelper::InitPool( MEMBUCKET_GAMEPLAY );
	camLookAheadHelper::InitPool( MEMBUCKET_GAMEPLAY );
	camLookAtDampingHelper::InitPool( MEMBUCKET_GAMEPLAY );
	camStickyAimHelper::InitPool( MEMBUCKET_GAMEPLAY );
	camOscillator::InitPool( MEMBUCKET_GAMEPLAY );
	camSplineNode::InitPool( MEMBUCKET_GAMEPLAY );
	camSpringMount::InitPool( MEMBUCKET_GAMEPLAY );
	camThirdPersonFrameInterpolator::InitPool( MEMBUCKET_GAMEPLAY );

#if !__FINAL
	if(camBaseCamera::GetPool())
	{
		camBaseCamera::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camBaseSwitchHelper::GetPool())
	{
		camBaseSwitchHelper::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camCatchUpHelper::GetPool())
	{
		camCatchUpHelper::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camCollision::GetPool())
	{
		camCollision::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camControlHelper::GetPool())
	{
		camControlHelper::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camEnvelope::GetPool())
	{
		camEnvelope::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camFrameInterpolator::GetPool())
	{
		camFrameInterpolator::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camBaseFrameShaker::GetPool())
	{
		camBaseFrameShaker::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camHintHelper::GetPool())
	{
		camHintHelper::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camInconsistentBehaviourZoomHelper::GetPool())
	{
		camInconsistentBehaviourZoomHelper::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camLookAheadHelper::GetPool())
	{
		camLookAheadHelper::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camLookAtDampingHelper::GetPool())
	{
		camLookAtDampingHelper::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if (camStickyAimHelper::GetPool())
	{
		camStickyAimHelper::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camOscillator::GetPool())
	{
		camOscillator::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camSplineNode::GetPool())
	{
		camSplineNode::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camSpringMount::GetPool())
	{
		camSpringMount::GetPool()->SetSizeIsFromConfigFile(false);
	}
	if(camThirdPersonFrameInterpolator::GetPool())
	{
		camThirdPersonFrameInterpolator::GetPool()->SetSizeIsFromConfigFile(false);
	}
	
#endif // !__FINAL
}

void camFactory::ShutdownPools()
{
	camBaseCamera::ShutdownPool();
	camBaseSwitchHelper::ShutdownPool();
	camCatchUpHelper::ShutdownPool();
	camCollision::ShutdownPool();
	camControlHelper::ShutdownPool();
	camEnvelope::ShutdownPool();
	camFrameInterpolator::ShutdownPool();
	camBaseFrameShaker::ShutdownPool();
	camHintHelper::ShutdownPool();
	camInconsistentBehaviourZoomHelper::ShutdownPool();
	camLookAheadHelper::ShutdownPool(); 
	camLookAtDampingHelper::ShutdownPool(); 
	camStickyAimHelper::ShutdownPool();
	camOscillator::ShutdownPool();
	camSplineNode::ShutdownPool();
	camSpringMount::ShutdownPool();
	camThirdPersonFrameInterpolator::ShutdownPool();
}

void camFactory::Init()
{
	//Load the camera metadata.
	ms_MetadataStore = rage_new camMetadataStore();
	cameraFatalAssertf(ms_MetadataStore != NULL, "Failed to allocate memory for the camera metadata");

	char metadataFilename[RAGE_MAX_PATH] = "";

#if __BANK
	if(PARAM_rawCameraMetadata.Get())
	{
		const char* alternativeMetadataPath = NULL;
		PARAM_rawCameraMetadata.Get(alternativeMetadataPath);

		if ((alternativeMetadataPath && alternativeMetadataPath[0] == '\0') || !alternativeMetadataPath)
		{
			formatf(metadataFilename, RAGE_MAX_PATH, "%s%s", CFileMgr::GetAssetsFolder(), g_CameraMetadataAssetsSubPath);
		}
		else if (alternativeMetadataPath)
		{
			formatf(metadataFilename, RAGE_MAX_PATH, "%s", alternativeMetadataPath);
		}

		ASSERT_ONLY(bool hasSucceeded = )PARSER.InitAndLoadObject(metadataFilename, "", *ms_MetadataStore);
		cameraFatalAssertf(hasSucceeded, "Failed to load the camera metadata XML file (%s)", metadataFilename);
	}
	else
#endif // __BANK
	{
		const CDataFileMgr::DataFile* metadataFile = DATAFILEMGR.GetFirstFile(CDataFileMgr::CAMERA_METADATA_FILE);
		GAMETOOL_ONLY(if (metadataFile && DATAFILEMGR.IsValid(metadataFile)))
		{
			cameraFatalAssertf(metadataFile && DATAFILEMGR.IsValid(metadataFile), "Failed to find the camera metadata PSO file");

			formatf(metadataFilename, RAGE_MAX_PATH, "%s.%s", metadataFile->m_filename, META_FILE_EXT);

			psoFile* cameraPsoFile = psoLoadFile(metadataFilename, PSOLOAD_PREP_FOR_PARSER_LOADING, atFixedBitSet32().Set(psoFile::REQUIRE_CHECKSUM));
			cameraFatalAssertf(cameraPsoFile, "Failed to load the camera metadata PSO file (%s)", metadataFilename);

			ASSERT_ONLY(const bool hasSucceeded = )psoInitAndLoadObject(*cameraPsoFile, *ms_MetadataStore);
			cameraFatalAssertf(hasSucceeded, "Failed to parse the camera metadata PSO file (%s)", metadataFilename);

			delete cameraPsoFile;

			PARSER.LoadPatchFile(metadataFile->m_filename, "meta", ms_MetadataStore->parser_GetPointer(), PARSER.GetStructure(ms_MetadataStore));
		}
	}

	parRestRegisterSingleton("Camera", *ms_MetadataStore, metadataFilename);

	//Insert the flat list of object metadata into a hash map, to improve the efficiency of metadata searches.
	const int numObjects = ms_MetadataStore->m_MetadataList.GetCount();
	for(int i=0; i<numObjects; i++)
	{
		camBaseObjectMetadata* metadata = ms_MetadataStore->m_MetadataList[i];
		if(metadata)
		{
			const u32 nameHash = metadata->m_Name.GetHash();
			if(cameraVerifyf(!ms_MetadataMap.Access(nameHash), "Found a duplicate name hash for a camera object (name: %s, hash: %u)",
				SAFE_CSTRING(metadata->m_Name.GetCStr()), nameHash))
			{
				ms_MetadataMap.Insert(nameHash, metadata);
			}
		}
		else
		{
			Warningf("Meta Data NULL on index %d", i);
		}
	}

	MapFactoryHelpers();

#if __BANK && PARSER_ALL_METADATA_HAS_NAMES
	if(!ms_MetadataWidgetGroup && !BANKMGR.FindWidget("Camera/Create camera widgets"))
	{
		//A new level has been loaded after the camera widgets were instantiated, so we must explicitly add the metadata widgets here.
		bkBank* bank = BANKMGR.FindBank("Camera");
		if(bank)
		{
			AddWidgetsForMetadata(*bank);
		}
	}
#endif // __BANK && PARSER_ALL_METADATA_HAS_NAMES

	camCollision::InitClass();
	camControlHelper::InitClass();
	camThirdPersonPedAimInCoverCamera::InitClass();
#if __BANK
	camMarketingFreeCamera::InitClass();
#endif // __BANK
}

void camFactory::Shutdown()
{
	camControlHelper::ShutdownClass();

	ms_MetadataMap.Kill();

	//Clean up the factory helper map.
	static atMap<u32, camFactoryHelperBase*>::Iterator it = ms_FactoryHelperMap.CreateIterator();
	for(it.Start(); !it.AtEnd(); it.Next())
	{
		camFactoryHelperBase* helper = it.GetData();
		if(helper)
		{
			delete helper;
		}
	}

	ms_FactoryHelperMap.Kill();

#if __BANK && PARSER_ALL_METADATA_HAS_NAMES
	//Remove any widgets associated with the metadata.
	if(ms_MetadataWidgetGroup)
	{
		ms_MetadataWidgetGroup->Destroy();
		ms_MetadataWidgetGroup = NULL;
	}

	camBuildWidgetsVisitor::ShutdownClass();
#endif // __BANK && PARSER_ALL_METADATA_HAS_NAMES

	if(ms_MetadataStore)
	{
		delete ms_MetadataStore;
		ms_MetadataStore = NULL;
	}
}

void camFactory::MapFactoryHelpers()
{
	//Map camera helpers.
	MAP_FACTORY_HELPER(camOscillatorMetadata,						camOscillator);
	MAP_FACTORY_HELPER(camEnvelopeMetadata,							camEnvelope);
	MAP_FACTORY_HELPER(camShakeMetadata,							camOscillatingFrameShaker);
	MAP_FACTORY_HELPER(camAnimatedShakeMetadata,					camAnimatedFrameShaker);
	MAP_FACTORY_HELPER(camSpringMountMetadata,						camSpringMount);
	MAP_FACTORY_HELPER(camControlHelperMetadata,					camControlHelper);
	MAP_FACTORY_HELPER(camCollisionMetadata,						camCollision);
	MAP_FACTORY_HELPER(camHintHelperMetadata,						camHintHelper);
	MAP_FACTORY_HELPER(camInconsistentBehaviourZoomHelperMetadata,	camInconsistentBehaviourZoomHelper);
	MAP_FACTORY_HELPER(camCatchUpHelperMetadata,					camCatchUpHelper);
	MAP_FACTORY_HELPER(camLookAheadHelperMetadata,					camLookAheadHelper);
	MAP_FACTORY_HELPER(camLookAtDampingHelperMetadata,				camLookAtDampingHelper);
	MAP_FACTORY_HELPER(camStickyAimHelperMetadata,					camStickyAimHelper);

	// - Switch helpers.
	MAP_FACTORY_HELPER(camLongSwoopSwitchHelperMetadata,			camLongSwoopSwitchHelper);
	MAP_FACTORY_HELPER(camShortRotationSwitchHelperMetadata,		camShortRotationSwitchHelper);
	MAP_FACTORY_HELPER(camShortTranslationSwitchHelperMetadata,		camShortTranslationSwitchHelper);
	MAP_FACTORY_HELPER(camShortZoomInOutSwitchHelperMetadata,		camShortZoomInOutSwitchHelper);
	MAP_FACTORY_HELPER(camShortZoomToHeadSwitchHelperMetadata,		camShortZoomToHeadSwitchHelper);

	//Map cameras.
	MAP_FACTORY_HELPER(camFirstPersonPedAimCameraMetadata,			camFirstPersonPedAimCamera);
#if FPS_MODE_SUPPORTED
	MAP_FACTORY_HELPER(camFirstPersonShooterCameraMetadata,			camFirstPersonShooterCamera);
#endif // FPS_MODE_SUPPORTED
	MAP_FACTORY_HELPER(camFirstPersonHeadTrackingAimCameraMetadata,	camFirstPersonHeadTrackingAimCamera);
	MAP_FACTORY_HELPER(camThirdPersonPedAimCameraMetadata,			camThirdPersonPedAimCamera);
	MAP_FACTORY_HELPER(camThirdPersonPedAimInCoverCameraMetadata,	camThirdPersonPedAimInCoverCamera);
	MAP_FACTORY_HELPER(camThirdPersonPedAssistedAimCameraMetadata,	camThirdPersonPedAssistedAimCamera);
	MAP_FACTORY_HELPER(camThirdPersonPedMeleeAimCameraMetadata,		camThirdPersonPedMeleeAimCamera);
	MAP_FACTORY_HELPER(camThirdPersonVehicleAimCameraMetadata,		camThirdPersonVehicleAimCamera);
	MAP_FACTORY_HELPER(camTableGamesCameraMetadata,					camTableGamesCamera);
	MAP_FACTORY_HELPER(camCinematicAnimatedCameraMetadata,			camCinematicAnimatedCamera);
	MAP_FACTORY_HELPER(camCinematicCameraManCameraMetadata,			camCinematicCamManCamera);
	MAP_FACTORY_HELPER(camCinematicGroupCameraMetadata,				camCinematicGroupCamera);
	MAP_FACTORY_HELPER(camCinematicHeliChaseCameraMetadata,			camCinematicHeliChaseCamera);
	MAP_FACTORY_HELPER(camCinematicIdleCameraMetadata,				camCinematicIdleCamera);
	MAP_FACTORY_HELPER(camCinematicFirstPersonIdleCameraMetadata,	camCinematicFirstPersonIdleCamera);
	MAP_FACTORY_HELPER(camCinematicMountedCameraMetadata,			camCinematicMountedCamera);
	MAP_FACTORY_HELPER(camCinematicMountedPartCameraMetadata,		camCinematicMountedPartCamera);
	MAP_FACTORY_HELPER(camCinematicPedCloseUpCameraMetadata,		camCinematicPedCloseUpCamera);
	MAP_FACTORY_HELPER(camCinematicPositionCameraMetadata,			camCinematicPositionCamera);
	MAP_FACTORY_HELPER(camCinematicWaterCrashCameraMetadata,		camCinematicWaterCrashCamera);
	MAP_FACTORY_HELPER(camCinematicStuntCameraMetadata,				camCinematicStuntCamera);
	MAP_FACTORY_HELPER(camCinematicVehicleOrbitCameraMetadata,		camCinematicVehicleOrbitCamera);
	MAP_FACTORY_HELPER(camCinematicVehicleLowOrbitCameraMetadata,	camCinematicVehicleLowOrbitCamera);
	MAP_FACTORY_HELPER(camCinematicTrainTrackingCameraMetadata,		camCinematicTrainTrackCamera);
	MAP_FACTORY_HELPER(camCinematicTwoShotCameraMetadata,			camCinematicTwoShotCamera);
	MAP_FACTORY_HELPER(camCinematicVehicleTrackingCameraMetadata,	camCinematicVehicleTrackingCamera);
	MAP_FACTORY_HELPER(camAnimatedCameraMetadata,					camAnimatedCamera);
#if !__FINAL //No debug cameras in final builds.
	MAP_FACTORY_HELPER(camFreeCameraMetadata,						camFreeCamera);
#endif // !__FINAL
	MAP_FACTORY_HELPER(camFollowPedCameraMetadata,					camFollowPedCamera);
	MAP_FACTORY_HELPER(camFollowVehicleCameraMetadata,				camFollowVehicleCamera);
	MAP_FACTORY_HELPER(camFollowObjectCameraMetadata,				camFollowObjectCamera);
	MAP_FACTORY_HELPER(camFollowParachuteCameraMetadata,			camFollowParachuteCamera);
#if __BANK //Marketing tools are only available in BANK builds.
	MAP_FACTORY_HELPER(camMarketingFreeCameraMetadata,				camMarketingFreeCamera);
	MAP_FACTORY_HELPER(camMarketingAToBCameraMetadata,				camMarketingAToBCamera);
	MAP_FACTORY_HELPER(camMarketingMountedCameraMetadata,			camMarketingMountedCamera);
	MAP_FACTORY_HELPER(camMarketingOrbitCameraMetadata,				camMarketingOrbitCamera);
	MAP_FACTORY_HELPER(camMarketingStickyCameraMetadata,			camMarketingStickyCamera);
#endif // __BANK
	MAP_FACTORY_HELPER(camScriptedCameraMetadata,					camScriptedCamera);
	MAP_FACTORY_HELPER(camScriptedFlyCameraMetadata,				camScriptedFlyCamera);
	MAP_FACTORY_HELPER(camRoundedSplineCameraMetadata,				camRoundedSplineCamera); 
	MAP_FACTORY_HELPER(camSmoothedSplineCameraMetadata,				camSmoothedSplineCamera); 
	MAP_FACTORY_HELPER(camTimedSplineCameraMetadata,				camTimedSplineCamera); 
	MAP_FACTORY_HELPER(camCustomTimedSplineCameraMetadata,			camCustomTimedSplineCamera); 
	MAP_FACTORY_HELPER(camSwitchCameraMetadata,						camSwitchCamera);

#if GTA_REPLAY
	MAP_FACTORY_HELPER(camReplayRecordedCameraMetadata,				camReplayRecordedCamera);
	MAP_FACTORY_HELPER(camReplayPresetCameraMetadata,				camReplayPresetCamera);
	MAP_FACTORY_HELPER(camReplayFreeCameraMetadata,					camReplayFreeCamera);
#endif // GTA_REPLAY

	//Map directors.
	//NOTE: The directors need not be pooled as they are not dynamically allocated outside of session init and shutdown.
	MAP_FACTORY_HELPER_NO_POOL(camCinematicDirectorMetadata,		camCinematicDirector);
	MAP_FACTORY_HELPER_NO_POOL(camCutsceneDirectorMetadata,			camCutsceneDirector);
	MAP_FACTORY_HELPER_NO_POOL(camDebugDirectorMetadata,			camDebugDirector);
	MAP_FACTORY_HELPER_NO_POOL(camGameplayDirectorMetadata,			camGameplayDirector);
	MAP_FACTORY_HELPER_NO_POOL(camMarketingDirectorMetadata,		camMarketingDirector);
	MAP_FACTORY_HELPER_NO_POOL(camScriptDirectorMetadata,			camScriptDirector);
	MAP_FACTORY_HELPER_NO_POOL(camSwitchDirectorMetadata,			camSwitchDirector);
	MAP_FACTORY_HELPER_NO_POOL(camSyncedSceneDirectorMetadata,		camSyncedSceneDirector);
	MAP_FACTORY_HELPER_NO_POOL(camAnimSceneDirectorMetadata,		camAnimSceneDirector);
	MAP_FACTORY_HELPER_NO_POOL(camReplayDirectorMetadata,			camReplayDirector);

	//MAP_FACTORY_HELPER_NO_POOL(camCinematicShotMetadata,						camBaseCinematicShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicHeliTrackingShotMetadata,			camHeliTrackingShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicVehicleOrbitShotMetadata,			camVehicleOrbitShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicVehicleLowOrbitShotMetadata,			camVehicleLowOrbitShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicCameraManShotMetadata,				camCameraManShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicCraningCameraManShotMetadata,		camCraningCameraManShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicVehiclePartShotMetadata,				camCinematicVehiclePartShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicInVehicleCrashShotMetadata,			camCinematicInVehicleCrashShot);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicTrainStationShotMetadata,			camCinematicTrainStationShot); //need new class
	MAP_FACTORY_HELPER_NO_POOL(camCinematicTrainRoofMountedShotMetadata,		camCinematicTrainRoofShot); //need new class
	MAP_FACTORY_HELPER_NO_POOL(camCinematicTrainPassengerShotMetadata,			camCinematicPassengerShot); //need new class
	MAP_FACTORY_HELPER_NO_POOL(camCinematicTrainTrackShotMetadata,				camCinematicTrainTrackShot); //need new class
	
	MAP_FACTORY_HELPER_NO_POOL(camCinematicPoliceCarMountedShotMetadata,		camPoliceCarMountedShot); //need new class
	MAP_FACTORY_HELPER_NO_POOL(camCinematicPoliceHeliMountedShotMetadata,		camPoliceHeliMountedShot); //need new class
	MAP_FACTORY_HELPER_NO_POOL(camCinematicPoliceInCoverShotMetadata,			camPoliceInCoverShot); //need new class
	MAP_FACTORY_HELPER_NO_POOL(camCinematicPoliceExitVehicleShotMetadata,		camPoliceExitVehicleShot); 
	MAP_FACTORY_HELPER_NO_POOL(camCinematicPoliceRoadBlockShotMetadata,			camCinematicPoliceRoadBlockShot); 
	MAP_FACTORY_HELPER_NO_POOL(camCinematicVehicleGroupShotMetadata,			camCinematicVehicleGroupShot);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootIdleShotMetadata,				camCinematicOnFootIdleShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootFirstPersonIdleShotMetadata,	camCinematicOnFootFirstPersonIdleShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicStuntJumpShotMetadata,				camCinematicStuntJumpShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicParachuteHeliShotMetadata,			camCinematicParachuteHeliShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicParachuteCameraManShotMetadata,		camCinematicParachuteCameraManShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootAssistedAimingKillShotMetadata, camCinematicOnFootAssistedAimingKillShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootAssistedAimingReactionShotMetadata, camCinematicOnFootAssistedAimingReactionShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootMeleeShotMetadata,				camCinematicOnFootMeleeShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicVehicleBonnetShotMetadata,			camCinematicVehicleBonnetShot);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicBustedShotMetadata,					camCinematicBustedShot);
	
	MAP_FACTORY_HELPER_NO_POOL(camCinematicScriptRaceCheckPointShotMetadata,	camCinematicScriptRaceCheckPointShot);
	
	MAP_FACTORY_HELPER_NO_POOL(camCinematicMissileKillShotMetadata,				camCinematicMissileKillShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicWaterCrashShotMetadata,				camCinematicWaterCrashShot);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicFallFromHeliShotMetadata,			camCinematicFallFromHeliShot);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootSpectatingShotMetadata,		camCinematicOnFootSpectatingShot);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicVehicleConvertibleRoofShotMetadata,  camCinematicVehicleConvertibleRoofShot);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicInVehicleContextMetadata,			camCinematicInVehicleContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicInVehicleWantedContextMetadata,		camCinematicInVehicleWantedContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicInVehicleFirstPersonContextMetadata,	camCinematicInVehicleFirstPersonContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicInVehicleOverriddenFirstPersonContextMetadata,	camCinematicInVehicleOverriddenFirstPersonContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicInVehicleConvertibleRoofContextMetadata, camCinematicInVehicleConvertibleRoofContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicInTrainContextMetadata,				camCinematicInTrainContext); 
	MAP_FACTORY_HELPER_NO_POOL(camCinematicInTrainAtStationContextMetadata,		camCinematicInTrainAtStationContext); 
	MAP_FACTORY_HELPER_NO_POOL(camCinematicInVehicleCrashContextMetadata,		camCinematicInVehicleCrashContext); 

	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootIdleContextMetadata,			camCinematicOnFootIdleContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicStuntJumpContextMetadata,			camCinematicStuntJumpContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicParachuteContextMetadata,			camCinematicParachuteContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootMeleeContextMetadata,			camCinematicOnFootMeleeContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootAssistedAimingContextMetadata,	camCinematicOnFootAssistedAimingContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootMeleeContextMetadata,			camCinematicOnFootMeleeContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicSpectatorNewsChannelContextMetadata,	camCinematicSpectatorNewsChannelContext);
	
	MAP_FACTORY_HELPER_NO_POOL(camCinematicInVehicleMultiplayerPassengerContextMetadata,	camCinematicInVehicleMultiplayerPassengerContext);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicScriptContextMetadata,				camCinematicScriptContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicScriptedRaceCheckPointContextMetadata, camCinematicScriptedRaceCheckPointContext);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicBustedContextMetadata,				camCinematicBustedContext);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicMissileKillContextMetadata,			camCinematicMissileKillContext);
	MAP_FACTORY_HELPER_NO_POOL(camCinematicWaterCrashContextMetadata,			camCinematicWaterCrashContext);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicFallFromHeliContextMetadata,			camCinematicFallFromHeliContext);

	MAP_FACTORY_HELPER_NO_POOL(camCinematicOnFootSpectatingContextMetadata,		camCinematicOnFootSpectatingContext);
	
	MAP_FACTORY_HELPER_NO_POOL(camCinematicScriptedMissionCreatorFailContextMetadata, camCinematicScriptedMissionCreatorFailContext);
	
	MAP_FACTORY_HELPER_NO_POOL(camNearClipScannerMetadata,						camNearClipScanner);
}

u32 camFactory::GetMaxCameraSize()
{
	u32 maxSize = 0;
	OUTPUT_ONLY(const char* largestType = NULL);

	//NOTE: There is no need to size abstract classes.
	CHECK_CLASS_SIZE(camFirstPersonPedAimCamera,			maxSize, largestType)
#if FPS_MODE_SUPPORTED
	CHECK_CLASS_SIZE(camFirstPersonShooterCamera,			maxSize, largestType)
#endif // FPS_MODE_SUPPORTED
	CHECK_CLASS_SIZE(camFirstPersonHeadTrackingAimCamera,	maxSize, largestType)
	CHECK_CLASS_SIZE(camThirdPersonPedAimCamera,			maxSize, largestType)
	CHECK_CLASS_SIZE(camThirdPersonPedAimInCoverCamera,		maxSize, largestType)
	CHECK_CLASS_SIZE(camThirdPersonPedAssistedAimCamera,	maxSize, largestType)
	CHECK_CLASS_SIZE(camThirdPersonPedMeleeAimCamera,		maxSize, largestType)
	CHECK_CLASS_SIZE(camThirdPersonVehicleAimCamera,		maxSize, largestType)
	CHECK_CLASS_SIZE(camTableGamesCamera,					maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicAnimatedCamera,			maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicCamManCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicGroupCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicHeliChaseCamera,			maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicIdleCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicFirstPersonIdleCamera,		maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicMountedCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicMountedPartCamera,			maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicPedCloseUpCamera,			maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicPositionCamera,			maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicWaterCrashCamera,			maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicStuntCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicVehicleOrbitCamera,		maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicVehicleLowOrbitCamera,		maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicTrainTrackCamera,			maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicTwoShotCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camCinematicVehicleTrackingCamera,		maxSize, largestType)
	CHECK_CLASS_SIZE(camAnimatedCamera,						maxSize, largestType)
#if !__FINAL //No debug cameras in final builds.
	CHECK_CLASS_SIZE(camFreeCamera,							maxSize, largestType)
#endif // !__FINAL
	CHECK_CLASS_SIZE(camFollowPedCamera,					maxSize, largestType)
	CHECK_CLASS_SIZE(camFollowVehicleCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camFollowObjectCamera,					maxSize, largestType)
	CHECK_CLASS_SIZE(camFollowParachuteCamera,				maxSize, largestType)
#if __BANK //Marketing tools are only available in BANK builds.
	CHECK_CLASS_SIZE(camMarketingFreeCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camMarketingAToBCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camMarketingMountedCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camMarketingOrbitCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camMarketingStickyCamera,				maxSize, largestType)
#endif // __BANK
	CHECK_CLASS_SIZE(camScriptedCamera,						maxSize, largestType)
	CHECK_CLASS_SIZE(camScriptedFlyCamera,					maxSize, largestType)
	CHECK_CLASS_SIZE(camRoundedSplineCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camSmoothedSplineCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camTimedSplineCamera,					maxSize, largestType)
	CHECK_CLASS_SIZE(camSwitchCamera,						maxSize, largestType)

#if GTA_REPLAY
	CHECK_CLASS_SIZE(camReplayRecordedCamera,				maxSize, largestType)
	CHECK_CLASS_SIZE(camReplayPresetCamera,					maxSize, largestType)
	CHECK_CLASS_SIZE(camReplayFreeCamera,					maxSize, largestType)
#endif // GTA_REPLAY

#if !__FINAL
	cameraDisplayf("Largest camera type: %s (%u bytes)", largestType, maxSize);
#endif	//	!__FINAL

	return maxSize;
}

u32 camFactory::GetMaxFrameShakerSize()
{
	u32 maxSize = 0;
	OUTPUT_ONLY(const char* largestType = NULL);

	//NOTE: There is no need to size abstract classes.
	CHECK_CLASS_SIZE(camOscillatingFrameShaker,				maxSize, largestType)
	CHECK_CLASS_SIZE(camAnimatedFrameShaker,				maxSize, largestType)

#if !__FINAL
	cameraDisplayf("Largest frame shaker type: %s (%u bytes)", largestType, maxSize);
#endif	//	!__FINAL

	return maxSize;
}

u32 camFactory::GetMaxSwitchHelperSize()
{
	u32 maxSize = 0;
	OUTPUT_ONLY(const char* largestType = NULL);

	//NOTE: There is no need to size abstract classes.
	CHECK_CLASS_SIZE(camLongSwoopSwitchHelper,			maxSize, largestType)
	CHECK_CLASS_SIZE(camShortRotationSwitchHelper,		maxSize, largestType)
	CHECK_CLASS_SIZE(camShortTranslationSwitchHelper,	maxSize, largestType)
	CHECK_CLASS_SIZE(camShortZoomInOutSwitchHelper,		maxSize, largestType)
	CHECK_CLASS_SIZE(camShortZoomToHeadSwitchHelper,	maxSize, largestType)

#if !__FINAL
		cameraDisplayf("Largest Switch helper type: %s (%u bytes)", largestType, maxSize);
#endif	//	!__FINAL

	return maxSize;
}

const camBaseObjectMetadata* camFactory::FindObjectMetadata(const atHashWithStringNotFinal& name, const parStructure* parserStructureToVerify)
{
	const u32 nameHash						= name.GetHash();
	camBaseObjectMetadata** metadataRef		= ms_MetadataMap.Access(nameHash);
	const camBaseObjectMetadata* metadata	= metadataRef ? *metadataRef : NULL;

	if(metadata && parserStructureToVerify)
	{
		const bool isClassValid = IsObjectMetadataOfClass(*metadata, *parserStructureToVerify);

		if(!cameraVerifyf(isClassValid, "The metadata for a camera object (%s) was not of the expected type (name: %s, hash: %u)",
			SAFE_CSTRING(metadata->m_Name.GetCStr()), SAFE_CSTRING(parserStructureToVerify->GetName()), parserStructureToVerify->GetNameHash()))
		{
			metadata = NULL; //The metadata is not of the expected class, so NULL our pointer.
		}
	}

	return metadata;
}

bool camFactory::IsObjectMetadataOfClass(const camBaseObjectMetadata& metadata, const parStructure& parserStructureToVerify)
{
	const parStructure* parserStructure	= metadata.parser_GetStructure();
	const bool isOfClass				= cameraVerifyf(parserStructure, "Could not find a parser structure for a camera object (%s)",
											SAFE_CSTRING(metadata.m_Name.GetCStr())) && parserStructure->IsSubclassOf(&parserStructureToVerify);

	return isOfClass;
}

camBaseObject* camFactory::CreateObject(const atHashWithStringNotFinal& name, const atHashWithStringNotFinal& classIdToVerify)
{
	camBaseObject* object = NULL;

	const camBaseObjectMetadata* metadata = FindObjectMetadata(name);
	if(cameraVerifyf(metadata, "Could not find the requested camera metadata (name: %s, hash: %u)", SAFE_CSTRING(name.GetCStr()), name.GetHash()))
	{
		object = CreateObject(*metadata, classIdToVerify);
	}

	return object;
}

camBaseObject* camFactory::CreateObject(const camBaseObjectMetadata& metadata, const atHashWithStringNotFinal& classIdToVerify)
{
	camBaseObject* object = NULL;

	parStructure* parserStructure = metadata.parser_GetStructure();
	if(cameraVerifyf(parserStructure, "Could not find a parser structure for a camera object (%s)", SAFE_CSTRING(metadata.m_Name.GetCStr())))
	{
		u32 metadataTypeHash			= parserStructure->GetNameHash();
		camFactoryHelperBase** helper	= ms_FactoryHelperMap.Access(metadataTypeHash);

		if(cameraVerifyf(helper && *helper, "Could not find a camera factory helper for a camera object (%s) of type %s",
			SAFE_CSTRING(metadata.m_Name.GetCStr()), SAFE_CSTRING(parserStructure->GetName())))
		{
			object = (*helper)->CreateObject(metadata, classIdToVerify);

#if !__NO_OUTPUT && !__FINAL
			if(object)
			{
				//collect create call stack information for this object
				object->CollectCreateObjectStack();
			}
#endif //!__NO_OUTPUT

#if __BANK
			if(object && (BANKMGR.FindWidget("Camera/Create camera widgets") == NULL))
			{
				object->AddWidgetsForInstance();
			}
#endif // __BANK
		}
	}

	return object;
}

bool camFactory::VerifySimpleObjectPool(fwBasePool* pool, const char* ASSERT_ONLY(className))
{
	bool isPoolValid	= cameraVerifyf((pool != NULL), "Cannot create a %s camera object as its pool has not been allocated", SAFE_CSTRING(className));
	if(isPoolValid)
	{
		isPoolValid		= cameraVerifyf(pool->GetNoOfFreeSpaces() > 0, "Cannot create a %s camera object as its pool is full (%d slots)",
							SAFE_CSTRING(className), pool->GetSize());
#if __DEV
		if(!isPoolValid)
		{
			//print out the create call stack of the objects in the pool.
			camFactory::DebugDumpCreateObjectCallStack(pool);
		}
#endif // __DEV
	}

	return isPoolValid;
}

#if __BANK
void camFactory::AddWidgets(bkBank& bank)
{
#if PARSER_ALL_METADATA_HAS_NAMES
	AddWidgetsForMetadata(bank);
#endif // PARSER_ALL_METADATA_HAS_NAMES

	bank.AddButton("Dump camera pool to TTY", datCallback(CFA(DebugDumpAllObjectPools)));
	bank.AddButton("Dump objects create call stacks to TTY", datCallback(CFA(DebugDumpAllCreateObjectsCallStack)));

	camBaseCamera::AddWidgets(bank);
	camFrame::AddWidgets(bank);
	camOscillatingFrameShaker::AddWidgets(bank);
	camAnimatedFrameShaker::AddWidgets(bank);
	camFollowCamera::AddWidgets(bank);
	camFollowVehicleCamera::AddWidgets(bank);
	camBaseSplineCamera::AddWidgets(bank);
	camThirdPersonPedAimCamera::AddWidgets(bank);
	camThirdPersonPedAimInCoverCamera::AddWidgets(bank);
	camThirdPersonFrameInterpolator::AddWidgets(bank);
	camSwitchCamera::AddWidgets(bank);
	camControlHelper::AddWidgets(bank);
	camMarketingFreeCamera::AddWidgets(bank);
	camLookAheadHelper::AddWidgets(bank);
	camInconsistentBehaviourZoomHelper::AddWidgets(bank);
	camCinematicCamManCamera::AddWidgets(bank);
	camCinematicHeliChaseCamera::AddWidgets(bank);
	camThirdPersonPedAssistedAimCamera::AddWidgets(bank);
	camCinematicTwoShotCamera::AddWidgets(bank);
}

const s32 g_DebugRenderTextHorizontalBorder			= 7;
const s32 g_DebugRenderTextVerticalBorder			= 12;
const s32 g_DebugRenderTextObjectPoolColumnWidth	= 40;
const s32 g_DebugRenderTextNumObjectsColumnWidth	= 12;
const s32 g_DebugRenderTextMaxObjectsColumnWidth	= 12;
const s32 g_DebugRenderMaxObjectPoolBarLength		= 55;

void camFactory::DebugDumpAllObjectPools()
{
	cameraDisplayf("############################################################################");
	cameraDisplayf("#### Dumping camera pool details on TTY                                 ####");
	DebugDumpObjectPool(camBaseCamera::GetPool());
	DebugDumpObjectPool(camBaseSwitchHelper::GetPool());
	DebugDumpObjectPool(camCatchUpHelper::GetPool());
	DebugDumpObjectPool(camCollision::GetPool());
	DebugDumpObjectPool(camControlHelper::GetPool());
	DebugDumpObjectPool(camEnvelope::GetPool());
	DebugDumpObjectPool(camFrameInterpolator::GetPool(), false);
	DebugDumpObjectPool(camBaseFrameShaker::GetPool());
	DebugDumpObjectPool(camHintHelper::GetPool());
	DebugDumpObjectPool(camInconsistentBehaviourZoomHelper::GetPool());
	DebugDumpObjectPool(camLookAheadHelper::GetPool());
	DebugDumpObjectPool(camLookAtDampingHelper::GetPool());
	DebugDumpObjectPool(camStickyAimHelper::GetPool());
	DebugDumpObjectPool(camOscillator::GetPool());
	DebugDumpObjectPool(camSpringMount::GetPool());
	DebugDumpObjectPool(camSplineNode::GetPool(), false);
	DebugDumpObjectPool(camThirdPersonFrameInterpolator::GetPool(), false);
	cameraDisplayf("############################################################################");
	cameraAssertf(false, "Forced assert after dumping the camera pools call stacks. This is meant to stop the game output spamming the TTY while you analyze the callstacks. You can ignore this.");
}

void camFactory::DebugDumpObjectPool(const fwBasePool* pool, bool isCamBaseObject)
{
	if(!pool)
	{
		return;
	}

	const s32 storageSize = (s32)pool->GetStorageSize();
	const s32 numObjects  = pool->GetNoOfUsedSpaces();
	const s32 maxObjects  = pool->GetSize();
	cameraDisplayf("############################################################################");
	cameraDisplayf("#### Dumping objects information for pool %s %d byte(s) %d/%d", pool->GetName(), storageSize, numObjects, maxObjects);
	cameraDisplayf("############################################################################");

	if(!isCamBaseObject)
	{
		return;
	}

	const s32 debugStringLength = 512;
	char debugString[debugStringLength];

	for(s32 index = 0; index < maxObjects; ++index)
	{
		const camBaseObject* pObject = (const camBaseObject*)pool->GetSlot(index);
		if(pObject)
		{
			const char* className       = pObject->GetClassId().GetCStr();
			const u32   classNameHashed = pObject->GetClassId().GetHash();
			const char* instanceName    = pObject->GetName();

			//check if is a camera object
			if(camBaseCamera::GetPool() == pool)
			{
				formatf(debugString, debugStringLength, "#### [%02d]: Class: %s (%u)", index, SAFE_CSTRING(className), classNameHashed, SAFE_CSTRING(instanceName));

				const camBaseCamera* pCamera = (const camBaseCamera*)pObject;
				pCamera->DebugAppendDetailedText(debugString, debugStringLength);
				//Append useful details about this camera, such as the script thread that owns it, if it is script controlled.
				camInterface::GetScriptDirector().DebugAppendScriptControlledCameraDetails(*pCamera, debugString, debugStringLength);
			}
			else
			{
				formatf(debugString, debugStringLength, "#### [%02d]: Class: %s (%u) Name %s", index, SAFE_CSTRING(className), classNameHashed, SAFE_CSTRING(instanceName));
			}

			cameraDisplayf("%s", debugString);
		}
	}
}

void camFactory::DebugDumpAllCreateObjectsCallStack()
{
	cameraDisplayf("############################################################################");
	cameraDisplayf("#### Dumping camera pool call stack details on TTY                      ####");
	DebugDumpCreateObjectCallStack(camBaseCamera::GetPool());
	DebugDumpCreateObjectCallStack(camBaseSwitchHelper::GetPool());
	DebugDumpCreateObjectCallStack(camCatchUpHelper::GetPool());
	DebugDumpCreateObjectCallStack(camCollision::GetPool());
	DebugDumpCreateObjectCallStack(camControlHelper::GetPool());
	DebugDumpCreateObjectCallStack(camEnvelope::GetPool());
	DebugDumpCreateObjectCallStack(camFrameInterpolator::GetPool(), false);
	DebugDumpCreateObjectCallStack(camBaseFrameShaker::GetPool());
	DebugDumpCreateObjectCallStack(camHintHelper::GetPool());
	DebugDumpCreateObjectCallStack(camInconsistentBehaviourZoomHelper::GetPool());
	DebugDumpCreateObjectCallStack(camLookAheadHelper::GetPool());
	DebugDumpCreateObjectCallStack(camLookAtDampingHelper::GetPool());
	DebugDumpCreateObjectCallStack(camStickyAimHelper::GetPool());
	DebugDumpCreateObjectCallStack(camOscillator::GetPool());
	DebugDumpCreateObjectCallStack(camSpringMount::GetPool());
	DebugDumpCreateObjectCallStack(camSplineNode::GetPool(), false);
	DebugDumpCreateObjectCallStack(camThirdPersonFrameInterpolator::GetPool(), false);
	cameraDisplayf("############################################################################");
	cameraAssertf(false, "Forced assert after dumping the camera pools call stacks. This is meant to stop the game output spamming the TTY while you analyze the callstacks. You can ignore this.");
}

void camFactory::DebugDumpCreateObjectCallStack(const fwBasePool* pool, bool isCamBaseObject)
{
	if(!pool)
	{
		return;
	}
	const s32 storageSize = (s32)pool->GetStorageSize();
	const s32 numObjects  = pool->GetNoOfUsedSpaces();
	const s32 maxObjects  = pool->GetSize();
	cameraDisplayf("############################################################################");
	cameraDisplayf("#### Dumping call stacks for pool %s %d byte(s) %d/%d", pool->GetName(), storageSize, numObjects, maxObjects);
	cameraDisplayf("############################################################################");

	if(!isCamBaseObject)
	{
		return;
	}

#if !__NO_OUTPUT
	for(s32 index = 0; index < maxObjects; ++index)
	{
		const camBaseObject* pObject = (const camBaseObject*)pool->GetSlot(index);
		if(pObject)
		{
			const char* className       = pObject->GetClassId().GetCStr();
			const u32   classNameHashed = pObject->GetClassId().GetHash();
			const char* instanceName    = pObject->GetName();
			cameraDisplayf("#### [%02d]: Class: %s (%u) Name %s", index, className ? className : "invalid", classNameHashed, instanceName ? instanceName : "invalid");
			pObject->PrintCollectedObjectStack();
		}
	}
#endif //!__NO_OUTPUT
}

#if PARSER_ALL_METADATA_HAS_NAMES
void camFactory::AddWidgetsForMetadata(bkBank& bank)
{
	if(ms_MetadataStore)
	{
		ms_MetadataWidgetGroup = bank.PushGroup("Metadata");
		{
			bank.AddButton("Save", datCallback(CFA(SaveMetadata)), "Save all camera metadata");
			bank.AddButton("Restart camera system", datCallback(CFA(camInterface::DebugShouldRestartSystemAtEndOfFrame)), "Shutdown and restart the camera system");

			bank.PushGroup("Directors", false);
			{
				const int numDirectors = ms_MetadataStore->m_DirectorList.GetCount();
				for(int i=0; i<numDirectors; i++)
				{
					camBaseDirectorMetadata* director = ms_MetadataStore->m_DirectorList[i];
					if(director)
					{
						bank.PushGroup(SAFE_CSTRING(director->m_Name.GetCStr()), false);
						{
							camBuildWidgetsVisitor widgets;
							widgets.m_CurrBank = &bank;
							widgets.Visit(*director);
						}
						bank.PopGroup(); //Director name
					}
				}
			}
			bank.PopGroup(); //Directors

			bank.PushGroup("Cameras and helpers", false);
			{
				const int numObjects = ms_MetadataStore->m_MetadataList.GetCount();
				for(int i=0; i<numObjects; i++)
				{
					camBaseObjectMetadata* object = ms_MetadataStore->m_MetadataList[i];
					if(object)
					{
						bank.PushGroup(SAFE_CSTRING(object->m_Name.GetCStr()), false);
						{
							camBuildWidgetsVisitor widgets;
							widgets.m_CurrBank = &bank;
							widgets.Visit(*object);
						}
						bank.PopGroup(); //Object name
					}
				}
			}
			bank.PopGroup(); //Cameras and helpers
		}
		bank.PopGroup(); //Metadata
	}
}

void camFactory::SaveMetadata()
{
	if(ms_MetadataStore)
	{
		sysMemStartDebug();

		char filename[RAGE_MAX_PATH];

		const char* alternativeMetadataPath = NULL;
		PARAM_rawCameraMetadata.Get(alternativeMetadataPath);

		if ((alternativeMetadataPath && alternativeMetadataPath[0] == '\0') || !alternativeMetadataPath)
		{
			formatf(filename, RAGE_MAX_PATH, "%s%s", CFileMgr::GetAssetsFolder(), g_CameraMetadataAssetsSubPath);
		}
		else if (alternativeMetadataPath)
		{
			formatf(filename, RAGE_MAX_PATH, "%s", alternativeMetadataPath);
		}

		ASSERT_ONLY(const bool hasSucceeded = )PARSER.SaveObject(filename, "", ms_MetadataStore, parManager::XML);
		cameraAssertf(hasSucceeded, "Failed to save the camera metadata to %s, is the file checked out?", filename);

		sysMemEndDebug();
	}
}

camWidgetStringProxy::camWidgetStringProxy(atHashString* hashString, const char* widgetString)
: m_HashString(hashString)
{
	cameraAssertf((int)strlen(widgetString) < (g_MaxWidgetStringProxyLength - 1),
		"The camera metadata reference \"%s\" is longer than %d characters and will be trimmed", widgetString, g_MaxWidgetStringProxyLength);
	safecpy(m_WidgetString, widgetString, g_MaxWidgetStringProxyLength);
}

void camWidgetStringProxy::OnChange()
{
	//Update the metadata hash string.
	if(m_HashString)
	{
		m_HashString->SetFromString(m_WidgetString);
	}
}

atArray<camWidgetStringProxy*> camBuildWidgetsVisitor::ms_ProxyStrings;

void camBuildWidgetsVisitor::ShutdownClass()
{
	const int numProxyStrings = ms_ProxyStrings.GetCount();
	for(int i=0; i<numProxyStrings; i++)
	{
		if(ms_ProxyStrings[i])
		{
			delete ms_ProxyStrings[i];
		}
	}

	ms_ProxyStrings.Reset();
}

void camBuildWidgetsVisitor::StringMember(parPtrToMember ptrToMember, const char* ptrToString, parMemberString& metadata)
{
	if((metadata.GetSubtype() == parMemberStringSubType::SUBTYPE_ATNONFINALHASHSTRING) && ptrToMember)
	{
		//Create a string proxy to enable this string to be edited.
		camWidgetStringProxy* stringProxy	= rage_new camWidgetStringProxy(reinterpret_cast<atHashString*>(ptrToMember),
			ptrToString ? ptrToString : "*NULL*");
		if(stringProxy)
		{
			ms_ProxyStrings.Grow() = stringProxy;

			m_CurrBank->AddText(metadata.GetName(), stringProxy->m_WidgetString, g_MaxWidgetStringProxyLength, false,
				datCallback(MFA(camWidgetStringProxy::OnChange), stringProxy));
		}
	}
	else
	{
		parBuildWidgetsVisitor::StringMember(ptrToMember, ptrToString, metadata);
	}
}
#endif // PARSER_ALL_METADATA_HAS_NAMES

#endif // __BANK
