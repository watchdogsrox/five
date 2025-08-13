#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include "atl/hashstring.h"
#include "atl/map.h"
#include "fwsys/config.h"
#include "system/ipc.h"			// sysIpcPriority

enum ConfigBool {
	CB_FALSE = 0,
	CB_TRUE = 1,
};

//-----------------------------------------------------------------------------

#define NUM_LINK_DENSITIES 16

struct CPopulationConfig
{
public:
	CPopulationConfig();

	void MergeData(const CPopulationConfig& mergeIn);

	int m_ScenarioPedsMultiplier_Base;
	int m_ScenarioPedsMultiplier;
	int m_AmbientPedsMultiplier_Base;
	int m_AmbientPedsMultiplier;
	int m_MaxTotalPeds_Base;
	int m_MaxTotalPeds;
	int m_PedMemoryMultiplier;
	int m_PedsForVehicles_Base;
	int m_PedsForVehicles;

	int m_VehicleTimesliceMaxUpdatesPerFrame_Base;
	int m_VehicleTimesliceMaxUpdatesPerFrame;
	int m_VehicleAmbientDensityMultiplier_Base;
	int m_VehicleAmbientDensityMultiplier;
	int m_VehicleMemoryMultiplier;
	int m_VehicleParkedDensityMultiplier_Base;
	int m_VehicleParkedDensityMultiplier;
	int m_VehicleLowPrioParkedDensityMultiplier_Base;
	int m_VehicleLowPrioParkedDensityMultiplier;
	int m_VehicleUpperLimit_Base;
	int m_VehicleUpperLimit;
	int m_VehicleUpperLimitMP;
	int m_VehicleParkedUpperLimit_Base;
	int m_VehicleParkedUpperLimit;
	int m_VehicleKeyholeShapeInnerThickness_Base;
	int m_VehicleKeyholeShapeInnerThickness;
	int m_VehicleKeyholeShapeOuterThickness_Base;
	int m_VehicleKeyholeShapeOuterThickness;
	int m_VehicleKeyholeShapeInnerRadius_Base;
	int m_VehicleKeyholeShapeInnerRadius;
	int m_VehicleKeyholeShapeOuterRadius_Base;
	int m_VehicleKeyholeShapeOuterRadius;
	int m_VehicleKeyholeSideWallThickness_Base;
	int m_VehicleKeyholeSideWallThickness;
	int m_VehicleMaxCreationDistance_Base;
	int m_VehicleMaxCreationDistance;
	int m_VehicleMaxCreationDistanceOffscreen_Base;
	int m_VehicleMaxCreationDistanceOffscreen;
	int m_VehicleCullRange_Base;
	int m_VehicleCullRange;
	int m_VehicleCullRangeOnScreenScale_Base;
	int m_VehicleCullRangeOnScreenScale;
	int m_VehicleCullRangeOffScreen_Base;
	int m_VehicleCullRangeOffScreen;
	int m_DensityBasedRemovalRateScale_Base;
	int m_DensityBasedRemovalRateScale;
	int m_DensityBasedRemovalTargetHeadroom_Base;
	int m_DensityBasedRemovalTargetHeadroom;
	int m_VehicleSpacing_Base[NUM_LINK_DENSITIES];
	int m_VehicleSpacing[NUM_LINK_DENSITIES];
	int m_PlayersRoadScanDistance_Base;
	int m_PlayersRoadScanDistance;
	int m_PlayerRoadDensityInc_Base[NUM_LINK_DENSITIES];
	int m_PlayerRoadDensityInc[NUM_LINK_DENSITIES];
	int m_NonPlayerRoadDensityDec_Base[NUM_LINK_DENSITIES];
	int m_NonPlayerRoadDensityDec[NUM_LINK_DENSITIES];
	int m_VehiclePopulationFrameRate_Base;
	int m_VehiclePopulationFrameRate;
	int m_VehiclePopulationCyclesPerFrame_Base;
	int m_VehiclePopulationCyclesPerFrame;
	int m_VehiclePopulationFrameRateMP_Base;
	int m_VehiclePopulationFrameRateMP;
	int m_VehiclePopulationCyclesPerFrameMP_Base;
	int m_VehiclePopulationCyclesPerFrameMP;
	int m_PedPopulationFrameRate_Base;
	int m_PedPopulationFrameRate;
	int m_PedPopulationCyclesPerFrame_Base;
	int m_PedPopulationCyclesPerFrame;
	int m_PedPopulationFrameRateMP_Base;
	int m_PedPopulationFrameRateMP;
	int m_PedPopulationCyclesPerFrameMP_Base;
	int m_PedPopulationCyclesPerFrameMP;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

struct C2dEffectConfig
{
public:
	C2dEffectConfig();

