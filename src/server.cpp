#include "shared.hpp"

#include <filesystem> //todo move to shared
#include <string>
#include <vector>

bool createListenSocket(SOCKET& _listenSock) {
	addrinfo* serverAddress, hints{};
	hints.ai_family = AF_INET;		//todo: add support for ipv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_IP;
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

	constexpr const int32_t recvBufLen_c = 1024;
	constexpr const int32_t sendBufLen_c = 8 * 1024; //variable in future
	char recvBuf[recvBufLen_c];
	char sendBuf[sendBufLen_c];
	ZeroMemory(recvBuf, recvBufLen_c);
	ZeroMemory(sendBuf, sendBufLen_c);

	if (setsockopt(listenSock, SOL_SOCKET, SO_SNDBUF, (const char*)&(sendBufLen_c), sizeof(sendBufLen_c)) == SOCKET_ERROR) 
		return EXIT_FAILURE;
	if (setsockopt(listenSock, SOL_SOCKET, SO_RCVBUF, (const char*)&(recvBufLen_c), sizeof(recvBufLen_c)) == SOCKET_ERROR) 
		return EXIT_FAILURE;

	if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
		fprintf(stderr, "Couldn't listen on bound socket!\n");
		closesocket(listenSock);
		WSACleanup();
		return 1;
	}
	printf("Waiting for incoming connections...\n");
	SOCKET clientSocket;
	clientSocket = INVALID_SOCKET;
	while (clientSocket == INVALID_SOCKET) {
		clientSocket = accept(listenSock, NULL, NULL);
		if (clientSocket == SOCKET_ERROR) clientSocket = INVALID_SOCKET;
	 }

	FILE* inputFile = fopen(filepaths.front().string().c_str(), "rb");
	uint64_t leftSize = std::filesystem::file_size(filepaths.front());
	if (!inputFile) {			//todo supporting many files; ending on it for now
		fprintf(stderr, "Couldn't open file at %s\n", filepaths.front().string().c_str());
		closesocket(listenSock);
		WSACleanup();
		return EXIT_FAILURE;
	}

	std::string currFilename = filepaths.front().string();
	header_t currHeader{};
	currHeader.msgSize = sizeof(header_t) + currFilename.size();
	currHeader.msgType = SEND_FILE_DATA;
	memcpy(sendBuf, &currHeader, sizeof(header_t));
	memcpy(sendBuf+16, &leftSize, 8);
	memcpy(sendBuf+24, currFilename.c_str(), currFilename.size());
	if (send(listenSock, sendBuf, currHeader.msgSize, 0) != 0) {	//todo
		fprintf(stderr, "Couldn't send data!\n");
		closesocket(listenSock);
		WSACleanup();
		return EXIT_FAILURE;
	}

	while (true) { //mainLoop
		int32_t bytesReceived = recv(listenSock,recvBuf,recvBufLen_c,0);
		if (bytesReceived == 0)
			goto _mainloopEnd_;
		if (bytesReceived == recvBufLen_c) {
			//todo-check for more
		}
		
		header_t recvHeader = *(header_t*)recvBuf;
		if (bytesReceived < sizeof(header_t)) {
			fprintf(stderr, "Bytes received mismatch!\nGot %i should be %llu\n", bytesReceived, recvHeader.msgSize);
			pushHeader(sendBuf, currHeader, SEND_RETRY);
			send(listenSock, sendBuf, currHeader.msgSize, 0);
			continue;
		}

		switch (recvHeader.msgType) {
		case CONNECTION_KEEP: {
			debugf("CONNECTION_KEEP\n");
			///--------------------------
			////TODO : SHIT HERE
			///--------------------------
			pushHeader(sendBuf, currHeader, CONNECTION_END);
			send(listenSock, sendBuf, currHeader.msgSize, 0);
			goto _mainloopEnd_;
		} break;
		case CONNECTION_END: {
			debugf("CONNECTION_END\n");
			goto _mainloopEnd_;
		} break;
		case SEND_TRY: {
			debugf("SEND_TRY\n");
			pushHeader(sendBuf, currHeader, CONNECTION_END);
			send(listenSock, sendBuf, currHeader.msgSize, 0);
			goto _mainloopEnd_;
		} break;
		case SEND_RETRY: {
			debugf("SEND_RETRY\n");
			send(listenSock, sendBuf, currHeader.msgSize, 0);
		} break;
		case SEND_FILE_DATA: {
			debugf("SEND_FILE_DATA\n");
			pushHeader(sendBuf, currHeader, CONNECTION_END);
			send(listenSock, sendBuf, currHeader.msgSize, 0);
			goto _mainloopEnd_;
		} break;
		} //switch
	 }
_mainloopEnd_:
	debugf("Leaving app gracefully\n");
	WSACleanup();
	return EXIT_SUCCESS;
}