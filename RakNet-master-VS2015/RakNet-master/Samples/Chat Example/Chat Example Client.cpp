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

enum {
	ID_GB3_CHAT = ID_USER_PACKET_ENUM,
	ID_GB3_PLAYER_POSITION,
};

struct SPlayerPos {
	float _x;
	float _y;
	float _z;
};

// We copy this from Multiplayer.cpp to keep things all in one file for this example
unsigned char GetPacketIdentifier(RakNet::Packet *p);
unsigned int CheckForUserCommands(char* message);
void UpdatePackets();

RakNet::RakPeerInterface *g_rakPeerInterface;
bool g_isServer = false;

RakNet::SystemAddress g_sendAddress = RakNet::UNASSIGNED_SYSTEM_ADDRESS;

const unsigned int isQuit = 1;
const unsigned int isContinue = 2;

// Holds packets
RakNet::Packet* packet;

int main(void) {
	// Pointers to the interfaces of our server and client.
	// Note we can easily have both in the same program
	g_rakPeerInterface = RakNet::RakPeerInterface::GetInstance();
	g_rakPeerInterface->SetTimeoutTime(30000, RakNet::UNASSIGNED_SYSTEM_ADDRESS);

	char userInput[10];
	printf("Welcome to the the DRakNet.\n");
	printf("Would you like to be a Server(s) or Client(c) today?.\n");
	Gets(userInput, sizeof(userInput));
	g_isServer = userInput[0] == 's' || userInput[0] == 'S';
	printf("Thank you\n");


	// A server
	// Holds user data
	char portstring[30];
	puts("Enter the port to listen on");
	Gets(portstring, sizeof(portstring));
	if (portstring[0] == 0)
		strcpy(portstring, "1234");

	puts("Setting up socket.");

	// I am creating two socketDesciptors, to create two sockets. One using IPV6 and the other IPV4
	RakNet::SocketDescriptor socketDescriptors[2];
	socketDescriptors[0].port = atoi(portstring);
	socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4
	socketDescriptors[1].port = atoi(portstring);
	socketDescriptors[1].socketFamily = AF_INET6; // Test out IPV6

	unsigned int maxConnections = 1;
	if (g_isServer) {
		puts("What is the Max connections you would like?\n");
		Gets(userInput, sizeof(userInput));
		maxConnections = atoi(userInput);
	}

	bool isSuccess = g_rakPeerInterface->Startup(maxConnections, socketDescriptors, 2) == RakNet::RAKNET_STARTED;

	if (g_isServer) {
		g_rakPeerInterface->SetMaximumIncomingConnections(maxConnections);
	}

	if (!isSuccess) {
		printf("Failed to start dual IPV4 and IPV6 ports. Trying IPV4 only.\n");

		// Try again, but leave out IPV6
		isSuccess = g_rakPeerInterface->Startup(maxConnections, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
		if (!isSuccess) {
			puts("Server failed to start.");
			puts("Would you like to try again? Y or N \n");
			Gets(userInput, sizeof(userInput));
			exit(1);
		}
	}

	g_rakPeerInterface->SetOccasionalPing(true);
	g_rakPeerInterface->SetUnreliableTimeout(1000);

	if (!g_isServer) {
		char ip[64], serverPort[30];

		puts("Enter IP to connect to");
		Gets(ip, sizeof(ip));
		g_rakPeerInterface->AllowConnectionResponseIPMigration(false);
		if (ip[0] == 0)
			strcpy(ip, "127.0.0.1");

		puts("Enter the port to connect to");
		Gets(serverPort, sizeof(serverPort));
		if (serverPort[0] == 0)
			strcpy(serverPort, "1234");

		RakNet::ConnectionAttemptResult car = g_rakPeerInterface->Connect(ip, atoi(serverPort), nullptr, 0);
		RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
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

	printf("\nMy GUID is %s\n", g_rakPeerInterface->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS).ToString());

	if (g_isServer) {
		puts("'quit' to quit.'ping' to ping.\n'pingip' to ping an ip address\n'ban' to ban an IP from connecting.\n'kick to kick the first connected player.\nType to talk.");
	} else {
		puts("'quit' to quit.'ping' to ping.\n'pingip' to ping an ip address\nType to talk.");
	}



	// GetPacketIdentifier returns this
	unsigned char packetIdentifier;

	char message[2048];
	char messageWithIdentifier[2049];
	// Loop for input
	while (1) {

		// This sleep keeps RakNet responsive
		RakSleep(30);

		//Processing User Input
		if (_kbhit()) {
			// Notice what is not here: something to keep our network running.  It's
			// fine to block on gets or anything we want
			// Because the network engine was painstakingly written using threads.
			Gets(message, sizeof(message));

			unsigned int result = CheckForUserCommands(message);
			if (result == isQuit) {
				break;
			} else if (result == isContinue) {
				continue;
			}

			messageWithIdentifier[0] = ID_GB3_CHAT;
			messageWithIdentifier[1] = '\0';
			strncat(messageWithIdentifier, message, sizeof(message));

			//printf("Message with ID: %s\n", messageWithIdentifier);
			// Message now holds what we want to broadcast
			//char message2[2048];

			// Append Server: to the message so clients know that it ORIGINATED from the server
			// All messages to all clients come from the server either directly or by being
			// relayed from other clients
			/*if (g_isServer)
			{
			message2[0] = 0;
			const static char prefix[] = "Server: ";
			strncpy(message2, prefix, sizeof(message2));
			strncat(message2, message, sizeof(message2) - strlen(prefix) - 1);
			}*/


			// message2 is the data to send
			// strlen(message2)+1 is to send the null terminator
			// HIGH_PRIORITY doesn't actually matter here because we don't use any other priority
			// RELIABLE_ORDERED means make sure the message arrives in the right order
			// We arbitrarily pick 0 for the ordering stream
			// RakNet::UNASSIGNED_SYSTEM_ADDRESS means don't exclude anyone from the broadcast
			// true means broadcast the message to everyone connected
			if (g_isServer) {
				g_rakPeerInterface->Send(messageWithIdentifier, (const int)strlen(messageWithIdentifier) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
			} else {
				g_rakPeerInterface->Send(messageWithIdentifier, (const int)strlen(messageWithIdentifier) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_sendAddress, false);
			}
		}
		//End Processing User Input

		UpdatePackets();
	}

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

unsigned int CheckForUserCommands(char* message) {
	if (strcmp(message, "quit") == 0) {
		puts("Quitting.");
		return isQuit;
	}
	if (strcmp(message, "playerpos") == 0) {
		puts("Sending Player Position \n");
		SPlayerPos *playerPos = new SPlayerPos();
		playerPos->_x = 6.0f;
		playerPos->_y = 8.0f;
		playerPos->_z = 9.0f;
		memcpy(message, playerPos, sizeof(SPlayerPos));
	}

	if (g_isServer) {
		if (strcmp(message, "ban") == 0) {
			printf("Enter IP to ban.  You can use * as a wildcard\n");
			Gets(message, sizeof(message));
			g_rakPeerInterface->AddToBanList(message);
			printf("IP %s added to ban list.\n", message);

			return isContinue;
		}
	}

	if (strcmp(message, "getconnectionlist") == 0) {
		RakNet::SystemAddress systems[10];
		unsigned short numConnections = 10;
		g_rakPeerInterface->GetConnectionList((RakNet::SystemAddress*) &systems, &numConnections);
		for (int i = 0; i < numConnections; i++) {
			printf("%i. %s\n", i + 1, systems[i].ToString(true));
		}
		return isContinue;
	}

	return 0;
}

void UpdatePackets() {
	// Get a packet from either the server or the client
	for (packet = g_rakPeerInterface->Receive(); packet; g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive()) {
		// Check if this is a network message packet
		switch (GetPacketIdentifier(packet)) {
		case ID_DISCONNECTION_NOTIFICATION:
			// Connection lost normally
			printf("ID_DISCONNECTION_NOTIFICATION from %s\n", packet->systemAddress.ToString(true));;
			break;


		case ID_NEW_INCOMING_CONNECTION:
			// Somebody connected.  We have their IP now
			printf("ID_NEW_INCOMING_CONNECTION from %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());

			printf("Remote internal IDs:\n");
			for (int index = 0; index < MAXIMUM_NUMBER_OF_INTERNAL_IDS; index++) {
				RakNet::SystemAddress internalId = g_rakPeerInterface->GetInternalID(packet->systemAddress, index);
				if (internalId != RakNet::UNASSIGNED_SYSTEM_ADDRESS) {
					printf("%i. %s\n", index + 1, internalId.ToString(true));
				}
			}

			break;
		case ID_CONNECTION_REQUEST_ACCEPTED:
			printf("ID_CONNECTION_REQUEST_ACCEPTED\n");
			if (!g_isServer) {
				g_sendAddress = packet->systemAddress;
			}
			break;
		case ID_INCOMPATIBLE_PROTOCOL_VERSION:
			printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
			break;

		case ID_CONNECTED_PING:
		case ID_UNCONNECTED_PING:
			printf("Ping from %s\n", packet->systemAddress.ToString(true));
			break;

		case ID_CONNECTION_LOST:
			// Couldn't deliver a reliable packet - i.e. the other system was abnormally
			// terminated
			printf("ID_CONNECTION_LOST from %s\n", packet->systemAddress.ToString(true));;
			break;

		case ID_GB3_CHAT:
		{
			printf("ID_GB3_CHAT\n");
			unsigned char* tdata = packet->data;
			tdata++;
			printf("%s\n", tdata);
		}
		break;
		default:
			// The server knows the static data of all clients, so we can prefix the message
			// With the name data
			printf("Unsupported packet identifier %i\n", packet->data[0]);
			//printf("%s\n", packet->data);

			// Relay the message.  We prefix the name for other clients.  This demonstrates
			// That messages can be changed on the server before being broadcast
			// Sending is the same as before
			/*sprintf(message, "%s", packet->data);
			if (g_isServer)
			{
			g_rakPeerInterface->Send(message, (const int)strlen(message) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);
			}*/

			break;
		}
	}
}