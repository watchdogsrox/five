<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="class">

  <enumdef type="eScriptedAnimFlags" generate="bitset">
    <enumval name="AF_LOOPING" description ="Repeat the clip"/>
    <enumval name="AF_HOLD_LAST_FRAME" description ="Hold on the last frame of the clip"/>
    <enumval name="AF_REPOSITION_WHEN_FINISHED" description ="When the clip finishes pop the peds physical representation position to match the visual representations position"/>
    <enumval name="AF_NOT_INTERRUPTABLE" description ="Can the task not be interrupted by external events"/>
    <enumval name="AF_UPPERBODY" description ="Only plays the upper body part of the clip. Uses upper body ik to reduce lower body movement."/>
    <enumval name="AF_SECONDARY" description ="The task will run in the secondary task slot. This means it can be used as well as a movement task (for instance)"/>
    <enumval name="AF_REORIENT_WHEN_FINISHED" description ="When the clip finishes pop the peds physical representation direction to match the visual representations direction. Note that the animators must not unwind the clip and must have an independent mover node"/>
    <enumval name="AF_ABORT_ON_PED_MOVEMENT" description ="Abort the task early if the ped attempts to move."/>
    <enumval name="AF_ADDITIVE" description ="Play back the animation as an additive"/>
    <enumval name="AF_TURN_OFF_COLLISION" description ="Override physics (will turn off collision and gravity for the duration of the clip)"/>
    <enumval name="AF_OVERRIDE_PHYSICS" description ="Override physics (will turn off collision and gravity for the duration of the clip)"/>
    <enumval name="AF_IGNORE_GRAVITY" description ="Do not apply gravity while the clip is playing"/>
    <enumval name="AF_EXTRACT_INITIAL_OFFSET" description ="Extract an initial offset from the playback position authored by the animators"/>
    <enumval name="AF_EXIT_AFTER_INTERRUPTED" description ="Exit the clip task if it is interrupted by another task (ie Natural Motion).  Without this flag set looped clips will restart ofter the NM task"/>
    <enumval name="AF_TAG_SYNC_IN" description ="Sync the anim with foot steps whilst blending in (use for seamless transitions from walk / run into a full body anim)"/>
    <enumval name="AF_TAG_SYNC_OUT" description ="Sync the anim with foot steps whilst blending out (use for seamless transitions from a full body anim into walking / running behavior)"/>
    <enumval name="AF_TAG_SYNC_CONTINUOUS" description ="// Sync with foot steps all the time (Only useful to synchronize a partial anim e.g. an upper body)"/>
    <enumval name="AF_FORCE_START" description ="Force the anim task to start even if the ped is falling / ragdolling / etc. Can fix issues with peds not playing their anims immediately after a warp / etc"/>
    <enumval name="AF_USE_KINEMATIC_PHYSICS" description ="Use the kinematic physics mode to play back the anim"/>
    <enumval name="AF_USE_MOVER_EXTRACTION" description ="Uses mover extraction to reposition the ped in the correct animated position every frame"/>
    <enumval name="AF_HIDE_WEAPON" description ="Indicates that the ped's weapon should be hidden while this animation is playing."/>
    <enumval name="AF_ENDS_IN_DEAD_POSE" description ="When the anim ends, kill the ped and use the currently playing anim as the dead pose"/>
    <enumval name="AF_ACTIVATE_RAGDOLL_ON_COLLISION" description ="Activate the peds ragdoll (and fall down) on colliding with anything during anim playback."/>
    <enumval name="AF_DONT_EXIT_ON_DEATH" description ="Currently used only on secondary anim tasks. Secondary anim tasks will end automatically when the ped dies. Setting this flag stops that from happening."/>
    <enumval name="AF_ABORT_ON_WEAPON_DAMAGE" description ="Allow aborting from damage events (including non-ragdoll damage events) even when blocking other ai events using AF_NOT_INTERRUPTABLE."/>
    <enumval name="AF_DISABLE_FORCED_PHYSICS_UPDATE" description="Prevent adjusting the capsule on the enter state.  Useful if script is doing a sequence of scripted anims and they are far away" />
    <enumval name="AF_PROCESS_ATTACHMENTS_ON_START" description="Force the attachments to be processed at the start of the clip" />
    <enumval name="AF_EXPAND_PED_CAPSULE_FROM_SKELETON" description="Expands the capsule to the extents of the skeleton" />
    <enumval name="AF_USE_ALTERNATIVE_FP_ANIM" description="Will use an alternative first person version of the anim on the local player when in first person (the anim must be in the same dictionary, and be names the same but with _FP appended to the end)" />
    <enumval name="AF_BLENDOUT_WRT_LAST_FRAME" description="Start blending out the anim early, so that the blend out duration completes at the end of the animation." />
    <enumval name="AF_USE_FULL_BLENDING" description="Use full blending for this anim and override the heading/position adjustment in CTaskScriptedAnimation::CheckIfClonePlayerNeedsHeadingPositionAdjust(), so that we don't correct errors (special case such as scrip-side implemented AI tasks, i.e. diving)" />
  </enumdef>

	<enumdef type="eAnimPriorityFlags" generate="bitset">
		<enumval name="AF_PRIORITY_LOW" description ="Use the lowest TASK_SCRIPTED_ANIMATION slot"/>
		<enumval name="AF_PRIORITY_MEDIUM" description ="Use the medium TASK_SCRIPTED_ANIMATION slot"/>
		<enumval name="AF_PRIORITY_HIGH" description ="Use the high TASK_SCRIPTED_ANIMATION slot"/>
	</enumdef>

  <enumdef type="eSyncedSceneFlags" generate="bitset">
    <enumval name="SYNCED_SCENE_USE_KINEMATIC_PHYSICS" description ="When this flag is set, the task will run in kinematic physics mode (other entities will collide, and be pushed out of the way)"/>
    <enumval name="SYNCED_SCENE_TAG_SYNC_OUT" description ="When this flag is set, The task will do a tag synchronized blend out with the movement behaviour of the ped."/>
    <enumval name="SYNCED_SCENE_DONT_INTERRUPT" description ="When this flag is set, the scene will not be interrupted by ai events like falling, entering water / etc. Also blocks all weapon reactions / etc"/>
    <enumval name="SYNCED_SCENE_ON_ABORT_STOP_SCENE" description ="When this flag is set, the entire scene will be stopped if this task is aborted"/>
    <enumval name="SYNCED_SCENE_ABORT_ON_WEAPON_DAMAGE" description ="When this flag is set, the task will end if the ped takes weapon damage"/>
    <enumval name="SYNCED_SCENE_BLOCK_MOVER_UPDATE" description ="When this flag is set, the task will not update the mover. Can be used to include vehicles in a synced scene when the scene is attached to them"/>
    <enumval name="SYNCED_SCENE_LOOP_WITHIN_SCENE" description ="When this flag is set, the tasks with anims shorter than the scene will loop while the scene is playing"/>
    <enumval name="SYNCED_SCENE_PRESERVE_VELOCITY" description ="When this flag is set, the task will preserve it's velocity on clean up (must be using physics)"/>
    <enumval name="SYNCED_SCENE_EXPAND_PED_CAPSULE_FROM_SKELETON" description ="When this flag is set, the ped capsule will attempt to expand to cover the skeleton during playback"/>
    <enumval name="SYNCED_SCENE_ACTIVATE_RAGDOLL_ON_COLLISION" description ="When this flag is set, the ped will switch to ragdoll and fall down on making contact with a physical object (other than flat ground)"/>
    <enumval name="SYNCED_SCENE_HIDE_WEAPON" description ="When this flag is set, the ped's weapon will be hidden while the scene is playing"/>
    <enumval name="SYNCED_SCENE_ABORT_ON_DEATH" description ="When this flag is set, the task will end if the ped dies"/>
    <enumval name="SYNCED_SCENE_VEHICLE_ABORT_ON_LARGE_IMPACT" description ="When running the scene on a vehicle, allow the scene to abort if the vehicle takes a heavy collision from another vehicle"/>
    <enumval name="SYNCED_SCENE_VEHICLE_ALLOW_PLAYER_ENTRY" description ="When running the scene on a vehicle, allow player peds to enter the vehicle"/>
    <enumval name="SYNCED_SCENE_PROCESS_ATTACHMENTS_ON_START" description ="When this flag is set, process the attachments at the start of the scene"/>
    <enumval name="SYNCED_SCENE_NET_ON_EARLY_NON_PED_STOP_RETURN_TO_START" description ="When this flag is set, a non-ped entity will be returned to their starting position if the scene finishes early"/>
    <enumval name="SYNCED_SCENE_SET_PED_OUT_OF_VEHICLE_AT_START" description ="When this flag is set, The ped will be automatically set out of his vehicle at the start of the scene."/>
    <enumval name="SYNCED_SCENE_NET_DISREGARD_ATTACHMENT_CHECKS" description ="When this flag is set, the attachment checks done in CNetworkSynchronisedScenes::Update when pending start will be disregarded"/>
  </enumdef>

  <enumdef type="eIkControlFlags" generate="bitset">
    <enumval name="AIK_DISABLE_LEG_IK" description ="Disable leg ik during the animation"/>
    <enumval name="AIK_DISABLE_ARM_IK" description ="Disable arm ik during the animation"/>
    <enumval name="AIK_DISABLE_HEAD_IK" description ="Disable head ik during the animation"/>
    <enumval name="AIK_DISABLE_TORSO_IK" description ="Disable torso ik during the animation"/>
    <enumval name="AIK_DISABLE_TORSO_REACT_IK" description ="Disable torso react ik during the animation"/>
    <enumval name="AIK_USE_LEG_ALLOW_TAGS" description="Use anim leg allow tags to determine when leg ik is enabled"/>
    <enumval name="AIK_USE_LEG_BLOCK_TAGS" description="Use anim leg block tags to determine when leg ik is disabled"/>
    <enumval name="AIK_USE_ARM_ALLOW_TAGS" description="Use anim arm allow tags to determine when ik is enabled"/>
    <enumval name="AIK_USE_ARM_BLOCK_TAGS" description="Use anim arm block tags to determine when ik is disabled"/>
    <enumval name="AIK_PROCESS_WEAPON_HAND_GRIP" description="Processes the weapon hand grip ik while the anim is running"/>
    <enumval name="AIK_USE_FP_ARM_LEFT" description="Use first person ik setup for left arm (cannot be used with AIK_DISABLE_ARM_IK)"/>
    <enumval name="AIK_USE_FP_ARM_RIGHT" description="Use first person ik setup for right arm (cannot be used with AIK_DISABLE_ARM_IK)"/>
    <enumval name="AIK_DISABLE_TORSO_VEHICLE_IK" description="Disable torso vehicle ik during the animation"/>
    <enumval name="AIK_LINKED_FACIAL" description="Searches the dictionary of the clip being played for another clip with the _facial suffix to be played as a facial animation."/>
	</enumdef>

  <enumdef type="eRagdollBlockingFlags" generate="bitset">
    <enumval name="RBF_BULLET_IMPACT" description ="Block ragdoll reactions from bullet hits"/>
    <enumval name="RBF_VEHICLE_IMPACT" description ="Block ragdoll reactions from vehicle hits"/>
    <enumval name="RBF_FIRE" description ="Block the on fire ragdoll behaviour"/>
    <enumval name="RBF_ELECTROCUTION" description ="Block stun gun and electric ragdoll fence reactions"/>
    <enumval name="RBF_PLAYER_IMPACT" description ="Block ragdoll from being pushed by the player and other ragdolls"/>
    <enumval name="RBF_EXPLOSION" description ="Block ragdoll reactions from explosions"/>
    <enumval name="RBF_IMPACT_OBJECT" description ="Block ragdoll reactions from being hit by moving objects"/>
    <enumval name="RBF_MELEE" description ="Block melee ragdoll reactions"/>
    <enumval name="RBF_RUBBER_BULLET" description ="Block ragdoll from being hit with rubber bullets"/>
    <enumval name="RBF_FALLING" description ="Disable ragdoll from falling"/>
    <enumval name="RBF_WATER_JET" description ="block ragdoll from water jets"/>
    <enumval name="RBF_DROWNING" description ="Block drowning ragdoll reactions"/>
    <enumval name="RBF_ALLOW_BLOCK_DEAD_PED" description ="By default, ragdoll blocking functions only work while the ped remains alive. turn this on to also block when the ped is dead."/>
    <enumval name="RBF_PLAYER_BUMP" description ="Block ragdoll activations from the player running into the ped. Doesn't block activation from other ragdolls falling on the ped."/>
    <enumval name="RBF_PLAYER_RAGDOLL_BUMP" description ="Block ragdoll activations from the player ragdolling into the ped. Doesn't block activation from collisions with non ragdolling players or ai ragdolls."/>
    <enumval name="RBF_PED_RAGDOLL_BUMP" description ="Block ragdoll activations from othe non player peds ragdolling into the ped. Doesn't block activation from collisions with players (ragdolling or otherwise)."/>
    <enumval name="RBF_VEHICLE_GRAB" description ="Block ragdoll activations from grabbing the car door whilst it's trying to drive off."/>
    <enumval name="RBF_SMOKE_GRENADE" description ="Block ragdoll activations from smoke grenade damage."/>
  </enumdef>
</ParserSchema>