	void MergeData(const C2dEffectConfig& mergeIn);

	// PURPOSE:	Max number of CAudioAttr objects.
	// NOTES: Used to be NUM_AUDIO_ATTRS (280).
	int		m_MaxAttrsAudio;

	// PURPOSE:	Max number of CBuoyancyAttr objects.
	// NOTES: Used to be NUM_BUOYANCY_ATTRS (100).
	int		m_MaxAttrsBuoyancy;

	// PURPOSE:	Max number of CDecalAttr objects.
	int		m_MaxAttrsDecal;

	// PURPOSE:	Max number of CExplosionAttr objects.
	// NOTES: Used to be NUM_EXPLOSION_ATTRS (50).
	int		m_MaxAttrsExplosion;

	// PURPOSE:	Max number of CLadderInfo objects.
	// NOTES: Used to be NUM_LADDER_ATTRS (122).
	int		m_MaxAttrsLadder;

	// PURPOSE: Max number of CAudioCollisionInfo objects
	int		m_MaxAttrsAudioCollision;

	// PURPOSE:	Max number of CLightAttr objects.
	// NOTES: Used to be NUM_LIGHT_ATTRS (0).
	int		m_MaxAttrsLight;

	// PURPOSE:	Max number of CLightShaftAttr objects.
	// NOTES: Used to be NUM_LIGHTSHAFT_ATTRS (600).
	int		m_MaxAttrsLightShaft;

	// PURPOSE:	Max number of CParticleAttr objects.
	// NOTES: Used to be NUM_PARTICLE_ATTRS (2500).
	int		m_MaxAttrsParticle;

	// PURPOSE:	Max number of CProcObjAttr objects.
	// NOTES: Used to be NUM_PROCOBJ_ATTRS (100).
	int		m_MaxAttrsProcObj;

	// PURPOSE:	Max number of CScrollBarAttr objects.
	// NOTES: Used to be NUM_SCROLLBAR_ATTRS (35).
	int		m_MaxAttrsScrollBar;

	// PURPOSE:	Max number of CSpawnPoint objects.
	// NOTES: Used to be NUM_SPAWNPOINT_ATTRS (10900).
	int		m_MaxAttrsSpawnPoint;

	// PURPOSE:	Max number of CSwayableAttr objects.
	// NOTES: Used to be NUM_SWAYABLE_ATTRS (20).
	int		m_MaxAttrsSwayable;

	// PURPOSE:	Max number of CWalkDontWalkAttr objects.
	// NOTES: Used to be NUM_WALKDONTWALK_ATTRS (30).
	int		m_MaxAttrsWalkDontWalk;

	// PURPOSE:	Max number of CWindDisturbanceAttr objects.
	int		m_MaxAttrsWindDisturbance;

	// PURPOSE:	Max number of CWorldPointAttr objects.
	// NOTES: Used to be NUM_WORLDPOINT_ATTRS (250).
	int		m_MaxAttrsWorldPoint;

	// PURPOSE:	Max number of C2dEffect objects in g2dEffectStore.
	// NOTES: Used to be NUM_2D_EFFECTS (13250).
	int		m_MaxEffects2d;

	// PURPOSE:	Max number of C2dEffect objects in gWorld2dEffectStore.
	// NOTES: Used to be NUM_WORLD_2D_EFFECTS (10900).
	int		m_MaxEffectsWorld2d;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Configuration data related to CModelInfo.
*/
struct CModelInfoConfig
{
public:
	CModelInfoConfig();

