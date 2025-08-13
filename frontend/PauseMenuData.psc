<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema" generate="class">


	<enumdef type="eMenuOption">
		<enumval name="MENU_OPTION_SLIDER"/>
		<enumval name="MENU_OPTION_DISPLAY_ON_OFF"/>
		<enumval name="MENU_OPTION_DISPLAY_INVERT_LOOK"/>
		<enumval name="MENU_OPTION_DISPLAY_TARGET_CONFIG"/>
		<enumval name="MENU_OPTION_DISPLAY_CONTROL_CONFIG"/>
		<enumval name="MENU_OPTION_CONTROLLER_SENSITIVITY"/>
		<enumval name="MENU_OPTION_DISPLAY_GPS_SPEECH"/>
		<enumval name="MENU_OPTION_DISPLAY_SPEAKER_OUTPUT"/>
		<enumval name="MENU_OPTION_DISPLAY_SWHEADSET"/>
		<enumval name="MENU_OPTION_DISPLAY_RADIO_STATIONS"/>
		<enumval name="MENU_OPTION_DISPLAY_VOICE_OUTPUT"/>
    <enumval name="MENU_OPTION_DISPLAY_UR_PLAYMODE"/>
    <enumval name="MENU_OPTION_DISPLAY_AUDIO_MUTE_ON_FOCUS_LOSS"/>
    <enumval name="MENU_OPTION_DISPLAY_MINIMAP_MODE"/>
		<enumval name="MENU_OPTION_DISPLAY_RETICLE_MODE"/>
		<enumval name="MENU_OPTION_DISPLAY_LANGUAGE"/>
		<enumval name="MENU_OPTION_CAMERA_HEIGHT"/>
		<enumval name="MENU_OPTION_DISPLAY_SS_FRONTREAR"/>
		<enumval name="MENU_OPTION_CAMERA_ZOOM"/>
		<enumval name="MENU_OPTION_CAMERA_ZOOM_NO_FP"/>
		<enumval name="MENU_OPTION_GAME_FLOW"/>
		<enumval name="MENU_OPTION_GFX_QUALITY"/>
		<enumval name="MENU_OPTION_GFX_SHADERQ"/>
		<enumval name="MENU_OPTION_GFX_SHADOWS"/>
		<enumval name="MENU_OPTION_GFX_SOFTNESS"/>
		<enumval name="MENU_OPTION_GFX_WATER"/>
		<enumval name="MENU_OPTION_VID_VSYNC"/>
		<enumval name="MENU_OPTION_VID_ASPECT"/>
		<enumval name="MENU_OPTION_GFX_MSAA"/>
		<enumval name="MENU_OPTION_GFX_ANISOT"/>
		<enumval name="MENU_OPTION_GFX_AMBIENTOCC"/>
		<enumval name="MENU_OPTION_GFX_DXVERSION"/>
		<enumval name="MENU_OPTION_GFX_NIGHTLIGHTS"/>
    <enumval name="MENU_OPTION_ROLL_YAW_PITCH"/>
		<enumval name="MENU_OPTION_MEMORY_GRAPH"/>
		<enumval name="MENU_OPTION_SCREEN_TYPE"/>
		<enumval name="MENU_OPTION_DISPLAY_VOICE_TYPE"/>
		<enumval name="MENU_OPTION_VOICE_FEEDBACK"/>
		<enumval name="MENU_OPTION_MOUSE_TYPE"/>
   	<enumval name="MENU_OPTION_REPLAY_MODE"/>
   	<enumval name="MENU_OPTION_REPLAY_MEM_LIMIT"/>
    <enumval name="MENU_OPTION_REPLAY_AUTO_RESUME_RECORDING"/>
    <enumval name="MENU_OPTION_REPLAY_AUTO_SAVE_RECORDING"/>
    <enumval name="MENU_OPTION_VIDEO_UPLOAD_PRIVACY"/>
    <enumval name="MENU_OPTION_CONTROLS_CONTEXT"/>
    <enumval name="MENU_OPTION_DISPLAY_CONTROL_DRIVEBY"/>
    <enumval name="MENU_OPTION_FEED_DELAY"/>
    <enumval name="MENU_OPTION_GFX_REFLECTION"/>
    <enumval name="MENU_OPTION_CAM_VEH_OFF"/>
    <enumval name="MENU_OPTION_DISPLAY_MEASUREMENT"/>
    <enumval name="MENU_OPTION_FPS_DEFAULT_AIM_TYPE"/>
  </enumdef>

	<enumdef type="eDepth">
		<enumval name="MENU_DEPTH_HEADER"/>
		<enumval name="MENU_DEPTH_1ST"/>
		<enumval name="MENU_DEPTH_2ND"/>
		<enumval name="MENU_DEPTH_3RD"/>
		<enumval name="MENU_DEPTH_4TH"/>
	</enumdef>

	<enumdef type="eMenuAction">
		<enumval name="MENU_OPTION_ACTION_NONE"/>
		<enumval name="MENU_OPTION_ACTION_LINK"/>
		<enumval name="MENU_OPTION_ACTION_PREF_CHANGE"/>
		<enumval name="MENU_OPTION_ACTION_TRIGGER"/>
		<enumval name="MENU_OPTION_ACTION_FILL_CONTENT"/>
		<enumval name="MENU_OPTION_ACTION_FILL_CONTENT_FROM_SCRIPT"/>
		<enumval name="MENU_OPTION_ACTION_INCEPT"/>
		<enumval name="MENU_OPTION_ACTION_REFERENCE"/>
		<enumval name="MENU_OPTION_ACTION_SEPARATOR"/>
	</enumdef>

	<enumdef type="eMenuItemBits">
		<enumval name="InitiallyDisabled"/>
	</enumdef>

	<enumdef type="eMenuScreenBits">
		<enumval name="NotDimmable"/>
		<enumval name="Input_NoAdvance"/>
		<enumval name="Input_NoBack"/>
		<enumval name="Input_NoBackIfNavigating"/>
		<enumval name="Input_NoTabChange"/>
		<enumval name="Sound_NoAccept"/>
		<enumval name="AlwaysUseButtons"/>
		<enumval name="SF_NoClearRootColumns"/>
		<enumval name="SF_NoChangeLayout"/>
		<enumval name="SF_NoMenuAdvance"/>
		<enumval name="SF_CallImmediately"/>
		<enumval name="NeverGenerateMenuData"/>
		<enumval name="LayoutChangedOnBack"/>
		<enumval name="HandlesDisplayDataSlot"/>
    <enumval name="EnterMenuOnMouseClick"/>
	</enumdef>

	<enumdef type="eMenuScrollBits">
		<enumval name="Width_1"/>
		<enumval name="Width_2"/>
		<enumval name="Width_3"/>
		<enumval name="InitiallyVisible"/>
		<enumval name="InitiallyInvisible"/>
		<enumval name="ManualUpdate"/>
		<enumval name="ShowIfNotFull"/>
		<enumval name="ShowAsPages"/>
		<enumval name="Arrows_LeftRight"/>
		<enumval name="Arrows_UpDown"/>
		<enumval name="Align_Left"/>
		<enumval name="Align_Right"/>
		<enumval name="Offset_2Left"/>
		<enumval name="Offset_1Left"/>
		<enumval name="Offset_1Right"/>
		<enumval name="Offset_2Right"/>
	</enumdef>

	<enumdef type="eMenuVersionBits">
		<enumval name="kRenderMenusInitially"/>
		<enumval name="kNoBackground"/>
		<enumval name="kFadesIn"/>
		<enumval name="kGeneratesMenuData"/>
		<enumval name="kNoPlayerInfo"/>
		<enumval name="kNoHeaderText"/>
		<enumval name="kMenuSectionJump"/>
		<enumval name="kNoSwitchDelay"/>
		<enumval name="kNoTabChangeWhileLocked"/>
		<enumval name="kNoTabChange"/>
		<enumval name="kAllowBackingOut"/>
		<enumval name="kAllowStartButton"/>
		<enumval name="kNotDimmable"/>
		<enumval name="kAllHighlighted"/>
		<enumval name="kHideHeaders"/>
		<enumval name="kTabsAreColumns"/>
		<enumval name="kForceButtonRender"/>
		<enumval name="kForceProcessInput"/>
		<enumval name="kAutoShiftDepth"/>
		<enumval name="kNoTimeWarp"/>
		<enumval name="kCanHide"/>
		<enumval name="kUseAlternateContentPos"/>
		<enumval name="kNoPeds"/>
		<enumval name="kAllowBackingOutInMenu"/>
		<enumval name="kDelayAudioUntilUnsuppressed"/>
	</enumdef>

	<enumdef type="eMenuScreen" preserveNames="true">
