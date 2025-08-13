<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


  <structdef type="CargenPriorityAreas::Area">
      <Vector3 name="minPos"/>
      <Vector3 name="maxPos"/>
  </structdef>

  <structdef type="CargenPriorityAreas">
    <array name="areas" type="atArray">
      <struct type="CargenPriorityAreas::Area"/>
    </array>
  </structdef>

</ParserSchema>
