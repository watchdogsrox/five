<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

    <structdef type="CAnimSceneDictionary">
      <map name="m_scenes" type="atBinaryMap" key="atHashString">
        <pointer type="CAnimScene" policy="owner" />
      </map>
    </structdef>

</ParserSchema>