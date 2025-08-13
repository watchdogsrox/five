<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
>


  <structdef type="CWeaponModelInfo::InitData">
    <string 	name="m_modelName" type="atString" ui_key="true"/>
    <string 	name="m_txdName" type="atString" />
	  <string		name="m_ptfxAssetName" type="ConstString"/>
    <float 		name="m_lodDist"/>
    <float    name="m_HDDistance" init="3.0f"/>
    <string   name="m_ExpressionSetName" type="atHashString" init="null"/>
    <string   name="m_ExpressionDictionaryName" type="atHashString" init="null"/>
    <string   name="m_ExpressionName" type="atHashString" init="null"/>
  </structdef>

  <structdef type="CWeaponModelInfo::InitDataList">
    <array name="m_InitDatas" type="atArray">
      <struct type="CWeaponModelInfo::InitData"/>
    </array>
  </structdef>

</ParserSchema>
