/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* One semaphore in a list. */
struct semaphore_elem
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };
/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value)
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/** less function for the thread,
    Compares the value of two list elements A and B, given
    auxiliary data AUX.  Returns true if A is less than B, or
    false if A is greater than or equal to B. */
static bool
highPriorityShiftsLeft (const struct list_elem *_a,
             const struct list_elem *_b,
             void *aux) {
    /// Retreive struct thread
    const struct thread *a = list_entry(_a, struct thread, elem);
    const struct thread *b = list_entry(_b, struct thread, elem);
    /// higher priority shifts left
    return (a->priority > b->priority);

}

static bool
highPriorityShiftsLeft_lock(const struct list_elem *_a,
                            const struct list_elem *_b,
                            void *aux) {
    const struct lock *a = list_entry(_a, struct lock, elem);
    const struct lock *b = list_entry(_b, struct lock, elem);

    return a->holder->priority > b->holder->priority;
}

static bool
highPriorityShiftsLeft_cond(const struct list_elem *_a,
                            const struct list_elem *_b,
                            void *aux) {
    const struct semaphore a = list_entry(_a, struct semaphore_elem, elem)->semaphore;
    const struct semaphore b = list_entry(_b, struct semaphore_elem, elem)->semaphore;

    //list_sort(&a.waiters, highPriorityShiftsLeft, NULL);
    //list_sort(&b.waiters, highPriorityShiftsLeft, NULL);

    struct thread *at = list_entry(list_front(&a.waiters), struct thread, elem);
    struct thread *bt = list_entry(list_front(&b.waiters), struct thread, elem);

    return at->priority > bt->priority;
}


/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0)
    {
      list_insert_ordered(&sema->waiters, &thread_current()->elem, highPriorityShiftsLeft, NULL);
      //printf("Blocking thread: %s\n", thread_current()->name);
      thread_block ();
    }
    //printf("sema_down: thread %s successfully obtained semaphore\n", thread_current()->name);
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema)
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0)
    {
      sema->value--;
      success = true;
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  /** TODO: Instead of popping front, we need to pop
  the one who has the highest priority
  it is important that sema->waiters have different list
  so it is necessary to iterate thru sema->waiters for highest priority
  */
  if (!list_empty (&sema->waiters)) {
      /// double check if any priority change,
      list_sort(&sema->waiters, highPriorityShiftsLeft, NULL);
      /*printf("sema_up: unblocking thread %s\n", (list_entry (list_front (&sema->waiters),
                                struct thread, elem))->name);*/
    //printf("sema_up: unblocking %s\n", list_entry(list_front(&sema->waiters), struct thread, elem)->name);
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  }
  //printf("sema_up: by thread %s\n", thread_current()->name);
  sema->value++;
  intr_set_level (old_level);
  /**if unblocked thread is higher than current thread, yield*/
    if (thread_current()->priority < thread_get_maxReadyPriority()) {
        thread_yield();
    }
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void)
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++)
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_)
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++)
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

    //lock = malloc(sizeof (struct lock));
  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/**
   lock_priorityDonation
        compare current thread's priority to holder's priority,
        if current current > holder,
            donate the priority to the holder,
            save original priority
            lock_priorityDonation
*/
void
lock_priorityDonation(struct lock *lock) {

    struct thread *beneficiary = lock->holder;
    struct thread *donor = thread_current();

    // if there exists lock holder
    if (beneficiary != NULL) {
        // check if this possible donor can donate
        if (donor->priority > beneficiary->priority) {
            if (beneficiary->donationLevel == 0) {
                beneficiary->originalPriority = beneficiary->priority;
            }
            beneficiary->priority = donor->priority;
            beneficiary->donationLevel++;
            //printf("Donation: %s donates to %s of priority %d\n", donor->name, beneficiary->name, beneficiary->priority);
        }
        if (beneficiary->lock_wanted != NULL) // Exit condition
            lock_priorityDonation(beneficiary->lock_wanted);
    }

}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

    /**
    Give the lock holder maximum priority between current and holder
    Make it go thru the chain
    */
//    enum intr_level old_level = intr_disable();
    thread_current()->lock_wanted = lock;
    lock_priorityDonation(lock);
//    intr_set_level(old_level);

  sema_down (&lock->semaphore);
  lock->holder = thread_current ();
  list_insert_ordered(&thread_current()->holding_locks, &lock->elem, highPriorityShiftsLeft_lock, NULL);
    thread_current()->lock_wanted = NULL;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success) /**Do the same for acquire here*/
    lock->holder = thread_current ();
  return success;
}

