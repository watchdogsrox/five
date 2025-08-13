<?xml version="1.0"?>
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

<structdef type="hrsAgitationControl::Tune" cond="ENABLE_HORSE">
<float name="m_fBaselineAgitation" min="0" max="1" step="0.01f"/>
<float name="m_fDecayRate" min="0" max="20" step="0.01f"/>
<float name="m_fMinTimeSinceSpurForDecay" init="0.5f" min="0" max="10" step="0.01f"/>
  <bool name="m_bBuckingDisabled"></bool>
<struct name="m_KFByRiderMountSpeed" type="rage::ptxKeyframe"/>
<struct name="m_KFByFreshness" type="rage::ptxKeyframe"/>
</structdef>

</ParserSchema>