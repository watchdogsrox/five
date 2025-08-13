<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <const name="MAX_PROCEDURAL_TAGS" value="255"/>
  
  <enumdef type="ProcObjDataFlags">
    <enumval name="PROCOBJ_ALIGN_OBJ" />
    <enumval name="PROCOBJ_USE_GRID" />
    <enumval name="PROCOBJ_USE_SEED" />
    <enumval name="PROCOBJ_IS_FLOATING" />
    <enumval name="PROCOBJ_CAST_SHADOW" />
    <enumval name="PROCOBJ_NETWORK_GAME" />
  </enumdef>
  
  <structdef type="CProcObjInfo"  onPostLoad="OnPostLoad" onPreSave="OnPreSave">
    <string name="m_Tag" type="atHashString" ui_key="true" description="The tag name of the procedural item" ui_liveedit="false"/>
    <string name="m_PlantTag" type="atHashString" description="The name of proc grass tag to share collision triangle with this proc object def (only 1 procgrass name allowed per whole range of proctags)" />
    <string name="m_ModelName" type="atHashString" description="The name of the object to be generated" />
    <Float16 name="m_Spacing" min="2.0" max="100.0" step="0.1" init="5.0" description="The average spacing of the objects the objects will be placed on average of 1 object every n square metres"/>
    <Float16 name="m_MinXRotation" min="-6.28318531f" max ="6.28318531f" step="0.01f" init="0.0f" description="The minimum x rotation in radians" />
    <Float16 name="m_MaxXRotation" min="-6.28318531f" max ="6.28318531f" step="0.01f" init="0.0f" description="The maximum x rotation in radians" />
    <Float16 name="m_MinYRotation" min="-6.28318531f" max ="6.28318531f" step="0.01f" init="0.0f" description="The minimum y rotation in radians" />
    <Float16 name="m_MaxYRotation" min="-6.28318531f" max ="6.28318531f" step="0.01f" init="0.0f" description="The maximum y rotation in radians" />
    <Float16 name="m_MinZRotation" min="-6.28318531f" max ="6.28318531f" step="0.01f" init="0.0f" description="The minimum z rotation in radians" />
    <Float16 name="m_MaxZRotation" min="-6.28318531f" max ="6.28318531f" step="0.01f" init="0.0f" description="The maximum z rotation in radians" />
    <Float16 name="m_MinScale" min="0.01f" max="10.0f" step="0.01f" init="1.0f" description="The min scale of the object in x and y (ignored if the object is dynamic) - NOTE: the collision will not be scaled" />
    <Float16 name="m_MaxScale" min="0.01f" max="10.0f" step="0.01f" init="1.0f" description="The max scale of the object in x and y (ignored if the object is dynamic) - NOTE: the collision will not be scaled" />
    <Float16 name="m_MinScaleZ" min="-1.0f" max="10.0f" step="0.01f" init="1.0f" description="The min scale of the object in z (if -1, then xy scaling is used) (ignored if the object is dynamic) - NOTE: the collision will not be scaled" />
    <Float16 name="m_MaxScaleZ" min="-1.0f" max="10.0f" step="0.01f" init="1.0f" description="The max scale of the object in z (ignored if the object is dynamic) - NOTE: the collision will not be scaled" />
    <Float16 name="m_MinZOffset" min="-10.0f" max="10.0f" step="0.01f" init="0.0f" description="Min offset to add to the object's height" />
    <Float16 name="m_MaxZOffset" min="-10.0f" max="10.0f" step="0.01f" init="0.0f" description="Max offset to add to the object's height" />
    <Float16 name="m_MinDistance" min="1" max="200" step="1" init="40" description="No objects created closer than this" />
    <Float16 name="m_MaxDistance" min="1" max="200" step="1" init="75" description="Objects further than this will be deleted (hence it should be bigger than model's LodDist, etc.) note: only *first* MAXDIST within proc id group is used for rejection test (so make it to be biggest in the whole group)" />
    <u8 name="m_MinTintPalette" min="0" max="255" step="1" init="0" description="Tint: minimum palette index to use (0; n-1) (255=random from whole range)" />
    <u8 name="m_MaxTintPalette" min="0" max="255" step="1" init="0" description="Tint: maximum palette index to use (0; n-1) (255=random), final tint palette is randomly selected between min and max tint palette" />
    <bitset name="m_Flags" type="fixed8" init="PROCOBJ_ALIGN_OBJ" values="ProcObjDataFlags" />
  </structdef>

  <enumdef type="PlantInfoFlags">
    <enumval name="PROCPLANT_LOD0" />
    <enumval name="PROCPLANT_LOD1" />
    <enumval name="PROCPLANT_LOD2" />
    <enumval name="PROCPLANT_FURGRASS" />
    <enumval name="PROCPLANT_CAMERADONOTCULL" />
    <enumval name="PROCPLANT_UNDERWATER" />
    <enumval name="PROCPLANT_GROUNDSCALE1VERT" />
    <enumval name="PROCPLANT_NOGROUNDSKEW_LOD0" />
    <enumval name="PROCPLANT_NOGROUNDSKEW_LOD1" />
    <enumval name="PROCPLANT_NOGROUNDSKEW_LOD2" />
    <enumval name="PROCPLANT_NOSHADOW" />
  </enumdef>
  
  <structdef type="CPlantInfo"  onPostLoad="OnPostLoad">
    <string name="m_Tag" type="atHashString" ui_key="true" description="The tag name of the procedural item" ui_liveedit="false"/>
    <Color32 name="m_Color" description="Plant color: R, G, B"/>
    <Color32 name="m_GroundColor" description="Ground color: R, G, B + encoded scale" ui_liveedit="false"/>
    <Float16 name="m_ScaleXY" min="0.01f" max="10.0f" step="0.01f" init="1.0f" description="Scale XY" />
    <Float16 name="m_ScaleZ" min="0.01f" max="10.0f" step="0.01f" init="1.0f" description="Scale Z" />
    <Float16 name="m_ScaleVariationXY" min="0.0f" max="10.0f" step="0.01f" init="1.0f" description="Scale variation XY" />
    <Float16 name="m_ScaleVariationZ" min="0.0f" max="10.0f" step="0.01f" init="1.0f" description="Scale variation Z" />
    <Float16 name="m_ScaleRangeXYZ" min="0.0f" max="10.0f" step="0.01f" init="1.0f" description="Scale range XYZ" />
    <Float16 name="m_ScaleRangeZ" min="0.0f" max="10.0f" step="0.01f" init="1.0f" description="Scale range Z" />
    <Float16 name="m_MicroMovementsScaleH" min="0.0f" max="1.0f" step="0.001f" init="0.2f" description="Micro-movements: global horizontal scale" />
    <Float16 name="m_MicroMovementsScaleV" min="0.0f" max="1.0f" step="0.001f" init="0.2f" description="Micro-movements: global vertical scale" />
    <Float16 name="m_MicroMovementsFreqH" min="0.01f" max="10.0f" step="0.01f" init="1.0f" description="Micro-movements: global horizontal frequency" />
    <Float16 name="m_MicroMovementsFreqV" min="0.01f" max="10.0f" step="0.01f" init="0.5f" description="Micro-movements: global vertical frequency" />
    <Float16 name="m_WindBendScale" min="0.01f" max="10.0f" step="0.01f" init="1.0f" description="Wind bend scale" />
    <Float16 name="m_WindBendVariation" min="0.01f" max="10.0f" step="0.01f" init="0.5f" description="Wind bend variation" />
    <Float16 name="m_CollisionRadius" min="0.01f" max="10.0f" step="0.01f" init="1.0f" description="Collision radius" />
    <Float16 name="m_Density" min="0.0001f" max="100.0f" step="0.01f" init="0.2f" description="Num of plants per sq metre" />
    <Float16 name="m_DensityRange" min="0.0001f" max="1.0f" step="0.01f" init="0.1f" description="Density range" />
    <u8 name="m_ModelId" min="0" max="31" init="0" description="Model_id of the plant" />
    <u8 name="m_TextureId" min="0" max="31" init="0" description="Texture number to use for the model (was UV offset for PS2)" />
    <bitset name="m_Flags" type="fixed16" init="BIT(PROCPLANT_LOD0) | BIT(PROCPLANT_LOD1) | BIT(PROCPLANT_LOD2)" values="PlantInfoFlags" />
    <u8 name="m_Intensity" description="Intensity" />
    <u8 name="m_IntensityVar" description="Intensity Variation" />
  </structdef>

  <enumdef type="ProcTagLookupFlags">
    <enumval name="PROCTAGLOOKUP_NEXTGENONLY" />
  </enumdef>

 <structdef type="ProcTagLookup" onPreLoad="PreLoad" onPostSave="PostSave">
   <string name="name" type="atString" platform="tool" ui_key="true" />
   <string name="procObjTag" type="atString" platform="tool" />
   <string name="plantTag" type="atString" platform="tool" />
   <bitset name="m_Flags" type="fixed8" values="ProcTagLookupFlags" />
 </structdef>
  
  <structdef type="CProceduralInfo"  onPostLoad="ConstructTagTable">
    <array name="m_procObjInfos" type="atArray" ui_liveedit="false">
      <struct type="CProcObjInfo" />
    </array>
    <array name="m_plantInfos" type="atArray" ui_liveedit="false">
      <struct type="CPlantInfo" />
    </array>
    <array name="m_procTagTable" type="member" size="MAX_PROCEDURAL_TAGS" ui_liveedit="false">
      <struct type="ProcTagLookup" />
    </array>
  </structdef>

</ParserSchema>
