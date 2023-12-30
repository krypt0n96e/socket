#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Hàm để trích xuất tên tệp và định dạng tệp từ yêu cầu POST
int extract_filename(char *buffer, char **filename) {
    char *filename_start = strstr(buffer, "filename=\"");
    if (filename_start == NULL) {
        return 0;
    }

    filename_start += strlen("filename=\"");
    char *filename_end = strchr(filename_start, '\"');
    if (filename_end == NULL) {
        return 0;
    }

    int filename_length = filename_end - filename_start;
    *filename = (char *)malloc(filename_length + 1);
    strncpy(*filename, filename_start, filename_length);
    (*filename)[filename_length] = '\0';

    return 1;
}

int main() {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Tạo socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Cấu hình socket để định nghĩa IP và port sử dụng
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Failed to set socket options");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Gán địa chỉ IP và port đã cấu hình cho socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Failed to bind socket");
        exit(EXIT_FAILURE);
    }

    // Lắng nghe yêu cầu kết nối từ client
    if (listen(server_fd, 3) < 0) {
        perror("Failed to listen");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Chấp nhận kết nối từ client
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Failed to accept connection");
            exit(EXIT_FAILURE);
        }

        char buffer[BUFFER_SIZE] = {0};
        valread = read(new_socket, buffer, BUFFER_SIZE);
        printf("%s\n", buffer);

        // Xử lý yêu cầu POST từ client để nhận file
        if (strncmp(buffer, "POST /upload", 12) == 0) {
            char *response_header = "HTTP/1.1 200 OK\nContent-Type: text/plain; charset=UTF-8\n\n";
            send(new_socket, response_header, strlen(response_header), 0);

            char *filename = NULL;
            if (extract_filename(buffer, &filename)) {
                // Lưu dữ liệu từ yêu cầu POST vào tệp với tên và định dạng gốc
                FILE *fileptr = fopen(filename, "wb");
                if (fileptr == NULL) {
                    perror("Failed to create file");
                    exit(EXIT_FAILURE);
                }

                int n;
                while ((n = read(new_socket, buffer, BUFFER_SIZE)) > 0) {
                    fwrite(buffer, sizeof(char), n, fileptr);
                }

                fclose(fileptr);
                free(filename);
            } else {
                perror("Failed to extract filename");
            }
        }

        // Phản hồi yêu cầu GET từ client với giao diện web
        char *response_header = "HTTP/1.1 200 OK\nContent-Type: text/html; charset=UTF-8\n\n";
        char *response_body = "<html><head><title>File Transfer</title></head><body><h1>File Transfer</h1><form method=\"POST\" action=\"/upload\" enctype=\"application/x-www-form-urlencoded\"><input type=\"file\" name=\"file\" /><input type=\"submit\" value=\"Upload File\" /></form><script>var form=document.querySelector('form');form.addEventListener('submit',function(event){event.preventDefault();var fileInput=document.querySelector('input[type=\"file\"]');var file=fileInput.files[0];var formData=new FormData();formData.append('file',file);var xhr=new XMLHttpRequest();xhr.open('POST','/upload',true);xhr.onload=function(){if(xhr.status===200){alert('File uploaded successfully!');form.reset();}else{alert('File upload failed.');}};xhr.send(formData);});function disconnect(){var xhr=new XMLHttpRequest();xhr.open('GET','/disconnect',true);xhr.onload=function(){if(xhr.status===200){alert('Disconnected from server.');}else{alert('Disconnect failed.');}};xhr.send();}</script><button onclick=\"disconnect()\">Disconnect</button></body></html>";
        send(new_socket, response_header, strlen(response_header), 0);
        send(new_socket, response_body, strlen(response_body), 0);

        // Xử lý yêu cầu GET từ client để ngắt kết nối
        if (strncmp(buffer, "GET /disconnect", 15) == 0) {
            char *response = "HTTP/1.1 200 OK\nContent-Type: text/plain; charset=UTF-8\n\n";
            send(new_socket, response, strlen(response), 0);
            break;
        }

        close(new_socket);
    }

    return 0;
}
