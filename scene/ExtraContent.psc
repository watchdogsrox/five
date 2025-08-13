<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
	<structdef type="CDLCScript">
		<string name="m_startupScript" type="atFinalHashString"/>
		<s32 name="m_scriptCallstackSize"/>
	</structdef>

	<structdef type="sOverlayInfos">
		<array name="m_overlayInfos" type="atArray">
			<struct type="sOverlayInfo"/>
		</array>
	</structdef>

  <structdef type="sWorldPointScriptBrains">
    <string name="m_ScriptName" type="atFinalHashString"/>
    <float name="m_ActivationRange"/>
    <s32 name="m_setToWhichThisBrainBelongs"/>
  </structdef>

  <structdef type="sScriptBrainLists">
    <array name="m_worldPointBrains" type="atArray">
      <struct type="sWorldPointScriptBrains"/>
    </array>
  </structdef>

    <structdef type="sOverlayInfo">
		<string name="m_nameId" type="atHashString"/>
		<string name="m_changeSet" type="atHashString"/>
		<string name="m_changeSetGroupToExecuteWith" type="atHashString"/>
		<s8 name="m_version"/>
	</structdef>
<structdef type="CExtraTextMetaFile">
	<bool name="m_hasGlobalTextFile" init="false"/>
	<bool name="m_hasAdditionalText" init="false"/>
	<bool name="m_isTitleUpdate" init="false"/>	
</structdef>

<structdef type="CCloudStorageFile">
  <string name="m_File" type="atString"/>
  <string name="m_MountPoint" type="atString"/>
</structdef>

<structdef type="CExtraContentCloudPackDescriptor">
  <string name="m_Name" type="atString"/>
  <string name="m_ProductId" type="atString"/>
</structdef>

<structdef type="CExtraContentCloudManifest">

  <array name="m_Files" type="atArray">
    <struct type="CCloudStorageFile"/>
  </array>

  <array name="m_CompatibilityPacks" type="atArray">
    <struct type="CExtraContentCloudPackDescriptor"/>
  </array>

  <array name="m_PaidPacks" type="atArray">
    <struct type="CExtraContentCloudPackDescriptor"/>
  </array>

  <string name="m_WeaponPatchNameMP" type="atString"/>
  <string name="m_ScriptPatchName" type="atString"/>

</structdef>

<structdef type="SContentUnlocks">
  <array name="m_listOfUnlocks" type="atArray">
    <string name="m_contentUnlock" type="atHashString"/>
  </array>
</structdef>

  <enumdef type="eExtraContentPackType">
    <enumval name="EXTRACONTENT_TU_PACK" />
	<enumval name="EXTRACONTENT_LEVEL_PACK" />
	<enumval name="EXTRACONTENT_COMPAT_PACK" />
    <enumval name="EXTRACONTENT_PAID_PACK" />
  </enumdef>

  <structdef type="ContentChangeSetGroup">
    <string name="m_NameHash" type="atHashString"/>
    <array name="m_ContentChangeSets" type="atArray">
      <string name="m_changeSet" type="atHashString"/>
    </array>
  </structdef>

  <structdef type="SSetupData" onPostLoad= "OnPostLoad">
  <string name="m_deviceName" type="atString"/>
  <string name="m_datFile" type="atString"/>
  <string name="m_timeStamp" type="atString"/>
  <string name="m_nameHash" type="atFinalHashString"/>
  <array name="m_contentChangeSets" type="atArray">
    <string name="m_changeSet" type="atHashString"/>
  </array>
  <array name="m_contentChangeSetGroups" type="atArray">
    <struct type="ContentChangeSetGroup"/>
  </array>   
  <string name="m_startupScript" type="atFinalHashString"/>
  <s32 name="m_scriptCallstackSize"/>
  <enum name="m_type" type="eExtraContentPackType">EXTRACONTENT_PAID_PACK</enum>
  <s32 name="m_order" init="0"/>
  <s32 name="m_minorOrder" init="0"/>
  <bool name="m_isLevelPack" init="false"/>
  <string name="m_dependencyPackHash" type="atHashString"/>
  <string name="m_requiredVersion" type="atString"/>
  <s32 name="m_subPackCount"/>  
</structdef>

<structdef type="SMandatoryPacksData" >
  <array name="m_Paths" type="atArray">
    <string name="m_PackPath" type="atString"/>    
  </array>
</structdef>

  <structdef type="SExtraTitleUpdateMount" >
  <string name="m_deviceName" type="atString"/>
  <string name="m_path" type="atString"/>
</structdef>

<structdef type="SExtraTitleUpdateData" >
  <array name="m_Mounts" type="atArray">
    <struct type="SExtraTitleUpdateMount"/>
  </array>
</structdef>

<structdef type="SExtraFolderMount" >
  <string name="m_path" type="atString"/>
  <string name="m_mountAs" type="atString"/>
</structdef>

<structdef type="SExtraFolderMountData" >
  <array name="m_FolderMounts" type="atArray">
    <struct type="SExtraFolderMount"/>
  </array>
</structdef>

</ParserSchema>
