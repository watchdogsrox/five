//
// animation/Filter.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "Filter_parser.h"

/* <!-- Metadata for XML I/O -->

<enumdef type="fwAnimBoneTag">
<enumval name="BONETAG_INVALID" value="-1" /> 
<enumval name="BONETAG_ROOT" value="0" /> 
<enumval name="BONETAG_PELVIS" value="11816" /> 
<enumval name="BONETAG_SPINE_ROOT" value="57597" /> 
<enumval name="BONETAG_SPINE0" value="23553" /> 
<enumval name="BONETAG_SPINE1" value="24816" /> 
<enumval name="BONETAG_SPINE2" value="24817" /> 
<enumval name="BONETAG_SPINE3" value="24818" /> 
<enumval name="BONETAG_NECK" value="39317" /> 
<enumval name="BONETAG_HEAD" value="31086" /> 
<enumval name="BONETAG_R_CLAVICLE" value="10706" /> 
<enumval name="BONETAG_R_UPPERARM" value="40269" /> 
<enumval name="BONETAG_R_FOREARM" value="28252" /> 
<enumval name="BONETAG_R_HAND" value="57005" /> 
<enumval name="BONETAG_R_FINGER0" value="58866" /> 
<enumval name="BONETAG_R_FINGER01" value="64016" /> 
<enumval name="BONETAG_R_FINGER02" value="64017" /> 
<enumval name="BONETAG_R_FINGER1" value="58867" /> 
<enumval name="BONETAG_R_FINGER11" value="64096" /> 
<enumval name="BONETAG_R_FINGER12" value="64097" /> 
<enumval name="BONETAG_R_FINGER2" value="58868" /> 
<enumval name="BONETAG_R_FINGER21" value="64112" /> 
<enumval name="BONETAG_R_FINGER22" value="64113" /> 
<enumval name="BONETAG_R_FINGER3" value="58869" /> 
<enumval name="BONETAG_R_FINGER31" value="64064" /> 
<enumval name="BONETAG_R_FINGER32" value="64065" /> 
<enumval name="BONETAG_R_FINGER4" value="58870" /> 
<enumval name="BONETAG_R_FINGER41" value="64080" /> 
<enumval name="BONETAG_R_FINGER42" value="64081" /> 
<enumval name="BONETAG_L_CLAVICLE" value="64729" /> 
<enumval name="BONETAG_L_UPPERARM" value="45509" /> 
<enumval name="BONETAG_L_FOREARM" value="61163" /> 
<enumval name="BONETAG_L_HAND" value="18905" /> 
<enumval name="BONETAG_L_FINGER0" value="26610" /> 
<enumval name="BONETAG_L_FINGER01" value="4089" /> 
<enumval name="BONETAG_L_FINGER02" value="4090" /> 
<enumval name="BONETAG_L_FINGER1" value="26611" /> 
<enumval name="BONETAG_L_FINGER11" value="4169" /> 
<enumval name="BONETAG_L_FINGER12" value="4170" /> 
<enumval name="BONETAG_L_FINGER2" value="26612" /> 
<enumval name="BONETAG_L_FINGER21" value="4185" /> 
<enumval name="BONETAG_L_FINGER22" value="4186" /> 
<enumval name="BONETAG_L_FINGER3" value="26613" /> 
<enumval name="BONETAG_L_FINGER31" value="4137" /> 
<enumval name="BONETAG_L_FINGER32" value="4138" /> 
<enumval name="BONETAG_L_FINGER4" value="26614" /> 
<enumval name="BONETAG_L_FINGER41" value="4153" /> 
<enumval name="BONETAG_L_FINGER42" value="4154" /> 
<enumval name="BONETAG_L_THIGH" value="58271" /> 
<enumval name="BONETAG_L_CALF" value="63931" /> 
<enumval name="BONETAG_L_FOOT" value="14201" /> 
<enumval name="BONETAG_L_TOE" value="2108" /> 
<enumval name="BONETAG_R_THIGH" value="51826" /> 
<enumval name="BONETAG_R_CALF" value="36864" /> 
<enumval name="BONETAG_R_FOOT" value="52301" /> 
<enumval name="BONETAG_R_TOE" value="20781" /> 
<enumval name="BONETAG_NECKROLL" value="35731" /> 
<enumval name="BONETAG_L_ARMROLL" value="5232" /> 
<enumval name="BONETAG_R_ARMROLL" value="37119" /> 
<enumval name="BONETAG_L_FOREARMROLL" value="61007" /> 
<enumval name="BONETAG_R_FOREARMROLL" value="43810" /> 
<enumval name="BONETAG_L_THIGHROLL" value="23639" /> 
<enumval name="BONETAG_R_THIGHROLL" value="6442" /> 
<enumval name="BONETAG_PH_L_HAND" value="60309" /> 
<enumval name="BONETAG_PH_R_HAND" value="28422" /> 
<enumval name="BONETAG_WEAPON_GRIP" value="41922" /> 
<enumval name="BONETAG_WEAPON_GRIP2" value="18212" /> 
<enumval name="BONETAG_CAMERA" value="33399" /> 
</enumdef>

*/

