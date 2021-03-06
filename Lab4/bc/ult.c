// latest

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <poll.h>
#include <errno.h>

#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termios.h>

int j = 0;
volatile sig_atomic_t print_flag = false;
volatile int STOP = 0; 
int wait_flag = 1;                    /* TRUE while no signal received */
static int read_flag = 0;
char global_buf[51];
char input_buffer[1024];
char output_buffer[52];
int flag_setup = 0;

// variables for socket	
int sock_id,port_no,option_val;
struct sockaddr_in s_addport,p_addport,c_addport;	
struct hostent *hostp; 
struct termios oldtermios;


int ttyraw(int fd)
{
	struct termios newtermios;
	if(tcgetattr(fd, &oldtermios) < 0)
		return(-1);
	newtermios = oldtermios;

	newtermios.c_lflag &= ~(ICANON);
	newtermios.c_cc[VERASE] = 127;

	/* You tell me why TCSAFLUSH. */
	if(tcsetattr(fd, TCSAFLUSH, &newtermios) < 0)
		return(-1);
	return(0);
}

int ttyreset(int fd)
{
	if(tcsetattr(fd, TCSAFLUSH, &oldtermios) < 0)
		return(-1);

	return(0);
}

void handle_alarm( int sig ) {    
	printf("Timeout!\n");	
	printf("?\n");		
}

void handle_sigpoll (int status)
{	
	int c_len = sizeof(c_addport);
    int bytes = recvfrom(sock_id, input_buffer, sizeof(input_buffer), 0, (struct sockaddr *) &c_addport, &c_len);
	//printf("string received: %s, %zu\n",input_buffer,strlen(input_buffer));	

	if (strcmp(input_buffer,"KO")==0) 
	{
		//printf("before zero\n");
		alarm(0);
		printf("| doesn't want to chat\n");
		printf("?\n");	
	}
	
	if (strcmp(input_buffer,"OK")==0) 
	{	
		printf("Connection Established! \n");
		alarm(0);
		flag_setup = 1;
		printf(">");
		fflush(stdout);			
	}
	
	if (strcmp(input_buffer,"E")==0) 
	{
		printf("\n| chat terminated\n");
		printf("?\n");		
		flag_setup = 0;
		if(flag_setup == 0)
		{
			tcgetattr(0, &oldtermios);			
		}
	}
	
	if(input_buffer[0]=='D')
	{		
		printf("\n| %.*s\n", (int)strlen(input_buffer)-1, input_buffer + 1);	
		printf(">");						
		fflush(stdout);
	}
	
	fflush(stdin);	
	fflush(stdout);	
	fflush(stdout);	

	if(sizeof(global_buf)>0)
		write(1,global_buf,sizeof(global_buf));	
}

void chopnl(char *s) {              //strip '\n'
    s[strcspn(s,"\n")] = '\0';
}