<!-- EVERYTHING BELOW IS AUTOMATICALLY GENERATED BY REBUILDALLENUMS! EDIT AT YOUR OWN RISK -->
		<enumval name="MENU_UNIQUE_ID_INVALID" value="-1"/>
		<enumval name="MENU_UNIQUE_ID_MAP" value="0"/>
		<enumval name="MENU_UNIQUE_ID_START" value="0"/>
		<enumval name="MENU_UNIQUE_ID_INFO"/>
		<enumval name="MENU_UNIQUE_ID_FRIENDS"/>
		<enumval name="MENU_UNIQUE_ID_GALLERY"/>
		<enumval name="MENU_UNIQUE_ID_SOCIALCLUB"/>
		<enumval name="MENU_UNIQUE_ID_GAME"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS"/>
		<enumval name="MENU_UNIQUE_ID_PLAYERS"/>
		<enumval name="MENU_UNIQUE_ID_WEAPONS"/>
		<enumval name="MENU_UNIQUE_ID_MEDALS"/>
		<enumval name="MENU_UNIQUE_ID_STATS"/>
		<enumval name="MENU_UNIQUE_ID_AVAILABLE"/>
		<enumval name="MENU_UNIQUE_ID_VAGOS"/>
		<enumval name="MENU_UNIQUE_ID_COPS"/>
		<enumval name="MENU_UNIQUE_ID_LOST"/>
		<enumval name="MENU_UNIQUE_ID_HOME_MISSION"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_SETTINGS"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_INVITE"/>
		<enumval name="MENU_UNIQUE_ID_STORE"/>
		<enumval name="MENU_UNIQUE_ID_HOME_HELP"/>
		<enumval name="MENU_UNIQUE_ID_HOME_BRIEF"/>
		<enumval name="MENU_UNIQUE_ID_HOME_FEED"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_AUDIO"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_DISPLAY"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_CONTROLS"/>
		<enumval name="MENU_UNIQUE_ID_NEW_GAME"/>
		<enumval name="MENU_UNIQUE_ID_LOAD_GAME"/>
		<enumval name="MENU_UNIQUE_ID_SAVE_GAME"/>
		<enumval name="MENU_UNIQUE_ID_HEADER"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_SAVE_GAME"/>
		<enumval name="MENU_UNIQUE_ID_HOME"/>
		<enumval name="MENU_UNIQUE_ID_CREWS"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_SAVEGAME"/>
		<enumval name="MENU_UNIQUE_ID_GALLERY_ITEM"/>
		<enumval name="MENU_UNIQUE_ID_FREEMODE"/>
		<enumval name="MENU_UNIQUE_ID_MP_CHAR_1"/>
		<enumval name="MENU_UNIQUE_ID_MP_CHAR_2"/>
		<enumval name="MENU_UNIQUE_ID_MP_CHAR_3"/>
		<enumval name="MENU_UNIQUE_ID_MP_CHAR_4"/>
		<enumval name="MENU_UNIQUE_ID_MP_CHAR_5"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_MULTIPLAYER"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_MY_MP"/>
		<enumval name="MENU_UNIQUE_ID_MISSION_CREATOR"/>
		<enumval name="MENU_UNIQUE_ID_GAME_MP"/>
		<enumval name="MENU_UNIQUE_ID_LEAVE_GAME"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_PRE_LOBBY"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_LOBBY"/>
		<enumval name="MENU_UNIQUE_ID_PARTY"/>
		<enumval name="MENU_UNIQUE_ID_LOBBY"/>
		<enumval name="MENU_UNIQUE_ID_PLACEHOLDER"/>
		<enumval name="MENU_UNIQUE_ID_STATS_CATEGORY"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_LIST"/>
		<enumval name="MENU_UNIQUE_ID_SAVE_GAME_LIST"/>
		<enumval name="MENU_UNIQUE_ID_MAP_LEGEND"/>
		<enumval name="MENU_UNIQUE_ID_CREWS_CATEGORY"/>
		<enumval name="MENU_UNIQUE_ID_CREWS_FILTER"/>
		<enumval name="MENU_UNIQUE_ID_CREWS_CARD"/>
		<enumval name="MENU_UNIQUE_ID_SPECTATOR"/>
		<enumval name="MENU_UNIQUE_ID_STATS_LISTITEM"/>
