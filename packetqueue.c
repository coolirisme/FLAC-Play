#include <stdlib.h>
#include "packetqueue.h"

void PacketQueueInit(PacketList list)
{
	list.first=NULL;
	list.last=NULL;
	list.num_packets=0;
}

void PacketQueuePut(PacketList *list,PacketNode *packet)
{
	if(list->num_packets==0)
	{
		list->first=packet;
		list->last=packet;
		list->last->next=NULL;
		list->num_packets+=1;
	}
	else
	{
		list->last->next=packet;
		list->last=list->last->next;
		list->num_packets+=1;
	}
}

PacketNode *PacketQueueGet(PacketList *list)
{
	PacketNode *packet;
	if(list->num_packets==0)
	{
		packet=NULL;
		return packet;
	}
	packet=list->first;
	list->first=list->first->next;
	list->num_packets-=1;
	return packet;
}
