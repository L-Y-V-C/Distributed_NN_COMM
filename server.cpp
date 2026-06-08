/* Server code in C */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <thread>
#include <map>
#include <string>

using namespace std;

map<string, int> clients;
map<string, struct sockaddr_in> udpClients;
int serverUdpSeqNum = 1;

void selectMenu(int &op);
bool processMessage(int ConnectFD);
void receiveFunction(int ConnectFD);
void prepareMessage(int op, char *buffer);
void convertIntToString(int byteSize, int num, char *str);
unsigned int calculateChecksum(char* buffer, int length);
void serverSendByUDP(int SocketFD, char* largeBuffer, int totalSize, struct sockaddr_in destAddr);
void handleUdpMode(int SocketFD);

void serverSendByUDP(int SocketFD, char* largeBuffer, int totalSize, struct sockaddr_in destAddr)
{
	int datagramSize = 500, tpSize = 3, cpSize = 3, snSize = 4, hashSize = 1;
	int headerSize = tpSize + cpSize + snSize + hashSize;
	int payloadSize = datagramSize - headerSize;
	int totalPackets = (totalSize + payloadSize - 1) / payloadSize;

	char datagram[500];
	char tmpStr[32];

	for(int i = 0; i < totalPackets; i++) {
		bzero(datagram, 500);
		int datagramOffset = 0;
		int dataStart = i * payloadSize;
		int dataLength = min(payloadSize, totalSize - dataStart);

		// hash
		unsigned int numChecksum = calculateChecksum(largeBuffer + dataStart, dataLength);
		convertIntToString(hashSize, numChecksum, tmpStr);
		memcpy(datagram + datagramOffset, tmpStr, hashSize);
		datagramOffset += hashSize;
		// total Packets
		convertIntToString(tpSize, totalPackets, tmpStr);
		memcpy(datagram + datagramOffset, tmpStr, tpSize);
		datagramOffset += tpSize;
		// current packet
		convertIntToString(cpSize, i + 1, tmpStr);
		memcpy(datagram + datagramOffset, tmpStr, cpSize);
		datagramOffset += cpSize;
		// sequence num
		convertIntToString(snSize, serverUdpSeqNum, tmpStr);
		memcpy(datagram + datagramOffset, tmpStr, snSize);
		datagramOffset += snSize;
		// payload
		memcpy(datagram + datagramOffset, largeBuffer + dataStart, dataLength);
		datagramOffset += dataLength;

		if (dataLength < payloadSize)
			memset(datagram + datagramOffset, '#', payloadSize - dataLength);

		sendto(SocketFD, datagram, datagramSize, 0, (struct sockaddr*)&destAddr, sizeof(destAddr));
		usleep(2000);
	}
	serverUdpSeqNum++;
}

unsigned int calculateChecksum(char* buffer, int length) {
	unsigned int sum = 0;
	for (int i = 0; i < length; i++)
		sum += (unsigned char)buffer[i];
	unsigned int mod = 7;
	return sum % mod;
}

void receiveFunction(int ConnectFD) {
	for(;;) {
		if(!processMessage(ConnectFD))
			break;
	}
	shutdown(ConnectFD, SHUT_RDWR);
	close(ConnectFD);
}

void convertIntToString(int byteSize, int num, char *str)
{
	int power10 = 1;

	for(int i = 1; i < byteSize; i++)
		power10 *= 10;

	for(int i = 0; i < byteSize; i++)
	{
		int digit = num / power10;
		str[i] = '0' + digit;
		num %= power10;
		power10 /= 10;
	}
}

