#if __BANK

#include "DebugNodes.h"

#include "script\thread.h"
#include "script\wrapper.h"
#include "net\netaddress.h"
#include "net\tcp.h"
#include "bank\msgbox.h"

#define __DEBUGNODESYS 0
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////



static bool s_DebugNodeSysInitted = false;
static s32 s_DebugNodesSocket;
static const u32 s_DebugNodesPort = 20606;

#ifndef NET_MAKE_IP
//Creates a 32-bit IP address in host byte order.
#define NET_MAKE_IP(a, b, c, d)\
	unsigned((((a) & 0xFF) << 24)\
	| (((b) & 0xFF) << 16)\
	| (((c) & 0xFF) << 8)\
	| (((d) & 0xFF) << 0))
#endif

static const u32 s_DebugNodesIP = NET_MAKE_IP(10,11,19,80);

void DebugNodeSys::Init()
{
	Shutdown();

	// TODO: NS - this looks odd. The IP and port are being stored in network byte order, but ConnectAsync requires host byte order...
	netSocketAddress addr(netIpAddress(netIpV4Address(net_htonl(s_DebugNodesIP))),net_htons(s_DebugNodesPort));
	netSocketAddress proxyAddr;

	netTcpAsyncOp tcpAasyncOp;
	s_DebugNodeSysInitted = netTcp::ConnectAsync(addr, proxyAddr, &s_DebugNodesSocket, "", 60, CR_INVALID, &tcpAasyncOp);

	if(!s_DebugNodeSysInitted)
		return;

	//Wait for Connect to finish
	while (tcpAasyncOp.Pending())
	{
		sysIpcSleep(100);
	}

	if (AssertVerify(tcpAasyncOp.Succeeded()))
	{
		char buffer[1024];
		formatf(buffer,"/init\n");

		netTcpResult res = netTcp::SendBuffer(s_DebugNodesSocket,buffer,ustrlen(buffer),60);

		if(res != NETTCP_RESULT_OK)
			Displayf("nooo %d\n",res);
	}
}

void DebugNodeSys::Shutdown()
{
	if(!s_DebugNodeSysInitted)
		return;

	netTcp::Close(s_DebugNodesSocket);

	s_DebugNodeSysInitted = false;
}


// functions to keep the script viewer working - possibly can be removed later on
bool DebugNodeSys::LoadSetScript(s32 user,s32 index,s32& address,s32 size)
{
	NoteNodeSet* nodeset = (NoteNodeSet*)&address;
	return LoadSet(user, index, nodeset, size);
}

// functions to keep the script viewer working - possibly can be removed later on
bool DebugNodeSys::SaveSetScript(s32 user,s32 index,s32& address,s32 size)
{
	NoteNodeSet* nodeset = (NoteNodeSet*)&address;
	return SaveSet(user, index, nodeset, size);
}

bool DebugNodeSys::LoadSet(s32 user,s32 index,NoteNodeSet *nodeset,s32 ASSERT_ONLY(size))
{
	if(!s_DebugNodeSysInitted)
		Init();

	if(!s_DebugNodeSysInitted)
		return false;

	char buffer[2048];
	formatf(buffer,"/load_set, %d, %d\n",user,index);
	netTcpResult res = netTcp::SendBuffer(s_DebugNodesSocket,buffer,ustrlen(buffer),60);

	if(res != NETTCP_RESULT_OK)
		return false;

	u32 recv;
	res = netTcp::Receive(s_DebugNodesSocket,buffer,2048,&recv);

	if(res != NETTCP_RESULT_OK)
		return false;

	Assert(sizeof(NoteNodeSet) == (size/* * 4*/));

	char* p_item = strtok(buffer,",");
	Displayf("%s\n",p_item);
	p_item = strtok(NULL,","); if (p_item == NULL) return false;
	Displayf("%s\n",p_item);
	p_item = strtok(NULL,","); if (p_item == NULL) return false;
	Displayf("%s\n",p_item);
	p_item = strtok(NULL,","); if (p_item == NULL) return false;
	Displayf("%s\n",p_item);
	p_item = strtok(NULL,","); if (p_item == NULL) return false;
	Displayf("%s\n",p_item);
	nodeset->name = p_item;
	p_item = strtok(NULL,","); if (p_item == NULL) return false;
	Displayf("%s\n",p_item);
	nodeset->creation_number = atoi(p_item);
	p_item = strtok(NULL,","); if (p_item == NULL) return false;
	Displayf("%s\n",p_item);
	nodeset->node_creation_number = atoi(p_item);

	for(s32 i=0;i<nodeset->node_creation_number;i++)
	{
		NoteNode* node = &(nodeset->nodes[i]);

		p_item = strtok(NULL,","); if (p_item == NULL) return false;
		Displayf("%s\n",p_item);
		node->type = atoi(p_item);
		p_item = strtok(NULL,","); if (p_item == NULL) return false;
		Displayf("%s\n",p_item);
		node->coord.x = (float)atof(p_item);
		p_item = strtok(NULL,","); if (p_item == NULL) return false;
		Displayf("%s\n",p_item);
		node->coord.y = (float)atof(p_item);
		p_item = strtok(NULL,","); if (p_item == NULL) return false;
		Displayf("%s\n",p_item);
		node->coord.z = (float)atof(p_item);
		p_item = strtok(NULL,","); if (p_item == NULL) return false;
		Displayf("%s\n",p_item);
		node->label = p_item;
		p_item = strtok(NULL,","); if (p_item == NULL) return false;
		Displayf("%s\n",p_item);
		node->creation_number = atoi(p_item);
		p_item = strtok(NULL,","); if (p_item == NULL) return false;
		Displayf("%s\n",p_item);
		node->link = atoi(p_item);
	}

	//nodeset->nodes_size = NUMBER_OF_NODES_PER_SET;
	nodeset->open = true;

	return true;
}

