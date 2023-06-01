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


/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */


bool cmp_sema_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);


void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* sema_down 함수는 특정 리소스에 대한 액세스를 제어하기 위해 세마포어를 사용하는데,
 이 함수는 세마포어 값을 '내리는' 동작을 수행합니다.
 intr_disable()를 통해 현재의 인터럽트 상태를 비활성화하고, 이전 상태를 저장합니다.
 만약 세마포어의 값이 0 (즉, 사용 가능한 리소스가 없음) 이라면, 현재 스레드는 대기 상태로 전환되며 세마포어의 대기 목록에 추가됩니다.
 세마포어의 값이 0이 아니라면, 즉 리소스가 사용 가능하다면, 세마포어의 값은 1 감소하며, 스레드는 리소스를 사용하게 됩니다.
 마지막으로 인터럽트 상태를 복원합니다.
 이런 식으로 sema_down 함수는 세마포어를 이용해 공유 리소스에 대한 스레드 접근을 동기화하고 제어하는 역할을 합니다. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		// list_push_back (&sema->waiters, &thread_current ()->elem);
		list_insert_ordered(&sema->waiters, &thread_current()->elem, cmp_sema_priority, NULL);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
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
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	list_sort(&sema->waiters, cmp_priority, NULL);
	if (!list_empty (&sema->waiters))
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
		
	sema->value++;
	test_max_priority();
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
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
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* lock_init 함수는 lock을 초기화하는 역할을 합니다.

인자:
- lock: 초기화할 lock 구조체의 포인터 (struct lock *)

동작:
- lock이 NULL인지 확인하고, NULL이 아니라면 ASSERT를 통해 확인합니다.
- lock의 holder를 NULL로 설정합니다.
- lock의 semaphore를 1로 초기화합니다. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* 
이 함수는 "lock_acquire"라는 이름에서 알 수 있듯이, 특정 자원에 대한 lock을 획득하는 역할을 합니다. 주로 멀티 스레딩 환경에서 공유 자원을 안전하게 관리하기 위해 사용됩니다. 이 함수는 다음과 같은 작업을 수행합니다:

ASSERT (lock != NULL); : 함수에 전달된 lock 인스턴스가 NULL이 아닌지 확인합니다. 이는 입력의 유효성 검사로, lock이 NULL이면 프로그램은 에러를 발생시킵니다.

ASSERT (!intr_context ()); : 현재 코드가 인터럽트 컨텍스트 내에서 실행되고 있는지 확인합니다. 이는 함수가 인터럽트 처리 루틴에서 호출되는 것을 방지하기 위한 것입니다. 이는 안전하지 않으며, 종종 디버깅이 어렵거나 불가능한 문제를 일으킬 수 있습니다.

ASSERT (!lock_held_by_current_thread (lock)); : 현재 스레드가 이미 lock을 가지고 있지 않은지 확인합니다. 재진입을 방지하기 위한 것으로, 이미 lock을 가지고 있는 스레드가 다시 lock을 획득하려고 시도하는 것을 방지합니다.

sema_down (&lock->semaphore); : 세마포어를 사용하여 lock 획득을 시도합니다. 이 함수는 lock이 사용 가능해질 때까지 현재 스레드를 차단합니다.

lock->holder = thread_current (); : lock을 획득한 후에, 이 함수는 lock의 소유자를 현재 스레드로 설정합니다. 이는 다른 스레드가 이 lock을 사용하려는 시도를 방지합니다.

결국, 이 함수는 전달받은 lock을 안전하게 획득하는 역할을 합니다. 만약 lock이 이미 다른 스레드에 의해 사용 중이라면, 해당 lock이 해제될 때까지 기다립니다.*/
void
lock_acquire (struct lock *lock) {
	
	int curr_priority = thread_get_priority();

	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));
	/*🙌*/
	// if (lock->holder != NULL)
	// {
	// 	thread_current()->waiting_lock = lock;
	// 	if (lock->holder->priority < curr_priority)	
	// 		nested_donation(lock, curr_priority);
	// 	list_push_back(&lock->holder->donations, &thread_current()->d_elem);		
	// }
	sema_down (&lock->semaphore);
	lock->holder = thread_current ();
	thread_current()->waiting_lock = NULL;
	
}

/* 해당 코드는 LOCK을 획득하려고 시도하고, 성공하면 true를 반환하고 실패하면 false를 반환합니다.
 현재 스레드에 의해 이미 보유된 락을 획득하는 것은 허용되지 않습니다.
 이 함수는 슬립하지 않으므로 인터럽트 핸들러 내에서 호출될 수 있습니다. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));
	/*👍*/
	if (thread_mlfqs == false) {
		remove_lock(lock);
		donate_priority();
	}

	lock->holder = NULL;
	sema_up (&lock->semaphore);
	// {
	// 	int holder_priority;
	// 	int donor_priority;
	// 	holder_priority = lock->holder->original_priority;
	// 	/* donation list 에서 스레드를 제거하고
	// 	우선순위를 다시 계산하도록 remove_with_lock():donation list에서 스레드 엔트리를 제거 *
	// 	, refresh_priority():우선순위를 다시 계산 함수를 호출 */

	// 	if (!list_empty(&lock->holder->donations))
	// 	{
	// 		/*donation list에서 스레드 엔트리를 제거*/
	// 		remove_released_thread(lock);
	// 		/* priority를 다시 계산 함수를 호출
	// 			lock holder의 priority 무조건 최대로하기
	// 		*/
	// 		/*multiple donation*/
	// 		donor_priority = find_max_priority(&lock->holder->donations);
	// 		if (donor_priority > holder_priority) /*lock들고 있는 thread의 priority가 크다면 그값을 갖고 아니면 donate 전 값이 높다면 그 값으로 */
	// 			lock->holder->priority = donor_priority;
	// 		else
	// 			lock->holder->priority = holder_priority;
	// 	}
	// 	/*lock 대기 중인 thread 있음*/
	// 	if (lock->holder->waiting_lock != NULL)
	// 	{ /*thread priority 물려주기*/
	// 		nested_donation(lock->holder->waiting_lock, lock->holder->priority);
	// 	}
	// }
	
	
	lock->holder = NULL;
	sema_up (&lock->semaphore);
}




/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
	int priority;
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
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
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));
	/*😂*/
	waiter.priority = thread_get_priority();
	sema_init (&waiter.semaphore, 0);
	list_insert_ordered(&cond->waiters, &waiter.elem, cmp_sema_priority, NULL);
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
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}

bool cmp_sema_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
	int priority_a = list_entry(a, struct semaphore_elem, elem)->priority;/*list_entry 함수를 사용하여 첫 번째 리스트 원소(a)를 struct thread 타입으로 변환합니다.*/
	int priority_b = list_entry(b, struct semaphore_elem, elem)->priority;/*list_entry 함수를 사용하여 두 번째 리스트 원소(b)를 struct thread 타입으로 변환합니다.*/
	
	return priority_a > priority_b;	/*첫 번째 스레드(thread_a)의 우선 순위가 두 번째 스레드(thread_b)의 우선 순위보다 높으면 true를 반환합니다.*/
	
};

