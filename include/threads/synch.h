#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* semaphore는 스레드 간의 동기화를 달성하기 위해 사용되는 자료 구조입니다.

멤버 변수:
- value: 현재 semaphore의 값.
- waiters: 대기 중인 스레드들의 리스트. (list 구조체)*/
struct semaphore {
	unsigned value;             
	struct list waiters;
};

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* lock은 스레드의 동기화를 제어하는 데 사용되는 자료 구조입니다. */
struct lock {
	struct thread *holder;      /* Tholder: 락을 소유하고 있는 스레드 (디버깅용). */
	struct semaphore semaphore; /* semaphore: 접근을 제어하는 이진 세마포어. */
};

void lock_init (struct lock *); //lock 자료구조를 초기화
void lock_acquire (struct lock *); //lock을 요청
bool lock_try_acquire (struct lock *); //lock을 반환
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* condition은 컨디션 변수라는 동기화 프리미티브를 나타냅니다.
동기화 프리미티브는 여러 프로세스나 스레드 간에 데이터를 안전하게 공유하도록 도와주는 기본적인 도구나 메커니즘을 말합니다.
*/
struct condition {
	struct list waiters;        
	/* 이 필드는 대기 중인 스레드들의 목록을 관리합니다.
	 컨디션 변수에 대기 중인 모든 스레드들이 이 목록에 포함되며,
	 스레드가 컨디션 변수를 통해 신호를 받으면 이 목록에서 제거됩니다. */
};
/*이 함수는 컨디션 변수를 초기화합니다. 컨디션 변수가 사용되기 전에 이 함수를 호출해야 합니다*/
void cond_init (struct condition *);
/* 이 함수는 주어진 락을 해제하고, 컨디션 변수에 대해 대기하는 스레드를 차단합니다.*/
void cond_wait (struct condition *, struct lock *);
/*이 함수는 대기 중인 스레드 중 하나에게 신호를 보냅니다. 신호를 받은 스레드는 대기 상태에서 벗어나 작업을 계속합니다. */
void cond_signal (struct condition *, struct lock *);
/* 이 함수는 모든 대기 중인 스레드에게 신호를 보냅니다. 이를 통해 대기 중인 모든 스레드가 깨어나서 작업을 계속하게 됩니다.*/
void cond_broadcast (struct condition *, struct lock *);


/* Optimization barrier.
 *
 * The compiler will not reorder operations across an
 * optimization barrier.  See "Optimization Barriers" in the
 * reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
