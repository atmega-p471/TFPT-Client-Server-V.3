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
        struct {
            uint16_t error_code;
            char error_msg[100];
        } error;
    };
};

int main() {
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
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "Bind failed: " << WSAGetLastError() << endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "TFTP Server started on port " << PORT << endl;

    while (true) {
        char buffer[1024];
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);

        int packet_len = recvfrom(sock, buffer, sizeof(buffer), 0,
            (sockaddr*)&client_addr, &client_addr_len);
        
        if (packet_len == SOCKET_ERROR) {
            cerr << "recvfrom failed: " << WSAGetLastError() << endl;
            continue;
        }

        TftpPacket* packet = (TftpPacket*)buffer;
        uint16_t opcode = ntohs(packet->opcode);

        if (opcode == RRQ) {
            string filename = packet->request.filename_mode;
            cout << "RRQ for file: " << filename << endl;

            ifstream file(filename, ios::binary);
            if (!file) {
                TftpPacket err_pkt;
                err_pkt.opcode = htons(ERROR_OP);
                err_pkt.error.error_code = htons(1);
                strncpy(err_pkt.error.error_msg, "File not found", sizeof(err_pkt.error.error_msg) - 1);
                err_pkt.error.error_msg[sizeof(err_pkt.error.error_msg) - 1] = '\0';

                sendto(sock, (char*)&err_pkt, 4 + strlen(err_pkt.error.error_msg) + 1, 0,
                    (sockaddr*)&client_addr, client_addr_len);
                continue;
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
                    (sockaddr*)&client_addr, client_addr_len);

                TftpPacket ack_pkt;
                int ack_len = recvfrom(sock, (char*)&ack_pkt, sizeof(ack_pkt), 0,
                    (sockaddr*)&client_addr, &client_addr_len);
                
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
        }
        else if (opcode == WRQ) {
            string filename = packet->request.filename_mode;
            cout << "WRQ for file: " << filename << endl;

            ofstream file(filename, ios::binary);
            if (!file) {
                TftpPacket err_pkt;
                err_pkt.opcode = htons(ERROR_OP);
                err_pkt.error.error_code = htons(2);
                strncpy(err_pkt.error.error_msg, "Cannot create file", sizeof(err_pkt.error.error_msg) - 1);
                err_pkt.error.error_msg[sizeof(err_pkt.error.error_msg) - 1] = '\0';

                sendto(sock, (char*)&err_pkt, 4 + strlen(err_pkt.error.error_msg) + 1, 0,
                    (sockaddr*)&client_addr, client_addr_len);
                continue;
            }

            TftpPacket ack_pkt;
            ack_pkt.opcode = htons(ACK);
            ack_pkt.ack.block_num = 0;
            
            sendto(sock, (char*)&ack_pkt, 4, 0,
                (sockaddr*)&client_addr, client_addr_len);

            uint16_t block_num = 1;
            while (true) {
                TftpPacket data_pkt;
                int data_len = recvfrom(sock, (char*)&data_pkt, sizeof(data_pkt), 0,
                    (sockaddr*)&client_addr, &client_addr_len);
                
                if (data_len == SOCKET_ERROR) {
                    cerr << "recvfrom failed: " << WSAGetLastError() << endl;
                    break;
                }

                if (ntohs(data_pkt.opcode) == DATA && 
                    ntohs(data_pkt.data.block_num) == block_num) {
                    
                    int data_size = data_len - 4;
                    file.write(data_pkt.data.data, data_size);

                    ack_pkt.ack.block_num = htons(block_num);
                    sendto(sock, (char*)&ack_pkt, 4, 0,
                        (sockaddr*)&client_addr, client_addr_len);

                    if (data_size < BLOCK_SIZE) break;
                    block_num++;
                }
            }
            file.close();
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}