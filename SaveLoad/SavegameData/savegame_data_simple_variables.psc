<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="eWantedLevel" preserveNames="true">
    <enumval name="WANTED_CLEAN"/>
    <enumval name="WANTED_LEVEL1"/>
    <enumval name="WANTED_LEVEL2"/>
    <enumval name="WANTED_LEVEL3"/>
    <enumval name="WANTED_LEVEL4"/>
    <enumval name="WANTED_LEVEL5"/>
    <enumval name="WANTED_LEVEL_LAST"/>
  </enumdef>

  <structdef type="CSimpleVariablesSaveStructure" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <bool name="m_bClosestSaveHouseDataIsValid"/>
    <float name="m_fHeadingOfClosestSaveHouse"/>
    <Vector3 name="m_vCoordsOfClosestSaveHouse"/>
    <bool name="m_bFadeInAfterLoad"/>
    <bool name="m_bPlayerShouldSnapToGroundOnSpawn" init="true"/>

    <u32 name="m_MillisecondsPerGameMinute"/>
    <u32 name="m_LastClockTick"/>

    <u32 name="m_GameClockHour"/>
    <u32 name="m_GameClockMinute"/>
    <u32 name="m_GameClockSecond"/>

    <u32 name="m_GameClockDay"/>
    <u32 name="m_GameClockMonth"/>
    <u32 name="m_GameClockYear"/>

    <u32 name="m_moneyCheated"/>
    <u8 name="m_PlayerFlags"/>

    <u32 name="m_TimeInMilliseconds"/>
    <u32 name="m_FrameCounter"/>

    <s32 name="m_OldWeatherType"/>
    <s32 name="m_NewWeatherType"/>
    <s32 name="m_ForcedWeatherType"/>
    <float name="m_InterpolationValue"/>
    <s32 name="m_WeatherTypeInList"/>
    <float name="m_Rain"/>

    <enum name="m_MaximumWantedLevel" type="eWantedLevel"/>
    <s32 name="m_nMaximumWantedLevel"/>

    <s32 name="m_NumberOfTimesPickupHelpTextDisplayed"/>

    <bool name="m_bHasDisplayedPlayerQuitEnterCarHelpText"/>

    <bool name="m_bIncludeLastStationOnSinglePlayerStat"/>

    <!-- Only actually used on PC, contains triggered tamper actions - renamed for obfuscation -->
    <u32 name="m_activationDataThing"/>

    <!-- Id of saved single player session. -->
    <u32 name="m_SpSessionIdHigh"/>
    <u32 name="m_SpSessionIdLow"/>

    <array name="m_fogOfWar" type="atArray">
      <u8/>
    </array>

  </structdef>

</ParserSchema>
