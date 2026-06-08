 /* Client code in C */

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
#include <atomic>

using namespace std;

atomic<bool> connected(true);
atomic<bool> logged(false);
atomic<int> globalSeqNum(1);
atomic<int> udpSeqNum(1);

char myNickname[32];

void convertIntToString(int byteSize, int num, char *str);
string convertIntToBigString(int byteSize, string num);
unsigned int calculateChecksum(char* buffer, int length);

void selectMenu(int &op);
void sendFunction(int SocketFD, int connType);
void receiveFunction(int SocketFD, int connType);
bool processMessage(int SocketFD, int connType);
void print1stMenu(int &op);
void print2ndMenu(int &op);
void checkOption(int op, int menu, int SocketFD, int connType);
void makeMessage(int op, int SocketFD, int connType);
void toDatagram(int SocketFD, char *buffer, int bufferSize);
bool readNetworkData(int connType, int SocketFD, char* dest, int size, char* sourceBuffer, int &memOffset);

void convertIntToString(int byteSize, int num, char *str) {
	int power10 = 1;
	for(int i = 1; i < byteSize; i++)
		power10 *= 10;
	for(int i = 0; i < byteSize; i++) {
		int digit = num / power10;
		str[i] = '0' + digit;
		num %= power10;
		power10 /= 10;
	}
}

unsigned int calculateChecksum(char* buffer, int length) {
	unsigned int sum = 0;
	for (int i = 0; i < length; i++)
		sum += (unsigned char)buffer[i];
	unsigned int mod = 7;
	return sum % mod;
}

void toDatagram(int SocketFD, char *buffer, int bufferSize) {
	int datagramSize = 500, tpSize = 3, cpSize = 3, snSize = 4, hashSize = 1;
	int headerSize = tpSize + cpSize + snSize + hashSize;
	unsigned int numChecksum;

	int payloadSize = datagramSize - headerSize;
	int totalPackets = (bufferSize + payloadSize - 1) / payloadSize; // ceiling

	char datagram[500];
	char tmpStr[10];

	for(int i = 0; i < totalPackets; i++) {
		bzero(datagram, 500);
		int datagramOffset = 0;

		// copy payload
		int dataStart = i * payloadSize;
		// take minimum
		int dataSize = min(payloadSize, bufferSize - dataStart);

		// hash
		numChecksum = calculateChecksum(buffer + dataStart, dataSize);
		convertIntToString(hashSize, numChecksum, tmpStr);

		memcpy(datagram + datagramOffset, tmpStr, hashSize);
		datagramOffset += hashSize;

		// total packets
		convertIntToString(tpSize, totalPackets, tmpStr);
		memcpy(datagram + datagramOffset, tmpStr, tpSize);
		datagramOffset += tpSize;

		// current packet
		convertIntToString(cpSize, i + 1, tmpStr);
		memcpy(datagram + datagramOffset, tmpStr, cpSize);
		datagramOffset += cpSize;

		// sequence number
		convertIntToString(snSize, udpSeqNum, tmpStr);
		memcpy(datagram + datagramOffset, tmpStr, snSize);
		datagramOffset += snSize;

		memcpy(datagram + datagramOffset, buffer + dataStart, dataSize);
		datagramOffset += dataSize;

		// padding
		if (dataSize < payloadSize)
			memset(datagram + datagramOffset, '#', payloadSize - dataSize);

		cout<<"\n[UDP sending datagram "<<i+1<<"/"<<totalPackets<<"]\n"<<datagram<<"\n";
		//printf("\n[UDP sending datagram %d/%d]\n%.*s...\n", i + 1, totalPackets, 100, datagram);
		write(SocketFD, datagram, datagramSize);
		usleep(2000);
	}
	udpSeqNum++;
}

