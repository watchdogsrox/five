<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class, psoChecks">

  <hinsert>
#include		"fwscene/mapdata/maptypes.h"
#include		"scene/2dEffect.h"
#include		"task/Scenario/ScenarioPointFlags.h"
	</hinsert>

	<!--
		Base Extension Class (abstract)
	-->
	<structdef type="CExtensionDef" base="::rage::fwExtensionDef">
		<Vector3	name="m_offsetPosition" />
	</structdef>

	
	<!--
		Audio Emitter
	-->
	<structdef type="CExtensionDefAudioEmitter" base="CExtensionDef">
		<Vector4	name="m_offsetRotation" />
		<u32		name="m_effectHash" />
	</structdef>

	
	<!--
		Buoyancy
	-->
	<structdef type="CExtensionDefBuoyancy" base="CExtensionDef">
	</structdef>

	
	<!--
		Explosion Effect
	-->
	<structdef type="CExtensionDefExplosionEffect" base="CExtensionDef">
		<Vector4 	name="m_offsetRotation" />
		<string 	name="m_explosionName" type="atString" />
		<int 		name="m_boneTag" />
		<int 		name="m_explosionTag" />
		<int 		name="m_explosionType" />
		<u32 		name="m_flags" />
	</structdef>


	<!--
		Particle Effect
	-->
	<structdef type="CExtensionDefParticleEffect" base="CExtensionDef">
		<Vector4 	name="m_offsetRotation" />
		<string 	name="m_fxName" type="atString" />
		<int 		name="m_fxType" />
		<int 		name="m_boneTag" />
		<float 		name="m_scale" />
		<int 		name="m_probability" />
		<int 		name="m_flags" />
		<Color32 	name="m_color" />
	</structdef>


	<!--
		Decal
	-->
	<structdef type="CExtensionDefDecal" base="CExtensionDef">
		<Vector4 	name="m_offsetRotation" />
		<string 	name="m_decalName" type="atString" />
		<int 		name="m_decalType" />
		<int 		name="m_boneTag" />
		<float 		name="m_scale" />
		<int 		name="m_probability" />
		<int 		name="m_flags" />
	</structdef>


	<!--
		Wind Disturbance
	-->
	<structdef type="CExtensionDefWindDisturbance" base="CExtensionDef">
		<Vector4 	name="m_offsetRotation" />
		<int 		name="m_disturbanceType" />
		<int 		name="m_boneTag" />
		<Vector4 	name="m_size" />
		<float 		name="m_strength" />
		<int 		name="m_flags" />
	</structdef>


  <!--
	Light Attr
	// ==================================================================================
	// WARNING!! IF YOU CHANGE THE FIELDS HERE YOU MUST ALSO KEEP IN SYNC WITH:
	// 
	// CLightAttrDef - x:\gta5\src\dev\game\scene\loader\MapData_Extensions.psc
	// CLightAttr    - x:\gta5\src\dev\game\scene\2dEffect.h
	// CLightAttr    - x:\gta5\src\dev\rage\framework\tools\src\cli\ragebuilder\gta_res.h
	// WriteLight    - x:\gta5\src\dev\game\debug\AssetAnalysis\AssetAnalysis.cpp
	// WriteLights   - X:\gta5\src\dev\tools\AssetAnalysisTool\src\AssetAnalysisTool.cpp
	// aaLightRecord - X:\gta5\src\dev\tools\AssetAnalysisTool\src\AssetAnalysisTool.h
	// ==================================================================================
	-->
	<structdef type="CLightAttrDef">
		<array name="m_posn" type="member" size="3">
			<float />
		</array>

		<!-- General data -->
		<array name="m_colour" type="member" size="3">
			<u8 />
		</array>
		<u8 name="m_flashiness" />
		<float name="m_intensity" />
		<u32 name="m_flags" />
		<s16 name="m_boneTag" />
		<u8 name="m_lightType" />
		<u8 name="m_groupId" />
		<u32 name="m_timeFlags" />
		<float name="m_falloff" />
		<float name="m_falloffExponent" />
		<array name="m_cullingPlane" type="member" size="4">
			<float />
		</array>
    	<u8 name="m_shadowBlur" />
		<u8 name="m_padding1" />
		<s16 name="m_padding2" />
		<u32 name="m_padding3" />

		<!-- Volume data -->
		<float name="m_volIntensity" />
		<float name="m_volSizeScale" />
		<array name="m_volOuterColour" type="member" size="3">
			<u8 />
		</array>
		<u8 name="m_lightHash" />
		<float name="m_volOuterIntensity" />

		<!-- Corona data -->
		<float name="m_coronaSize" />
		<float name="m_volOuterExponent" />
		<u8 name="m_lightFadeDistance" />
		<u8 name="m_shadowFadeDistance" />
		<u8 name="m_specularFadeDistance" />
		<u8 name="m_volumetricFadeDistance" />
	    <float name="m_shadowNearClip" />
		<float name="m_coronaIntensity" />
		<float name="m_coronaZBias" />

		<!-- Spot data -->
		<array name="m_direction" type="member" size="3">
			<float />
		</array>
		<array name="m_tangent" type="member" size="3">
			<float />
		</array>
		<float name="m_coneInnerAngle" />
		<float name="m_coneOuterAngle" />

		<!-- Line data -->
		<array name="m_extents" type="member" size="3">
			<float />
		</array>

		<!-- Texture data -->
		<u32 name="m_projectedTextureKey" />
	</structdef>

	<!--
		Light Effect
	-->
	<structdef type="CExtensionDefLightEffect" base="CExtensionDef">
		<array 		name="m_instances" type="atArray">
			<struct type="CLightAttrDef" />
		</array>
	</structdef>

	<!--
		Light Shaft Effect
	// ==================================================================================
	// WARNING!! IF YOU CHANGE THE FIELDS HERE YOU MUST ALSO KEEP IN SYNC WITH:
	// 
	// CExtensionDefLightShaft - x:\gta5\src\dev\game\scene\loader\MapData_Extensions.psc
	// CLightShaft             - x:\gta5\src\dev\game\scene\2dEffect.h
	// 
	// also density type and volume type must match:
	// 
	// x:\gta5\src\dev\game\scene\loader\MapData_Extensions.psc
	// x:\gta5\src\dev\game\renderer\Lights\LightCommon.h
	// ==================================================================================
	-->
	<enumdef type="CExtensionDefLightShaftDensityType">
		<enumval name="LIGHTSHAFT_DENSITYTYPE_CONSTANT" />
		<enumval name="LIGHTSHAFT_DENSITYTYPE_SOFT" />
		<enumval name="LIGHTSHAFT_DENSITYTYPE_SOFT_SHADOW" />
		<enumval name="LIGHTSHAFT_DENSITYTYPE_SOFT_SHADOW_HD" />
		<enumval name="LIGHTSHAFT_DENSITYTYPE_LINEAR" />
		<enumval name="LIGHTSHAFT_DENSITYTYPE_LINEAR_GRADIENT" />
		<enumval name="LIGHTSHAFT_DENSITYTYPE_QUADRATIC" />
		<enumval name="LIGHTSHAFT_DENSITYTYPE_QUADRATIC_GRADIENT" />
	</enumdef>
	<enumdef type="CExtensionDefLightShaftVolumeType">
		<enumval name="LIGHTSHAFT_VOLUMETYPE_SHAFT" />
		<enumval name="LIGHTSHAFT_VOLUMETYPE_CYLINDER" />
	</enumdef>
	<structdef type="CExtensionDefLightShaft" base="CExtensionDef">
		<Vector3 name="m_cornerA" />
		<Vector3 name="m_cornerB" />
		<Vector3 name="m_cornerC" />
		<Vector3 name="m_cornerD" />
		<Vector3 name="m_direction" />
		<float   name="m_directionAmount" init="0" />
		<float   name="m_length" />
		<float   name="m_fadeInTimeStart" init="0" />
		<float   name="m_fadeInTimeEnd" init="0" />
		<float   name="m_fadeOutTimeStart" init="0" />
		<float   name="m_fadeOutTimeEnd" init="0" />
		<float   name="m_fadeDistanceStart" init="0" />
		<float   name="m_fadeDistanceEnd" init="0" />
		<Color32 name="m_color" />
		<float   name="m_intensity" />
		<u8      name="m_flashiness" init="0" /> <!-- see eLightFlashiness (FL_*) -->
		<u32     name="m_flags" /> <!-- see LIGHTSHAFTFLAG_* -->
		<enum    name="m_densityType" type="CExtensionDefLightShaftDensityType" init="0" />
		<enum    name="m_volumeType" type="CExtensionDefLightShaftVolumeType" init="0" />
		<float   name="m_softness" init="0.0" />
		<!--new flag hacks-->
		<bool    name="m_scaleBySunIntensity" init="true" />
	</structdef>

	
	<!--
		Scrollbars
		E.g. used in MH09 (Times Sq.) for ticker text around buildings.
	-->
	<structdef type="CExtensionDefScrollbars" base="CExtensionDef">
		<float		name="m_height" />
		<int		name="m_scrollbarsType" />
		<array 		name="m_points" type="atArray">
			<Vector3 />
		</array>
	</structdef>

	
	<!--
		Spawn Point
	-->
	<structdef type="CExtensionDefSpawnPoint" base="CExtensionDef">
		<Vector4 	name="m_offsetRotation" />
    <string name="m_spawnType" type="atHashString"/>
		<string name="m_pedType" type="atHashString"/>
		<string name="m_group" type="atHashString"/>
		<string name="m_interior" type="atHashString"/>
		<string name="m_requiredImap" type="atHashString"/>
		<enum		name="m_availableInMpSp" type="CSpawnPoint::AvailabilityMpSp" init="0"/>
		<float		name="m_probability" init="0.0f"/>
		<float		name="m_timeTillPedLeaves" init="0.0f"/>
		<float  name="m_radius" init="0.0f" />
		<u8 		name="m_start" />
		<u8 		name="m_end" />
		<bitset		name="m_flags" type="fixed32" values="CScenarioPointFlags::Flags"/>
		<bool		name="m_highPri" init="false"/>
		<bool		name="m_extendedRange" init="false"/>
    <bool		name="m_shortRange" init="false"/>
  </structdef>

	
	<!--
		Swayable Effect    
		E.g. for overhead traffic lights swaying in the breeze.
	-->
	<structdef type="CExtensionDefSwayableEffect" base="CExtensionDef">
		<int 		name="m_boneTag" />
		<float		name="m_lowWindSpeed" />
		<float		name="m_lowWindAmplitude" />
		<float		name="m_highWindSpeed" />
		<float		name="m_highWindAmplitude" />
	</structdef>

	
	<!--
		Proc Object
	-->
	<structdef type="CExtensionDefProcObject" base="CExtensionDef">
		<float		name="m_radiusInner" />
		<float		name="m_radiusOuter" />
		<float		name="m_spacing" />
		<float		name="m_minScale" />
		<float		name="m_maxScale" />
		<float		name="m_minScaleZ" />
		<float		name="m_maxScaleZ" />
		<float		name="m_minZOffset" />
		<float		name="m_maxZOffset" />
		<u32		name="m_objectHash" />
		<u32		name="m_flags" />
	</structdef>

	
	<!--
		Walk Don't Walk
	-->
	<structdef type="CExtensionDefWalkDontWalk" base="CExtensionDef">
	</structdef>

	
	<!--
		Script Child Object
	-->
	<structdef type="CExtensionDefScriptChild">
		<Vector3 	name="m_position" />
		<float 		name="m_rotationZ" />
	</structdef>

	
	<!--
		Script.
	-->
	<structdef type="CExtensionDefScript" base="CExtensionDef">
		<string 	name="m_scriptName" type="atString" />
		<array 		name="m_children" type="atArray">
			<struct type="CExtensionDefScriptChild" />
		</array>
	</structdef>

	
	<!--
		Ladder.
	-->
  <enumdef type="CExtensionDefLadderMaterialType">
    <enumval name="METAL_SOLID_LADDER" />
    <enumval name="METAL_LIGHT_LADDER" />
    <enumval name="WOODEN_LADDER" />
  </enumdef>
	<structdef type="CExtensionDefLadder" base="CExtensionDef">
		<Vector3 	name="m_bottom" />
		<Vector3 	name="m_top" />
		<Vector3 	name="m_normal" />
    <enum     name="m_materialType" type="CExtensionDefLadderMaterialType" init="0" />
    <string   name="m_template" type="atHashString">default</string>
		<bool 		name="m_canGetOffAtTop" />
    <bool 		name="m_canGetOffAtBottom" />
	</structdef>


  <!--
		Audio Collisions.
	-->
  <structdef type="CExtensionDefAudioCollisionSettings" base="CExtensionDef">
    <string   name="m_settings" type="atHashString">default</string>
  </structdef>
  
 
	<!--
		Climbing handhold.
	-->
	<structdef type="CExtensionDefClimbHandHold" base="CExtensionDef">
		<Vector3	name="m_left" />
		<Vector3	name="m_right" />
		<Vector3	name="m_normal" />
	</structdef>
	
	
	<!--
		Expression
	-->

	<structdef type="CExtensionDefExpression" base="CExtensionDef">
		<string		name="m_expressionDictionaryName" type="atFinalHashString" />
		<string		name="m_expressionName" type="atFinalHashString" />
    <string		name="m_creatureMetadataName" type="atFinalHashString" />
    <bool		  name="m_initialiseOnCollision" init="false"/>
  </structdef>
	
	
	<!--
		Door
	-->
	<structdef type="CExtensionDefDoor" base="CExtensionDef">
		<bool		name="m_enableLimitAngle" />
		<bool		name="m_startsLocked" />
		<bool		name="m_canBreak" />
		<float		name="m_limitAngle" />
    <float		name="m_doorTargetRatio" />
    <string		name="m_audioHash" type="atHashString"></string>
	</structdef>
	
	
	<!--
		Light
	-->
	<structdef type="CExtensionDefLight" base="CExtensionDef">
	</structdef>
	
	
	<!--
		Spawn point override
	-->
	<structdef type="CExtensionDefSpawnPointOverride" base="CExtensionDef">
		<string		name="m_ScenarioType" type="atHashString" />
		<u8			name="m_iTimeStartOverride" />
		<u8			name="m_iTimeEndOverride" />
		<string		name="m_Group" type="atHashString" />
		<string		name="m_ModelSet" type="atHashString" />
		<enum		name="m_AvailabilityInMpSp" type="CSpawnPoint::AvailabilityMpSp" init="0"/>
		<bitset		name="m_Flags" type="fixed32" values="CScenarioPointFlags::Flags" />
		<float		name="m_Radius" init="0.0f" />
		<float		name="m_TimeTillPedLeaves" init="0.0f" />
	</structdef>
	
</ParserSchema>
