<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

  <structdef type="CAnimSceneEntityInitialiser" >
  </structdef>

  <structdef type="CAnimScenePedInitialiser" base="CAnimSceneEntityInitialiser">
  </structdef>

  <structdef type="CAnimSceneObjectInitialiser" base="CAnimSceneEntityInitialiser">
  </structdef>

  <structdef type="CAnimSceneVehicleInitialiser" base="CAnimSceneEntityInitialiser">
  </structdef>

  <structdef type="CAnimSceneBooleanInitialiser" base="CAnimSceneEntityInitialiser">
  </structdef>

  <structdef type="CAnimSceneCameraInitialiser" base="CAnimSceneEntityInitialiser">
  </structdef>

</ParserSchema>