void makeMessage(int op, int SocketFD, int connType) {
	char buffer[256], input[256], inputSizeStr[32];
	int inputSize, offset;
	int byteSize[] = {4, 7, 5, 5};
	int byteSizeAux[] = {0, 0, 7};

	offset = 1;
	bzero(buffer, 256);

	switch(op) {
		case 3: // send file
		{
			char fileChunk[10];
			static char readBuffer[250000], writeBuffer[250000];
			int bytesRead, readSize = 0;

			bzero(writeBuffer, 250000);
			bzero(readBuffer, 250000);
			writeBuffer[0] = 'F';

			if(connType) {
				int nndSize = 5, fnSize = 3, nnoSize = 5, seqNumSize = 12, fSize = 22, hashSize = 1;
				string numBigStr, inputSizeBigStr;
				unsigned int numChecksum;

				printf("Send to: ");
				fflush(stdout);
				fgets(input, 256, stdin);

				inputSize = strlen(input) - 1;
				convertIntToString(nndSize, inputSize, inputSizeStr);

				// nickname destination
				memcpy(writeBuffer + offset, inputSizeStr, nndSize);
				offset += nndSize;
				memcpy(writeBuffer + offset, input, inputSize);
				offset += inputSize;

				printf("File name: ");
				fflush(stdout);
				fgets(input, 256, stdin);

				inputSize = strlen(input) - 1;
				input[inputSize] = '\0';

				string fileName(input);
				FILE *file = fopen(fileName.c_str(), "rb");
				if(!file) {
					printf(
						"--------------------\n"
						"ERROR!\n"
						"Cannot open the file\n"
						"--------------------\n"
					);
					return;
				}
				while ((bytesRead = fread(fileChunk, 1, 10, file)) > 0) {
					memcpy(readBuffer + readSize, fileChunk, bytesRead);
					readSize += bytesRead;
				}

				fclose(file);

				convertIntToString(fnSize, inputSize, inputSizeStr);

				// file name
				memcpy(writeBuffer + offset, inputSizeStr, fnSize);
				offset += fnSize;
				memcpy(writeBuffer + offset, input, inputSize);
				offset += inputSize;

				inputSize = strlen(myNickname);
				convertIntToString(nnoSize, inputSize, inputSizeStr);

				// nickname origin
				memcpy(writeBuffer + offset, inputSizeStr, nnoSize);
				offset += nnoSize;
				memcpy(writeBuffer + offset, myNickname, inputSize);
				offset += inputSize;

				numBigStr = to_string(globalSeqNum++);
				inputSizeBigStr = convertIntToBigString(seqNumSize, numBigStr);

				// sequence number
				memcpy(writeBuffer + offset, inputSizeBigStr.c_str(), seqNumSize);
				offset += seqNumSize;

				numBigStr = to_string(readSize);
				inputSizeBigStr = convertIntToBigString(fSize, numBigStr);

				// file
				memcpy(writeBuffer + offset, inputSizeBigStr.c_str(), fSize);
				offset += fSize;
				memcpy(writeBuffer + offset, readBuffer, readSize);
				offset += readSize;

				numChecksum = calculateChecksum(writeBuffer, offset);
				convertIntToString(hashSize, numChecksum, inputSizeStr);

				// hash
				memcpy(writeBuffer + offset, inputSizeStr, hashSize);
				offset += hashSize;
				//cout<<"\nMESSAGE MADE:\n"<<writeBuffer<<endl<<endl;
				toDatagram(SocketFD, writeBuffer, offset);
			}
			break;
		}
		default:break;
	}
}

