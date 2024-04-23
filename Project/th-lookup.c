#include <semaphore.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "hashmap.h"
#include "util.h"
#include "queue.h"

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define NUM_THREADS 10
#define INPUT_FORMAT_STR "%1024s"
#define MAX_FILENAME_LENGTH 256
#define MAX_FILE_SIZE 2560

sem_t *mutex;
sem_t *add_to_end;
sem_t *add_to_start;
sem_t *hashmap_write_sem;
sem_t *hashmap_get_sem;
sem_t *Queue_sem;
sem_t *write_to_output;

typedef struct FILEDATA{
    char filename[MAX_FILENAME_LENGTH];
    char file_results[MAX_FILE_SIZE];
} FILEDATA;

typedef struct Node {
    char name[256];
    int count;
    char ip[INET6_ADDRSTRLEN];
    struct Node* next;
} Node;

typedef struct Queue{
    Node* front;
    Node* rear;
    int size;
} Queue;

Queue Q;
hashmap dns_cache;
int count;

void initialiseQueue(Queue* Q){
    Q->front = NULL;
    Q->rear = NULL;
    Q->size = 0;
}

int isEmpty(Queue* Q){
    if(Q->size == 0){
        return 1;
    } 
    return 0;
}

int isFull(Queue* Q){
    if(Q->size >= 100){
        return 1;
    }
    return 0;
}

void* addToStart(Queue* Q, Node* toAddS){
    sem_wait(add_to_start);
    Q->front = toAddS;
    Q->rear = toAddS;
    sem_wait(mutex);
    Q->size = 1;
    sem_post(mutex);
    sem_post(add_to_start);
    return NULL;
}

void* addToEnd(Queue* Q, Node* toAddE){
    while(isFull(Q) == 1){
        usleep(rand() % 100);
    }
    sem_wait(add_to_end);
    Q->rear->next = toAddE;
    Q->rear = toAddE;
    sem_wait(mutex);
    Q->size++;
    sem_post(mutex);
    sem_post(add_to_end);
    return NULL;
}

void* dns_worker(void* arg){
    
    usleep(rand() % 100);

    Node* host_name_dns = (Node*) arg;
    if(isEmpty(&Q) == 1){
        fprintf(stderr, "Queue is empty\n");
    }

    //Create a temporary buffer to store the ip
    char ip_string[INET6_ADDRSTRLEN];

    sem_wait(hashmap_get_sem);
    char* cached_ip = hashmap_get(&dns_cache, host_name_dns->name);
    sem_post(hashmap_get_sem);

    if(cached_ip != NULL){
        printf("%s, %s is already found in cache\n", host_name_dns->name, cached_ip);
    } else {
        if(dnslookup(host_name_dns->name, ip_string, sizeof(ip_string)) == UTIL_FAILURE){
            fprintf(stderr, "dnslookup error (Bogus Hostname): %s\n", host_name_dns->name);
            strncpy(host_name_dns->ip, "", sizeof(ip_string));
        }else {
            //If dnslookip is successful, cast found ip to node->ip
            strncpy(host_name_dns->ip, ip_string, sizeof(ip_string));

            //Add new dnslookup to hashmap
            sem_wait(hashmap_write_sem);
            hashmap_set(&dns_cache, host_name_dns->name, strdup(ip_string), NULL);
            sem_post(hashmap_write_sem);
        }
    }
    return NULL;
}

void enqueue(Queue* Q, Node* host_name_node){

    if(isFull(Q) == 1){
        usleep(rand() % 100);
    }

	if(isEmpty(Q) == 1){
		addToStart(Q, host_name_node);
	} else {
		Node* current = Q->front;
		while(current != NULL){
			/* This traverses through the queue looking for a node with a name that matches the given name. 
			If a matching node is found, increment the count of the node and exit the function. 
			If the node is not found, it continues traversing until the end of the linked list */
        	if(strcmp(current->name, host_name_node->name) == 0){
            	current->count++;
            	return;
        	}
        	current = current->next;
    	}
    	//Call addToEnd to check if queue is full or not
    	addToEnd(Q, host_name_node);
	}
}

void* requester(void* arg){

    FILEDATA* file_data = (FILEDATA*)arg;

    FILE* input_fp = fopen(file_data->filename, "r");
    if(input_fp == NULL){
        perror("Error opening the file trying to be read, Bogus input file path\n");
        return NULL;
    }
    
    //Create a variable for storing the current host_name being read from file
    char host_name[MAX_FILENAME_LENGTH];

    //The while loop reads each line of the file passed and creates a node for it
    while(fscanf(input_fp, INPUT_FORMAT_STR, host_name) > 0) {
        Node* host_name_node = (Node*)malloc(sizeof(Node));
        if(host_name_node == NULL){
            fprintf(stderr, "Memory allocation failed when creating host_name_node in requester function\n");
            exit(EXIT_FAILURE);
        }
		strcpy(host_name_node->name, host_name);
        host_name_node->count = 1;
        host_name_node->next = NULL;
        enqueue(&Q, host_name_node);
    }
    fclose(input_fp);
    return NULL;
}

