<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="eNmBlendOutItemType">
    <enumval name="NMBT_NONE"/>
    <enumval name="NMBT_SINGLECLIP"/>
    <enumval name="NMBT_DIRECTIONALBLEND"/>
    <enumval name="NMBT_AIMFROMGROUND"/>
    <enumval name="NMBT_FORCEMOTIONSTATE"/>
    <enumval name="NMBT_SETBLEND"/>
    <enumval name="NMBT_DIRECTBLENDOUT"/>
    <enumval name="NMBT_UPPERBODYREACTION"/>
    <enumval name="NMBT_DISALLOWBLENDOUT"/>
    <enumval name="NMBT_CRAWL"/>
  </enumdef>

  <enumdef type="CNmBlendOutSet::CNmBlendOutSetFlags">
    <enumval name="BOSF_ForceDefaultBlendOutSet"/>
    <enumval name="BOSF_ForceNoBlendOutSet"/>
    <enumval name="BOSF_AllowRootSlopeFixup"/>
    <enumval name="BOSF_AllowWhenDead"/>
    <enumval name="BOSF_DontAbortOnFall"/>
  </enumdef>

  <structdef type="CNmBlendOutItem">
    <string name="m_id" type="atHashString"></string>
    <enum name="m_type" type="eNmBlendOutItemType">NMBT_NONE</enum>
    <string name="m_nextItemId" type="atHashString"></string>
  </structdef>

  <structdef type="CNmBlendOutPoseItem" base="CNmBlendOutItem">
    <string name="m_clipSet" type="atHashString"></string>
    <string name="m_clip" type="atHashString"></string>
    <bool name="m_addToPointCloud" init="false" />
    <bool name="m_no180Blend" init="false"/>
    <bool name="m_looping" init="false"/>
    <bool name="m_allowInstantBlendToAim" init="false"/>
    <float name="m_duration" init="-1.0f" min="-1.0f" max="999999.0f"/>
    <float name="m_minPlaybackRate" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_maxPlaybackRate" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_earlyOutPhase" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_armedAIEarlyOutPhase" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_movementBreakOutPhase" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_turnBreakOutPhase" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_playerAimOrFireBreakOutPhase" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_dropDownPhase" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_ragdollFrameBlendDuration" init="0.25f" min="0.0f" max="10.0f"/>
    <float name="m_fullBlendHeadingInterpRate" init="2.0f" min="0.0f" max="30.0f"/>
    <float name="m_zeroBlendHeadingInterpRate" init="5.0f" min="0.0f" max="30.0f"/>
  </structdef>

  <structdef type="CNmBlendOutMotionStateItem" base="CNmBlendOutItem">
    <enum name="m_motionState" type="CPedMotionStates::eMotionState">MotionState_None</enum>
    <bool name="m_forceRestart" init="false"/>
    <float name="m_motionStartPhase" init="0.0f" description="Usefull when forcing the motion state to idle. Sets the initial walk / runstart phase the next time the ped moves off."/>
  </structdef>

  <structdef type="CNmBlendOutBlendItem" base="CNmBlendOutItem">
    <float name="m_blendDuration" init ="0.25f" min="0.0f" max="10.0f"/>
    <bool name="m_tagSync" init ="false"/>
  </structdef>

  <structdef type="CNmBlendOutReactionItem" base="CNmBlendOutItem">
    <string name="m_clipSet" type="atHashString"></string>
    <float name="m_doReactionChance" init="1.0f" min="0.0f" max="1.0f"/>
  </structdef>
  
  <structdef type="CNmBlendOutSet">
    <bitset name="m_ControlFlags" type="fixed32" init="BIT(CNmBlendOutSet::BOSF_AllowRootSlopeFixup)" values="CNmBlendOutSet::CNmBlendOutSetFlags"/>
    <array name="m_items" type="atArray">
      <pointer type="CNmBlendOutItem" policy="owner"/>
    </array>
    <string name="m_fallbackSet" type="atHashString"></string>
  </structdef>

  <structdef type="CNmBlendOutSetManager">
    <map name="m_sets" type="atBinaryMap" key="atHashString">
      <pointer type="CNmBlendOutSet" policy="owner"/>
    </map>
  </structdef>

</ParserSchema>