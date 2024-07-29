#include "queue.h"
#include <stdlib.h>
#include <threads.h>

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

queue q;
thrd_queue threads;
mtx_t q_lock;

void add_thread(thrd_t thread, cnd_t cnd) {
    ThreadNode* thread_node = malloc(sizeof(ThreadNode));
    thread_node->thrd = thread;
    thread_node->cnd = cnd;
    thread_node->next = NULL;
    mtx_lock(&q_lock);
    if(threads.front == NULL) {
        threads.front = threads.back = thread_node;
    }
    else {
        threads.back->next = thread_node;
        threads.back = thread_node;
    }
    threads.waiting++;
    mtx_unlock(&q_lock);
}
void remove_thread(void) {
    mtx_lock(&q_lock);
    ThreadNode *tmp = threads.front;
    if(tmp != NULL) {
        threads.front = threads.front->next;
        if(threads.front == NULL) {
            threads.back = NULL;
        }
        cnd_destroy(&tmp->cnd);
        free(tmp);
    }
    mtx_unlock(&q_lock);
}
void initQueue(void) {
    q.front = q.back = NULL;
    threads.front = threads.back = NULL;
    q.size = 0;
    q.visited = 0;
    mtx_init(&q_lock, mtx_plain);
}
void destroyQueue(void) {
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
    mtx_unlock(&q_lock);
    mtx_destroy(&q_lock);
}

void enqueue(void* Data) {
    Node *newElem = malloc(sizeof(Node));
    newElem->data = Data;
    newElem->next = NULL;
    // adding the node to the queue
    mtx_lock(&q_lock);
    if(q.front == NULL) {
        q.front = q.back = newElem;
    }
    else {
        q.back->next = newElem;
        q.back = newElem;
    }
    q.size++;

    // waking up the oldest thread if exists
    if(threads.front != NULL) {
        cnd_signal(&threads.front->cnd);
    }
    mtx_unlock(&q_lock);
}
void* dequeue(void) {
    mtx_lock(&q_lock);
    while(q.front == NULL) {
        // add thread to the thread queue
        cnd_t cond;
        cnd_init(&cond);
        add_thread(thrd_current(), cond);

        // start waiting until this thread is woken up
        cnd_wait(&cond, &q_lock);

        // remove thread from queue and start work
        remove_thread();
    }
    Node *temp = q.front;
    void *data = q.front->data;
    q.front = q.front->next;
    if(q.front == NULL) {
        q.back = NULL;
    }
    free(temp);
    q.size--;
    //each dequeue makes one item visit a queue
    q.visited++;
    mtx_unlock(&q_lock);
    return data;
}


bool tryDequeue(void** data) {
    mtx_lock(&q_lock);
    if(q.front != NULL) {
        Node *temp = q.front;
        *data = q.front->data;
        q.front = q.front->next;
        if(q.front == NULL) {
            q.back = NULL;
        }
        free(temp);
        q.size--;
        q.visited++;
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
