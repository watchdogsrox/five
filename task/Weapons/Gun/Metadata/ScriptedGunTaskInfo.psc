<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="CScriptedGunTaskInfo::eScriptedGunTaskFlags">
    <enumval name="AllowWrapping"/>
    <enumval name="AbortOnIdle"/>
    <enumval name="UseAdditiveFiring"/>
    <enumval name="UseDirectionalBreatheAdditives"/>
    <enumval name="TrackRopeOrientation"/>
    <enumval name="ForcedAiming"/>
    <enumval name="ForceFireFromCamera"/>
  </enumdef>
 
  <structdef type="CScriptedGunTaskInfo">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <float name="m_MinAimSweepHeadingAngleDegs"/>
    <float name="m_MaxAimSweepHeadingAngleDegs"/>
    <float name="m_MinAimSweepPitchAngleDegs"/>
    <float name="m_MaxAimSweepPitchAngleDegs"/>
    <float name="m_fHeadingOffset"/>
    <string name="m_ClipSet" type="atFinalHashString"/>
    <string name="m_IdleCamera" type="atHashString"/>
    <string name="m_Camera" type="atHashString"/>
    <bitset name="m_Flags" type="fixed32" values="CScriptedGunTaskInfo::eScriptedGunTaskFlags"/>
  </structdef>
  
</ParserSchema>