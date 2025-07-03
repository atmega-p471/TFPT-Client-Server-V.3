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
#define MAX_FILENAME 256

using namespace std;

enum Opcode {
    RRQ = 1,
    WRQ = 2,
    DATA = 3,
    ACK = 4,
    ERROR_OP = 5
};

void send_file(int sock, const string& filename, sockaddr_in server_addr) {
    char buffer[1024];
    
    
    uint16_t* opcode = (uint16_t*)buffer;
    *opcode = htons(WRQ);
    
    char* ptr = buffer + 2;
    strcpy(ptr, filename.c_str());
    ptr += filename.size() + 1;
    strcpy(ptr, "octet");
    ptr += strlen("octet") + 1;
    
    socklen_t addr_len = sizeof(server_addr);
    int packet_size = ptr - buffer;
    sendto(sock, buffer, packet_size, 0, (sockaddr*)&server_addr, addr_len);
    
    
    recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&server_addr, &addr_len);
    
    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "File not found: " << filename << endl;
        return;
    }
    
    uint16_t block_num = 1;
    while (true) {
        
        uint16_t* opcode_ptr = (uint16_t*)buffer;
        *opcode_ptr = htons(DATA);
        
        uint16_t* block_ptr = (uint16_t*)(buffer + 2);
        *block_ptr = htons(block_num);
        
        file.read(buffer + 4, BLOCK_SIZE);
        streamsize read_size = file.gcount();
        int send_size = 4 + read_size;
        
        sendto(sock, buffer, send_size, 0, (sockaddr*)&server_addr, addr_len);
        
        
        recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&server_addr, &addr_len);
        
        uint16_t recv_opcode = ntohs(*(uint16_t*)buffer);
        uint16_t recv_block = ntohs(*(uint16_t*)(buffer + 2));
        
        if (recv_opcode == ACK && recv_block == block_num) {
            if (read_size < BLOCK_SIZE) break;
            block_num++;
        }
    }
    file.close();
    cout << "File sent: " << filename << endl;
}

void receive_file(int sock, const string& filename, sockaddr_in server_addr) {
    char buffer[1024];
    
    
    uint16_t* opcode = (uint16_t*)buffer;
    *opcode = htons(RRQ);
    
    char* ptr = buffer + 2;
    strcpy(ptr, filename.c_str());
    ptr += filename.size() + 1;
    strcpy(ptr, "octet");
    ptr += strlen("octet") + 1;
    
    socklen_t addr_len = sizeof(server_addr);
    int packet_size = ptr - buffer;
    sendto(sock, buffer, packet_size, 0, (sockaddr*)&server_addr, addr_len);
    
    ofstream file(filename, ios::binary);
    if (!file) {
        cerr << "Cannot create file: " << filename << endl;
        return;
    }
    
    uint16_t block_num = 1;
    while (true) {
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&server_addr, &addr_len);
        
        uint16_t recv_opcode = ntohs(*(uint16_t*)buffer);
        uint16_t recv_block = ntohs(*(uint16_t*)(buffer + 2));
        
        if (recv_opcode == DATA && recv_block == block_num) {
            int data_size = len - 4;
            file.write(buffer + 4, data_size);
            
            
            uint16_t ack_buffer[2];
            ack_buffer[0] = htons(ACK);
            ack_buffer[1] = htons(block_num);
            sendto(sock, ack_buffer, 4, 0, (sockaddr*)&server_addr, addr_len);
            
            if (data_size < BLOCK_SIZE) break;
            block_num++;
        }
    }
    file.close();
    cout << "File received: " << filename << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cout << "How to use " << argv[0] << " server_ip / get|put / files (file1, file2 ...)" << endl;
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    string mode = argv[2];
    for (int i = 3; i < argc; i++) {
        if (mode == "put") {
            send_file(sock, argv[i], server_addr);
        } else if (mode == "get") {
            receive_file(sock, argv[i], server_addr);
        }
    }

    close(sock);
    return 0;
}
