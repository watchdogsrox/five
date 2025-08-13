<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="eLensArtefactBlurType">
  <enumval name="LENSARTEFACT_BLUR_2X"/>
  <enumval name="LENSARTEFACT_BLUR_4X"/>
  <enumval name="LENSARTEFACT_BLUR_8X"/>
  <enumval name="LENSARTEFACT_BLUR_16X"/>
  <enumval name="LENSARTEFACT_BLUR_0X"/>
</enumdef>

<enumdef type="eLensArtefactStreakDir">
  <enumval name="LENSARTEFACT_STREAK_HORIZONTAL"/>
  <enumval name="LENSARTEFACT_STREAK_VERTICAL"/>
  <enumval name="LENSARTEFACT_STREAK_DIAGONAL_1"/>
  <enumval name="LENSARTEFACT_STREAK_DIAGONAL_2"/>
  <enumval name="LENSARTEFACT_STREAK_NONE"/>
</enumdef>

<enumdef type="eLensArtefactMaskType">
  <enumval name="LENSARTEFACT_MASK_HORIZONTAL"/>
  <enumval name="LENSARTEFACT_MASK_VERTICAL"/>
  <enumval name="LENSARTEFACT_MASK_NONE"/>
</enumdef> 

<structdef type="LensArtefact">
  <string name="m_name" type="atHashString"/>
  <enum name="m_blurType" type="eLensArtefactBlurType" init="LENSARTEFACT_BLUR_2X"/>
  <Vector2 name="m_scale" min="-10.0f" max="10.0f" init="0.0f" step="0.01f"/>
  <Vector2 name="m_offset" min="-1.0f" max="1.0f" init="0.5f" step="0.01f"/>
  <Color32 name="m_colorShift"/>
  <float name="m_opacity" min="0.0f" max="1.0f" init="0.0f" step="0.01f"/>
  <enum name="m_fadeMaskType" type="eLensArtefactMaskType" init="LENSARTEFACT_MASK_NONE"/>
  <Vector2 name="m_fadeMaskMinMax" min="0.0f" init="0.0f"/>
  <enum name="m_streakDirection" type="eLensArtefactStreakDir" init="LENSARTEFACT_STREAK_NONE"/>
  <bool name="m_enabled" init="false"/>
  <int name="m_sortIndex" init="0"/>
</structdef>

 <structdef type="LensArtefacts">
  <array name="m_layers" type="atArray">
    <struct type="LensArtefact"/>
  </array>
</structdef>
  
</ParserSchema>
