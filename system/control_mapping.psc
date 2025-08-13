<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class">

	<hinsert>
#include		"input/mapper_defs.h"
	</hinsert>

	<autoregister allInFile="true"/>
	
	<!-- NOTE: If you add an input you will also need to edit common:/data/control/settings.meta to assign a mapper the input belongs to! You will also need to add the input to commands_pad.sch. -->
	<enumdef type="rage::InputType" preserveNames="true">
		<!-- General -->
		<enumval name="INPUT_NEXT_CAMERA" description="If you want to add an input contact me (Thomas Randall) or Stephen Robertson so we can ensure all files are updated and add PC mappings." />
		
		<enumval name="INPUT_LOOK_LR"/>
		<enumval name="INPUT_LOOK_UD"/>
		
		<enumval name="INPUT_LOOK_UP_ONLY"/>
		<enumval name="INPUT_LOOK_DOWN_ONLY"/>
		<enumval name="INPUT_LOOK_LEFT_ONLY"/>
		<enumval name="INPUT_LOOK_RIGHT_ONLY"/>
		
		<enumval name="INPUT_CINEMATIC_SLOWMO"/>
		<enumval name="INPUT_SCRIPTED_FLY_UD"/>
		<enumval name="INPUT_SCRIPTED_FLY_LR"/>
		<enumval name="INPUT_SCRIPTED_FLY_ZUP"/>
		<enumval name="INPUT_SCRIPTED_FLY_ZDOWN"/>
				
		<enumval name="INPUT_WEAPON_WHEEL_UD"/>
		<enumval name="INPUT_WEAPON_WHEEL_LR"/>

		<enumval name="INPUT_WEAPON_WHEEL_NEXT"/>
		<enumval name="INPUT_WEAPON_WHEEL_PREV"/>

		<enumval name="INPUT_SELECT_NEXT_WEAPON"/>
		<enumval name="INPUT_SELECT_PREV_WEAPON"/>

		<enumval name="INPUT_SKIP_CUTSCENE"/>
		<enumval name="INPUT_CHARACTER_WHEEL"/>

		<enumval name="INPUT_MULTIPLAYER_INFO"/>

		<!-- On Foot -->
		<enumval name="INPUT_SPRINT"/>
		<enumval name="INPUT_JUMP"/>
		<enumval name="INPUT_ENTER"/>
		<enumval name="INPUT_ATTACK"/>
		<enumval name="INPUT_AIM"/>
		<enumval name="INPUT_LOOK_BEHIND"/>
		<enumval name="INPUT_PHONE"/>

		<enumval name="INPUT_SPECIAL_ABILITY"/>
		<enumval name="INPUT_SPECIAL_ABILITY_SECONDARY"/>

		<enumval name="INPUT_MOVE_LR"/>
		<enumval name="INPUT_MOVE_UD"/>

		<enumval name="INPUT_MOVE_UP_ONLY"/>
		<enumval name="INPUT_MOVE_DOWN_ONLY"/>
		<enumval name="INPUT_MOVE_LEFT_ONLY"/>
		<enumval name="INPUT_MOVE_RIGHT_ONLY"/>

		<enumval name="INPUT_DUCK"/>
		<enumval name="INPUT_SELECT_WEAPON"/>
		<enumval name="INPUT_PICKUP"/>
		<enumval name="INPUT_SNIPER_ZOOM"/>
		<enumval name="INPUT_SNIPER_ZOOM_IN_ONLY"/>
		<enumval name="INPUT_SNIPER_ZOOM_OUT_ONLY"/>
		<enumval name="INPUT_SNIPER_ZOOM_IN_SECONDARY"/>
		<enumval name="INPUT_SNIPER_ZOOM_OUT_SECONDARY"/>
		<enumval name="INPUT_COVER"/>
		<enumval name="INPUT_RELOAD"/>
		<enumval name="INPUT_TALK"/>
		<enumval name="INPUT_DETONATE"/>
		<enumval name="INPUT_HUD_SPECIAL"/>
		<enumval name="INPUT_ARREST"/>
		<enumval name="INPUT_ACCURATE_AIM"/>
		<enumval name="INPUT_CONTEXT"/>
		<enumval name="INPUT_CONTEXT_SECONDARY"/>
		<enumval name="INPUT_WEAPON_SPECIAL"/>
		<enumval name="INPUT_WEAPON_SPECIAL_TWO"/>
		<enumval name="INPUT_DIVE"/>
		<enumval name="INPUT_DROP_WEAPON"/>
		<enumval name="INPUT_DROP_AMMO"/>
		<enumval name="INPUT_THROW_GRENADE"/>
	
		<!-- In Vehicle -->
		<enumval name="INPUT_VEH_MOVE_LR"/>
		<enumval name="INPUT_VEH_MOVE_UD"/>

		<enumval name="INPUT_VEH_MOVE_UP_ONLY"/>
		<enumval name="INPUT_VEH_MOVE_DOWN_ONLY"/>
		<enumval name="INPUT_VEH_MOVE_LEFT_ONLY"/>
		<enumval name="INPUT_VEH_MOVE_RIGHT_ONLY"/>

		<enumval name="INPUT_VEH_SPECIAL"/>
		<enumval name="INPUT_VEH_GUN_LR"/>
		<enumval name="INPUT_VEH_GUN_UD"/>
		<enumval name="INPUT_VEH_AIM"/>
		<enumval name="INPUT_VEH_ATTACK"/>
		<enumval name="INPUT_VEH_ATTACK2"/>
		<enumval name="INPUT_VEH_ACCELERATE"/>
		<enumval name="INPUT_VEH_BRAKE"/>
		<enumval name="INPUT_VEH_DUCK"/>
		<enumval name="INPUT_VEH_HEADLIGHT"/>
		<enumval name="INPUT_VEH_EXIT"/>
		<enumval name="INPUT_VEH_HANDBRAKE"/>
		<enumval name="INPUT_VEH_HOTWIRE_LEFT"/>
		<enumval name="INPUT_VEH_HOTWIRE_RIGHT"/>
		<enumval name="INPUT_VEH_LOOK_BEHIND"/>
		<enumval name="INPUT_VEH_CIN_CAM"/>
		<enumval name="INPUT_VEH_NEXT_RADIO"/>
		<enumval name="INPUT_VEH_PREV_RADIO"/>
		<enumval name="INPUT_VEH_NEXT_RADIO_TRACK"/>
		<enumval name="INPUT_VEH_PREV_RADIO_TRACK"/>
		<enumval name="INPUT_VEH_RADIO_WHEEL"/>
		<enumval name="INPUT_VEH_HORN"/>
		<enumval name="INPUT_VEH_FLY_THROTTLE_UP"/>
		<enumval name="INPUT_VEH_FLY_THROTTLE_DOWN"/>
		<enumval name="INPUT_VEH_FLY_YAW_LEFT"/>
		<enumval name="INPUT_VEH_FLY_YAW_RIGHT"/>
		<enumval name="INPUT_VEH_PASSENGER_AIM"/>
		<enumval name="INPUT_VEH_PASSENGER_ATTACK"/>
		<enumval name="INPUT_VEH_SPECIAL_ABILITY_FRANKLIN"/>
		<enumval name="INPUT_VEH_STUNT_UD"/>
		<enumval name="INPUT_VEH_CINEMATIC_UD"/>
		<enumval name="INPUT_VEH_CINEMATIC_UP_ONLY"/>
		<enumval name="INPUT_VEH_CINEMATIC_DOWN_ONLY"/>
		<enumval name="INPUT_VEH_CINEMATIC_LR"/>
		<enumval name="INPUT_VEH_SELECT_NEXT_WEAPON"/>
		<enumval name="INPUT_VEH_SELECT_PREV_WEAPON"/>
		<enumval name="INPUT_VEH_ROOF"/>
		<enumval name="INPUT_VEH_JUMP"/>
		<enumval name="INPUT_VEH_GRAPPLING_HOOK"/>
		<enumval name="INPUT_VEH_SHUFFLE"/>
		<enumval name="INPUT_VEH_DROP_PROJECTILE"/>
		<enumval name="INPUT_VEH_MOUSE_CONTROL_OVERRIDE"/>
		

		<!-- In Aircraft -->
		<enumval name="INPUT_VEH_FLY_ROLL_LR"/>
		<enumval name="INPUT_VEH_FLY_ROLL_LEFT_ONLY"/>
		<enumval name="INPUT_VEH_FLY_ROLL_RIGHT_ONLY"/>
		<enumval name="INPUT_VEH_FLY_PITCH_UD"/>
		<enumval name="INPUT_VEH_FLY_PITCH_UP_ONLY"/>
		<enumval name="INPUT_VEH_FLY_PITCH_DOWN_ONLY"/>
		<enumval name="INPUT_VEH_FLY_UNDERCARRIAGE"/>
		<enumval name="INPUT_VEH_FLY_ATTACK"/>
		<enumval name="INPUT_VEH_FLY_SELECT_NEXT_WEAPON"/>
		<enumval name="INPUT_VEH_FLY_SELECT_PREV_WEAPON"/>
		<enumval name="INPUT_VEH_FLY_SELECT_TARGET_LEFT"/>
		<enumval name="INPUT_VEH_FLY_SELECT_TARGET_RIGHT"/>
		<enumval name="INPUT_VEH_FLY_VERTICAL_FLIGHT_MODE"/>
		<enumval name="INPUT_VEH_FLY_DUCK"/>
		<enumval name="INPUT_VEH_FLY_ATTACK_CAMERA"/>
		<enumval name="INPUT_VEH_FLY_MOUSE_CONTROL_OVERRIDE"/>

		<!-- In Submarine -->
		<enumval name="INPUT_VEH_SUB_TURN_LR"/>
		<enumval name="INPUT_VEH_SUB_TURN_LEFT_ONLY"/>
		<enumval name="INPUT_VEH_SUB_TURN_RIGHT_ONLY"/>
		<enumval name="INPUT_VEH_SUB_PITCH_UD"/>
		<enumval name="INPUT_VEH_SUB_PITCH_UP_ONLY"/>
		<enumval name="INPUT_VEH_SUB_PITCH_DOWN_ONLY"/>
		<enumval name="INPUT_VEH_SUB_THROTTLE_UP"/>
		<enumval name="INPUT_VEH_SUB_THROTTLE_DOWN"/>
		<enumval name="INPUT_VEH_SUB_ASCEND"/>
		<enumval name="INPUT_VEH_SUB_DESCEND"/>
		<enumval name="INPUT_VEH_SUB_TURN_HARD_LEFT"/>
		<enumval name="INPUT_VEH_SUB_TURN_HARD_RIGHT"/>
		<enumval name="INPUT_VEH_SUB_MOUSE_CONTROL_OVERRIDE"/>

		<!-- On pushbike -->
		<enumval name="INPUT_VEH_PUSHBIKE_PEDAL"/>
		<enumval name="INPUT_VEH_PUSHBIKE_SPRINT"/>
		<enumval name="INPUT_VEH_PUSHBIKE_FRONT_BRAKE"/>
		<enumval name="INPUT_VEH_PUSHBIKE_REAR_BRAKE"/>
		
			<!-- Melee -->
		<enumval name="INPUT_MELEE_ATTACK_LIGHT"/>
		<enumval name="INPUT_MELEE_ATTACK_HEAVY"/>
		<enumval name="INPUT_MELEE_ATTACK_ALTERNATE"/>
		<enumval name="INPUT_MELEE_BLOCK"/>
		
		<!-- Parachute -->
		<enumval name="INPUT_PARACHUTE_DEPLOY"/>
		<enumval name="INPUT_PARACHUTE_DETACH"/>
		<enumval name="INPUT_PARACHUTE_TURN_LR"/>
		<enumval name="INPUT_PARACHUTE_TURN_LEFT_ONLY"/>
		<enumval name="INPUT_PARACHUTE_TURN_RIGHT_ONLY"/>
		<enumval name="INPUT_PARACHUTE_PITCH_UD"/>
		<enumval name="INPUT_PARACHUTE_PITCH_UP_ONLY"/>
		<enumval name="INPUT_PARACHUTE_PITCH_DOWN_ONLY"/>
		<enumval name="INPUT_PARACHUTE_BRAKE_LEFT"/>
		<enumval name="INPUT_PARACHUTE_BRAKE_RIGHT"/>
		<enumval name="INPUT_PARACHUTE_SMOKE"/>
		<enumval name="INPUT_PARACHUTE_PRECISION_LANDING"/>


		<!-- PC Specific -->
		<enumval name="INPUT_MAP"/>

		<enumval name="INPUT_SELECT_WEAPON_UNARMED"/>
		<enumval name="INPUT_SELECT_WEAPON_MELEE"/>
		<enumval name="INPUT_SELECT_WEAPON_HANDGUN"/>
		<enumval name="INPUT_SELECT_WEAPON_SHOTGUN"/>
		<enumval name="INPUT_SELECT_WEAPON_SMG"/>
		<enumval name="INPUT_SELECT_WEAPON_AUTO_RIFLE"/>
		<enumval name="INPUT_SELECT_WEAPON_SNIPER"/>
		<enumval name="INPUT_SELECT_WEAPON_HEAVY"/>
		<enumval name="INPUT_SELECT_WEAPON_SPECIAL"/>

		<enumval name="INPUT_SELECT_CHARACTER_MICHAEL"/>
		<enumval name="INPUT_SELECT_CHARACTER_FRANKLIN"/>
		<enumval name="INPUT_SELECT_CHARACTER_TREVOR"/>
		<enumval name="INPUT_SELECT_CHARACTER_MULTIPLAYER"/>

		<enumval name="INPUT_SAVE_REPLAY_CLIP"/>
		<enumval name="INPUT_SPECIAL_ABILITY_PC"/>

		<!-- Cellphone -->
		<enumval name="INPUT_CELLPHONE_UP"/>
		<enumval name="INPUT_CELLPHONE_DOWN"/>
		<enumval name="INPUT_CELLPHONE_LEFT"/>
		<enumval name="INPUT_CELLPHONE_RIGHT"/>
		<enumval name="INPUT_CELLPHONE_SELECT"/>
		<enumval name="INPUT_CELLPHONE_CANCEL"/>
		<enumval name="INPUT_CELLPHONE_OPTION"/>
		<enumval name="INPUT_CELLPHONE_EXTRA_OPTION"/>
		<enumval name="INPUT_CELLPHONE_SCROLL_FORWARD"/>
		<enumval name="INPUT_CELLPHONE_SCROLL_BACKWARD"/>
		<enumval name="INPUT_CELLPHONE_CAMERA_FOCUS_LOCK"/>
		<enumval name="INPUT_CELLPHONE_CAMERA_GRID"/>
		<enumval name="INPUT_CELLPHONE_CAMERA_SELFIE"/>
		<enumval name="INPUT_CELLPHONE_CAMERA_DOF"/>
		<enumval name="INPUT_CELLPHONE_CAMERA_EXPRESSION"/>
		
		<!-- Frontend -->
		<enumval name="INPUT_FRONTEND_DOWN"/>
		<enumval name="INPUT_FRONTEND_UP"/>
		<enumval name="INPUT_FRONTEND_LEFT"/>
		<enumval name="INPUT_FRONTEND_RIGHT"/>
		<enumval name="INPUT_FRONTEND_RDOWN"/>
		<enumval name="INPUT_FRONTEND_RUP"/>
		<enumval name="INPUT_FRONTEND_RLEFT"/>
		<enumval name="INPUT_FRONTEND_RRIGHT"/>
		<enumval name="INPUT_FRONTEND_AXIS_X"/>
		<enumval name="INPUT_FRONTEND_AXIS_Y"/>
		<enumval name="INPUT_FRONTEND_RIGHT_AXIS_X"/>
		<enumval name="INPUT_FRONTEND_RIGHT_AXIS_Y"/>
		<enumval name="INPUT_FRONTEND_PAUSE"/>
		<enumval name="INPUT_FRONTEND_PAUSE_ALTERNATE" description="Alternate pause to get around conflict issues with Esc going back on PC." />
		<enumval name="INPUT_FRONTEND_ACCEPT"/>
		<enumval name="INPUT_FRONTEND_CANCEL"/>
		<enumval name="INPUT_FRONTEND_X"/>
		<enumval name="INPUT_FRONTEND_Y"/>
		<enumval name="INPUT_FRONTEND_LB"/>
		<enumval name="INPUT_FRONTEND_RB"/>
		<enumval name="INPUT_FRONTEND_LT"/>
		<enumval name="INPUT_FRONTEND_RT"/>
		<enumval name="INPUT_FRONTEND_LS"/>
		<enumval name="INPUT_FRONTEND_RS"/>
		<enumval name="INPUT_FRONTEND_LEADERBOARD"/>
		<enumval name="INPUT_FRONTEND_SOCIAL_CLUB"/>
		<enumval name="INPUT_FRONTEND_SOCIAL_CLUB_SECONDARY"/>
		<enumval name="INPUT_FRONTEND_DELETE"/>
		<enumval name="INPUT_FRONTEND_ENDSCREEN_ACCEPT"/> 
		<enumval name="INPUT_FRONTEND_ENDSCREEN_EXPAND"/> 
		<enumval name="INPUT_FRONTEND_SELECT"/>

		<!-- New scripted commands to replace use of direct pad controls and front end inputs in minigames, etc. -->

		<enumval name="INPUT_SCRIPT_LEFT_AXIS_X"/>
		<enumval name="INPUT_SCRIPT_LEFT_AXIS_Y"/>
		<enumval name="INPUT_SCRIPT_RIGHT_AXIS_X"/>
		<enumval name="INPUT_SCRIPT_RIGHT_AXIS_Y"/>
		<enumval name="INPUT_SCRIPT_RUP"/>
		<enumval name="INPUT_SCRIPT_RDOWN"/>
		<enumval name="INPUT_SCRIPT_RLEFT"/>
		<enumval name="INPUT_SCRIPT_RRIGHT"/>
		<enumval name="INPUT_SCRIPT_LB"/>
		<enumval name="INPUT_SCRIPT_RB"/>
		<enumval name="INPUT_SCRIPT_LT"/>
		<enumval name="INPUT_SCRIPT_RT"/>
		<enumval name="INPUT_SCRIPT_LS"/>
		<enumval name="INPUT_SCRIPT_RS"/>
		<enumval name="INPUT_SCRIPT_PAD_UP"/>
		<enumval name="INPUT_SCRIPT_PAD_DOWN"/>
		<enumval name="INPUT_SCRIPT_PAD_LEFT"/>
		<enumval name="INPUT_SCRIPT_PAD_RIGHT"/>
		<enumval name="INPUT_SCRIPT_SELECT"/>

		<!-- NEXT GEN & PC -->
		<enumval name="INPUT_CURSOR_ACCEPT" description="The cursor's primary button (e.g. mouse click or touch pad/screen."/>
		<enumval name="INPUT_CURSOR_CANCEL" description="The cursor's secondary button (e.g. mouse right click."/>
		<enumval name="INPUT_CURSOR_X" description="The cursor X position in the range 0...1" />
		<enumval name="INPUT_CURSOR_Y" description="The cursor Y position in the range 0...1" />
		<enumval name="INPUT_CURSOR_SCROLL_UP" description="Cursor scroll up (e.g. mousewheel) 0...1" />
		<enumval name="INPUT_CURSOR_SCROLL_DOWN" description="Cursor scroll up (e.g. mousewheel) 0...1" />
		<enumval name="INPUT_ENTER_CHEAT_CODE" description="Brings up the cheat code input box." />
		<enumval name="INPUT_INTERACTION_MENU" description="Opens interaction menu." />
		<enumval name="INPUT_MP_TEXT_CHAT_ALL" description="Opens a text chat box, transmitting to all players on a server" />
		<enumval name="INPUT_MP_TEXT_CHAT_TEAM" description="Opens a text chat box, transmitting to all members of the player's team" />
		<enumval name="INPUT_MP_TEXT_CHAT_FRIENDS" description="Opens a text chat box, transmitting to all the player's friends on a server" />
		<enumval name="INPUT_MP_TEXT_CHAT_CREW" description="Opens a text chat box, transmitting to all the player's friends on a server" />
		<enumval name="INPUT_PUSH_TO_TALK" description="Press to transmit voice." />

		
		<!-- CREATORS - Additional inputs to allow more sensible mapping of mouse and keyboard controls to creators -->
		<enumval name="INPUT_CREATOR_LS" description="Creator Left stick equivalent for functions like field of view, etc./" />
		<enumval name="INPUT_CREATOR_RS" description="Creator right stick equivalent for functions like field of view, etc./" />
		<enumval name="INPUT_CREATOR_LT" description="Creator left trigger equivalent for zooming./" />
		<enumval name="INPUT_CREATOR_RT" description="Creator right trigger equivalent for zooming./" />
		<enumval name="INPUT_CREATOR_MENU_TOGGLE" description="Creator menu toggle on/off for PC keyboard and mouse." />
		<enumval name="INPUT_CREATOR_ACCEPT" description="Creator accept/place items button." />
		<enumval name="INPUT_CREATOR_DELETE" description="Creator delete items button." />
		
				
		<!-- LEGACY SUPPORT -->
		<enumval name="INPUT_ATTACK2"/>
		<enumval name="INPUT_RAPPEL_JUMP"/>
		<enumval name="INPUT_RAPPEL_LONG_JUMP"/>
		<enumval name="INPUT_RAPPEL_SMASH_WINDOW"/>
		<enumval name="INPUT_PREV_WEAPON"/>
		<enumval name="INPUT_NEXT_WEAPON"/>
		<enumval name="INPUT_MELEE_ATTACK1"	description="This has been renamed to INPUT_MELEE_ATTACK_LIGHT. If this should be kept as it is, let me know thomas.randall@rockstarleeds.com"/>
		<enumval name="INPUT_MELEE_ATTACK2"	description="This has been renamed to INPUT_MELEE_ATTACK_HEAVY. If this should be kept as it is, let me know thomas.randall@rockstarleeds.com"/>
		<enumval name="INPUT_WHISTLE"		description="This is mapped to nothing and is not used anywhere, it is added here so scripts work. If this should be kept let me know thomas.randall@rockstarleeds.com"/>
		<enumval name="INPUT_MOVE_LEFT"/>
		<enumval name="INPUT_MOVE_RIGHT"/>
		<enumval name="INPUT_MOVE_UP"/>
		<enumval name="INPUT_MOVE_DOWN"/>
		<enumval name="INPUT_LOOK_LEFT"/>
		<enumval name="INPUT_LOOK_RIGHT"/>
		<enumval name="INPUT_LOOK_UP"/>
		<enumval name="INPUT_LOOK_DOWN"/>
		<enumval name="INPUT_SNIPER_ZOOM_IN"/>
		<enumval name="INPUT_SNIPER_ZOOM_OUT"/>
		<enumval name="INPUT_SNIPER_ZOOM_IN_ALTERNATE"/>
		<enumval name="INPUT_SNIPER_ZOOM_OUT_ALTERNATE"/>
		<enumval name="INPUT_VEH_MOVE_LEFT"/>
		<enumval name="INPUT_VEH_MOVE_RIGHT"/>
		<enumval name="INPUT_VEH_MOVE_UP"/>
		<enumval name="INPUT_VEH_MOVE_DOWN"/>
		<enumval name="INPUT_VEH_GUN_LEFT"/>
		<enumval name="INPUT_VEH_GUN_RIGHT"/>
		<enumval name="INPUT_VEH_GUN_UP"/>
		<enumval name="INPUT_VEH_GUN_DOWN"/>
		<enumval name="INPUT_VEH_LOOK_LEFT"/>
		<enumval name="INPUT_VEH_LOOK_RIGHT"/>

		<!-- These have been mobed as some of these will be needed as the frontend inputs should be used. They are here to fix the code/script dependancy issue. -->
		
		<enumval name="INPUT_REPLAY_START_STOP_RECORDING"/>
		<enumval name="INPUT_REPLAY_START_STOP_RECORDING_SECONDARY"/>

		<enumval name="INPUT_SCALED_LOOK_LR"/>
		<enumval name="INPUT_SCALED_LOOK_UD"/>
		<enumval name="INPUT_SCALED_LOOK_UP_ONLY"/>
		<enumval name="INPUT_SCALED_LOOK_DOWN_ONLY"/>
		<enumval name="INPUT_SCALED_LOOK_LEFT_ONLY"/>
		<enumval name="INPUT_SCALED_LOOK_RIGHT_ONLY"/>

		<enumval name="INPUT_REPLAY_MARKER_DELETE"/>
    <enumval name="INPUT_REPLAY_CLIP_DELETE"/>
		<enumval name="INPUT_REPLAY_PAUSE"/>
		<enumval name="INPUT_REPLAY_REWIND"/>
		<enumval name="INPUT_REPLAY_FFWD"/>
		<enumval name="INPUT_REPLAY_NEWMARKER"/>
		<enumval name="INPUT_REPLAY_RECORD"/>
		<enumval name="INPUT_REPLAY_SCREENSHOT"/>
		<enumval name="INPUT_REPLAY_HIDEHUD"/>
		<enumval name="INPUT_REPLAY_STARTPOINT"/>
		<enumval name="INPUT_REPLAY_ENDPOINT"/>
		<enumval name="INPUT_REPLAY_ADVANCE"/>
		<enumval name="INPUT_REPLAY_BACK"/>
		<enumval name="INPUT_REPLAY_TOOLS"/>
		<enumval name="INPUT_REPLAY_RESTART"/>
		<enumval name="INPUT_REPLAY_SHOWHOTKEY"/>
		<enumval name="INPUT_REPLAY_CYCLEMARKERLEFT"/>
		<enumval name="INPUT_REPLAY_CYCLEMARKERRIGHT"/>
		<enumval name="INPUT_REPLAY_FOVINCREASE"/>
		<enumval name="INPUT_REPLAY_FOVDECREASE"/>
		<enumval name="INPUT_REPLAY_CAMERAUP"/>
		<enumval name="INPUT_REPLAY_CAMERADOWN"/>
		<enumval name="INPUT_REPLAY_SAVE"/>
		<enumval name="INPUT_REPLAY_TOGGLETIME"/>
		<enumval name="INPUT_REPLAY_TOGGLETIPS"/>
		<enumval name="INPUT_REPLAY_PREVIEW"/>
		<enumval name="INPUT_REPLAY_TOGGLE_TIMELINE"/>
		<enumval name="INPUT_REPLAY_TIMELINE_PICKUP_CLIP"/>
		<enumval name="INPUT_REPLAY_TIMELINE_DUPLICATE_CLIP"/>
		<enumval name="INPUT_REPLAY_TIMELINE_PLACE_CLIP"/>
		<enumval name="INPUT_REPLAY_CTRL"/>
		<enumval name="INPUT_REPLAY_TIMELINE_SAVE"/>
    <enumval name="INPUT_REPLAY_PREVIEW_AUDIO"/>

    <enumval name="INPUT_VEH_DRIVE_LOOK"/>
		<enumval name="INPUT_VEH_DRIVE_LOOK2"/>
		<enumval name="INPUT_VEH_FLY_ATTACK2"/>

		<enumval name="INPUT_RADIO_WHEEL_UD"/>
		<enumval name="INPUT_RADIO_WHEEL_LR"/>

		<enumval name="INPUT_VEH_SLOWMO_UD"/>
		<enumval name="INPUT_VEH_SLOWMO_UP_ONLY"/>
		<enumval name="INPUT_VEH_SLOWMO_DOWN_ONLY"/>

		<enumval name="INPUT_VEH_HYDRAULICS_CONTROL_TOGGLE"/>
		<enumval name="INPUT_VEH_HYDRAULICS_CONTROL_LEFT"/>
		<enumval name="INPUT_VEH_HYDRAULICS_CONTROL_RIGHT"/>
		<enumval name="INPUT_VEH_HYDRAULICS_CONTROL_UP"/>
		<enumval name="INPUT_VEH_HYDRAULICS_CONTROL_DOWN"/>
		<enumval name="INPUT_VEH_HYDRAULICS_CONTROL_LR"/>
		<enumval name="INPUT_VEH_HYDRAULICS_CONTROL_UD"/>

		<enumval name="INPUT_SWITCH_VISOR"/>
		<enumval name="INPUT_VEH_MELEE_HOLD"/>
		<enumval name="INPUT_VEH_MELEE_LEFT"/>
		<enumval name="INPUT_VEH_MELEE_RIGHT"/>
				
		<enumval name="INPUT_MAP_POI"/>
		
		<enumval name="INPUT_REPLAY_SNAPMATIC_PHOTO"/>

    <enumval name="INPUT_VEH_CAR_JUMP"/>
    <enumval name="INPUT_VEH_ROCKET_BOOST"/>
    <enumval name="INPUT_VEH_FLY_BOOST"/>
    <enumval name="INPUT_VEH_PARACHUTE"/>
		<enumval name="INPUT_VEH_BIKE_WINGS"/>

		<enumval name="INPUT_VEH_FLY_BOMB_BAY"/>
		<enumval name="INPUT_VEH_FLY_COUNTER"/>

		<enumval name="INPUT_VEH_TRANSFORM"/>
		<enumval name="INPUT_QUAD_LOCO_REVERSE"/>
		
		<enumval name="INPUT_RESPAWN_FASTER"/>

    <!-- removed because of CNC keyword <enumval name="INPUT_CNC_INTERACT_ATM"/>  -->
		

    <enumval name="INPUT_HUDMARKER_SELECT"/>

    <enumval name="INPUT_EAT_SNACK" description="Used in the GTAO weapon wheel"/>
    <enumval name="INPUT_USE_ARMOR" description="Used in the GTAO weapon wheel"/>
    <!-- THESE SHOULD ALWAYS BE THE LAST EMUM VALUES (IN THIS ORDER) AS THEY ARE USED TO KEEP TRACK OF THE NUMBER OF INPUTS -->
		<enumval name="MAX_INPUTS"/>
		<enumval name="UNDEFINED_INPUT"	value="-1"		description="NOT an actual input and should NEVER be used (except to signify no input and error detection)!"/>
		
		<!-- THESE SHOULD NEVER BE USED. THEY ARE FOR THE DYNAMIC MAPPING SYSTEM TO TIE MAPPINGS TO CENTERED MOUSE INPUT -->
		<enumval name="DYNAMIC_MAPPING_MOUSE_X" value="-10" description="These should never be used. They are for the dynamic mapping system to tie mappings to centered mouse input!" />
		<enumval name="DYNAMIC_MAPPING_MOUSE_Y" value="-20" description="These should never be used. They are for the dynamic mapping system to tie mappings to centered mouse input!" />


		<!-- IMPORTANT: UPDATED THESE IF NEW SCRIPTED INPUTS ARE ADDED OR ALTERED! -->
		<enumval name="FIRST_INPUT"          value="INPUT_NEXT_CAMERA"         description="IMPORTANT: UPDATED THESE IF NEW SCRIPTED INPUTS ARE ADDED OR ALTERED!" />
		<enumval name="SCRIPTED_INPUT_FIRST" value ="INPUT_SCRIPT_LEFT_AXIS_X" description="IMPORTANT: UPDATED THESE IF NEW SCRIPTED INPUTS ARE ADDED OR ALTERED!" />
		<enumval name="SCRIPTED_INPUT_LAST"  value ="INPUT_SCRIPT_SELECT"      description="IMPORTANT: UPDATED THESE IF NEW SCRIPTED INPUTS ARE ADDED OR ALTERED!" />
	</enumdef>
	
	<enumdef type="rage::Mapper" preserveNames="true">
		<enumval name="ON_FOOT"/>
		<enumval name="IN_VEHICLE"/>
		<enumval name="MELEE"/>
		<enumval name="PARACHUTE"/>
		<enumval name="FRONTEND"/>
		<enumval name="REPLAY"/>
		
		<!-- IMPORTANT: Inputs that are marked assigned to the DEPRECATED mapper should not be used. -->
		<enumval name="DEPRECATED_MAPPER" description="IMPORTANT: Inputs that are marked assigned to the DEPRECATED mapper should not be used." />

		<!-- IMPORTANT: Not an actual mapper and this is needed to be the number of mappers. -->
		<enumval name="MAPPERS_MAX"/>
		
		<!-- Not an acutal mapper and is used internally for an unkown/undifined mapper. -->
		<enumval name="UNDEFINED_MAPPER" value="-1" description="NOT an actual mapper and should NEVER be used (except to signify no mapper or error detection)!"/>
	</enumdef>

	<!--
		Input Groups are used for grouping rage::InputType for the creation of help text icons. For example, INPUTGROUP_MOVE would be made up from
		INPUT_MOVE_UD and INPUT_MOVE_LR and will produce a/some icon(s) depending on the source, e.g. If the same stick is used for both it would show
		a stick being pushed in all four directions rather than two icons for up/down and left/right.
	-->
	<enumdef type="rage::InputGroup" preserveNames="true">
		<enumval name="INPUTGROUP_MOVE"/>
		<enumval name="INPUTGROUP_LOOK"/>
		<enumval name="INPUTGROUP_WHEEL"/>
		<enumval name="INPUTGROUP_CELLPHONE_NAVIGATE" />
		<enumval name="INPUTGROUP_CELLPHONE_NAVIGATE_UD" />
		<enumval name="INPUTGROUP_CELLPHONE_NAVIGATE_LR" />
		<enumval name="INPUTGROUP_FRONTEND_DPAD_ALL" />
		<enumval name="INPUTGROUP_FRONTEND_DPAD_UD" />
		<enumval name="INPUTGROUP_FRONTEND_DPAD_LR" />
		<enumval name="INPUTGROUP_FRONTEND_LSTICK_ALL" />
		<enumval name="INPUTGROUP_FRONTEND_RSTICK_ALL" />
		<enumval name="INPUTGROUP_FRONTEND_GENERIC_UD" />
		<enumval name="INPUTGROUP_FRONTEND_GENERIC_LR" />
		<enumval name="INPUTGROUP_FRONTEND_GENERIC_ALL" />
		<enumval name="INPUTGROUP_FRONTEND_BUMPERS" />
		<enumval name="INPUTGROUP_FRONTEND_TRIGGERS" />
		<enumval name="INPUTGROUP_FRONTEND_STICKS" />
		<enumval name="INPUTGROUP_SCRIPT_DPAD_ALL" />
		<enumval name="INPUTGROUP_SCRIPT_DPAD_UD" />
		<enumval name="INPUTGROUP_SCRIPT_DPAD_LR" />
		<enumval name="INPUTGROUP_SCRIPT_LSTICK_ALL" />
		<enumval name="INPUTGROUP_SCRIPT_RSTICK_ALL" />
		<enumval name="INPUTGROUP_SCRIPT_BUMPERS" />
		<enumval name="INPUTGROUP_SCRIPT_TRIGGERS" />
		<enumval name="INPUTGROUP_WEAPON_WHEEL_CYCLE" />
		<enumval name="INPUTGROUP_FLY"/>
		<enumval name="INPUTGROUP_SUB"/>
		<enumval name="INPUTGROUP_VEH_MOVE_ALL"/>
		<enumval name="INPUTGROUP_CURSOR"/>
		<enumval name="INPUTGROUP_CURSOR_SCROLL"/>
		<enumval name="INPUTGROUP_SNIPER_ZOOM_SECONDARY"/>
    	<enumval name="INPUTGROUP_VEH_HYDRAULICS_CONTROL"/>

		<enumval name="MAX_INPUTGROUPS"/>
		<enumval name="INPUTGROUP_INVALID" value="-1"/>
	</enumdef>

	<const name="::rage::MAX_INPUTGROUPS" value="32"/>

	<!--
		The types of Input Groups
	-->
	<enumdef type="rage::InputGroupTypes" preserveNames="true">
		<enumval name="IGT_SINGLE_OR_DOUBLE_AXIS"	value="2" />
		<enumval name="IGT_GENERIC_SINGLE_AXIS"		value="3" />
		<enumval name="IGT_DOUBLE_DPAD_AXIS"		value="4" />
		<enumval name="IGT_GENERIC_DOUBLE_AXIS"		value="6" />
	</enumdef>
	
	<!--
		Enumconsts are used to hold various constants that the ControlInput settings use for parser constants.
	-->
	<enumdef type="rage::ControlInput::EnumConsts">
		<enumval name="INPUT_GROUP_LIMIT" value="6" description="The maximum amount of inputs that can be part of an input group." />
	</enumdef>

	<const name="::rage::ControlInput::INPUT_GROUP_LIMIT" value="6"/>

	<!--
		AxisDefinitions specify a dependancy between an axis input and a non axis input. For example INPUT_MOVE_UD is an alternate representation of INPUT_MOVE_UP_ONLY and INPUT_MOVE_DOWN_ONLY.
		The Following Definition:
		
			<Input>INPUT_MOVE_UD</Input>
			<Negative>INPUT_MOVE_UP_ONLY</Negative>
			<Positive>INPUT_MOVE_DOWN_ONLY</Positive>
		
		Specifies that the Axis INPUT_MOVE_UD uses the key/mouse/gamepad binding for INPUT_MOVE_UP_ONLY as the negative direction of the axis and the INPUT_MOVE_DOWN_ONLY mapping as the positive
		binding for the axis.
		
		NOTE: input defined in m_Input (e.g. <Input>INPUT_MOVE_UD</Input>) is implicitly unmappable!
	-->
	<structdef type="rage::ControlInput::AxisDefinition">
		<enum name="m_Input"    type="rage::InputType" description="The input that is based/dependent upon other inputs. NOTE: Implicitly Unmappable." />
		<enum name="m_Negative" type="rage::InputType" description="The first input that the specified input is dependent on." />
		<enum name="m_Positive" type="rage::InputType" description="The second input that the specified input is dependent on." />
	</structdef>

	
	<structdef type="rage::ControlInput::InputGroupDefinition">
		<enum name="m_InputGroup" type="rage::InputGroup"                                            description="The input group being defined." />
		<array name="m_Inputs"    type="atFixedArray" size="::rage::ControlInput::INPUT_GROUP_LIMIT" description="The inputs that make up the group.">
			<enum type="rage::InputType" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::InputMapperAssignment">
		<enum  name="m_Mapper"	type="rage::Mapper" />
		<array name="m_Inputs" type="atArray">
			<enum  type="rage::InputType" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::RelatedInputs">
		<array name="m_Inputs" type="atArray">
			<enum type="rage::InputType" />
		</array>
	</structdef>
	
	<structdef type="rage::ControlInput::InputSettings">
		<array name="m_HistorySupport" type="atArray">
			<enum type="rage::InputType" />
		</array>
		<array name="m_MapperAssignements" type="atArray">
			<struct type="rage::ControlInput::InputMapperAssignment" />
		</array>
		<array name="m_RelatedInputs" type="atArray" description="Related inputs that should be disabled together.">
			<struct type="rage::ControlInput::RelatedInputs" />
		</array>
		<array name="m_AxisDefinitions" type="atArray">
			<struct type="rage::ControlInput::AxisDefinition" />
		</array>
		<array name="m_InputGroupDefinitions" type="atFixedArray" size="::rage::MAX_INPUTGROUPS">
			<struct type="rage::ControlInput::InputGroupDefinition"/>
		</array>
	</structdef>

	<!-- Only the mappings need to be savable in the final version (i.e. preserveNames="true"). -->
	<structdef type="rage::ControlInput::Mapping" preserveNames="true">
		<enum	name="m_Input"		type="rage::InputType" />
		<enum	name="m_Source"		type="rage::ioMapperSource" />
		<array	name="m_Parameters"	type="atArray">
			<enum type="rage::ioMapperParameter" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::ControlSettings" preserveNames="true">
		<array name="m_Mappings" type="atArray">
			<struct type="rage::ControlInput::Mapping" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::DeviceSettings" preserveNames="true">
		<string name="m_Description"		type="wide_member" size="50" />
		<struct name="m_ControlSettings"	type="rage::ControlInput::ControlSettings" />
	</structdef>
	
	
	<!-- ==========================================================================================
		 Dynamic Mappings
		 ==========================================================================================
		 These are used to dynamically alter mappings on the fly based on other mappings.
		 ========================================================================================== -->
	<enumdef type="rage::ControlInput::DynamicMappingConstants">
		<enumval name="DYNAMIC_MAPPINGS_AMOUNT"      value="19"/>
		<enumval name="DYNAMIC_MAPPINGS_AXIS_AMOUNT" value="2"/>
	</enumdef>
		
	<const name="rage::ControlInput::DYNAMIC_MAPPINGS_AXIS_AMOUNT" value="2"/>
		
	<structdef type="rage::ControlInput::DynamicMapping">
		<enum name="m_Input"	type="rage::InputType" />
		<array name="m_Mappings" type="atFixedArray" size="rage::ControlInput::DYNAMIC_MAPPINGS_AXIS_AMOUNT" description="The input(s) to base these mappings on.">
			<enum type="rage::InputType" />
		</array>
	</structdef>

	<const name="rage::ControlInput::DYNAMIC_MAPPINGS_AMOUNT" value="19"/>
	
	<structdef type="rage::ControlInput::DynamicMappings">
		<array name="m_Mappings" type="atFixedArray" size="rage::ControlInput::DYNAMIC_MAPPINGS_AMOUNT">
			<struct type="rage::ControlInput::DynamicMapping" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::DynamicMappingList">
		<map name="m_DynamicMappings" type="atMap" key="atHashString">
			<struct type="rage::ControlInput::DynamicMappings" />
		</map>
	</structdef>


	<!-- ==========================================================================================
		 PC Input Conflict And Mapping Screen Lists
		 ========================================================================================== -->	
	<structdef type="rage::ControlInput::InputList">
		<array name="m_Inputs" type="atArray">
			<enum type="rage::InputType" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::ConflictList">
		<array name="m_Categories" type="atArray">
			<string type="atHashWithStringNotFinal" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::MappingList">
		<string name="m_Name" type="atHashWithStringNotFinal" />
		<array name="m_Categories" type="atArray">
			<string type="atHashWithStringNotFinal" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::MouseSettings">
		<float name="m_OnFootMinMouseSensitivity"		description="The minimum on foot mouse sensitivity." />
		<float name="m_OnFootMaxMouseSensitivity"		description="The maximum on foot mouse sensitivity." />
		<float name="m_OnFootMouseSensitivityPower"		description="The power used to go from m_OnFootMinSensitivity to m_OnFootMaxSensitivity." />
		<float name="m_InVehicleMinMouseSensitivity"	description="The minimum in vehicle mouse sensitivity." />
		<float name="m_InVehicleMaxMouseSensitivity"	description="The maximum in vehicle mouse sensitivity." />
		<float name="m_InVehicleMouseSensitivityPower"	description="The power used to go from m_InVehicleMinMouseSensitivity to m_InVehicleMaxMouseSensitivity." />
	</structdef>


	<structdef type="rage::ControlInput::MappingSettings">
		<map name="m_Categories" type="atMap" key="atHashString">
			<struct type="rage::ControlInput::InputList" />
		</map>
		<array name="m_ConflictList" type="atArray">
			<struct type="rage::ControlInput::ConflictList" />
		</array>
		<array name="m_ConflictExceptions" type="atArray">
			<struct type="rage::ControlInput::InputList" />
		</array>
		<array name="m_MappingList" type="atArray">
			<struct type="rage::ControlInput::MappingList" />
		</array>
		<array name="m_UnmappableList" type="atArray">
			<string type="atHashString" />
		</array>
		
		<array name="m_IdenticalMappingLists" type="atArray" description="Identical mappings are mappings/inputs that will always be mapped to the same button presses!">
			<struct type="rage::ControlInput::InputList" />
		</array>
		<struct name="m_MouseSettings" type="rage::ControlInput::MouseSettings" />
	</structdef>
	

	<!-- ==========================================================================================
		 Joystick To Pad Representation
		 ========================================================================================== -->

	<enumdef type="rage::ControlInput::Gamepad::EnumConsts">
		<enumval name="GUID_SIZE" value="37" description="Text size fo a guid string." />
	</enumdef>
	
	<structdef type="rage::ControlInput::Gamepad::Source" preserveNames="true">
		<enum name="m_PadParameter"			type="rage::ioMapperParameter" />
		<enum name="m_JoystickParameter"	type="rage::ioMapperParameter" />
		<enum name="m_JoystickSource"		type="rage::ioMapperSource" />
	</structdef>

	<!-- Currently we only have an array of gamepad setup settings but more information can be added about a specific controller. -->
	<structdef type="rage::ControlInput::Gamepad::Definition" preserveNames="true">
		<!--
			We use an array instead of a map here as it is possible for there to be more than one definition for a source. For example,
			particularly the Default that might have an axis and buttons mapped to the triggers to try and catch as many devices as possible.
		-->
		<array name="m_Definitions" type="atArray">
			<struct type="rage::ControlInput::Gamepad::Source" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::Gamepad::DefinitionList" preserveNames="true">
		<map name="m_Devices" type="atMap" key="atFinalHashString">
			<struct type="rage::ControlInput::Gamepad::Definition" />
		</map>
	</structdef>



	<!-- ==========================================================================================
		 Steam Controller actions to input list.
		 ========================================================================================== -->
	<structdef type="rage::ControlInput::SteamController::SingleMappings">
		<string name="m_Action" type="member" size="26" />
		<array name="m_Inputs" type="atArray">
			<enum type="rage::InputType" />
		</array>
	</structdef>
	<structdef type="rage::ControlInput::SteamController::AxisMappings">
		<string name="m_Action" type="member" size="26" />
		<float name="m_Scale" init="1.0f" />
		<array name="m_XInputs" type="atArray">
			<enum type="rage::InputType" />
		</array>
		<array name="m_XLeftInputs" type="atArray">
			<enum type="rage::InputType" />
		</array>
		<array name="m_XRightInputs" type="atArray">
			<enum type="rage::InputType" />
		</array>
		<array name="m_YInputs" type="atArray">
			<enum type="rage::InputType" />
		</array>
		<array name="m_YUpInputs" type="atArray">
			<enum type="rage::InputType" />
		</array>
		<array name="m_YDownInputs" type="atArray">
			<enum type="rage::InputType" />
		</array>
	</structdef>

	<structdef type="rage::ControlInput::SteamController::Definition">
		<array name="m_Buttons" type="atArray">
			<struct type="rage::ControlInput::SteamController::SingleMappings" />
		</array>
		<array name="m_Triggers" type="atArray">
			<struct type="rage::ControlInput::SteamController::SingleMappings" />
		</array>
		<array name="m_Axis" type="atArray">
			<struct type="rage::ControlInput::SteamController::AxisMappings" />
		</array>
		<string name="m_LoadSet" type="member" size="26" />
	</structdef>
	
	<!-- ==========================================================================================
		 Keyboard Layout Definition
		 ========================================================================================== -->
	<enumdef type="rage::ControlInput::Keyboard::Constants">
		<enumval name="KEY_INFO_TEXT_LENGTH" value="10" description="3 chars (supporting 3 bytes per char for UTF-8) + NULL."/>
		<enumval name="MAX_NUM_KEYS"		 value="255"/>
	</enumdef>
	<const name="rage::ControlInput::Keyboard::KEY_INFO_TEXT_LENGTH" value="10"/>
	<const name="rage::ControlInput::Keyboard::MAX_NUM_KEYS"		 value="255"/>
	
	<structdef type="rage::ControlInput::Keyboard::KeyInfo" preserveNames="true">
		<enum name="m_Key" type ="rage::ioMapperParameter" />
		<u32 name="m_Icon" init="0" description="The icon number to show as button prompts. An icon of '0' means show a blank icon and overlay 'm_Text' over it."/>
		<string type="member" name="m_Text" size="rage::ControlInput::Keyboard::KEY_INFO_TEXT_LENGTH" description="The text to be shown on a blank icon. Size includes NULL terminator."/>
	</structdef>
	
	<structdef type="rage::ControlInput::Keyboard::Layout" preserveNames="true">
		<array type="atFixedArray" size="rage::ControlInput::Keyboard::MAX_NUM_KEYS" name="m_Keys">
			<struct type="rage::ControlInput::Keyboard::KeyInfo" />
		</array>
	</structdef>
		
</ParserSchema>
