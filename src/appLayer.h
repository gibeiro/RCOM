#ifndef APP_H
#define APP_H

#define C_DATA       0x01
#define C_START      0x02
#define C_END        0x03
#define T_FILE_SIZE  0x00
#define T_FILE_NAME  0x01

typedef enum {SENDER, RECEIVER} Client;

int sendFile(const char* path, const char* port, const int payload_size);
int receiveFile(const char* port);

#endif /*APP_H*/
