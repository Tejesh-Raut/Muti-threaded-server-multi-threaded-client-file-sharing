#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>
#include <time.h>
#include <pthread.h>

const char *myarg = NULL;

typedef struct
{
	//Or whatever information that you need
	int *file_num;
	int *sleep_time_between_downloads;
	int *portno;
	int *time_for_request;
	int *numrequests;
	int *total_time;
}
struct_arguments;

void error(char *msg)
{
	perror(msg);
	exit(0);
}
//variables needed: sockfd, file_num
void *mythread(void *args)
{
	struct sockaddr_in serv_addr;
	struct hostent *server;
	struct_arguments *actual_args = args;
	int sockfd;
	time_t start = time(NULL);
	time_t endwait = start + *(actual_args->time_for_request);
	while(start < endwait)
	{
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) 
			error("ERROR opening socket");
			server = gethostbyname(myarg);
		if (server == NULL) 
		{
			fprintf(stderr,"ERROR, no such host\n");
			exit(0);
		}

		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
		serv_addr.sin_port = htons(*(actual_args->portno));

		/* connect to server */

		if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
			error("ERROR connecting");

		int r;
		printf("connected \n");
		if(*(actual_args->file_num) == -1)
		{
			r = rand() % 5000;
		}
		if(*(actual_args->file_num) != -1)
			printf("requested the file with number %d \n", *(actual_args->file_num));
		else
			printf("requested the file with number %d \n", r);
		char buffer[256];
		bzero(buffer,256);
		char chararrayint[4];
		char fileextension[5];
		char* fileextensionsrc = ".txt";
		if(*(actual_args->file_num) != -1)
			sprintf(chararrayint,"%d", *(actual_args->file_num));
		else
			sprintf(chararrayint,"%d", r);
		strncpy(buffer, "get files/foo", 256);
		strcat(buffer, chararrayint);
		strcpy(fileextension, fileextensionsrc);
		strcat(buffer, fileextension);

		/* send user message to server */
		int n;
		printf("sending user message to server \n");
		n = write(sockfd,buffer,strlen(buffer));
		if (n < 0) 
			error("ERROR writing to socket");
		time_t start1 = time(NULL);
		bzero(buffer,256);
		int filefd;
		char file_path[256];
		strncpy(file_path, "files/foo", 256);
		strcat(file_path, chararrayint);
		strcat(file_path, fileextension);
		filefd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		if(filefd == -1)
		{
			perror("open");
			exit(EXIT_FAILURE);
		}
		do
		{
			n = read(sockfd, buffer, 255);
			if(n<0)
				error("ERROR reading from socket");
			if(write(filefd, buffer, n) == -1)
			{
				perror("write");
				exit(EXIT_FAILURE);
			}
		}while(n > 0);
		close(filefd);
		*(actual_args->total_time) = *(actual_args->total_time) + (time(NULL) - start1);
		printf("Downloaded the file %s\n", file_path);
		*(actual_args->numrequests) = *(actual_args->numrequests) +1;
		close(sockfd);
		printf("Closed sockfd, sleeping \n");
		sleep(*(actual_args->sleep_time_between_downloads));
		start = time(NULL);
	}/*end of while*/
	free(actual_args);
	printf("Freed actual_args\n");
	return NULL;
}

int main(int argc, char *argv[])
{
	int portno, num_threads, time_for_request, sleep_time_between_downloads, numrequests, total_time;
	numrequests =0;
	total_time = 0;
	if (argc < 6)
	{
		fprintf(stderr,"usage %s hostname port num_threads time_for_request sleep_time_between_downloads mode(random or fixed) \n", argv[0]);
		exit(0);
	}
	
	srand (time (NULL) );
	myarg = argv[1];
	/* create socket, get sockfd handle */

	portno = atoi(argv[2]);
	num_threads = atoi(argv[3]);
	time_for_request = atoi(argv[4]);
	sleep_time_between_downloads = atoi(argv[5]);

	/* fill in server address in sockaddr_in datastructure */
	
	int file_num;
	if(strcmp(argv[6], "random") == 0)
	{
		file_num = -1;
	}
	else
	{
		if(strcmp(argv[6], "fixed") == 0)
		{
			int r = rand() % 5000;
			file_num = r;
		}
		else
		{
			printf("Wrong mode inputed\n" );
			return 0;
		}
	}
	pthread_t t[num_threads];
	int i;
	for (i = 0; i < num_threads; i++)
	{
		struct_arguments *args = malloc(sizeof *args);
		args->sleep_time_between_downloads = &sleep_time_between_downloads;
		args->file_num = &file_num;
		args->portno = &portno;
		args->time_for_request = &time_for_request;
		args->numrequests = &numrequests;
		args->total_time = &total_time;
		if(pthread_create(&t[i], NULL, mythread, args))
		{
			free(args);
			printf("Error while creating thread %d\n",i );
			exit(-1);
			//goto error_handler;
		}
	}
	pthread_yield(); // useful to give other threads more chance to run
	for (i = 0; i < num_threads; i++)
	{
		pthread_join(t[i], NULL);
		printf("Closed thread %d\n", i);
	}
	//pthread_exit(NULL);
	double throughput = (double)(numrequests)/(time_for_request);
	printf("Done \n");
	printf("Throughput = %f req/s \n", throughput);
	double avg_response_time = (double)(total_time)/(numrequests);
	printf("average response time = %f sec\n", avg_response_time);
	return 0;
}
