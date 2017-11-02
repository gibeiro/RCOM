#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "appLayer.h"

#define DEFAULT_PORT			"/dev/ttyS0"
#define DEFAULT_PAYLOAD_SIZE	512


int main(int argc, char** argv){

	int i = 2;
	char* path = NULL;
	char* port = DEFAULT_PORT;
	int payload_size = DEFAULT_PAYLOAD_SIZE;
	
	if(strcmp(argv[1],"send")){
		path = argv[i++];
	}else if(!strcmp(argv[1],"receive")){
		printf(
			"Usage:\n%s send <path> [-p <port>] [-s <payload_size>]\n%s receive [-p <port>]\n",
			argv[0],
			argv[0]
		);
		return 1;
	}
	
	for(;i < argc; i++){
		if(strcmp(argv[i],"-p")) port = argv[++i];
		else if(strcmp(argv[i],"-s")) payload_size = atoi(argv[++i]);		
	}

	if(path == NULL) return receiveFile(port);
	else return sendFile(path,port,payload_size);

}