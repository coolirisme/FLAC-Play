#ifndef _PACKET_QUEUE_
#define _PACKET_QUEUE_

typedef struct PacketNode
{
	char *packet;
	int packet_size;
	struct PacketNode *next;
}PacketNode;

typedef struct PacketList
{
	PacketNode *first;
	PacketNode *last;
	int num_packets;
}PacketList;

void PacketQueueInit(PacketList list);
void PacketQueuePut(PacketList *list,PacketNode *packet);
PacketNode *PacketQueueGet(PacketList *list);

#endif
