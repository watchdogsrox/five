<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema" generate="class">

  <!-- This matches up with an Actionscript enum, so please be sure to update both -->
  <enumdef type="uiPageDeck::eItemFocusState">
    <enumval name="STATE_UNFOCUSED" value="0" />
    <enumval name="STATE_FOCUSED" />
  </enumdef>

  <enumdef type="uiPageDeck::eItemHoverState">
    <enumval name="STATE_UNHOVERED" value="0" />
    <enumval name="STATE_HOVERED" />
  </enumdef>

  <!-- 
        Sets the scale that content should align to on a given axis
        
        None - We perfom no default scaling (item can still provide it's own however)
        Fit - We maintain the aspect ratio and scale to fit
  -->
  <enumdef type="uiPageDeck::eScaleType">
    <enumval name="SCALE_NONE" value="0" />
    <enumval name="SCALE_FIT" />
  </enumdef>
  
  <!-- Horizontal Alignment Options -->
  <enumdef type="uiPageDeck::eHAlign">
    <enumval name="HALIGN_LEFT" value="0" />
    <enumval name="HALIGN_CENTER" />
    <enumval name="HALIGN_RIGHT" />
  </enumdef>

  <!-- Vertical Alignment Options -->
  <enumdef type="uiPageDeck::eVAlign">
    <enumval name="VALIGN_TOP" value="0" />
    <enumval name="VALIGN_CENTER" />
    <enumval name="VALIGN_BOTTOM" />
  </enumdef>

  <!-- Grid row/column sizing options -->
  <enumdef type="uiPageDeck::eGridSizingType">
    <enumval name="SIZING_TYPE_DIP" />
    <enumval name="SIZING_TYPE_STAR" />
  </enumdef>
  
</ParserSchema>