bool processMessage(int SocketFD, int connType) {
	static char messageBuffer[250000];
	static int totalPackets = 0, currentPacket = 0, packetsReceivedCount = 0;
	static char expectedGlobalSeq[13] = "000000000000";

	char msg[256], msgType, nickname[256];
	int n, i, tmpSize;
	int byteSize[] = {5, 3, 7, 5, 5};
	int byteSizeAux[] = {0, 7, 5};

	if(connType) {
		char datagramBuffer[500];
		n = read(SocketFD, datagramBuffer, 500);
		if(n <= 0) {
			printf(
				"----------------------------\n"
				"[UDP] Error reading datagram\n"
				"----------------------------\n"
			);
			return false;
		}

		if(datagramBuffer[0] == 'k' || datagramBuffer[0] == 'K')
			return true;

		char tmpAux[10];
		memcpy(tmpAux, datagramBuffer + 1, 3);
		tmpAux[3] = '\0';
		totalPackets = atoi(tmpAux);

		memcpy(tmpAux, datagramBuffer + 4, 3);
		tmpAux[3] = '\0';
		currentPacket = atoi(tmpAux);

		//datagramBuffer[n < 500 ? n : 499] = '\0';
		cout<<"\n[UDP datagram received "<<currentPacket<<"/"<<totalPackets<<"]\n"<<datagramBuffer<<"\n";
		//printf("\n[UDP datagram received]\n%.*s...\n", 100, datagramBuffer);

		int payloadLen = 489; // tmp
		if(currentPacket == totalPackets) {
			while(payloadLen > 0 && datagramBuffer[11 + payloadLen - 1] == '#')
				payloadLen--;
		}

		// hash
		unsigned int calculatedHash = calculateChecksum(datagramBuffer + 11, payloadLen);
		char calcHashStr[2];
		convertIntToString(1, calculatedHash, calcHashStr);

		// send nack
		if(datagramBuffer[0] != calcHashStr[0]) {
			char nackDatagram[500];
			bzero(nackDatagram, 500);
			nackDatagram[0] = 'k';
			memcpy(nackDatagram + 1, datagramBuffer + 7, 4); // Extraer el seqNum local que está en la posición 5
			memset(nackDatagram + 5, '#', 495);
			write(SocketFD, nackDatagram, 500);
			printf("\n[UDP] ERROR: Wrong hash in datagram. Sending NACK\n");
			return true;
		}

		char tmp[10];

		if(currentPacket == 1) {
			packetsReceivedCount = 0;
			bzero(messageBuffer, sizeof(messageBuffer));

			int checkOffset = 1;
			memcpy(tmp, datagramBuffer + 11 + checkOffset, 5);
			tmp[5] = '\0';
			checkOffset += 5 + atoi(tmp);

			memcpy(tmp, datagramBuffer + 11 + checkOffset, 3);
			tmp[3] = '\0';
			checkOffset += 3 + atoi(tmp);

			memcpy(tmp, datagramBuffer + 11 + checkOffset, 5);
			tmp[5] = '\0';
			checkOffset += 5 + atoi(tmp);

			memcpy(expectedGlobalSeq, datagramBuffer + 11 + checkOffset, 12);
			expectedGlobalSeq[12] = '\0';
		}

		int assemblyOffset = (currentPacket - 1) * 489;
		memcpy(messageBuffer + assemblyOffset, datagramBuffer + 11, payloadLen);
		packetsReceivedCount++;

		// send ack
		char ackDatagram[500];
		bzero(ackDatagram, 500);
		ackDatagram[0] = 'K';
		memcpy(ackDatagram + 1, expectedGlobalSeq, 12);
		char tpStr[4], cpStr[4];
		convertIntToString(3, totalPackets, tpStr);
		convertIntToString(3, currentPacket, cpStr);
		memcpy(ackDatagram + 13, tpStr, 3);
		memcpy(ackDatagram + 16, cpStr, 3);
		memset(ackDatagram + 19, '#', 481);
		write(SocketFD, ackDatagram, 500);

		if (packetsReceivedCount == totalPackets)
			msgType = messageBuffer[0];
		else
			return true;
	}
	return true;
}

bool readNetworkData(int connType, int SocketFD, char* dest, int size, char* sourceBuffer, int &memOffset) {
	if(!connType) {
		int bytesRead = 0;
		while(bytesRead < size) {
			int r = read(SocketFD, dest + bytesRead, size - bytesRead);
			if(r <= 0)
				return false;
			bytesRead += r;
		}
	} else {
		memcpy(dest, sourceBuffer + memOffset, size);
		memOffset += size;
	}
	dest[size] = '\0';
	return true;
}

void sendFunction(int SocketFD, int connType) {
	int op;
	for(;connected;) {
		if(!logged) {
			print1stMenu(op);
			checkOption(op, 1, SocketFD, connType);
		} else {
			print2ndMenu(op);
			checkOption(op, 2, SocketFD, connType);
		}
	}
}

void receiveFunction(int SocketFD, int connType) {
	for(;;) {
		if(!processMessage(SocketFD, connType)) {
			connected = false;
			break;
		}
	}
	shutdown(SocketFD, SHUT_RDWR);
	close(SocketFD);
}

int main(void) {
	int net_op = 1;

	struct sockaddr_in stSockAddr;
	int Res, SocketFD;

	SocketFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if(-1 == SocketFD) {
		perror("cannot create socket");
		exit(EXIT_FAILURE);
	}

	memset(&stSockAddr, 0, sizeof(struct sockaddr_in));
	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(45000);
	Res = inet_pton(AF_INET, "127.0.0.1", &stSockAddr.sin_addr);

	if(0 > Res) {
		perror("error: first parameter is not a valid address family");
		close(SocketFD);
		exit(EXIT_FAILURE);
	} else if(0 == Res) {
		perror("char string (second parameter does not contain valid ipaddress");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	if(-1 == connect(SocketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in))) {
		perror("connect failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	thread send = thread(sendFunction, SocketFD, net_op);
	thread receive = thread(receiveFunction, SocketFD, net_op);

	send.join();
	receive.join();

	shutdown(SocketFD, SHUT_RDWR);
	close(SocketFD);

	return 0;
}
