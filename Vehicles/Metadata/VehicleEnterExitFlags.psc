<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
              generate="class">

<enumdef type="CVehicleEnterExitFlags::Flags"	generate="bitset"> 
	<enumval name="ResumeIfInterupted" 						description="SCRIPT - If the ped is interrupted then the task resumes, restart getting into the vehicle"/>
	<enumval name="WarpToEntryPosition"						description="SCRIPT - Warps the ped to the entrypoint ready to open door/enter"/>
	<enumval name="QuitIfAllDoorsBlocked" 				description="Quit if all the doors to get to the desired seat or seat type are blocked"/>
	<enumval name="JackIfOccupied" 								description="SCRIPT - Force jack a ped, regardless of who is inside"/>
	<enumval name="Warp"													description="SCRIPT - Warps the ped into the vehicle"/>
	<enumval name="WarpAfterTime"									description="Warps the ped into the vehicle after the timer"/>
	<enumval name="DontWaitForCarToStop"					description="SCRIPT - Ped will jump out if vehicle is moving"/>
	<enumval name="DelayForTime"									description=""/>
	<enumval name="DontCloseDoor"									description="SCRIPT - Ped won't close the door as they exit"/>
	<enumval name="WarpIfDoorBlocked"							description="SCRIPT - If the entry point is blocked, ped will warp out"/>
	<enumval name="ForcedExit"										description="Forced exit, ped won't reserve the seat, whoever forced the ped to leave will have already reserved it"/>
	<enumval name="BeJacked"											description="This ped is being jacked, no need to reserve doors just be jacked straight out"/>
	<enumval name="JumpOut"												description="SCRIPT - Dive out of the car rather than waiting sensibly"/>
	<enumval name="ThroughWindscreen"							description="Dive out of the windscreen"/>
	<enumval name="CloseDoorOnly"									description="Approach the door and close it from the outside rather than getting in"/>
	<enumval name="DontUseDriverSeat"							description="When entering a vehicle, consider the driver seat invalid to pick"/>
	<enumval name="DontDefaultWarpIfDoorBlocked"	description="SCRIPT - Dont default WarpIfDoorBlocked flag in TASK_LEAVE_ANY_VEHICLE"/>
	<enumval name="PreferLeftEntry"								description="SCRIPT - Prefer left sided entrypoints"/>
	<enumval name="PreferRightEntry"							description="SCRIPT - Prefer right sided entrypoints"/>
	<enumval name="JustPullPedOut"								description="SCRIPT - Quit once the ped has been pulled out"/>
	<enumval name="UseDirectEntryOnly"						description="SCRIPT - Don't goto an entrypoint which requires us to shuffle"/>
	<enumval name="GetDraggedAlong"								description=""/>
	<enumval name="WarpIfShuffleLinkIsBlocked"		description="SCRIPT - Allow warping out if shuffle link is blocked"/>
	<enumval name="DontJackAnyone"								description="SCRIPT - Don't allow the ped entering/exiting to jack"/>
	<enumval name="WaitForEntryToBeClearOfPeds"		description="SCRIPT - Wait for the peds entry point to be free from peds before exiting"/>
	<enumval name="JustOpenDoor"									description=""/>
	<enumval name="CanWarpOverPassengers"					description=""/>
	<enumval name="WarpOverPassengers"						description=""/>
	<enumval name="HasPickedUpBike"								description=""/>
	<enumval name="IsWarpingOverPassengers"				description=""/>
	<enumval name="WarpIfDoorBlockedNext"					description=""/>
	<enumval name="PlayerControlledVehicleEntry"	description=""/>
	<enumval name="WaitForLeader"									description=""/>
	<enumval name="JackedFromInside"							description=""/>
	<enumval name="IsExitingVehicle"							description=""/>
	<enumval name="IsStreamedExit"								description="Passed into the exit vehicle task so we use the correct streamed exit anim"/>
	<enumval name="VehicleIsUpsideDown"						description=""/>
	<enumval name="VehicleIsOnSide"								description=""/>
	<enumval name="EnteringOnVehicle"							description=""/>
	<enumval name="ExitToWalk"										description=""/>
	<enumval name="ExitToRun"											description=""/>
	<enumval name="IsTransitionToSkyDive"					description=""/>
	<enumval name="InAirExit"											description=""/>
	<enumval name="InWater"												description=""/>
	<enumval name="JackAndGetIn"									description=""/>
	<enumval name="IsArrest"									    description="Set when exiting a vehicle if we're being arrested. Could in theory be used for police busting logic as well"/>
	<enumval name="JackWantedPlayersRatherThanStealCar"	description="Set if this ped should prioritise jacking hated players over stealing the car"/>
	<enumval name="ExitToAim"									    description="Set when exiting a vehicle in combat and wanting to aim at a known target"/>
	<enumval name="DontAbortForCombat"				    description="Prevents a ped from reacting to combat events"/>
	<enumval name="HasInterruptedAnim"					  description="Ped has interrupted the anim early"/>
	<enumval name="DeadFallOut"										description="Ped will fall out of vehicle as if dead"/>
	<enumval name="DontSetPedOutOfVehicle"				description="Set when a ped is dead when getting out"/>
	<enumval name="GettingOffBecauseDriverJacked"	description=""/>
	<enumval name="UseFastClipRate"								description="Use increased enter/exit clip rate"/>
	<enumval name="DontApplyForceWhenSettingPedIntoVehicle" description=""/>
	<enumval name="ExitUnderWater" description=""/>
	<enumval name="IgnoreEntryPointCollisionCheck" description=""/>
	<enumval name="CombatEntry"										description="Entering from aiming/cover"/>
	<enumval name="FromCover"											description="Entering from cover"/>
  <enumval name="IgnoreExitDetachOnCleanup"     description="Bypasses the detach from attachment entity in the cleanup"/>
	<enumval name="ConsiderJackingFriendlyPeds"		description="Used when evaluating which entry point to go to"/>
	<enumval name="WantsToJackFriendlyPed"				description="Player can choose whether or not to jack in certain situations"/>
	<enumval name="HasCheckedForFriendlyJack"			description="Have we checked if we should jack the driver or not?"/>
  <enumval name="ExitToSwim"			              description=""/>
	<enumval name="IsFleeExit"			              description=""/>
   <enumval name="ForceFleeAfterJack"			              description=""/>
  <enumval name="ExitSeatOntoVehicle"			      description=""/>
  <enumval name="AllowBlockedJackReactions"			      description=""/>
  <enumval name="WaitForJackInterrupt"          description=""/>
  <enumval name="UseOnVehicleJack"              description=""/>  
  <enumval name="WaitForCarToSettle"          description=""/>
  <enumval name="HasJackedAPed"									description=""/>
  <enumval name="SwitchedPlacesWithJacker"			description=""/>
  <enumval name="InAirEntry"                  description="Jumping onto a horse from above for example"/>
  <enumval name="JumpEntry"                  description="Jumping onto a horse from above for example"/>
  <enumval name="DropEntry"                  description="Dropping onto a horse from above for example"/>
  <enumval name="UseAlternateShuffleSeat"			description=""/>
  <enumval name="NeedToTurnTurret"			description=" "/>
	<enumval name="UsedClimbUp"			description=" "/>
  <enumval name="DoorInFrontBeingUsed"          description="Limits how far a rear door can open during exit if someone else is using the door in front"/>
  <enumval name="ScriptedTask"          description="Denotes that the task was started from script native call"/>
  </enumdef>

</ParserSchema>
