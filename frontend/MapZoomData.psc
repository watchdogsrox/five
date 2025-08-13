<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
  
	<structdef type="sMapZoomData::sMapZoomLevel">
		<float name="fZoomScale" />
		<float name="fZoomSpeed" />
		<float name="fScrollSpeed" />
		<Vector2 name="vTiles" />
	</structdef>
  
  <structdef type="sMapZoomData">
    <array name="zoomLevels" type="atArray">
      <struct type="sMapZoomData::sMapZoomLevel" />
    </array>
  </structdef>
  
</ParserSchema>