/*
	New
	Jared Hebert
	Andrew O'Brien
	EECE.4830
	Project Phase 1
	UDP Server
	code from https://www.binarytides.com/udp-socket-programming-in-winsock/ used
*/

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include<stdio.h>
#include <iostream>
#include<time.h>
#include<winsock2.h>
#include <WS2tcpip.h>


using namespace::std;

#pragma comment(lib,"ws2_32.lib") //Winsock Library

//global constants

#define BUFLEN 10000


//Adjust Corruption and loss variables here
//	|
//	|
//	|
//	|
//	|
// \ /
//	V

#define corruptionPercent 0.000
#define corruptionStrength 0.500
#define percentLost 0.000

#define ackcorruptionPercent 0.000
#define ackcorruptionStrength 0.100
#define ackpercentLost 0.000

#define errorCheckActive 1

//---------------------------------

char inStr[] = "Space_dress.bmp";
char outStr[] = "bmp_out.bmp";

unsigned short checksum(char* packetData, int size) {
	/*
		Take the sum of the 1024 bytes split up into 16 bit words ((first byte << 8 )| second byte + ... + 1024th Byte)
		The sum is stored into result and then returns is complement.
		Ex: packeData[1} has 1024 blocks which contains 1 byte in each block, this function adds those up together
			and takes it complement.
	*/
	unsigned short result = 0;
	int i;
	char tempData[2];

	for (i = 0; i < size; i += 2) {
		tempData[0] = packetData[i];
		tempData[1] = packetData[i + 1];
		result += (((unsigned short)tempData[0]) << 8) | (unsigned short)tempData[1];
	}

	//printf("Check val = %x\n", ~result);

	return ~result;
}

int Make_Packet(FILE** fpIn, char* packetData, unsigned short sequence) {
	/*
		updates packetData array to be the next 1KB in your defined "fpIn"
		returns 1 when the end of the file has been reached
		otherwise 0

		adds the checksum values in fields 1024-1025, and the sequence number in 1028-1029
	*/
	int freadReturn;
	int fileEnd = 0;


	freadReturn = fread(packetData, 1, 1024, *fpIn);
	if (freadReturn == 0) {
		fileEnd = 1;
	}

	unsigned short checksumVal = checksum(packetData, 1024);
	//printf("sent check = %x\n", checksumVal);

	packetData[1024] = (char)((checksumVal & 0xFF00) >> 8);
	packetData[1025] = (char)(checksumVal & 0x00FF);
	packetData[1028] = (char)((sequence & 0xFF00) >> 8);
	packetData[1029] = (char)(sequence & 0x00FF);

	return fileEnd;
}

unsigned short Make_Packet_Ack(unsigned short ack_num, char* packetData) {
	/*
		turns the ack num to be sent into a packed were bits 0-1 contain the ack value and bits 2-5 contain the checksum
		it calculates 2 seperate checksums becaue just one was causing some erroneous checks, screwing up the transmission

		returns the checksum value
	*/
	char temp_packet[2];
	int check_val;

	temp_packet[0] = (char)((ack_num & 0xFF00) >> 8);
	temp_packet[1] = (char)(ack_num & 0x00FF);

	int result = 0;
	int i;

	for (i = 0; i < 2; i++) {
		result += packetData[i];
	}

	check_val = ~result;

	packetData[0] = temp_packet[0];
	packetData[1] = temp_packet[1];
	packetData[2] = (char)((check_val & 0xFF000000) >> 24);
	packetData[3] = (char)((check_val & 0x00FF0000) >> 16);
	packetData[4] = (char)((check_val & 0x0000FF00) >> 8);
	packetData[5] = (char)(check_val & 0x000000FF);


	return check_val;
}

int Make_File(FILE** fpOut, char* packetData) {

	/*
		writes the data contained in packetData to the file defined "fpOut"
		returns 1 when no data was written
		otherwise 0
	*/

	int fwriteReturn;
	int fileEnd = 0;
	char dataBuf[1024];


	for (int i = 0; i < 1024; ++i) {
		dataBuf[i] = packetData[i];
	}

	fwriteReturn = fwrite(dataBuf, 1, 1024, *fpOut);
	if (fwriteReturn != 1024) {
		fileEnd = 1;
	}

	return fileEnd;
}

int Corruptor_Challenge(float challenge, float corruption_amount, char* packetData) {
	/*
		passes a pacekt of data through a challenge, difficulty determined by the value of challenge in a decimal representation of the percent
		difficulty, 3 sig-figs.
		corruption_ammount is the decimal representation of the ammount of corruption each corrupted packet will expirence.
		if the challenge hits, the packet will be randomly disrupted. if not it will be unaffected.
		returns 1 if challenge hits, otherwise 0
	*/

	int random_num, challenge_result;

	random_num = rand() % 1000;

	if (random_num < (challenge * 1000)) {
		// challenge hits

		for (int i = 0; i < sizeof(packetData); ++i) {
			int r = rand() % 1000;
			if (r < (corruption_amount * 1000)) {
				packetData[i] = packetData[i] ^ 0xFF;
			}
		}

		challenge_result = 1;
	}
	else {
		//challenge misses

		challenge_result = 0;
	}

	return challenge_result;
}

