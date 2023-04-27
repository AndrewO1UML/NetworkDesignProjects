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

#define timeout 10 //ms
#define Window_Size 10
#define BUFLEN 10000 
#define packetDebugMode 0

#define corruptionPercent 0.500
#define corruptionStrength 0.500

char inStr[] = "Space_dress.bmp";
char outStr[] = "bmp_out.bmp";
char ack0[5] = "Ack0";
char ack1[5] = "Ack1";

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
	SOCKET client;
	sockaddr_in client_addr, si_other;
	hostent* localHost;

	FILE* fpIn;
	FILE* fpOut;

	char buf[BUFLEN];
	char message[20] = "file_send_done";

	char endResult;
	int slen;

	char concat[1030];

	int j;
	int i;
	int checkValid = 1;
	int error = 0;
	int sequence = 1;

	slen = sizeof(si_other);

	char* localIP;

	int imgSize;	//in kB
	unsigned short send_base;
	unsigned short next_seq;
	unsigned short ack;
	unsigned short final_ack;
	char packetData[1030];

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
	
	int read_timeout = timeout;

	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char *) &read_timeout, sizeof read_timeout);

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


	//start communication

	clock_t start, end;

	start = clock();

	next_seq = 0;
	send_base = 0;
	ack = 0;
	final_ack = 0xFFFF;
	int transmission_done = 0;
	int final_ack_lock = 0;

	while (!transmission_done)
	{
		
		if ((next_seq - send_base) < (Window_Size - 1)) {
			//If not all packets in the window are sent, keep sending


			fileEnd = Make_Packet(&fpIn, packetData, next_seq);

			if (fileEnd == 0) {
				//send the message
				if (sendto(client, packetData, 1030, 0, (struct sockaddr*)&si_other, slen) == SOCKET_ERROR)
				{
					printf("sendto() failed with error code : %d", WSAGetLastError());
					while (1);
					exit(EXIT_FAILURE);
				}
				printf("Sent Packet: %x\n", next_seq);

			}
			else {
				if (final_ack_lock == 0) {
					final_ack = next_seq - 1;
					final_ack_lock = 1;
					printf("final_ack = %x\n", final_ack);
				}
				
			}


			++next_seq;
		}
		else if ((next_seq - send_base) == (Window_Size - 1)) {
			//all packets in window have been sent, wait for ack

			//receive a reply and print it
			//clear the buffer by filling null, it might have previously received data
			memset(buf, '\0', BUFLEN);

			//try to receive some data, this is a blocking call

			if (recvfrom(client, buf, BUFLEN, 0, (struct sockaddr*)&si_other, &slen) == SOCKET_ERROR)
			{
				//printf("recvfrom() failed with error code : %d", WSAGetLastError());
				error = WSAGetLastError();
			}

			//puts(buf);
			

			if (error == 10060) {
				//Timeout happened, need to resend window
				next_seq = send_base;
				printf("Timeout!\n");

				if (fseek(fpIn, 1024 * send_base, SEEK_SET) != 0) {
					//this failed
					printf("fseek() failed\n");
				}
				
				error = 0;
			}
			else {
				ack = ((int)buf[0] << 8) & 0xFF00;
				ack |= ((int)buf[1]) & 0x00FF;

				int check0, check1;

				check0 = ((int)buf[2] << 24) & 0xFF000000;
				check0 |= ((int)buf[3] << 16) & 0x00FF0000;
				check0 |= ((int)buf[4] << 8) & 0x0000FF00;
				check0 |= ((int)buf[5]) & 0x000000FF;

				int result = 0;

				for (int i = 0; i < 2; i++) {
					result += buf[i];
				}

				check1 = ~result;

				if (check0 == check1) {
					//checksum is good
					//sometimes checksum passes but the ack is wrong, ignoring definitely screwed up acks
					//printf("ack = %x\n", ack);
					if ((ack & 0x8000) == 0) {
						send_base = ack + 1;

						printf("ack = %x\n", ack);

						//close up shop
						if (ack == final_ack) {
							transmission_done = 1;
							fclose(fpIn);

							//send the closing message
							if (sendto(client, message, strlen(message), 0, (struct sockaddr*)&si_other, slen) == SOCKET_ERROR)
							{
								printf("sendto() failed with error code : %d", WSAGetLastError());
								while (1);
								exit(EXIT_FAILURE);
							}
						}

					}
				}
				else {
					printf("Bad ack check\n");
				}
			}
		}
		else {
			// should not be possible, send help
			//guess we just restart
			//printf("I shouldn't be here\n");
			next_seq = 0;
			send_base = 0;
			//while (1);
		}

		

		//slow down for testing
		//Sleep(100);
	}

	end = clock();

	double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;	//taken from: https://www.geeksforgeeks.org/how-to-measure-time-taken-by-a-program-in-c/


	//close socket
	closesocket(client);
	WSACleanup();

	printf("time taken = %f", cpu_time_used);
	while (1);

	return 0;
}
