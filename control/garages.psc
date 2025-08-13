<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="GarageType">
    <enumval name="GARAGE_NONE"/>
    <enumval name="GARAGE_MISSION"/>
    <enumval name="GARAGE_RESPRAY"/>
    <enumval name="GARAGE_FOR_SCRIPT_TO_OPEN_AND_CLOSE"/>
    <enumval name="GARAGE_AMBIENT_PARKING"/>
    <enumval name="GARAGE_SAFEHOUSE_PARKING_ZONE"/>
  </enumdef>

  <structdef type="CGarage::Box">
    <float name="BaseX" />
    <float name="BaseY" />
    <float name="BaseZ" />

    <float name="Delta1X" />
    <float name="Delta1Y" />
    <float name="Delta2X" />
    <float name="Delta2Y" />

    <float name="CeilingZ" />
    <bool name="useLineIntersection" init="false"/>
  </structdef>

  <structdef type="CGarages::CGarageInitData">
    <string name="name" type="atHashString" description="Name of this garage" ui_key="true"/>
    <string name="owner" type="atHashString" description="The the owner name of this garage, or 'any'(0)"/>

    <array name="m_boxes" type="atArray">
      <struct type="CGarage::Box"/>
    </array>

    <enum name="type" type="GarageType" description="Type of garage"/>
    <int name="permittedVehicleType" init="-1" description="What types of vehicles can be stored"/>
    <bool name="startedWithVehicleSavingEnabled" init="true" description="Does this garages start with vehicle saving enabled"/>

    <bool name="isEnclosed" init="false" description="Is this garage an Enclosed Garage"/>
    <bool name="isMPGarage" init="false" description="Is this garage an MP Garage"/>
    <s8 name ="InteriorBoxIDX" init="-1" description="Index of which box defines the Interior area of a garage"/>
    <s8 name ="ExteriorBoxIDX" init="-1" description="Index of which box defines the Exterior area of a garage"/>
  </structdef>

  <structdef type="CGarages::CGarageInitDataList">
    <array name="garages" type="atArray">
      <struct type="CGarages::CGarageInitData"/>
    </array>
  </structdef>

</ParserSchema>

