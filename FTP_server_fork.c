/* Server side FTP to be act as file database and transfer server       */
/* Ngakan Putu Ariastu , 21 February 2018  */
/* Description : The Client side of the FTP, have function to request command for FTP */


/* Library declaration*/
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define PORT_CMD 2222 //control command port
#define PORT_DATA 2221 // data transfer port
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
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

/* main program */
int main(int argc, char* argv[])
{
    int serverfd_cmd,serverfd_data, cmd_socket, data_socket;
	struct sigaction sa;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

	/*********************************************/
    // Creating socket file descriptor for command
    if ((serverfd_cmd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket for command path failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverfd_cmd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &opt, sizeof(opt)))
    {
        perror("setsockopt for command path failed");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT_CMD );

    // Forcefully attaching socket to the port
    if (bind(serverfd_cmd, (struct sockaddr *)&address,
                                 sizeof(address))<0)
    {
        perror("bind for command path failed");
        exit(EXIT_FAILURE);
    }

	/*********************************************/
	// Creating socket file descriptor for data
    if ((serverfd_data = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket for data path failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverfd_data, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &opt, sizeof(opt)))
    {
        perror("setsockopt for data path failed");
        exit(EXIT_FAILURE);
    }
    address.sin_port = htons( PORT_DATA );

    // Forcefully attaching socket to the port
    if (bind(serverfd_data, (struct sockaddr *)&address,
                                 sizeof(address))<0)
    {
        perror("bind for data path failed");
        exit(EXIT_FAILURE);
    }
	

	/* Listen to connection with buffer size 3 */
    if (listen(serverfd_cmd, 3) < 0)
    {
        perror("listen for command failed");
        exit(EXIT_FAILURE);
    }
	 if (listen(serverfd_data, 3) < 0)
    {
        perror("listen for data failed");
        exit(EXIT_FAILURE);
    }
	
	sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
	
	while (1) 
	{ 
		printf ("Waiting For Connection...\n");
		
		if ((cmd_socket = accept(serverfd_cmd, (struct sockaddr *)&address,
		                   (socklen_t*)&addrlen))<0)
		{
		    perror("accept");
		    exit(EXIT_FAILURE);
		}
		printf ("Command Path to client Established\n");

		if ((data_socket = accept(serverfd_data, (struct sockaddr *)&address,
		                   (socklen_t*)&addrlen))<0)
		{
		    perror("accept");
		    exit(EXIT_FAILURE);
		}
		printf ("Data Path to client Established\n");
		
		if (!fork()) { // this is the child process
            close(serverfd_data); // child doesn't need the listener
			close(serverfd_cmd);
			
			
            /* variable for ftp */
			int not_done = 1;
			int actual_size;
			int valread;
			int command[1];
			int request = 0;
			int transfer = 0;
			char optn[10];
			char filename[15];
			FILE *in;
			uint8_t BUFFER[BUFFER_SIZE+1];

			while(not_done == 1)
			{
				printf (">> reading client command...\n");
				valread = read( cmd_socket , command, sizeof(command));
				if (command[0] == exit_server )
				{
					printf("exiting server\n");
					not_done = 0;
				}
				else if (command[0] == request_file)
					request = 1;

				if (request == 1)
				{
					printf(">> finding file...\n");
					valread = read( data_socket ,filename, 15);
					if ((in = fopen(filename,"rb")) > 0)
					{
						printf("! file found...\n");
						command[0] = found;
						send(cmd_socket , command , sizeof(command) , 0 );
						valread = read( cmd_socket , command, sizeof(command));
						if (command[0] == confirm_file)
						{
							printf("! file transfer confirmed...\n");
							transfer = 1;
							request = 0;
						}
						else if (command[0] == cancel_file)
						{
							printf("! file transfer cancelled...\n");
							request = 0;
							fclose(in);
						}
					}
					else
					{
						printf("! file not found...\n");
						command[0] = not_found;
						send(cmd_socket , command , sizeof(command) , 0 );
						request = 0;
					}
				}
				if (transfer == 1)
				{
					int n = 0;
					while (!feof(in)) 
					{
						command[0] = not_finished;
						send(cmd_socket , command , sizeof(command) , 0 );
					
					
						//for(int i = 0; i < 1025; i++) BUFFER[i] = "";
						actual_size = fread(BUFFER, sizeof(uint8_t), 1024, in);
						send(data_socket , &actual_size , sizeof(int) , 0 );
						BUFFER[BUFFER_SIZE] = checksum(BUFFER, actual_size);
						send(data_socket , BUFFER , sizeof(BUFFER) , 0 );
						printf("! sending...%d %d %u\n", n, actual_size, BUFFER[BUFFER_SIZE]);
						valread = read( cmd_socket , command, sizeof(command));
						while(command[0] != data_ok){
							send(data_socket , BUFFER , sizeof(BUFFER) , 0 );
							printf("! resending...%d %d %u\n", n, actual_size, BUFFER[BUFFER_SIZE]);
							valread = read( cmd_socket , command, sizeof(command));
						}
						if(command[0] == data_ok){
							printf("ACK %d\n",n);
						}
						n++;
					}
					command[0] = finished;
					send(cmd_socket , command , sizeof(command) , 0 );
					printf("! sending finished...\n");
					transfer = 0;
					fclose(in);
				}
			}
            exit(0);
        }
        close(cmd_socket);  // parent doesn't need this
		close(data_socket);  // parent doesn't need this
		
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