<!-- EVERYTHING HERE IS AUTOMATICALLY GENERATED! EDIT AT YOUR OWN RISK -->
		<enumval name="MENU_UNIQUE_ID_CREW_MINE"/>
		<enumval name="MENU_UNIQUE_ID_CREW_ROCKSTAR"/>
		<enumval name="MENU_UNIQUE_ID_CREW_FRIENDS"/>
		<enumval name="MENU_UNIQUE_ID_CREW_INVITES"/>
		<enumval name="MENU_UNIQUE_ID_CREW_LIST"/>
		<enumval name="MENU_UNIQUE_ID_MISSION_CREATOR_CATEGORY"/>
		<enumval name="MENU_UNIQUE_ID_MISSION_CREATOR_LISTITEM"/>
		<enumval name="MENU_UNIQUE_ID_MISSION_CREATOR_STAT"/>
		<enumval name="MENU_UNIQUE_ID_FRIENDS_LIST"/>
		<enumval name="MENU_UNIQUE_ID_FRIENDS_OPTIONS"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_MP_CHARACTER_SELECT"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_MP_CHARACTER_CREATION"/>
		<enumval name="MENU_UNIQUE_ID_CREATION_HERITAGE"/>
		<enumval name="MENU_UNIQUE_ID_CREATION_LIFESTYLE"/>
		<enumval name="MENU_UNIQUE_ID_CREATION_YOU"/>
		<enumval name="MENU_UNIQUE_ID_PARTY_LIST"/>
		<enumval name="MENU_UNIQUE_ID_REPLAY_MISSION"/>
		<enumval name="MENU_UNIQUE_ID_REPLAY_MISSION_LIST"/>
		<enumval name="MENU_UNIQUE_ID_REPLAY_MISSION_ACTIVITY"/>
		<enumval name="MENU_UNIQUE_ID_CREW"/>
