<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

  <structdef type="CAnimSceneHelperBase">
    <pad bytes="4" platform="32bit"/>  <!-- m_pOwningScene-->
    <pad bytes="8" platform="64bit"/>
  </structdef>
  
  <structdef type="CAnimSceneMatrix" base="CAnimSceneHelperBase" onPreAddWidgets="OnPreAddWidgets" onPostAddWidgets="OnPostAddWidgets">
    <Mat34V name="m_matrix" hideWidgets="true"/>
    <pad bytes="4" platform="32bit"/>    <!-- m_bankData-->
    <pad bytes="8" platform="64bit"/>
  </structdef>

  <!--<structdef type="CAnimSceneMatrixPtr" onPreAddWidgets="OnPreAddWidgets" onPostAddWidgets="OnPostAddWidgets" onPostLoad="OnPostLoad">
    <pointer name="m_pMatrix" type="Mat34V" policy="owner" hideWidgets="true"/>
  </structdef>-->

  <structdef type="CAnimSceneClip" base="CAnimSceneHelperBase" onPreAddWidgets="OnPreAddWidgets" >
    <string name="m_clipDictionaryName" type="atHashString" hideWidgets="true"/>
    <string name="m_clipName" type="atHashString" hideWidgets="true"/>
    <pad bytes="4" platform="32bit"/>    <!-- m_widgets-->
    <pad bytes="8" platform="64bit"/>
  </structdef>

  <structdef type="CAnimSceneClipSet" base="CAnimSceneHelperBase" >
    <string name="m_clipSetName" type="atHashString" />
  </structdef>

  <structdef type="CAnimSceneEntityHandle" base="CAnimSceneHelperBase" onPreAddWidgets="OnPreAddWidgets" onPreSave="OnPreSave">
    <string name="m_entityId" type="atHashString" hideWidgets="true"/>
    <pad bytes="4" platform="32bit"/>    <!-- m_pEntity-->
    <pad bytes="8" platform="64bit"/>
    <pad bytes="4" platform="32bit"/>    <!-- m_widgets-->
    <pad bytes="8" platform="64bit"/>
  </structdef>

</ParserSchema>