	void MergeData(const CModelInfoConfig& mergeIn);

	// PURPOSE: default player model name
	// NOTES: name of ped model to use as default player construction
	ConstString	m_defaultPlayerName;

	// PURPOSE: default player model name during the playgo prologue
	// NOTES: name of ped model to use as default player construction during playgo prologue
	ConstString	m_defaultProloguePlayerName;

	// PURPOSE: Max number of composite entity model infos.
	// NOTES:	Used to be NUM_COMP_ENTITY_MODEL_INFOS (50).
	int		m_MaxBaseModelInfos;

	// PURPOSE: Max number of composite entity model infos.
	// NOTES:	Used to be NUM_COMP_ENTITY_MODEL_INFOS (50).
	int		m_MaxCompEntityModelInfos;

	// PURPOSE:	Max number of mlo instances.
	// NOTES:	Used to be NUM_MLO_ISNTANCES (15000)
	int		m_MaxMloInstances;

	// PURPOSE: Max number of mlo model infos.
	// NOTES:	Used to be NUM_MLO_MODEL_INFOS (220).
	int		m_MaxMloModelInfos;

	// PURPOSE: Max number of pedestrian object types.
	// NOTES:	Used to be NUM_PED_MODEL_INFOS (350).
	int		m_MaxPedModelInfos;

	// PURPOSE: Max number of time object types.
	// NOTES:	Used to be NUM_TIME_MODEL_INFOS (1800).
	int		m_MaxTimeModelInfos;

	// PURPOSE: Max number of vehicle object types.
	// NOTES:	Used to be NUM_VEHICLE_MODEL_INFOS (256).
	int		m_MaxVehicleModelInfos;

	// PURPOSE: Max number of weapon object types, also includes ambient props.
	// NOTES:	Used to be NUM_WEAPON_MODEL_INFOS (100).
	int		m_MaxWeaponModelInfos;

	// PURPOSE: Max number of extra ped object types.
	int		m_MaxExtraPedModelInfos;

	// PURPOSE: Max number of extra vehicle object types.
	int		m_MaxExtraVehicleModelInfos;

	// PURPOSE: Max number of extra weapon object types.
	int		m_MaxExtraWeaponModelInfos;
	
	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

struct CExtensionConfig
{
public:
	CExtensionConfig();

	void MergeData(const CExtensionConfig& mergeIn);

	// PURPOSE:	Max number of CDoorExtension objects.
	int		m_MaxDoorExtensions;

	// PURPOSE:	Max number of CLightExtension objects
	int		m_MaxLightExtensions;

	// PURPOSE:	Max number of CSpawnPointOverrideExtension objects
	int		m_MaxSpawnPointOverrideExtensions;

	// PURPOSE:	Max number of CExpressionExtension objects
	int		m_MaxExpressionExtensions;

	// PURPOSE: Max number of fwClothCollisionsExtension objects ( composite bounds )
	int		m_MaxClothCollisionsExtensions;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Part of CGameConfig which holds some values for the configuration of the
	core streaming engine.
*/
struct CConfigStreamingEngine
{
public:
	// PURPOSE:	Constructor, sets up default values if we want any.
	CConfigStreamingEngine();

	// PURPOSE:	Merge in override values from another configuration object.
	// PARAMS:	mergeIn		- Reference to configuration values to merge in from.
	void MergeData(const CConfigStreamingEngine& mergeIn);

	// PURPOSE:	Max archive count, passed in to strStreamingEngine::InitClass().
	// NOTES:	Used to be ARCHIVE_COUNT in 'streaming/streaming.cpp'.
	int		m_ArchiveCount;

	// PURPOSE:	The size of the physical streaming buffer, passed in to
	//			strStreamingEngine::InitClass(), in kB.
	// NOTES:	Used to be PHYSICAL_STREAMING_BUFFER in 'streaming/streaming.cpp'.
	int		m_PhysicalStreamingBuffer;

