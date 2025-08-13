<?xml version="1.0"?>
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

<structdef type="hrsFreshnessControl::Tune" cond="ENABLE_HORSE">
<struct name="m_KFBySpeed" type="rage::ptxKeyframe"/>
<float name="m_fReplenishDelay" init="0" min="0" max="10" step="0.01f"/>
</structdef>

</ParserSchema>