<!-- EVERYTHING HERE IS AUTOMATICALLY GENERATED! EDIT AT YOUR OWN RISK -->
		<enumval name="MENU_UNIQUE_ID_CREATION_HERITAGE_LIST"/>
		<enumval name="MENU_UNIQUE_ID_CREATION_LIFESTYLE_LIST"/>
		<enumval name="MENU_UNIQUE_ID_PLAYERS_LIST"/>
		<enumval name="MENU_UNIQUE_ID_PLAYERS_OPTIONS"/>
		<enumval name="MENU_UNIQUE_ID_PLAYERS_OPTIONS_LIST"/>
		<enumval name="MENU_UNIQUE_ID_PARTY_OPTIONS"/>
		<enumval name="MENU_UNIQUE_ID_PARTY_OPTIONS_LIST"/>
		<enumval name="MENU_UNIQUE_ID_CREW_OPTIONS"/>
		<enumval name="MENU_UNIQUE_ID_CREW_OPTIONS_LIST"/>
		<enumval name="MENU_UNIQUE_ID_FRIENDS_OPTIONS_LIST"/>
		<enumval name="MENU_UNIQUE_ID_FRIENDS_MP"/>
		<enumval name="MENU_UNIQUE_ID_TEAM_SELECT"/>
		<enumval name="MENU_UNIQUE_ID_HOME_DIALOG"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_EMPTY"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_FEED"/>
		<enumval name="MENU_UNIQUE_ID_GALLERY_OPTIONS"/>
		<enumval name="MENU_UNIQUE_ID_GALLERY_OPTIONS_LIST"/>
		<enumval name="MENU_UNIQUE_ID_BRIGHTNESS_CALIBRATION"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_TEXT_SELECTION"/>
		<enumval name="MENU_UNIQUE_ID_LOBBY_LIST"/>
<!-- EVERYTHING HERE IS AUTOMATICALLY GENERATED! EDIT AT YOUR OWN RISK -->
		<enumval name="MENU_UNIQUE_ID_LOBBY_LIST_ITEM"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_CORONA"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_CORONA_LOBBY"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_CORONA_JOINED_PLAYERS"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_CORONA_INVITE_PLAYERS"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_CORONA_INVITE_FRIENDS"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_CORONA_INVITE_CREWS"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_JOINED_PLAYERS"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_INVITE_PLAYERS"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_INVITE_FRIENDS"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_INVITE_CREWS"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_FACEBOOK"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_JOINING_SCREEN"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_SETTINGS_LIST"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_DETAILS_LIST"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_INVITE_LIST"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_JOINED_LIST"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_CORONA_INVITE_MATCHED_PLAYERS"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_CORONA_INVITE_LAST_JOB_PLAYERS"/>
		<enumval name="MENU_UNIQUE_ID_CORONA_INVITE_MATCHED_PLAYERS"/>
