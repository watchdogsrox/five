<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class, psoChecks">

  <hinsert>
#include  "scene/loader/MapData_Buildings.h"
  </hinsert>

  <enumdef type="eCompositeEntityEffectType" >
    <enumval name="kCompositeEntityEffectTypePtx"/>
  </enumdef>

  <structdef type="CCompositeEntityEffectsSetting">
    <enum name="m_fxType" type="eCompositeEntityEffectType" />
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

  <structdef type="CCompositeEntityAnimation">
    <string name="m_animDict" type="member" size="64"/>
    <string name="m_animName" type="member" size="64"/>
    <string name="m_animatedModel" type="member" size="64"/>
    <float name="m_punchInPhase" type="member" init="0.0f"/>
    <float name="m_punchOutPhase" type="member" init="1.0f"/>  
    <array name="m_effectsData" type="atArray">
      <struct type="CCompositeEntityEffectsSetting"/>
    </array>
  </structdef>

  <structdef type="CCompositeEntityArchetypeDef" base="CBaseArchetypeDef">
    <string name="m_startModel" type="member" size="64"/>
    <string name="m_endModel" type="member" size="64"/>

    <string name="m_startImapFile" type="atHashString"/>
    <string name="m_endImapFile" type="atHashString"/>
    
    <array name="m_animations" type="atArray">
      <struct type="CCompositeEntityAnimation"/>
    </array>
  </structdef>

</ParserSchema>
