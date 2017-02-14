/*
*  Copyright (c) 2014, Oculus VR, Inc.
*  All rights reserved.
*
*  This source code is licensed under the BSD-style license found in the
*  LICENSE file in the root directory of this source tree. An additional grant
*  of patent rights can be found in the PATENTS file in the same directory.
*
*/

// ----------------------------------------------------------------------
// RakNet version 1.0
// Filename ChatExample.cpp
// Very basic chat engine example
// ----------------------------------------------------------------------

#include "MessageIdentifiers.h"

#include "RakPeerInterface.h"
#include "RakNetStatistics.h"
#include "RakNetTypes.h"
#include "BitStream.h"
#include "RakSleep.h"
#include "PacketLogger.h"
#include <assert.h>
#include <cstdio>
#include <cstring>
#include <stdlib.h>
#include "Kbhit.h"
#include <stdio.h>
#include <string.h>
#include "Gets.h"

// We copy this from Multiplayer.cpp to keep things all in one file for this example
unsigned char GetPacketIdentifier(RakNet::Packet *p);
int CheckForCommands(char* message);

RakNet::RakPeerInterface *g_rakPeerInterface;
bool g_isServer = false;
RakNet::SystemAddress g_serverAddress = RakNet::UNASSIGNED_SYSTEM_ADDRESS;

enum UserInputResult {
	UIR_BREAK,
	UIR_CONTINUE,
	UIR_POS,
	UIR_COUNT,
};

enum {
	ID_GB3_CHAT = ID_USER_PACKET_ENUM,
	ID_GB3_POS,
};

struct SPos {
	float x, y, z;
};


