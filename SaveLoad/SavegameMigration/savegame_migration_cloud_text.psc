<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CSaveGameMigrationCloudTextLine" preserveNames="true" cond="__ALLOW_EXPORT_OF_SP_SAVEGAMES">
    <string name="m_TextKey" type="atString" />
    <string name="m_LocalizedText" type="atString" />
  </structdef>

  <structdef type="CSaveGameMigrationCloudTextData" preserveNames="true" cond="__ALLOW_EXPORT_OF_SP_SAVEGAMES">
    <array name="m_ArrayOfTextLines" type="atArray">
      <struct type="CSaveGameMigrationCloudTextLine"/>
    </array>
  </structdef>

</ParserSchema>
