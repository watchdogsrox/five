<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CTaskAdvance::Tunables" base="CTuning">
    <float name="m_TimeToWaitAtPosition" min="0.0f" max="10.0f" step="1.0f" init="3.0f"/>
    <float name="m_TimeBetweenPointUpdates" min="0.0f" max="10.0f" step="1.0f" init="3.0f"/>
    <float name="m_TimeBetweenSeekChecksAtTacticalPoint" min="0.0f" max="10.0f" step="1.0f" init="5.0f"/>
  </structdef>
  
  <structdef type="CTaskAimFromGround::Tunables" base="CTuning">
    <float name="m_MaxAimFromGroundTime" min="0.0f" max="60.0f" step="0.1f" init="8.0f" description="The maximum time (in seconds) to aim from ground"/>
  </structdef>

  <structdef type="CTaskMoveCombatMounted::Tunables" base="CTuning">
    <array name="m_CircleTestRadii" type="atArray">
      <float min="0.0f" max="100.0f" step="0.1f"/>
    </array>
    <float name="m_CircleTestsMoveDistToTestNewPos" min="0.0f" max="20.0f" step="0.1f" init="4.0f"/>
    <float name="m_MinTimeSinceAnyCircleJoined" min="0.0f" max="10.0f" step="0.1f" init="0.2f"/>
    <float name="m_MinTimeSinceSameCircleJoined" min="0.0f" max="10.0f" step="0.1f" init="0.4f"/>
    <float name="m_TransitionReactionTime" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_VelStartCircling" min="0.0f" max="20.0f" step="0.1f" init="3.0f"/>
    <float name="m_VelStopCircling" min="0.0f" max="20.0f" step="0.1f" init="3.5f"/>
    <u32 name="m_MaxTimeWaitingForCircleMs" min="0" max="10000" step="100" init="3000"/>
  </structdef>

  <enumdef type="CTaskVariedAimPose::AimPose::Transition::Flags">
    <enumval name="OnlyUseForReactions"/>
    <enumval name="CanUseForReactions"/>
    <enumval name="Urgent"/>
    <enumval name="OnlyUseForLawEnforcementPeds"/>
    <enumval name="OnlyUseForGangPeds"/>
  </enumdef>

  <structdef type="CTaskVariedAimPose::AimPose::Transition" >
    <string name="m_ToPose" type="atHashString" />
    <string name="m_ClipSetId" type="atHashString" />
    <string name="m_ClipId" type="atHashString" />
    <float name="m_Rate" init="1.0f"/>
    <bitset name="m_Flags" type="fixed8" values="CTaskVariedAimPose::AimPose::Transition::Flags"/>
  </structdef>

  <structdef type="CTaskVariedAimPose::AimPose" >
    <string name="m_Name" type="atHashString" />
    <bool name="m_IsCrouching" />
    <bool name="m_IsStationary" />
    <string name="m_LoopClipSetId" type="atHashString" />
    <string name="m_LoopClipId" type="atHashString" />
    <array name="m_Transitions" type="atArray">
      <struct type="CTaskVariedAimPose::AimPose::Transition"/>
    </array>
  </structdef>

  <structdef type="CTaskVariedAimPose::Tunables" base="CTuning">
    <float name="m_MinTimeBeforeCanChooseNewPose" min="0.0f" max="5.0f" step="0.01f" init="0.25f"/>
    <float name="m_MinTimeBeforeNewPose" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_MaxTimeBeforeNewPose" min="0.0f" max="10.0f" step="0.1f" init="4.0f"/>
    <float name="m_DistanceForMinTimeBeforeNewPose" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
    <float name="m_DistanceForMaxTimeBeforeNewPose" min="0.0f" max="100.0f" step="1.0f" init="30.0f"/>
    <float name="m_AvoidNearbyPedHorizontal" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
    <float name="m_AvoidNearbyPedVertical" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
    <float name="m_AvoidNearbyPedDotThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.8f"/>
    <float name="m_TargetRadius" min="0.0f" max="100.0f" step="1.0f" init="5.0f"/>
    <float name="m_MinTimeBetweenReactions" min="0.0f" max="100.0f" step="1.0f" init="1.0f"/>
    <float name="m_MinAnimOffsetMagnitude" min="0.0f" max="10.0f" step="0.1f" init="0.5f"/>
    <float name="m_Rate" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_MaxDistanceToCareAboutBlockingLineOfSight" min="0.0f" max="25.0f" step="1.0f" init="15.0f"/>
    <float name="m_MaxDistanceToUseUrgentTransitions" min="0.0f" max="50.0f" step="1.0f" init="25.0f"/>
    <float name="m_TimeToUseUrgentTransitionsWhenThreatened" min="0.0f" max="30.0f" step="1.0f" init="10.0f"/>
    <float name="m_MinTimeBetweenReactionChecksForGunAimedAt" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_ChancesToReactForGunAimedAt" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <int name="m_MaxClipsToCheckPerFrame" min="0" max="10" step="1" init="5"/>
    <bool name="m_DebugDraw" init="false"/>
    <string name="m_DefaultStandingPose" type="atHashString" />
    <string name="m_DefaultCrouchingPose" type="atHashString" />
    <array name="m_AimPoses" type="atArray">
      <struct type="CTaskVariedAimPose::AimPose"/>
    </array>
  </structdef>

  <enumdef type="CTaskCover::eAnimFlags">
    <enumval name="AF_Low"/>
    <enumval name="AF_EnterLeft"/>
    <enumval name="AF_FaceLeft"/>
    <enumval name="AF_AtEdge"/>
    <enumval name="AF_ToLow"/>
    <enumval name="AF_AimDirect"/>
    <enumval name="AF_Center"/>
    <enumval name="AF_ToPeek"/>
    <enumval name="AF_Scope"/>
  </enumdef>

  <structdef type="CTaskCover::CoverAnimStateInfo" >
    <array name="m_Clips" type="atArray">
      <string type="atHashString" />
    </array>
    <bitset name="m_Flags" type="fixed32" values="CTaskCover::eAnimFlags"/>
  </structdef>

  <structdef type="CTaskCover::Tunables" base="CTuning">
    <float name="m_FPSBlindFireOutroBlendOutPelvisOffsetTime" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
    <float name="m_FPSAimOutroBlendOutPelvisOffsetTime" min="0.0f" max="2.0f" step="0.01f" init="0.0f"/>
    <float name="m_FPSPeekToAimBlendDurationHigh" min="0.0f" max="2.0f" step="0.01f" init="0.0f"/>
    <float name="m_FPSPeekToAimBlendDurationLow" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
    <float name="m_FPSPeekToBlindFireBlendDurationHigh" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
    <float name="m_FPSPeekToBlindFireBlendDurationLow" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
    <float name="m_FPSDefaultBlendDurationHigh" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
    <float name="m_FPSDefaultBlendDurationLow" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
    <float name="m_PriorityCoverWeighting" min="0.0f" max="10.0f" step="0.1f" init="6.0f"/>
    <float name="m_AngleToCameraWeighting" min="0.0f" max="10.0f" step="0.1f" init="4.0f"/>
    <float name="m_AngleToDynamicCoverWeighting" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
    <float name="m_DistanceWeighting" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
    <float name="m_AngleToCoverWeighting" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
    <float name="m_AngleOfCoverWeighting" min="0.0f" max="10.0f" step="0.1f" init="4.0f"/>
    <float name="m_EdgeWeighting" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
    <float name="m_NetworkBlendOutDurationRun" min="0.0f" max="2.0f" step="0.1f" init="0.75f"/>
    <float name="m_NetworkBlendOutDurationRunStart" min="0.0f" max="2.0f" step="0.1f" init="0.5f"/>
    <float name="m_NetworkBlendOutDuration" min="0.0f" max="2.0f" step="0.1f" init="0.25f"/>
    <float name="m_MaxPlayerToCoverDist" min="0.0f" max="20.0f" step="0.1f" init="5.0f"/>
    <float name="m_MaxPlayerToCoverDistFPS" min="0.0f" max="20.0f" step="0.1f" init="4.0f"/>
    <float name="m_MaxAngularDiffBetweenDynamicAndStaticCover" min="-1.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_RangeToUseDynamicCoverPointMin" min="0.0f" max="10.0f" step="0.01f" init="0.65f"/>
    <float name="m_RangeToUseDynamicCoverPointMax" min="0.0f" max="10.0f" step="0.01f" init="2.5f"/>
    <float name="m_MinDistToCoverAnyDir" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_MinDistToPriorityCoverToForce" min="0.0f" max="5.0f" step="0.01f" init="2.5f"/>
    <float name="m_MinDistToCoverSpecificDir" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
    <float name="m_BehindPedToCoverCosTolerance" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_SearchToCoverCosTolerance" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_CapsuleZOffset" min="-1.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_TimeBetweenTestSpheresIntersectingRoute" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_MaxDistToCoverWhenPlayerIsClose" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_MinCoverToPlayerCoverDist" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_MinMoveToCoverDistForCoverMeAudio" min="0.0f" max="25.0f" step="0.1f" init="6.0f"/>
    <float name="m_MaxSecondsAsTopLevelTask" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
    <bool name="m_ForceStreamingFailure" init="false"/>
    <string name="m_StreamedUnarmedCoverMovementClipSetId" type="atHashString" />
    <string name="m_StreamedUnarmedCoverMovementFPSClipSetId" type="atHashString" init="cover@move@extra@core" />
    <string name="m_StreamedOneHandedCoverMovementClipSetId" type="atHashString" />
    <string name="m_AIOneHandedAimingClipSetId" type="atHashString" />
    <string name="m_AITwoHandedAimingClipSetId" type="atHashString" />
    <string name="m_CoreWeaponClipSetId" type="atHashString"/>
    <string name="m_CoreWeaponAimingClipSetId" type="atHashString" init="cover@weapon@1h"/>
    <string name="m_CoreWeaponAimingFPSClipSetId" type="atHashString" init="cover@first_person@weapon@1h"/>
  </structdef>

  <structdef type="CTaskEnterCover::EnterClip" >
    <string name="m_EnterClipId" type="atHashString" />
    <bitset name="m_Flags" type="fixed32" values="CTaskCover::eAnimFlags"/>
  </structdef>

  <structdef type="CTaskEnterCover::Tunables::AnimStateInfos" >
    <array name="m_SlidingEnterCoverAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_PlayerStandEnterCoverAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
  </structdef>

  <structdef type="CTaskEnterCover::StandingEnterClips" autoregister="true" >
    <string name="m_StandClip0Id" type="atHashString" />
    <string name="m_StandClip1Id" type="atHashString" />
    <string name="m_StandClip2Id" type="atHashString" />
    <bitset name="m_Flags" type="fixed32" values="CTaskCover::eAnimFlags"/>
  </structdef>

  <structdef type="CTaskEnterCover::Tunables" base="CTuning">
    <float name="m_CoverEntryRatePlayer" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_CoverEntryRateAI" min="0.0f" max="5.0f" step="0.01f" init="0.6f"/>
    <float name="m_CoverEntryShortDistanceAI" min="0.0f" max="5.f" step="0.01f" init="2.5f"/>
    <float name="m_CoverEntryShortDistancePlayer" min="0.0f" max="5.0f" step="0.01f" init="2.5f"/>
    <float name="m_CoverEntryStandDistance" min="0.0f" max="2.5f" step="0.01f" init="1.0f"/>
    <float name="m_CoverEntryStandStrafeDistance" min="0.0f" max="2.5f" step="0.01f" init="0.25f"/>
    <float name="m_CoverEntryMinDistance" min="0.0f" max="2.5f" step="0.01f" init="1.0f"/>
    <float name="m_CoverEntryMaxDistance" min="0.0f" max="10.0f" step="0.01f" init="7.0f"/>
    <float name="m_CoverEntryMinDistanceAI" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
    <float name="m_CoverEntryMaxDistanceAI" min="0.0f" max="10.0f" step="0.01f" init="5.0f"/>
    <float name="m_CoverEntryMaxDirectDistance" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
    <s32 name="m_CoverEntryMinTimeNavigatingAI" min="0" max="5000" step="1" init="1000"/>
    <float name="m_CoverEntryMinAngleToScale" min="0.0f" max="1.0f" step="0.001f" init="0.05f"/>
    <float name="m_CoverEntryHeadingReachedTol" min="0.0f" max="1.0f" step="0.001f" init="0.05f"/>
    <float name="m_CoverEntryPositionReachedTol" min="0.0f" max="1.0f" step="0.001f" init="0.01f"/>
    <float name="m_FromCoverExitDistance" min="0.0f" max="1.0f" step="0.001f" init="0.5f"/>
    <float name="m_NetworkBlendInDuration" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
    <float name="m_DistFromCoverToAllowReloadCache" min="0.0f" max="10.0f" step="0.1f" init="7.5f"/>
    <bool name="m_WaitForFootPlant" init="true"/>
    <bool name="m_EnableFootTagSyncing" init="true"/>
    <bool name="m_ForceToTarget" init="true"/>
    <bool name="m_EnableInitialHeadingBlend" init="true"/>
    <bool name="m_EnableTranslationScaling" init="true"/>
    <bool name="m_EnableRotationScaling" init="true"/>
    <bool name="m_PreventTranslationOvershoot" init="true"/>
    <bool name="m_PreventRotationOvershoot" init="true"/>
    <bool name="m_DoInitialHeadingBlend" init="true"/>
    <bool name="m_DoFinalHeadingFixUp" init="true"/>
    <float name="m_MinDistToPlayEntryAnim" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
    <float name="m_MinDistToScale" min="0.0f" max="0.1f" step="0.001f" init="0.01f"/>
    <float name="m_MaxSpeed" min="0.0f" max="20.0f" step="0.01f" init="10.0f"/>
    <float name="m_MaxRotSpeed" min="0.0f" max="10.0f" step="0.01f" init="10.0f"/>
    <float name="m_MinTransScale" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxTransScale" min="0.0f" max="10.0f" step="0.01f" init="2.5f"/>
    <float name="m_MinRotScale" min="0.0f" max="10.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxRotScale" min="0.0f" max="10.0f" step="0.01f" init="2.5f"/>
    <float name="m_DeltaTolerance" min="0.0f" max="0.1f" step="0.001f" init="0.001f"/>
    <float name="m_MinRotDelta" min="0.0f" max="1.5f" step="0.001f" init="0.2f"/>
    <float name="m_MaxAngleToSetDirectly" min="0.0f" max="1.5f" step="0.001f" init="0.25f"/>
    <float name="m_AiEntryHalfAngleTolerance" min="0.0f" max="1.57f" step="0.001f" init="0.196f"/>
    <bool name="m_EnableNewAICoverEntry" init="true"/>
    <bool name="m_EnableUseSwatClipSet" init="true"/>
    <bool name="m_UseShortDistAngleRotation" init="true"/>
    <bool name="m_DisableAiCoverEntryStreamCheck" init="false"/>
    <float name="m_DistToUseShortestRotation" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
    <float name="m_InCoverTolerance" min="0.0f" max="0.1f" step="0.001f" init="0.05f"/>
    <float name="m_DotThresholdForCenterEnter" min="0.0f" max="1.0f" step="0.01f" init="0.8f"/>
    <float name="m_AiEntryMinRate" min="0.0f" max="1.0f" step="0.01f" init="0.9f"/>
    <float name="m_AiEntryMaxRate" min="0.0f" max="2.0f" step="0.01f" init="1.1f"/>
    <float name="m_PlayerSprintEntryRate" min="0.0f" max="2.0f" step="0.01f" init="1.1f"/>
    <float name="m_DefaultPlayerStandEntryStartMovementPhase" min="0.0f" max="1.0f" step="0.01f" init="0.225f"/>
    <float name="m_DefaultPlayerStandEntryEndMovementPhase" min="0.0f" max="1.0f" step="0.01f" init="0.45f"/>
    <float name="m_MaxAngleToBeginRotationScale" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_MaxDefaultAngularVelocity" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
    <float name="m_EnterCoverInterruptMinTime" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
    <float name="m_EnterCoverInterruptDistanceTolerance" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
    <float name="m_EnterCoverInterruptHeadingTolerance" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_EnterCoverAimInterruptDistanceTolerance" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
    <float name="m_EnterCoverAimInterruptHeadingTolerance" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <struct name="m_ThirdPersonAnimStateInfos" type="CTaskEnterCover::Tunables::AnimStateInfos"/>
    <struct name="m_FirstPersonAnimStateInfos" type="CTaskEnterCover::Tunables::AnimStateInfos"/>
    <array name="m_AIEnterCoverClips" type="atArray">
      <struct type="CTaskEnterCover::EnterClip"/>
    </array>
    <array name="m_AIStandEnterCoverClips" type="atArray">
      <struct type="CTaskEnterCover::StandingEnterClips"/>
    </array>
    <array name="m_AIEnterTransitionClips" type="atArray">
      <struct type="CTaskEnterCover::EnterClip"/>
    </array>
    <string name="m_EnterCoverAIAimingBase1H" type="atHashString" />
    <string name="m_EnterCoverAIAimingBase2H" type="atHashString" />
    <string name="m_EnterCoverAIAimingSwat1H" type="atHashString" />
    <string name="m_EnterCoverAIAimingSwat2H" type="atHashString" />
    <string name="m_EnterCoverAITransition1H" type="atHashString" />
    <string name="m_EnterCoverAITransition2H" type="atHashString" />
  </structdef>

  <structdef type="CTaskExitCover::ExitClip" >
    <string name="m_ExitClipId" type="atHashString" />
    <bitset name="m_Flags" type="fixed32" values="CTaskCover::eAnimFlags"/>
  </structdef>

  <structdef type="CTaskExitCover::Tunables::AnimStateInfos" >
    <array name="m_StandExitAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
  </structdef>
  
  <structdef type="CTaskExitCover::Tunables" base="CTuning">  
    <array name="m_CornerExitClips" type="atArray">
      <struct type="CTaskExitCover::ExitClip"/>
    </array>
    <string name="m_ExitCoverBaseClipSetId" type="atHashString" init="cover@move@base@core"/>
    <string name="m_ExitCoverBaseFPSClipSetId" type="atHashString" init="cover@first_person@move@base@core"/>
    <string name="m_ExitCoverExtraClipSetId" type="atHashString" init="cover@move@extra@core"/>
    <string name="m_ExitCoverExtraFPSClipSetId" type="atHashString" init="cover@move@extra@core"/>
    <float name="m_MinInputToInterruptIdle" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
    <float name="m_CornerExitHeadingModifier" min="0.0f" max="10.0f" step="0.1f" init="3.5f"/>
    <float name="m_ExitCornerZOffset" min="0.0f" max="3.0f" step="0.1f" init="0.25f"/>
    <float name="m_ExitCornerYOffset" min="0.0f" max="3.0f" step="0.1f" init="1.0f"/>
    <float name="m_ExitCornerDirOffset" min="0.0f" max="5.0f" step="0.1f" init="2.0f"/>
    <struct name="m_ThirdPersonAnimStateInfos" type="CTaskExitCover::Tunables::AnimStateInfos"/>
    <struct name="m_FirstPersonAnimStateInfos" type="CTaskExitCover::Tunables::AnimStateInfos"/>
  </structdef>

  <structdef type="CTaskInCover::ThrowProjectileClip" >
    <string name="m_IntroClipId" type="atHashString" />
    <string name="m_PullPinClipId" type="atHashString" />    
    <string name="m_BaseClipId" type="atHashString" />
    <string name="m_ThrowLongClipId" type="atHashString" />
    <string name="m_ThrowShortClipId" type="atHashString" />
    <string name="m_ThrowLongFaceCoverClipId" type="atHashString" />
    <string name="m_ThrowShortFaceCoverClipId" type="atHashString" />
    <bitset name="m_Flags" type="fixed32" values="CTaskCover::eAnimFlags"/>
  </structdef>

  <structdef type="CTaskInCover::Tunables" base="CTuning">
    <float name="m_MovementClipRate" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_TurnClipRate" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_MaxInputForIdleExit" min="-127.0f" max="127.0f" step="1.0f" init="20.0f"/>
    <float name="m_InputYAxisCornerExitValue" min="-127.0f" max="127.0f" step="1.0f" init="80.0f"/>
    <float name="m_ControlDebugXPos" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
    <float name="m_ControlDebugYPos" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
    <float name="m_ControlDebugRadius" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
    <float name="m_ControlDebugBeginAngle" min="-3.14f" max="3.14f" step="0.01f" init="0.0f"/>
    <float name="m_ControlDebugEndAngle" min="-3.14f" max="3.14f" step="0.01f" init="1.57f"/>
    <float name="m_MinStickInputToMoveInCover" min="0.0f" max="127.0f" step="1.0f" init="25.0f"/>
    <float name="m_MinStickInputXAxisToTurnInCover" min="0.0f" max="127.0f" step="1.0f" init="50.0f"/>  
    <float name="m_InputYAxisQuitValueAimDirect" min="-127.0f" max="127.0f" step="1.0f" init="-10.0f"/>
    <float name="m_InputYAxisQuitValue" min="-127.0f" max="127.0f" step="1.0f" init="-80.0f"/>
    <float name="m_StartExtendedProbeTime" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
    <float name="m_MinTimeToSpendInTask" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
    <float name="m_DesiredDistanceToCover" min="0.0f" max="1.0f" step="0.001f" init="0.025f"/>
    <float name="m_DesiredDistanceToCoverToRequestStep" min="0.0f" max="1.0f" step="0.001f" init="0.04f"/>
    <float name="m_OptimumDistToRightCoverEdgeCrouched" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_OptimumDistToLeftCoverEdgeCrouched" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_OptimumDistToRightCoverEdge" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
    <float name="m_OptimumDistToLeftCoverEdge" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
    <float name="m_MinMovingProbeOffset" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_MaxMovingProbeOffset" min="0.0f" max="3.0f" step="0.01f" init="0.65f"/>
    <float name="m_MinTurnProbeOffset" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_MaxTurnProbeOffset" min="0.0f" max="3.0f" step="0.01f" init="0.65f"/>
    <float name="m_DefaultProbeOffset" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_MinStoppingEdgeCheckProbeOffset" min="0.0f" max="2.0f" step="0.01f" init="0.4f"/>
    <float name="m_MaxStoppingEdgeCheckProbeOffset" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
    <float name="m_MinStoppingProbeOffset" min="0.0f" max="2.0f" step="0.01f" init="0.3f"/>
    <float name="m_MaxStoppingProbeOffset" min="0.0f" max="2.0f" step="0.01f" init="0.85f"/>
    <float name="m_HeadingChangeRate" min="0.0f" max="16.0f" step="0.1f" init="8.0f"/>
    <float name="m_MinTimeBeforeAllowingCornerMove" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_CrouchedLeftFireOffset" min="-1.0f" max="1.0f" step="0.01f" init="-0.22f"/>
    <float name="m_CrouchedRightFireOffset" min="-1.0f" max="1.0f" step="0.01f" init="0.325f"/>
    <float name="m_CoverLeftFireModifier" min="0.0f" max="1.6f" step="0.01f" init="0.8f"/>
    <float name="m_CoverRightFireModifier" min="0.0f" max="1.6f" step="0.01f" init="0.8f"/>
    <float name="m_CoverLeftFireModifierLow" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
    <float name="m_CoverRightFireModifierLow" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
    <float name="m_CoverLeftFireModifierCloseToEdge" min="0.0f" max="2.0f" step="0.01f" init="1.1f"/>
    <float name="m_CoverRightFireModifierCloseToEdge" min="0.0f" max="2.0f" step="0.01f" init="1.1f"/>
    <float name="m_CoverLeftIncreaseModifier" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
    <float name="m_CoverRightIncreaseModifier" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
    <float name="m_AimTurnCosAngleTolerance" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_InCoverMovementSpeedEnterCover" min="0.0f" max="20.0f" step="0.01f" init="10.0f"/>
    <float name="m_InCoverMovementSpeed" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
    <float name="m_SteppingMovementSpeed" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <bool name="m_UseAutoPeekAimFromCoverControls" init="true"/>
    <bool name="m_ComeBackInWhenAimDirectChangeInHighCover" init="false"/>
    <float name="m_AlternateControlStickInputThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_EdgeCapsuleRadius" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
    <float name="m_EdgeStartXOffset" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
    <float name="m_EdgeEndXOffset" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
    <float name="m_EdgeStartYOffset" min="0.0f" max="2.0f" step="0.01f" init="0.9f"/>
    <float name="m_EdgeEndYOffset" min="-2.0f" max="2.0f" step="0.01f" init="-0.2f"/>
    <float name="m_InsideEdgeStartYOffset" min="0.0f" max="2.0f" step="0.01f" init="0.0f"/>
    <float name="m_InsideEdgeEndYOffset" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
    <float name="m_InsideEdgeStartXOffset" min="0.0f" max="2.0f" step="0.01f" init="0.0f"/>
    <float name="m_InsideEdgeEndXOffset" min="-2.0f" max="2.0f" step="0.01f" init="0.0f"/>
    <float name="m_WallTestYOffset" min="0.0f" max="2.0f" step="0.01f" init="0.7f"/>
    <float name="m_InitialLowEdgeWallTestYOffset" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
    <float name="m_HighCloseEdgeWallTestYOffset" min="0.0f" max="2.0f" step="0.01f" init="0.9f"/>
    <float name="m_WallTestStartXOffset" min="0.0f" max="2.0f" step="0.01f" init="0.0f"/>
    <float name="m_WallTestEndXOffset" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
    <float name="m_WallHighTestZOffset" min="0.0f" max="2.0f" step="0.01f" init="0.15f"/>
    <float name="m_MovingEdgeTestStartYOffset" min="-1.0f" max="3.0f" step="0.01f" init="1.25f"/>
    <float name="m_MovingEdgeTestEndYOffset" min="-1.0f" max="3.0f" step="0.01f" init="-0.2f"/>
    <float name="m_CoverToCoverEdgeTestStartYOffset" min="-1.0f" max="3.0f" step="0.01f" init="0.4f"/>
    <float name="m_CoverToCoverEdgeTestEndYOffset" min="-1.0f" max="3.0f" step="0.01f" init="-0.2f"/>
    <float name="m_SteppingEdgeTestStartYOffset" min="-1.0f" max="3.0f" step="0.01f" init="0.6f"/>
    <float name="m_SteppingEdgeTestEndYOffset" min="-1.0f" max="3.0f" step="0.01f" init="-0.2f"/>
    <float name="m_InitialLowEdgeTestStartYOffset" min="-1.0f" max="3.0f" step="0.01f" init="1.0f"/>
    <float name="m_InitialLowEdgeTestEndYOffset" min="-1.0f" max="3.0f" step="0.01f" init="-0.2f"/>
    <float name="m_EdgeHighZOffset" min="-2.0f" max="2.0f" step="0.01f" init="0.25f"/>
    <float name="m_EdgeLowZOffset" min="-2.0f" max="2.0f" step="0.01f" init="-0.25f"/>
    <float name="m_EdgeMinimumOffsetDiff" min="0.0f" max="4.0f" step="0.01f" init="0.05f"/>
    <float name="m_EdgeMaximumOffsetDiff" min="0.0f" max="4.0f" step="0.01f" init="1.0f"/>
    <float name="m_PinnedDownPeekChance" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_PinnedDownBlindFireChance" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>    
    <float name="m_MinTimeBeforeAllowingAutoPeek" min="0.0f" max="4.0f" step="0.01f" init="0.5f"/>
    <bool name="m_EnableAimDirectlyIntros" init="false"/>
    <float name="m_PedDirToPedCoverCosAngleTol" min="-1.0f" max="1.0f" step="0.01f" init="0.95f"/>
    <float name="m_CamToPedDirCosAngleTol" min="-1.0f" max="1.0f" step="0.01f" init="-0.25f"/>
    <float name="m_CamToCoverDirCosAngleTol" min="-1.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_MinDistanceToTargetForPeek" min="0.0f" max="100.0f" step="0.05f" init="8.0f"/>
    <float name="m_TimeBetweenPeeksWithoutLOS" min="0.0f" max="100.0f" step="0.05f" init="3.5f"/>
    <array name="m_ThrowProjectileClips" type="atArray">
      <struct type="CTaskInCover::ThrowProjectileClip"/>
    </array>
    <s32 name="m_RecreateWeaponTime" init="750" min="0" max="2000"/>
    <float name="m_BlindFireHighCoverMinPitchLimit" min="-1.5f" max="1.5f" step="0.01f" init="-0.4f"/>
    <float name="m_BlindFireHighCoverMaxPitchLimit" min="-1.5f" max="1.5f" step="0.01f" init="0.25f"/>
    <bool name="m_EnableLeftHandIkInCover" init="false"/>
    <bool name="m_EnableReloadingWhilstMovingInCover" init="true"/>
    <float name="m_AimIntroRateForAi" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_AimOutroRateForAi" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_MinReactToFireRate" min="0.0f" max="5.0f" step="0.01f" init="0.9f"/>
    <float name="m_MaxReactToFireRate" min="0.0f" max="5.0f" step="0.01f" init="1.15f"/>
    <float name="m_MaxReactToFireDelay" min="0.0f" max="5.0f" step="0.01f" init="0.35f"/>
    <float name="m_MinTimeUntilReturnToIdleFromAimAfterAimedAt" min="0.0f" max="2.0f" step="0.01f" init="0.35f"/>
    <float name="m_MaxTimeUntilReturnToIdleFromAimAfterAimedAt" min="0.0f" max="2.0f" step="0.01f" init="0.40f"/>
    <float name="m_MinTimeUntilReturnToIdleFromAimDefault" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_MaxTimeUntilReturnToIdleFromAimDefault" min="0.0f" max="5.0f" step="0.01f" init="3.0f"/>
    <float name="m_GlobalLateralTorsoOffsetInLeftCover" min="-10.0f" max="10.0f" step="0.01f" init="5.0f"/>
    <float name="m_WeaponLongBlockingOffsetInHighLeftCover" min="-2.0f" max="2.0f" step="0.01f" init="-0.2"/>
    <float name="m_WeaponLongBlockingOffsetInLeftCover" min="-2.0f" max="2.0f" step="0.01f" init="-0.275f"/>
    <float name="m_WeaponLongBlockingOffsetInLeftCoverFPS" min="-2.0f" max="2.0f" step="0.01f" init="-0.135f"/>
    <float name="m_WeaponBlockingOffsetInLeftCover" min="-2.0f" max="2.0f" step="0.01f" init="-0.275f"/>
    <float name="m_WeaponBlockingOffsetInLeftCoverFPS" min="-2.0f" max="2.0f" step="0.01f" init="-0.12f"/>
    <float name="m_WeaponBlockingOffsetInRightCover" min="-2.0f" max="2.0f" step="0.01f" init="0.2f"/>
    <float name="m_WeaponBlockingOffsetInRightCoverFPS" min="-2.0f" max="2.0f" step="0.01f" init="0.00f"/>
    <float name="m_WeaponBlockingLengthLongWeaponOffset" min="-2.0f" max="2.0f" step="0.01f" init="-0.2f"/>
    <float name="m_WeaponBlockingLengthLongWeaponOffsetFPS" min="-2.0f" max="2.0f" step="0.01f" init="-0.2f"/>
    <float name="m_WeaponBlockingLengthOffset" min="-2.0f" max="2.0f" step="0.01f" init="-0.075f"/>
    <float name="m_WeaponBlockingLengthOffsetFPS" min="-2.0f" max="2.0f" step="0.01f" init="-0.188"/>
    <string name="m_CoverStepClipSetId" type="atHashString" />
    <float name="m_PinnedDownTakeCoverAmount" min="0.0f" max="100.0f" step="0.1f" init="40.0f"/>
    <float name="m_AmountPinnedDownByDamage" min="0.0f" max="100.0f" step="0.1f" init="100.0f"/>
    <float name="m_AmountPinnedDownByBullet" min="0.0f" max="100.0f" step="0.1f" init="25.0f"/>
    <float name="m_AmountPinnedDownByWitnessKill" min="0.0f" max="100.0f" step="0.1f" init="50.0f"/>
    <float name="m_PinnedDownByBulletRange" min="0.0f" max="100.0f" step="0.05f" init="3.0f"/>
    <float name="m_PinnedDownDecreaseAmountPerSecond" min="0.0f" max="100.0f" step="0.01f" init="30.0f"/>
    <float name="m_AimIntroTaskBlendOutDuration" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
    <u32 name="m_MinTimeToBePinnedDown" min="0" max="10000" step="10" init="2000"/>
    <float name="m_TimeBetweenBurstsMaxRandomPercent" min="0.0f" max="1.0f" step="0.01f" init="0.125f"/>
    <u32 name="m_AimOutroDelayTime" min="0" max="10000" step="10" init="200"/>
    <u32 name="m_AimOutroDelayTimeFPS" min="0" max="10000" step="10" init="0"/>
    <u32 name="m_AimOutroDelayTimeFPSScope" min="0" max="10000" step="10" init="80"/>
    <bool name="m_AimOutroDelayWhenPeekingOnly" init="true"/>
    <bool name="m_EnableAimOutroDelay" init="false"/>
    <string name="m_ThrowProjectileClipSetId" type="atHashString" />
    <string name="m_ThrowProjectileFPSClipSetId" type="atHashString" init="Cover_Wpn_Thrown_Grenade" />
  </structdef>

  <structdef type="CTaskAimGunFromCoverIntro::AimIntroClip" >
    <array name="m_Clips" type="atArray">
      <string type="atHashString" />
    </array>
    <bitset name="m_Flags" type="fixed32" values="CTaskCover::eAnimFlags"/>
  </structdef>

  <structdef type="CTaskAimGunFromCoverIntro::AimStepInfo" >
    <float name="m_StepOutX" min="0.0f" max="3.0f" step="0.001f" init="0.5f"/>
    <float name="m_StepOutY" min="0.0f" max="3.0f" step="0.001f" init="0.5f"/>
    <float name="m_StepTransitionMinAngle" min="-3.14f" max="3.14f" step="0.001f" init="0.5f"/>
    <float name="m_StepTransitionMaxAngle" min="-3.14f" max="3.14f" step="0.001f" init="0.5f"/>
    <float name="m_PreviousTransitionExtraScalar" min="-1.0f" max="1.0f" step="0.001f" init="0.05f"/>
    <float name="m_NextTransitionExtraScalar" min="-1.0f" max="1.0f" step="0.001f" init="0.05f"/>
    <string name="m_PreviousTransitionClipId" type="atHashString" />
    <string name="m_NextTransitionClipId" type="atHashString" />
  </structdef>

  <structdef type="CTaskAimGunFromCoverIntro::AimStepInfoSet" >
    <array name="m_StepInfos" type="atArray">
      <struct type="CTaskAimGunFromCoverIntro::AimStepInfo"/>
    </array>
  </structdef>

  <structdef type="CTaskAimGunFromCoverIntro::Tunables" base="CTuning">
    <float name="m_UpperBodyAimBlendInDuration" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
    <float name="m_IntroMovementDuration" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
    <bool name="m_DisableIntroScaling" init="false"/>
    <bool name="m_DisableRotationScaling" init="false"/>
    <bool name="m_DisableIntroOverShootCheck" init="false"/>
    <bool name="m_UseConstantIntroScaling" init="false"/>
    <bool name="m_RenderArcsAtCoverPosition" init="false"/>
    <bool name="m_RenderAimArcDebug" init="false"/>
    <bool name="m_UseMoverPositionWhilePeeking" init="true"/>
    <bool name="m_DisableWeaponBlocking" init="false"/>
    <bool name="m_DisableTranslationScaling" init="true"/>
    <float name="m_ArcRadius" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
    <float name="m_IntroScalingDefaultStartPhase" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
    <float name="m_IntroScalingDefaultEndPhase" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_IntroRotScalingDefaultStartPhase" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
    <float name="m_IntroRotScalingDefaultEndPhase" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_AiAimIntroCloseEnoughTolerance" min="0.0f" max="1.0f" step="0.01f" init="0.02f"/>
    <float name="m_MaxStepBackDist" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_MinStepOutDist" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxStepOutDist" min="0.0f" max="5.0f" step="0.01f" init="1.5f"/>
    <float name="m_IntroMaxScale" min="0.0f" max="10.0f" step="0.01f" init="5.0f"/>
    <float name="m_IntroRate" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_IntroRateToPeekFPS" min="0.0f" max="5.0f" step="0.01f" init="1.5f"/>
    <float name="m_OutroRate" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_OutroRateFromPeekFPS" min="0.0f" max="5.0f" step="0.01f" init="1.5f"/>
    <float name="m_SteppingApproachRateSlow" min="0.0f" max="30.0f" step="0.01f" init="0.35f"/>
    <float name="m_SteppingApproachRate" min="0.0f" max="30.0f" step="0.01f" init="0.75f"/>
    <float name="m_SteppingApproachRateFast" min="0.0f" max="30.0f" step="0.01f" init="1.75f"/>
    <float name="m_SteppingHeadingApproachRate" min="0.0f" max="30.0f" step="0.01f" init="4.0f"/>
    <float name="m_MinRotationalSpeedScale" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxRotationalSpeedScale" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
    <float name="m_HeadingReachedTolerance" min="0.0f" max="0.5f" step="0.001f" init="0.05f"/>
    <float name="m_StepOutCapsuleRadiusScale" min="0.0f" max="1.0f" step="0.001f" init="0.7f"/>
    <float name="m_AimDirectlyMaxAngle" min="0.0f" max="3.14f" step="0.001f" init="1.74f"/> 
    <float name="m_StepOutLeftX" min="-3.0f" max="0.0f" step="0.001f" init="-0.5f"/>
    <float name="m_StepOutRightX" min="0.0f" max="3.0f" step="0.001f" init="0.5f"/>
    <float name="m_StepOutY" min="-3.0f" max="3.0f" step="0.001f" init="-0.5f"/>
    <float name="m_LowXClearOffsetCapsuleTest" min="-3.0f" max="3.0f" step="0.001f" init="0.5f"/>
    <float name="m_LowXOffsetCapsuleTest" min="-3.0f" max="3.0f" step="0.001f" init="0.0f"/>
    <float name="m_LowYOffsetCapsuleTest" min="-3.0f" max="3.0f" step="0.001f" init="0.0f"/>
    <float name="m_LowZOffsetCapsuleTest" min="-3.0f" max="3.0f" step="0.001f" init="0.0f"/>
    <float name="m_LowOffsetCapsuleLength" min="0.0f" max="3.0f" step="0.001f" init="0.25f"/>
    <float name="m_LowOffsetCapsuleRadius" min="0.0f" max="3.0f" step="0.001f" init="0.25f"/>
    <Vector2 name="m_LowLeftStep" min="-3.0f" max="3.0f" step="0.001f" init="-0.1f, 0.5f"/>
    <Vector2 name="m_LowRightStep" min="-3.0f" max="3.0f" step="0.001f" init="0.1f, 0.5f"/>
    <float name="m_LowBlockedBlend" min="0.0f" max="1.0f" step="0.001f" init="0.5f"/>
    <float name="m_LowStepOutLeftXBlocked" min="-3.0f" max="3.0f" step="0.001f" init="1.0f"/>
    <float name="m_LowStepOutLeftYBlocked" min="-3.0f" max="3.0f" step="0.001f" init="0.5f"/>
    <float name="m_LowStepBackLeftXBlocked" min="-3.0f" max="3.0f" step="0.001f" init="0.0f"/>
    <float name="m_LowStepBackLeftYBlocked" min="-3.0f" max="3.0f" step="0.001f" init="1.0f"/>
    <float name="m_LowStepOutRightXBlocked" min="-3.0f" max="3.0f" step="0.001f" init="1.0f"/>
    <float name="m_LowStepOutRightYBlocked" min="-3.0f" max="3.0f" step="0.001f" init="0.5f"/>
    <float name="m_LowStepBackRightXBlocked" min="-3.0f" max="3.0f" step="0.001f" init="0.0f"/>
    <float name="m_LowStepBackRightYBlocked" min="-3.0f" max="3.0f" step="0.001f" init="1.0f"/>
    <float name="m_LowSideZOffset" min="-3.0f" max="3.0f" step="0.001f" init="1.0f"/>
    <float name="m_DistConsideredAtAimPosition" min="0.0f" max="1.0f" step="0.001f" init="0.025f"/>
    <float name="m_MinPhaseToApplyExtraHeadingAi" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_MinPhaseToApplyExtraHeadingPlayer" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_MaxAngularHeadingVelocityAi" min="0.0f" max="10.0f" step="0.01f" init="4.0f"/>
    <float name="m_MaxAngularHeadingVelocityPlayer" min="0.0f" max="10.0f" step="0.01f" init="7.5f"/>
    <float name="m_MaxAngularHeadingVelocityPlayerForcedStandAim" min="0.0f" max="20.0f" step="0.01f" init="10.0f"/>
    <struct name="m_HighLeftAimStepInfoSet" type="CTaskAimGunFromCoverIntro::AimStepInfoSet"/>
    <struct name="m_HighRightAimStepInfoSet" type="CTaskAimGunFromCoverIntro::AimStepInfoSet"/>
    <array name="m_AimIntroClips" type="atArray">
      <struct type="CTaskAimGunFromCoverIntro::AimIntroClip"/>
    </array>
  </structdef>

  <structdef type="CTaskAimGunFromCoverOutro::Tunables::AnimStateInfos" >
    <array name="m_AimOutroAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
  </structdef>

  <structdef type="CTaskAimGunFromCoverOutro::Tunables" base="CTuning">
    <bool name="m_DisableOutroScaling" init="false"/>
    <bool name="m_DisableRotationScaling" init="false"/>
    <bool name="m_DisableOutroOverShootCheck" init="false"/>
    <bool name="m_UseConstantOutroScaling" init="false"/>
    <float name="m_OutroRotationScalingDefaultStartPhase" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_OutroRotationScalingDefaultEndPhase" min="0.0f" max="1.0f" step="0.01f" init="0.6f"/>
    <float name="m_OutroScalingDefaultStartPhase" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_OutroScalingDefaultEndPhase" min="0.0f" max="1.0f" step="0.01f" init="0.6f"/>
    <float name="m_OutroMaxScale" min="0.0f" max="10.0f" step="0.01f" init="5.0f"/>
    <float name="m_AdditionalModifier" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_EndHeadingTolerance" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_DesiredDistanceToCover" min="0.0f" max="1.0f" step="0.01f" init="0.225f"/>
    <float name="m_InCoverMovementSpeed" min="0.0f" max="50.0f" step="0.1f" init="15.0f"/>
    <float name="m_OutroMovementDuration" min="0.0f" max="5.0f" step="0.01f" init="0.4f"/>
    <float name="m_UpperBodyAimBlendOutDuration" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_MaxAngularHeadingVelocityAi" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
    <float name="m_MaxAngularHeadingVelocityPlayer" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
    <float name="m_MaxAngularHeadingVelocityPlayerFPS" min="0.0f" max="10.0f" step="0.01f" init="7.5f"/>
    <float name="m_MaxAngularHeadingVelocityPlayerForcedStandAim" min="0.0f" max="20.0f" step="0.01f" init="6.0f"/>
    <struct name="m_ThirdPersonAnimStateInfos" type="CTaskAimGunFromCoverOutro::Tunables::AnimStateInfos"/>
    <struct name="m_FirstPersonAnimStateInfos" type="CTaskAimGunFromCoverOutro::Tunables::AnimStateInfos"/>
  </structdef>

  <structdef type="CTaskAimGunBlindFire::BlindFirePitchAnimAngles" >
    <float name="m_MinPitchAngle" min="-1.57f" max="1.57f" step="0.01f" init="-0.75f"/>
    <float name="m_MaxPitchAngle" min="-1.57f" max="1.57f" step="0.01f" init="0.75f"/>
    <float name="m_MinPitchAngle2H" min="-1.57f" max="1.57f" step="0.01f" init="-0.75f"/>
    <float name="m_MaxPitchAngle2H" min="-1.57f" max="1.57f" step="0.01f" init="0.75f"/>
  </structdef>

  <structdef type="CTaskAimGunBlindFire::BlindFireAnimStateInfoNew" >
    <string name="m_IntroClip0Id" type="atHashString" />
    <string name="m_IntroClip1Id" type="atHashString" />
    <string name="m_SweepClip0Id" type="atHashString" />
    <string name="m_SweepClip1Id" type="atHashString" />
    <string name="m_SweepClip2Id" type="atHashString" />
    <string name="m_OutroClip0Id" type="atHashString" />
    <string name="m_OutroClip1Id" type="atHashString" />
    <string name="m_CockGunClip0Id" type="atHashString" />
    <string name="m_CockGunClip1Id" type="atHashString" />
    <string name="m_CockGunWeaponClipId" type="atHashString" />    
    <bitset name="m_Flags" type="fixed32" values="CTaskCover::eAnimFlags"/>
    <float name="m_MinHeadingAngle" min="-3.14f" max="3.14f" step="0.01f" init="-1.57f"/>
    <float name="m_MaxHeadingAngle" min="-3.14f" max="3.14f" step="0.01f" init="1.57f"/>
    <struct name="m_ThirdPersonBlindFirePitchAngles" type="CTaskAimGunBlindFire::BlindFirePitchAnimAngles"/>
    <struct name="m_FirstPersonPersonBlindFirePitchAngles" type="CTaskAimGunBlindFire::BlindFirePitchAnimAngles"/>
  </structdef>
  
  <structdef type="CTaskAimGunBlindFire::Tunables" base="CTuning">
    <bool name="m_RemoveReticuleDuringBlindFire" init="false"/>
    <bool name="m_DontRemoveReticuleDuringBlindFireNew" init="false"/>    
    <float name="m_LowBlindFireAimingDirectlyLimitAngle" min="0.0f" max="3.14f" step="0.01f" init="1.57f"/>
    <float name="m_HighBlindFireAimingDirectlyLimitAngle" min="0.0f" max="3.14f" step="0.01f" init="1.57f"/>
    <array name="m_BlindFireAnimStateNewInfos" type="atArray">
      <struct type="CTaskAimGunBlindFire::BlindFireAnimStateInfoNew"/>
    </array>
  </structdef>

  <structdef type="CTaskMotionInCover::Tunables::AnimStateInfos" >
    <array name="m_IdleAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_AtEdgeAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_PeekingAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_StoppingAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_MovingAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_EdgeTurnAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_CoverToCoverAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_SteppingAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_WalkStartAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_SettleAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_TurnEnterAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_TurnEndAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_TurnWalkStartAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <string name="m_WeaponHoldingFilterId" type="atHashString" init="upperbody_filter" />
  </structdef>

    <structdef type="CTaskMotionInCover::Tunables" base="CTuning">
    <float name="m_CoverToCoverClipRate" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <bool name="m_DisableCoverToCoverTranslationScaling" init="false"/>
    <bool name="m_DisableCoverToCoverRotationScaling" init="false"/>
    <bool name="m_UseButtonToMoveAroundCorner" init="false"/>
    <bool name="m_EnableCoverToCover" init="true"/>
    <bool name="m_EnableWalkStops" init="true"/>
    <bool name="m_EnableCoverPeekingVariations" init="true"/>
    <bool name="m_EnableCoverPinnedVariations" init="true"/>
    <bool name="m_EnableCoverIdleVariations" init="true"/>
    <bool name="m_UseSprintButtonForCoverToCover" init="false"/>
    <u8 name="m_VerifyCoverInterval" min="0" max="255" step="1" init="45"/>
    <u32 name="m_MinTimeForCornerMove" min="0" max="1000" step="1" init="500"/>
    <float name="m_DefaultSettleBlendDuration" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
    <float name="m_HeightChangeSettleBlendDuration" min="0.0f" max="2.0f" step="0.01f" init="0.35f"/>
    <float name="m_MinTimeStayPinned" min="0.0f" max="10.0f" step="0.01f" init="0.2f"/>
    <float name="m_MaxTimeStayPinned" min="0.0f" max="10.0f" step="0.01f" init="1.2f"/>
    <u32 name="m_PinnedDownThreshold" min="0" max="100" step="1" init="40"/>
    <bool name="m_ForcePinnedDown" init="false"/>
    <float name="m_MinDistanceToTargetForIdleVariations" min="0.0f" max="100.0f" step="0.05f" init="5.0f"/>
    <float name="m_MinTimeBetweenIdleVariations" min="0.0f" max="10.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxTimeBetweenIdleVariations" min="0.0f" max="10.0f" step="0.01f" init="4.0f"/>
    <float name="m_MinWaitTimeToPlayPlayerIdleVariations" min="0.0f" max="20.0f" step="0.01f" init="3.5f"/>
    <float name="m_MinTimeBetweenPlayerIdleVariations" min="0.0f" max="10.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxTimeBetweenPlayerIdleVariations" min="0.0f" max="10.0f" step="0.01f" init="4.0f"/>
    <float name="m_CoverToCoverDuration" min="0.0f" max="3.0f" step="0.01f" init="1.0f"/>
    <float name="m_CoverToCoverMinScalePhase" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_CoverToCoverMaxScalePhase" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
    <float name="m_CoverToCoverMinRotScalePhase" min="0.0f" max="1.0f" step="0.01f" init="0.9f"/>
    <float name="m_CoverToCoverMaxRotScalePhase" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
    <float name="m_MaxRotationalSpeedScale" min="0.0f" max="25.0f" step="0.01f" init="3.0f"/>
    <float name="m_MaxRotationalSpeed" min="0.0f" max="25.0f" step="0.01f" init="2.0f"/>
    <float name="m_MinStickInputToEnableMoveAroundCorner" min="0.0f" max="127.0f" step="1.0f" init="50.0f"/>
    <float name="m_MinStickInputToEnableCoverToCover" min="0.0f" max="127.0f" step="1.0f" init="50.0f"/>
    <float name="m_MinStickInputToMoveAroundCorner" min="0.0f" max="127.0f" step="1.0f" init="50.0f"/>
    <float name="m_MaxStoppingDuration" min="0.0f" max="2.0f" step="0.1f" init="1.0f"/>
    <float name="m_MinStoppingDist" min="0.0f" max="0.5f" step="0.001f" init="0.025f"/>
    <float name="m_MinTimeToScale" min="0.0f" max="0.5f" step="0.001f" init="0.025f"/>
    <float name="m_CTCDepthDistanceCompletionOffset" min="0.0f" max="2.5f" step="0.001f" init="1.25f"/>
    <float name="m_EdgeLowCoverMoveTime" min="0.0f" max="4.0f" step="0.01f" init="1.0f"/>
    <float name="m_MinTimeToStandUp" min="0.0f" max="4.0f" step="0.01f" init="1.0f"/>
    <float name="m_CoverToCoverMinDistToScale" min="0.0f" max="0.5f" step="0.001f" init="0.05f"/>
    <float name="m_CoverToCoverMinAngToScale" min="0.0f" max="0.5f" step="0.001f" init="0.05f"/>
    <float name="m_CoverToCoverMinAng" min="0.0f" max="0.1f" step="0.001f" init="0.01f"/>
    <float name="m_CoverToCoverDistTol" min="0.0f" max="0.1f" step="0.001f" init="0.01f"/>
    <float name="m_CoverToCoverMaxDistToStep" min="0.0f" max="0.5f" step="0.001f" init="0.1f"/>
    <float name="m_CoverToCoverAngTol" min="0.0f" max="0.1f" step="0.001f" init="0.01f"/>
    <float name="m_CoverToCoverMaxAngToStep" min="0.0f" max="0.5f" step="0.001f" init="0.1f"/>
    <float name="m_CoverToCoverMaxAccel" min="0.0f" max="50.0f" step="0.1f" init="10.f"/>
    <float name="m_ForwardDistToStartSideScale" min="0.0f" max="2.5f" step="0.01f" init="1.0f"/>
    <float name="m_CoverToCoverMinDepthToScale" min="0.0f" max="0.5f" step="0.001f" init="0.05f"/>
    <float name="m_CoverToCoverSmallAnimDist" min="0.0f" max="0.5f" step="0.001f" init="0.05f"/>
    <float name="m_HeadingReachedTolerance" min="0.0f" max="0.5f" step="0.001f" init="0.05f"/>
    <float name="m_BlendToIdleTime" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
    <float name="m_InsideCornerStopDistance" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>    
    <float name="m_CoverHeadingCloseEnough" min="0.0f" max="0.5f" step="0.01f" init="0.01f"/>
    <float name="m_CoverHeadingCloseEnoughTurn" min="0.0f" max="0.5f" step="0.01f" init="0.5f"/>
    <float name="m_CoverPositionCloseEnough" min="0.0f" max="0.5f" step="0.01f" init="0.05f"/>
    <float name="m_DefaultStillToTurnBlendDuration" min="0.0f" max="0.5f" step="0.01f" init="0.07f"/>
    <float name="m_DefaultEdgeTurnBlendDuration" min="0.0f" max="0.5f" step="0.01f" init="0.25f"/>
    <float name="m_PeekToEdgeTurnBlendDuration" min="0.0f" max="0.5f" step="0.01f" init="0.25f"/>
    <float name="m_MaxMoveSpeedInCover" min="0.0f" max="10.0f" step="0.01f" init="4.0f"/>
    <float name="m_MinEdgeDistanceForStoppingAnim" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>    
    <bool name="m_UseNewStepAndWalkStarts" init="true"/>
    <bool name="m_UseNewTurns" init="true"/>
    <bool name="m_UseNewTurnWalkStarts" init="true"/>
    <string name="m_CoreMotionClipSetId" type="atHashString" />
    <string name="m_CoreMotionFPSClipSetId" type="atHashString" />
    <string name="m_CoreAIMotionClipSetId" type="atHashString" />
    <array name="m_PeekingVariationAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_PeekingLow1HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PeekingLow2HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PeekingHigh1HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PeekingHigh2HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PinnedLow1HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PinnedLow2HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PinnedHigh1HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PinnedHigh2HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_OutroReact1HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_OutroReact2HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>    
    <array name="m_IdleLow1HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_IdleLow2HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_IdleHigh1HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_IdleHigh2HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PlayerIdleLow0HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PlayerIdleLow1HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PlayerIdleLow2HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PlayerIdleHigh0HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PlayerIdleHigh1HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PlayerIdleHigh2HVariationClipsets" type="atArray">
      <string type="atHashString" />
    </array>
    <array name="m_PinnedIntroAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_PinnedIdleAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_PinnedOutroAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <array name="m_IdleVariationAnimStateInfos" type="atArray">
      <struct type="CTaskCover::CoverAnimStateInfo"/>
    </array>
    <struct name="m_ThirdPersonAnimStateInfos" type="CTaskMotionInCover::Tunables::AnimStateInfos"/>
    <struct name="m_FirstPersonAnimStateInfos" type="CTaskMotionInCover::Tunables::AnimStateInfos"/>
    <bool name="m_EnableFirstPersonLocoAnimations" init="false"/>
    <bool name="m_EnableFirstPersonAimingAnimations" init="false"/>
    <bool name="m_EnableAimOutroToPeekAnims" init="false"/>
    <bool name="m_EnableBlindFireToPeek" init="true"/>
  </structdef>

  <structdef type="CAiCoverClipVariationHelper::Tunables" base="CTuning">
    <u32 name="m_MinUsesForPeekingVariationChange" min="0" max="32" step="1" init="4"/>
    <u32 name="m_MaxUsesForPeekingVariationChange" min="0" max="32" step="1" init="12"/>
    <u32 name="m_MinUsesForPinnedVariationChange" min="0" max="32" step="1" init="4"/>
    <u32 name="m_MaxUsesForPinnedVariationChange" min="0" max="32" step="1" init="12"/>
    <u32 name="m_MinUsesForOutroReactVariationChange" min="0" max="32" step="1" init="4"/>
    <u32 name="m_MaxUsesForOutroReactVariationChange" min="0" max="32" step="1" init="12"/>    
    <u32 name="m_MinUsesForIdleVariationChange" min="0" max="32" step="1" init="4"/>
    <u32 name="m_MaxUsesForIdleVariationChange" min="0" max="32" step="1" init="12"/>
  </structdef>

  <structdef type="CPlayerCoverClipVariationHelper::Tunables" base="CTuning">
    <u32 name="m_MinUsesForIdleVariationChange" min="0" max="32" step="1" init="4"/>
    <u32 name="m_MaxUsesForIdleVariationChange" min="0" max="32" step="1" init="12"/>
  </structdef>

  <structdef type="CDynamicCoverHelper::Tunables" base="CTuning">
    <bool name="m_EnableConflictingNormalCollisionRemoval" init="true"/>
    <bool name="m_UseStickHistoryForCoverSearch" init="true"/>
    <u32 name="m_StickDownDuration" min="0" max="2000" step="10" init="200"/>
    <float name="m_StickDownMinRange" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
    <bool name="m_UseCameraOrientationWeighting" init="true"/>
    <bool name="m_UseCameraOrientationWhenStill" init="true"/>
    <bool name="m_UseCameraOrientationForBackwardsDirection" init="true"/>
    <float name="m_BehindThreshold" min="-1.0f" max="1.0f" step="0.01f" init="-0.9f"/>
    <float name="m_DistanceToWallStanding" min="-1.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_DistanceToWallCrouching" min="-1.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_DistanceToWallCoverToCover" min="-1.0f" max="1.0f" step="0.01f" init="0.2f"/>    
    <float name="m_OCMCrouchedForwardClearanceOffset" min="0.0f" max="2.0f" step="0.01f" init="0.6f"/>
    <float name="m_OCMStandingForwardClearanceOffset" min="0.0f" max="2.0f" step="0.01f" init="0.7f"/>
    <float name="m_OCMSideClearanceDepth" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_OCMClearanceCapsuleRadius" min="0.0f" max="2.0f" step="0.1f" init="0.25f"/>
    <float name="m_OCMSideTestDepth" min="0.0f" max="5.0f" step="0.01f" init="0.8f"/>
    <float name="m_OCMCrouchedHeightOffset" min="-1.0f" max="1.0f" step="0.01f" init="-0.15f"/>
    <float name="m_OCMStandingHeightOffset" min="-1.0f" max="1.0f" step="0.01f" init="0.15f"/>
    <float name="m_CTCSideOffset" min="-5.0f" max="5.0f" step="0.01f" init="-0.5f"/>
    <float name="m_CTCProbeDepth" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
    <float name="m_CTCForwardOffset" min="0.0f" max="10.0f" step="0.1f" init="1.5f"/>
    <float name="m_CTCSpacingOffset" min="0.0f" max="2.0f" step="0.01f" init="0.2f"/>
    <float name="m_CTCCapsuleRadius" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_CTCHeightOffset" min="-1.0f" max="1.0f" step="0.01f" init="-0.25f"/>
    <float name="m_LowCoverProbeHeight" min="-1.0f" max="1.0f" step="0.01f" init="-0.2f"/>
    <float name="m_HighCoverProbeHeight" min="-1.0f" max="1.0f" step="0.01f" init="0.45f"/>
    <float name="m_CTCClearanceCapsuleRadius" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_CTCClearanceCapsuleStartForwardOffset" min="-2.0f" max="2.0f" step="0.01f" init="0.75f"/>
    <float name="m_CTCClearanceCapsuleEndForwardOffset" min="-2.0f" max="2.0f" step="0.01f" init="-0.75f"/>
    <float name="m_CTCClearanceCapsuleStartZOffset" min="-1.0f" max="1.0f" step="0.01f" init="-0.25f"/>
    <float name="m_CTCClearanceCapsuleEndZOffset" min="-2.0f" max="2.0f" step="0.01f" init="0.75f"/>
    <float name="m_CTCMinDistance" min="0.0f" max="5.0f" step="0.01f" init="1.5f"/>
    <float name="m_VehicleEdgeProbeXOffset" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_VehicleEdgeProbeZOffset" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_MaxZDiffBetweenCoverPoints" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_MaxZDiffBetweenPedPos" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
    <float name="m_MaxHeadingDiffBetweenCTCPoints" min="0.0f" max="3.14f" step="0.01f" init="0.785f"/>
    <float name="m_PedToCoverCapsuleRadius" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_PedToCoverEndPullBackDistance" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
    <float name="m_PedToCoverEndZOffset" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
    <float name="m_MaxStickInputAngleInfluence" min="0.0f" max="3.14f" step="0.1f" init="0.78f"/>
    <float name="m_IdleYStartOffset" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
  </structdef>

  <structdef type="CClipScalingHelper::Tunables" base="CTuning">
    <bool name="m_DisableRotationScaling" init="false"/>
    <bool name="m_DisableRotationOvershoot" init="false"/>
    <bool name="m_DisableTranslationScaling" init="false"/>
    <bool name="m_DisableTranslationOvershoot" init="false"/>
    <float name="m_MinVelocityToScale" min="0.0f" max="0.5f" step="0.0001f" init="0.05f"/>
    <float name="m_MaxTransVelocity" min="0.0f" max="20.0f" step="0.0001f" init="8.0f"/>
    <float name="m_MinRemainingAnimDurationToScale" min="0.0f" max="0.5f" step="0.0001f" init="0.01f"/>
    <float name="m_MinAnimRotationDeltaToScale" min="0.0f" max="1.0f" step="0.0001f" init="0.05f"/>
    <float name="m_MinAnimTranslationDeltaToScale" min="0.0f" max="1.0f" step="0.0001f" init="0.01f"/>
    <float name="m_MinCurrentRotationDeltaToScale" min="0.0f" max="1.0f" step="0.0001f" init="0.05f"/>
    <float name="m_DefaultMinRotationScalingValue" min="0.0f" max="50.0f" step="0.01f" init="0.2f"/>
    <float name="m_DefaultMaxRotationScalingValue" min="0.0f" max="50.0f" step="0.01f" init="4.0f"/>
    <float name="m_DefaultMinTranslationScalingValue" min="0.0f" max="50.0f" step="0.01f" init="0.2f"/>
    <float name="m_DefaultMaxTranslationScalingValue" min="0.0f" max="50.0f" step="0.01f" init="4.0f"/>
  </structdef>

  <structdef type="CTaskDraggingToSafety::Tunables::ObstructionProbe">
    <float name="m_Height" min="0.0f" max="5.0f" step="0.1f" init="2.0f"/>
    <float name="m_Radius" min="0.0f" max="3.0f" step="0.1f" init="0.35f"/>
    <float name="m_ExtraHeightForGround" min="0.0f" max="2.0f" step="0.1f" init="0.5f"/>
  </structdef>
  
  <structdef type="CTaskDraggingToSafety::Tunables::Rendering">
    <bool name="m_Enabled" init="false"/>
    <bool name="m_ObstructionProbe" init="true"/>
  </structdef>
  
  <structdef type="CTaskDraggingToSafety::Tunables" base="CTuning">
    <struct name="m_ObstructionProbe" type="CTaskDraggingToSafety::Tunables::ObstructionProbe"/>
    <struct name="m_Rendering" type="CTaskDraggingToSafety::Tunables::Rendering"/>
    <float name="m_MaxTimeForStream" min="0.0f" max="5.0f" step="0.1f" init="3.0f"/>
    <float name="m_CoverMinDistance" min="0.0f" max="1000.0f" step="1.0f" init="3.0f"/>
    <float name="m_CoverMaxDistance" min="0.0f" max="1000.0f" step="1.0f" init="20.0f"/>
    <float name="m_LookAtUpdateTime" min="0.0f" max="5.0f" step="0.1f" init="3.0f"/>
    <int name="m_LookAtTime" min="0" max="10000" step="500" init="3000"/>
    <float name="m_CoverWeightDistance" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_CoverWeightUsage" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
    <float name="m_CoverWeightValue" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
    <Vector3 name="m_SeparationPickup" />
    <Vector3 name="m_SeparationDrag" />
    <Vector3 name="m_SeparationPutdown" />
    <float name="m_AbortAimedAtMinDistance" min="0.0f" max="100.0f" step="1.0f" init="7.5f"/>
    <float name="m_CoverResponseTimeout" min="0.0f" max="10.0f" step="0.5f" init="1.0f"/>
    <float name="m_MinDotForPickupDirection" min="0.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_MaxDistanceForHolster" min="0.0f" max="50.0f" step="0.5f" init="7.5f"/>
    <float name="m_MaxDistanceForPedToBeVeryCloseToCover" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
    <int name="m_MaxNumPedsAllowedToBeVeryCloseToCover" min="0" max="5" step="1" init="1"/>
    <float name="m_TimeBetweenCoverPointSearches" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_MinDistanceToSetApproachPosition" min="0.0f" max="1.0f" step="0.1f" init="0.1f"/>
    <float name="m_MaxDistanceToConsiderTooClose" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
    <float name="m_MaxDistanceToAlwaysLookAtTarget" min="0.0f" max="50.0f" step="1.0f" init="20.0f"/>
    <float name="m_MaxHeightDifferenceToApproachTarget" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_MaxXYDistanceToApproachTarget" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_MaxTimeToBeObstructed" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
  </structdef>

  <structdef type="CTaskCombat::Tunables::BuddyShot">
    <bool name="m_Enabled" init="true"/>
    <float name="m_MinTimeBeforeReact" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
    <float name="m_MaxTimeBeforeReact" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxTimeSinceShot" min="0.0f" max="3.0f" step="0.01f" init="1.0f"/>
    <float name="m_MaxDistance" min="0.0f" max="30.0f" step="1.0f" init="15.0f"/>
  </structdef>

  <structdef type="CTaskCombat::Tunables::LackOfHostility::WantedLevel">
    <bool name="m_Enabled" init="false"/>
    <float name="m_MinTimeSinceLastHostileAction" min="0.0f" max="600.0f" step="1.0f" init="0.0f"/>
  </structdef>

  <structdef type="CTaskCombat::Tunables::LackOfHostility">
    <struct name="m_WantedLevel1" type="CTaskCombat::Tunables::LackOfHostility::WantedLevel"/>
    <struct name="m_WantedLevel2" type="CTaskCombat::Tunables::LackOfHostility::WantedLevel"/>
    <struct name="m_WantedLevel3" type="CTaskCombat::Tunables::LackOfHostility::WantedLevel"/>
    <struct name="m_WantedLevel4" type="CTaskCombat::Tunables::LackOfHostility::WantedLevel"/>
    <struct name="m_WantedLevel5" type="CTaskCombat::Tunables::LackOfHostility::WantedLevel"/>
    <float name="m_MaxSpeedForVehicle" min="0.0f" max="50.0f" step="1.0f" init="3.0f"/>
  </structdef>
  
  <structdef type="CTaskCombat::Tunables::EnemyAccuracyScaling">
    <int name="m_iMinNumEnemiesForScaling" min="0" max="100" step="1" init="3" />
    <float name="m_fAccuracyReductionPerEnemy" min="0.0f" max="1.0f" step="0.01f" init="0.075f" />
    <float name="m_fAccuracyReductionFloor" min="0.0f" max="1.0f" step="0.01f" init="0.05f" />
  </structdef>

  <structdef type="CTaskCombat::Tunables::ChargeTuning">
    <bool name="m_bChargeTargetEnabled" init="false"/>
    <u8  name="m_uMaxNumActiveChargers" min="0" max="4" step="1" init="0"/>
    <u32 name="m_uConsiderRecentChargeAsActiveTimeoutMS" min="1" max="120000" step="1000" init="30000"/>
    <u32 name="m_uMinTimeBetweenChargesAtSameTargetMS" min="0" max="120000" step="1000" init="4000"/>
    <u32 name="m_uMinTimeForSamePedToChargeAgainMS" min="0" max="120000" step="1000" init="30000"/>
    <u32 name="m_uCheckForChargeTargetPeriodMS" min="0" max="2000" step="10" init="250"/>
    <float name="m_fMinTimeInCombatSeconds" min="0.0f" max="60.0f" step="1.0f" init="5.0f" />
    <float name="m_fMinDistanceToTarget" min="0.0f" max="50.0f" step="0.5f" init="2.0f"/>
    <float name="m_fMaxDistanceToTarget" min="0.0f" max="200.0f" step="0.5f" init="30.0f"/>
    <float name="m_fMinDistToNonTargetEnemy" min="0.0f" max="200.0f" step="0.5f" init="4.0f"/>
    <float name="m_fMinDistBetweenTargetAndOtherEnemies" min="0.0f" max="200.0f" step="0.5f" init="4.0f"/>
    <float name="m_fDistToHidingTarget_Outer" min="0.0f" max="50.0f" step="1.0f" init="18.0f"/>
    <float name="m_fDistToHidingTarget_Inner" min="0.0f" max="50.0f" step="1.0f" init="6.0f"/>
    <float name="m_fChargeGoalCompletionRadius" min="0.0f" max="10.0f" step="0.1f" init="1.5f"/>
    <float name="m_fCancelTargetOutOfCoverMovedDist" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
    <float name="m_fCancelTargetInCoverMovedDist" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
  </structdef>
  
  <structdef type="CTaskCombat::Tunables::ThrowSmokeGrenadeTuning">
    <bool name="m_bThrowSmokeGrenadeEnabled" init="false"/>
    <u8  name="m_uMaxNumActiveThrowers" min="0" max="4" step="1" init="1"/>
    <u32 name="m_uConsiderRecentThrowAsActiveTimeoutMS" min="1" max="120000" step="1000" init="30000"/>
    <u32 name="m_uMinTimeBetweenThrowsAtSameTargetMS" min="0" max="120000" step="1000" init="2000"/>
    <u32 name="m_uMinTimeForSamePedToThrowAgainMS" min="0" max="120000" step="1000" init="20000"/>
    <u32 name="m_uCheckForSmokeThrowPeriodMS" min="0" max="2000" step="10" init="275"/>
    <float name="m_fMinDistanceToTarget" min="0.0f" max="50.0f" step="0.5f" init="6.0f"/>
    <float name="m_fMaxDistanceToTarget" min="0.0f" max="200.0f" step="0.5f" init="25.0f"/>
    <float name="m_fDotMinThrowerToTarget" min="0.0f" max="1.0f" step="0.01f" init="0.85f"/>
    <float name="m_fMinLoiteringTimeSeconds" min="0.0f" max="200.0f" step="0.5f" init="20.0f"/>
  </structdef>
  
  <structdef type="CTaskCombat::Tunables" base="CTuning">
    <struct name="m_BuddyShot" type="CTaskCombat::Tunables::BuddyShot"/>
    <struct name="m_LackOfHostility" type="CTaskCombat::Tunables::LackOfHostility"/>
    <struct name="m_EnemyAccuracyScaling" type="CTaskCombat::Tunables::EnemyAccuracyScaling"/>
    <struct name="m_ChargeTuning" type="CTaskCombat::Tunables::ChargeTuning"/>
    <struct name="m_ThrowSmokeGrenadeTuning" type="CTaskCombat::Tunables::ThrowSmokeGrenadeTuning"/>
    <float name="m_MaxDistToCoverZ" min="0.0f" max="5.0f" step="0.1f" init="1.5f"/>
    <float name="m_MaxDistToCoverXY" min="0.0f" max="5.0f" step="0.1f" init="0.5f"/>
    <float name="m_fAmbientAnimsMinDistToTargetSq" min="0.0f" max="10000.0f" step=".5f" init="81.0f"/>
    <float name="m_fAmbientAnimsMaxDistToTargetSq" min="0.0f" max="10000.0f" step=".5f" init="2500.0f"/>
    <float name="m_fGoToDefAreaTimeOut" min="0.0f" max="20.0f" step=".25f" init="10.0f"/>
    <float name="m_fFireContinuouslyDistMin" min="0.0f" max="1000.0f" step=".25f" init="2.0f"/>
    <float name="m_fFireContinuouslyDistMax" min="0.0f" max="1000.0f" step=".25f" init="12.0f"/>
    <float name="m_fLostTargetTime" min="0.0f" max="50.0f" step="1.0f" init="30.0f"/>
    <float name="m_fMinTimeAfterAimPoseForStateChange" min="0.0f" max="1.5f" step="0.05f" init="1.0f"/>
    <float name="m_fMaxAttemptMoveToCoverDelay" min="0.0f" max="100.0f" step="0.25f" init="18.5f"/>
    <float name="m_fMinAttemptMoveToCoverDelay" min="0.0f" max="100.0f" step="0.25f" init="16.5f"/>
    <float name="m_fMaxAttemptMoveToCoverDelayGlobal" min="0.0f" max="100.0f" step="0.25f" init="15.0f"/>
    <float name="m_fMinAttemptMoveToCoverDelayGlobal" min="0.0f" max="100.0f" step="0.25f" init="13.0f"/>
    <float name="m_fMinDistanceForAltCover" min="0.0f" max="100.0f" step=".5f" init="3.0f"/>
    <float name="m_fMinTimeStandingAtCover" min="0.0f" max="100.0f" step="0.25f" init="8.0f"/>
    <float name="m_fMinTimeBetweenFrustratedPeds" min="0.0f" max="100.0f" step="0.5f" init="15.0f"/>
    <float name="m_fMaxTimeBetweenFrustratedPeds" min="0.0f" max="100.0f" step="0.5f" init="18.5f"/>
    <float name="m_fRetreatTime" min="0.0f" max="20.0f" step="0.25f" init="5.0f"/>
    <float name="m_fTargetTooCloseDistance" min="0.0f" max="50.0f" step="0.1f" init="10.0f"/>
    <float name="m_fTimeBetweenJackingAttempts" min="0.0f" max="20.0f" step="0.25f" init="10.0f"/>
    <float name="m_fTimeBetweenCoverSearchesMin" min="0.0f" max="10.0f" step="0.05f" init="1.0f"/>
    <float name="m_fTimeBetweenCoverSearchesMax" min="0.0f" max="10.0f" step="0.05f" init="1.25f"/>
    <float name="m_fTimeBetweenAltCoverSearches" min="0.0f" max="10.0f" step="0.05f" init="2.5f"/>
    <float name="m_fShoutTargetPositionInterval" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
    <float name="m_fShoutBlockingLosInterval" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
    <float name="m_fTimeBetweenDragsMin" min="0.0f" max="100.0f" step="1.0f" init="20.0f"/>
    <float name="m_fTimeBetweenSecondaryTargetUsesMin" min="0.0f" max="100.0f" step="0.5f" init="16.0f"/>
    <float name="m_fTimeBetweenSecondaryTargetUsesMax" min="0.0f" max="100.0f" step="0.5f" init="22.0f"/>
    <float name="m_fTimeToUseSecondaryTargetMin" min="0.0f" max="100.0f" step="0.5f" init="4.0f"/>
    <float name="m_fTimeToUseSecondaryTargetMax" min="0.0f" max="100.0f" step="0.5f" init="6.0f"/>
    <float name="m_fTimeBetweenCombatDirectorUpdates" min="0.0f" max="100.0f" step="1.0f" init="2.0f"/>
    <float name="m_fTimeBetweenPassiveAnimsMin" min="0.0f" max="100.0f" step="0.1f" init="4.0f"/>
    <float name="m_fTimeBetweenPassiveAnimsMax" min="0.0f" max="100.0f" step="0.1f" init="5.5f"/>
    <float name="m_fTimeBetweenQuickGlancesMin" min="0.0f" max="100.0f" step="0.1f" init="1.75f"/>
    <float name="m_fTimeBetweenQuickGlancesMax" min="0.0f" max="100.0f" step="0.1f" init="2.50f"/>
    <float name="m_fTimeBetweenGestureAnimsMin" min="0" max="100.0f" step="0.1f" init="7.0f"/>
    <float name="m_fTimeBetweenGestureAnimsMax" min="0" max="100.0f" step="0.1f" init="10.0f"/>
    <float name="m_fTimeBetweenFailedGestureMin" min="0" max="10.0f" step="0.1f" init="0.35f"/>
    <float name="m_fTimeBetweenFailedGestureMax" min="0" max="10.0f" step="0.1f" init="0.75f"/>
    <float name="m_fTimeBetweenGesturesMinGlobal" min="0" max="100.0f" step="0.1f" init="6.5f"/>
    <float name="m_fTimeBetweenGesturesMaxGlobal" min="0" max="100.0f" step="0.1f" init="8.0f"/>
    <float name="m_fTimeSinceLastAimedAtForGesture" min="0.0f" max="10.0f" step="0.1f" init="1.25f"/>
    <float name="m_fMinTimeBeforeReactToExplosion" min="0.0f" max="1.0f" step="0.01f" init="0.15f"/>
    <float name="m_fMaxTimeBeforeReactToExplosion" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
    <float name="m_TargetInfluenceSphereRadius" min="0.0f" max="15.0f" step="0.25f" init="7.0f"/>
    <float name="m_TargetMinDistanceToRoute" min="0.0f" max="15.0f" step="0.25f" init="5.5f"/>
    <float name="m_TargetMinDistanceToAwayFacingNavLink" min="0.0f" max="50.0f" step="0.1f" init="25.0f"/>
    <float name="m_fMaxWaitForCoverExitTime" min="0.0f" max="5.0f" step="0.1f" init="2.5f"/>
    <float name="m_fMaxDstanceToMoveAwayFromAlly" min="0.0f" max="15.0f" step="0.1f" init="4.0f"/>
    <float name="m_fTimeBetweenAllyProximityChecks" min="0.0f" max="15.0f" step="0.1f" init="2.5f"/>
    <float name="m_fMinDistanceFromPrimaryTarget" min="0.0f" max="100.0f" step="0.1f" init="15.0f"/>
    <float name="m_fMaxAngleBetweenTargets" min="0.0f" max="100.0f" step="0.1f" init="0.785398f"/>
    <float name="m_MaxDistanceFromPedToHelpPed" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
    <float name="m_MaxDotToTargetToHelpPed" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_MaxHeadingDifferenceForQuickGlanceInSameDirection" min="0.0f" max="3.14f" step="0.01f" init="0.78f"/>
    <float name="m_MinTimeBetweenQuickGlancesInSameDirection" min="0.0f" max="30.0f" step="1.0f" init="8.0f"/>
    <float name="m_MaxSpeedToStartJackingVehicle" min="0.0f" max="50.0f" step="1.0f" init="1.25f"/>
    <float name="m_MaxSpeedToContinueJackingVehicle" min="0.0f" max="50.0f" step="1.0f" init="2.25f"/>
    <float name="m_TargetJackRadius" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
    <float name="m_SafetyProportionInDefensiveAreaMin" min="0.0f" max="1.0f" step="0.01f" init="0.65f"/>
    <float name="m_SafetyProportionInDefensiveAreaMax" min="0.0f" max="1.0f" step="0.01f" init="0.80f"/>
    <float name="m_MaxMoveToDefensiveAreaAngleVariation" min="0.0f" max="1.0f" step="0.01f" init="0.3927f"/>
    <float name="m_MinDistanceToEnterVehicleIfTargetEntersVehicle" min="0.0f" max="100.0f" step="0.05f" init="20.0"/>
    <float name="m_MaxDistanceToMyVehicleToChase" min="0.0f" max="100.0f" step="0.05f" init="35.0"/>
    <float name="m_MaxDistanceToVehicleForCommandeer" min="0.0f" max="100.0f" step="0.05f" init="20.0"/>
    <u8 name="m_NumEarlyVehicleEntryDriversAllowed" min="0" max="5" step="1" init="3"/>
    <u32 name="m_SafeTimeBeforeLeavingCover" min="0" max="10000" step="100" init="2000"/>
    <u32 name="m_WaitTimeForJackingSlowedVehicle" min="0" max="20000" step="100" init="4000"/>
    <float name="m_MaxInjuredTargetTimerVariation" min="0.0f" max="5.0f" step="0.01f" init=".75"/>
    <u8 name="m_MaxNumPedsChasingOnFoot" min="0" max="10" step="1" init="3"/>
    <float name="m_FireTimeAfterStaticMovementAbort" min="0.0f" max="20.0f" step="0.1f" init="7.5"/>
    <float name="m_MinMovingToCoverTimeToStop" min="0.0f" max="20.0f" step="0.1f" init="4.75"/>
    <float name="m_MinDistanceToCoverToStop" min="0.0f" max="20.0f" step="0.1f" init="9.0"/>
    <float name="m_FireTimeAfterStoppingMoveToCover" min="0.0f" max="20.0f" step="0.1f" init="5.0"/>
    <float name="m_ApproachingTargetVehicleHoldFireDistance" min="0.0f" max="20.0f" step="0.1f" init="3.5"/>
    <float name="m_MinDefensiveAreaRadiusForWillAdvance" min="0.0f" max="100.0f" step="0.1f" init="10.0"/>
    <float name="m_MaxDistanceToHoldFireForArrest" min="0.0f" max="100.0f" step="0.1f" init="7.5"/>
    <float name="m_TimeToDelayChaseOnFoot" min="0.0f" max="100.0f" step="0.1f" init="0.75"/>
    <float name="m_FireTimeAfterChaseOnFoot" min="0.0f" max="100.0f" step="0.1f" init="2.5"/>
    <u32 name="m_MinTimeToChangeChaseOnFootSpeed" min="0" max="10000" step="100" init="2500"/>
    <bool name="m_EnableForcedFireForTargetProximity" init="false"/>
    <float name="m_MinForceFiringStateTime" min="0.0f" max="100.0f" step="0.1f" init="2.0f"/>
    <float name="m_MaxForceFiringStateTime" min="0.0f" max="100.0f" step="0.1f" init="2.5f"/>
    <float name="m_TimeBeforeInitialForcedFire" min="0.0f" max="100.0f" step="0.1f" init="3.0f"/>
    <float name="m_TimeBetweenForcedFireStates" min="0.0f" max="100.0f" step="0.1f" init="4.0f"/>
    <float name="m_MinTimeInStateForForcedFire" min="0.0f" max="100.0f" step="0.1f" init="2.0f"/>
    <float name="m_MinForceFiringDistance" min="0.0f" max="100.0f" step="0.1f" init="1.5f"/>
    <float name="m_MaxForceFiringDistance" min="0.0f" max="100.0f" step="0.1f" init="5.0f"/>
    <float name="m_MinDistanceForAimIntro" min="0.0f" max="100.0f" step="0.1f" init="0.75f"/>
    <float name="m_MinDistanceToBlockAimIntro" min="0.0f" max="10000.0f" step="0.1f" init="10.0f"/>
    <float name="m_MinBlockedLOSTimeToBlockAimIntro" min="0.0f" max="10000.0f" step="0.1f" init="5.0f"/>
    <float name="m_AmbientAnimLengthBuffer" min="0.0f" max="10.0f" step="0.05f" init="0.25f"/>
    <u32 name="m_TimeBetweenPlayerArrestAttempts" min="0" max="10000" step="100" init="3719"/>
    <u32 name="m_TimeBetweenArmedMeleeAttemptsInMs" min="0" max="10000" step="100" init="2000"/>
    <bool name="m_AllowMovingArmedMeleeAttack" init="false"/>
    <float name="m_TimeToHoldFireAfterJack" init="1.5f" min="0.0f" max="10.0f" />
    <u32 name="m_MinTimeBetweenMeleeJackAttempts" min="0" max="10000" step="100" init="2000"/>
    <u32 name="m_MinTimeBetweenMeleeJackAttemptsOnNetworkClone" min="0" max="10000" step="100" init="3500"/>
    <float name="m_MaxTimeToHoldFireAtTaskInitialization" min="0.0f" max="20.0f" init="2.0f" />
    <u32 name="m_MaxTimeToRejectRespawnedTarget" min="0" max="10000" step="100" init="1000"/>
    <float name="m_MinDistanceForLawToFleeFromCombat" min="0.0f" max="1000.0f" init="80.0f" />
    <float name="m_MaxDistanceForLawToReturnToCombatFromFlee" min="0.0f" max="1000.0f" init="35.0f" />
    <float name="m_fTimeLosBlockedForReturnToInitialPosition" min="0.0f" max="1000.0f" init="5.0f" />
  </structdef>

  <structdef type="CTaskVehiclePersuit::Tunables::ApproachTarget">
    <float name="m_TargetArriveDist" min="0.0f" max="25.0f" step="1.0f" init="10.0f"/>
    <float name="m_CruiseSpeed" min="0.0f" max="50.0f" step="1.0f" init="25.0f"/>
    <float name="m_MaxDistanceToConsiderClose" min="0.0f" max="100.0f" step="1.0f" init="80.0f"/>
    <float name="m_CruiseSpeedWhenClose" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
    <float name="m_CruiseSpeedWhenObstructedByLawEnforcementPed" min="0.0f" max="50.0f" step="1.0f" init="3.0f"/>
    <float name="m_CruiseSpeedWhenObstructedByLawEnforcementVehicle" min="0.0f" max="50.0f" step="1.0f" init="5.0f"/>
    <float name="m_CruiseSpeedTooManyNearbyEntities" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
    <float name="m_DistanceToConsiderCloseEntitiesTarget" min="0.0f" max="100.0f" step="1.0f" init="25.0f"/>
    <float name="m_DistanceToConsiderEntityBlocking" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
    <int name="m_MaxNumberVehiclesNearTarget" min="0" max="50" step="1" init="10"/>
    <int name="m_MaxNumberPedsNearTarget" min="0" max="50" step="1" init="5"/>
  </structdef>

  <structdef type="CTaskVehiclePersuit::Tunables" base="CTuning">
    <struct name="m_ApproachTarget" type="CTaskVehiclePersuit::Tunables::ApproachTarget"/>
    <float name="m_ObstructionProbeAngleA" min="0.0f" max="180.0f" step="1.0f" init="15.0f"/>
    <float name="m_ObstructionProbeAngleB" min="0.0f" max="180.0f" step="1.0f" init="25.0f"/>
    <float name="m_ObstructionProbeAngleC" min="0.0f" max="180.0f" step="1.0f" init="35.0f"/>
    <float name="m_IdealDistanceOnBikeAndTargetUnarmed" min="0.0f" max="100.0f" step="1.0f" init="25.0f"/>
    <float name="m_IdealDistanceOnBikeAndTargetArmed" min="0.0f" max="100.0f" step="1.0f" init="25.0f"/>
    <float name="m_IdealDistanceInVehicleAndTargetUnarmed" min="0.0f" max="100.0f" step="1.0f" init="15.0f"/>
    <float name="m_IdealDistanceInVehicleAndTargetArmed" min="0.0f" max="100.0f" step="1.0f" init="20.0f"/>
    <float name="m_IdealDistanceShotAt" min="0.0f" max="100.0f" step="1.0f" init="50.0f"/>
    <float name="m_IdealDistanceCouldLeaveCar" min="0.0f" max="100.0f" step="1.0f" init="50.0f"/>
    <float name="m_DistanceToStopMultiplier" min="0.0f" max="1.0f" step="0.1f" init="0.75f"/>
    <float name="m_DistanceToStopMassIdeal" min="0.0f" max="10000.0f" step="10.0f" init="3000.0f"/>
    <float name="m_DistanceToStopMassWeight" min="0.0f" max="1.0f" step="0.1f" init="0.25f"/>
    <float name="m_MinDriverTimeToLeaveVehicle" min="0.0f" max="5.0f" step="0.1f" init="2.5f"/>
    <float name="m_MaxDriverTimeToLeaveVehicle" min="0.0f" max="5.0f" step="0.1f" init="3.25f"/>
    <float name="m_MinPassengerTimeToLeaveVehicle" min="0.0f" max="5.0f" step="0.1f" init="0.5f"/>
    <float name="m_MaxPassengerTimeToLeaveVehicle" min="0.0f" max="5.0f" step="0.1f" init="0.75f"/>
    <float name="m_MaxSpeedForEarlyCombatExit" min="0.0f" max="100.0f" step="0.25f" init="6.0f"/>
    <float name="m_MinSpeedToJumpOutOfVehicle" min="0.0f" max="100.0f" step="0.25f" init="10.0f"/>
    <float name="m_MinTimeBoatOutOfWaterForExit" min="0.0f" max="10.0f" step="1.0f" init="2.0f"/>
    <float name="m_AvoidanceMarginForOtherLawEnforcementVehicles" min="0.0f" max="5.0f" step="0.1f" init="3.0f"/>
    <float name="m_MinTimeToWaitForOtherPedToExit" min="0.0f" max="15.0f" step="1.0f" init="7.0f"/>
    <float name="m_MinDelayExitTime" min="0.0f" max="10.0f" step="0.01f" init="0.125f"/>
    <float name="m_MaxDelayExitTime" min="0.0f" max="10.0f" step="0.01f" init="0.425f"/>
    <float name="m_PreventShufflingExtraRange" min="0.0f" max="100.0f" step="1.0f" init="15.0f"/>
    <float name="m_MaxTimeWaitForExitBeforeWarp" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
    <float name="m_MinTargetStandingOnTrainSpeed" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
    <int name="m_DistanceToFollowInCar" min="0" max="50" step="1" init="10"/>
  </structdef>

  <structdef type="CTaskVehicleCombat::Tunables" base="CTuning">
    <float name="m_MinTimeBetweenShootOutTiresGlobal" min="0.0f" max="3600.0f" step="1.0f" init="120.0f"/>
    <float name="m_MaxTimeBetweenShootOutTiresGlobal" min="0.0f" max="3600.0f" step="1.0f" init="240.0f"/>
    <float name="m_MinTimeInCombatToShootOutTires" min="0.0f" max="300.0f" step="1.0f" init="120.0f"/>
    <float name="m_MaxTimeInCombatToShootOutTires" min="0.0f" max="300.0f" step="1.0f" init="240.0f"/>
    <float name="m_ChancesToApplyReactionWhenShootingOutTire" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_MinTimeToPrepareWeapon" min="0.0f" max="30.0f" step="0.1f" init="0.5f"/>
    <float name="m_MaxTimeToPrepareWeapon" min="0.0f" max="30.0f" step="0.1f" init="1.0f"/>
    <u32 name="m_MaxTimeSinceTargetLastHostileForLawDriveby" min="0" max="10000" step="100" init="5000"/>    
  </structdef>

  <structdef type="CTaskVehicleChase::Tunables::CloseDistance">
    <float name="m_MinDistanceToStart" min="0.0f" max="250.0f" step="1.0f" init="100.0f"/>
    <float name="m_MinDistanceToContinue" min="0.0f" max="250.0f" step="1.0f" init="100.0f"/>
  </structdef>

  <structdef type="CTaskVehicleChase::Tunables::Block">
    <float name="m_MaxDotToStartFromAnalyze" min="-1.0f" max="1.0f" step="0.01f" init="-0.707f"/>
    <float name="m_MaxDotToContinueFromAnalyze" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_MinTargetSpeedToStartFromPursue" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
    <float name="m_MinTargetSpeedToContinueFromPursue" min="0.0f" max="50.0f" step="1.0f" init="8.0f"/>
  </structdef>

  <structdef type="CTaskVehicleChase::Tunables::Pursue">
    <float name="m_MinDotToStartFromAnalyze" min="-1.0f" max="1.0f" step="0.01f" init="-0.707f"/>
    <float name="m_MinDotToContinueFromAnalyze" min="-1.0f" max="1.0f" step="0.01f" init="-0.75f"/>
    <float name="m_IdealDistance" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
  </structdef>

  <structdef type="CTaskVehicleChase::Tunables::Ram">
    <float name="m_StraightLineDistance" min="0.0f" max="50.0f" step="1.0f" init="30.0f"/>
    <float name="m_MinTargetSpeedToStartFromPursue" min="0.0f" max="50.0f" step="1.0f" init="13.0f"/>
    <float name="m_MinTargetSpeedToContinueFromPursue" min="0.0f" max="50.0f" step="1.0f" init="13.0f"/>
  </structdef>

  <structdef type="CTaskVehicleChase::Tunables::SpinOut">
    <float name="m_StraightLineDistance" min="0.0f" max="50.0f" step="1.0f" init="30.0f"/>
    <float name="m_MinTargetSpeedToStartFromPursue" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
    <float name="m_MinTargetSpeedToContinueFromPursue" min="0.0f" max="50.0f" step="1.0f" init="8.0f"/>
  </structdef>

  <structdef type="CTaskVehicleChase::Tunables::PullAlongside">
    <float name="m_StraightLineDistance" min="0.0f" max="50.0f" step="1.0f" init="30.0f"/>
    <float name="m_MinTargetSpeedToStartFromPursue" min="0.0f" max="50.0f" step="0.01f" init="0.25f"/>
    <float name="m_MinTargetSpeedToContinueFromPursue" min="0.0f" max="50.0f" step="0.01f" init="0.05f"/>
  </structdef>

  <structdef type="CTaskVehicleChase::Tunables::AggressiveMove">
    <float name="m_MaxDistanceToStartFromPursue" min="0.0f" max="250.0f" step="1.0f" init="25.0f"/>
    <float name="m_MinDotToStartFromPursue" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_MinSpeedLeewayToStartFromPursue" min="0.0f" max="50.0f" step="1.0f" init="5.0f"/>
    <float name="m_MaxTargetSteerAngleToStartFromPursue" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_MaxDistanceToContinueFromPursue" min="0.0f" max="250.0f" step="1.0f" init="30.0f"/>
    <float name="m_MinDotToContinueFromPursue" min="-1.0f" max="1.0f" step="0.01f" init="-0.2f"/>
    <float name="m_MaxTimeInStateToContinueFromPursue" min="0.0f" max="60.0f" step="1.0f" init="10.0f"/>
    <float name="m_MaxTargetSteerAngleToContinueFromPursue" min="0.0f" max="1.0f" step="0.01f" init="0.15f"/>
    <float name="m_MinDelay" min="0.0f" max="60.0f" step="1.0f" init="3.0f"/>
    <float name="m_MaxDelay" min="0.0f" max="60.0f" step="1.0f" init="5.0f"/>
    <float name="m_WeightToRamFromPursue" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
    <float name="m_WeightToBlockFromPursue" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_WeightToSpinOutFromPursue" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_WeightToPullAlongsideFromPursue" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
  </structdef>

  <structdef type="CTaskVehicleChase::Tunables::Cheat">
    <float name="m_MinSpeedDifferenceForPowerAdjustment" min="0.0f" max="100.0f" step="1.0f" init="1.0f"/>
    <float name="m_MaxSpeedDifferenceForPowerAdjustment" min="0.0f" max="100.0f" step="1.0f" init="50.0f"/>
    <float name="m_PowerForMinAdjustment" min="0.0f" max="50.0f" step="1.0f" init="1.0f"/>
    <float name="m_PowerForMaxAdjustment" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
    <float name="m_PowerForMaxAdjustmentNetwork" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
  </structdef>

  <structdef type="CTaskVehicleChase::Tunables" base="CTuning">
    <struct name="m_CloseDistance" type="CTaskVehicleChase::Tunables::CloseDistance" />
    <struct name="m_Block" type="CTaskVehicleChase::Tunables::Block" />
    <struct name="m_Pursue" type="CTaskVehicleChase::Tunables::Pursue" />
    <struct name="m_Ram" type="CTaskVehicleChase::Tunables::Ram" />
    <struct name="m_SpinOut" type="CTaskVehicleChase::Tunables::SpinOut" />
    <struct name="m_PullAlongside" type="CTaskVehicleChase::Tunables::PullAlongside" />
    <struct name="m_AggressiveMove" type="CTaskVehicleChase::Tunables::AggressiveMove" />
    <struct name="m_Cheat" type="CTaskVehicleChase::Tunables::Cheat" />
    <float name="m_MaxDotForHandBrake" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_TimeBetweenCarChaseShockingEvents" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_DistanceForCarChaseShockingEvents" min="0.0f" max="100.0f" step="1.0f" init="35.0f"/>
  </structdef>

  <structdef type="CTaskHelicopterStrafe::Tunables" base="CTuning">
    <int name="m_FlightHeightAboveTarget" min="0" max="100" step="1" init="20"/>
    <int name="m_MinHeightAboveTerrain" min="0" max="100" step="1" init="20"/>
    <float name="m_TargetDirectionMinDot" min="-1.0f" max="1.0f" step="0.01f" init="0.75f"/>
    <float name="m_TargetOffset" min="0.0f" max="100.0f" step="1.0f" init="60.0f"/>
    <float name="m_TargetMinSpeedToIgnore" min="0.0f" max="100.0f" step="1.0f" init="5.0f"/>
    <float name="m_TargetMaxSpeedToStrafe" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
    <float name="m_TimeToAvoidTargetAfterDamaged" min="0.0f" max="30.0f" step="1.0f" init="10.0f"/>
    <float name="m_AvoidOffsetXY" min="0.0f" max="50.0f" step="1.0f" init="35.0f"/>
    <float name="m_AvoidOffsetZ" min="0.0f" max="50.0f" step="1.0f" init="20.0f"/>
    <float name="m_MinDotToBeConsideredInFront" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_BehindRotateAngleLookAhead" min="0.0f" max="90.0f" step="1.0f" init="30.0f"/>
    <float name="m_SearchRotateAngleLookAhead" min="0.0f" max="90.0f" step="1.0f" init="30.0f"/>
    <float name="m_CircleRotateAngleLookAhead" min="0.0f" max="90.0f" step="1.0f" init="5.0f"/>
    <float name="m_BehindTargetAngle" min="0.0f" max="90.0f" step="1.0f" init="45.0f"/>
    <float name="m_TargetOffsetFilter" min="0.0f" max="100.0f" step="0.1f" init="5.0f"/>
    <float name="m_MinTimeBetweenStrafeDirectionChanges" min="0.0f" max="60.0f" step="1.0f" init="5.0f"/>
  </structdef>

  <structdef type="CTaskShootOutTire::Tunables" base="CTuning">
    <float name="m_MinTimeoutToAcquireLineOfSight" min="0.0f" max="60.0f" step="1.0f" init="5.0f"/>
    <float name="m_MaxTimeoutToAcquireLineOfSight" min="0.0f" max="60.0f" step="1.0f" init="15.0f"/>
    <float name="m_TimeBetweenLineOfSightChecks" min="0.0f" max="3.0f" step="0.1f" init="0.5f"/>
    <float name="m_MinTimeToWaitForShot" min="0.0f" max="10.0f" step="1.0f" init="2.0f"/>
    <float name="m_MaxTimeToWaitForShot" min="0.0f" max="10.0f" step="1.0f" init="4.0f"/>
    <int name="m_MaxWaitForShotFailures" min="0" max="10" step="1" init="3"/>
    <float name="m_MinSpeedToApplyReaction" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
  </structdef>

  <structdef type="CTaskCombatFlank::Tunables" base="CTuning">
    <float name="m_fInfluenceSphereInnerWeight" min="0.0f" max="10.0f" step="0.1f" init="5.0f"/>
    <float name="m_fInfluenceSphereOuterWeight" min="0.0f" max="10.0f" step="0.1f" init="5.0f"/>
    <float name="m_fInfluenceSphereRequestRadius" min="0.0f" max="50.0f" step="0.1f" init="20.0f"/>
    <float name="m_fInfluenceSphereCheckRouteRadius" min="0.0f" max="50.0f" step="0.1f" init="15.0f"/>
    <float name="m_fSmallInfluenceSphereRadius" min="0.0f" max="50.0f" step="0.1f" init="10.0f"/>
    <float name="m_fDistanceBetweenInfluenceSpheres" min="0.0f" max="50.0f" step="0.1f" init="12.0f"/>
    <float name="m_fAbsoluteMinDistanceToTarget" min="0.0f" max="50.0f" step="0.1f" init="7.0f"/>
    <float name="m_fCoverPointScoreMultiplier" min="0.0f" max="5.0f" step="0.1f" init="1.35f"/>
  </structdef>

  <structdef type="CTaskCombatAdditionalTask::Tunables" base="CTuning">
    <int name="m_iBulletEventResponseLengthMs" min="0" max="100000" step="100" init="6000"/>
    <float name="m_fChanceOfDynamicRun" min="0.0f" max="1.0f" step="0.01f" init="0.70f"/>
    <float name="m_fMaxDynamicStrafeDistance" min="0.0f" max="1000.0f" step="0.25f" init="10.0f"/>
    <float name="m_fMinTimeInState" min="0.0f" max="5.0f" step="0.1f" init="1.5f"/>
    <float name="m_fMoveBlendRatioLerpTime" min="0.0f" max="5.0f" step="0.1f" init="2.0f"/>
    <float name="m_fMinDistanceToClearCorner" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_fMaxDistanceFromCorner" min="0.0f" max="10.0f" step="0.1f" init="5.25f"/>
    <float name="m_fMaxLeavingCornerDistance" min="0.0f" max="10.0f" step="0.1f" init="2.5f"/>
    <float name="m_fBlockedLosAimTime" min="0.0f" max="10.0f" step="0.1f" init="3.5f"/>
    <float name="m_fStartAimingDistance" min="0.0f" max="100.0f" step="0.1f" init="17.5f"/>
    <float name="m_fStopAimingDistance" min="0.0f" max="100.0f" step="0.1f" init="22.5f"/>
    <float name="m_fMinOtherPedDistanceDiff" min="0.0f" max="100.0f" step="0.1f" init="2.0f"/>
    <float name="m_fMinTimeBetweenRunDirectlyChecks" min="0.0f" max="100.0f" step="0.1f" init="0.75f"/>
    <float name="m_fMaxTimeStrafing" min="0.0f" max="100.0f" step="0.1f" init="4.75f"/>
    <float name="m_fMinTimeRunning" min="0.0f" max="100.0f" step="0.1f" init="3.5f"/>
    <float name="m_fForceStrafeDistance" min="0.0f" max="100.0f" step="0.1f" init="17.5f"/>
    <float name="m_fMaxLosBlockedTimeToForceClearLos" min="0.0f" max="100.0f" step="0.1f" init="5.0f"/>
  </structdef>

  <structdef type="CTaskBoatCombat::Tunables::Rendering">
    <bool name="m_Enabled" init="false"/>
    <bool name="m_CollisionProbe" init="true"/>
    <bool name="m_LandProbe" init="true"/>
  </structdef>

  <structdef type="CTaskBoatCombat::Tunables" base="CTuning">
    <struct name="m_Rendering" type="CTaskBoatCombat::Tunables::Rendering"/>
    <float name="m_MinSpeedForChase" min="0.0f" max="50.0f" step="1.0f" init="3.0f"/>
    <float name="m_TimeToLookAheadForCollision" min="0.0f" max="10.0f" step="1.0f" init="4.0f"/>
    <float name="m_DepthForLandProbe" min="0.0f" max="25.0f" step="0.1f" init="4.0f"/>
    <float name="m_TimeToWait" min="0.0f" max="60.0f" step="1.0f" init="10.0f"/>
  </structdef>

  <structdef type="CTaskBoatChase::Tunables" base="CTuning">
    <float name="m_IdealDistanceForPursue" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
  </structdef>

  <structdef type="CTaskSubmarineChase::Tunables" base="CTuning">
    <float name="m_TargetBufferDistance" min="0.0f" max="100.0f" step="1.0f" init="5.0f"/>
    <float name="m_SlowDownDistanceMax" min="0.0f" max="100.0f" step="1.0f" init="25.0f"/>
    <float name="m_SlowDownDistanceMin" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
  </structdef>

  <structdef type="CTaskBoatStrafe::Tunables" base="CTuning">
    <float name="m_AdditionalDistanceForApproach" min="0.0f" max="50.0f" step="1.0f" init="35.0f"/>
    <float name="m_AdditionalDistanceForStrafe" min="0.0f" max="50.0f" step="1.0f" init="25.0f"/>
    <float name="m_CruiseSpeedForStrafe" min="0.0f" max="50.0f" step="1.0f" init="5.0f"/>
    <float name="m_RotationLookAhead" min="0.0f" max="360.0f" step="1.0f" init="5.0f"/>
    <float name="m_MaxAdjustmentLookAhead" min="0.0f" max="10.0f" step="1.0f" init="1.0f"/>
  </structdef>

  <structdef type="CTaskHeliCombat::Tunables::Chase">
    <float name="m_MinSpeed" min="0.0f" max="50.0f" step="1.0f" init="5.0f"/>
    <float name="m_MinTargetOffsetX" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
    <float name="m_MaxTargetOffsetX" min="0.0f" max="50.0f" step="1.0f" init="35.0f"/>
    <float name="m_MinTargetOffsetY" min="0.0f" max="50.0f" step="1.0f" init="0.0f"/>
    <float name="m_MaxTargetOffsetY" min="0.0f" max="50.0f" step="1.0f" init="20.0f"/>
    <float name="m_MinTargetOffsetZ" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
    <float name="m_MaxTargetOffsetZ" min="0.0f" max="50.0f" step="1.0f" init="30.0f"/>
    <float name="m_MinTargetOffsetZ_TargetInAir" min="0.0f" max="50.0f" step="1.0f" init="0.0f"/>
    <float name="m_MaxTargetOffsetZ_TargetInAir" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
  </structdef>

  <structdef type="CTaskSubmarineCombat::Tunables" base="CTuning">
  </structdef>

  <structdef type="CTaskHeliCombat::Tunables" base="CTuning">
    <struct name="m_Chase" type="CTaskHeliCombat::Tunables::Chase"/>
  </structdef>

  <structdef type="CTaskHeliChase::Tunables::Drift">
    <float name="m_MinValueForCorrection" min="0.0f" max="10.0f" step="0.01f" init="1.5f"/>
    <float name="m_MaxValueForCorrection" min="0.0f" max="10.0f" step="0.01f" init="2.0f"/>
    <float name="m_MinRate" min="0.0f" max="3.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxRate" min="0.0f" max="3.0f" step="0.01f" init="0.75f"/>
  </structdef>

  <structdef type="CTaskHeliChase::Tunables" base="CTuning">
    <struct name="m_DriftX" type="CTaskHeliChase::Tunables::Drift"/>
    <struct name="m_DriftY" type="CTaskHeliChase::Tunables::Drift"/>
    <struct name="m_DriftZ" type="CTaskHeliChase::Tunables::Drift"/>
    <int name="m_MinHeightAboveTerrain" min="0" max="50" step="1" init="20"/>
    <float name="m_SlowDownDistanceMin" min="0.0f" max="100.0f" step="1.0f" init="15.0f"/>
    <float name="m_SlowDownDistanceMax" min="0.0f" max="100.0f" step="1.0f" init="25.0f"/>
    <float name="m_CruiseSpeed" min="0.0f" max="50.0f" step="1.0f" init="35.0f"/>
    <float name="m_MaxDistanceForOrientation" min="0.0f" max="10.0f" step="1.0f" init="5.0f"/>
    <float name="m_NearDistanceForOrientation" min="0.0f" max="100.0f" step="1.0f" init="25.0f"/>
  </structdef>

  <structdef type="CTaskPlaneChase::Tunables" base="CTuning">

  </structdef>

  <structdef type="CTaskSearchBase::Tunables" base="CTuning">
    <float name="m_TimeToGiveUp" min="0.0f" max="600.0f" step="1.0f" init="120.0f"/>
    <float name="m_MaxPositionVariance" min="0.0f" max="25.0f" step="1.0f" init="5.0f"/>
    <float name="m_MaxDirectionVariance" min="0.0f" max="3.14f" step="0.01f" init="0.75f"/>
  </structdef>

  <structdef type="CTaskSearch::Tunables" base="CTuning">
    <float name="m_TimeToStare" min="0.0f" max="60.0f" step="1.0f" init="5.0f"/>
    <float name="m_MoveBlendRatio" min="0.0f" max="3.0f" step="0.1f" init="2.0f"/>
    <float name="m_TargetReached" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
    <float name="m_CruiseSpeed" min="0.0f" max="50.0f" step="1.0f" init="40.0f"/>
  </structdef>

  <structdef type="CTaskInvestigate::Tunables" base="CTuning">
    <s32 name="m_iTimeToStandAtSearchPoint"   min="1000" max="60000" step="500" init="4000"/>
    <float name="m_fMinDistanceToUseVehicle"  min="0.f"  max="100.f" step="0.5f" init="30.f"/>
    <float name="m_fMinDistanceSavingToUseVehicle" min="0.f" max="100.f" step="0.5f" init ="0.5f"/>
    <float name="m_fTimeToStandAtPerimeter" min="0.f" max="20.f" step="0.1f" init="10.f"/>
    <float name="m_fNewPositionThreshold" min="0.f" max="10.f" step="0.1f" init="5.f"/>
  </structdef>

  <structdef type="CTaskSearchForUnknownThreat::Tunables" base="CTuning">
    <s32 name="m_iMinTimeBeforeSearchingForNewHidingPlace" min="1000" max="60000" step="500" init="10000"/>
    <s32 name="m_iMaxTimeBeforeSearchingForNewHidingPlace" min="1000" max="60000" step="500" init="20000"/>
  </structdef>

  <structdef type="CTaskSearchInAutomobile::Tunables" base="CTuning">
    <float name="m_FleeOffset" min="0.0f" max="50.0f" step="1.0f" init="25.0f"/>
    <float name="m_CruiseSpeed" min="0.0f" max="50.0f" step="1.0f" init="25.0f"/>
  </structdef>

  <structdef type="CTaskSearchInBoat::Tunables" base="CTuning">
  </structdef>

  <structdef type="CTaskSearchInHeli::Tunables" base="CTuning">
    <float name="m_FleeOffset" min="0.0f" max="50.0f" step="1.0f" init="25.0f"/>
    <float name="m_CruiseSpeed" min="0.0f" max="50.0f" step="1.0f" init="25.0f"/>
    <int name="m_MinHeightAboveTerrain" min="0" max="50" step="1" init="20"/>
  </structdef>

  <structdef type="CTaskSearchOnFoot::Tunables" base="CTuning">
    <float name="m_FleeOffset" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
    <float name="m_TargetRadius" min="0.0f" max="100.0f" step="1.0f" init="1.0f"/>
    <float name="m_CompletionRadius" min="0.0f" max="10.0f" step="1.0f" init="1.0f"/>
    <float name="m_SlowDownDistance" min="0.0f" max="10.0f" step="1.0f" init="3.0f"/>
    <float name="m_FleeSafeDistance" min="0.0f" max="200.0f" step="1.0f" init="150.0f"/>
    <float name="m_MoveBlendRatio" min="0.0f" max="3.0f" step="0.1f" init="1.0f"/>
  </structdef>

  <structdef type="CTaskTargetUnreachable::Tunables" base="CTuning">
    <float name="m_fTimeBetweenRouteSearches" min="0.0f" max="10.0f" step="0.1f" init="3.5f"/>
  </structdef>

  <structdef type="CTaskTargetUnreachableInInterior::Tunables" base="CTuning">
    <float name="m_fDirectionTestProbeLength" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
  </structdef>

  <structdef type="CTaskTargetUnreachableInExterior::Tunables" base="CTuning">
    <float name="m_RangePercentage" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_MaxDistanceFromNavMesh" min="0.0f" max="50.0f" step="1.0f" init="40.0f"/>
    <float name="m_TargetRadius" min="0.0f" max="25.0f" step="1.0f" init="3.0f"/>
    <float name="m_MoveBlendRatio" min="0.0f" max="3.0f" step="0.01f" init="2.0f"/>
    <float name="m_CompletionRadius" min="0.0f" max="25.0f" step="1.0f" init="3.0f"/>
    <float name="m_MinTimeToWait" min="0.0f" max="60.0f" step="1.0f" init="5.0f"/>
    <float name="m_MaxTimeToWait" min="0.0f" max="60.0f" step="1.0f" init="15.0f"/>
  </structdef>

  <structdef type="CTaskDyingDead::Tunables" base="CTuning">
    <float name="m_VehicleForwardInitialScale" min="-10.0f" max="10.0f" step="0.1f" init="3.0f"/>
    <float name="m_VehicleForwardScale" min="-10.0f" max="10.0f" step="0.1f" init="0.2f"/>
    <float name="m_TimeToApplyPushFromVehicleForce" min="0.0f" max="5.0f" step="0.01f" init="0.35f"/>
    <float name="m_ForceToApply" min="0.0f" max="100.0f" step="0.1f" init="25.0f"/>
    <float name="m_MinFallingSpeedForAnimatedDyingFall" min="0.0f" max="30.0f" step="0.01f" init="6.5f"/>
    <float name="m_SphereTestRadiusForDeadWaterSettle" min="0.0f" max="30.0f" step="0.01f" init="0.4"/>
    <float name="m_RagdollAbortPoseDistanceThreshold" min="0.0f" max="10.0f" step="0.001f" init="0.1f"/>
    <float name="m_RagdollAbortPoseMaxVelocity" min="0.0f" max="10.0f" step="0.001f" init="0.4f"/>
    <u32 name="m_TimeToThrowWeaponMS" min="0" max="10000" step="10" init="500"/>
    <u32 name="m_TimeToThrowWeaponPlayerMS" min="0" max="10000" step="10" init="200"/>
  </structdef>

  <structdef type="CTaskDamageElectric::Tunables" base="CTuning">
    <float name="m_FallsOutofVehicleVelocity" min="0.0f" max="100.0f" step="0.1f" init="10.0f"/>
  </structdef>

  <structdef type="CTaskReactAimWeapon::Tunables::Ability::Situation::Weapon">
    <string name="m_ClipSetId" type="atHashString"/>
    <float name="m_Rate" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <bool name="m_HasSixDirections" init="false" />
    <bool name="m_HasCreateWeaponTag" init="false" />
    <bool name="m_HasInterruptTag" init="false" />
  </structdef>

  <structdef type="CTaskReactAimWeapon::Tunables::Ability::Situation">
    <struct name="m_Pistol" type="CTaskReactAimWeapon::Tunables::Ability::Situation::Weapon"/>
    <struct name="m_Rifle" type="CTaskReactAimWeapon::Tunables::Ability::Situation::Weapon"/>
    <struct name="m_MicroSMG" type="CTaskReactAimWeapon::Tunables::Ability::Situation::Weapon"/>
  </structdef>
  
  <structdef type="CTaskReactAimWeapon::Tunables::Ability">
    <struct name="m_Flinch" type="CTaskReactAimWeapon::Tunables::Ability::Situation"/>
    <struct name="m_Surprised" type="CTaskReactAimWeapon::Tunables::Ability::Situation"/>
    <struct name="m_Sniper" type="CTaskReactAimWeapon::Tunables::Ability::Situation"/>
    <struct name="m_None" type="CTaskReactAimWeapon::Tunables::Ability::Situation"/>
  </structdef>

  <structdef type="CTaskReactAimWeapon::Tunables" base="CTuning">
    <struct name="m_Professional" type="CTaskReactAimWeapon::Tunables::Ability"/>
    <struct name="m_NotProfessional" type="CTaskReactAimWeapon::Tunables::Ability"/>
    <float name="m_Rate" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_MaxRateVariance" min="0.0f" max="1.0f" step="0.01f" init="0.15f"/>
  </structdef>

  <structdef type="CPedTargetting::Tunables" base="CTuning">
    <float name="m_fExistingTargetScoreWeight" min="0.0f" max="5.0f" step="0.01f" init="1.5f"/>
    <float name="m_fTargetingInactiveDisableTime" min="0.0f" max="25.0f" step="0.01f" init="4.0f"/>
    <float name="m_fBlockedLosWeighting" min="0.0f" max="5.0f" step="0.01f" init="0.2f"/>
    <float name="m_fTimeToIgnoreBlockedLosWeighting" min="0.0f" max="15.0f" step="0.05f" init="5.0f"/>
    <float name="m_fPlayerHighThreatWeighting" min="0.0f" max="100.0f" step="0.01f" init="10.0f"/>
    <float name="m_fTargetInAircraftWeighting" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <int name="m_iTargetNotSeenIgnoreTimeMs" min="0" max="25000" step="100" init="5000"/>
    <float name="m_fPlayerThreatDistance" min="0.0f" max="100.0f" step="0.1f" init="15.0f"/>
    <float name="m_fPlayerDirectThreatDistance" min="0.0f" max="100.0f" step="0.1f" init="27.5f"/>
    <float name="m_fPlayerBeingTargetedExtraDistance" min="0.0f" max="100.0f" step="0.1f" init="5.0f"/>
    <int name="m_iPlayerDirectThreatTimeMs" min="0" max="25000" step="100" init="5000"/>
  </structdef>

  <structdef type="CTaskSharkCircle::Tunables" base="CTuning">
    <float name="m_AdvanceDistanceSquared" min="0.0f" max="400.0f" step="0.1f" init="25.0f"/>
    <float name="m_MoveRateOverride" min="0.0f" max="2.0f" step="0.01f" init="0.7f"/>
  </structdef>

  <structdef type="CTaskSharkAttack::Tunables" base="CTuning">
    <float name="m_SurfaceProjectionDistance" min="0.0f" max="20.0f" step="0.1f" init="5.0f"/>
    <float name="m_SurfaceZOffset" min="0.0f" max="10.0f" step="0.01f" init="0.75f"/>
    <float name="m_MinDepthBelowSurface" min="0.0f" max="10.0f" step="0.01f" init="0.75f"/>
    <float name="m_CirclingAngularSpeed" min="0.0f" max="200.0f" step="0.1f" init="15.0f"/>
    <float name="m_TimeToCircle" min="0.0f" max="60.0f" step="0.1f" init="15.0f"/>
    <float name="m_MinCircleRadius" min="0.0f" max="30.0f" step="0.1f" init="5.0f"/>
    <float name="m_MaxCircleRadius" min="0.0f" max="30.0f" step="0.1f" init="20.0f"/>
    <float name="m_CirclingMBR" min="0.0f" max="3.0f" step="0.1f" init="1.0f"/>
    <float name="m_DiveProjectionDistance" min="0.0f" max="50.0f" step="0.1f" init="5.0f"/>
    <float name="m_DiveDepth" min="0.0f" max="30.0f" step="0.1f" init="5.0f"/>
    <float name="m_DiveMBR" min="0.0f" max="3.0f" step="0.1f" init="1.0f"/>
    <s32 name="m_MinNumberFakeApproaches" min="0" max="10" init="2"/>
    <s32 name="m_MaxNumberFakeApproaches" min="0" max="10" init="4"/>
    <float name="m_FakeLungeOffset" min="0.0f" max="50.0f" step="0.1f" init="5.0f"/>
    <float name="m_LungeForwardOffset" min="0.0f" max="20.0f" step="0.1f" init="5.0f"/>
    <float name="m_LungeZOffset" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_LungeChangeDistance" min="0.0f" max="10.0f" step="0.01f" init="0.5f"/>
    <float name="m_LungeTargetRadius" min="0.0f" max="20.0f" step="0.1f" init="1.0f"/>
    <float name="m_FollowTimeout" min="0.0f" max="60.0f" step="0.1f" init="30.0f"/>
    <float name="m_FollowYOffset" min="0.0f" max="20.0f" step="0.1f" init="8.0f"/>
    <float name="m_FollowZOffset" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_LandProbeLength" min="0.0f" max="20.0f" step="0.1f" init="8.0f"/>
    <float name="m_MovingVehicleVelocityThreshold" min="0.0f" max="100.0f" step="0.1f" init="3.0f"/>
    <float name="m_SharkFleeDist" min="0.0f" max="100.0f" step="0.1f" init="35.0f"/>
  </structdef>

  <structdef type="CTaskAnimatedHitByExplosion::Tunables" base="CTuning">
    <float name="m_InitialRagdollDelay" min="0.0f" max="10.0f" step="0.01f" init="0.15f" description="Block ragdoll (from explosions, object impacts and death) for this many seconds after starting the animated hit by explosion task"/>
    <bool name="m_AllowPitchAndRoll" init="false" />
  </structdef>

  <structdef type="CTaskStandGuard::Tunables" base="CTuning">
    <s32 name="m_MinStandWaitTimeMS" init="2000"/>
    <s32 name="m_MaxStandWaitTimeMS" init="5000"/>
    <s32 name="m_MinDefendPointWaitTimeMS" init="5000"/>
    <s32 name="m_MaxDefendPointWaitTimeMS" init="10000"/>
    <float name="m_MinNavmeshPatrolRadiusFactor" init="0.4f"/>
    <float name="m_MaxNavmeshPatrolRadiusFactor" init="0.8f"/>
    <float name="m_RouteRadiusFactor" init="1.5f"/>
  </structdef>

  <structdef type="CTaskReactToBuddyShot::Tunables" base="CTuning">
  </structdef>

  <structdef type="CCombatTaskManager::Tunables" base="CTuning">
    <float name="m_fTimeBetweenUpdates" min="0.0f" max="10.0f" step="0.1f" init="1.12f"/>
    <int name="m_iMaxPedsInCombatTask" min="0" max="80" step="1" init="20"/>
  </structdef>

  <structdef type="CTaskMeleeActionResult::Tunables" base="CTuning">
    <float name="m_ActionModeTime" min="0.0f" max="60.0f" step="0.1f" init="5.0f"/>
    <float name="m_ForceRunDelayTime" min="0.0f" max="60.0f" step="0.1f" init="5.0f"/>
    <u32 name="m_MeleeEnduranceDamage" min="0" max="500" step="1" init="100"/>
  </structdef>

  <structdef type="CCoverDebug::Tunables::BoundInfo" cond="!__FINAL">
    <bool name="m_RenderCells" init="false"/>
    <bool name="m_RenderBoundingAreas" init="false"/>
    <bool name="m_RenderBlockingAreas" init="false"/>
    <bool name="m_BlockObject" init="false"/>
    <bool name="m_BlockVehicle" init="false"/>
    <bool name="m_BlockMap" init="false"/>
    <bool name="m_BlockPlayer" init="true"/>
    <float name="m_BlockingBoundHeight" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
    <float name="m_BlockingBoundGroundExtraOffset" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
  </structdef>

  <structdef type="CCoverDebug::sContextDebugInfo" cond="!__FINAL">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <array name="m_SubContexts" type="atArray">
      <struct type="CCoverDebug::sContextDebugInfo"/>
    </array>
    <u32 name="m_MaxNumDebugDrawables" min="0" max="500" step="1" init="50"/>
  </structdef>

  <structdef type="CCoverDebug::Tunables" base="CTuning" cond="!__FINAL">
    <struct name="m_BoundingAreas" type="CCoverDebug::Tunables::BoundInfo"/>
    <struct name="m_DefaultCoverDebugContext" type="CCoverDebug::sContextDebugInfo"/>
    <s32 name="m_CurrentSelectedContext" min="-1" max="20" step="1" init="-1"/>
    <float name="m_CoverModifier" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <bool name="m_RenderOtherDebug" init="false"/>
    <bool name="m_EnableDebugDynamicCoverFinder" init="true"/>
    <bool name="m_RenderCoverProbeNames" init="false"/>
    <bool name="m_RenderEdges" init="true"/>
    <bool name="m_RenderTurnPoints" init="true"/>
    <bool name="m_RenderAimOutroTests" init="false"/>
    <bool name="m_RenderCornerMoveTestPos" init="false"/>
    <bool name="m_RenderBatchProbeOBB" init="false"/>
    <bool name="m_RenderPlayerCoverSearch" init="false"/>
    <bool name="m_RenderInitialSearchDirection" init="true"/>
    <bool name="m_RenderInitialCoverProbes" init="true"/>
    <bool name="m_RenderCapsuleSearchDirection" init="true"/>
    <bool name="m_RenderCoverCapsuleTests" init="true"/>
    <bool name="m_RenderCollisions" init="true"/>
    <bool name="m_RenderNormals" init="true"/>
    <bool name="m_RenderCoverPoint" init="true"/>
    <bool name="m_RenderCoverPointHelpText" init="true"/>
    <bool name="m_RenderDebug" init="false"/>
    <bool name="m_RenderCoverMoveHelpText" init="false"/>
    <bool name="m_RenderCoverInfo" init="false"/>
    <bool name="m_RenderCoverPoints" init="false"/>
    <bool name="m_RenderCoverPointAddresses" init="false"/>
    <bool name="m_RenderCoverPointTypes" init="false"/>
    <bool name="m_RenderCoverPointArcs" init="false"/>
    <bool name="m_RenderCoverPointHeightRulers" init="false"/>
    <bool name="m_RenderCoverPointUsage" init="false"/>
    <bool name="m_RenderCoverPointLowCorners" init="false"/>
    <bool name="m_RenderSearchDirectionVec" init="false"/>
    <bool name="m_RenderProtectionDirectionVec" init="false"/>
    <bool name="m_RenderCoverArcSearch" init="false"/>
    <bool name="m_RenderCoverPointScores" init="false"/>
    <bool name="m_OutputStickInputToTTY" init="false"/>
    <bool name="m_EnableTaskDebugSpew" init="false"/>
    <bool name="m_EnableMoveStateDebugSpew" init="false"/>
    <bool name="m_UseRagValues" init="false"/>
    <bool name="m_EnableNewCoverDebugRendering" init="true"/>
    <bool name="m_DebugSearchIgnoreMovement" init="false"/>
    <s32 name="m_TextOffset" min="-50" max="50" step="1" init="10"/>
    <s32 name="m_DefaultRenderDuration" min="0" max="50000" step="1000" init="3500"/>
    <s32 name="m_LastCreatedCoverPoint" min="-1" max="50000" step="1" init="0"/>
  </structdef>

</ParserSchema>
