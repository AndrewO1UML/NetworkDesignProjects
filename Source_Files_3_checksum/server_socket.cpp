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

#define BUFLEN 10000

#define corruptionPercent 0.050
#define corruptionStrength 0.500

char inStr[] = "Space_dress.bmp";
char outStr[] = "bmp_out.bmp";

char checksum(char* packetData, int size) {
	/*
		Take the sum of the 1024 Bytes (first byte + second byte + ... + 1024th Byte)
		The sum is stored into result and then returns is complement.
		Ex: packeData[1} has 1024 blocks which contains 1 byte in each block, this function adds those up together
			and takes it complement.
	*/
	char result = 0;
	int i;

	for (i = 0; i < size; i++) {
		result += packetData[i];
	}

	return ~result;
}

int Make_Packet(FILE** fpIn, char* packetData) {
	/*
		updates packetData array to be the next 1KB in your defined "fpIn"
		returns 1 when the end of the file has been reached
		otherwise 0
	*/
	int freadReturn;
	int fileEnd = 0;


	freadReturn = fread(packetData, 1, 1024, *fpIn);
	if (freadReturn == 0) {
		fileEnd = 1;
	}

	return fileEnd;
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

		for (int i = 0; i < 1024; ++i) {
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

int main(int argc, char* argv[])
{
	WSADATA wsa;
	SOCKET server;
	sockaddr_in server_addr, client;
	hostent* localHost;
	int slen, recv_len;
	char* localIP;
	char buf[BUFLEN];
	char ack[5] = "Ack";
	char nack[5] = "Nack";
	char done_message[20] = "file_send_done";
	char checkVal0, checkVal1;
	char packetData[1024];

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



		//print details of the client/peer and the data received
		//printf("Received packet from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

		if (strcmp(buf, done_message) == 0) {
			fileEnd = 1;
		}
		else {
			//corrupt packet to simulate unreliable connection
			int packetCorrupted = Corruptor_Challenge(corruptionPercent, corruptionStrength, buf);
			//calculate checksums
			for (int i = 0; i < 1024; ++i) {
				packetData[i] = buf[i];
			}
			checkVal0 = buf[1024];

			checkVal1 = checksum(packetData, 1024);

			//printf("check0 = %x, check1 = %x\n", checkVal0, checkVal1);

			// if they are equal, send the ack to tell client to send next packet
			if (checkVal0 == checkVal1) {
				fileEnd = Make_File(&fpOut, packetData);

				if (sendto(server, ack, recv_len, 0, (struct sockaddr*)&client, slen) == SOCKET_ERROR)
				{
					printf("sendto() failed with error code : %d", WSAGetLastError());
					while (1);
					exit(EXIT_FAILURE);
				}
			}

			// if they arent equal send the nack and tell client to resent
			else {
				if (sendto(server, nack, recv_len, 0, (struct sockaddr*)&client, slen) == SOCKET_ERROR)
				{
					printf("sendto() failed with error code : %d", WSAGetLastError());
					while (1);
					exit(EXIT_FAILURE);
				}
			}
			
		}


		if (fileEnd == 1) {
			if (sendto(server, buf, recv_len, 0, (struct sockaddr*)&client, slen) == SOCKET_ERROR)
			{
				printf("sendto() failed with error code : %d", WSAGetLastError());
				while (1);
				exit(EXIT_FAILURE);
			}

			fclose(fpOut);

		}


	}

	//close socket
	closesocket(server);
	WSACleanup();

	return 0;
}
