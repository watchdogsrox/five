<?xml version="1.0"?>
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
              xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd" generate="class">
  
  <hinclude>
    "atl/bitset.h"
  </hinclude>

  <enumdef type="eAnimSceneFlags" generate="bitset">
    <enumval name="ASF_LOOPING" description ="Loop the entire scene"/>
    <enumval name="ASF_HOLD_AT_END" description ="Hold on the last frame of the scene instead of ending it automatically"/>
    <enumval name="ASF_AUTO_START_WHEN_LOADED" description ="Automatically start the scene when the load finishes"/>
  </enumdef>
  
</ParserSchema>