	// PURPOSE:	The size of the virtual streaming buffer, passed in to
	//			strStreamingEngine::InitClass(), in kB.
	// NOTES:	Used to be VIRTUAL_STREAMING_BUFFER in 'streaming/streaming.cpp'.
	int		m_VirtualStreamingBuffer;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Part of CGameConfig which holds some values for the configuration for the
	Rockstar Online Services.
*/
struct CConfigOnlineServices
{
public:
	// PURPOSE:	Constructor, sets up default values if we want any.
	CConfigOnlineServices();

	// PURPOSE:	Merge in override values from another configuration object.
	// PARAMS:	mergeIn		- Reference to configuration values to merge in from.
	void MergeData(const CConfigOnlineServices& mergeIn);

    // PURPOSE: rockstar online service game name
    ConstString m_RosTitleName;

	// PURPOSE: rockstar online service title version assigned by RAGE Team
	int m_RosTitleVersion;

	// PURPOSE: rockstar online service social club version assigned by RAGE Team
	int m_RosScVersion;

	// PURPOSE: steam application id for rockstar online services
	int m_SteamAppId;

	// PURPOSE: Title Directory for PC save games
	ConstString m_TitleDirectoryName;

	//  PURPOSE: the multiplayer session template name to use when creating
	//  multiplayer sessions on the Xbox One MPSD (multiplayer session directory). It is
	//  configured via the XDP (Xbox Developer Portal).
	ConstString m_MultiplayerSessionTemplateName;

	// PURPOSE: friendly rockstar title name used by ScAuth's client ID in user facing urls
	ConstString m_ScAuthTitleId;

	PAR_SIMPLE_PARSABLE;
};


struct CConfigUGCDescriptions
{
public:

	CConfigUGCDescriptions();

	void MergeData(const CConfigUGCDescriptions& mergeIn);

	int m_MaxDescriptionLength;
	int m_MaxNumDescriptions;
	int m_SizeOfDescriptionBuffer;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

// PURPOSE:	This specifies the size of the broadcast data and the maximum number of players the data is synced with.
struct CBroadcastDataEntry
{
	// PURPOSE:	This specifies the size of the broadcast data in bytes
	int		m_SizeOfData;

	// PURPOSE:	The maximum number of players the data is synced with
	int		m_MaxParticipants;

	// PURPOSE:	The number of instances of this broadcast data
	int		m_NumInstances;

	PAR_SIMPLE_PARSABLE;
};

/*
PURPOSE
	Part of CGameConfig which holds the sizes of the preallocated script broadcast 
	data.
*/

struct CConfigNetScriptBroadcastData
{
public:

	static const unsigned NUM_BROADCAST_DATA_ENTRIES = 10;

public:

	CConfigNetScriptBroadcastData();

	void MergeData(const CConfigNetScriptBroadcastData& mergeIn);

	// PURPOSE:	An array of predefined sizes of script host broadcast data
	atFixedArray<CBroadcastDataEntry, NUM_BROADCAST_DATA_ENTRIES>	m_HostBroadcastData;

	// PURPOSE:	An array of predefined sizes of script local player broadcast data
	atFixedArray<CBroadcastDataEntry, NUM_BROADCAST_DATA_ENTRIES>	m_PlayerBroadcastData;

	PAR_SIMPLE_PARSABLE;
};

struct CScriptStackSizeDataEntry
{
	atHashValue m_StackName;

	int m_SizeOfStack;

	int m_NumberOfStacksOfThisSize;

	PAR_SIMPLE_PARSABLE;
};

struct CConfigScriptStackSizes
{
//	CConfigScriptStackSizes();

	void MergeData(const CConfigScriptStackSizes& mergeIn);

	atArray<CScriptStackSizeDataEntry> m_StackSizeData;

	PAR_SIMPLE_PARSABLE;
};

struct CScriptResourceExpectedMaximum
{
	atHashValue m_ResourceTypeName;

	int m_ExpectedMaximum;

	PAR_SIMPLE_PARSABLE;
};

struct CConfigScriptResourceExpectedMaximums
{
	void MergeData(const CConfigScriptResourceExpectedMaximums& mergeIn);

	atArray<CScriptResourceExpectedMaximum> m_ExpectedMaximumsArray;

	PAR_SIMPLE_PARSABLE;
};


struct CScriptTextLines
{
	atHashValue m_NameOfScriptTextLine;

