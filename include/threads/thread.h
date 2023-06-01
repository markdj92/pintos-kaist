#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif



/*쓰레드의 생명 주기에서 상태*/
enum thread_status {
	THREAD_RUNNING,     /* 스레드가 실행되기 위해 준비된 상태. 이 상태에서는 스레드 스케줄러에 의해 선택되어 실행될 수 있다. */
	THREAD_READY,       /* 스레드가 실행되기 위해 준비된 상태입니다. 이 상태에서는 스레드 스케줄러에 의해 선택되어 실행될 수 있습니다. */
	THREAD_BLOCKED,     /* 스레드가 특정 이벤트(예: 입출력 작업의 완료, 락의 해제)가 발생하기를 기다리는 상태입니다. 이 상태에서는 스레드가 다른 작업을 수행할 수 없으며, 이벤트가 발생할 때까지 기다립니다. */
	THREAD_DYING        /* 스레드의 실행이 완료되거나 예외가 발생하여 스레드가 종료된 상태입니다. 종료된 스레드는 더 이상 실행되지 않습니다. */
};

/* 스레드 식별자 유형 */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */


struct thread {
	/* thread.c에 의해 소유되는 식별자 */
	tid_t tid;                          /* 스레드 식별자 */
	enum thread_status status;          /* 스레드 상태 */
	char name[16];                      /* 스레드 이름(디버깅 용도) */
	int priority;                      /* 우선순위(priority) */
	
	int64_t wakeup_ticks;				/*local_ticks/wakeup_ticks*/
	// int recent_cpu;
	/* thread.c와 synch.c사이에서 공유되는 멤버 */
	struct list_elem elem;              /* 리스트 요소 elem 멤버는 thread.c와 synch.c사이에서 공유되는 리스트 요소를 나타낸다.*/
	/*이 멤버는 리스트에 스레드를 삽입하거나 제거하는 데 사용되며 스레드 관리와 동기화에 필요한 작업을 수행한다.*/
	int original_priority; /* 초기 우선순위를 저장하기 위한 필드 */
	struct lock *waiting_lock; /* 해당 스레드가 대기 중인 락의 주소를 저장할 필드 */
	struct list donations; /*여러 번의 우선순위 기부를 고려하기 위한 리스트*/
	struct list_elem d_elem; /*해당 리스트를 위한 elem*/
#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* 스위칭에 필요한 정보를 담고 있는 구조체 */
	unsigned magic;                     /* 스택 오버프로우를 감지하기 위한 값 */
};

/* "만약 false인 경우 (기본값), round-robin 스케줄러를 사용합니다.
만약 true인 경우, multi-level feedback queue 스케줄러를 사용합니다.
커널 커맨드 라인 옵션 "-o mlfqs"에 의해 제어됩니다." 
*/
/* Round-robin 스케줄링은 실행 가능한 스레드나 프로세스 간에 동일한 시간 슬라이스를 할당하여 순환적으로 CPU 실행을 분할합니다. 
이는 공정한 실행을 보장하고, 우선순위가 동일한 작업들이 CPU 시간을 공평하게 나누는 데 유용합니다.*/
/*multi-level feedback queue 스케줄러가 사용됩니다.
 Multi-level feedback queue 스케줄링은 작업의 우선순위를 여러 개의 큐로 구분하고,
 우선순위에 따라 다른 큐에서 작업을 선택합니다.
 이는 작업의 특성에 따라 우선순위를 조정하여 성능을 향상시키는데 도움이 됩니다.*/
extern bool thread_mlfqs;
/*thread_mlfqs라는 외부 변수(extern)로 선언되어 있습니다. 이 변수는 multi-level feedback queue 스케줄링 여부를 나타내는 불리언 값입니다.*/
extern int64_t global_tick;
/*Global Tick (전역 시각):

Global tick은 전체 시스템에 대한 시간의 흐름을 나타냅니다.
Global tick은 일반적으로 하드웨어 타이머의 인터럽트를 기반으로 증가합니다.
시스템의 모든 스레드와 타이머 이벤트에 대한 상대적인 시간을 추적하는 데 사용됩니다.
스레드의 wakeup_ticks 값과 비교하여 스레드를 깨우는 등 시간 기반의 스케줄링 결정에 활용됩니다.*/

//readylist의 우선순위가 가장 높은 값이랑 현재 running_thread의 우선순위를 비교
void test_max_priority(void);


void thread_init (void);
void thread_start (void);
/*스레드 초기화와 스레드 시작 함수를 선언하고 있습니다.*/

void thread_tick (void);
void thread_print_stats (void);
/*스레드 틱(tick)과 스레드 통계 정보 출력 함수를 선언하고 있습니다.*/

typedef void thread_func (void *aux);
/*thread_func라는 함수 타입을 정의하고 있습니다. 이 타입은 void 형식의 aux 포인터 인자를 가지는 함수를 의미합니다.*/
tid_t thread_create (const char *name, int priority, thread_func *, void *);
/*새로운 스레드를 생성하는 함수를 선언하고 있습니다.
 이 함수는 스레드 이름, 우선순위, 함수 포인터와 aux 포인터를 인자로 받으며, 생성된 스레드의 식별자(tid_t)를 반환합니다.*/

void thread_block (void);
void thread_unblock (struct thread *);
/* 스레드를 블록시키거나 언블록시키는 함수를 선언하고 있습니다.
블록된 스레드는 실행 대기 상태에서 제외되고, 언블록된 스레드는 다시 실행 대기 상태로 들어갑니다.*/

struct thread *thread_current (void);
/* 현재 실행 중인 스레드의 포인터를 반환하는 함수를 선언하고 있습니다.*/
tid_t thread_tid (void);
const char *thread_name (void);
/*현재 스레드의 식별자와 이름을 반환하는 함수를 선언하고 있습니다.*/
void thread_exit (void) NO_RETURN;
void thread_yield (void);
/*스레드를 종료하고 실행을 양도하는 함수를 선언하고 있습니다. NO_RETURN은 해당 함수가 반환하지 않는다는 것을 나타냅니다.*/
int thread_get_priority (void);
void thread_set_priority (int);
/*현재 스레드의 우선순위를 반환하거나 설정하는 함수를 선언하고 있습니다.*/
int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);
/*멀티레벨 피드백 큐 스케줄러에서 사용되는 함수들로, 각각 현재 스레드의 nice 값, nice 값을 설정하는 함수, 최근 CPU 사용량, 로드 평균을 반환하는 함수를 선언하고 있습니다.*/
void do_iret (struct intr_frame *tf);
/*인터럽트 프레임을 인자로 받아 실행을 복원하는 함수를 선언하고 있습니다.*/
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void donate_priority(void);
void remove_with_lock(struct lock *lock);
void refresh_priority(void);


#endif /* threads/thread.h */
