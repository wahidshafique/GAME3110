#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include <thread>
#include <iostream>
#include <BitStream.h>
#include "Gets.h"

using namespace RakNet;
using namespace std;

const char* HOST = "127.0.0.1";
short START_PORT = 6500;
RakPeerInterface* g_rakPeerInterface = nullptr;
bool g_isRunning = true;
bool g_isGameRunning = false;

enum {
	ID_GB3_NUM_GUESS = ID_USER_PACKET_ENUM,
};

void numberGuessing() {
	char numberGuess[32];
	while (true) {
		if (g_isGameRunning) {
			printf("Guess a number, any number, the other number, your other number \n");
			Gets(numberGuess, sizeof(numberGuess));
			int numberGuessednum = atoi(numberGuess);
			BitStream bs;
			bs.Write((unsigned char)ID_GB3_NUM_GUESS);
			g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
		}
	}
}

void packetListener() {
	printf("listening for packets..");
	Packet* packet;
	while (g_isRunning) {
		for (packet = g_rakPeerInterface->Receive(); packet != nullptr;
		g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive()) {
			unsigned char packetIdentifier = packet->data[0];
			printf("packet received %i\n", packet->data[0]);
			switch (packetIdentifier) {
			case ID_CONNECTION_REQUEST_ACCEPTED:
				printf("connections accepted \n");
				g_isGameRunning = true;
				break;
			case ID_NEW_INCOMING_CONNECTION:
				printf("new connection");
				g_isGameRunning = true;
				break;
			case ID_GB3_NUM_GUESS:
			{
				BitStream bs(packet->data, sizeof(packet->data), false);
				bs.IgnoreBytes(sizeof(DefaultMessageIDTypes));
				int userGuess;
				bs.Read(userGuess);
				printf("User guessed: %i\n", userGuess);
			}
			//
			default:
				break;
			}
		}
	}
}

int main() {
	g_rakPeerInterface = RakPeerInterface::GetInstance();

	while (IRNS2_Berkley::IsPortInUse(START_PORT, HOST, AF_INET, SOCK_DGRAM) == true) {
		START_PORT++;
	}

	SocketDescriptor socket(START_PORT, HOST);
	unsigned int maxConnections = 4;
	StartupResult result = g_rakPeerInterface->Startup(maxConnections, &socket, 1);
	g_rakPeerInterface->SetMaximumIncomingConnections(maxConnections);
	if (result == RAKNET_STARTED) {
		printf("Racknet has started on port: %i\n", START_PORT);
	} else {
		printf("error");
		system("pause");
		exit(0);
	}
	thread packetListener(packetListener);
	thread numberGuesser(numberGuessing);

	printf("what port would you like to connect to \n");
	char userInput[2048];
	Gets(userInput, sizeof(userInput));
	int userPort = atoi(userInput);
	ConnectionAttemptResult r = g_rakPeerInterface->Connect(HOST, userPort, nullptr, 0);
	if (r == CONNECTION_ATTEMPT_STARTED) {
		printf("connect attempt started %i \n", userPort);
	} else {
		printf("error \n");
		system("pause");
		exit(0);
	}
	while (g_isRunning) {

	}
	g_isRunning = false;
	return 0;
}