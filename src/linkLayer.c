#include "linkLayer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <strings.h>

#define FLAG		0x7E
#define ESC			0x7D
#define FLAG_SUB	0x5E
#define ESC_SUB		0x5D
#define A1			0x03
#define A2			0x01
#define SET			0x03
#define DISC		0x0B
#define UA			0x07
#define RR			(parity<<7)|0x05
#define REJ			(parity<<7)|0x01
#define START		0
#define FLAG_RCV	1
#define A_RCV		2
#define C_RCV		3
#define BCC_OK		4
#define STOP		5
#define MAX_TRIES 3

static Client client;
static Terminal	terminal;

static char sequence_nr = 0;
static char num_transmissions = 0;

int sendFrame(const char* frame, int* frame_size);
int receiveFrame(const char* frame, int* frame_size);
int bcc(const char* buf, const int len);
int stuffing(char* frame, int* frame_length);
int destuffing(char* frame, int* frame_length);
void configTerminal();
void resetTerminal();
int checkSupervisionFrame();
int checkInformationFrame();

int llopen(const char* port, Client c){

	/* sets the client type: SENDER or RECEIVER*/
	client = c;

	/* configures the serial port's terminal */
	configTerminal();

	/* opens serial port as a file */
	int fd = open(port, O_RDWR | O_NOCTTY);

	/* creates a frame buffer */
	char frame[1024];
	int frame_size;

	switch(client){
		case SENDER:
			/* prepares a SET frame */
			frame_size = 5;
			frame[0] = FLAG;
			frame[1] = A1;
			frame[2] = SET;
			frame[3] = bcc(frame+1,2);
			frame[4] = FLAG;

			/* sends the SET frame */
			if(!sendFrame(frame, &frame_size)) return -1;

			/* receives a response frame */
			if(!receiveFrame(frame, &frame_size)) return -1;

			/* checks if the response is a valid UA frame */
			if(!checkSupervisionFrame(frame, A1, UA)) return -1;
		break;

		case RECEIVER:
			/* receives a request frame */
			if(!receiveFrame(frame, &frame_size)) return -1;

			/* checks if the request is a valid SET frame */
			if(!checkSupervisionFrame(frame, A1, SET)) return -1;

			/* prepares a UA frame */
			frame_size = 5;
			frame[0] = FLAG;
			frame[1] = A1;
			frame[2] = UA;
			frame[3] = bcc(frame+1, 2);
			frame[4] = FLAG;

			/* sends the UA frame */
			if(!sendFrame(frame, &frame_size))	return -1;
		break;
	}

	/* return the serial port's file descriptor */
	return fd;
}

int llwrite(int fd, char* buf, int len){

	/* creates a frame buffer */
	char frame[1024];
	int frame_size;

	/* prepares a I frame */
	frame_size = 5 + len;
	frame[0] = FLAG;
	frame[1] = A1;
	frame[2] = sequence_nr << 6;
	frame[3] = bcc(frame+1, 2);
	memcpy(frame+4, buf, len);
	frame[4+len] = bcc(buf, len);
	frame[5+len] = FLAG;

	/* stuffs the I frame */
	stuffing(frame, &frame_size);

	/* sends the I frame */
	int nr_bytes_sent = sendFrame(frame, &frame_size);
	if(!nr_bytes_sent) return -1;

	/* receives a response frame */
	if(!receiveFrame(frame, &frame_size)) return -1;	

	/* checks if the response is a valid REJ frame */
	if(checkSupervisionFrame(frame, A1, REJ|(parity << 7))){

		/* increments and checks the reattempt counter */
		if(++num_transmissions == MAX_TRIES){
			num_transmissions = 0;
			return -1;
		}

		/* reattempts to send the I frame */
		llwrite(fd,buf,len);
	}

	/* checks if the response isn't a valid RR frame */
	else if(!checkSupervisionFrame(frame, A1, RR|(parity << 7))) return -1;

	/* sets the new sequence number from the response frame */
	sequence_nr = frame[2] >> 7;

	/* resets the transmission counter*/
	num_transmissions = 0;

	/* returns the number of bytes sent */
	return nr_bytes_sent;
}

int llread(int fd, char* buf){

	/* creates a frame buffer */
	char frame[1024];
	int frame_size;

	/* receives a command frame */
	if(!receiveFrame(frame, &frame_size)) return -1;

	/* destuffs the command frame */
	destuff(frame, &frame_size);
	int nr_bytes_received = frame_size;

	/* sets the new sequence number from the command frame */
	sequence_nr = ~(frame[2] >> 6)) & 0x01

	/* checks if the commands is a valid I frame */
	if(!checkInformationFrame(frame, A1, (~parity & 0x01) << 6)){	
		/* prepares a REJ frame */
		frame_size = 5 ;
		frame[0] = FLAG;
		frame[1] = A1;
		frame[2] = REJ|(sequence_nr << 7);
		frame[3] = bcc(frame+1, 2);
		frame[4] = FLAG;

		/* sends the REJ frame */
		if(!sendFrame(frame, &frame_size)) return -1;

		/* reattempts to receive a valid command frame */
		return llread(fd, buf);
	}

	/* prepares a RR frame */
	frame_size = 5 ;
	frame[0] = FLAG;
	frame[1] = A1;
	frame[2] = RR|(sequence_nr << 7);
	frame[3] = bcc(frame+1, 2);
	frame[4] = FLAG;

	/* sends the RR frame */
	if(!sendFrame(frame, &frame_size)) return -1;

	/* returns the number of bytes received */
	return nr_bytes_received;

}

