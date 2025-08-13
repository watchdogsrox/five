<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema" generate="class">

  <hinsert>
    #include "system/bit.h"
  </hinsert>
  
  <enumdef type="eScriptRouterContextSource">
    <enumval name="SRCS_INVALID" value="0" />
    <enumval name="SRCS_NATIVE_LANDING_PAGE" />
    <enumval name="SRCS_PROSPERO_GAME_INTENT" />
    <enumval name="SRCS_SCRIPT" />
  </enumdef>

  <enumdef type="eScriptRouterContextMode">
    <enumval name="SRCM_INVALID" value="0" />
    <enumval name="SRCM_FREE" />
    <enumval name="SRCM_STORY" />
    <enumval name="SRCM_CREATOR" />
    <enumval name="SRCM_EDITOR" />
    <enumval name="SRCM_LANDING_PAGE" />
  </enumdef>

  <enumdef type="eScriptRouterContextArgument">
    <enumval name="SRCA_INVALID" value="0" />
    <enumval name="SRCA_NONE" />
    <enumval name="SRCA_LOCATION" />
    <enumval name="SRCA_PROPERTY" />
    <enumval name="SRCA_HEIST" />
    <enumval name="SRCA_SERIES" />
    <enumval name="SRCA_WEBPAGE" />
    <enumval name="SRCA_GAMBLING" />
    <enumval name="SRCA_CHARACTER_CREATOR" />
    <enumval name="SRCA_STORE" />
    <enumval name="SRCA_MEMBERSHIP" />
    <enumval name="SRCA_LANDING_PAGE_ENTRYPOINT_ID"/>
  </enumdef>

</ParserSchema>