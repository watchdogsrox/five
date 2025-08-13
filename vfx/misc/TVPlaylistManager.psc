<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CTVVideoInfo">
    <string name="m_Name" type="atHashString"/>
    <string name="m_VideoFileName" type="atString"/>
    <float name="m_fDuration" init="0.0f"/>
    <bool name="m_bNotOnDisk" init="false"/>
  </structdef>

  <structdef type="CTVPlayListSlot">
    <string name="m_Name" type="atHashString"/>
    <array name="m_TVVideoInfoNames" type="atArray">
      <string type="atHashString"/>
    </array>
  </structdef>

  <structdef type="CTVPlayList">
    <string name="m_Name" type="atHashString"/>
    <array name="m_TVPlayListSlotNames" type="atArray">
      <string type="atHashString"/>
    </array>
  </structdef>

  <structdef type="CTVPlaylistContainer">
    <array name="m_Videos" type="atArray">
      <struct type="CTVVideoInfo"/>
    </array>
    <array name="m_PlayListSlots" type="atArray">
      <struct type="CTVPlayListSlot"/>
    </array>
    <array name="m_Playlists" type="atArray">
      <struct type="CTVPlayList"/>
    </array>
  </structdef>

</ParserSchema>
