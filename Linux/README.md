# Компиляция
g++ tftp_client.cpp -o client
g++ tftp_server.cpp -o server

# Запуск
./server

# Запуск клиента. Тест

# Отправка файлов
./client 127.0.0.1 put file1.txt image.jpg

# Получение файлов
./client 127.0.0.1 get file1.txt backup.zip