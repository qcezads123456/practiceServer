#include "thread_pool.h"

typedef enum state{
    IDLE,
    RUNNING,
    TERMINATE
}state;

typedef struct task_t
{
    void (*function)(void *arg);
    void *arg;
}task_t;

typedef struct calthread_t{
    float rate;
    pthread_t cthread;
}calthread_t;

typedef struct thread_t
{
    struct thread_t *prev, *next;
    pthread_t thread;
    pthread_cond_t notify;
    state s;
}thread_t;


static const int init_task_capacity=4096;
static const int init_thread_size=4;
static thread_pool_t *global_pool = NULL;
static calthread_t *global_calthread=NULL;

static void * worker(void* arg){
    thread_t *self=(thread_t*)arg;
    while(1){
        pthread_mutex_lock(&global_pool->lock);
        while((global_pool->task_count==0 || self->s!=RUNNING) && !global_pool->shutdown && self->s != TERMINATE){
            if(self->s==RUNNING){
                global_pool->t_tail->next=self;
                self->prev=global_pool->t_tail;
                self->next=NULL;
                global_pool->t_tail=self;
            }
            self->s= IDLE;
            pthread_cond_wait(&(self->notify),&global_pool->lock);
        }
        if(global_pool->task_count==0 && (global_pool->shutdown || self->s == TERMINATE)){
            if(global_pool->shutdown){
                global_pool->thread_size-=1;
                if(global_pool->thread_size==0){ 
                    pthread_cond_signal(&global_pool->all_done);
                }
            }
            pthread_mutex_unlock(&global_pool->lock);
            break;
        }
        while(global_pool->task_count>0){
            task_t running_task=global_pool->queue[global_pool->head];
            global_pool->head=(global_pool->head+1)%global_pool->task_size;
            global_pool->task_count--;
            pthread_mutex_unlock(&global_pool->lock);
            running_task.function(running_task.arg);
            pthread_mutex_lock(&global_pool->lock);
        }
        if(!global_pool->shutdown){
            global_pool->t_tail->next=self;
            self->prev=global_pool->t_tail;
            self->next=NULL;
            global_pool->t_tail=self;
            self->s=IDLE;
        }
        pthread_mutex_unlock(&global_pool->lock);
    }
    pthread_cond_destroy(&self->notify);

    free(self);
    return NULL;
}

bool addTask(void(*func)(void *),void *inarg){
    pthread_mutex_lock(&global_pool->lock);
    bool success=0;
    if(global_pool->task_count<global_pool->task_size){
        global_pool->queue[global_pool->tail].function=func;
        global_pool->queue[global_pool->tail].arg=inarg;
        global_pool->tail=(global_pool->tail+1)%global_pool->task_size;
        global_pool->task_count++;
        success=1;
    }
    if(success){
        if(global_pool->dummy_head->next!=NULL){
            thread_t *running_thread=global_pool->dummy_head->next;
            running_thread->s=RUNNING;
            if(running_thread->next!=NULL){
                running_thread->next->prev=global_pool->dummy_head;
                global_pool->dummy_head->next=running_thread->next;
            }
            else{
                global_pool->dummy_head->next=NULL;
                global_pool->t_tail=global_pool->dummy_head;
            }
            running_thread->next=NULL;
            running_thread->prev=NULL;
            pthread_cond_signal(&(running_thread->notify));
        }
    }
    pthread_mutex_unlock(&global_pool->lock);
    return success;
}
static void *cal(){
    struct timespec ts;
    ts.tv_nsec=200000000; //200ms
    ts.tv_sec=0;
    while(1){
        nanosleep(&ts,NULL);
        if(!global_pool->shutdown){
            global_calthread->rate=(float)global_pool->task_count/(float)global_pool->task_size;
            if(global_calthread->rate>=0.8){
                pthread_mutex_lock(&global_pool->lock);
                task_t *newqueue,*oldqueue;
                newqueue=malloc(sizeof(task_t)*global_pool->task_size*2);
                oldqueue=global_pool->queue;
                for(int i=0;i<global_pool->task_count;i++){
                    newqueue[i]=global_pool->queue[(global_pool->head+i)%global_pool->task_size];
                }
                global_pool->head=0;
                global_pool->tail=global_pool->task_count;
                global_pool->queue=newqueue;
                global_pool->task_size *= 2;
                free(oldqueue);
                thread_t *newthread;
                newthread=(thread_t*)malloc(sizeof(thread_t));
                newthread->next = NULL;
                newthread->prev = NULL;
                newthread->s=RUNNING;  //after creating a new thread ,it starts comsuming tasks right away
                pthread_cond_init(&(newthread->notify), NULL);
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
                pthread_create(&newthread->thread,&attr,worker,newthread);
                pthread_attr_destroy(&attr);
                global_pool->thread_size += 1;
            }
            else if(global_calthread->rate<=0.3){
                if(global_pool->task_size>init_task_capacity){
                    task_t *newqueue,*oldqueue;
                    bool flag=0;
                    if(global_pool->task_size*0.9>=init_task_capacity){
                        newqueue=malloc(sizeof(task_t)*(global_pool->task_size*0.9));
                    }
                    else{
                        newqueue=malloc(sizeof(task_t)*(init_task_capacity));
                        flag=1;
                    }
                        oldqueue=global_pool->queue;
                    for(int i=0;i<global_pool->task_count;i++){
                        newqueue[i]=global_pool->queue[(global_pool->head+i)%global_pool->task_size];
                    }
                    global_pool->head=0;
                    global_pool->tail=global_pool->task_count;
                    global_pool->queue=newqueue;
                    if(!flag){
                        global_pool->task_size *= 0.9;
                    }
                    else{
                        global_pool->task_size=init_task_capacity;
                    }
                    free(oldqueue);
                }
                if(global_pool->thread_size>init_thread_size){
                    thread_t *old=global_pool->dummy_head->next;
                    if(old!=NULL){
                        global_pool->dummy_head->next=old->next;
                        if(global_pool->dummy_head->next!=NULL){
                            old->next->prev=global_pool->dummy_head;
                        }
                        else{
                            global_pool->t_tail=global_pool->dummy_head;
                        }
                        old->prev=NULL;
                        old->next=NULL;
                        old->s=TERMINATE;
                        pthread_cond_signal(&old->notify);
                        global_pool->thread_size -= 1;
                    }
                }
            }
            pthread_mutex_unlock(&global_pool->lock);
        }
        else{
            pthread_mutex_unlock(&global_pool->lock);
            break;
        }
    }
    return NULL;
}
void thread_kill(){
    pthread_mutex_lock(&global_pool->lock);
    global_pool->shutdown=1;
    thread_t *head_thread=global_pool->dummy_head->next;
    while(head_thread!=NULL){
        pthread_cond_signal(&head_thread->notify);
        head_thread=head_thread->next;
    }
    pthread_mutex_unlock(&global_pool->lock);
}

