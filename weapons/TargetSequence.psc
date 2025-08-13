<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CTargetSequenceEntry" >
	  <enum name="m_TargetBone" type="eAnimBoneTag"/>
	  <Vector3 name="m_Offset" min="1.0f" max="1.0f" init="0.0f"/>
    <float name="m_ShotDelay" min="0.0f" max="10.0f" init="0.0f"/>
  </structdef>

  <structdef type="CTargetSequence" >
	  <array name="m_Entries" type="atArray">
		  <struct type="CTargetSequenceEntry"/>
	  </array>
  </structdef>

  <structdef type="CTargetSequenceGroup" >
    <string name="m_Id" type="atHashString"/>
    <array name="m_Sequences" type="atArray">
      <struct type="CTargetSequence"/>
    </array>
  </structdef>

  <structdef type="CTargetSequenceManager" >
    <array name="m_Groups" type="atArray">
      <struct type="CTargetSequenceGroup"/>
    </array>
  </structdef>

</ParserSchema>