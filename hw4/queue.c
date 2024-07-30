#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

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
    thrd_t *deleting_thrd;
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
void add_thread(thrd_t thread, cnd_t cnd) {
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
    newElem->deleting_thrd;
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
        // if there are threads waiting for dequeue
        newElem->deleting_thrd = &threads.front->thrd;
        // remove thread from queue
        remove_thread();
        // wakeup for dequeue
        cnd_signal(&threads.front->cnd);
    }
    mtx_unlock(&q_lock);
}

void* dequeue(void) {
    mtx_lock(&q_lock);
    void *data = NULL;
    Node *temp = NULL;
    if(q.front == NULL) {
        // add thread to the thread queue
        cnd_t cond;
        cnd_init(&cond);
        add_thread(thrd_current(), cond);
        // start waiting until this thread is woken up, occurs after enqueue
        cnd_wait(&cond, &q_lock);
    }
    else {
        // q not empty can attempt regular dequeue
        temp = q.front;
        data = q.front->data;
        q.front = q.front->next;
        if(q.front == NULL) {
            q.back = NULL;
        }
        free(temp);
        q.size--;
        q.visited++;
        mtx_unlock(&q_lock);
        return data;
    }
    // currently the working thread can be any Node in the queue, need to find the correct one for deletion
    temp = q.front;
    Node *prev = NULL;
    // if the current_thrd corresponds to the front node
    if(*temp->deleting_thrd == thrd_current()) {
        data = temp->data;
        temp = temp->next;
        if(temp == NULL) {
            q.back = NULL;
        }
        free(temp);
        q.size--;
        //each dequeue makes one item visit a queue
        q.visited++;
        mtx_unlock(&q_lock);
        return data;
    }
    // search the corresponding running thread for the node to remove
    while(temp != NULL && *temp->deleting_thrd != thrd_current()) {
        prev = temp;
        temp = temp->next;
    }
    if(temp == NULL || prev == NULL) {
        printf("Invalid \n");
    }
    else {
        data = temp->data;
        // if the node to remove is the back
        if(temp == q.back) {
            prev->next = NULL;
            q.back = prev;
        }
        else {
            // unlink the node from the queue
            prev->next = temp->next;
        }
        free(temp);
        q.size--;
        //each dequeue makes one item visit a queue
        q.visited++;
        mtx_unlock(&q_lock);
    }
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
