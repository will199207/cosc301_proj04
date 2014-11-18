/*

 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <ucontext.h>
#include "threadsalive.h"
static int stack, list_len, list_loc;
static ucontext_t * m;
static ucontext_t * rv;
static struct node * head;
/* ***************************** 
     linked list functions
   ***************************** */
/*
I have no idea why valgrind crashes. I'm sure its realated to a memory error somewhere 
but I don't have any idea how to find it.  
*/
void append(ucontext_t * context, struct node ** n){
	/*
	Function accesses the shared "head" of the linked list, 
	and appends a new node with the context passed into it 
	to the end of the list
	*/
	if (*n==NULL){
		*n = malloc(sizeof(struct node));
		(*n)->thread = *context;
		(*n)->pos = list_loc++;
		(*n)->next= NULL;
	}else{
		//printf("Appended Node #: %d\n", list_loc);
		struct node *temp = *n;
		while(temp->next!=NULL){
			temp = temp->next;}
		temp->next = malloc(sizeof(struct node)); 
		temp = temp->next;
		temp->pos = list_loc++;
		temp->thread = *context;
	}
list_len++;
}
void del_lst(struct node ** n){
	/*
	Function accesses the shared "head" of the linked list, 
	and appends a new node with the context passed into it 
	to the end of the list
	*/
	if (*n==NULL){
	}else{
		struct node *temp = *n;
		while((*n)->next!=NULL){
			list_loc = (*n)->pos;
			free(((*n)->thread).uc_stack.ss_sp); ///Fix this somehow..
			//printf("Deleted Node #: %d\n", list_loc);
			temp= temp->next; 
			free(*n); //should free both the node and the ucontext_t....
			*n = temp; 
			}
	}
}
 ucontext_t* pop(struct node ** n){
	/*
	This function accesses the shared variable "head" and pops it from the list. 
	It sets head->next as head and frees the malloced memory that head had. It then returns
	a malloced ucontext_t that was the context in head. If queue is empty, a null pointer is returned.
	if del==1, this thread needs to be cleared. 
	*/
	
	if (*n == NULL){
		return NULL;
	}else{		
		rv = &((*n)->thread);
		*n = (*n)->next;
		list_len--;
		return rv;
	}
}
	
	
		




/* ***************************** 
     stage 1 library functions
   ***************************** */


void ta_libinit(void) {
	m = malloc(sizeof(ucontext_t));
	rv = malloc(sizeof(ucontext_t));
	stack = 1024*128;
	list_loc=0;
	list_len=0;
    return;
}

void ta_create(void (*func)(void *), void *arg) {
	ucontext_t *new = malloc(sizeof(ucontext_t));  
	getcontext(new);
	//new stack for the child made from the heap
	new->uc_stack.ss_sp = malloc(stack);
	new->uc_stack.ss_size = stack;
	new->uc_stack.ss_flags = 0;
	new->uc_link = m; 

	//write the stack/funct and args to the new thread
	makecontext(new, (void (*)(void))func, 1, arg);
	//put the context on my new queue of nodes
	append(new, &head);
    return;
}

void ta_yield(void) {
	ucontext_t *current = malloc(sizeof(ucontext_t)); 
	getcontext(current);
	append(current, &head); 
	swapcontext(current, m); 
}

int ta_waitall(void) {
	/*
	This function pops a context from the stack, swaps it with the main (essentally running it)
	and repeats untill there arent any more contexts to be run
	//IF A THREAD ENDS, NEED TO CLEAR ITS STACK/HEAP!?!
	how do I do this.....
	*/
	ucontext_t *new;
	struct node *ptr = &(*head);
	do{
		new = pop(&head);
		swapcontext(m, new);
	}while(new!=NULL);
	del_lst(&ptr); 
	free(m); 
	free(rv); 
    return 0; 
}


/* ***************************** 
     stage 2 library functions
   ***************************** */

void ta_sem_init(tasem_t *sem, int value) {
	sem->lock = (int*)malloc(sizeof(int)); 
	*(sem->lock) =value;
}

void ta_sem_destroy(tasem_t *sem) {
	free(sem->lock);
}

void ta_sem_post(tasem_t *sem) {

	if (sem->lock <0){ //value is negative, threads are waiting, give up the lock (increment its value)
		*(sem->lock) = (*(sem->lock))+1;
		ucontext_t current, new;
		getcontext(&current);
		append(&current, &head); //since this processor is not blocked, it goes to the ready queue			
		new = *(pop(&head)); //give up CPU
		swapcontext(&current, &new); 
	}else{
		*(sem->lock) = (*(sem->lock))+1;
		//process continues, it gave up the lock but nobody was waiting
		//lock is now open b/c the thread called post, meaning its crit section is done, but it still has the CPU
	}	
}

void ta_sem_wait(tasem_t *sem) {
	*(sem->lock) = (*(sem->lock))-1;
	while (*(sem->lock) <1){
		//lock is taken, process has to wait, may as well give up CPU
		ucontext_t current, new; 
		getcontext(&current); //since we arent swapping, we need to getcontext()
		append(&current, &head); //put youself on the queue
		new = *(pop(&head)); //run someone off the ready queue (who might unlock)
		swapcontext(&current, &new); //process that gave up CPU will return to this swap
	}
//if the lock isnt taken, run. Don't put yourself on any queues, you have the lock, use it
}

void ta_lock_init(talock_t *mutex) { 
	(mutex->sem) = malloc(sizeof(tasem_t)); 
	ta_sem_init(mutex->sem, 1); 
	mutex->lock = (int*)malloc(sizeof(int));
	(*(mutex->lock)) =0;
	
}

void ta_lock_destroy(talock_t *mutex) {
	free(mutex->lock);
	ta_sem_destroy(mutex->sem);
	free(mutex->sem);
	
}

void ta_lock(talock_t *mutex) {

	if((*(mutex->lock)) < 0){//lock is closed, its value was less than 0 before you showed up
		ta_sem_wait(mutex->sem); //wait
	}else{
		(*(mutex->lock)) --;}
	//otherwise, lock==-1, its locked, you have it, and you continue to run
}

void ta_unlock(talock_t *mutex) {
	//there is a security issue here where any thread can call unlock and get the mutex. no way of knowing 
	//whether the calling thread had the lock....
	if (*(mutex->lock) == -1){   //if its locked, increment it (unlocking it)
		(*(mutex->lock))++; 
		ta_sem_post(mutex->sem);}

//otherwise do nothing. just because you unlocked doesn't mean you have to give up the CPU.

}


/* ***************************** 
     stage 3 library functions
   ***************************** */

void ta_cond_init(tacond_t *cond) {
	cond->condition = malloc(sizeof(int));
	cond->sem = malloc(sizeof(tasem_t)); 
	ta_sem_init(cond->sem, 1); 

}

void ta_cond_destroy(tacond_t *cond) {
	free(cond->condition); 
	ta_sem_destroy(cond->sem); 
	free(cond->sem); 

	
}

void ta_wait(talock_t *mutex, tacond_t *cond) {
	ta_unlock(mutex); 
	ta_sem_wait(cond->sem);
	ta_lock(mutex);
}

void ta_signal(tacond_t *cond) {
	ta_sem_post(cond->sem);
}

