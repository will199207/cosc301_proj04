/*
 * 
 */

#ifndef __THREADSALIVE_H__
#define __THREADSALIVE_H__
#include <ucontext.h>
/* ***************************
        type definitions
   *************************** */
struct node {
	ucontext_t thread;
	int pos; 
	struct node *next;
};
typedef struct {
	struct node **queue;
	int *lock; 

} tasem_t;

typedef struct {
	tasem_t * sem; 
	int* lock;

} talock_t;

typedef struct {
	tasem_t *sem; 

	int*condition; //0 or 1 condition that will be waited on

} tacond_t;


/* ***************************
       stage 1 functions
   *************************** */
ucontext_t* pop(struct node**);
void del_lst(struct node **);
void append(ucontext_t *, struct node**);
void ta_libinit(void);
void ta_create(void (*)(void *), void *);
void ta_yield(void);
int ta_waitall(void);

/* ***************************
       stage 2 functions
   *************************** */

void ta_sem_init(tasem_t *, int);
void ta_sem_destroy(tasem_t *);
void ta_sem_post(tasem_t *);
void ta_sem_wait(tasem_t *);
void ta_lock_init(talock_t *);
void ta_lock_destroy(talock_t *);
void ta_lock(talock_t *);
void ta_unlock(talock_t *);

/* ***************************
       stage 3 functions
   *************************** */

void ta_cond_init(tacond_t *);
void ta_cond_destroy(tacond_t *);
void ta_wait(talock_t *, tacond_t *);
void ta_signal(tacond_t *);

#endif /* __THREADSALIVE_H__ */
