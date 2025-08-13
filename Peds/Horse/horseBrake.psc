<?xml version="1.0"?>
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

<structdef type="hrsBrakeControl::Tune" cond="ENABLE_HORSE">
<float name="m_fSoftBrakeDecel" init="0.45f" min="0" max="1" step="0.01f"/>
<float name="m_fHardBrakeDecel" init="1.0f" min="0" max="1" step="0.01f"/>
</structdef>

</ParserSchema>