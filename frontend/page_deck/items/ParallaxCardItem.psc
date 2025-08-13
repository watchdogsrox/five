<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CParallaxCardItem::ParallaxTextureInfo">
    <string type="ConstString" name="m_backgroundTexture" parName="BackgroundTexture" />
    <string type="ConstString" name="m_foregroundTexture" parName="ForegroundTexture" />
    <int name="m_parallaxType" parName="ParallaxType" init="0" />
  </structdef>
  
  <structdef type="CParallaxCardItem" base="CCardItem" >
    <array name="m_textureInfoCollection" parName="Textures" type="atArray">
      <struct type="CParallaxCardItem::ParallaxTextureInfo" />
    </array>
  </structdef>
  
</ParserSchema>
