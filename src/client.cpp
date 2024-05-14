#include "shared.hpp"

bool createClientSocket(char* _targetIp, SOCKET& _clientSock, addrinfo*& _serverAddr) {
	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if (getaddrinfo(_targetIp, serverPort_c, &hints, &_serverAddr) != 0) {
		fprintf(stderr, "Couldn't resolve server ip addr - %s\n", _targetIp);
		fprintf(stderr, "Wsa:%i", WSAGetLastError());
		return false;
	}
	_clientSock = socket(_serverAddr->ai_family, _serverAddr->ai_socktype, _serverAddr->ai_protocol);
	if (_clientSock == SOCKET_ERROR || _clientSock == INVALID_SOCKET) {
		fprintf(stderr, "Couldn't create socket\n");
		fprintf(stderr, "Wsa:%i", WSAGetLastError());
		return false;
	}
	return true;
}

inline bool setBufferSizes(SOCKET& _sock, int32_t _recvBuffSize, int32_t _sendBuffSize) {
	if (setsockopt(_sock, SOL_SOCKET, SO_SNDBUF, (const char*)&(_recvBuffSize), sizeof(_recvBuffSize)) == SOCKET_ERROR)
		return false;
	if (setsockopt(_sock, SOL_SOCKET, SO_SNDBUF, (const char*)&(_sendBuffSize), sizeof(_sendBuffSize)) == SOCKET_ERROR)
		return false;
	return true;
}

int32_t main(int32_t _argc, char** _argv) {
	if (_argc < 2) {
		printf("Incorrect calling method!\nShould be ./client <server ip>\n");
		return EXIT_FAILURE;
	}
	if (!initWinSock())
		return EXIT_FAILURE;
	char* targetIp = _argv[1];
	debugf("Target ip:'%s'\n", targetIp);
	SOCKET clientSock = INVALID_SOCKET; 
	addrinfo* serverAddr;
	if (!createClientSocket(targetIp, clientSock, serverAddr)) {
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

	constexpr const int32_t recvBufLen_c = 8 * 1024; //variable in future
	constexpr const int32_t sendBufLen_c = 1024;
	char recvBuf[recvBufLen_c], sendBuf[sendBufLen_c];
	ZeroMemory(recvBuf, recvBufLen_c);
	ZeroMemory(sendBuf, sendBufLen_c);
	if (!setBufferSizes(clientSock, recvBufLen_c, sendBufLen_c)) {
		fprintf(stderr, "Couldn't set socket buffer size\n");
		closesocket(clientSock);
		WSACleanup();
		return EXIT_FAILURE;
	}
	
	header_t currHeader{};
	FILE* currFile = nullptr;
	while (true) { //mainLoop
		ZeroMemory(recvBuf, recvBufLen_c);
		ZeroMemory(sendBuf, sendBufLen_c);
		int32_t bytesReceived = recv(clientSock, recvBuf, recvBufLen_c, 0);
		debugf("bytesReceived: %i\n", bytesReceived);
		if (bytesReceived == 0) {
			goto _mainloopEnd_;
		}
		if (bytesReceived == recvBufLen_c) {
			//todo-check for more
		}

		header_t recvHeader = *(header_t*)recvBuf;
		debugf("header: id = %llu | size = %llu\n", recvHeader.msgType, recvHeader.msgSize);
		if (bytesReceived < sizeof(header_t)) {
			fprintf(stderr, "Bytes received mismatch!\nGot %i should be %llu\n", bytesReceived, recvHeader.msgSize);
			pushHeader(sendBuf, currHeader, SEND_RETRY);
			send(clientSock, sendBuf, currHeader.msgSize, 0);
			continue;
		}

		switch (recvHeader.msgType) {
		case CONNECTION_KEEP: {
			debugf("CONNECTION_KEEP\n");
			pushHeader(sendBuf, currHeader, CONNECTION_KEEP);
			send(clientSock, sendBuf, currHeader.msgSize, 0);
		} break;
		case CONNECTION_END: {
			debugf("CONNECTION_END\n");
			goto _mainloopEnd_;
		} break;
		case SEND_TRY: {
			debugf("SEND_TRY\n");
			if (currFile == nullptr)
				debugf("no file to be written to!\n");
			size_t bytesWritten = fwrite(recvBuf+16, 1, recvHeader.msgSize - 16, currFile); // potentially should check bytes read by fwrite
			printf("bytes written:%u\n", bytesWritten);
			ZeroMemory(recvBuf, recvHeader.msgSize);
			pushHeader(sendBuf, currHeader, CONNECTION_KEEP);
			send(clientSock, sendBuf, currHeader.msgSize, 0);
		} break;
		case SEND_RETRY: {
			debugf("SEND_RETRY\n");
			send(clientSock, sendBuf, currHeader.msgSize, 0);
		} break;
		case SEND_FILE_DATA: {
			debugf("SEND_FILE_DATA\n");
			uint64_t currFileSize = *(uint64_t*)(recvBuf+16);
			std::string currFilename(recvBuf+24);	// sizeof(header) + sizeof(uint64_t)
			printf("Downloading file named \"%s\" with size of %llu bytes\n", currFilename.c_str(), currFileSize);
			if (currFile) fclose(currFile);
			currFile = fopen(currFilename.c_str(), "wb");
			pushHeader(sendBuf, currHeader, CONNECTION_KEEP);
			send(clientSock, sendBuf, currHeader.msgSize, 0);
		} break;
		} //switch
	}
_mainloopEnd_:
	if(currFile)
		fclose(currFile);
	closesocket(clientSock);
	WSACleanup();
	return EXIT_SUCCESS;
}