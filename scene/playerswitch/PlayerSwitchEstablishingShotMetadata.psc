<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
              generate="class">

  <!-- a helper that encapsulates the settings for a player Switch establishing shot -->
  <structdef type="CPlayerSwitchEstablishingShotMetadata">
    <string	name="m_Name" type="atHashString" ui_key="true"/>
    <string	name="m_OverriddenShotForFirstPersonOnFoot" type="atHashString" description="An optional alternate establishing shot to be used only when the destination ped is in a first-person view on-foot"/>
    <Vector3 name="m_Position" init="0.0f, 0.0f, 0.0f" min="-99999.0f" max="99999.0f" description="World position"/>
    <Vector3 name="m_Orientation" init="0.0f, 0.0f, 0.0f" min="-180.0f" max="180.0f" description="Euler angles in degrees, YXZ rotation order"/>
    <Vector3 name="m_CatchUpPosition" init="0.0f, 0.0f, 0.0f" min="-99999.0f" max="99999.0f" description="World position for catch-up exit"/>
    <Vector3 name="m_CatchUpOrientation" init="0.0f, 0.0f, 0.0f" min="-180.0f" max="180.0f" description="Euler angles in degrees, YXZ rotation order, for catch-up exit"/>
    <float name="m_Fov" init="45.0f" min="5.0f" max="130.0f" description="Vertical field of view, in degrees"/>
    <float name="m_CatchUpFov" init="50.0f" min="5.0f" max="130.0f" description="Vertical field of view, in degrees, for catch-up exit"/>
    <float name="m_GameplayRelativeHeading" init="0.0f" min="-180.0f" max="180.0f" description="Gameplay camera heading relative to attach parent, in degrees"/>
    <float name="m_GameplayRelativePitch" init="0.0f" min="-89.0f" max="89.0f" description="Gameplay camera pitch relative to attach parent, in degrees"/>
    <u32 name="m_HoldDuration" init="2500" min="0" max="600000" description="The length of time to hold at the end of the swoop down to the establishing shot, in milliseconds"/>
    <u32 name="m_InterpolateOutDuration" init="0" min="0" max="600000" description="If greater than 0ms, the establishing shot will interpolate straight to gameplay, skipping any gameplay outro behaviour"/>
    <bool name="m_ShouldApplyGameplayCameraOrientation" init="false" description="If true, the GameplayRelativeHeading and GameplayRelativePitch angles will be applied when interpolating to gameplay"/>
    <bool name="m_ShouldIntepolateToCatchUp" init="false" description="If true, the establishing shot will interpolation to a gameplay catch-up behaviour, using the CatchUpPosition and CatchUpOrientation"/>
    <bool name="m_ShouldInhibitFirstPersonOnFoot" init="false" description="If true, any active on-foot gameplay camera will be inhibited from being in first-person for the duration of this establishing shot"/>
  </structdef> 

  <!-- a simple container for the player Switch establishing shot metadata -->
  <structdef type="CPlayerSwitchEstablishingShotMetadataStore">
    <array name="m_ShotList" type="atArray">
      <struct type="CPlayerSwitchEstablishingShotMetadata" />
    </array>
  </structdef>

</ParserSchema>