int main(int argc, char* argv[]){

    /* Check minimum args */
    if(argc < MINARGS){
        fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
        fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
        return EXIT_FAILURE;
    }

    /* Open output file */
    FILE* output_fp = fopen(argv[argc - 1], "w");
    if(output_fp == NULL){
        perror("Error opening the output file, Bogus output file path\n");
        return EXIT_FAILURE;
    }

    /* Open all sems */
    mutex = sem_open("mutex", O_CREAT, S_IRUSR | S_IWUSR, 1);
	if(mutex == SEM_FAILED) {
        perror("Failed to create mutex");
        return EXIT_FAILURE;
    }

    add_to_end = sem_open("add_to_end", O_CREAT, S_IRUSR | S_IWUSR, 1);
	if(add_to_end == SEM_FAILED) {
        perror("Failed to create add_to_end");
        return EXIT_FAILURE;
    }

    add_to_start = sem_open("add_to_start", O_CREAT, S_IRUSR | S_IWUSR, 1);
	if(add_to_start == SEM_FAILED) {
        perror("Failed to create add_to_start");
        return EXIT_FAILURE;
    }

	hashmap_write_sem = sem_open("hashmap_write_sem", O_CREAT, S_IRUSR | S_IWUSR, 1);
	if(hashmap_write_sem == SEM_FAILED) {
        perror("Failed to create hashmap_write_sem");
        return EXIT_FAILURE;
    }

    hashmap_get_sem = sem_open("hashmap_get_sem", O_CREAT, S_IRUSR | S_IWUSR, 1);
	if(hashmap_get_sem == SEM_FAILED) {
        perror("Failed to create hashmap_get_sem");
        return EXIT_FAILURE;
    }

    Queue_sem = sem_open("Queue_sem", O_CREAT, S_IRUSR | S_IWUSR, 1);
	if(Queue_sem == SEM_FAILED) {
        perror("Failed to create Queue_sem");
        return EXIT_FAILURE;
    }

    write_to_output = sem_open("write_to_output", O_CREAT, S_IRUSR | S_IWUSR, 1);
	if(write_to_output == SEM_FAILED) {
        perror("Failed to create write_to_output");
        return EXIT_FAILURE;
    }

    /* Create and initialise the queue and call it Q */
    initialiseQueue(&Q);

    /* Initialise hashmap */
    hashmap_init(&dns_cache, 1000);

    /* Store variable for amount of files being passed */
    int num_files = argc - 2;
    int num_threads = (num_files < NUM_THREADS) ? num_files : NUM_THREADS;

    /* Store file names in struct for later use */
    FILEDATA file_data[num_files];
    for(int i = 0; i < num_files; i++) {
        sprintf(file_data[i].filename, "names%d.txt", i + 1); //This casts the name of the input file to names1.txt and so on
        strcpy(file_data[i].file_results, argv[i + 1]);       //This copies all contents on the input file to the file_data[i].file_results
    }

    /* Create all needed requester threads */
    pthread_t* requesterThreads = malloc(num_threads * sizeof(pthread_t));
    for(int i = 0; i < num_threads; i++) {
        pthread_create(&requesterThreads[i], NULL, requester, (void *)&file_data[i]); // Pass the filename as argument
    }

    /* wait for queue to be populated and then create all resolver threads */
    pthread_t* resolverThreads = malloc(50 * sizeof(pthread_t));
    Node* resolverTemp = Q.front;
    while(resolverTemp != NULL){
        sem_wait(Queue_sem);
        pthread_create(&resolverThreads[count], NULL, dns_worker, resolverTemp);
        resolverTemp = resolverTemp->next;
        count++;
        usleep(rand() % 100);
        sem_post(Queue_sem);
    }

    /* Join all requester threads */
    for(int i = 0; i < num_threads; i++){
        pthread_join(requesterThreads[i], NULL);
    }

    /* Join all resolverThreads */
    for(int i = 0; i < num_threads; i++){
        pthread_join(resolverThreads[i], NULL);
    }
     printf("all threads have been joined\n");
    
    /* This is a check to see results are populated */
     for(Node* tempCheck = Q.front; tempCheck != NULL; tempCheck = tempCheck->next){
        printf("This is the check before writing --- [ %d, %s, %s ]\n", tempCheck->count, tempCheck->name, tempCheck->ip);
    }

    /* This is where output_fp is written to */
    printf("\n");
    printf("consolidating results and recording in results.txt...\n");
    Node* tempNode = malloc(sizeof(Node));
    tempNode = Q.front;
    sem_wait(write_to_output);
    while(tempNode != NULL){
        fprintf(output_fp, "%d, %s, %s\n", tempNode->count, tempNode->name, tempNode->ip);
        tempNode = tempNode->next;
    }
    sem_post(write_to_output);
    printf("\n");
    printf("------All results printed------\n");

    /* Free memory for each node in the queue */
    Node* current = Q.front;
    while (current != NULL) {
        Node* temp = current;
        current = current->next;
        free(temp);
    }

    /* Free memory allocated for requesterThreads array */
    free(requesterThreads);

    /* Free memory allocated for resolverThreads array */
    free(resolverThreads);

    /* Close all semaphore handles */
    sem_close(mutex);
    sem_close(add_to_start);
    sem_close(add_to_end);
    sem_close(hashmap_write_sem);
    sem_close(hashmap_get_sem);
    sem_close(write_to_output);
    sem_close(Queue_sem);
    
    /* Close the output file pointer */
    fclose(output_fp);

    /* Cleanup the hashmap */
    hashmap_cleanup(&dns_cache);
    
    return EXIT_SUCCESS;
}