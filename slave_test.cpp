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
#include <cstring>

void send_matrix(int socket_fd, float matrix[][3], int rows, int cols)
{
	// SEND DIMENSIONS
	send(socket_fd, &rows, sizeof(int), 0);
	send(socket_fd, &cols, sizeof(int), 0);
	// SEND MATRIX DATA
	send(socket_fd, matrix, rows * cols * sizeof(float), 0);

	printf("\nMatrix SENT\n");
}

void receive_matrix(int socket_fd)
{
	int rows;
	int cols;

	// RECEIVE DIMENSIONS
	recv(socket_fd, &rows, sizeof(int), 0);
	recv(socket_fd, &cols, sizeof(int), 0);

	printf("\nReceiving matrix...\nRows: %d\nCols: %d\n", rows, cols);

	float matrix[100][100];

	// RECEIVE MATRIX DATA
	recv(socket_fd, matrix, rows * cols * sizeof(float), 0);

	printf("\nRECEIVED MATRIX:\n");

	for(int i = 0; i < rows; i++)
	{
		for(int j = 0; j < cols; j++)
			std::cout<<matrix[i][j]<<" ";
		printf("\n");
	}
}

int main(void)
{
	struct sockaddr_in stSockAddr;
	int Res;
	int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (-1 == SocketFD)
	{
		perror("cannot create socket");
		exit(EXIT_FAILURE);
	}

	memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(8888);
	Res = inet_pton(AF_INET, "127.0.0.1", &stSockAddr.sin_addr);

	if (0 > Res)
	{
		perror("error: first parameter is not a valid address family");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}
	else if (0 == Res)
	{
		perror("char string (second parameter does not contain valid ipaddress");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	if (-1 == connect(SocketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in)))
	{
		perror("connect failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	shutdown(SocketFD, SHUT_RDWR);
	close(SocketFD);

	return 0;
}