<!-- EVERYTHING HERE IS AUTOMATICALLY GENERATED! EDIT AT YOUR OWN RISK -->
		<enumval name="MENU_UNIQUE_ID_CORONA_INVITE_LAST_JOB_PLAYERS"/>
		<enumval name="MENU_UNIQUE_ID_CREW_LEADERBOARDS"/>
		<enumval name="MENU_UNIQUE_ID_HOME_OPEN_JOBS"/>
		<enumval name="MENU_UNIQUE_ID_CREW_REQUEST"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_RACE"/>
		<enumval name="MENU_UNIQUE_ID_RACE_INFO"/>
		<enumval name="MENU_UNIQUE_ID_RACE_INFOLIST"/>
		<enumval name="MENU_UNIQUE_ID_RACE_LOBBYLIST"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_BETTING"/>
		<enumval name="MENU_UNIQUE_ID_BETTING"/>
		<enumval name="MENU_UNIQUE_ID_BETTING_INFOLIST"/>
		<enumval name="MENU_UNIQUE_ID_BETTING_LOBBYLIST"/>
		<enumval name="MENU_UNIQUE_ID_INCEPT_TRIGGER"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_SIXAXIS"/>
		<enumval name="MENU_UNIQUE_ID_REPLAY_RANDOM"/>
		<enumval name="MENU_UNIQUE_ID_CUTSCENE_EMPTY"/>
		<enumval name="MENU_UNIQUE_ID_HOME_NEWSWIRE"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_CAMERA"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_GRAPHICS"/>
    <enumval name="MENU_UNIQUE_ID_SETTINGS_ADVANCED_GFX"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_VOICE_CHAT"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_MISC_CONTROLS"/>
		<enumval name="MENU_UNIQUE_ID_HELP"/>
		<enumval name="MENU_UNIQUE_ID_MOVIE_EDITOR"/>
		<enumval name="MENU_UNIQUE_ID_EXIT_TO_WINDOWS"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_LANDING_PAGE"/>
		<enumval name="MENU_UNIQUE_ID_SHOW_ACCOUNT_PICKER"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_REPLAY"/>
		<enumval name="MENU_UNIQUE_ID_REPLAY_EDITOR"/>
		<enumval name="MENU_UNIQUE_ID_KEYMAP"/>
		<enumval name="MENU_UNIQUE_ID_KEYMAP_LIST"/>
		<enumval name="MENU_UNIQUE_ID_KEYMAP_LISTITEM"/>
		<enumval name="MENU_UNIQUE_ID_SETTINGS_FIRST_PERSON"/>
		<enumval name="MENU_UNIQUE_ID_HEADER_LANDING_KEYMAPPING"/>
    <enumval name="MENU_UNIQUE_ID_PROCESS_SAVEGAME"/>
    <enumval name="MENU_UNIQUE_ID_PROCESS_SAVEGAME_LIST"/>
		<enumval name="MENU_UNIQUE_ID_IMPORT_SAVEGAME"/>
		<enumval name="MENU_UNIQUE_ID_CREDITS"/>
		<enumval name="MENU_UNIQUE_ID_LEGAL"/>
		<enumval name="MENU_UNIQUE_ID_CREDITS_LEGAL"/>
	</enumdef>

	<enumdef type="eInstructionButtons">
		<enumval name="ARROW_UP"/>
		<enumval name="ARROW_DOWN"/>
		<enumval name="ARROW_LEFT"/>
		<enumval name="ARROW_RIGHT"/>

		<enumval name="DPAD_UP"/>
		<enumval name="DPAD_DOWN"/>
		<enumval name="DPAD_LEFT"/>
		<enumval name="DPAD_RIGHT"/>
		<enumval name="DPAD_NONE"/>
		<enumval name="DPAD_ALL"/>
		<enumval name="DPAD_UPDOWN"/>
		<enumval name="DPAD_LEFTRIGHT"/>

		<enumval name="LSTICK_UP"/>
		<enumval name="LSTICK_DOWN"/>
		<enumval name="LSTICK_LEFT"/>
		<enumval name="LSTICK_RIGHT"/>
		<enumval name="LSTICK_CLICK"/>
		<enumval name="LSTICK_ALL"/>
		<enumval name="LSTICK_UPDOWN"/>
		<enumval name="LSTICK_LEFTRIGHT"/>
		<enumval name="LSTICK_ROTATE"/>

		<enumval name="RSTICK_UP"/>
		<enumval name="RSTICK_DOWN"/>
		<enumval name="RSTICK_LEFT"/>
		<enumval name="RSTICK_RIGHT"/>
		<enumval name="RSTICK_CLICK"/>
		<enumval name="RSTICK_ALL"/>
		<enumval name="RSTICK_UPDOWN"/>
		<enumval name="RSTICK_LEFTRIGHT"/>
		<enumval name="RSTICK_ROTATE"/>

		<enumval name="BUTTON_A"/>
		<enumval name="BUTTON_B"/>
		<enumval name="BUTTON_X"/>
		<enumval name="BUTTON_Y"/>

		<enumval name="BUTTON_L1"/>
		<enumval name="BUTTON_L2"/>
		<enumval name="BUTTON_R1"/>
		<enumval name="BUTTON_R2"/>

		<enumval name="BUTTON_START"/>
		<enumval name="BUTTON_BACK"/>

		<enumval name="SIXAXIS_YAW"/>
		<enumval name="SIXAXIS_PITCH"/>
		<enumval name="SIXAXIS_PITCH_UP"/>
		<enumval name="SIXAXIS_ROLL"/>
		
		<enumval name="ICON_SPINNER"/>

		<enumval name="ARROW_UPDOWN"	/>
		<enumval name="ARROW_LEFTRIGHT" />
		<enumval name="ARROW_ALL"		/>

		<enumval name="BUTTONS_TRIGGERS"/>
		<enumval name="BUTTONS_BUMPERS"	/>
		<enumval name="STICK_CLICKS"/>

		<enumval name="NOTHING"/>
		<enumval name="MAX_INSTRUCTIONAL_BUTTONS" />
		<enumval name="ICON_INVALID" value="9999"/>
	</enumdef>

	<structdef type="CMenuDisplayOption" generate="class" simple="true">
		<string name="cTextId" type="atHashWithStringDev"/>
	</structdef>

	<structdef type="CMenuDisplayValue" generate="class" simple="true">
		<enum name="MenuOption" type="eMenuOption"/>

		<array name="MenuDisplayOptions" type="atArray">
			<struct type="CMenuDisplayOption"/>
		</array>
	</structdef>

	<structdef type="SGeneralMovieData" generate="class">
		<Vector2 name="vPos" min="0.0f" max="1.0f" init="0.0f,0.0f"/>
		<Vector2 name="vSize" min="0.0f" max="1.0f" init="1.0f,1.0f"/>
	</structdef>

	<structdef type="SGeneralPauseDataConfig" generate="class" base="SGeneralMovieData">
		<array name="RequiredMovies" type="atFixedArray" size="4">
			<string name="movie" type="atString"/>
		</array>
		<bool name="bRequiresMovieView" init="true"/>
		<bool name="bDependsOnSharedComponents" init="false"/>
		<u8 name="LoadingOrder" init="255"/>
    <string name="HAlign" type="member" size="2" init="C" description="Horizontal Alignment. Can Be (S)tretch, (L)eft, (C)enter, (R)ight"/>
		
	</structdef>

	<structdef type="CGeneralPauseData" generate="class" simple="true" version="2">
		<map name="MovieSettings" type="atBinaryMap" key="atHashWithStringBank">
			<struct type="SGeneralPauseDataConfig"/>
		</map>
	</structdef>

	<structdef type="CSpinnerData" generate="class" version="3" base="SGeneralMovieData">
		<array name="Offsets" type="atArray">
			<Vector2 min="-1.0f" max="1.0f"/>
		</array>
	</structdef>

	<structdef type="CArrowData" generate="class" version="1" base="SGeneralMovieData">
		<Vector2 name="vPosRight" min="0.0f" max="1.0f" init="0.0f,0.0f"/>
		<u8 name="shownAlpha" init="255"/>
		<u8 name="hiddenAlpha" init="128"/>
	</structdef>

	<structdef type="CGradient" generate="class" version="1" base="SGeneralMovieData">
		<u8 name="minAlpha" init="40"/>
		<u8 name="maxAlpha" init="255"/>
		<u8 name="flatAlpha" init="140"/>
	</structdef>

	<structdef type="CPauseMenuPersistentData" generate="class" simple="true" version="2">
		<struct type="CSpinnerData" name="Spinner"/>
		<struct type="CGradient" name="Gradient"/>
		<struct type="CArrowData" name="Arrows"/>
	</structdef>

	<!-- Dynamic menu creation-->
	<structdef type="CPauseMenuDynamicData" generate="class" simple="true" version="1">
		<string name="type" type="atHashWithStringDev"/>
		<map name="params" type="atBinaryMap" key="atHashWithStringDev">
			<string name="value" type="atString"/>
		</map>
	</structdef>

</ParserSchema>
