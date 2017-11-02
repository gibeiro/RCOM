#include "appLayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int fileSize(FILE* file){
	fseek(file, 0, SEEK_END);
	int size = ftell(file);
	fseek(file, 0, SEEK_SET);
	return size;
}

char* fileName(const char* path){
	char* file_name = path + strlen(path);
	while(*file_name != '/' && --file_name > path);
	return file_name;
}

int sendFile(const char* path, const char* port, const int payload_size){

	FILE* file = fopen(path,"r");
	int file_size = fileSize(file);
	char* file_name = fileName(path);
	int i = file_size;
	char packet[1024];
	char seq_nr = 0;

	int fd = llopen(port,SENDER);

	packet[0] = C_START;
	packet[1] = T_FILE_SIZE;
	packet[2] = sizeof(int);
	memcpy(packet+3, &file_size, sizeof(int));
	packet[7] = T_FILE_NAME;
	packet[8] = strlen(file_name);
	strcpy(packet+9, file_name);

	llwrite(fd, packet, strlen(packet));

	for(; i > 0; i -= payload_size){
		memset(packet,'\0',sizeof(packet));
		packet[0] = C_DATA;
		packet[1] = seq_nr++;
		int bytes_written = fread(packet+4, sizeof(char), payload_size, file);
		packet[2] = bytes_written >> 8;
		packet[3] = bytes_written & 255;
		llwrite(fd,packet,strlen(packet));
	}

	memset(packet,'\0',sizeof(packet));
	packet[0] = C_END;
	packet[1] = T_FILE_SIZE;
	packet[2] = sizeof(int);
	memcpy(packet+3, &file_size, sizeof(int));
	packet[7] = T_FILE_NAME;
	packet[8] = strlen(file_name);
	strcpy(packet+9,file_name);

	llwrite(fd, packet, strlen(packet));
	llclose(fd);

	return 0;
}

int receiveFile(const char* port){
	FILE* file;
	char packet[1024];
	char file_name[64];
	int fd = llopen(port, RECEIVER);
	while(llread(fd,packet)){
		switch(packet[0]){
			case C_START:{
				int l1 = packet[2] << 8 + packet[3];
				int l2 = packet[5+l1] << 8 + packet[6+l1];
				memcpy(file_name, packet+len1+6, len2);
				file = fopen(file_name,"w");
				break;
			}case C_DATA:{
				int payload_size = packet[2] << 8 + packet[3];
				fwrite(packet+4, sizeof(char), payload_size, file);
				break;
			}case C_END:
				fclose(file);
				return llclose(fd);
		}
		memset(packet,'\0',sizeof(packet));
	}
}
