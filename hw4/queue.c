//#include "queue.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <unistd.h>
typedef struct ThreadNode {
    thrd_t thrd;
    cnd_t cnd;
    struct ThreadNode *next;
}ThreadNode;

typedef struct ThreadQueue {
    ThreadNode *front;
    ThreadNode *back;
    size_t waiting;
}thrd_queue;

typedef struct Node{
    void *data;
    struct Node *next;

}Node;

typedef struct Queue {
    Node *front;
    Node *back;
    size_t size;
    size_t visited;
}queue;

queue q;
thrd_queue threads;
mtx_t q_lock;
bool awaken = false;

void initQueue(void) {
    printf("Initiating Queue \n");
    q.front = q.back = NULL;
    threads.front = threads.back = NULL;
    q.size = 0;
    q.visited = 0;
    mtx_init(&q_lock, mtx_plain);
}
void destroyQueue(void) {
    printf("Destroying Queue \n");
    mtx_lock(&q_lock);
    while(q.front != NULL) {
        Node *tmp = q.front;
        q.front = q.front->next;
        free(tmp);
    }
    q.size = 0;
    q.back = NULL;

    while(threads.front != NULL) {
        ThreadNode *tmp = threads.front;
        threads.front = threads.front->next;
        cnd_destroy(&tmp->cnd);
        free(tmp);
    }
    threads.back = NULL;
    threads.waiting = 0;
    mtx_unlock(&q_lock);
    mtx_destroy(&q_lock);
}
void add_thread(thrd_t thread) {
    cnd_t cnd;
    cnd_init(&cnd);
    ThreadNode* thread_node = malloc(sizeof(ThreadNode));
    thread_node->thrd = thread;
    thread_node->cnd = cnd;
    thread_node->next = NULL;
    if(threads.front == NULL) {
        threads.front = threads.back = thread_node;
    }
    else {
        threads.back->next = thread_node;
        threads.back = thread_node;
    }
    threads.waiting++;
}
void remove_thread(void) {
    ThreadNode *tmp = threads.front;
    if(tmp != NULL) {
        threads.front = threads.front->next;
        if(threads.front == NULL) {
            threads.back = NULL;
        }
        cnd_destroy(&tmp->cnd);
        free(tmp);
    }
}

void enqueue(void* Data) {
    Node *newElem = malloc(sizeof(Node));
    newElem->data = Data;
    newElem->next = NULL;
    // adding the node to the back of the queue
    mtx_lock(&q_lock);
    if(q.front == NULL) {
        q.front = q.back = newElem;
    }
    else {
        q.back->next = newElem;
        q.back = newElem;
    }
    q.size++;
    // if threads are waiting and none have been awaken yet start awakening
    // if there are no threads waiting no need to awake
    // if a thread is awaken he will be responsible for awakening the rest of the threads
    if(threads.front != NULL && !awaken) {
        // if there are threads waiting for dequeue
        // wakeup for dequeue
        cnd_signal(&threads.front->cnd);
        awaken = true;
        // once the correct thread signaled can be dequeued for the next waiting thread to be signaled
        // remove thread from queue - the mutex still hasn't been released
        remove_thread();
    }
    mtx_unlock(&q_lock);
}

void* awaken_dequeue(void** data) {
    Node *temp = q.front;
    if(temp == NULL) {
        printf("temp can't be null after thread has been awakene \n");
        return NULL;
    }
    data = temp->data;
    q.front = q.front->next;
    if(q.front == NULL) {
        q.back = NULL;
    }
    q.visited++;
    q.size--;
    free(temp);
    threads.waiting--;
    awaken = false;
    // if there are still threads waiting and there are more elements to dequeue, then awaken the next thread.
    if(threads.front != NULL && q.size > 0) {
        cnd_signal(&threads.front->cnd);
        awaken = true;
        remove_thread();
    }
    return data;
}
void* dequeue(void) {
    mtx_lock(&q_lock);
    void *data = NULL;
    // if no elements in the queue or there is at least one thread that is awaken go to sleep
    if(q.front == NULL || awaken) {
        add_thread(thrd_current());
        cnd_wait(&threads.back->cnd, &q_lock);
        // thread has been awakened, can delete the first
        data = awaken_dequeue(data);
        mtx_unlock(&q_lock);
        return data;
    }
    // if q has elements and none is awaken, meaning in this point dequeue was called after series of enqueues
    Node *temp = q.front;
    if(temp == NULL) {
        printf("temp can't be null after thread has been awakened\n");
        return NULL;
    }
    data = temp->data;
    q.front = q.front->next;
    if(q.front == NULL) {
        q.back = NULL;
    }
    q.visited++;
    q.size--;
    mtx_unlock(&q_lock);
    return data;
}

bool tryDequeue(void** data) {
    mtx_lock(&q_lock);
    // if there are more elements than awaken + waiting threads,
    if(threads.front != NULL && awaken + threads.waiting < q.size) {
        add_thread(thrd_current());
        cnd_wait(&threads.back->cnd, &q_lock);
        // once awaken there are enough threads to work with
        *data = awaken_dequeue(data);
        mtx_unlock(&q_lock);
        return true;
    }
    // if no threads are waiting dequeue and there are elements
    if(threads.front == NULL && q.front != NULL) {
        Node *temp = q.front;
        *data = temp->data;
        q.front = q.front->next;
        if(q.front == NULL) {
            q.back = NULL;
        }
        q.visited++;
        q.size--;
        free(temp);
        mtx_unlock(&q_lock);
        return true;
    }
    mtx_unlock(&q_lock);
    return false;
}

size_t size(void) {
    return q.size;
}
size_t waiting(void) {
    return threads.waiting;
}
size_t visited(void) {
    return q.visited;
}