void handleUdpMode(int SocketFD) {
	struct sockaddr_in clientAddr;
	socklen_t addrLen = sizeof(clientAddr);
	char datagram[500];

	char largeAssemblyBuffer[250000];
	int expectedPackets = 0;
	int receivedPacketsCount = 0;

	printf("\nUDP mode running and listening...\n");

	for(;;)
	{
		bzero(datagram, 500);
		int n = recvfrom(SocketFD, datagram, 500, 0, (struct sockaddr*)&clientAddr, &addrLen);
		if (n <= 0)
			continue;

		if (datagram[0] == 'k' || datagram[0] == 'K')
			continue;

		char tmpAux[10];
		memcpy(tmpAux, datagram + 1, 3);
		tmpAux[3] = '\0';
		int totalPackets = atoi(tmpAux);

		memcpy(tmpAux, datagram + 4, 3);
		tmpAux[3] = '\0';
		int currentPacket = atoi(tmpAux);

		int payloadLen = 489; // tmp
		if (currentPacket == totalPackets) {
			while (payloadLen > 0 && datagram[11 + payloadLen - 1] == '#')
				payloadLen--;
		}

		// hash
		unsigned int calculatedHash = calculateChecksum(datagram + 11, payloadLen);
		char calcHashStr[2];
		convertIntToString(1, calculatedHash, calcHashStr);

		if (datagram[0] != calcHashStr[0])
		{
			char nackDatagram[500];
			bzero(nackDatagram, 500);
			nackDatagram[0] = 'k';
			memcpy(nackDatagram + 1, datagram + 7, 4);
			memset(nackDatagram + 5, '#', 495);
			sendto(SocketFD, nackDatagram, 500, 0, (struct sockaddr*)&clientAddr, addrLen);
			printf("\n[UDP] ERROR: Wrong hash in datagram. NACK sent\n");
			continue;
		}

		if (currentPacket == 1)
		{
			expectedPackets = totalPackets;
			receivedPacketsCount = 0;
			bzero(largeAssemblyBuffer, sizeof(largeAssemblyBuffer));
		}

		// big buffer made
		int payloadOffset = (currentPacket - 1) * 489;
		memcpy(largeAssemblyBuffer + payloadOffset, datagram + 11, payloadLen);
		receivedPacketsCount++;

		// msg process
		if (receivedPacketsCount == expectedPackets)
		{
			char msgType = largeAssemblyBuffer[0];
			char tmpStr[32];

			switch (msgType)
			{
				case 'F': // file send UDP
				{
					int memOffset = 1;

					// nickname destiny
					memcpy(tmpStr, largeAssemblyBuffer + memOffset, 5); tmpStr[5] = '\0';
					memOffset += 5;
					int nndSize = atoi(tmpStr);
					char nickDest[256];
					memcpy(nickDest, largeAssemblyBuffer + memOffset, nndSize); nickDest[nndSize] = '\0';
					memOffset += nndSize;

					// file name
					memcpy(tmpStr, largeAssemblyBuffer + memOffset, 3); tmpStr[3] = '\0';
					memOffset += 3;
					int fnSize = atoi(tmpStr);
					char fileName[256];
					memcpy(fileName, largeAssemblyBuffer + memOffset, fnSize); fileName[fnSize] = '\0';
					memOffset += fnSize;

					// nickname origin
					memcpy(tmpStr, largeAssemblyBuffer + memOffset, 5); tmpStr[5] = '\0';
					memOffset += 5;
					int nnoSize = atoi(tmpStr);
					char nickOrig[256];
					memcpy(nickOrig, largeAssemblyBuffer + memOffset, nnoSize); nickOrig[nnoSize] = '\0';
					memOffset += nnoSize;

					// sequence number
					char seqNumStr[13];
					memcpy(seqNumStr, largeAssemblyBuffer + memOffset, 12); seqNumStr[12] = '\0';
					memOffset += 12;

					// file Size
					char tmpSizeStr[25];
					memcpy(tmpSizeStr, largeAssemblyBuffer + memOffset, 22); tmpSizeStr[22] = '\0';
					memOffset += 22;
					long long fileSize = atoll(tmpSizeStr);

					memOffset += fileSize;

					// hash
					int hashPos = memOffset;
					int receivedHash = largeAssemblyBuffer[hashPos] - '0';
					unsigned int calculatedHash = calculateChecksum(largeAssemblyBuffer, hashPos);

					if (calculatedHash == receivedHash)
					{
						printf("[UDP] Correct full hash. Forwarding file to target: %s\n", nickDest);

						if (udpClients.find(nickDest) != udpClients.end())
						{
							serverSendByUDP(SocketFD, largeAssemblyBuffer, hashPos + 1, udpClients[nickDest]);
						}
						else
						{
							printf("[UDP] Target '%s' does not exist\n", nickDest);
							char errorBuf[256];
							char errorData[1][256];
							strcpy(errorData[0], "Destination Nickname not found");
							prepareMessage(1, errorBuf, errorData);
							serverSendByUDP(SocketFD, errorBuf, strlen(errorBuf), clientAddr);
						}
					}
					else
					{
						printf("[UDP] Error: Assembled message has wrong hash. File discarded.\n");
					}
					break;
				}
				default:
					break;
			}
		}
	}
}

int main(void)
{
	int op = 1;

	struct sockaddr_in stSockAddr;
	int SocketFD;
	SocketFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if(-1 == SocketFD)
	{
		perror("can not create socket");
		exit(EXIT_FAILURE);
	}

	memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(45000);
	stSockAddr.sin_addr.s_addr = INADDR_ANY;

	if(-1 == bind(SocketFD,(const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in)))
	{
		perror("error bind failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	if(!op)
	{
		if(-1 == listen(SocketFD, 10))
		{
			perror("error listen failed");
			close(SocketFD);
			exit(EXIT_FAILURE);
		}
	}

	if(op)
	{
		struct sockaddr_in clientAddr;
		socklen_t addrLen = sizeof(clientAddr);
	}

	for(;;)
	{
		if(!op)
		{
			printf("\nTCP mode...\n");
			int ConnectFD = accept(SocketFD, NULL, NULL);
			thread(receiveFunction, ConnectFD).detach();
		}
		else
		{
			handleUdpMode(SocketFD);
			break;
		}

		//shutdown(ConnectFD, SHUT_RDWR);
		//close(ConnectFD);
	}

	close(SocketFD);

	return 0;
}
