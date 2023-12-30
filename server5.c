#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define DEFAULT_PORT 8888
#define BUFFER_SIZE 1024

int conNect = 1;

void handleClient(int clientSocket);
void removeLines(char *filename); 

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int port = DEFAULT_PORT;

    // Tạo socket cho server
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Failed to create socket");
        return 1;
    }

    // Thiết lập thông tin địa chỉ cho server
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    // Gán địa chỉ cho socket của server
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Failed to bind the socket");
        return 1;
    }

    // Lắng nghe kết nối từ client
    if (listen(serverSocket, 1) == -1) {
        perror("Failed to listen for connections");
        return 1;
    }

    printf("Server is listening on port %d...\n", port);

    while (conNect) {
        // Chấp nhận kết nối từ client
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == -1) {
            perror("Failed to accept client connection");
            return 1;
        }

        printf("Client connected: %s\n", inet_ntoa(clientAddr.sin_addr));

        // Xử lý client trong một luồng riêng biệt
        handleClient(clientSocket);

        // Đóng socket của client
        close(clientSocket);
    }

    // Đóng socket của server
    close(serverSocket);

    return 0;
}

void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytesReceived;

    // Gửi giao diện web cho client
    const char* webPage =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html>\r\n"
        "<head><title>File Upload</title></head>\r\n"
        "<body>\r\n"
        "<h1>File Upload</h1>\r\n"
        "<form action=\"/upload\" method=\"post\" enctype=\"multipart/form-data\">\r\n"
        "<input type=\"file\" name=\"file\"/>\r\n"
        "<input type=\"submit\" value=\"Upload\"/>\r\n"
        "</form>\r\n"
        "<a href=\"/disconnect\">Disconnect</a>\r\n"
        "</body>\r\n"
        "</html>\r\n";

    if (send(clientSocket, webPage, strlen(webPage), 0) == -1) {
        perror("Failed to send web page to client");
        return;
    }

    // Nhận yêu cầu từ client
    bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
    if (bytesReceived == -1) {
        perror("Failed to receive data from client");
        return;
    }

    // Kiểm tra yêu cầu client
    if (strncmp(buffer, "POST /upload", 12) == 0) {
        // Tìm độ dài của file được gửi
        const char* contentLengthHeader = "Content-Length: ";
        char* contentLengthPos = strstr(buffer, contentLengthHeader);
        if (contentLengthPos == NULL) {
            perror("Invalid request: missing Content-Length header");
            return;
        }

        // Tìm vị trí bắt đầu của độ dài file
        contentLengthPos += strlen(contentLengthHeader);

        // Lấy giá trị độ dài file
        int contentLength = atoi(contentLengthPos);

        // Di chuyển con trỏ đến vị trí bắt đầu của file trong buffer
        char* filePos = strstr(buffer, "\r\n\r\n") + 4;

        // Tìm vị trí bắt đầu tên file
        const char* fileNameHeader = "filename=\"";
        char* fileNamePos = strstr(buffer, fileNameHeader);
        if (fileNamePos == NULL) {
            perror("Invalid request: missing filename in Content-Disposition header");
            return;
        }

        // Di chuyển con trỏ đến vị trí bắt đầu của tên file
        fileNamePos += strlen(fileNameHeader);

        // Tìm vị trí kết thúc của tên file
        char* fileNameEndPos = strchr(fileNamePos, '"');
        if (fileNameEndPos == NULL) {
            perror("Invalid request: missing closing quote in filename");
            return;
        }

        // Lấy tên file
        char fileName[256]; // Assuming maximum file name length is 255
        strncpy(fileName, fileNamePos, fileNameEndPos - fileNamePos);
        fileName[fileNameEndPos - fileNamePos] = '\0';

        // Tạo và mở file để lưu dữ liệu
        FILE* file = fopen(fileName, "wb");
        if (file == NULL) {
            perror("Failed to open file for writing");
            return;
        }

        // Ghi dữ liệu từ buffer vào file
        fwrite(filePos, 1, bytesReceived - (filePos - buffer), file);

        // Tiếp tục nhận dữ liệu từ client cho đến khi đủ độ dài file
        int remainingBytes = contentLength - (bytesReceived - (filePos - buffer));
        while (remainingBytes > 0) {
            bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
            if (bytesReceived == -1) {
                perror("Failed to receive data from client");
                fclose(file);
                return;
            }

            fwrite(buffer, 1, bytesReceived, file);
            remainingBytes -= bytesReceived;
        }

        printf("File uploaded successfully\n");
        webPage =
            "<html>"
            "<body>"
            "<p style=\"color: green;\">File selected successfully!</p>\r\n"
            "</body>"
            "</html>";
        send(clientSocket, webPage, strlen(webPage), 0);
        // Đóng file
        fclose(file);
        // Xoá dòng thừa
        removeLines(fileName);
    }

    // Đóng kết nối nếu client yêu cầu
    if (strncmp(buffer, "GET /disconnect", 15) == 0) {
        const char* disconnectResponse = "<html>"
                                         "<body>"
                                         "<h1>Disconnected</h1>"
                                         "</body>"
                                         "</html>";
        send(clientSocket, disconnectResponse, strlen(disconnectResponse), 0);
        conNect = 0;
        printf("Disconnected\n");
        return;
    }
}

void removeLines(char* filename) {
    FILE* sourceFile, * tempFile;
    char tempFilename[256]= "fixed_";
    strcat(tempFilename,filename);
    char buffer[1024];
    int count = 0;

    sourceFile = fopen(filename, "rb");
    if (sourceFile == NULL) {
        printf("Cannot open file %s\n", filename);
        return;
    }
    remove(filename);
    tempFile = fopen(tempFilename, "wb");
    if (tempFile == NULL) {
        printf("Cannot create temporary file\n");
        fclose(sourceFile);
        return;
    }

    // Bỏ qua 4 dòng đầu
    while (count < 4 && fgets(buffer, sizeof(buffer), sourceFile) != NULL) {
        count++;
    }

    if((strstr(filename,".txt")!=NULL)||(strstr(filename,".c")!=NULL))
        // Sao chép dữ liệu từ dòng thứ 5 đến hết (cho file text)
    while (fgets(buffer, sizeof(buffer), sourceFile) != NULL) {
        if ((strstr(buffer, "-----------------------------") != NULL)||(strstr(buffer, "------WebKitFormBoundary") != NULL)) {
            break;
        }
        fputs(buffer, tempFile);
    }
    else

    // Sao chép dữ liệu từ dòng thứ 5 đến hết (cho file bin)
    while (fread(buffer,1,sizeof(buffer),sourceFile)) {
        fwrite(buffer,1,sizeof(buffer),tempFile);
    }

    fclose(sourceFile);
    fclose(tempFile);

    printf("File fixed!\n");
}