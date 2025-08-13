<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
  generate="class">

  <structdef type="CMiniMap_SonarTunables" simple="true">
    <float name="fSoundRange_BarelyAudible"		 init="1.0f"	min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_MostlyAudible"		 init="1.8f"	min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_ClearlyAudible"	 init="3.1f"	min="0.0f" max="1000.0f" step="0.1f" />

    <float name="fSoundRange_Whisper"					 init="10.0f"	min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_Talking"					 init="15.0f"	min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_Shouting"				 init="30.0f"	min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_Megaphone"				 init="35.0f"	min="0.0f" max="1000.0f" step="0.1f" />

    <float name="fSoundRange_FootstepBase"		 init="20.0f"	min="0.0f" max="1000.0f" step="0.1f" description="Base footstep range on materials (as opposed to foliage). Multiplied by stealth state and the material loudness."/>
    <float name="fSoundRange_HeavyFootstep"		 init="25.0f"	min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_FootstepFoliage"  init="10.0f"	min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_LandFromFall"		 init="17.5"	min="0.0f" max="1000.0f" step="0.1f" />

    <float name="fSoundRange_WeaponSpinUp"		 init="10.0f"	 min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_Gunshot"					 init="30.0f"	 min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_SilencedGunshot"	 init="5.0f"	 min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_ProjectileLanding" init="5.0f"   min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_Explosion"				 init="40.0f"	 min="0.0f" max="1000.0f" step="0.1f" />

    <float name="fSoundRange_ObjectCollision"  init="15.0f" min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_GlassBreak"			 init="25.0f" min="0.0f" max="1000.0f" step="0.1f" />

    <float name="fSoundRange_CarHorn"					 init="15.0f" min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_CarLowSpeed"			 init="10.0f" min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_CarHighSpeed"		 init="30.0f" min="0.0f" max="1000.0f" step="0.1f" />

    <float name="fSoundRange_WaterSplashSmall" init="10.0f" min="0.0f" max="1000.0f" step="0.1f" />
    <float name="fSoundRange_WaterSplashLarge" init="20.0f" min="0.0f" max="1000.0f" step="0.1f" />

    <float name="fMinListenerRangeToDrawSonarBlips"	init="50.0f" min="0.0f" max="1000.0f" step="0.1f" description="If other peds are within this range of the player, draw the sonar blips."/>

    <float name="fRainSnowSoundReductionAmount" init="3.0f"	 min="0.0f" max="10.0f"   step="0.1f" />
    <float name="fRadioSoundReductionAmount"		init="3.0f"	 min="0.0f" max="10.0f"   step="0.1f" />
    <float name="fRadioSoundReductionDistance"  init="3.0f"	 min="0.0f" max="10.0f"   step="0.1f" />
    <float name="fConversationSoundReductionAmount" init="3.0f"	 min="0.0f" max="10.f" step="0.1f" />
    <float name="fConversationSoundReductionDistance" init="3.0f"	 min="0.0f" max="10.f" step="0.1f" />
  </structdef>

  <structdef type="CMiniMap_HealthBarTunables" simple="true">
    <float name="fStaminaDepletionBlinkPercentage" init="0.1f" min="0.0f" max="1.0f" step="0.05f"/>
    <int name="iHealthDepletionBlinkPercentage" init="25" min="0" max="100" step="5"/>
  </structdef>

  <enumdef type="MM_BITMAP_VERSION">
    <enumval name="MM_BITMAP_VERSION_SEA" description="Draws minimap_sea bitmaps"/>
    <enumval name="MM_BITMAP_VERSION_MINIMAP" description="Draws minimap bitmaps"/>
    <enumval name="MM_BITMAP_VERSION_ALPHA" description="Draws minimap_alpha bitmaps"/>
  </enumdef>
  
  <structdef type="CMiniMap_BitmapTunables" simple="true">
    <u16 name="iBitmapTilesX"       init="2"  min="1" max="100" step="1" />
    <u16 name="iBitmapTilesY"       init="3"  min="1" max="100" step="1" />
    <Vector2 name="vBitmapTileSize" init="4500.0, 4500.0" min="0.0" max="100000.0" step="0.1" />
    <Vector2 name="vBitmapStart"    init="-4140.0, 8400"  min="-100000.0" max="100000.0" step="0.1" />
    <bool name="bAlwaysDrawBitmap"  init="false"/>
    <enum name="eBitmapForPause"    init="MM_BITMAP_VERSION_SEA" type="MM_BITMAP_VERSION"/>
    <enum name="eBitmapForMinimap"  init="MM_BITMAP_VERSION_MINIMAP" type="MM_BITMAP_VERSION"/>
  </structdef>

  <structdef type="CMiniMap_OverlayTunables" simple="true">
    <Vector2 name="vPos"            init="0, 0"  min="-100000.0" max="100000.0" step="0.1" />
    <Vector2 name="vScale"          init="100, 100" min="0.0" max="500.0" step="0.01" />
    <float name="fDisplayZ"         init="33.0"  min="-100000.0" max="100000.0" step="0.1" description="Player Z value at which to display the overlay"/>
  </structdef>

  <structdef type="CMiniMap_FogOfWarTunables" simple="true">
    <float name="fWorldX"           init="-3447" min="-100000" max="100000" step="1" />
    <float name="fWorldY"           init="-7722" min="-100000" max="100000" step="1" />
    <float name="fWorldW"           init="7647" min="-100000" max="100000" step="1" />
    <float name="fWorldH"           init="11368" min="-100000" max="100000" step="1" />
    <float name="fBaseAlpha"      init="0.0" min="0.0" max="1.0" step="0.01" />
    <float name="fTopAlpha"       init="1.0" min="0.0" max="1.0" step="0.01" />
    <float name="fFowWaterHeight" init="50" min="-100000.0" max="100000.0" step="0.1" />
  </structdef>

  <structdef type="CMiniMap_CameraTunables" simple="true">
    <float name="fPauseMenuTilt"                  init="80.0" min="0.0"  max="200.0" step="1.0"/>
    <float name="fBitmapRequiredZoom"             init="80.0" min="18.0" max="600.0" step="1.0" description="zoom level at which we need to display the bitmap"/>
    <float name="fExteriorFootZoom"               init="83.0" min="18.0" max="600.0" step="1.0"/>
    <float name="fExteriorFootZoomRunning"        init="90.0" min="18.0" max="600.0" step="1.0"/>
    <float name="fExteriorFootZoomWanted"         init="60.0" min="18.0" max="600.0" step="1.0"/>
    <float name="fExteriorFootZoomWantedRunning"  init="45.0" min="18.0" max="600.0" step="1.0"/>
    <float name="fExteriorFootTilt"               init="0.0"  min="0.0"  max="200.0" step="1.0"/>
    <float name="fExteriorFootOffset"             init="30.0" min="0.0"  max="100.0" step="0.1"/>
    <float name="fInteriorFootZoom"               init="500.0" min="18.0" max="600.0" step="1.0"/>
    <float name="fInteriorVerySmall"              init="800.0" min="18.0" max="800.0" step="1.0"/>
    <float name="fInteriorVeryLarge"              init="200.0" min="18.0" max="800.0" step="1.0"/>
    <float name="fInteriorFootTilt"               init="0.0"  min="0.0"  max="200.0" step="1.0"/>
    <float name="fInteriorFootOffset"             init="30.0" min="0.0"  max="100.0" step="0.1"/>
    <float name="fParachutingZoom"                init="72.0" min="18.0" max="600.0" step="1.0"/>
    <float name="fVehicleStaticZoom"              init="96.0" min="18.0" max="600.0" step="1.0"/>
    <float name="fVehicleStaticWantedZoom"        init="66.0" min="18.0" max="600.0" step="1.0"/>
    <float name="fVehicleMovingZoom"              init="48.0" min="18.0" max="600.0" step="1.0"/>
    <float name="fVehicleMovingWantedZoom"        init="48.0" min="18.0" max="600.0" step="1.0"/>
    <float name="fVehicleSpeedZoomScalar"         init="0.8"  min="0.0" max="10.0" step="0.01"/>
    <float name="fVehicleTilt"                    init="60.0" min="0.0"  max="200.0" step="1.0"/>
    <float name="fVehicleOffset"                  init="30.0" min="0.0"  max="100.0" step="0.1"/>
    <float name="fAltimeterTilt"                  init="60.0" min="0.0"  max="200.0" step="1.0"/>
    <float name="fAltimeterOffset"                init="15.0" min="0.0"  max="100.0" step="0.1"/>
    <float name="fBigmapTilt"                     init="0.0"  min="0.0"  max="200.0" step="1.0"/>
    <float name="fBigmapOffset"                   init="0.0"  min="0.0"  max="100.0" step="0.1"/>
    <float name="fRangeZoomedScalarStandard"      init="2.1"  min="0.0" max="10.0" step="0.1" description="Amount to zoom out when dpad down is pressed"/>
    <float name="fRangeZoomedScalarPlane"         init="4.2"  min="0.0" max="10.0" step="0.1" description="Amount to zoom out when dpad down is pressed"/>
  </structdef>

  <structdef type="CMiniMap_TileTunables" simple="true">
    <Vector2 name="vMiniMapWorldSize" init="9400.0, 12492.0" min="-100000" max="100000" step="0.1"/>
    <Vector2 name="vMiniMapWorldStart" init="-4500.0, 8000.0" min="-100000" max="100000" step="0.1"/>
    <bool name="bDrawVectorSeaPaused" init="false"/>
    <bool name="bDrawVectorSeaMinimap" init="true"/>
  </structdef>

	<structdef type="CMiniMap_DisplayTunables" simple="true">
		<u16 name="ScriptOverlayTime" init="300"/>
		<u16 name="WantedOverlayTime" init="500"/>
		<float name="MaskNormalAlpha" init=".70f" min="0.0f" max="1.0f" step="0.05f"/>
		<float name="MaskFlashAlpha" init="1.0f" min="0.0f" max="1.0f" step="0.05f"/>
	</structdef>
</ParserSchema>