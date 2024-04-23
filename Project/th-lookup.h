#ifndef TH_LOOKUP_H
#define TH_LOOKUP_H

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

typedef struct FILEDATA {
    char filename[MAX_FILENAME_LENGTH];
    char file_results[MAX_FILE_SIZE];
} FILEDATA;

typedef struct Node {
    char name[MAX_FILENAME_LENGTH];
    int count;
    char ip[INET6_ADDRSTRLEN];
    struct Node* next;
} Node;

typedef struct Queue {
    Node* front;
    Node* rear;
    int size;
} Queue;

Queue Q;
hashmap dns_cache;

/* Function to initialise a Queue. Returns an empty queue */
void initialiseQueue(Queue* Q);

/* Function to check if a Queue is empty. Returns 1 if Queue is empty, 0 otherwise */
int isEmpty(Queue* Q);

/* Function to check if a Queue is full. Returns 1 if Queue is full, 0 otherwise */
int isFull(Queue* Q);

/* Function to add a passed node to the front of the Queue. toAddS is the node to be added and Q is the Queue to add it to */
void* addToStart(Queue* Q, Node* toAddS);

/* Function to add a passed node to the front of the Queue. toAddE is the node to be added and Q is the Queue to add it to */
void* addToEnd(Queue* Q, Node* toAddE);

/* Function to make dns_lookup on a passed node. Each resolver thread takes the next node in the Queue and will go through the dns_worker function.
Function creates a temp string to store the IP address, it first checks the hashmap to see if a dns call has been cast on the host name, if yes, exit 
function. If no, perform dnslookup and if successful, cast dns lookup result into the corresponding nodes->ip. Then create entry in hashmap to 
limit future unneccasary lookups. */
void* dns_worker(void* arg);

/* Function to add a passed node to the a Queue. If the Queue is currently full it will sleep, if the Queue is empty (The current node is the first
to be passed to the enqueue function) it will add the passed node to the start of the desired Queue. If the Queue is not empty, it will traverse
the queue looking for another node that matches the current hostname. If one is found, increment that nodes count and exit the function, skipping 
the dns lookup. Otherwise add the node to the end of the Queue */
void enqueue(Queue* Q, Node* host_name_node);

/* This function is responsible for reading all passed input files and extracting the host names from them. Once host names are scanned,
it will create a node and copy the host name into the node. After this it will then send the node to the enqueue function */
void* requester(void* arg);

#endif