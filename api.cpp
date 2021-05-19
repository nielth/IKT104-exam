#include "mbed.h"
#include "root_ca_cert.h"

#include "main.h"

#define TLS 443
#define TCP 80

Data data; // Struct in header

NetworkInterface *_net = NetworkInterface::get_default_instance();
nsapi_size_or_error_t result;

TLSSocket tls_socket;
TCPSocket tcp_socket;

void disconnect_network(){
    _net->disconnect();
}

void get_clock(){
    connect_server( "api.ipgeolocation.io",
                        "/timezone?apiKey=7b0bd535129a4a0fb01e726e5baeb05f", TLS);
    char *raw_json = raw_json_function(TLS);
    char *unix_clock = json_parser(raw_json, (char*)"date_time_unix");
    data.raw_clock = atoi(unix_clock);
    free(unix_clock);
    char *is_dst = json_parser(raw_json, (char*)"dst_savings");
    data.is_dst = atoi(is_dst);
    free(is_dst);           // Free heap memory !!!
    free(raw_json);
    tls_socket.close();
}

void get_weather(){
    connect_server(     "api.openweathermap.org",
                        "/data/2.5/"
                        "weather?id=3155041&units=metric&appid="
                        "e618b7ac4def9601371460b9e925ab70",
                        TCP);
    char *raw_json = raw_json_function(TCP);
    tcp_socket.close();
    char *weather = json_parser(raw_json, (char*)"description");    //json_func.cpp
    strcpy(data.weather, weather);
    free(raw_json);         // Free heap memory !!!
    free(weather);
    tcp_socket.close();
}

void connect_network() {
    printf("Connecting to the network...\r\n");
    if (!_net) {
        printf("Error! No network interface found.\r\n");
        return;
    }
    result = _net->connect();
    if (result != 0) {
        printf("Error! _net->connect() returned: %d\r\n", result);
        return;
    }
}

bool resolve_hostname(SocketAddress &address, const char hostname[]) {
    /* get the host address */
    printf("\nResolve hostname %s\r\n", hostname);
    nsapi_size_or_error_t result = _net->gethostbyname(hostname, &address);
    if (result != 0) {
        printf("Error! gethostbyname(%s) returned: %d\r\n", hostname, result);
        return false;
    }

    printf("%s address is %s\r\n", hostname,
            (address.get_ip_address() ? address.get_ip_address() : "None"));

    return true;
}

bool send_http_request(const char host_name[], const char api_message[], int REMOTE_PORT) {
    /* loop until whole request sent */
    int j = 0;
    char buffer[200];

    printf("%s\n%s\n", host_name, api_message);
    snprintf(buffer, sizeof(buffer),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            api_message, host_name);

    nsapi_size_t bytes_to_send = strlen(buffer);
    nsapi_size_or_error_t bytes_sent = 0;

    printf("\r\nSending message: \r\n%s", buffer);

    while (bytes_to_send) {
        if (REMOTE_PORT == TLS)     bytes_sent = tls_socket.send(buffer + bytes_sent, bytes_to_send);
        else if (REMOTE_PORT == TCP) bytes_sent = tcp_socket.send(buffer + bytes_sent, bytes_to_send);
        if (bytes_sent < 0) {
        printf("Error! _socket.send() returned: %d\r\n", bytes_sent);
        return false;
        } else {
        printf("sent %d bytes\r\n", bytes_sent);
        }

        bytes_to_send -= bytes_sent;
    }
    printf("Complete message sent\r\n");
    return true;
}

