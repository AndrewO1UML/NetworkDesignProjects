/*
	New
	Jared Hebert
	Andrew O'Brien
	EECE.4830
	UDP CLient
	some code from https://www.binarytides.com/udp-socket-programming-in-winsock/ used
*/

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<iostream>
#include<winsock2.h>
#include<WS2tcpip.h>


using namespace::std;

#pragma comment(lib,"ws2_32.lib") //Winsock Library

#define BUFLEN 10000 
#define packetDebugMode 0

#define corruptionPercent 0.000
#define corruptionStrength 0.000

char inStr[] = "bmp_24.bmp";
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


	fwriteReturn = fwrite(packetData, 1, 1024, *fpOut);
	printf("fwriteReturn = %d\n", fwriteReturn);
	if (fwriteReturn != 1024) {
		fileEnd = 1;
	}

	return fileEnd;
}

int Corruptor_Challenge(float challenge	, float corruption_amount, char* packetData) {
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
	SOCKET client;
	sockaddr_in client_addr, si_other;
	hostent* localHost;
	
	FILE* fpIn;
	FILE* fpOut;

	char buf[BUFLEN];
	char message[20] = "file_send_done";
	
	char endResult; 
	int slen;

	char concat[1025]; 
		
	int j; 
	int i; 
	
	slen = sizeof(si_other);

	char* localIP;

	int imgSize;	//in kB
	char packetData[1024];

	srand(time(NULL));		//Initialize random numbers



	//Init winsock library
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		return 1;
	}

	printf("Initialised.\n");

	//create socket
	if ((client = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d", WSAGetLastError());
	}

	printf("Socket created.\n");

	//cout << "Local IP: " << inet_addr("127.0.0.1") << endl;

	//define address of server to cennect to
	si_other.sin_family = AF_INET;
	si_other.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	si_other.sin_port = htons(8888);

	//Open files

	fpIn = fopen(inStr, "rb");
	if (fpIn == NULL) {
		printf("In file failed to open :( \n");
		return 1;
	}

	if (packetDebugMode == 1) {

		fpOut = fopen(outStr, "wb");
		if (fpOut == NULL) {
			printf("Out file failed to open :( \n");
			return 1;
		}
	}

	// The function below ingests a file and reproduces it 1KB at a time, only run when the global packetDebugMode is set to 1.

	int fileEnd = 0;

	if (packetDebugMode == 1) {


		int fileEnd = 0;
		int fileEndOut = 0;
		int packetCorrupted;

		while (fileEnd == 0) {

			fileEnd = Make_Packet(&fpIn, packetData);

			packetCorrupted = Corruptor_Challenge(corruptionPercent, corruptionStrength, packetData);

			if (packetCorrupted) {
				printf("Packet Corrupted\n");
			}
			//printf("%s\n", packetData);
			fileEndOut = Make_File(&fpOut, packetData);

			//printf("fileEndIn = %d fileEndOut = %d\n", fileEnd, fileEndOut);

		}

		if (fileEnd == 1) {
			fclose(fpIn);
			fclose(fpOut);
		}

		return 0;
	}

	

	//start communication

	while (fileEnd == 0)
	{ 
		fileEnd = Make_Packet(&fpIn, packetData); 
		endResult = checksum(packetData, 1024); 
		
		// concatenate packetData with checksum (endResult) 
		j = 0; 
		for (i = 0; i < 1024; i++) {
			concat[j] = packetData[i];
			j++; 
		}
		concat[1025] = endResult; 
		
		if (fileEnd == 0) {
			//send the message
			if (sendto(client, concat, 1025, 0, (struct sockaddr*)&si_other, slen) == SOCKET_ERROR)
			{
				printf("sendto() failed with error code : %d", WSAGetLastError());
				while (1);
				exit(EXIT_FAILURE);
			}	
		}
		else {
			//send the message
			if (sendto(client, message, strlen(message), 0, (struct sockaddr*)&si_other, slen) == SOCKET_ERROR)
			{
				printf("sendto() failed with error code : %d", WSAGetLastError());
				while (1);
				exit(EXIT_FAILURE);
			}
		}

		//receive a reply and print it
		//clear the buffer by filling null, it might have previously received data
		memset(buf, '\0', BUFLEN);
		//try to receive some data, this is a blocking call
		if (recvfrom(client, buf, BUFLEN, 0, (struct sockaddr*)&si_other, &slen) == SOCKET_ERROR)
		{
			printf("recvfrom() failed with error code : %d", WSAGetLastError());
			while (1);
			exit(EXIT_FAILURE);
		}

		puts(buf);

		//delay 500ms to artificially slow program down
		Sleep(100);

		if (fileEnd == 1) {
			fclose(fpIn);
		}
	}

	//close socket
	closesocket(client);
	WSACleanup();

	return 0;
}