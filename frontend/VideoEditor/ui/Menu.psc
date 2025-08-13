<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="eCOL_TYPE">
	<enumval name="COL_TYPE_LIST"/>
	<enumval name="COL_TYPE_BASIC_PAGE"/>
	<enumval name="COL_TYPE_LOAD_PROJ_INFO"/>
	<enumval name="COL_TYPE_IMG_PROJ_INFO"/>
	<enumval name="COL_TYPE_IMG_ONE"/>
	<enumval name="COL_TYPE_IMG_FOUR"/>
	<enumval name="COL_TYPE_IMG_TWELVE"/>
  <enumval name="COL_TYPE_TEXT_PLACEMENT"/>
  <enumval name="COL_TYPE_LIST_LONG_AUDIO"/>
</enumdef>

<enumdef type="eCOL_DATA_TYPE">
	<enumval name="COL_DATA_TYPE_STANDARD"/>
	<enumval name="COL_DATA_TYPE_LOAD_FILES"/>
  <enumval name="COL_DATA_TYPE_AUDIO_LIST"/>
</enumdef>
           
<enumdef type="eCOL_NUM">
    <enumval name="COL_1"/>
    <enumval name="COL_2"/>
    <enumval name="COL_3"/>
</enumdef>

<structdef type="CVideoEditorMenuOption">
    <string name="cTextId" type="atHashWithStringDev"/>
    <string name="Context" type="atHashString"/>
    <string name="Block" type="atHashString"/>
    <string name="LinkMenuId" type="atHashString"/>
    <string name="JumpMenuId" type="atHashString"/>
    <string name="TriggerAction" type="atHashString"/>
    <string name="ToggleValue" type="atHashString"/>
    <string name="WarningText" type="atHashString"/>
    <string name="DependentAction" type="atHashString"/>
    <string name="UniqueId" type="atHashString"/>
</structdef>


<structdef type="CVideoEditorMenuBasicPage">
  <string name="Header" type="atHashWithStringDev"/>
  <string name="Body" type="atHashWithStringDev"/>
  <string name="Block" type="atHashString"/>
</structdef>
  
  
<structdef type="CVideoEditorMenuItem">

  <string name="MenuId" type="atHashString"/>
  <enum name="columnId" type="eCOL_NUM"/>
	<enum name="columnType" type="eCOL_TYPE"/>
  <enum name="columnDataType" type="eCOL_DATA_TYPE"/>
  <string name="BackMenuId" type="atHashString"/>

  <array name="Option" type="atArray">
    <struct type="CVideoEditorMenuOption"/>
  </array>


  <pointer name="m_pContent" parName="Content" type="CVideoEditorMenuBasicPage"			policy="simple_owner"/>


</structdef>


<structdef type="CVideoEditorErrorMessage">

  <string name="type" type="atHashString"/>
  <string name="header" type="atHashString"/>
  <string name="body" type="atHashString"/>

</structdef>

<structdef type="CVideoEditorPositions">

  <string name="name" type="atString"/>
  <float name="pos_x"/>
  <float name="pos_y"/>
  <float name="size_x"/>
  <float name="size_y"/>

</structdef>

  <structdef type="CVideoEditorMenuArray">
	<array name="CVideoEditorMenuItems" type="atArray">
		<struct type="CVideoEditorMenuItem"/>
	</array>

  <array name="CVideoEditorErrorMessages" type="atArray">
    <struct type="CVideoEditorErrorMessage"/>
  </array>

    <array name="CVideoEditorPositioning" type="atArray">
      <struct type="CVideoEditorPositions"/>
    </array>
    
</structdef>

</ParserSchema>