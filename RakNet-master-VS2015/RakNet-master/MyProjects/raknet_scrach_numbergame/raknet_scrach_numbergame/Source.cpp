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
const int maxNum = 20;
const int HIGH_RAND = 20;
const int LOW_RAND = 1;
bool g_otherActive = false;
bool g_isRunning = true;
bool g_everyoneReady = false;
bool g_isGameRunning = false;
int g_randomNum = 0;
char myUsername[32] = "";
int myGuesses = 0;
enum {
	ID_GB3_NUM_GUESS = ID_USER_PACKET_ENUM,
	ID_GB3_NUM_CORRECT
};

enum ReadyEvents {
	RE_PLAYER_READY = 0,
	RE_GAME_OVER,
	RE_RESTART,
};

void PacketListener();
void NumberGuessing();

int main() {
	srand(time(NULL));

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
	unsigned int maxConnections = 6;
	StartupResult result = g_rakPeerInterface->Startup(maxConnections, &socket, 1);
	g_rakPeerInterface->SetMaximumIncomingConnections(maxConnections);

	if (result == RAKNET_STARTED) {
		printf("Raknet has started on port: %i\n", startingPort);
	} else {
		printf("Raknet will not start\n");
		system("pause");
		exit(0);
	}

	std::thread packetListener(PacketListener);


	printf("type in a port you would like to connect to, type 'n' to start a new game\n");
	char userInput[32];
	Gets(userInput, sizeof(userInput));
	if (userInput[0] == 'n') {
		g_randomNum = rand() % HIGH_RAND + LOW_RAND;
		printf("DEBUG PURPOSES: %i \n", g_randomNum);
	} else {
		int userPort = atoi(userInput);
		ConnectionAttemptResult carResult = g_rakPeerInterface->Connect("127.0.0.1", userPort, nullptr, 0);
		if (carResult != CONNECTION_ATTEMPT_STARTED) {
			printf("connection attempt failed\n");
			system("pause");
			exit(0);
		}
	}

	//register ourselves to the ready event plugin
	g_readyEventPlugin.AddToWaitList(RE_PLAYER_READY, g_rakPeerInterface->GetMyGUID());
	g_readyEventPlugin.AddToWaitList(RE_GAME_OVER, g_rakPeerInterface->GetMyGUID());
	g_readyEventPlugin.AddToWaitList(RE_RESTART, g_rakPeerInterface->GetMyGUID());

	printf("are you ready to start the game? (y)/(n)?\n");
	Gets(userInput, sizeof(userInput));
	if (userInput[0] == 'y' || userInput[0] == 'Y') {
		printf("give yourself a username? \n");
		Gets(myUsername, sizeof(myUsername));
		g_readyEventPlugin.SetEvent(RE_PLAYER_READY, true);
		std::thread numberGuesser(NumberGuessing);
		numberGuesser.join();
	} else {
		g_isRunning = false;
	}

	//this line makes sure that the return 0 doesn't execute until the 
	//number guesser thread is finished
	packetListener.join();
	return 0;
}

void NumberGuessing() {
	srand(time(NULL));
	char numberGuess[32];
	while (g_isRunning) {
		if (g_isGameRunning) {
			printf("Guess the secret number between 1 and 20 to win! \n");
			Gets(numberGuess, sizeof(numberGuess));
			BitStream bs;

			if (g_randomNum < 1) {
				g_randomNum = rand() % HIGH_RAND + LOW_RAND;
			}
			int numberGuessNum = atoi(numberGuess);
			if (numberGuessNum != g_randomNum) {
				bs.Write((unsigned char)ID_GB3_NUM_GUESS);
			} else {
				printf("You win! You guessed %i, and the answer was %i ! \n", numberGuessNum, g_randomNum);
				bs.Write((unsigned char)ID_GB3_NUM_CORRECT);
			}
			myGuesses++;
			bs.Write(myUsername);
			bs.Write(numberGuessNum);
			bs.Write(myGuesses);
			bs.Write(g_randomNum);
			g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
			if (numberGuessNum == g_randomNum) {
				g_isGameRunning = false;
				g_readyEventPlugin.SetEvent(RE_PLAYER_READY, false);
				g_readyEventPlugin.SetEvent(RE_GAME_OVER, true);

			}
		}
	}
}