bool receive_http_response(int REMOTE_PORT) {
    size_t MAX_MESSAGE_RECEIVED_LENGTH = 200;
    char buffer[MAX_MESSAGE_RECEIVED_LENGTH];
    int remaining_bytes = MAX_MESSAGE_RECEIVED_LENGTH;
    int received_bytes = 0;

    /* loop until there is nothing received or we've ran out of buffer space */
    nsapi_size_or_error_t result = remaining_bytes;
    while (result > 0 && remaining_bytes > 0) {
        if      (REMOTE_PORT == TLS) result = tls_socket.recv(buffer + received_bytes, remaining_bytes);
        else if (REMOTE_PORT == TCP) result = tcp_socket.recv(buffer + received_bytes, remaining_bytes);
        if (result < 0) {
            printf("Error! _socket.recv() returned: %d\r\n", result);
            return false;
    }
    received_bytes += result;
    remaining_bytes -= result;
    }

    /* the message is larger but we only want the HTTP(S) response code in this function */
    printf("Received %d bytes:\r\n%.*s\r\n\r\n", received_bytes,
            strstr(buffer, "\n") - buffer, buffer);

    return true;
}

void connect_server(const char host_name[], const char api_message[], int REMOTE_PORT) {
    if      (REMOTE_PORT == TLS) result = tls_socket.open(_net);
    else if (REMOTE_PORT == TCP) result = tcp_socket.open(_net);
    if (result != 0) {
        printf("Error! _socket.open() returned: %d\r\n", result);
        return;
    }

    if (REMOTE_PORT == TLS) {
        result = tls_socket.set_root_ca_cert(root_ca_cert);
        if (result != NSAPI_ERROR_OK) {
            printf("Error: _socket.set_root_ca_cert() returned %d\n", result);
            return;
        }
        tls_socket.set_hostname(host_name);
    }

    SocketAddress address;
    if (!resolve_hostname(address, host_name)) {
        return;
    }

    address.set_port(REMOTE_PORT);
    printf("Opening connection to remote port %d\r\n", REMOTE_PORT);

    if      (REMOTE_PORT == TLS) result = tls_socket.connect(address);
    else if (REMOTE_PORT == TCP) result = tcp_socket.connect(address);

    if (result != 0) {
        printf("Error! _socket.connect() returned: %d\r\n", result);
        return;
    }

    /* exchange an HTTP(S) request and response */
    if (!send_http_request(host_name, api_message, REMOTE_PORT)) {
        return;
    }

    if (!receive_http_response(REMOTE_PORT)) {
        return;
    }
}

char* raw_json_function(int REMOTE_PORT){
    size_t MAX_MESSAGE_RECEIVED_LENGTH = 2000;      // Expect no longer than 2000 - 1 characters from request
    char buffer[MAX_MESSAGE_RECEIVED_LENGTH];
    int remaining_bytes = MAX_MESSAGE_RECEIVED_LENGTH;
    int received_bytes = 0;
    int json_start;

    nsapi_size_or_error_t result = remaining_bytes;
    while (result > 0 && remaining_bytes > 0) {

        if      (REMOTE_PORT == TLS) result = tls_socket.recv(buffer + received_bytes, remaining_bytes);
        else if (REMOTE_PORT == TCP) result = tcp_socket.recv(buffer + received_bytes, remaining_bytes);
                                
      if (result < 0) {
        printf("Error! _socket.recv() returned: %d\r\n", result);
        return NULL;
        }
        received_bytes += result;
        remaining_bytes -= result;
    }

    buffer[received_bytes + 1] = '\0';

    printf("received %d\r\n", received_bytes);

    char *raw_json;
    
    for (int i = 0; i < received_bytes + 1; i++) {
        if(buffer[i] == '{'){
            json_start = i;
            raw_json = (char *)malloc(sizeof(char) * received_bytes - json_start + 1); // Allocate memory to raw_json
            if (raw_json == NULL){
                printf("Out of memory!\n");
                return nullptr;
            }
            break;
        }
    }

    int json_raw_length = 0;
    int left_bracket = 0;
    int right_bracket = 0;

    for (int i = 0; i < sizeof buffer - json_start; i++) {
      raw_json[i] = buffer[json_start + i];
      if (buffer[json_start + i] == '{') {
        left_bracket++;
        } else if (buffer[json_start+i] == '}') {
            right_bracket++;
        }

        if (json_raw_length > 10 && left_bracket == right_bracket){
            json_raw_length = i;
            raw_json[json_raw_length + 1] = '\0';
            break;
        }
        json_raw_length++;
    }    
    return raw_json;
}