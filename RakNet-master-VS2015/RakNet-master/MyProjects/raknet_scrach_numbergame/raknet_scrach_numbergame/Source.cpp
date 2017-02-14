//Raknet project built from scratch
//p2p number guessing game

#include "BitStream.h"
#include "ConnectionGraph2.h"
#include "FullyConnectedMesh2.h"
#include "Gets.h"
#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"
#include "ReadyEvent.h"
#include <iostream>
#include <thread>

using namespace RakNet;

//**************************************************//
//			RAKNET CLASS DECLARATION				//
//													//
//**************************************************//
RakPeerInterface* g_rakPeerInterface = nullptr;
ReadyEvent g_readyEventPlugin;

// These two plugins are just to automatically create a fully connected mesh so I don't have to call connect more than once
FullyConnectedMesh2 g_fcm2;
ConnectionGraph2 g_cg2;

unsigned short startingPort = 6500;

//**************************************************//
//			GAME SPECIFIC DECLARATION				//
//													//
//**************************************************//

bool g_isRunning = true;
bool g_isGameRunning = false;
bool g_choseNum = false;
int myNum = 0;
int myGuesses = 0;

enum {
	ID_GB3_NUM_CORRECT,
	ID_GB3_NUM_GUESS = ID_USER_PACKET_ENUM,
};

enum ReadyEvents {
	RE_PLAYER_READY = 0,
	RE_GAME_OVER,
	RE_RESTART
};

void NumberGuessing() {
	char numberGuess[32];
	char yourNum[32];
	while (g_isRunning) {
		if (g_isGameRunning) {
			if (!g_choseNum) {
				printf("Choose your number for the other player to guess [0-20]\n");
				Gets(yourNum, sizeof(yourNum));
				myNum = atoi(yourNum);
				if (myNum <= 20) {
					g_choseNum = true;
					BitStream bs;
					bs.Write((unsigned char)ID_GB3_NUM_CORRECT);
				} else {
					continue;
				}
			}

			printf("Guess the other players number between 0 and 20\n");
			Gets(numberGuess, sizeof(numberGuess));
			BitStream bs;
			bs.Write((unsigned char)ID_GB3_NUM_GUESS);
			int numberGuessNum = atoi(numberGuess);
			myGuesses++;
			bs.Write(numberGuessNum);
			bs.Write(myGuesses);
			g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
		}
	}
}

void PacketListener() {
	printf("Listening for packets...\n");
	Packet* packet;

	while (g_isRunning) {
		for (packet = g_rakPeerInterface->Receive();
		packet != nullptr;
			g_rakPeerInterface->DeallocatePacket(packet),
			packet = g_rakPeerInterface->Receive()) {
			unsigned char packetIdentifier = packet->data[0];
			printf("Packet received!!!! %i\n", packetIdentifier);
			switch (packetIdentifier) {
			case ID_CONNECTION_REQUEST_ACCEPTED:
				printf("ID_CONNECTION_REQUEST_ACCEPTED\n");
				g_readyEventPlugin.AddToWaitList(RE_PLAYER_READY, packet->guid);
				g_readyEventPlugin.AddToWaitList(RE_GAME_OVER, packet->guid);
				break;
			case ID_NEW_INCOMING_CONNECTION:
				printf("ID_NEW_INCOMING_CONNECTION\n");
				g_readyEventPlugin.AddToWaitList(RE_PLAYER_READY, packet->guid);
				g_readyEventPlugin.AddToWaitList(RE_GAME_OVER, packet->guid);
				break;
			case ID_READY_EVENT_SET:
				printf("ID_READY_EVENT_SET\n");
				break;
			case ID_READY_EVENT_UNSET:
				printf("ID_READY_EVENT_UNSET\n");
				break;
			case ID_READY_EVENT_ALL_SET:
				printf("ID_READY_EVENT_ALL_SET\n");
				//all the players are ready for the time of their life
				{
					BitStream bs(packet->data, packet->length, false);
					bs.IgnoreBytes(sizeof(MessageID));
					int readyEvent;
					bs.Read(readyEvent);
					if (readyEvent == RE_PLAYER_READY) {
						if (!g_isGameRunning) {
							system("cls");
							g_isGameRunning = true;
						}
					}
				}
				break;
			case ID_GB3_NUM_GUESS:
			{
				BitStream bs(packet->data, packet->length, false);
				bs.IgnoreBytes(sizeof(MessageID));
				int userGuess;
				bs.Read(userGuess);
				int userGuessNum;
				bs.Read(userGuessNum);
				printf("The Other User guessed: %i\n", userGuess);
				if (userGuess == myNum) {
					printf("The Other was correct! You put: %i, They put %i\n it took them %i guesses", userGuess, myNum, userGuessNum);
					g_isGameRunning = false;
				}
			}
			break;
			default:
				break;
			}
		}
	}
}

int main() {
	g_rakPeerInterface = RakNet::RakPeerInterface::GetInstance();

	g_rakPeerInterface->AttachPlugin(&g_readyEventPlugin);
	g_rakPeerInterface->AttachPlugin(&g_fcm2);
	g_rakPeerInterface->AttachPlugin(&g_cg2);

	g_fcm2.SetAutoparticipateConnections(true);
	g_fcm2.SetConnectOnNewRemoteConnection(true, "");
	g_cg2.SetAutoProcessNewConnections(true);

	while (IRNS2_Berkley::IsPortInUse(startingPort, "127.0.0.1", AF_INET, SOCK_DGRAM) == true) {
		startingPort++;
	}

	SocketDescriptor socket(startingPort, "127.0.0.1");
	unsigned int maxConnections = 4;
	StartupResult result = g_rakPeerInterface->Startup(maxConnections, &socket, 1);
	g_rakPeerInterface->SetMaximumIncomingConnections(maxConnections);
	if (result == RAKNET_STARTED) {
		printf("Raknet has started on port: %i\n", startingPort);
	} else {
		printf("something went terribly wrong\n");
		system("pause");
		exit(0);
	}

	std::thread packetListener(PacketListener);
	std::thread numberGuesser(NumberGuessing);

	char userInput[32];
	printf("would you like to connect to a port (y)/(n)?\n");
	Gets(userInput, sizeof(userInput));

	if (userInput[0] == 'y' || userInput[0] == 'Y') {
		printf("type in a port you would like to connect to or don't, it's ip to you\n");
		char userInput[2048];
		Gets(userInput, sizeof(userInput));
		int userPort = atoi(userInput);
		ConnectionAttemptResult carResult = g_rakPeerInterface->Connect("127.0.0.1", userPort, nullptr, 0);
		if (carResult == CONNECTION_ATTEMPT_STARTED) {
			printf("connection attempt starting!!!\n");
		} else {
			printf("connection attempt failed\n");
			system("pause");
			exit(0);
		}
	}

	//register ourselves to the ready event plugin
	g_readyEventPlugin.AddToWaitList(RE_PLAYER_READY, g_rakPeerInterface->GetMyGUID());
	g_readyEventPlugin.AddToWaitList(RE_GAME_OVER, g_rakPeerInterface->GetMyGUID());


	printf("are you ready to start the game? (y)/(n)?\n");
	Gets(userInput, sizeof(userInput));
	if (userInput[0] == 'y' || userInput[0] == 'Y') {
		g_readyEventPlugin.SetEvent(RE_PLAYER_READY, true);
	} else {
		g_isRunning = false;
	}

	printf("waiting for players...\n");

	//this line makes sure that the return 0 doesn't execute until the 
	//number guesser thread is finished
	numberGuesser.join();
	return 0;
}