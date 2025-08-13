<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema" generate="class">

  <hinsert>
    #include "system/bit.h"
  </hinsert>
  
  <enumdef type="eWarningMessageStyle">
    <enumval name="WARNING_MESSAGE_STANDARD" value="0" />
    <enumval name="WARNING_MESSAGE_DISPLAY_CALIBRATION" />
    <enumval name="WARNING_MESSAGE_REPORT_SCREEN" />
  </enumdef>

  <!--
    This matches the enum eWarningButtonFlags, but instead of being the bitmask
    values directly, the values represent the bit index. This is how parGen
    flag types work on serialize/deserialize.
    
    So be sure to keep it in sync with the values in that file
    -->
  <enumdef type="eParsableWarningButtonFlags">
    <!-- This is a convenience helper for runtime, don't set via .meta as otherwise
      you'll be setting bit 0 -->
    <enumval name="UI_WARNING_NONE"                   value="0" />
    
    <enumval name="UI_WARNING_SELECT"                 value="0" />
    <enumval name="UI_WARNING_OK"                     value="1" />
    <enumval name="UI_WARNING_YES"                    value="2" />
    <enumval name="UI_WARNING_BACK"                   value="3" />
    <enumval name="UI_WARNING_CANCEL"                 value="4" />
    <enumval name="UI_WARNING_NO"                     value="5" />
    <enumval name="UI_WARNING_RETRY"                  value="6" />
    <enumval name="UI_WARNING_RESTART"                value="7" />
    <enumval name="UI_WARNING_SKIP"                   value="8" />
    <enumval name="UI_WARNING_QUIT"                   value="9" />
    <enumval name="UI_WARNING_ADJUST_LEFTRIGHT"       value="10" />
    <enumval name="UI_WARNING_IGNORE"                 value="11" />
    <enumval name="UI_WARNING_SHARE"                  value="12" />
    <enumval name="UI_WARNING_SIGNIN"                 value="13" />
    <enumval name="UI_WARNING_CONTINUE"               value="14" />
    <enumval name="UI_WARNING_ADJUST_STICKLEFTRIGHT"  value="15" />
    <enumval name="UI_WARNING_ADJUST_UPDOWN"          value="16" />
    <enumval name="UI_WARNING_OVERWRITE"              value="17" />
    <enumval name="UI_WARNING_SOCIAL_CLUB"            value="18" />
    <enumval name="UI_WARNING_CONFIRM"                value="19" />
    
    <enumval name="UI_WARNING_CONTNOSAVE"             value="20" />
    <enumval name="UI_WARNING_RETRY_ON_A"             value="21" />
    <enumval name="UI_WARNING_BACK_WITH_ACCEPT_SOUND" value="22" />
    
    <!-- Jump from 22 to 27 is NOT a mistake, they exist in the frontend.xml but not exposed to the enum -->
    <enumval name="UI_WARNING_SPINNER"                value="27" />
    <enumval name="UI_WARNING_RETURNSP"               value="28" />
    <enumval name="UI_WARNING_CANCEL_ON_B"            value="29" />
    
    <enumval name="UI_WARNING_NOSOUND"                value="30" />

    <enumval name="UI_WARNING_EXIT"                   value="31" />
    
    <!-- Commonly combined flags for ease of use as a bitmask at runtime 
      do not set via parGen as you won't ge the results you expect -->
    <enumval name="UI_WARNING_YES_NO" value="BIT(UI_WARNING_YES)|BIT(UI_WARNING_NO)" />
    <enumval name="UI_WARNING_OK_CANCEL" value="BIT(UI_WARNING_OK)|BIT(UI_WARNING_CANCEL)" />
  </enumdef>

</ParserSchema>