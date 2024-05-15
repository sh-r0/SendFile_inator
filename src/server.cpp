#include "shared.hpp"

#include <filesystem> 
#include <vector>

bool createListenSocket(SOCKET& _listenSock) {
	addrinfo* serverAddress, hints{};
	hints.ai_family = AF_INET;		//todo: add support for ipv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(NULL, serverPort_c, &hints, &serverAddress) != 0) {
		fprintf(stderr, "Couldn't get server address!\n");
		WSACleanup();
		return false;
	}
	_listenSock = INVALID_SOCKET;
	_listenSock = socket(serverAddress->ai_family, serverAddress->ai_socktype, serverAddress->ai_protocol);

	if (_listenSock == INVALID_SOCKET) {
		fprintf(stderr, "Couldn't create listen socket!\n");
		WSACleanup();
		return false;
	}
	if (bind(_listenSock, serverAddress->ai_addr, int(serverAddress->ai_addrlen)) == SOCKET_ERROR) {
		fprintf(stderr, "Couldn't bind listen socket!\n");
		WSACleanup();
		return false;
	}
	freeaddrinfo(serverAddress);
	return true;
}

int32_t main(int32_t _argc, char** _argv) {
	if (_argc < 2) {
		printf("Incorrect calling method!\nShould be ./server <filepath>\n");
		return EXIT_FAILURE;
	}
	if (!std::filesystem::exists(_argv[1])) {
		printf("Couldn't find file/directory named: %s\n", _argv[1]);
		return EXIT_FAILURE;
	}
	std::vector<std::filesystem::path> filepaths{ _argv[1] };
	SOCKET listenSock;
	if (!initWinSock()) 
		return EXIT_FAILURE;
	if (!createListenSocket(listenSock)) 
		return EXIT_FAILURE;

	if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
		fprintf(stderr, "Couldn't listen on bound socket!\n");
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}
	printf("Waiting for incoming connections...\n");
	SOCKET clientSock;
	clientSock = INVALID_SOCKET;
	while (clientSock == INVALID_SOCKET) {
		clientSock = accept(listenSock, NULL, NULL);
		if (clientSock == SOCKET_ERROR) clientSock = INVALID_SOCKET;
	 }

	constexpr const int32_t recvBufLen_c = 1024;
	constexpr const int32_t sendBufLen_c = 8 * 1024; //variable in future
	char recvBuf[recvBufLen_c];
	char sendBuf[sendBufLen_c];
	ZeroMemory(recvBuf, recvBufLen_c);
	ZeroMemory(sendBuf, sendBufLen_c);

	if (!setBufferSizes(clientSock, recvBufLen_c, sendBufLen_c)) {
		fprintf(stderr, "Couldn't set buffer size\n");
		closesocket(listenSock);
		closesocket(clientSock);
		WSACleanup();
		return EXIT_FAILURE;
	}

	FILE* inputFile = fopen(filepaths.front().string().c_str(), "rb");
	std::string currFilename = filepaths.front().filename().string();
	uint64_t leftSize = std::filesystem::file_size(filepaths.front());
	if (!inputFile) {			//todo supporting many files; ending on it for now
		fprintf(stderr, "Couldn't open file at %s\n", currFilename.c_str());
		closesocket(listenSock);
		WSACleanup();
		return EXIT_FAILURE;
	}

	header_t currHeader{};
	currHeader.msgSize = sizeof(header_t) + 8 + currFilename.size();
	currHeader.msgType = SEND_FILE_DATA;
	memcpy(sendBuf, &currHeader, sizeof(header_t));
	memcpy(sendBuf+16, &leftSize, 8);
	memcpy(sendBuf+24, currFilename.c_str(), currFilename.size());

	if (send(clientSock, sendBuf, currHeader.msgSize, 0) == SOCKET_ERROR) {	//todo
		fprintf(stderr, "Couldn't send data!\n");
		closesocket(listenSock);
		closesocket(clientSock);
		WSACleanup();
		return EXIT_FAILURE;
	}

	while (true) { //mainLoop
		int32_t bytesReceived = recv(clientSock,recvBuf,recvBufLen_c,0);
		debugf("bytesReceived: %i\n", bytesReceived);
		if (bytesReceived == 0)
			goto _mainloopEnd_;
		if (bytesReceived == recvBufLen_c) {
			__noop;
			//todo-check for more
		}
		
		header_t recvHeader = *(header_t*)recvBuf;
		if (bytesReceived < sizeof(header_t)) {
			fprintf(stderr, "Bytes received mismatch!\nGot %i should be %llu\n", bytesReceived, recvHeader.msgSize);
			pushHeader(sendBuf, currHeader, SEND_RETRY);
			send(clientSock, sendBuf, currHeader.msgSize, 0);
			continue;
		}

		switch (recvHeader.msgType) {
		case CONNECTION_KEEP: {
			debugf("CONNECTION_KEEP\n");
			debugf("bytes left = %llu\n", leftSize);
			if (leftSize == 0) {
				pushHeader(sendBuf, currHeader, CONNECTION_END);
				send(clientSock, sendBuf, currHeader.msgSize, 0);
				goto _mainloopEnd_;
			}
			size_t bytesLeft = sendBufLen_c - 16;
			if (leftSize < bytesLeft) bytesLeft = leftSize;
			leftSize -= bytesLeft;
			currHeader.msgType = SEND_TRY;
			currHeader.msgSize = 16 + bytesLeft;
			memcpy(sendBuf, &currHeader, sizeof(header_t));
			fread(sendBuf + 16, 1, bytesLeft, inputFile);
			send(clientSock, sendBuf, currHeader.msgSize, 0);
		} break;
		case CONNECTION_END: {
			debugf("CONNECTION_END\n");
			goto _mainloopEnd_;
		} break;
		case SEND_TRY: {
			debugf("SEND_TRY\n");
			pushHeader(sendBuf, currHeader, CONNECTION_END);
			send(clientSock, sendBuf, currHeader.msgSize, 0);
			goto _mainloopEnd_;
		} break;
		case SEND_RETRY: {
			debugf("SEND_RETRY\n");
			send(clientSock, sendBuf, currHeader.msgSize, 0);
		} break;
		case SEND_FILE_DATA: {
			debugf("SEND_FILE_DATA\n");
			pushHeader(sendBuf, currHeader, CONNECTION_END);
			send(clientSock, sendBuf, currHeader.msgSize, 0);
			goto _mainloopEnd_;
		} break;
		} //switch
	 }
_mainloopEnd_:
	debugf("Closing app...\n");
	closesocket(clientSock);
	closesocket(listenSock);
	WSACleanup();
	return EXIT_SUCCESS;
}