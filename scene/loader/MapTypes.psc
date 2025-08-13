<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="psoChecks">

  <hinsert>
#include		"fwscene/mapdata/maptypes.h"
	</hinsert>
	
	<structdef type="CCompEntityEffectsData" simple="true">
		<u32	name ="m_fxType"/>
		<Vector3 name="m_fxOffsetPos"/>
		<Vector4 name="m_fxOffsetRot"/>
		<u32	name ="m_boneTag"/>
		<float	name ="m_startPhase"/>
		<float	name ="m_endPhase"/>
		<bool	name ="m_ptFxIsTriggered"/>
		<string name="m_ptFxTag" type="member" size="64"/>
		<float	name ="m_ptFxScale"/>
		<float	name ="m_ptFxProbability"/>
		<bool	name ="m_ptFxHasTint"/>
		<u8		name ="m_ptFxTintR"/>
		<u8		name ="m_ptFxTintG"/>
		<u8		name ="m_ptFxTintB"/>
		<Vector3 name="m_ptFxSize"/>
	</structdef>

	<structdef type="CCompEntityAnims" simple="true">
		<string name="m_AnimDict" type="member" size="64"/>
		<string name="m_AnimName" type="member" size="64"/>
		<string name="m_AnimatedModel" type="member" size="64"/>
    <float	name ="m_punchInPhase" init="0.0"/>
    <float	name ="m_punchOutPhase" init="1.0"/>
		<array name="m_effectsData" type="atArray">
			<struct type="CCompEntityEffectsData"/>
		</array>
	</structdef>

	<structdef type="CCompositeEntityType" simple="true">
		<string name="m_Name" type="member" size="64"/>
		<float  name="m_lodDist"/>
		<u32   name="m_flags"/>
		<u32   name="m_specialAttribute"/>
		<Vector3 name="m_bbMin"/>
		<Vector3 name="m_bbMax"/>
		<Vector3 name="m_bsCentre"/>
		<float  name ="m_bsRadius"/>

    <pad bytes="4"/> <!-- name hash -->
    
		<string name="m_StartModel" type="member" size="64"/>
		<string name="m_EndModel" type="member" size="64"/>
    <string		name="m_StartImapFile" type="atHashString" />
    <string		name="m_EndImapFile" type="atHashString" />
		<string name="m_PtFxAssetName" type="atHashString" />
		<array name="m_Animations" type="atArray">
			<struct type="CCompEntityAnims"/>
		</array>
	</structdef>
	
	<!-- Map data describing archetypes, entities and additional game data for GTA V -->
	<structdef type="CMapTypes" base="::rage::fwMapTypes" >
    <array name="m_compositeEntityTypes" type="atArray">
      <struct type="CCompositeEntityType" />
    </array>
	</structdef>
	
		
</ParserSchema>
	