<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CDefaultCrimeInfo">
    <string name="m_Name" type="atHashString"/>
    <float name="m_timeBetweenCrimes" init="100" min="0" max="1000"/>
  </structdef>

  <structdef type="CStealVehicleCrime" base="CDefaultCrimeInfo"/>

</ParserSchema>
