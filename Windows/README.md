# Компиляция
g++ server.cpp -o tftp_server.exe -lws2_32
g++ client.cpp -o tftp_client.exe -lws2_32

# Запуск
.\tftp_server.exe

# Запуск клиентаю. Тест

# Для отправки файлов
.\tftp_client.exe 127.0.0.1 put file1.txt file2.jpg

# Для получения файлов
.\tftp_client.exe 127.0.0.1 get file1.txt backup.zip