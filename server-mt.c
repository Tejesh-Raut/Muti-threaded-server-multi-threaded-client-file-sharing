#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <pthread.h>
#include <limits.h>


/*A TAILQ_HEAD structure is declared as follows:*/
TAILQ_HEAD(headname , entry ) head;
int count_queue = 0;
int numrequests;
pthread_cond_t condvar  = PTHREAD_COND_INITIALIZER;
pthread_cond_t condvar1  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

struct entry {
	int fd;
	TAILQ_ENTRY(entry) entries ; /*declares a structure that connects the elements in the tail queue.*/
};

typedef struct 
{
	int thread_no ;
	int fileds;
}
struct_arguments;

void *thread(void *args)
{
	while(1)
	{
	struct_arguments *actual_args = args;
	//printf("Started thread number %d\n", actual_args->thread_no );
	pthread_mutex_lock(&count_mutex);
	/**mutex lock*/
	//printf("Mutex locked for queue in thread %d\n", actual_args->thread_no);
	while(TAILQ_EMPTY(&head))
	{
		//printf("Queue is empty, waiting for being non-empty\n");
		pthread_cond_wait(&condvar, &count_mutex);
	}
	//printf("Queue non-empty\n");
	pthread_mutex_unlock(&count_mutex);
	/**mutex unlock*/
	pthread_mutex_lock(&count_mutex);
	/**mutex lock*/
	int thread_fd = (head.tqh_first)->fd;
	TAILQ_REMOVE(&head, head.tqh_first, entries);
	count_queue = count_queue - 1;
	if(count_queue == numrequests - 1)
	{
		pthread_cond_signal(&condvar1);
	}
	pthread_mutex_unlock(&count_mutex);
	/**mutex unlock*/
	//printf("Mutex unlocked for queue in thread\n");
	char buffer[256];
	char filename[21];
	char filename_str[15];
	ssize_t read_return;
	bzero(filename,21);// 21 is filename siz to be difind
	int n = read(thread_fd,filename,21);
	//printf("Recieved request for file %s\n", filename);
	if (n <= 0)
		perror("ERROR reading from socket");
	strcpy(filename_str, &filename[4]);
	//printf("sending %s\n", filename_str);
	int fd = open(filename_str, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "unable to open '%s': %s\n", filename_str, strerror(errno));
		exit(1);
	}
	while(1)
	{
		read_return = read(fd, buffer, 256);
		if (read_return == 0)
			break;
		if (read_return == -1)
		{
			perror("read");
			exit(EXIT_FAILURE);
		}
		if (write(thread_fd, buffer, read_return) == -1)
		{
			perror("write");
			exit(EXIT_FAILURE);
		}
	}

	close(fd);
	//printf("file sent \n" );
	close(thread_fd);
	}
}

int main(int argc, char *argv[])
{
	int portno , num_threads;
	int sockfd ;
	struct sockaddr_in serv_addr, cli_addr;
	
	if( argc < 4)
	{
		fprintf(stderr,"usage %s portno , num_threads , numrequests \n" , argv[0]);
		exit(0);
	}
	portno = atoi(argv[1]);
	num_threads = atoi(argv[2]);
	numrequests = atoi(argv[3]);

	if(numrequests == 0)
		numrequests = INT_MAX;

	pthread_mutex_lock(&count_mutex);
	/**mutex lock*/
	TAILQ_INIT(&head);
	pthread_mutex_unlock(&count_mutex);
	/**mutex unlock*/

	/* define threads*/
	pthread_t t[num_threads];
	for(int i = 0 ;i < num_threads ; i++)
	{
		struct_arguments *args = malloc(sizeof *args);
		args->fileds = -1;
		args->thread_no = i;
		if(pthread_create(&t[i],NULL,&thread ,args ))
		{
			free(args);
			printf("Error in creating thread %d ", i);
			exit(-1);
		}
	}

	//struct entry *element;
	/*intialize the queue */

	/* create socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("ERROR opening socket");
	}

	/* fill in port number to listen on. IP address can be anything (INADDR_ANY) */
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	
	/* bind socket to this port number on this machine */
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		perror("ERROR on binding");
    
    /* listen for incoming connection requests */
	listen(sockfd,500);
	int clilen = sizeof(cli_addr);

	while(1)
	{
		pthread_mutex_lock(&count_mutex);
		/**mutex lock*/
		//printf("Mutex locked for count_queue in while loop of main\n");
		while(count_queue >= numrequests)
		{
			pthread_cond_wait(&condvar1, &count_mutex);
		}
		pthread_mutex_unlock(&count_mutex);
		/**mutex unlock*/
		//printf("Mutex unlocked for count_queue in while loop of main\n");
		//printf("accepting request \n");
		int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		//printf("newsockfd is %d\n",newsockfd );
		if (newsockfd < 0) 
		{
			printf("ERROR on accept\n");
			continue;
		}
		struct entry *element = malloc(sizeof(struct entry));/* Create new entry to enter into queue */
		if(element)
		{
			element->fd = newsockfd;
		}
		/*insert at the head*/
		pthread_mutex_lock(&count_mutex);
		/**mutex lock*/
		//printf("Mutex locked for queue in while loop of main\n");
		if(TAILQ_EMPTY(&head))
		{
			TAILQ_INSERT_TAIL(&head, element, entries);
			count_queue = count_queue + 1;
			pthread_cond_signal(&condvar);
		}
		else
		{
			TAILQ_INSERT_TAIL(&head, element, entries);
			count_queue = count_queue + 1;
		}
		pthread_mutex_unlock(&count_mutex);
		/**mutex unlock*/
		//printf("Mutex unlocked for queue in while loop of main\n");
		/*
		The worker thread job allocation code here
		*/
		//printf("accepting connection\n");
	}

}