<?xml version="1.0"?>

<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="psoChecks">

  <structdef type="LinearPiecewiseFogShape_ControlPoint" layoutHint="novptr" simple="true">
    <float name="heightValue"/>
    <float name="visibilityScale"/>
  </structdef>

  <structdef type="LinearPiecewiseFogShape" layoutHint="novptr" simple="true">
    <array name="m_ControlPoints" type="member" size="8">
      <struct type="LinearPiecewiseFogShape_ControlPoint" />
    </array>
  </structdef>

  <structdef type="LinearPiecewise_FogShapePalette" layoutHint="novptr" simple="true">
    <array name="m_Shapes" type="member" size="4">
      <struct type="LinearPiecewiseFogShape" />
    </array>
  </structdef>

</ParserSchema>
