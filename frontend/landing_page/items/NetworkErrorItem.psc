<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CNetworkErrorItem::SErrorDetails" simple="true" >
    <string name="m_title" parName="Title" type="atHashString" />
    <string name="m_description" parName="Description" type="atHashString" />
    <string name="m_tooltip" parName="Tooltip" type="atHashString" />
  </structdef>

  <structdef type="CNetworkErrorItem" base="CParallaxCardItem" >
    <struct name="m_defaultError" parName="DefaultError" type="SErrorDetails"  />
    <struct name="m_signInError" parName="SignInError" type="SErrorDetails"  />
    <struct name="m_connectionLinkError" parName="ConnectionLinkError" type="SErrorDetails"  />
    <struct name="m_signOnlineError" parName="SignOnlineError" type="SErrorDetails"  />
    <struct name="m_scsPrivilegeError" parName="ScsPrivilegeError" type="SErrorDetails"  />
    <struct name="m_permissionsError" parName="PermissionsError" type="SErrorDetails"  />
    <struct name="m_rosBannedError" parName="RosBannedError" type="SErrorDetails"  />
    <struct name="m_rosSuspendedError" parName="RosSuspendedError" type="SErrorDetails"  />
  </structdef>
  
</ParserSchema>
