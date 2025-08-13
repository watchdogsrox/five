<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CExtraContentSaveStructure::CInstalledPackagesStruct" preserveNames="true">
    <u32 name="m_nameHash"/>
  </structdef>

  <structdef type="CExtraContentSaveStructure" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <array name="m_InstalledPackages" type="atArray">
      <struct type="CExtraContentSaveStructure::CInstalledPackagesStruct"/>
    </array>
  </structdef>

</ParserSchema>
