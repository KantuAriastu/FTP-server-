/* Client side FTP to receive file from server     */
/* Ngakan Putu Ariastu , 6 February 2018  */
/* Description : to be run on client side of the game, client will be playing
                the X token as the player 2*/

/* Library declaration */
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>

#define PORT_CMD "2222" //control command port
#define PORT_DATA "2221" // data transfer port
#define BUFFER_SIZE 1024

/* Control Command for FTP */
#define data_ok 200
#define data_error 201
#define request_file 202
#define confirm_file 203
#define cancel_file 204
#define found 205
#define not_found 206
#define finished 207
#define not_finished 208
#define exit_server 199

/* function declaration */
uint8_t checksum (uint8_t *data, int n_data);
int check_checksum (uint8_t *data, int n_data);
void display_help(void);

/* main program */
int main(int argc, char* argv[])
{
    /* variable for connection setting */
    struct sockaddr_in address;
    int cmd_sock = 0, data_sock = 0;
    struct sockaddr_in serv_addr;
	int status;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	
	// creating socket file descriptor for command
	// FIND IP address and convert to binary form
	if ((status = getaddrinfo(argv[1], PORT_CMD, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }
	
   
    if ((cmd_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    if (connect(cmd_sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
	printf ("Command Path to server Established\n");
	
	// creating socket file descriptor for data
	// FIND IP address and convert to binary form
	if ((status = getaddrinfo(argv[1], PORT_DATA, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }
	
    if ((data_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    if (connect(data_sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
	printf ("Data Path to server Established\n");
	
	/* variable for ftp */
	int not_done = 1;
	int actual_size;
	int request = 0;
	int transfer = 0;
	char opt[10];
	char filename[15];
	int command[1];
	int valread;
	FILE *out;
	uint8_t BUFFER[BUFFER_SIZE+1];
	
	printf("Welcome to FTP Server :\n");
	while (not_done == 1)
	{	

		if (request == 1)
		{
			printf (">> enter file name : ");
			scanf ("%s",filename);
			send(data_sock , filename , strlen(filename) , 0 );
			valread = read( cmd_sock , command, sizeof(command));
			
			if ((command[0] == found))
			{
				printf("! file found\n");
				printf(">> start transfer? (y/n)\n>> ");
				scanf("%s",opt);
				if (strcmp(opt,"y") == 0)
				{	
					command[0] = confirm_file;
					send(cmd_sock , command , sizeof(command) , 0 );
					transfer = 1;
					request = 0;
					out = fopen(argv[2],"wb");
				}
				else if(strcmp(opt,"n") == 0)
				{
					command[0] = cancel_file;
					send(cmd_sock , command , sizeof(command) , 0 );
					request = 0;
				}
			}
			else if ((command[0] == not_found))
			{
				printf("! file not found\n");
				request = 0;
			}	
		}
		
		if (transfer == 1)
		{
			int n = 0;
			valread = read( cmd_sock , command, sizeof(command));
			while (command[0] == not_finished)
			{	
				//for(int i = 0; i < 1025; i++) BUFFER[i] = "";
				valread = read( data_sock , &actual_size, sizeof(int));
				valread = read( data_sock , BUFFER, sizeof(BUFFER));
				if(n == 868) BUFFER[BUFFER_SIZE] = 14;
				while((check_checksum(BUFFER, actual_size)) > 0)
				{
					command[0] == data_error;
					send(cmd_sock , command , sizeof(command) , 0 );
					printf("! data error...\n");
					valread = read( data_sock , BUFFER, sizeof(BUFFER));
				}
				
				command[0] = data_ok;
				send(cmd_sock , command , sizeof(command) , 0 );
				int y = fwrite(BUFFER, sizeof(uint8_t), actual_size, out);
				printf("! receiving...%d %d %u\n", n, actual_size, BUFFER[BUFFER_SIZE]);
				valread = read( cmd_sock , command, sizeof(command));
				n++;
								
			}
			if (command[0] == finished)
			{
				//valread = read( data_sock , BUFFER, BUFFER_SIZE+1);
				//int y = fwrite(BUFFER, sizeof(uint8_t), 1024, out);
				transfer = 0;
				printf("! receiving finished...\n");
				fclose(out);
			}
		}
		
		printf("Enter Command\n");
		printf(">> ");
		scanf("%s",opt);
		if (strcmp(opt,"exit") == 0)
		{	
			command[0] = exit_server;
			send(cmd_sock , command , sizeof(command) , 0 );
			not_done = 0;
		}
		
		else if (strcmp(opt,"help") == 0)
			display_help();
		
		else if (strcmp(opt,"request") == 0)
		{
			request = 1;
			command[0] = request_file;
			send(cmd_sock , command , sizeof(command) , 0 );
			
		}
		else 
			printf("!invalid command, refer to 'help' for list of command\n");
	}
		
    return 0;
}

/* Function Implementation */
uint8_t checksum (uint8_t *data, int n_data)
{
  uint8_t result = 0x00;
  for (int i = 0; i<n_data; i++)
    result ^= data[i];

  return result;
}

int check_checksum (uint8_t *data, int n_data)
{
	uint8_t result = checksum (data,n_data);
	if (result == data [BUFFER_SIZE])
		return 0;
	else
		return 1;
}

void display_help(void)
{	
	printf("list of command: \n");
	printf("'request' : find requested file\n");
	printf("'exit' : exit ftp server\n");
	printf("'y', start transfer file after file is found\n");
	printf("'n', cancel transfer file after file is found\n");
}