/**
    out of direct donor list, highest priority donor must be equal to current lock holder
    in order for the lock holder to return to its original priority
*/
/*
void
lock_priorityReturn(struct lock *lock) {
    struct thread *beneficiary = lock->holder;
    struct list waiters = lock->semaphore.waiters;
    //printf("lock_priorityReturn: from thread %s level=%d\n", beneficiary->name, beneficiary->donationLevel);
    if (!list_empty(&waiters)) {
        //max priority out of the donors,
        // set that max priority to beneficiary priority
        // donationLevel decrement
        // if there exists any locks that this thread is holding right now, then i will their max priority
        //list_sort(&waiters, highPriorityShiftsLeft, NULL);
        struct thread *highestPriorityThread = list_entry (list_front (&waiters),
                                struct thread, elem);
        /*printf("thread %s donors to thread %s, old: %d new %d\n",
               highestPriorityThread->name, beneficiary->name,
               beneficiary->priority, highestPriorityThread->priority);*//*
        if (beneficiary->priority == highestPriorityThread->priority) {
            // there is most likely direct donor
            if (beneficiary->donationLevel == 1) {
                printf("\n\n\nReturning to original priority\n\n\n");
                beneficiary->priority = beneficiary->originalPriority;
                beneficiary->donationLevel--;
            }else {
                 if(!list_empty(&beneficiary->holding_locks)) {
                     //list_sort(&beneficiary->holding_locks, highPriorityShiftsLeft_lock, NULL);
                     struct lock *nextHighestLock = list_entry(list_front(&beneficiary->holding_locks), struct lock, elem);
                     //list_sort(&nextHighestLock->semaphore.waiters, highestPriorityThread, NULL);
                     struct thread *nextHighestThread = list_entry(list_front(&nextHighestLock->semaphore.waiters), struct thread, elem);
                     beneficiary->priority = nextHighestThread->priority;
                     beneficiary->donationLevel--;
                 }else {
                    beneficiary->priority = beneficiary->originalPriority;
                    beneficiary->donationLevel--;
                 }
            }
        }

        //printf("Thread %s taking highest out of waiters which is %s\n", beneficiary->name, highestPriorityThread->name);
    }
}*/

/**
    function: roll-back priority
    lock holder has to have donated before, if not, no roll-back required
    lock holder has donated before, then the holder got donation as many times as its level
        that means, level = 1 is original
        otherwise, highest among every donors
*/


void
lock_priorityReturn(struct lock *lock) {
    struct thread* beneficiary = lock->holder;

    if (beneficiary->donationLevel > 0) {
        struct semaphore sem = lock->semaphore;
        struct list waiters = sem.waiters;

        if (!list_empty (&waiters)) { // catches if this lock has any waiting threads
            // but a thread can hold more than one lock, if level of donation is more than 0
            // so you need to check if the current thread has any other locks
            // if it does, out of these threads who want this lock, you need to check if
            // the highest priority thread donated to the current thread because that thread
            // is going to receive the lock now if it did
            struct thread* donor = list_entry (list_front (&waiters), struct thread, elem);
            //printf("current thread %s:%d highest waiting %s:%d\n", beneficiary->name, beneficiary->priority, donor->name, donor->priority);
            if(beneficiary->priority == donor->priority) {
                if (beneficiary->donationLevel == 1) {
                    beneficiary->priority = beneficiary->originalPriority;
                    beneficiary->donationLevel = 0;
                } else {

                    if (!list_empty (&beneficiary->holding_locks)) {
                        struct semaphore semap = list_entry (list_front (&beneficiary->holding_locks), struct lock, elem)->semaphore;
                        struct thread *highestWaiting = list_entry (list_front (&semap.waiters), struct thread, elem);
                        //if (beneficiary->priority < highestWaiting->priority) {
                            beneficiary->priority = highestWaiting->priority;
                            beneficiary->donationLevel--;
                        //}else {
                            // it has highest among all the threads, because of that, it's going to preempt the cpu
                            //beneficiary->donationLevel = 0;
                        //}
                    } else {
                        // beneficiary does not have anymore locks holding
                        beneficiary->priority = beneficiary->originalPriority;
                        beneficiary->donationLevel=0;
                    }
                }
            }
        }
    }
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));
    /**priority for this one will be taken care of by sema_up */

    //enum intr_level old_level = intr_disable();
  list_remove(&lock->elem);         // Remove this lock from holding list
  lock_priorityReturn(lock);
  lock->holder = NULL;
    //intr_set_level(old_level);
  sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock)
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}



/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock)
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  sema_init (&waiter.semaphore, 0);
  list_push_back(&cond->waiters, &waiter.elem);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
    list_sort(&cond->waiters, highPriorityShiftsLeft_cond, NULL);
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