void PacketListener() {
	//printf("Listening for packets...\n");

	Packet* packet;

	while (g_isRunning) {
		for (packet = g_rakPeerInterface->Receive();
		packet != nullptr;
			g_rakPeerInterface->DeallocatePacket(packet),
			packet = g_rakPeerInterface->Receive()) {
			unsigned char packetIdentifier = packet->data[0];
			//printf("Packet received!!!! %i\n", packetIdentifier);
			switch (packetIdentifier) {
			case ID_CONNECTION_REQUEST_ACCEPTED:
				printf("System: New player paired! \n");
				g_otherActive = true;
				g_readyEventPlugin.AddToWaitList(RE_PLAYER_READY, packet->guid);
				g_readyEventPlugin.AddToWaitList(RE_GAME_OVER, packet->guid);
				g_readyEventPlugin.AddToWaitList(RE_RESTART, packet->guid);
				break;
			case ID_NEW_INCOMING_CONNECTION:
				printf("A new player popped in to your game! lets wait for them to be ready \n");
				g_otherActive = true;
				g_readyEventPlugin.AddToWaitList(RE_PLAYER_READY, packet->guid);
				g_readyEventPlugin.AddToWaitList(RE_GAME_OVER, packet->guid);
				g_readyEventPlugin.AddToWaitList(RE_RESTART, packet->guid);
				break;

			case ID_ALREADY_CONNECTED:
				printf("already connected");
				g_readyEventPlugin.AddToWaitList(RE_PLAYER_READY, packet->guid);
				g_readyEventPlugin.AddToWaitList(RE_GAME_OVER, packet->guid);
				g_readyEventPlugin.AddToWaitList(RE_RESTART, packet->guid);
			case ID_READY_EVENT_SET: {
				BitStream bs(packet->data, packet->length, false);

				bs.PrintBits();

				bs.IgnoreBytes(sizeof(MessageID));
				int readyEvent;
				bs.Read(readyEvent);

				printf("%s is ready! \n", "user");
				if (readyEvent == RE_GAME_OVER) {
					printf("GAME OVER!!! \n");
					printf("Thanks for playing \n");
					char endInput[32];
					printf("\n are you ready to start another game? enter a value and press enter \n \
						do this twice so we know you're really sure..oh and both players have to be ready for this to work(y)/(n)?\n");
					Gets(endInput, sizeof(endInput));
					if (endInput[0] == 'y' || endInput[0] == 'Y') {
						g_randomNum = 0;
						myGuesses = 0;
						g_readyEventPlugin.SetEvent(RE_RESTART, true);
					} else {
						exit(0);
					}
				}
				if (readyEvent == RE_RESTART) {
					g_otherActive = true;
					g_readyEventPlugin.SetEvent(RE_PLAYER_READY, true);
				}
			}
									 break;
			case ID_READY_EVENT_UNSET:
				printf("ID_READY_EVENT_UNSET\n");
				g_otherActive = false;
				break;

			case ID_REMOTE_CONNECTION_LOST:
				printf("someone left... \n");
				break;

			case ID_READY_EVENT_ALL_SET:
				//printf("You are good to go!\n");
				//all the players are ready for the time of their life
			{
				BitStream bs(packet->data, packet->length, false);
				bs.IgnoreBytes(sizeof(MessageID));
				int readyEvent;
				bs.Read(readyEvent);
				if (readyEvent == RE_PLAYER_READY) {
					//ensure that there is atleast one other active player before starting the game
					if (!g_isGameRunning && g_otherActive) {
						system("cls");
						g_isGameRunning = true;
					} else {
						//printf("");
					}
					if (!g_otherActive) {
						printf("Its just you right now...the game will start when atleast 1 other player enters \n");
					}
				}
			}
			break;
			case ID_GB3_NUM_GUESS:
			{
				if (g_isGameRunning) {
					BitStream bs(packet->data, packet->length, false);
					bs.IgnoreBytes(sizeof(MessageID));
					char username[sizeof(myUsername)];
					bs.Read(username);
					int userGuess;
					bs.Read(userGuess);
					int userNumGuess;
					bs.Read(userNumGuess);
					int userRand;
					bs.Read(userRand);
					g_randomNum = userRand;
					printf("DEBUG: THE GLOBAL ANSWER IS %i \n", userRand);
					printf("%i was guessed by %s, they've guessed %i times so far \n", userGuess, username, userNumGuess);
				}
			}
			break;
			case ID_GB3_NUM_CORRECT:
			{
				BitStream bs(packet->data, packet->length, false);
				bs.IgnoreBytes(sizeof(MessageID));
				char username[sizeof(myUsername)];
				bs.Read(username);
				int userGuess;
				bs.Read(userGuess);
				int userNumGuess;
				bs.Read(userNumGuess);
				printf("%s is the winner!!! They guessed %i and it took them %i tries \n", username, userGuess, userNumGuess);
				g_isGameRunning = false;
			}
			break;
			default:
				break;
			}
		}
	}
}