int main(int argc, char *argv[])
{	    												
	// check the arguments
	if(argc!=2)
	{
		printf("Usage: %s portnumber \n",argv[0]);
		exit(1);
	}
	
 	struct sigaction sa;	
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGPOLL);
	sigaddset(&sa.sa_mask, SIGALRM);	
	sa.sa_handler = &handle_alarm;
	sa.sa_flags = 0;	 
	
 	if (sigaction(SIGALRM, &sa, 0) == -1) {
		perror(0);			
		exit(1);
	} 
	
	sa.sa_handler = &handle_sigpoll;	
	if (sigaction(SIGPOLL, &sa, 0) == -1) {
		perror(0);			
		exit(1);
	}
	
	//signal( SIGALRM, handle_alarm ); 
	
	bzero((char *) &s_addport, sizeof(s_addport));	
	bzero((char *) &p_addport, sizeof(p_addport));		
	
	// Setup UDP Socket
	sock_id = socket(AF_INET, SOCK_DGRAM, 0);					
	if(sock_id<0)
		perror("Error opening Socket\n");	
		
	
	fcntl(sock_id,F_SETOWN,getpid());           /* allow the process to receive SIGIO */
	fcntl(sock_id, F_SETFL, FASYNC); 	
	
	
	
	/* setsockopt: to rerun the server immediately after killing it */
	option_val = 1;
	setsockopt(sock_id, SOL_SOCKET, SO_REUSEADDR, (const void*)&option_val, sizeof(int));
	
	port_no = atoi(argv[1]);
	s_addport.sin_family = AF_INET;
	s_addport.sin_port = htons(port_no);
	s_addport.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(sock_id, (struct sockaddr *) &s_addport, sizeof(s_addport))== -1) 
	{
		printf("Bind failed!\n");
	}				
	
	printf("?\n");
	
	int ret;
	ssize_t bytes;
	char buf[51];
	struct pollfd fds[2];

	/* Descriptor zero is stdin */
	fds[0].fd = 0;
	fds[1].fd = sock_id;
	fds[0].events = POLLIN | POLLPRI;
	fds[1].events = POLLIN | POLLPRI;
	
	//printf("\n-2\n");
	fflush(stdout);
	
	while (1) 
	{
		/* Call poll() */
		
		if(flag_setup == 1)
			ttyraw(0);
		
		if(flag_setup == 0)
		{
			if(tcgetattr(0, &oldtermios) < 0)
				return(-1);
			printf("Enter input in IPAddress:PortNmber format!\n");
			fflush(stdout);
		}
			
		ret = poll(fds, 2, -1);		
		//printf("read_flag %d\n",read_flag);
		//printf("\n1\n");
		fflush(stdout);
				
		if (ret > 0) 
		{
			/* Regardless of requested events, poll() can always return these */
			if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) 
			{
				printf("Poll Error @stdin\n");
				break;
			}
			if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) 
			{
				printf("Poll Error @socket\n");
				break;
			}

			/* Data to be read from stdin */
			if (fds[0].revents & (POLLIN | POLLPRI)) 
			{
				// output_buffer: 50(message_limit) + 1('D') + 1('\0')
				bzero(output_buffer,52);
				bzero(input_buffer,1024);
				bzero(buf,51);							
				
				//printf("\n\nflag_setup: %d\n",flag_setup);	
				//printf("\n2\n");
				fflush(stdout);				

				if(flag_setup == 0)
				{
					//printf("inside 0\n");
					//int n = read(0,buf,51);	
					//printf("\n3\n");
					//fflush(stdout);
					fgets(buf,51,stdin);
					chopnl(buf);
				}
				
				//printf("\n4\n");
				fflush(stdout);
				
												
				if(flag_setup==1)
				//else
				{							
					//printf("inside 1\n");	
					//printf("wait1: %d\n",wait_flag);
					//printf("read1 %d\n",read_flag);					
					
					//global_buf = malloc(51);
					//bzero(global_buf,51);
					
					//tcgetattr(0,&oldtio);	
					//newtio = oldtio;														
					//newtio.c_lflag &= ~(ICANON);					
					//if(tcsetattr(0,TCSAFLUSH,&newtio)<0)
						//printf("error\n");
						
					//ttyraw(0);	
					
					//printf("\n5\n");
					//fflush(stdout);
						
					int i = 0;
					char c_in;
					fflush(stdin);
					fflush(stdout);
					while(1)
					{	
						if(read(0,&c_in,1)>0)
						{
													
							if((c_in == '\n'))
							{																
								global_buf[i] = '\0';
								//STOP = 1;
								read_flag = 0;								
								break;
							}
							
							if(c_in == '\b')
							{
								printf("in here!\n");
								global_buf[i] = '\0';
								i--;
							}
							
							global_buf[i] = c_in;		
							global_buf[i+1] = '\0';						
							fflush(stdin);							
							i++;	
							read_flag++;						
						}
					}						
					global_buf[strlen(global_buf)] = '\0';											

					//printf("\nmessage input after termio: %s\n",global_buf);
					strcpy(buf,global_buf);					
					bzero(global_buf,51);					
					//tcsetattr(0,TCSAFLUSH,&oldtio);                    
				}
				//ttyreset(0);
				//printf("\nmessage input after input: %s\n",buf);
				//printf("\n\nflag_setup: %d\n",flag_setup);		

				//printf("\n6\n");
				//fflush(stdout);
				
				if(strcmp(buf,"q")!=0 && strcmp(buf,"e")!=0 && flag_setup==0)
				{
					//printf("entered setup phase!\n\n");
					int i=0;
					int port_no;
					char port[10];
					while(buf[i]!=':')
						i++;		
					char ip[i];
					strncpy(ip,&buf[0],i);	
					ip[i]='\0';							
					//printf("ip: %s, strlen:%zu\n",ip,strlen(ip));
					strncpy(port,&buf[i+1],strlen(buf));
					port[strlen(port)]='\0';
					//puts(port);
									
					// server_info
					port_no = atoi(port);	
					p_addport.sin_family = AF_INET;
					p_addport.sin_port = htons(port_no);
					inet_aton(ip,&(p_addport.sin_addr));
					//printf("Peer IP: %s, Peer Port_Number: %d\n",inet_ntoa(p_addport.sin_addr), ntohs(p_addport.sin_port));
					
					bzero(buf,51);
					strcpy(output_buffer,"wannatalk");
					//printf("Sending: %s\n", output_buffer);
					bytes = sendto(sock_id, output_buffer, 10, 0,(struct sockaddr *)&p_addport, sizeof(struct sockaddr_in));
					if (bytes < 0) 
					{
						printf("Error - sendto error: %s\n", strerror(errno));
						break;
					}
					
					//signal( SIGALRM, handle_alarm ); 
					alarm(7);
/* 					char msg[3];

					int c_len = sizeof(c_addport);
					bytes = recvfrom(sock_id, msg, sizeof(msg), 0, (struct sockaddr *) &c_addport, &c_len);
					if (bytes < 0) 
					{
						printf("Error - recvfrom error: %s\n", strerror(errno));
						break;
					} 
					printf("is it here? %s\n",msg);

					if (strcmp(msg,"KO")==0) 
					{
						//printf("before zero\n");
						alarm(0);
						printf("| doesn't want to chat\n");
						printf("?\n");	

					}
					
					if (strcmp(msg,"OK")==0) 
					{		
						//printf("before zero2\n");
						alarm(0);
						flag_setup = 1;
						printf(">");
						fflush(stdout);						
					}
					
					//printf("clicked continue\n");
					continue; */
				}
				
				if(strcmp(buf,"q")==0 && flag_setup == 0 )
				{	
					bzero(buf,51);
					break;				
				} 
				
				if(strcmp(buf,"e")==0 && flag_setup == 1 )
				{					
					//printf("entered termination phase!\n");		
					strcpy(output_buffer,"E");					
					bytes = sendto(sock_id, output_buffer, 2, 0,(struct sockaddr *)&p_addport, sizeof(struct sockaddr_in));
					if (bytes < 0) 
					{
						printf("Error - sendto error: %s\n", strerror(errno));
						break;
					}					
					printf("Connection Terminated!\n",buf);	
					flag_setup = 0;			
					printf("?\n");
					fflush(stdout);
					bzero(buf,51);
					
					if(flag_setup == 0)
					{
						if(tcgetattr(0, &oldtermios) < 0)
							return(-1);
					}
				} 
				
  				if(strcmp(buf,"e")!=0 && flag_setup == 1 )
				{
					//printf("entered chat phase!\n");		
					strcpy(output_buffer,"D");
					strcat(output_buffer,buf);					
					bytes = sendto(sock_id, output_buffer, 52, 0,(struct sockaddr *)&p_addport, sizeof(struct sockaddr_in));
					if (bytes < 0) 
					{
						printf("Error - sendto error: %s\n", strerror(errno));
						break;
					}	
 					bzero(buf,51);
					//printf("%s\n",buf);					
				}  
								

			}

			/* Data to be read from socket */
			if (fds[1].revents & (POLLIN | POLLPRI)) 
			{
				//int c_len = sizeof(c_addport);
				//bytes = recvfrom(sock_id, input_buffer, sizeof(input_buffer), 0, (struct sockaddr *) &c_addport, &c_len);
				//printf("string received: %s, %zu\n",input_buffer,strlen(input_buffer));				
				
				//if (bytes < 0) 
				//{
				//	printf("Error - recvfrom error: %s\n", strerror(errno));
				//	break;
				//}
				
				//printf("\n7\n");
				//fflush(stdout);

				if (strcmp(input_buffer,"wannatalk")==0) 
				{
					printf("\n\n| chat request from %s %d\n",inet_ntoa(c_addport.sin_addr), ntohs(c_addport.sin_port));
					printf("?\n");
					
					char buf[4];
					fgets(buf,4,stdin);
					chopnl(buf);				
					//printf("inside buffer: %s\n",buf);
					
					if(strcmp(buf,"c")==0)
					{
						bzero(output_buffer,20);
						strcpy(output_buffer,"OK");
						bytes = sendto(sock_id, output_buffer, 3, 0,(struct sockaddr *)&c_addport, sizeof(struct sockaddr_in));
						if (bytes < 0) 
						{
							printf("Error - sendto error: %s\n", strerror(errno));
							break;
						}
						printf(">");						
						fflush(stdout);
						flag_setup = 1;
												
						p_addport.sin_family = AF_INET;
						p_addport.sin_port = c_addport.sin_port;
						p_addport.sin_addr.s_addr = c_addport.sin_addr.s_addr;
						//printf("Peer IP: %s, Peer Port_Number: %d\n",inet_ntoa(p_addport.sin_addr), ntohs(p_addport.sin_port));
						
					}
					
					if(strcmp(buf,"n")==0)
					{
						bzero(output_buffer,20);
						strcpy(output_buffer,"KO");
						bytes = sendto(sock_id, output_buffer, 3, 0,(struct sockaddr *)&c_addport, sizeof(struct sockaddr_in));
						if (bytes < 0) 
						{
							printf("Error - sendto error: %s\n", strerror(errno));
							break;
						}
						printf("?\n");						
						fflush(stdout);
					}
					
				}

				//printf("\n8\n");
				//fflush(stdout);
				
/* 				if (strcmp(input_buffer,"E")==0) 
				{
					printf("\n| chat terminated\n");
					printf("?\n");		
					flag_setup = 0;
					if(flag_setup == 0)
					{
						if(tcgetattr(0, &oldtermios) < 0)
							return(-1);
					}
				}
				
				if(input_buffer[0]=='D')
				{
					//printf("regular message\n");
					printf("\n| %.*s\n", (int)strlen(input_buffer)-1, input_buffer + 1);	
					printf(">");						
					fflush(stdout);
				} */
				
				bzero(input_buffer,1024);
			}
			
			//printf("\n9\n");
			//fflush(stdout);
		}
		
		//printf("\n10\n");
		//fflush(stdout);
	}
	ttyreset(0);
	
	// close server_socket
	close(sock_id);	
	
	return 0;
}
