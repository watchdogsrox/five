<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema" generate="class">


	<enumdef type="eReportLink">
	<enumval name="ReportLink_None"/>
	<enumval name="ReportLink_ReportRoot"/>
    <enumval name="ReportLink_PlayerMadeMissions"/>
    <enumval name="ReportLink_Crew"/>
    <enumval name="ReportLink_Confirmation"/>
	<enumval name="ReportLink_NoGamerHandle"/>
    <enumval name="ReportLink_Successful"/>
    <enumval name="ReportLink_CommendRoot"/>
    <enumval name="ReportLink_UGCReportRoot"/>
	<enumval name="ReportLink_Content"/>
    <enumval name="ReportLink_Other"/>
	</enumdef>

  <enumdef type="eReportType">
    <enumval name="ReportType_None"/>
    <enumval name="ReportType_Griefing"/>
    <enumval name="ReportType_OffensiveLanguage"/>
    <enumval name="ReportType_OffensiveLicensePlate"/>
    <enumval name="ReportType_Exploit"/>
    <enumval name="ReportType_GameExploit"/>
    <enumval name="ReportType_OffensiveUGC"/>
    <enumval name="ReportType_Other"/>
    <enumval name="ReportType_OffensiveCrewName"/>
    <enumval name="ReportType_OffensiveCrewMotto"/>
    <enumval name="ReportType_OffensiveCrewStatus"/>
    <enumval name="ReportType_OffensiveCrewEmblem"/>
    <enumval name="ReportType_PlayerMade_Title"/>
    <enumval name="ReportType_PlayerMade_Description"/>
    <enumval name="ReportType_PlayerMade_Photo"/>
    <enumval name="ReportType_PlayerMade_Content"/>
    <enumval name="ReportType_VoiceChat_Annoying"/>
    <enumval name="ReportType_VoiceChat_Hate"/>
    <enumval name="ReportType_TextChat_Annoying"/>
    <enumval name="ReportType_TextChat_Hate"/>
    
    <enumval name="ReportType_Friendly"/>
    <enumval name="ReportType_Helpful"/>
  </enumdef>


	<structdef type="CReportOption" generate="class" simple="true">
		<string name="LabelHash" type="atHashWithStringBank"/>
    <enum name="ReportLink" type="eReportLink" init="ReportLink_None"/>
    <enum name="ReportType" type="eReportType" init="ReportType_None"/>
	</structdef>

	<structdef type="CReportList" generate="class" simple="true">
	  <enum name="ReportLinkId" type="eReportLink" init="ReportLink_None"/>
    <string name="SubtitleHash" type="atHashWithStringBank" init="RP_DEFAULTSUB"/>
    <array name="ReportOptions" type="atArray">
      <struct type ="CReportOption"/>
    </array>
  </structdef>

  <structdef type="CReportArray" version="1" simple="true">
    <array name="m_ReportLists" type="atArray">
      <struct type ="CReportList"/>
    </array>
  </structdef>

</ParserSchema>
