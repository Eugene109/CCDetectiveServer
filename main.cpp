#include <fstream>
#include <iostream>
#include <string>

#include <mswsock.h>
#include <ws2tcpip.h>

using namespace std;

#define PORT "1234"
const int BUFLEN = 1024;

int main() {

    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[BUFLEN];
    int recvbuflen = BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // No longer need server socket
    closesocket(ListenSocket);

    fstream fout;
    long long unsigned nextHeader = 0;
    unsigned offset = 0;
    // Receive until the peer shuts down the connection

    /*
    ----------------------

    ALGORITHM

    ----------------------
    */

    // 1. read from the input socket, filling 1024 bytes of the buffer
    // 2. parse out the header information, creating a file and taking note of the next header.
    // 3. read from the socket the file data in blocks of 1024 bytes until the next header.
    // 4. parse the next bit of header information, if needed, jump to the next block.
    // 5. continue.4

    iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
    while ((iResult - offset) > 0) {
        if (iResult < 0) {
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }
        if (recvbuf[offset] != '\"') {
            // idk man
            // break;
            cerr << "\nheader is misplaced, exiting with -1\n";
            // return -1;
            break;
        }
        string filename = "";
        offset++;
        for (; true; offset++) {
            if (offset >= recvbuflen) {
                offset = 0;
                iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
            }
            if (recvbuf[offset] == '\"') {
                offset++;
                if (offset >= recvbuflen) {
                    offset = 0;
                    iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
                }
                break;
            }
            filename += recvbuf[offset];
        }
        cout << filename << "\n";

        string fileSizeStr = "";
        for (; true; offset++) {
            if (offset >= recvbuflen) {
                offset = 0;
                iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
            }
            if (recvbuf[offset] == 'b') {
                offset++;
                if (offset >= recvbuflen) {
                    offset = 0;
                    iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
                }
                break;
            }
            fileSizeStr += recvbuf[offset];
        }
        cout << fileSizeStr << "\n";

        nextHeader = stoll(fileSizeStr);
        cout << "\n1nextHeader: " << nextHeader << "\n";
        fout = fstream(filename, ios_base::trunc | ios_base::out | ios_base::binary);

        while (nextHeader + offset >= iResult) {
            fout.write(recvbuf + offset, iResult - offset);
            nextHeader -= (iResult - offset);
            offset = 0;
            iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
            // if (iResult == 0) {
            //     break;
            // }
        }
        fout.write(recvbuf + offset, nextHeader);
        offset += nextHeader;

        fout.close();
    }

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}
