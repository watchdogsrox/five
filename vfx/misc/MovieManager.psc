<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CMovieSubtitleEventArg" simple="true">
    <string name="m_cName" type="atHashString"/>
    <float name="m_fSubtitleDuration"/>
  </structdef>

  <structdef type="CMovieSubtitleEvent" simple="true">
    <float name="m_fTime"/>
    <s32 name="m_iEventId"/>
    <s32 name="m_iEventArgsIndex"/>
    <s32 name="m_iObjectId"/>
  </structdef>

  <structdef type="CMovieSubtitleContainer" simple="true">
    <string name="m_TextBlockName" type="atString"/>
    <array name="m_pCutsceneEventArgsList" type="atArray">
      <pointer type="CMovieSubtitleEventArg" policy="owner"/>
    </array>
    <array name="m_pCutsceneEventList" type="atArray">
      <pointer type="CMovieSubtitleEvent" policy="owner"/>
    </array>    
  </structdef>							
							
</ParserSchema>