bool DebugNodeSys::SaveSet(s32 user,s32 index,NoteNodeSet *nodeset,s32 ASSERT_ONLY(size))
{
	if(!s_DebugNodeSysInitted)
		Init();

	if(!s_DebugNodeSysInitted)
		return false;

	Assert(sizeof(NoteNodeSet) == (size/* * 4*/));

	char buffer[2048];
	
	//delete the current set of data... naive but I'm not fussed at the mo
	
	DeleteSet(user,index);

	//add the set in...

	formatf(buffer,"/save_set, %d, %d, %d, '%s', %d, %d",	index,
															user,
															//nodeset->nodes_size,
															nodeset->name.c_str(),
															nodeset->creation_number,
															nodeset->node_creation_number);

	//and each node of the set
	for(s32 i=0;i<nodeset->node_creation_number;i++)
	{
		char tempbuffer[2048];
		NoteNode* node = &nodeset->nodes[i];

		formatf(tempbuffer,", %d, %f, %f, %f, '%s', %d, %d",	node->type,
																node->coord.x,
																node->coord.y,
																node->coord.z,
																node->label.c_str(),
																node->creation_number,
																node->link);

		strcat(buffer,tempbuffer);
	}

	strcat(buffer,"\n");
	netTcpResult res = netTcp::SendBuffer(s_DebugNodesSocket,buffer,ustrlen(buffer),60);

	if(res != NETTCP_RESULT_OK)
		return false;

	return true;
}

bool DebugNodeSys::DeleteSet(s32 user,s32 index)
{
	if(!s_DebugNodeSysInitted)
		Init();

	if(!s_DebugNodeSysInitted)
		return false;

	char buffer[2048];
	formatf(buffer,"/delete_set, %d, %d\n",user,index);
	netTcpResult res = netTcp::SendBuffer(s_DebugNodesSocket,buffer,ustrlen(buffer),60);

	if(res != NETTCP_RESULT_OK)
		return false;

	return true;
}

void DebugNodeSys::SetupScriptCommands()
{
	SCR_REGISTER_UNUSED(NODEVIEWER_INIT,0xae9feb2ae7d2ae12,DebugNodeSys::Init);
	SCR_REGISTER_UNUSED(NODEVIEWER_SHUTDOWN,0x0fdeddb1a6c6a057,DebugNodeSys::Shutdown);
	SCR_REGISTER_UNUSED(NODEVIEWER_SAVE_SET,0x697c602bc8309a50,DebugNodeSys::SaveSetScript);
	SCR_REGISTER_UNUSED(NODEVIEWER_LOAD_SET,0xbd61a475d391c1ff,DebugNodeSys::LoadSetScript);
	SCR_REGISTER_UNUSED(NODEVIEWER_DELETE_SET,0x03468203efc58615,DebugNodeSys::DeleteSet);
}


#endif // #if __BANK
