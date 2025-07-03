#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 69
#define BLOCK_SIZE 512
#define MAX_ERROR_MSG 128

using namespace std;

enum Opcode {
    RRQ = 1,
    WRQ = 2,
    DATA = 3,
    ACK = 4,
    ERROR_OP = 5
};

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (sockaddr*)&server_addr, sizeof(server_addr));
    cout << "TFTP Server started on port " << PORT << endl;

    while (true) {
        char buffer[1024];
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&client_addr, &addr_len);
        
        uint16_t opcode = ntohs(*(uint16_t*)buffer);
        
        if (opcode == RRQ) {
            
            string filename(buffer + 2);
            cout << "RRQ for file: " << filename << endl;
            
            ifstream file(filename, ios::binary);
            if (!file) {
                
                char error_pkt[4 + MAX_ERROR_MSG];
                uint16_t* op_ptr = (uint16_t*)error_pkt;
                *op_ptr = htons(ERROR_OP);
                
                uint16_t* code_ptr = (uint16_t*)(error_pkt + 2);
                *code_ptr = htons(1); 
                
                strcpy(error_pkt + 4, "File not found");
                sendto(sock, error_pkt, 4 + strlen("File not found") + 1, 0, 
                      (sockaddr*)&client_addr, addr_len);
                continue;
            }
            
            uint16_t block_num = 1;
            while (true) {
                
                uint16_t data_pkt[2];
                data_pkt[0] = htons(DATA);
                data_pkt[1] = htons(block_num);
                
                char full_pkt[4 + BLOCK_SIZE];
                memcpy(full_pkt, data_pkt, 4);
                
                file.read(full_pkt + 4, BLOCK_SIZE);
                streamsize read_size = file.gcount();
                int send_size = 4 + read_size;
                
                sendto(sock, full_pkt, send_size, 0, (sockaddr*)&client_addr, addr_len);
                
                
                char ack_buf[4];
                recvfrom(sock, ack_buf, sizeof(ack_buf), 0, (sockaddr*)&client_addr, &addr_len);
                
                uint16_t ack_opcode = ntohs(*(uint16_t*)ack_buf);
                uint16_t ack_block = ntohs(*(uint16_t*)(ack_buf + 2));
                
                if (ack_opcode == ACK && ack_block == block_num) {
                    if (read_size < BLOCK_SIZE) break;
                    block_num++;
                }
            }
            file.close();
        }
        else if (opcode == WRQ) {
            
            string filename(buffer + 2);
            cout << "WRQ for file: " << filename << endl;
            
            ofstream file(filename, ios::binary);
            if (!file) {
                char error_pkt[4 + MAX_ERROR_MSG];
                uint16_t* op_ptr = (uint16_t*)error_pkt;
                *op_ptr = htons(ERROR_OP);
                
                uint16_t* code_ptr = (uint16_t*)(error_pkt + 2);
                *code_ptr = htons(2); 
                
                strcpy(error_pkt + 4, "Access violation");
                sendto(sock, error_pkt, 4 + strlen("Access violation") + 1, 0, 
                      (sockaddr*)&client_addr, addr_len);
                continue;
            }
            
            
            uint16_t ack_buf[2];
            ack_buf[0] = htons(ACK);
            ack_buf[1] = 0; 
            sendto(sock, ack_buf, sizeof(ack_buf), 0, (sockaddr*)&client_addr, addr_len);
            
            uint16_t block_num = 1;
            while (true) {
                len = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&client_addr, &addr_len);
                
                uint16_t data_opcode = ntohs(*(uint16_t*)buffer);
                uint16_t data_block = ntohs(*(uint16_t*)(buffer + 2));
                
                if (data_opcode == DATA && data_block == block_num) {
                    int data_size = len - 4;
                    file.write(buffer + 4, data_size);
                    
                    
                    ack_buf[0] = htons(ACK);
                    ack_buf[1] = htons(block_num);
                    sendto(sock, ack_buf, sizeof(ack_buf), 0, (sockaddr*)&client_addr, addr_len);
                    
                    if (data_size < BLOCK_SIZE) break;
                    block_num++;
                }
            }
            file.close();
        }
    }
    close(sock);
    return 0;
}