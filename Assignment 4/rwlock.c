#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <common.h>

/* XXX NOTE XXX  
       Do not declare any static/global variables. Answers deviating from this
       requirement will not be graded.
*/
void init_rwlock(rwlock_t *lock)
{
   /*Your code for lock initialization*/
   lock->value = (long)1 << 48;
}

void write_lock(rwlock_t *lock)
{
   /*Your code to acquire write lock*/
   long val1 = (long)1 << 48;
   long val2 = -1 * (long)1 << 48;
   while(lock->value<val1) sched_yield();
   while (atomic_add(&lock->value, val2))
      atomic_add(&lock->value,val1);
}

void write_unlock(rwlock_t *lock)
{
   /*Your code to release the write lock*/
   atomic_add(&lock->value, (long)1 << 48);
}

void read_lock(rwlock_t *lock)
{
   /*Your code to acquire read lock*/
   while(lock->value<=0) sched_yield();
   while (atomic_add(&lock->value, -1)<0)
      atomic_add(&lock->value, 1);
}

void read_unlock(rwlock_t *lock)
{
   /*Your code to release the read lock*/
   atomic_add(&lock->value, 1);
}
