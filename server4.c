#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080
#define MAX_BUFFER_SIZE 1024

void send_file(int client_socket, const char *file_name) {
    // Open the file for reading
    FILE *file = fopen(file_name, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Read and send the file in chunks
    char buffer[MAX_BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_socket, buffer, bytesRead, 0);
    }

    // Close the file
    fclose(file);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len = sizeof(client_address);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        // Accept a connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

        // Send web interface to the client
        const char *web_interface = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                                   "<html><body>"
                                   "<form action=\"/upload\" method=\"post\" enctype=\"multipart/form-data\">"
                                   "Select File: <input type=\"file\" name=\"file\"><br>"
                                   "<input type=\"submit\" value=\"Send File\">"
                                   "</form>"
                                   "</body></html>";

        send(client_socket, web_interface, strlen(web_interface), 0);

        // Receive client request
        char request[MAX_BUFFER_SIZE];
        recv(client_socket, request, sizeof(request), 0);

        // Check if the request is a file upload
        if (strstr(request, "POST /upload") != NULL) {
            // Extract file name from the request (simplified, without proper parsing)
            const char *file_start = strstr(request, "filename=\"") + strlen("filename=\"");
            const char *file_end = strstr(file_start, "\"");
            char file_name[MAX_BUFFER_SIZE];
            strncpy(file_name, file_start, file_end - file_start);
            file_name[file_end - file_start] = '\0';

            // Send success message
            const char *success_message = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                                         "<html><body>File sent successfully!</body></html>";
            send(client_socket, success_message, strlen(success_message), 0);

            // Receive and save the file
            send_file(client_socket, file_name);
        }

        // Close the connection
        close(client_socket);
    }

    // Close the server socket
    close(server_socket);

    return 0;
}