int main(void) {
	// Pointers to the interfaces of our server and client.
	// Note we can easily have both in the same program
	g_rakPeerInterface = RakNet::RakPeerInterface::GetInstance();
	g_rakPeerInterface->SetTimeoutTime(30000, RakNet::UNASSIGNED_SYSTEM_ADDRESS);


	// Holds user data
	char userInput[30];

	printf("This is a sample implementation of a text based chat server/client.\n");
	printf("What role would you like? Client(c) or Server(s)");
	Gets(userInput, sizeof(userInput));
	g_isServer = userInput[0] == 's' || userInput[0] == 'S';

	// A server
	puts("Enter the port to listen on");
	Gets(userInput, sizeof(userInput));
	if (userInput[0] == 0)
		strcpy(userInput, "1234");

	puts("Setting up socket");
	// 0 means we don't care about a connectionValidationInteger, and false
	// for low priority threads
	// I am creating two socketDesciptors, to create two sockets. One using IPV6 and the other IPV4
	RakNet::SocketDescriptor socketDescriptors[2];
	socketDescriptors[0].port = atoi(userInput);
	socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4 
	socketDescriptors[1].port = atoi(userInput);
	socketDescriptors[1].socketFamily = AF_INET6; // Test out IPV6 
	const unsigned int maxConnections = g_isServer ? 100 : 1;
	bool isSuccess = g_rakPeerInterface->Startup(maxConnections, socketDescriptors, 2) == RakNet::RAKNET_STARTED;

	if (g_isServer) {
		g_rakPeerInterface->SetMaximumIncomingConnections(maxConnections);
	}

	if (!isSuccess) {
		printf("Failed to start dual IPV4 and IPV6 ports. Trying IPV4 only.\n");

		// Try again, but leave out IPV6
		isSuccess = g_rakPeerInterface->Startup(maxConnections, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
		if (!isSuccess) {
			puts("failed to start.  Terminating.");
			exit(1);
		}
	}

	DataStructures::List< RakNet::RakNetSocket2* > sockets;
	g_rakPeerInterface->GetSockets(sockets);
	printf("Socket addresses used by RakNet:\n");
	for (unsigned int i = 0; i < sockets.Size(); i++) {
		printf("%i. %s\n", i + 1, sockets[i]->GetBoundAddress().ToString(true));
	}

	printf("\nMy IP addresses:\n");
	for (unsigned int i = 0; i < g_rakPeerInterface->GetNumberOfAddresses(); i++) {
		RakNet::SystemAddress sa = g_rakPeerInterface->GetInternalID(RakNet::UNASSIGNED_SYSTEM_ADDRESS, i);
		printf("%i. %s (LAN=%i)\n", i + 1, sa.ToString(false), sa.IsLANAddress());
	}

	if (!g_isServer) {
		puts("Enter IP to connect to");
		Gets(userInput, sizeof(userInput));
		g_rakPeerInterface->AllowConnectionResponseIPMigration(false);
		if (userInput[0] == 0)
			strcpy(userInput, "127.0.0.1");

		char port[10];
		puts("Enter the port to connect to");
		Gets(port, sizeof(port));
		if (port[0] == 0)
			strcpy(port, "1234");

		RakNet::ConnectionAttemptResult car = g_rakPeerInterface->Connect(userInput, atoi(port), nullptr, 0);
		RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
	}

	printf("\nMy GUID is %s\n", g_rakPeerInterface->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS).ToString());
	puts("'quit' to quit. 'stat' to show stats. 'ping' to ping.\n'pingip' to ping an ip address\n'ban' to ban an IP from connecting.\n'kick to kick the first connected player.\nType to talk.");

	char message[2046];
	char msgWithIdentifier[2048];
	// Holds packets
	RakNet::Packet* p;
	// GetPacketIdentifier returns this
	unsigned char packetIdentifier;
	// Loop for input
	while (1) {
		// This sleep keeps RakNet responsive
		RakSleep(30);
		if (_kbhit()) {
			Gets(message, sizeof(message));
			unsigned int result = CheckForCommands(message);
			if (result == UIR_BREAK) {
				//exit while loop, which in turn exits program
				break;
			} else if (result == UIR_CONTINUE) {
				//goes back to top of while loop
				continue;
			} else if (result == UIR_POS) {
				RakNet::BitStream bs;
				bs.Write((unsigned char)ID_GB3_POS);
				bs.Write(7.0f);

				if (g_isServer) {
					g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
				} else {
					g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
				}
				//continue;

				//SPos *pos = new SPos();
				//pos->x = 1.0f;
				//pos->y = 2.0f;
				//pos->z = 6.0f;

				//msgWithIdentifier[0] = ID_GB3_POS;
				//skip first byte
				//memcpy(&msgWithIdentifier[1], pos, 12);
				//msgWithIdentifier[1] = '\0';
				//strncat(msgWithIdentifier, message, sizeof(message));
			} else {
				// Notice what is not here: something to keep our network running.  It's
				// fine to block on gets or anything we want
				// Because the network engine was painstakingly written using threads.
				// Message now holds what we want to broadcast
				msgWithIdentifier[0] = ID_GB3_CHAT;
				msgWithIdentifier[1] = '\0';
				strncat(msgWithIdentifier, message, sizeof(message));

				if (g_isServer) {
					g_rakPeerInterface->Send(msgWithIdentifier, (const int)strlen(msgWithIdentifier) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
				} else {
					g_rakPeerInterface->Send(msgWithIdentifier, (const int)strlen(msgWithIdentifier) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
				}
			}



		}//end of kbhit(chat part)

		 // Get a packet from either the server or the client
		for (p = g_rakPeerInterface->Receive(); p != nullptr; g_rakPeerInterface->DeallocatePacket(p), p = g_rakPeerInterface->Receive()) {
			// We got a packet, get the identifier with our handy function
			packetIdentifier = GetPacketIdentifier(p);

			// Check if this is a network message packet
			switch (packetIdentifier) {

			case ID_CONNECTION_REQUEST_ACCEPTED:
				printf("ID_CONNECTION_REQUEST_ACCEPTED\n");
				g_serverAddress = p->systemAddress;
				break;
			case ID_DISCONNECTION_NOTIFICATION:
				// Connection lost normally
				printf("ID_DISCONNECTION_NOTIFICATION from %s\n", p->systemAddress.ToString(true));;
				break;


			case ID_NEW_INCOMING_CONNECTION:
				// Somebody connected.  We have their IP now
				printf("ID_NEW_INCOMING_CONNECTION from %s with GUID %s\n", p->systemAddress.ToString(true), p->guid.ToString());
				printf("Remote internal IDs:\n");
				for (int index = 0; index < MAXIMUM_NUMBER_OF_INTERNAL_IDS; index++) {
					RakNet::SystemAddress internalId = g_rakPeerInterface->GetInternalID(p->systemAddress, index);
					if (internalId != RakNet::UNASSIGNED_SYSTEM_ADDRESS) {
						printf("%i. %s\n", index + 1, internalId.ToString(true));
					}
				}
				break;

			case ID_INCOMPATIBLE_PROTOCOL_VERSION:
				printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
				break;

			case ID_CONNECTED_PING:
			case ID_UNCONNECTED_PING:
				printf("Ping from %s\n", p->systemAddress.ToString(true));
				break;

			case ID_CONNECTION_LOST:
				// Couldn't deliver a reliable packet - i.e. the other system was abnormally
				// terminated
				printf("ID_CONNECTION_LOST from %s\n", p->systemAddress.ToString(true));;
				break;
			case ID_GB3_CHAT:
			{
				//printf("ID_GB3_CHAT\n");
				unsigned char *pp = p->data;
				++pp;
				printf("%s\n", &p->data[1]);
				printf("%s\n", pp);
			}
			break;
			case ID_GB3_POS:
			{
				printf("ID_GB3_POS\n");
				RakNet::BitStream bs(p->data, p->length, false);
				bs.IgnoreBytes(sizeof(RakNet::MessageID));
				//bs.Write(ID_GB3_POS);
				float xPos;
				bs.Read(xPos);
				//temp pointer to char*
				//skip the first byte
				//go to the next byte in memory
				//++tempP;
				//SPos *pos = (SPos*)&p->data[1];
				//pos->x;
				printf("pos x: %f \n", xPos);
			}


			break;
			default:
				// The server knows the static data of all clients, so we can prefix the message
				// With the name data
				printf("%i\n", p->data[0]);

				// Relay the message.  We prefix the name for other clients.  This demonstrates
				// That messages can be changed on the server before being broadcast
				// Sending is the same as before
				/*sprintf(message, "%s", p->data);
				if (g_isServer)
				{
				g_rakPeerInterface->Send(message, (const int)strlen(message) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, p->systemAddress, true);
				}*/
				break;
			}

		}
	}//end of while

	g_rakPeerInterface->Shutdown(300);
	// We're done with the network
	RakNet::RakPeerInterface::DestroyInstance(g_rakPeerInterface);

	return 0;
}

// Copied from Multiplayer.cpp
// If the first byte is ID_TIMESTAMP, then we want the 5th byte
// Otherwise we want the 1st byte
unsigned char GetPacketIdentifier(RakNet::Packet *p) {
	if (p == 0)
		return 255;

	if ((unsigned char)p->data[0] == ID_TIMESTAMP) {
		RakAssert(p->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
		return (unsigned char)p->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
	} else
		return (unsigned char)p->data[0];
}

int CheckForCommands(char* message) {
	if (strcmp(message, "quit") == 0) {
		puts("Quitting.");
		return UIR_BREAK;
	}

	if (strcmp(message, "pingip") == 0) {
		char userInput[30];
		printf("Enter IP: ");
		Gets(message, sizeof(message));
		printf("Enter port: ");
		Gets(userInput, sizeof(userInput));
		if (userInput[0] == 0)
			strcpy(userInput, "1234");
		g_rakPeerInterface->Ping(message, atoi(userInput), false);

		return UIR_CONTINUE;
	}

	if (strcmp(message, "getconnectionlist") == 0) {
		RakNet::SystemAddress systems[10];
		unsigned short numConnections = 10;
		g_rakPeerInterface->GetConnectionList((RakNet::SystemAddress*) &systems, &numConnections);
		for (int i = 0; i < numConnections; i++) {
			printf("%i. %s\n", i + 1, systems[i].ToString(true));
		}
		return UIR_CONTINUE;
	}

	if (strcmp(message, "ban") == 0) {
		printf("Enter IP to ban.  You can use * as a wildcard\n");
		Gets(message, sizeof(message));
		g_rakPeerInterface->AddToBanList(message);
		printf("IP %s added to ban list.\n", message);
		return UIR_CONTINUE;
	}

	if (strcmp(message, "pos") == 0) {
		return UIR_POS;
	}

	return UIR_COUNT;
}