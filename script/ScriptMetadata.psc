<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="class">


  <structdef type="CMPOutfitsData">
    <array name="m_ComponentDrawables" type="atFixedArray" size="12">
      <int/>
    </array>
    <array name="m_ComponentTextures" type="atFixedArray" size="12">
      <int/>
    </array>
    <array name="m_PropIndices" type="atFixedArray" size="9">
      <int/>
    </array>
    <array name="m_PropTextures" type="atFixedArray" size="9">
      <int/>
    </array>
    <array name="m_TattooHashes" type="atFixedArray" size="3">
      <int/>
    </array>
  </structdef>
  
  
  <structdef type="CMPOutfitsMap">
    <array name="m_MPOutfitsData" type="atArray">
      <struct type="CMPOutfitsData"/>
    </array>
  </structdef>
  

  <structdef type="CMPOutfits">
	<struct name="m_MPOutfitsDataMale" type="CMPOutfitsMap"/>
	<struct name="m_MPOutfitsDataFemale" type="CMPOutfitsMap"/>
  </structdef>
  
  
  
  <structdef type="CBaseElementLocation">
    <Vector3 name="m_location" init="0.0, 0.0, 0.0"/>
    <Vector3 name="m_rotation" init="0.0, 0.0, 0.0"/>
  </structdef>

    <structdef type="CBaseElementLocationsMap">
    <array name="m_BaseElementLocations" type="atArray">
      <struct type="CBaseElementLocation"/>
    </array>
  </structdef>

  <structdef type="CBaseElements">
    <struct name="m_BaseElementLocationsMap_HighApt" type="CBaseElementLocationsMap"/>
    <struct name="m_BaseElementLocationsMap" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap1" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap2" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap3" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap4" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap5" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap6" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap7" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap8" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap9" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap10" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap11" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap12" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap13" type="CBaseElementLocationsMap"/>
    <struct name="m_ExtraBaseElementLocMap14" type="CBaseElementLocationsMap"/>
  </structdef>

  <structdef type="CMPApparelData">
    <map name="m_MPApparelDataMale" key="atHashString" type="atMap">
      <int/>
    </map>
    <map name="m_MPApparelDataFemale" key="atHashString" type="atMap">
      <int/>
    </map>
  </structdef>

  <structdef type="CScriptMetadata">
    <struct name="m_MPOutfits" type="CMPOutfits"/>
    <struct name="m_BaseElements" type="CBaseElements"/>
    <struct name="m_MPApparelData" type="CMPApparelData"/>
  </structdef>

</ParserSchema>
