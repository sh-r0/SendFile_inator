#include "shared.hpp"

int32_t main(int32_t _argc, char** _argv) {
	if (_argc < 2) {
		printf("Incorrect calling method!\nShould be ./client <server ip>\n");
		return EXIT_FAILURE;
	}
	if (!initWinSock())
		return EXIT_FAILURE;
	char* targetIp = _argv[1];
	debugf("Target ip:'%s'\n", targetIp);
	addrinfo* serverAddr, hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_IP;
	if (getaddrinfo(targetIp, serverPort_c, &hints, &serverAddr) != 0) {
		fprintf(stderr, "Couldn't resolve server ip addr - %s\n", targetIp);
		fprintf(stderr, "Wsa:%i", WSAGetLastError());
		WSACleanup();
		return EXIT_FAILURE;
	}
	SOCKET clientSock = socket(serverAddr->ai_family, serverAddr->ai_socktype, serverAddr->ai_protocol);
	if (clientSock == SOCKET_ERROR || clientSock == INVALID_SOCKET) {
		fprintf(stderr, "Couldn't create socket\n");
		fprintf(stderr, "Wsa:%i", WSAGetLastError());
		WSACleanup();
		return EXIT_FAILURE;
	}
	if (connect(clientSock, serverAddr->ai_addr, (int)serverAddr->ai_addrlen) != 0) {
		fprintf(stderr, "Couldn't connect to server\n");
		fprintf(stderr, "Wsa:%i", WSAGetLastError());
		closesocket(clientSock);
		WSACleanup();
		return EXIT_FAILURE;
	}
	freeaddrinfo(serverAddr);
	header_t currHeader{};
	constexpr const uint64_t sendBufSize_c = 1024;
	char sendBuf[sendBufSize_c];
	ZeroMemory(sendBuf, sendBufSize_c);

	pushHeader(sendBuf, currHeader, CONNECTION_END);
	send(clientSock, sendBuf, currHeader.msgSize, 0);

	closesocket(clientSock);
	WSACleanup();
	return EXIT_SUCCESS;
}