int llclose(int fd){
	/* creates a frame buffer */
	char frame[1024];
	int frame_size;

	switch(client){
		case SENDER:
			/* prepares a DISC frame */
			frame_size = 5;
			frame[0] = FLAG;
			frame[1] = A1;
			frame[2] = DISC;
			frame[3] = bcc(frame+1,2);
			frame[4] = FLAG;

			/* sends the DISC frame */
			if(!sendFrame(fd, frame, &frame_size)) return -1;

			/* receives a response frame */
			if(!receiveFrame(fd, frame, &frame_size)) return -1;

			/* checks if the response is a valid DISC frame */
			if(!checkSupervisionFrame(frame, A2, DISC)) return -1;

			/* prepares a UA frame */
			frame_size = 5;
			frame[0] = FLAG;
			frame[1] = A2;
			frame[2] = UA;
			frame[3] = bcc(frame+1,2);
			frame[4] = FLAG;

			/* sends the UA frame */
			if(!sendFrame(fd, frame, &frame_size)) return -1;

		break;
		case RECEIVER:
			/* receives a response frame */
			if(!receiveFrame(fd, frame, &frame_size)) return -1;

			/* checks if the response is a valid DISC frame */
			if(!checkSupervisionFrame(frame, A1, DISC)) return -1;

			/* prepares a DISC frame */
			frame_size = 5;
			frame[0] = FLAG;
			frame[1] = A2;
			frame[2] = DISC;
			frame[3] = bcc(frame+1,2);
			frame[4] = FLAG;

			/* sends the DISC frame */
			if(!sendFrame(fd, frame, &frame_size)) return -1;

			/* receives a response frame */
			if(!receiveFrame(fd, frame, &frame_size)) return -1;

			/* checks if the response is a valid UA frame */
			if(!checkSupervisionFrame(frame, A2, UA)) return -1;

		break;
	}

	/* closes serial port */
	close(fd);

	/* resets the serial port's terminal config */
	resetTerminal();

	return 0;
}

int stuffing(char* frame, int* frame_size){
	int i;
	char octet;
	for(i = 1; i < *frame_size - 1; i++){
		octet = frame[i];
		if(octet == ESC || octet == FLAG){
			memmove(frame+i+2, frame+i+1, *frame_size++ -i-1);
			if(octet == FLAG){
				frame[i++] = ESC;
				frame[i] = FLAG_SUB;
			}
			else frame[++i] = ESC_SUB;
		}
	}
	return *frame_size;
}

int destuffing(char* frame, int* frame_size){
	int i;
	char octet;
	for(i = 1; i < *frame_size - 1; i++){
		octet = frame[i];
		if(octet == ESC){
			if(frame[i+1] == FLAG_SUB) frame[i] = FLAG;
			else frame[i] = ESC;
			memmove(frame+i+1, frame+i+2, *frame_size-- -i-1);
		}
	}
	return *frame_size;
}

char bcc(const char* buf, const int size){
	char bcc = *buf;
	int i;
	for(i = 1; i < size; i++) bcc ^= buf[i];
	return bcc;
}


int sendFrame(const char* frame){
return 0;
}

int receiveFrame(const char* frame){
return 0;
}

int checkSupervisionFrame(const char* frame, const char a, const char c){
	return	frame[0] == FLAG						&&
	 				frame[1] == a								&&
					frame[2] == c								&&
					frame[3] == bcc(frame+1,2))	&&
					frame[4] == FLAG;
}

int checkInformationFrame(const char* frame, const int frame_size, const char a){
	return	frame[0] == FLAG															&&
					frame[1] == a																	&&
					frame[2] == parity << 6												&&
					frame[3] == bcc(frame+1,2))										&&
					frame[frame_size-2] == bcc(frame+4,length-6)	&&
					frame[frame_size-1] == FLAG;
}

void configTerminal(){
	tcgetattr(fd,&terminal);
	Terminal new_terminal;
	memset(&new_terminal,'\0',sizeof(new_terminal));
	new_terminal.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	new_terminal.c_iflag = IGNPAR;
	new_terminal.c_oflag = 0;
	new_terminal.c_lflag = 0;
	new_terminal.c_cc[VTIME] = 0;
	new_terminal.c_cc[VMIN] = 1;
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd,TCSANOW,&new_terminal);
}

void resetTerminal(){
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd,TCSANOW,&terminal);
}
