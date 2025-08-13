<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="DebugReplayObjectInfo" base="debugPlayback::NewObjectData">
    <Vec3V name="m_vBBMax"/>
    <Vec3V name="m_vBBMin"/>
    <u32 name="m_ClassType"/>
    <int name="m_ChainIndex"/>
    <string name="m_ModelName" type="ConstString"/>
    <bool name="m_bPlayerIsDriving"/>
    <bool name="m_bIsPlayer"/>
  </structdef>

<structdef type="QPedSpringEvent" base="debugPlayback::PhysicsEvent">
		<Vec3V name="m_Force"/>
		<Vec3V name="m_Damping"/>
		<float name="m_fMaxForce"/>
		<float name="m_fCompression"/>
		<float name="m_fMass"/>
	</structdef>

<structdef type="PedSpringEvent" base="debugPlayback::PhysicsEvent">
		<Vec3V name="m_vForce"/>
		<Vec3V name="m_vGroundNormal"/>
		<Vec3V name="m_vGroundVelocity"/>
		<float name="m_fSpringCompression"/>
		<float name="m_fSpringDamping"/>
		<float name="m_fMass"/>
	</structdef>

  <structdef type="DRNetBlenderStateEvent" base="debugPlayback::PhysicsEvent">
    <Mat34V name="m_Latest"/>
    <bool name="m_PositionUpdated"/>
    <bool name="m_OrientationUpdated"/>
    <bool name="m_VelocityUpdated"/>
    <bool name="m_AngVelocityUpdated"/>
  </structdef>

  <structdef type="PedGroundInstanceEvent" base="debugPlayback::PhysicsEvent">
		<Mat34V name="m_GroundMatrix"/>
		<Vec3V name="m_vExtent"/>
		<float name="m_fMinSize"/>
    <float name="m_fSpeedDiff"/>
    <float name="m_fSpeedAllowance"/>
    <struct name="m_GroundInstance" type="phInstId"/>
		<int name="m_iPassedAxisCount"/>
		<int name="m_iComponent"/>
	</structdef>

  
  <structdef type="ScriptEntityEvent" base="debugPlayback::PhysicsEvent">
    <string name="m_FuncName" type="atHashString"/>
    <int name="m_iEntityAccessCount"/>
  </structdef>

  <structdef type="EntityAttachmentUpdate" base="debugPlayback::PhysicsEvent">
		<Mat34V name="m_NewMat"/>
		<struct name="m_AttachedTo" type="phInstId"/>
	</structdef>

