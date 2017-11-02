#ifndef LINK_H
#define LINK_H

#define BAUDRATE B9600
#define COM1 	 0x3F8
#define COM2 	 0x2F8
#define COM3 	 0x3E8
#define COM4 	 0x2E8

typedef enum {
  SENDER,
  RECEIVER
} Client;

typedef struct termios Terminal;

int llopen(const char* port, Client c);
int llwrite(int fd, char* buf, int len);
int llread(int fd, char* buf);
int llclose(int fd);

#endif /*LINK_H*/
