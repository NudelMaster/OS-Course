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
    // default value for later use
    newElem->deleting_thrd = NULL;
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
        // wakeup for dequeue
        cnd_signal(&threads.front->cnd);
        // once the correct thread signaled can be dequeued for the next waiting thread to be signaled
        // remove thread from queue - the mutex still hasn't been released
        remove_thread();
    }
    mtx_unlock(&q_lock);
}

void* woken_thrd_dequeue() {
// function that removes an element according to a thread that has awakened
    void *data = NULL;
    Node *temp = NULL;
    temp = q.front;
    // if the current_thrd corresponds to the front node
    if(*temp->deleting_thrd == thrd_current()) {
        data = temp->data;
        q.front = q.front->next;
        if(q.front == NULL) {
            q.back = NULL;
        }
        free(temp);
        return data;
    }
    // search the corresponding running thread for the node to remove
    Node *prev = NULL;
    while(temp != NULL && *temp->deleting_thrd != thrd_current()) {
        prev = temp;
        temp = temp->next;
    }
    if(temp == NULL || prev == NULL) {
        printf("Error, temp and prev should not be NULL since node must exist in Queue\n");
        return NULL;
    }
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
    return data;
}
void* non_waiting_thrd_dequeue() {
    // function the dequeues element in case of more elements in queue than waiting threads
    void *data = NULL;
    Node *temp = NULL;
    temp = q.front;
    // if the current_thrd corresponds to the front node
    if(temp->deleting_thrd == NULL) {
        data = temp->data;
        q.front = q.front->next;
        if(q.front == NULL) {
            q.back = NULL;
        }
        free(temp);
        return data;
    }
    // search the corresponding running thread for the node to remove
    Node *prev = NULL;
    while(temp != NULL && temp->deleting_thrd != NULL) {
        prev = temp;
        temp = temp->next;
    }
    if(temp == NULL || prev == NULL) {
        printf("Error, temp and prev should not be NULL since node must exist in Queue\n");
        return NULL;
    }
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
    return data;
}
void* dequeue(void) {
    mtx_lock(&q_lock);
    void *data = NULL;
    if(q.size <= threads.waiting) {
        // in case of not having enough elements to dequeue send thread to sleep
        // add thread to the thread queue
        cnd_t cond;
        cnd_init(&cond);
        add_thread(thrd_current(), cond);
        // start waiting until this thread is woken up, occurs after enqueue
        cnd_wait(&cond, &q_lock);
        // after waking up there is at least one element in the front, reach the correct node and remove
        // when wakes up he has a node that waits for him for removal now the only thing left is to find and remove it
        data = woken_thrd_dequeue();
        threads.waiting--;
        q.size--;
        //each dequeue makes one item visit a queue
        q.visited++;
        mtx_unlock(&q_lock);
        return data;
        }
    // at this point there is at least 1 element in the queue which doesn't have a waiting thread,
    // find the element which doesn't have a thread attached to it
    data = non_waiting_thrd_dequeue();
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
