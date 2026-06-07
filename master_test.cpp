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
#include <cstring>

void send_matrix(int socket_fd, float matrix[][3], int rows, int cols)
{
	// SEND DIMENSIONS
	send(socket_fd, &rows, sizeof(int), 0);
	send(socket_fd, &cols, sizeof(int), 0);
	// SEND DATA
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
	int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(-1 == SocketFD)
	{
		perror("can not create socket");
		exit(EXIT_FAILURE);
	}

	memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(8888);
	stSockAddr.sin_addr.s_addr = INADDR_ANY;

	if(-1 == bind(SocketFD,(const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in)))
	{
		perror("error bind failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	if(-1 == listen(SocketFD, 10))
	{
		perror("error listen failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	int ConnectFD = accept(SocketFD, NULL, NULL);

	if(0 > ConnectFD)
	{
		perror("error accept failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	shutdown(ConnectFD, SHUT_RDWR);
	close(ConnectFD);
	close(SocketFD);

	return 0;
}