	s32 m_MaximumNumber;

	PAR_SIMPLE_PARSABLE;
};

struct CConfigScriptTextLines
{
	void MergeData(const CConfigScriptTextLines& mergeIn);

	atArray<CScriptTextLines> m_ArrayOfMaximumTextLines;

	PAR_SIMPLE_PARSABLE;
};

struct CThreadSetup 
{
	CThreadSetup();

	void MergeData(const CThreadSetup &mergeIn);

	sysIpcPriority GetPriority(sysIpcPriority defaultPriority) const;

	u32 GetCpuAffinityMask(u32 defaultAffinity) const;


	int m_Priority;
	atArray<int> m_CpuAffinity;


	PAR_SIMPLE_PARSABLE;
};

struct CConfigMediaTranscoding
{
	CConfigMediaTranscoding();

	void MergeData(const CConfigMediaTranscoding &mergeIn);

	// PURPOSE:	The size of the transcoding buffer for small objects, passed in to
	//			MediaTranscodingAllocator::Init(), in kB.
	int		m_TranscodingSmallObjectBuffer;
    int		m_TranscodingSmallObjectMaxPointers;

	// PURPOSE:	The size of the transcoding buffer, passed in to
	//			MediaTranscodingAllocator::Init(), in kB.
    int		m_TranscodingBuffer;
    int		m_TranscodingMaxPointers;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	CGameConfig holds various configuration values for the game engine, such
	as thread priorities and pool sizes (supported by the fwConfig base class).
	It is meant to be loaded from an XML file by a call to
	fwConfigManagerImpl<CGameConfig>::InitClass(), which also supports overrides
	based on platform and build type.

	Size overrides for fwPools are done by name, so no need to change any code
	here for that, but other values such as thread priorities or limits for other
	types of pools can be added here, and then accessed by systems using
	CGameConfig::Get().

	To add new variables, you will need to modify the structdef parse declaration
	in 'config.cpp'. You will also need some way of indicating that a variable value
	should be inherited, such as -1 if the only valid values are positive, and
	make MergeNonPoolData() support it - this should be the init value you specify
	in the structdef.

NOTES
	If you add something here, you may want to consider moving it up to fwConfig,
	if it's something that's relevant for a system at the framework level.

EXAMPLES
	Change the render thread priority:
		<ThreadPriorityRender>PRIO_ABOVE_NORMAL</ThreadPriorityRender>
*/
struct CGameConfig : public fwConfig
{
	// PURPOSE:	Used as a marker to indicate that a thread priority should not
	//			be overridden by some other value.
	static const int PRIO_INHERIT = -1000;

	// PURPOSE:	Constructor, normally called through fwConfigManagerImpl.
	CGameConfig();

	// PURPOSE:	Get the current game configuration.
	// RETURNS:	fwConfigManager's fwConfig object, cast to a CGameConfig.
	static const CGameConfig &Get()
	{	return static_cast<const CGameConfig&>(fwConfigManager::GetInstance().GetConfig());	}

	virtual void MergeNonPoolData(const fwConfig& mergeIn);

	virtual fwConfig* CreateClone() const
	{
		return rage_new CGameConfig(*this);
	}

    const ConstString& GetDebugScriptsPath() const { return m_DebugScriptsPath; }

	const CPopulationConfig& GetConfigPopulation() const
	{	return m_ConfigPopulation;	}

	const C2dEffectConfig& GetConfig2dEffects() const
	{	return m_Config2dEffects;	}

	const CModelInfoConfig& GetConfigModelInfo() const
	{	return m_ConfigModelInfo;	}

	const CExtensionConfig& GetConfigExtensions() const
	{	return m_ConfigExtensions;	}

	const CConfigStreamingEngine& GetConfigStreamingEngine() const
	{	return m_ConfigStreamingEngine;	}

    const CConfigOnlineServices& GetConfigOnlineServices() const
    {	return m_ConfigOnlineServices;	}

	const CConfigNetScriptBroadcastData& GetConfigNetScriptBroadcastData() const
	{	return m_ConfigNetScriptBroadcastData;	}