<structdef type="PedDesiredVelocityEvent" base="debugPlayback::PhysicsEvent">
		<Vec3V name="m_Linear"/>
		<Vec3V name="m_Angular"/>
	</structdef>

  <structdef type="VehicleDataEvent" base="debugPlayback::PhysicsEvent">
    <Vec3V name="m_Linear"/>
	<Vec3V name="m_vVehicleVelocity"/>
    <Vec3V name="m_Angular"/>
    <Vec4V name="m_InterpolationTargetAndTime"/>
    <struct type="StoreSkeleton::u32Quaternion" name="m_InterpolationTargetOrientation"/>
    <float name="m_fSteerAngle"/>
    <float name="m_fSecondSteerAngle"/>
    <float name="m_fCheatPowerIncrease"/>
    <float name="m_fThrottle"/>
    <float name="m_fBrake"/>
    <float name="m_fForwardSpeed"/>
    <float name="m_fDesiredSpeed"/>
    <float name="m_fSmoothedSpeed"/>
    <struct type="StoreSkeleton::u32Quaternion" name="m_Quat"/>
    <bool name="m_bLodEventScanning"/>
    <bool name="m_bLodDummy"/>
    <bool name="m_bLodSuperDummy"/>
    <bool name="m_bLodTimeSlicing"/>
    <bool name="m_bHandBrake"/>
    <bool name="m_bCheatFlagGrip2"/>
  </structdef>

  <structdef type="PedDataEvent" base="debugPlayback::PhysicsEvent">
    <Vec3V name="m_Linear"/>
    <Vec3V name="m_Angular"/>
    <struct type="StoreSkeleton::u32Quaternion" name="m_Quat"/>
  </structdef>
  
  <structdef type="PedCapsuleInfoEvent" base="debugPlayback::PhysicsEvent">
		<Vec3V name="m_Start"/>
		<Vec3V name="m_End"/>
		<Vec3V name="m_Ground"/>
    <Vec3V name="m_Offset"/>
    <float name="m_Radius"/>
	</structdef>

  <structdef type="VehicleAvoidenceDataEvent::StoredObstructionItem">
    <u32 name="m_AsU32"/>
  </structdef>

  <structdef type="VehicleAvoidenceDataEvent" base="debugPlayback::PhysicsEvent">
    <Vec3V name="m_vRight"/>
    <Vec3V name="m_vForwards"/>
    <array name="m_Obstructions" type="atArray">
      <struct type="VehicleAvoidenceDataEvent::StoredObstructionItem"/>
    </array>
    <float name="m_fMinRange"/>
    <float name="m_fMaxRange"/>
    <float name="m_fCentreAngle"/>
    <float name="m_fMaxAngle"/>
    <float name="m_fVehDriveOrientation"/>
  </structdef>

  <structdef type="VehicleTargetPointEvent" base="debugPlayback::PhysicsEvent">
    <string name="m_Task" type="atHashString"/>
    <Vec3V name="m_TargetPos"/>
  </structdef>

  <structdef type="PedAnimatedVelocityEvent" base="debugPlayback::PhysicsEvent">
    <Vec3V name="m_Linear"/>
  </structdef>


  <structdef type="CameraEvent" base="debugPlayback::EventBase">
    <Vec3V name="m_vCamPos"/>
    <Vec3V name="m_vCamUp"/>
    <Vec3V name="m_vCamFront"/>
    <float name="m_fFarClip"/>
    <float name="m_fTanFOV"/>
    <float name="m_fTanVFOV"/>
  </structdef>

  <structdef type="GameRecorder::FilterInfo">
		<array name="m_Filters" type="atArray">
			<string type="ConstString"/>
		</array>
    <bool name="m_bRecordSkeletonsEveryFrame"/>
    <bool name="m_bPosePedsIfPossible"/>
    <bool name="m_bUseLowLodSkeletons"/>
  </structdef>

<structdef type="GameRecorder::AppLevelSaveData" base="debugPlayback::DebugRecorder::DebugRecorderData">
		<array name="m_StoredSkeletonData" type="atArray">
			<pointer type="StoredSkelData" policy="owner"/>
		</array>
		<array name="m_StoredSkeletonDataIDs" type="atArray">
			<size_t/>
		</array>
	</structdef>

<structdef type="CStoreTasks::TaskInfo">
		<string name="m_TaskState" type="atHashString"/>
		<u8 name="m_TaskPriority"/>
	</structdef>

<structdef type="CStoreTasks" base="debugPlayback::PhysicsEvent">
		<array name="m_TaskList" type="atArray">
			<struct type="CStoreTasks::TaskInfo"/>
		</array>
		<string name="m_Title" type="atHashString"/>
	</structdef>

<structdef type="GameRecorder" base="debugPlayback::DebugRecorder">
		<Vec3V name="m_vTeleportLocation"/>
		<bool name="m_bSelectMode"/>
		<bool name="m_bPauseOnEventReplay"/>
		<bool name="m_bDestroyReplayObject"/>
		<bool name="m_bCreateObjectForReplay"/>
		<bool name="m_bInvolvePlayer"/>
		<bool name="m_bHasTeleportLocation"/>
		<bool name="m_bHighlightSelected"/>
	</structdef>

</ParserSchema>