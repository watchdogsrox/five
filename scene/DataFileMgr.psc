<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="DataFileType">
	<enumval name="RPF_FILE"/>
	<enumval name="IDE_FILE"/>
	<enumval name="DELAYED_IDE_FILE"/>
	<enumval name="IPL_FILE"/>
	<enumval name="PERMANENT_ITYP_FILE"/>
	<enumval name="BENDABLEPLANTS_FILE"/>
	<enumval name="HANDLING_FILE"/>
	<enumval name="VEHICLEEXTRAS_FILE"/>
	<enumval name="GXT_FOLDER"/>
	<enumval name="PEDSTREAM_FILE"/>
	<enumval name="CARCOLS_FILE"/>
	<enumval name="POPGRP_FILE"/>
	<enumval name="PEDGRP_FILE"/>
	<enumval name="CARGRP_FILE"/>
	<enumval name="POPSCHED_FILE"/>
	<enumval name="ZONEBIND_FILE"/>
	<enumval name="RADIO_FILE"/>
	<enumval name="RADAR_BLIPS_FILE"/>
	<enumval name="THROWNWEAPONINFO_FILE"/>
	<enumval name="RMPTFX_FILE"/>
	<enumval name="PED_PERSONALITY_FILE"/>
	<enumval name="PED_PERCEPTION_FILE"/>
	<enumval name="VEHICLE_CAMERA_OFFSETS_FILE"/>
	<enumval name="FRONTEND_MENU_FILE"/>
	<enumval name="LEADERBOARD_DATA_FILE"/>
	<enumval name="LEADERBOARD_ICONS_FILE"/>
	<enumval name="NETWORKOPTIONS_FILE"/>
	<enumval name="TIMECYCLE_FILE"/>
	<enumval name="TIMECYCLEMOD_FILE"/>
	<enumval name="WEATHER_FILE"/>
	<enumval name="PLSETTINGSMALE_FILE"/>
	<enumval name="PLSETTINGSFEMALE_FILE"/>
	<enumval name="PROCOBJ_FILE"/>
	<enumval name="PROC_META_FILE"/>
	<enumval name="VFX_SETTINGS_FILE"/>
	<enumval name="SP_STATS_DISPLAY_LIST_FILE"/>
	<enumval name="MP_STATS_DISPLAY_LIST_FILE"/>
	<enumval name="PED_VARS_FILE"/>
	<enumval name="DISABLE_FILE"/>
	<!-- not a file type, but a command to remove specified IPL from data file mgr -->
	<enumval name="HUDCOLOR_FILE"/>
	<enumval name="HUD_TXD_FILE"/>
	<enumval name="FRONTEND_DAT_FILE"/>
	<enumval name="SCROLLBARS_FILE"/>
	<enumval name="TIME_FILE"/>
	<enumval name="BLOODFX_FILE"/>
	<enumval name="ENTITYFX_FILE"/>
	<enumval name="EXPLOSIONFX_FILE"/>
	<enumval name="MATERIALFX_FILE"/>
	<enumval name="MOTION_TASK_DATA_FILE"/>
	<enumval name="DEFAULT_TASK_DATA_FILE"/>
	<enumval name="MOUNT_TUNE_FILE"/>
	<enumval name="PED_BOUNDS_FILE"/>
	<enumval name="PED_HEALTH_FILE"/>
	<enumval name="PED_COMPONENT_SETS_FILE"/>
	<enumval name="PED_IK_SETTINGS_FILE"/>
	<enumval name="PED_TASK_DATA_FILE"/>
	<enumval name="PED_SPECIAL_ABILITIES_FILE"/>
	<enumval name="WHEELFX_FILE"/>
	<enumval name="WEAPONFX_FILE"/>
	<enumval name="DECALS_FILE"/>
	<enumval name="NAVMESH_INDEXREMAPPING_FILE"/>
	<enumval name="NAVNODE_INDEXREMAPPING_FILE"/>
	<enumval name="AUDIOMESH_INDEXREMAPPING_FILE"/>
	<enumval name="JUNCTION_TEMPLATES_FILE"/>
  <enumval name="PATH_ZONES_FILE"/>
	<enumval name="DISTANT_LIGHTS_FILE"/>
	<enumval name="DISTANT_LIGHTS_HD_FILE"/>
	<enumval name="FLIGHTZONES_FILE"/>
	<enumval name="WATER_FILE"/>
	<enumval name="TRAINCONFIGS_FILE"/>
	<enumval name="TRAINTRACK_FILE"/>
	<enumval name="PED_METADATA_FILE"/>
	<enumval name="WEAPON_METADATA_FILE"/>
	<enumval name="VEHICLE_METADATA_FILE"/>
	<enumval name="DISPATCH_DATA_FILE"/>
	<enumval name="DEFORMABLE_OBJECTS_FILE"/>
	<enumval name="TUNABLE_OBJECTS_FILE"/>
	<enumval name="PED_NAV_CAPABILITES_FILE"/>
	<enumval name="WEAPONINFO_FILE"/>
	<enumval name="WEAPONCOMPONENTSINFO_FILE"/>
	<enumval name="LOADOUTS_FILE"/>
	<enumval name="FIRINGPATTERNS_FILE"/>
	<enumval name="MOTIVATIONS_FILE"/>
	<enumval name="SCENARIO_POINTS_FILE"/>
	<enumval name="SCENARIO_POINTS_PSO_FILE"/>
	<enumval name="STREAMING_FILE"/>
	<enumval name="STREAMING_FILE_PLATFORM_PS3"/>
	<enumval name="STREAMING_FILE_PLATFORM_XENON"/>
	<enumval name="STREAMING_FILE_PLATFORM_OTHER"/>
	<enumval name="PED_BRAWLING_STYLE_FILE"/>
	<enumval name="AMBIENT_PED_MODEL_SET_FILE"/>
	<enumval name="AMBIENT_PROP_MODEL_SET_FILE"/>
	<enumval name="AMBIENT_VEHICLE_MODEL_SET_FILE"/>
	<enumval name="LADDER_METADATA_FILE"/>
	<enumval name="HEIGHTMESH_INDEXREMAPPING_FILE"/>
	<enumval name="SLOWNESS_ZONES_FILE"/>
	<enumval name="LIQUIDFX_FILE"/>
	<enumval name="VFXVEHICLEINFO_FILE"/>
	<enumval name="VFXPEDINFO_FILE"/>
	<enumval name="DOOR_TUNING_FILE"/>
	<enumval name="PTFXASSETINFO_FILE"/>
	<enumval name="SCRIPTFX_FILE"/>
	<enumval name="VFXREGIONINFO_FILE"/>
	<enumval name="VFXINTERIORINFO_FILE"/>
	<enumval name="CAMERA_METADATA_FILE"/>
	<enumval name="STREET_VEHICLE_ASSOCIATION_FILE"/>
	<enumval name="VFXWEAPONINFO_FILE"/>
	<enumval name="EXPLOSION_INFO_FILE"/>
	<enumval name="JUNCTION_TEMPLATES_PSO_FILE"/>
	<enumval name="MAPZONES_FILE"/>
	<enumval name="SP_STATS_UI_LIST_FILE"/>
	<enumval name="MP_STATS_UI_LIST_FILE"/>
	<enumval name="OBJ_COVER_TUNING_FILE"/>
	<enumval name="STREAMING_REQUEST_LISTS_FILE"/>
	<enumval name="PLAYER_CARD_SETUP"/>
	<enumval name="WORLD_HEIGHTMAP_FILE"/>
	<enumval name="WORLD_WATERHEIGHT_FILE"/>
	<enumval name="PED_OVERLAY_FILE"/>
	<enumval name="WEAPON_ANIMATIONS_FILE"/>
	<enumval name="VEHICLE_POPULATION_FILE"/>
	<enumval name="ACTION_TABLE_DEFINITIONS"/>
	<enumval name="ACTION_TABLE_RESULTS"/>
	<enumval name="ACTION_TABLE_IMPULSES"/>
	<enumval name="ACTION_TABLE_RUMBLES"/>
	<enumval name="ACTION_TABLE_INTERRELATIONS"/>
	<enumval name="ACTION_TABLE_HOMINGS"/>
	<enumval name="ACTION_TABLE_DAMAGES"/>
	<enumval name="ACTION_TABLE_STRIKE_BONES"/>
	<enumval name="ACTION_TABLE_BRANCHES"/>
	<enumval name="ACTION_TABLE_STEALTH_KILLS"/>
	<enumval name="ACTION_TABLE_VFX"/>
	<enumval name="ACTION_TABLE_FACIAL_ANIM_SETS"/>
	<enumval name="VEHGEN_MARKUP_FILE"/>
	<enumval name="PED_COMPONENT_CLOTH_FILE"/>    
	<enumval name="TATTOO_SHOP_DLC_FILE"/>
  	<enumval name="VEHICLE_VARIATION_FILE"/>
  	<enumval name="CONTENT_UNLOCKING_META_FILE"/>
	<enumval name="SHOP_PED_APPAREL_META_FILE"/>
	<enumval name="AUDIO_SOUNDDATA"/>
	<enumval name="AUDIO_CURVEDATA"/>
	<enumval name="AUDIO_GAMEDATA"/>
	<enumval name="AUDIO_DYNAMIXDATA"/>
	<enumval name="AUDIO_SPEECHDATA"/>
	<enumval name="AUDIO_SYNTHDATA"/>
	<enumval name="AUDIO_WAVEPACK"/>
  	<enumval name="CLIP_SETS_FILE"/>
	<enumval name="EXPRESSION_SETS_FILE"/>
  	<enumval name="FACIAL_CLIPSET_GROUPS_FILE"/>
  	<enumval name="NM_BLEND_OUT_SETS_FILE"/>
	<enumval name="VEHICLE_SHOP_DLC_FILE"/>
	<enumval name="WEAPON_SHOP_INFO_METADATA_FILE"/>
	<enumval name="SCALEFORM_PREALLOC_FILE"/>
	<enumval name="CONTROLLER_LABELS_FILE"/>
	<enumval name="CONTROLLER_LABELS_FILE_360"/>
	<enumval name="CONTROLLER_LABELS_FILE_PS3"/>
	<enumval name="CONTROLLER_LABELS_FILE_PS3_JPN"/>
	<enumval name="CONTROLLER_LABELS_FILE_ORBIS"/>
	<enumval name="CONTROLLER_LABELS_FILE_ORBIS_JPN"/>
	<enumval name="CONTROLLER_LABELS_FILE_DURANGO"/>
  <enumval name="IPLCULLBOX_METAFILE"/>
	<enumval name="TEXTFILE_METAFILE"/>
	<enumval name="NM_TUNING_FILE"/>
    <enumval name="MOVE_NETWORK_DEFS"/>
    <enumval name="WEAPONINFO_FILE_PATCH"/>
	  <enumval name="DLC_SCRIPT_METAFILE"/>
  	<enumval name="VEHICLE_LAYOUTS_FILE"/>
    <enumval name="DLC_WEAPON_PICKUPS"/>  
    <enumval name="EXTRA_TITLE_UPDATE_DATA"/>
	  <enumval name="SCALEFORM_DLC_FILE"/>
	  <enumval name="OVERLAY_INFO_FILE"/>
    <enumval name="ALTERNATE_VARIATIONS_FILE"/>
    <enumval name="HORSE_REINS_FILE"/>
    <enumval name="FIREFX_FILE"/>
    <enumval name="INTERIOR_PROXY_ORDER_FILE"/>
    <enumval name="DLC_ITYP_REQUEST"/>
    <enumval name="EXTRA_FOLDER_MOUNT_DATA"/>
    <enumval name="BLOODFX_FILE_PATCH"/>
    <enumval name="SCRIPT_BRAIN_FILE"/>
	<enumval name="SCALEFORM_VALID_METHODS_FILE"/>
    <enumval name="DLC_POP_GROUPS"/>
    <enumval name="POPGRP_FILE_PATCH"/>
	<enumval name="SCENARIO_INFO_FILE"/>
  	<enumval name="CONDITIONAL_ANIMS_FILE"/>
    <enumval name="STATS_METADATA_PSO_FILE"/>
    <enumval name="VFXFOGVOLUMEINFO_FILE"/>
    <enumval name="RPF_FILE_PRE_INSTALL"/>
    <enumval name="RPF_FILE_PRE_INSTALL_ONLY"/>
	<enumval name="LEVEL_STREAMING_FILE"/>
	<enumval name="SCENARIO_POINTS_OVERRIDE_FILE"/>
	<enumval name="SCENARIO_POINTS_OVERRIDE_PSO_FILE"/>
	<enumval name="SCENARIO_INFO_OVERRIDE_FILE"/>
	<enumval name="PED_FIRST_PERSON_ASSET_DATA"/>
   <enumval name="GTXD_PARENTING_DATA"/>
   <enumval name="COMBAT_BEHAVIOUR_OVERRIDE_FILE"/>
    <enumval name="EVENTS_OVERRIDE_FILE"/>
    <enumval name="PED_DAMAGE_OVERRIDE_FILE"/>
    <enumval name="PED_DAMAGE_APPEND_FILE"/>
    <enumval name="BACKGROUND_SCRIPT_FILE"/>
    <enumval name="PS3_SCRIPT_RPF"/>
    <enumval name="X360_SCRIPT_RPF"/>
	  <enumval name="PED_FIRST_PERSON_ALTERNATE_DATA"/>
    <enumval name="ZONED_ASSET_FILE"/>
    <enumval name="ISLAND_HOPPER_FILE"/>
    <enumval name="CARCOLS_GEN9_FILE"/>
    <enumval name="CARMODCOLS_GEN9_FILE"/>
    <enumval name="COMMUNITY_STATS_FILE"/>
    <enumval name="GEN9_EXCLUSIVE_ASSETS_VEHICLES_FILE"/>
    <enumval name="GEN9_EXCLUSIVE_ASSETS_PEDS_FILE"/>
  </enumdef>

  <enumdef type="DataFileContents">
	<enumval name="CONTENTS_DEFAULT"/>
	<enumval name="CONTENTS_PROPS"/>
	<enumval name="CONTENTS_MAP"/>
	<enumval name="CONTENTS_LODS"/>
	<enumval name="CONTENTS_PEDS"/>
	<enumval name="CONTENTS_VEHICLES"/>
	<enumval name="CONTENTS_ANIMATION"/>
	<enumval name="CONTENTS_CUTSCENE"/>
	<enumval name="CONTENTS_DLC_MAP_DATA"/>
	<enumval name="CONTENTS_DEBUG_ONLY"/>
	<enumval name="CONTENTS_MAX"/>
  </enumdef>

  <enumdef type="InstallPartition">
	<enumval name="PARTITION_NONE" value="-1"/>
	<enumval name="PARTITION_0" value="0"/>
	<enumval name="PARTITION_1"/>
	<enumval name="PARTITION_2"/>
	<enumval name="PARTITION_MAX"/>
  </enumdef>

  <enumdef type="LoadingScreenContext">
  <enumval name="LOADINGSCREEN_CONTEXT_NONE" value="0"/>
  <enumval name="LOADINGSCREEN_CONTEXT_INTRO_MOVIE"/>
  <enumval name="LOADINGSCREEN_CONTEXT_LEGALSPLASH"/>
  <enumval name="LOADINGSCREEN_CONTEXT_LEGALMAIN"/>
  <enumval name="LOADINGSCREEN_CONTEXT_SWAP"/>
  <enumval name="LOADINGSCREEN_CONTEXT_PC_LANDING"/>
  <enumval name="LOADINGSCREEN_CONTEXT_LOADGAME"/>
  <enumval name="LOADINGSCREEN_CONTEXT_INSTALL"/>
  <enumval name="LOADINGSCREEN_CONTEXT_LOADLEVEL"/>
  <enumval name="LOADINGSCREEN_CONTEXT_MAPCHANGE"/>
  <enumval name="LOADINGSCREEN_CONTEXT_LAST_FRAME"/>
  </enumdef>
  
  <structdef type="CDataFileMgr::DataFile">
	<string name="m_filename" type="member" size="128" ui_key="true"/>
	<enum name="m_fileType" type="DataFileType" init="-1"/>
  <string name="m_registerAs" type="atString"/>
  <bool name="m_locked"/>
  <bool name="m_loadCompletely"/>
	  <bool name="m_overlay"/>
	  <bool name="m_patchFile"/>
  <bool name="m_disabled"/>
  <bool name="m_persistent"/>
  <bool name="m_enforceLsnSorting" init="true"/>
  <enum name="m_contents" type="DataFileContents"/>
	<enum name="m_installPartition" type="InstallPartition"/>
  </structdef>

  <structdef type="CDataFileMgr::DataFileArray">
	<array name="m_dataFiles" type="atArray">
	  <struct type="CDataFileMgr::DataFile"/>
	</array>
  </structdef>

  <structdef type="CDataFileMgr::ResourceReference">
    <string name="m_AssetName" type="atString" />
    <string name="m_Extension" type="member" size="8" />
  </structdef>

  <structdef type="CDataFileMgr::ChangeSetData">
    <string name="m_associatedMap" type="atString"/>

    <array name="m_filesToInvalidate" type="atArray">
      <string type="atString"/>
    </array>

    <array name="m_filesToDisable" type="atArray">
      <string type="atString"/>
    </array>

    <array name="m_filesToEnable" type="atArray">
      <string type="atString"/>
    </array>

    <array name="m_txdToLoad" type="atArray">
      <string type="atHashString"/>
    </array>

    <array name="m_txdToUnload" type="atArray">
      <string type="atHashString"/>
    </array>

    <array name="m_residentResources" type="atArray">
      <struct type="CDataFileMgr::ResourceReference" />
    </array>

    <array name="m_unregisterResources" type="atArray">
      <struct type="CDataFileMgr::ResourceReference" />
    </array>
  </structdef>

	<structdef type="ExecutionCondition">
		<string name="m_name" type="atHashString"/>
		<bool name="m_condition"/>
	</structdef>
	<structdef type="ExecutionConditions">
		<array name="m_activeChangesetConditions" type="atArray">
			<struct type="ExecutionCondition"/>
		</array>
		<string name="m_genericConditions" type="atFinalHashString"/>
	</structdef>
  <structdef type="CDataFileMgr::ContentChangeSet">
    <string name="m_changeSetName" type="atString" />

    <array name="m_mapChangeSetData" type="atArray">
      <struct type="CDataFileMgr::ChangeSetData"/>
    </array>

    <array name="m_filesToInvalidate" type="atArray">
      <string type="atString"/>
    </array>

    <array name="m_filesToDisable" type="atArray">
      <string type="atString"/>
    </array>

    <array name="m_filesToEnable" type="atArray">
      <string type="atString"/>
    </array>

    <array name="m_txdToLoad" type="atArray">
      <string type="atHashString"/>
    </array>

    <array name="m_txdToUnload" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_residentResources" type="atArray">
      <struct type="CDataFileMgr::ResourceReference" />
    </array>
    <array name="m_unregisterResources" type="atArray">
      <struct type="CDataFileMgr::ResourceReference" />
    </array>

    <bool name="m_requiresLoadingScreen" init="false"/>
    <enum name="m_loadingScreenContext" type="LoadingScreenContext" init="LOADINGSCREEN_CONTEXT_MAPCHANGE"/>
	<struct type="ExecutionConditions" name="m_executionConditions" />

    <bool name="m_useCacheLoader" init="false"/>
  </structdef>

  <structdef type="CDataFileMgr::ContentsOfDataFileXml">
	<array name="m_disabledFiles" type="atArray">
	  <string type="atString"/>
	</array>
	<array name="m_includedXmlFiles" type="atArray">
	  <struct type="CDataFileMgr::DataFileArray"/>
	</array>
    <array name="m_includedDataFiles" type="atArray">
      <string type="atString"/>
    </array>
  <array name="m_dataFiles" type="atArray">
    <struct type="CDataFileMgr::DataFile"/>
  </array>
  <array name="m_contentChangeSets" type="atArray">
    <struct type="CDataFileMgr::ContentChangeSet"/>
  </array>
  <array name="m_patchFiles" type="atArray">
      <string type="atString"/>
  </array>
  <array name="m_allowedFolders" type="atArray">
    <string type="atString"/>
  </array>
  </structdef>

</ParserSchema>