	const CConfigScriptStackSizes& GetConfigScriptStackSizes() const
	{	return m_ConfigScriptStackSizes;	}

	const CConfigUGCDescriptions& GetConfigUGCDescriptions() const
	{	return m_ConfigUGCDescriptions;	}

	const CConfigMediaTranscoding& GetConfigMediaTranscoding() const
	{	return m_ConfigMediaTranscoding;	}

#if !__FINAL
	s32 GetConfigMaxNumberOfScriptResource(const char *pResourceName) const;
#endif	//	!__FINAL

	s32 GetConfigMaxNumberOfScriptTextLines(const char *pNameOfTextLine) const;

	// -- Thread priority accessors --
	const CThreadSetup *GetThreadSetup(atHashString threadNameHash) const;
	sysIpcPriority GetThreadPriority(atHashString threadNameHash, sysIpcPriority defaultPriority) const;


	// -- Toggle Burnout Textures --
	bool UseVehicleBurnoutTexture() const { return m_UseVehicleBurnoutTexture != CB_FALSE; }

	// -- Is crouched movement allowed? --
	bool AllowCrouchedMovement() const { return m_AllowCrouchedMovement != CB_FALSE; }
	
	// -- Is parachuting allowed? --
	bool AllowParachuting() const { return m_AllowParachuting != CB_FALSE; }

	// -- Is stealth movement allowed? --
	bool AllowStealthMovement() const { return m_AllowStealthMovement != CB_FALSE; }

private:
	static void ThreadPriorityOverrideCb(u32 threadNameHash, sysIpcPriority &priority, u32 &cpuAffinityMask);

protected:
	CPopulationConfig		m_ConfigPopulation;

	C2dEffectConfig			m_Config2dEffects;

	// PURPOSE:	Configuration data related to CModelInfo.
	CModelInfoConfig		m_ConfigModelInfo;

	// PURPOSE:	Configuration data related to extensions.
	CExtensionConfig		m_ConfigExtensions;

	// PURPOSE:	Configuration of some values used by CStreaming.
	CConfigStreamingEngine	m_ConfigStreamingEngine;
	
    // PURPOSE:	Configuration of some values used by CLiveManager.
    CConfigOnlineServices	m_ConfigOnlineServices;

	// PURPOSE: Store the sizes of the UGN descriptions 
	CConfigUGCDescriptions m_ConfigUGCDescriptions;

	// PURPOSE:	Configuration data related to network script broadcast data.
	CConfigNetScriptBroadcastData	m_ConfigNetScriptBroadcastData;

	// PURPOSE:	Holds the number and size of all stacks available to scripts
	CConfigScriptStackSizes m_ConfigScriptStackSizes;

	// PURPOSE: (Not used in __FINAL builds) For each type of script resource, holds the maximum number of instances that can exist before the game will assert
	CConfigScriptResourceExpectedMaximums m_ConfigScriptResourceExpectedMaximums;

	// PURPOSE: Store the sizes of the arrays of different types of script text (No substrings, one number, one substring etc.)
	CConfigScriptTextLines m_ConfigScriptTextLines;

	// PURPOSE: Store details for configuring media transcoding
	CConfigMediaTranscoding m_ConfigMediaTranscoding;

	// PURPOSE: Setup instructions for all threads.
	atMap<atHashString, CThreadSetup> m_Threads;

	// PURPOSE: Toggle the functionality used for vehicle burnout textures 
	int		m_UseVehicleBurnoutTexture;

	// PURPOSE: Toggle the functionality used for crouched movement 
	int		m_AllowCrouchedMovement;
	
	// PURPOSE: Toggle the functionality used for parachuting
	int		m_AllowParachuting;

	// PURPOSE: Toggle the functionality used for stealth movement
	int		m_AllowStealthMovement;

    // PURPOSE: A full path to where the debug scripts are located ie( DRIVE:\PROJECT\script\CONFIG\singleplayer\sco\DEBUG\)
    ConstString m_DebugScriptsPath;

    PAR_PARSABLE;
};

//-----------------------------------------------------------------------------

#endif	// GAME_CONFIG_H

// End of file game/config.h