int Packet_Loss(float challenge, char* packetData) {
	/*
		used to simulate packet loss, replaces the packet with NULL if the challenge hits.
		percentage of packets lost is determined by the value in challenge, accurate down to 3 sig-figs

		returns 1 if packet is lost, else 0
	*/

	int random_num, isLost;

	random_num = rand() % 1000;

	if (random_num < (challenge * 1000)) {
		// challenge hits

		for (int i = 0; i < sizeof(packetData); ++i) {

			packetData[i] = 0;
		}

		isLost = 1;
	}

	else {
		// challenge misses

		isLost = 0;
	}

	return isLost;
}

int main(int argc, char* argv[])
{
	WSADATA wsa;
	SOCKET server;
	sockaddr_in server_addr, client;
	hostent* localHost;
	int slen, recv_len;
	char* localIP;
	char buf[BUFLEN];
	char ack[6];
	char done_message[20] = "file_send_done";
	unsigned short checkVal0, checkVal1, check_ack;
	char packetData[1030];
	unsigned short sequenceExpected = 0;
	unsigned short last_ack;
	unsigned short sequenceRecv;

	FILE* fpOut;

	slen = sizeof(client);

	srand(time(NULL));		//Initialize random numbers

	//init winsock library
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		return 1;
	}

	printf("Initialised.\n");

	//create socket
	if ((server = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d", WSAGetLastError());
	}

	printf("Socket created.\n");

	//cout << "Local IP: " << inet_addr("127.0.0.1") << endl;

	//define server address
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(8888);

	//bind socket to defined address
	if (bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {

		printf("Bind failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);

	}

	printf("Bind Done\n");

	fpOut = fopen(outStr, "wb");
	if (fpOut == NULL) {
		printf("Out file failed to open :( \n");
		return 1;
	}

	int fileEnd = 0;
	last_ack = 0;

	//keep listening for data
	while (fileEnd == 0)
	{


		//printf("Waiting for data...");
		fflush(stdout);

		//clear the buffer by filling null, it might have previously received data
		memset(buf, '\0', BUFLEN);

		//try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(server, buf, BUFLEN, 0, (struct sockaddr*)&client, &slen)) == SOCKET_ERROR)
		{
			printf("recvfrom() failed with error code : %d", WSAGetLastError());
			while (1);
			exit(EXIT_FAILURE);
		}


		//printf("Received packet from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

		if (strcmp(buf, done_message) == 0) {
			fileEnd = 1;
		}
		else {
			//corrupt packet or lose packet to simulate unreliable connection

			int packetCorrupted = Corruptor_Challenge(corruptionPercent, corruptionStrength, buf);
			int packetLost = Packet_Loss(percentLost, buf);


			//calculate checksums
			for (int i = 0; i < 1030; ++i) {
				packetData[i] = buf[i];
			}


			//simulating lost packet, need to force fail checksum to get program to skip make_packet
			if (packetLost == 0) {
				checkVal0 = ((unsigned short)packetData[1024] << 8) & 0xFF00;
				checkVal0 |= (unsigned short)packetData[1025] & 0x00FF;

				


				if (errorCheckActive == 1) {
					checkVal1 = checksum(packetData, 1024);
				}
				else if (errorCheckActive == 0) {
					checkVal1 = checkVal0;
				}

				sequenceRecv = ((unsigned short)packetData[1028] << 8) & 0xFF00;
				sequenceRecv |= ((unsigned short)packetData[1029]) & 0x00FF;

			}
			else {
				checkVal0 = 0xFF;
				checkVal1 = 0x00;
			}

			

			//printf("check0 = %x, check1 = %x\n", checkVal0, checkVal1);
			//while (1);

			//test of ack will be lost
			int ackLost = Packet_Loss(ackpercentLost, buf);

			// if they are equal, send the ack to tell client to send next packet
			if (checkVal0 == checkVal1) {
				if (packetLost == 0) {
					if (sequenceExpected == sequenceRecv) {
						fileEnd = Make_File(&fpOut, packetData);
						
						last_ack = sequenceExpected;
						++sequenceExpected;
						//printf("last_ack: %x\n", last_ack);


						if (ackLost == 0) {

							check_ack = Make_Packet_Ack(last_ack, ack);
							//printf("ack[0] = %x, ack[1] = %x\n", ack[0], ack[1]);
							int ackCorrupted = Corruptor_Challenge(ackcorruptionPercent, ackcorruptionStrength, ack);

							if (sendto(server, ack, recv_len, 0, (struct sockaddr*)&client, slen) == SOCKET_ERROR)
							{
								printf("sendto() failed with error code : %d", WSAGetLastError());
								while (1);
								exit(EXIT_FAILURE);
							}
						}

					}
					else if (last_ack == 0) {

					}
					else {
						if (ackLost == 0) {
							check_ack = Make_Packet_Ack(last_ack, ack);

							int ackCorrupted = Corruptor_Challenge(ackcorruptionPercent, ackcorruptionStrength, ack);

							if (sendto(server, ack, recv_len, 0, (struct sockaddr*)&client, slen) == SOCKET_ERROR)
							{
								printf("sendto() failed with error code : %d", WSAGetLastError());
								while (1);
								exit(EXIT_FAILURE);
							}
						}
					}
				}

				
			}
			
		}


		if (fileEnd == 1) {
			fclose(fpOut);
		}


	}

	//close socket
	closesocket(server);
	WSACleanup();

	return 0;
}
