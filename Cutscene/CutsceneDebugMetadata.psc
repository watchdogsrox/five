<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CutsceneApprovalStatuses">
    <string name="m_CutsceneName" type="atHashString" />
    <bool name="m_FinalApproved" init="false"/>
    <bool name="m_AnimationApproved" init="false"/>    
    <bool name="m_CameraApproved" init="false"/>
    <bool name="m_DofApproved" init="false"/>
    <bool name="m_LightingApproved" init="false"/>
    <bool name="m_FacialApproved" init="false"/>
  </structdef>

  <structdef type="ApprovedCutsceneList">
  <array name="m_ApprovalStatuses" type="atArray">
    <struct type="CutsceneApprovalStatuses"/>
  </array>
  </structdef>
  
</ParserSchema>