static thread_t *thread_init(){
    thread_t *newthread;
    newthread=(thread_t*)malloc(sizeof(thread_t));
    newthread->next=NULL;
    newthread->prev=NULL;
    newthread->s=IDLE;
    pthread_cond_init(&(newthread->notify), NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    pthread_create(&newthread->thread,&attr,worker,newthread);
    pthread_attr_destroy(&attr);
    return newthread;
}

static calthread_t *calthread_init(){
    calthread_t *new_cal;
    new_cal=(calthread_t*)malloc(sizeof(calthread_t));
    new_cal->rate=0.0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    pthread_create(&new_cal->cthread,&attr,cal,NULL);
    pthread_attr_destroy(&attr);
    return new_cal;
}

thread_pool_t *pool_init(){
    calthread_t *calthread;
    thread_pool_t *pool=NULL; 
    pool=(thread_pool_t*)malloc(sizeof(thread_pool_t));
    pool->dummy_head=(thread_t*)malloc(sizeof(thread_t));
    pool->dummy_head->next=NULL;
    pool->dummy_head->prev=NULL;
    pool->t_tail=NULL;
    task_t *init_queue;
    init_queue=malloc(sizeof(task_t)*init_task_capacity);
    pool->queue=init_queue;
    pthread_mutex_init(&pool->lock,NULL);
    pthread_cond_init(&(pool->all_done), NULL);
    pool->tail=0;
    pool->head=0;
    pool->task_count=0;
    pool->task_size=init_task_capacity;
    pool->thread_size=init_thread_size;
    pool->shutdown=0;
    global_pool=pool;
    calthread=calthread_init();
    global_calthread=calthread;
    for(int i=0;i<init_thread_size;i++){
        thread_t *newnode=thread_init();
        if(pool->dummy_head->next==NULL){
            pool->dummy_head->next=newnode;
            newnode->prev=pool->dummy_head;
            pool->t_tail=newnode;
        }
        else{
            pool->t_tail->next=newnode;
            newnode->prev=pool->t_tail;
            pool->t_tail=newnode;
        }
    }
    return pool;
}
void traversal_list(thread_pool_t *pool){
    int front=0;
    thread_t *head_node=pool->dummy_head->next;
    while(head_node!=NULL){
        printf("%d\n",front++);
        head_node=head_node->next;
    }
    int back=0;
    thread_t *tail_node=pool->t_tail;
    while(tail_node!=pool->dummy_head){
        printf("%d\n",back++);
        tail_node=tail_node->prev;
    }
}
void pool_destroy(thread_pool_t *pool){
    thread_kill();
    pthread_mutex_lock(&pool->lock);
    while (pool->thread_size > 0) {
        pthread_cond_wait(&(pool->all_done), &(pool->lock));
    }
    pthread_mutex_unlock(&pool->lock);
    pthread_cond_destroy(&pool->all_done);
    pthread_mutex_destroy(&pool->lock);
    free(pool->queue);
    free(pool);
}