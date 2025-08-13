<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


  <const name="MAX_PROPS_PER_PED" value="6"/>
  <const name="PV_MAX_COMP" value="12"/>
  <const name="NUM_ANCHORS" value="13"/>

  <structdef type="CPedFacialOverlays" simple="true">
    <array name="m_blemishes" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_facialHair" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_eyebrow" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_aging" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_makeup" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_blusher" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_damage" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_baseDetail" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_skinDetail1" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_skinDetail2" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_bodyOverlay1" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_bodyOverlay2" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_bodyOverlay3" type="atArray">
      <string type="atHashString"/>
    </array>
  </structdef>

  <structdef type="CPedSkinTones::sSkinComp" simple="true">
    <array name="m_skin" type="atArray">
      <string type="atHashString"/>
    </array>
  </structdef>
  
  <structdef type="CPedSkinTones" simple="true">
    <array name="m_comps" type="member" size="3">
      <struct type="CPedSkinTones::sSkinComp"/>
    </array>
    <array name="m_males" type="atArray">
      <u8/>
    </array>
    <array name="m_females" type="atArray">
      <u8/>
    </array>
    <array name="m_uniqueMales" type="atArray">
      <u8/>
    </array>
    <array name="m_uniqueFemales" type="atArray">
      <u8/>
    </array>
  </structdef>

  <enumdef type="CPedCompRestriction::Restriction">
    <enumval name="CantUse"/>
    <enumval name="MustUse"/>
  </enumdef>

  <structdef type="CPedCompRestriction" simple="true">
    <enum name="m_Component" type="ePedVarComp"/>
    <int name="m_DrawableIndex"/>
    <int name="m_TextureIndex" init ="-1"/>
    <enum name="m_Restriction" type="CPedCompRestriction::Restriction"/>
  </structdef>
  
  <enumdef type="CPedPropRestriction::Restriction">
    <enumval name="CantUse"/>
    <enumval name="MustUse"/>
  </enumdef>

  <structdef type="CPedPropRestriction" simple="true">
    <enum name="m_PropSlot" type="eAnchorPoints"/>
	<int name="m_PropIndex"/>
    <enum name="m_Restriction" type="CPedPropRestriction::Restriction"/>
  </structdef>
  
	<structdef type="CComponentInfo" simple="true">
		<string name="pedXml_audioID" type="atHashString" init="NONE"/>
		<string name="pedXml_audioID2" type="atHashString" init="NONE"/>
		<array name="pedXml_expressionMods" type="member" size="5">
			<float />
		</array>
    <u32 name="flags" />
    <bitset name="inclusions" type="fixed" numBits="32"/>
    <bitset name="exclusions" type="fixed" numBits="32"/>
	  <bitset name="pedXml_vfxComps" type="fixed16" numBits="16" values="ePedVarComp"/>
    <u16 name="pedXml_flags" />
    <u8 name="pedXml_compIdx" />
    <u8 name="pedXml_drawblIdx" />
	</structdef>

	<structdef type="CPVTextureData" simple="true" onPostLoad="PostLoad">
		<u8 name="texId" />
		<u8 name="distribution" />
	  <pad bytes="1"/> 
    <!-- useCount -->
	</structdef>
	
  <structdef type="CPVDrawblData::CPVClothComponentData" simple="true" onPostLoad="PostLoad">
	  <bool name="m_ownsCloth" />
    <pad bytes="11" platform="32bit"/>
    <pad bytes="23" platform="64bit"/>
    <!-- pgRef m_charCloth, s32 m_dictSlotId, u8 m_Paddingx3 -->
  </structdef>

	<structdef type="CPVDrawblData" simple="true">
		<u8 name="m_propMask" />
		<u8 name="m_numAlternatives" />
		<array name="m_aTexData" type="atArray">
			<struct type="CPVTextureData"/>
		</array>
    <struct name="m_clothData" type="CPVDrawblData::CPVClothComponentData"/>
	</structdef>

  <structdef type="CPVComponentData" simple="true">
		<u8 name="m_numAvailTex" />
		<array name="m_aDrawblData3" type="atArray">
			<struct type="CPVDrawblData"/>
		</array>
	</structdef>

  <structdef type="CPedSelectionSet" simple="true">
    <string name="m_name" type="atHashString"/>
    <array name="m_compDrawableId" type="member" size="12">
      <u8 />
    </array>
    <array name="m_compTexId" type="member" size="12">
      <u8 />
    </array>
    <array name="m_propAnchorId" type="member" size="MAX_PROPS_PER_PED">
      <u8 />
    </array>
    <array name="m_propDrawableId" type="member" size="MAX_PROPS_PER_PED">
      <u8 />
    </array>
    <array name="m_propTexId" type="member" size="MAX_PROPS_PER_PED">
      <u8 />
    </array>
  </structdef>

  <structdef type="CPedPropTexData" simple="true">
    <bitset name="inclusions" type="fixed" numBits="32"/>
    <bitset name="exclusions" type="fixed" numBits="32"/>
		<u8 name="texId" />
		<u8 name="inclusionId" />
		<u8 name="exclusionId" />
		<u8 name="distribution" />
	</structdef>
	
  <enumdef type="ePropRenderFlags">
    <enumval name="PRF_ALPHA" value="0"/>
    <enumval name="PRF_DECAL" />
    <enumval name="PRF_CUTOUT" />
  </enumdef>
  
	<structdef type="CPedPropMetaData" simple="true">
    <string name="audioId" type="atHashString" init="NONE"/>
    <array name="expressionMods" type="member" size="5">
      <float />
    </array>
    <array name="texData" type="atArray">
      <struct type="CPedPropTexData"/>
    </array>
    <bitset name="renderFlags" type="fixed8" numBits="3" values="ePropRenderFlags"/>
    <u32 name="propFlags" />
    <u16 name="flags" />
    <u8 name="anchorId" />
		<u8 name="propId" />
		<u8 name="stickyness" />
	</structdef>

	<structdef type="CAnchorProps" simple="true">
        <array name="props" type="atArray">
            <u8 />
        </array>
        <enum name="anchor" type="eAnchorPoints"/>
	</structdef>

	<structdef type="CPedPropInfo" simple="true">
    <u8 name="m_numAvailProps" />
		<array name="m_aPropMetaData" type="atArray">
			<struct type="CPedPropMetaData"/>
		</array>
		<array name="m_aAnchors" type="atArray">
			<struct type="CAnchorProps"/>
		</array>
	</structdef>

	<structdef type="CPedVariationInfo" simple="true">
		<bool name="m_bHasTexVariations" />
		<bool name="m_bHasDrawblVariations" />
		<bool name="m_bHasLowLODs" />
		<bool name="m_bIsSuperLOD" />
		<array name="m_availComp" type="member" size="12">
			<u8/>
		</array>
		<array name="m_aComponentData3" type="atArray">
			<struct type="CPVComponentData"/>
		</array>
    <array name="m_aSelectionSets" type="atArray">
      <struct type="CPedSelectionSet"/>
    </array>
		<array name="m_compInfos" type="atArray">
			<struct type="CComponentInfo" />
		</array>
		<struct type="CPedPropInfo" name="m_propInfo"/>
    <string name="m_dlcName" type="atFinalHashString"/>
		<pad bytes="2"/> <!-- u8 m_seqHeadTexIdx, u8 m_seqHeadDrawblIdx-->
	</structdef>

  <structdef type="FirstPersonTimeCycleData" simple="true">
    <int name="m_textureId"/>
    <string name="m_timeCycleMod" type="atHashString"/>
    <float name="m_intensity" />
  </structdef>

  <structdef type="FirstPersonPropAnim" simple="true">
    <string name="m_timeCycleMod" type="atHashString"/>
    <float name="m_maxIntensity" init="0.0"/>
    <float name="m_duration" init="0.0"/>
    <float name="m_delay" init="0.0"/>
  </structdef>

  <enumdef type="eFPTCInterpType">
    <enumval name="FPTC_LINEAR" value="0"/>
    <enumval name="FPTC_POW2"/>
    <enumval name="FPTC_POW4"/>
  </enumdef>

  <structdef type="FirstPersonAssetData" simple="true">
    <u8 name="m_localIndex"/>
    <enum name="m_eCompType" type="ePedVarComp" size="8" init="PV_MAX_COMP"/>
    <enum name="m_eAnchorPoint" type="eAnchorPoints" size="8" init="NUM_ANCHORS"/>
    <string name="m_dlcHash" type="atHashString"/>
    <array name="m_timeCycleMods" type="atArray">
      <struct type="FirstPersonTimeCycleData"/>
    </array>
    <struct name="m_putOnAnim" type="FirstPersonPropAnim"/>
    <struct name="m_takeOffAnim" type="FirstPersonPropAnim"/>
    <string name="m_audioId" type="atHashString"/>
    <u32 name="m_flags" />
    <string name="m_fpsPropModel" type="atHashString"/>
    <Vector3 name="m_fpsPropModelOffset"/>
    <float name="m_seWeather" init="1.0" />
    <float name="m_seEnvironment" init="1.0" />
    <float name="m_seDamage" init="1.0" />
    <float name="m_tcModFadeOffSecs" init="60.0" />
    <enum name="m_tcModInterpType" type="eFPTCInterpType" init="FPTC_LINEAR"/>    
  </structdef>

  <structdef type="FirstPersonPedData" simple="true">
    <string name="m_name" type="atHashString"/>
    <array name="m_objects" type="atArray">
      <struct type="FirstPersonAssetData"/>
    </array>
  </structdef>

  <structdef type="FirstPersonAssetDataManager" simple="true">
    <array name="m_peds" type="atArray">
      <struct type="FirstPersonPedData"/>
    </array>
  </structdef>


  <structdef type="FirstPersonAlternate" simple="true">
    <string name="m_assetName" type="atHashString"/>
    <u8 name="m_alternate" />
  </structdef>
  
  <structdef type="FirstPersonAlternateData" simple="true">
    <array name="m_alternates" type="atArray">
      <struct type="FirstPersonAlternate"/>
    </array>
  </structdef>


  <structdef type="MpTintLastGenMapping" simple="true">
    <u8 name="m_texIndex" />
    <u8 name="m_paletteRow" />
  </structdef>

  <structdef type="MpTintAssetData" simple="true">
    <string name="m_assetName" type="atHashString"/>
    <bool name="m_secondaryHairColor" />
    <bool name="m_secondaryAccsColor" />
    <array name="m_lastGenMapping" type="atArray">
      <struct type="MpTintLastGenMapping"/>
    </array>
  </structdef>

  <structdef type="MpTintDualColor" simple="true">
    <u8 name="m_primaryColor" />
    <u8 name="m_defaultSecondaryColor" />
  </structdef>

  <structdef type="MpTintData" simple="true">
    <array name="m_creatorHairColors" type="atArray">
      <struct type="MpTintDualColor"/>
    </array>
    <array name="m_creatorAccsColors" type="atArray">
      <u8/>
    </array>
    <array name="m_creatorLipstickColors" type="atArray">
      <u8/>
    </array>
    <array name="m_creatorBlushColors" type="atArray">
      <u8/>
    </array>
    <array name="m_barberHairColors" type="atArray">
      <struct type="MpTintDualColor"/>
    </array>
    <array name="m_barberAccsColors" type="atArray">
      <u8/>
    </array>
    <array name="m_barberLipstickColors" type="atArray">
      <u8/>
    </array>
    <array name="m_barberBlushColors" type="atArray">
      <u8/>
    </array>
    <array name="m_barberBlushFacepaintColors" type="atArray">
      <u8/>
    </array>
    <array name="m_assetData" type="atArray">
      <struct type="MpTintAssetData"/>
    </array>
  </structdef>

  <enumdef type="ePedVarComp">
		<enumval name="PV_COMP_INVALID" value="-1"/>
		<enumval name="PV_COMP_HEAD" value="0"/>
		<enumval name="PV_COMP_BERD"/>
		<enumval name="PV_COMP_HAIR"/>
		<enumval name="PV_COMP_UPPR"/>
		<enumval name="PV_COMP_LOWR"/>
		<enumval name="PV_COMP_HAND"/>
		<enumval name="PV_COMP_FEET"/>
		<enumval name="PV_COMP_TEEF"/>
		<enumval name="PV_COMP_ACCS"/>
		<enumval name="PV_COMP_TASK"/>
		<enumval name="PV_COMP_DECL"/>
		<enumval name="PV_COMP_JBIB"/>
		<enumval name="PV_COMP_MAX"/>
	</enumdef>
	
	<enumdef type="eAnchorPoints">
		<enumval name="ANCHOR_HEAD"/>
		<enumval name="ANCHOR_EYES"/>
		<enumval name="ANCHOR_EARS"/>
		<enumval name="ANCHOR_MOUTH"/>
		<enumval name="ANCHOR_LEFT_HAND"/>
		<enumval name="ANCHOR_RIGHT_HAND"/>
		<enumval name="ANCHOR_LEFT_WRIST"/>
		<enumval name="ANCHOR_RIGHT_WRIST"/>
		<enumval name="ANCHOR_HIP"/>
		<enumval name="ANCHOR_LEFT_FOOT"/>
		<enumval name="ANCHOR_RIGHT_FOOT"/>
		<enumval name="ANCHOR_PH_L_HAND"/>
		<enumval name="ANCHOR_PH_R_HAND"/>
		<enumval name="NUM_ANCHORS"/>
	</enumdef>

  <enumdef type="ePedCompFlags">
    <enumval name="PV_FLAG_NONE" value="0"/>
    <enumval name="PV_FLAG_BULKY" value="1"/>
    <enumval name="PV_FLAG_JOB" value="2"/>
    <enumval name="PV_FLAG_SUNNY" value="4"/>
    <enumval name="PV_FLAG_WET" value="8"/>
    <enumval name="PV_FLAG_COLD" value="16"/>
    <enumval name="PV_FLAG_NOT_IN_CAR" value="32"/>
    <enumval name="PV_FLAG_BIKE_ONLY" value="64"/>
    <enumval name="PV_FLAG_NOT_INDOORS" value="128"/>
    <enumval name="PV_FLAG_FIRE_RETARDENT" value="256"/>
    <enumval name="PV_FLAG_ARMOURED" value="512"/>
    <enumval name="PV_FLAG_LIGHTLY_ARMOURED" value="1024"/>
    <enumval name="PV_FLAG_HIGH_DETAIL" value="2048"/>
    <enumval name="PV_FLAG_DEFAULT_HELMET" value="4096"/>
    <enumval name="PV_FLAG_RANDOM_HELMET" value="8192"/>
    <enumval name="PV_FLAG_SCRIPT_HELMET" value="16384"/>
    <enumval name="PV_FLAG_FLIGHT_HELMET" value="32768"/>
    <enumval name="PV_FLAG_HIDE_IN_FIRST_PERSON" value="65536"/>
    <enumval name="PV_FLAG_USE_PHYSICS_HAT_2" value="131072"/>
    <enumval name="PV_FLAG_PILOT_HELMET" value="262144"/>
    <enumval name="PV_FLAG_WET_MORE_WET" value="524288"/>
    <enumval name="PV_FLAG_WET_LESS_WET" value="1048576"/>
  </enumdef>

</ParserSchema>
