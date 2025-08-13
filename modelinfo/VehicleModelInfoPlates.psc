<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CVehicleModelInfoPlateTextureSet">
		<string name="m_TextureSetName" type="atHashString" ui_key="true"/>
		<string name="m_DiffuseMapName" type="atHashString"/>
		<string name="m_NormalMapName" type="atHashString"/>
		<Vector4 name="m_FontExtents" noInit="true"/>
		<Vector2 name="m_MaxLettersOnPlate" noInit="true"/>
		<Color32 name="m_FontColor" init="0x00FFFFFF"/>
		<Color32 name="m_FontOutlineColor" init="0x00FFFFFF"/>
		<bool name="m_IsFontOutlineEnabled" init="false"/>
		<Vector2 name="m_FontOutlineMinMaxDepth" noInit="true"/>
	</structdef>

	<structdef type="CVehicleModelInfoPlates" onPostLoad="PostLoad">
		<array name="m_Textures" type="atArray">
			<struct type="CVehicleModelInfoPlateTextureSet"/>
		</array>
		<int name="m_DefaultTexureIndex" init="-1"/>
		<u8 name="m_NumericOffset" init="0"/>
		<u8 name="m_AlphabeticOffset" init="10"/>
		<u8 name="m_SpaceOffset" init="63"/>
		<u8 name="m_RandomCharOffset" init="36"/>
		<u8 name="m_NumRandomChar" init="4"/>
	</structdef>

	<structdef type="LicensePlateProbabilityNamed">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<u32 name="m_Value" init="1" min="1"/>
	</structdef>

	<structdef type="CVehicleModelPlateProbabilities" name="PlateProbabilities">
		<array name="m_Probabilities" type="atArray">
			<struct type="LicensePlateProbabilityNamed"/>
		</array>
	</structdef>

</ParserSchema>