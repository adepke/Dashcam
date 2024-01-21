#include "status.h"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

int socketHandle = -1;

bool initializeStatus() {
    socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketHandle < 0) {
        std::cerr << "Failed to open status socket!\n";
        return false;
    }

    sockaddr_in watchdogConnection;
    watchdogConnection.sin_family = AF_INET;
    watchdogConnection.sin_port = htons(watchdogPort);
    inet_aton("127.0.0.1", &watchdogConnection.sin_addr.s_addr);

    int error = connect(socketHandle, (sockaddr*)&watchdogConnection, sizeof(watchdogConnection));
    if (error != 0) {
        std::cerr << "Failed to connect to watchdog service!\n";
        return false;
    }

    return true;
}

void shutdownStatus() {
    if (socketHandle >= 0) {
        close(socketHandle);
        socketHandle = -1;
    }
}

void setState(const DashcamState state) {
    if (socketHandle >= 0) {
        char buffer[2];
        buffer[0] = static_cast<char>(state);
        buffer[1] = '\n';

        int error = send(socketHandle, buffer, 2, MSG_DONTWAIT);
        if (error < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "Failed to set status state!\n";
                return;
            }
        }
    }
}
