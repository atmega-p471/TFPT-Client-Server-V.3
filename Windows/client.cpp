#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

#define PORT 69
#define BLOCK_SIZE 512

using namespace std;

enum Opcode {
    RRQ = 1,
    WRQ = 2,
    DATA = 3,
    ACK = 4,
    ERROR_OP = 5
};

struct TftpPacket {
    uint16_t opcode;
    union {
        struct {
            char filename_mode[514];
        } request;
        struct {
            uint16_t block_num;
            char data[BLOCK_SIZE];
        } data;
        struct {
            uint16_t block_num;
        } ack;
    };
};

void send_file(SOCKET sock, const string& filename, sockaddr_in server_addr) {
    char buffer[1024];
    TftpPacket* wrq = (TftpPacket*)buffer;
    wrq->opcode = htons(WRQ);
    
    
    strncpy(wrq->request.filename_mode, filename.c_str(), sizeof(wrq->request.filename_mode) - 1);
    strncpy(wrq->request.filename_mode + filename.size() + 1, "octet", 
            sizeof(wrq->request.filename_mode) - filename.size() - 1);
    
    int packet_size = 4 + filename.size() + 6;
    socklen_t addr_len = sizeof(server_addr);
    
    sendto(sock, buffer, packet_size, 0,
        (sockaddr*)&server_addr, addr_len);

    TftpPacket ack_pkt;
    int ack_len = recvfrom(sock, (char*)&ack_pkt, sizeof(ack_pkt), 0,
        (sockaddr*)&server_addr, &addr_len);
    
    if (ack_len == SOCKET_ERROR) {
        cerr << "recvfrom failed: " << WSAGetLastError() << endl;
        return;
    }

    if (ntohs(ack_pkt.opcode) != ACK || ntohs(ack_pkt.ack.block_num) != 0) {
        cerr << "Invalid ACK received" << endl;
        return;
    }

    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "File not found: " << filename << endl;
        return;
    }
    
    uint16_t block_num = 1;
    while (true) {
        TftpPacket data_pkt;
        data_pkt.opcode = htons(DATA);
        data_pkt.data.block_num = htons(block_num);

        file.read(data_pkt.data.data, BLOCK_SIZE);
        streamsize read_size = file.gcount();
        int send_size = 4 + read_size;

        sendto(sock, (char*)&data_pkt, send_size, 0,
            (sockaddr*)&server_addr, addr_len);

        int ack_len = recvfrom(sock, (char*)&ack_pkt, sizeof(ack_pkt), 0,
            (sockaddr*)&server_addr, &addr_len);
        
        if (ack_len == SOCKET_ERROR) {
            cerr << "recvfrom failed: " << WSAGetLastError() << endl;
            break;
        }

        if (ntohs(ack_pkt.opcode) == ACK && 
            ntohs(ack_pkt.ack.block_num) == block_num) {
            if (read_size < BLOCK_SIZE) break;
            block_num++;
        }
    }
    file.close();
    cout << "File sent: " << filename << endl;
}

void receive_file(SOCKET sock, const string& filename, sockaddr_in server_addr) {
    char buffer[1024];
    TftpPacket* rrq = (TftpPacket*)buffer;
    rrq->opcode = htons(RRQ);
    
    
    strncpy(rrq->request.filename_mode, filename.c_str(), sizeof(rrq->request.filename_mode) - 1);
    strncpy(rrq->request.filename_mode + filename.size() + 1, "octet", 
            sizeof(rrq->request.filename_mode) - filename.size() - 1);
    
    int packet_size = 4 + filename.size() + 6;
    socklen_t addr_len = sizeof(server_addr);
    
    sendto(sock, buffer, packet_size, 0,
        (sockaddr*)&server_addr, addr_len);

    ofstream file(filename, ios::binary);
    if (!file) {
        cerr << "Cannot create file: " << filename << endl;
        return;
    }

    uint16_t block_num = 1;
    while (true) {
        TftpPacket data_pkt;
        int data_len = recvfrom(sock, (char*)&data_pkt, sizeof(data_pkt), 0,
            (sockaddr*)&server_addr, &addr_len);
        
        if (data_len == SOCKET_ERROR) {
            cerr << "recvfrom failed: " << WSAGetLastError() << endl;
            break;
        }

        if (ntohs(data_pkt.opcode) == DATA && 
            ntohs(data_pkt.data.block_num) == block_num) {
            
            int data_size = data_len - 4;
            file.write(data_pkt.data.data, data_size);

            TftpPacket ack_pkt;
            ack_pkt.opcode = htons(ACK);
            ack_pkt.ack.block_num = htons(block_num);
            
            sendto(sock, (char*)&ack_pkt, 4, 0,
                (sockaddr*)&server_addr, addr_len);

            if (data_size < BLOCK_SIZE) break;
            block_num++;
        }
    }
    file.close();
    cout << "File received: " << filename << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cout << "Usage: " << argv[0] << " <server_ip> <get|put> <file1> [file2] ..." << endl;
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed: " << WSAGetLastError() << endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    string mode = argv[2];
    for (int i = 3; i < argc; i++) {
        if (mode == "put") {
            send_file(sock, argv[i], server_addr);
        }
        else if (mode == "get") {
            receive_file(sock, argv[i], server_addr);
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}