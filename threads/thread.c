#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* thread.h 파일의 맨 위에 있는 큰 주석을 참조하라고 언급하며,
 구조체 thread의 magic 멤버에 대해 어떤 역할을 하는지 설명하고 있습니다.
 magic 멤버는 스택 오버플로우를 감지하는 데 사용되는 무작위 값을 나타냅니다.

스택 오버플로우란, 프로그램이 실행되는 동안 스택 메모리 영역을 초과하여 데이터가 덮어씌워지는 상황을 말합니다.
스택은 함수 호출과 관련된 데이터를 저장하는 데 사용되는 메모리 영역으로,
스택 오버플로우는 일반적으로 버그나 잘못된 프로그래밍으로 인해 발생합니다.

struct thread은 스레드의 정보를 저장하는 데 사용되는 구조체입니다.
magic 멤버는 스택 오버플로우를 감지하기 위한 보조적인 수단으로 사용됩니다.
해당 멤버에는 무작위로 생성된 값이 할당됩니다.

이 magic 멤버는 스레드가 할당된 스택의 끝 부분에 위치하며,
스택의 영역을 초과하는 경우 magic 값이 덮어씌워지게 됩니다.
스택 오버플로우가 발생하면 magic 값이 손상되고, 이를 이용하여 스택 오버플로우를 감지할 수 있습니다.
스택 오버플로우가 감지되면 프로그램은 이에 대응하여
예외 처리를 수행하거나 프로그램을 종료시키는 등의 동작을 수행할 수 있습니다.

이러한 방식으로 magic 멤버를 사용하면 스택 오버플로우를 신속하게 감지하고, 프로그램의 안정성을 높일 수 있습니다. */
#define THREAD_MAGIC 0xcd6abf4b

/*"기본 스레드"는 보통 프로그램의 실행 초기에 생성되는 메인 스레드를 의미할 수 있습니다.
 이 스레드는 프로그램의 주요 작업을 담당하고, 
 다른 스레드들을 생성하거나 관리하는 역할을 수행할 수 있습니다. */
#define THREAD_BASIC 0xd42df210

/*  "THREAD_READY" 상태에 있는 프로세스들의 목록을 나열하고 있으며,
 이 목록은 실행 가능한 프로세스들을 파악하고,
 이들을 스케줄링하여 CPU를 공정하게 할당하는 데 도움이 됩니다.
 이 목록은 실행 대기 중인 프로세스들의 상태를 파악하고,
 프로세스 스케줄링 알고리즘에 의해 이들을 실행으로 전환할 수 있도록 도와줍니다. */
static struct list ready_list;

/*슬립 상태에 있는 스레드란, 일시적으로 실행을 중지하고 대기 상태에 있는 스레드를 말합니다.
 스레드가 특정 이벤트의 발생을 기다리거나, 특정 시간 지연 후에 다시 실행되어야 할 때 슬립 상태에 들어갈 수 있습니다. */
static struct list sleep_list;

static struct list wait_list;

/* 유휴 스레드 
 예를 들어, 시스템에서는 CPU의 낭비를 방지하기 위해 유휴 상태일 때 주기적으로 타이머 인터럽트를 처리하거나,
 전력 관리를 위한 작업을 수행할 수 있습니다. */
static struct thread *idle_thread;

/* 초기 스레드
 "init.c:main()"을 실행 중인 스레드를 의미합니다.
 운영 체제에서 초기 스레드는 시스템이 부팅될 때 자동으로 생성되는 첫 번째 스레드입니다.
 이 스레드는 주로 시스템 초기화 및 기타 초기화 작업을 수행하는 역할을 담당합니다. */
static struct thread *initial_thread;

/*allocate_tid()에서 사용하는 락(lock)입니다.*/
/*이 주석은 allocate_tid() 함수에서 사용되는 락에 대한 설명을 제공합니다.
 락은 여러 스레드가 동시에 접근하는 공유 자원의 동기화를 보장하기 위해 사용되는 동기화 기법입니다.
 tid_lock은 allocate_tid() 함수에서 스레드 식별자를 할당할 때 사용되는 락으로, 스레드 식별자 할당 작업이 동시에 실행되는 것을 방지하여 충돌을 예방합니다.*/
static struct lock tid_lock;

/* 스레드 소멸 요청을 관리하기 위한 리스트입니다 */
/*destruction_req 리스트는 스레드 소멸 요청을 저장하고 관리하는 데 사용됩니다.
 소멸 요청은 스레드가 종료되어야 할 때, 해당 스레드의 자원을 해제하고 정리하기 위해 사용됩니다.
 이 리스트를 통해 스레드 소멸 요청을 추가하거나 처리할 수 있습니다.*/
static struct list destruction_req;

int64_t global_tick;

/* Statistics. */
static long long idle_ticks;    /* # 유휴 상태에서 소비한 타이머 틱의 수를 나타내는 long long 형식의 변수입니다. */
static long long kernel_ticks;  /* # 커널 스레드가 실행되는 동안 소비한 타이머 틱의 수를 추적합니다. 커널 스레드는 운영 체제의 핵심 부분을 실행하는 스레드입니다. */
static long long user_ticks;    /* # 용자 프로그램이 실행되는 동안 소비한 타이머 틱의 수를 추적합니다. 사용자 프로그램은 커널이 아닌 일반 응용 프로그램을 의미합니다. */

/* Scheduling. */
#define TIME_SLICE 4            /* # 각 스레드에 할당되는 타이머 틱의 수를 나타내는 상수입니다. 각 스레드는 TIME_SLICE 값만큼의 타이머 틱을 사용할 수 있습니다. */
static unsigned thread_ticks;   /* # 마지막으로 스레드가 양보(yield)한 이후로 경과한 타이머 틱의 수를 나타내는 unsigned 형식의 변수입니다. */

/* "만약 false인 경우 (기본값), 라운드 로빈 스케줄러를 사용합니다.
	만약 true인 경우, 멀티레벨 피드백 큐 스케줄러를 사용합니다.
	이는 커널 커맨드라인 옵션 '-o mlfqs'에 의해 제어됩니다." */
/*mlfqs="Multi-Level Feedback Queue Scheduling"의 약자로, 멀티레벨 피드백 큐 스케줄링을 의미합니다.*/
bool thread_mlfqs;
/*커널 스레드를 생성하는 함수입니다. 주어진 함수 포인터 thread_func와 aux 인자를 사용하여 스레드를 생성합니다.*/
static void kernel_thread (thread_func *, void *aux);
/*Idle 스레드의 실행 함수입니다. Idle 스레드는 CPU에 아무 작업도 수행하지 않고 대기하는 역할을 합니다. 해당 함수는 aux 인자를 사용하지 않으며, 비사용(UNUSED) 특성을 가지고 있습니다.*/
static void idle (void *aux UNUSED);
/*실행할 다음 스레드를 선택하는 함수입니다. 다음에 실행될 스레드를 결정하는 스케줄링 알고리즘에 따라 선택됩니다.*/
static struct thread *next_thread_to_run (void);
/*스레드를 초기화하는 함수입니다. 주어진 이름(name)과 우선순위(priority)를 사용하여 스레드를 초기화합니다.*/
static void init_thread (struct thread *, const char *name, int priority);
/*스케줄링을 수행하는 함수입니다. 주어진 스레드 상태(status)에 따라 스케줄링을 수행합니다.*/
static void do_schedule(int status);
/*다음 실행할 스레드를 선택하고, 현재 실행 중인 스레드와 선택된 스레드 간의 전환을 수행하는 함수입니다.
 스케줄링 알고리즘에 따라 실행할 스레드를 선택하고, 컨텍스트 전환을 통해 선택된 스레드를 실행합니다.*/
static void schedule (void);
/* 스레드에 할당할 고유한 식별자(tid)를 생성하는 함수입니다. 새로운 스레드가 생성될 때마다 호출되어 고유한 식별자를 할당합니다.*/
static tid_t allocate_tid (void);
/*cmp_priority라는 함수는 주어진 두 개의 list_elem 구조체를 비교하고 불리언 값을 반환하는 것으로 보입니다.
 여기서 aux는 추가적인 정보를 전달하는 데 사용될 수 있으며,
 UNUSED 매크로는 이 변수가 현재 함수에서 사용되지 않는다는 것을 나타내는 것으로 보입니다.
 UNUSED는 변수를 사용하지 않음으로 인한 경고를 방지하는 것일 수 있습니다.*/
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);



/* "T가 유효한 스레드를 가리키는 것으로 보인다면 true를 반환합니다." */
/*(t) != NULL: 포인터 t가 NULL이 아닌지 확인합니다. NULL은 유효한 메모리 주소가 아니므로, 스레드를 가리키지 않는 경우 false를 반환합니다.

(t)->magic == THREAD_MAGIC: t가 가리키는 구조체의 magic 멤버가 THREAD_MAGIC과 동일한지 확인합니다.
 magic 멤버는 스레드 구조체 내에 있는 임의의 값으로, 스레드가 정상적으로 초기화되었음을 나타내는 용도로 사용됩니다.
 따라서 magic 값이 THREAD_MAGIC과 일치하지 않는 경우에는 유효한 스레드를 가리키지 않는 것으로 간주되어 false를 반환합니다.*/
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* 현재 CPU의 스택 포인터 rsp를 읽습니다. 스택 포인터는 현재 실행 중인 코드에서 사용되는 스택의 최상단을 가리키는 포인터입니다.
rsp를 가장 가까운 페이지의 시작점으로 내림(round down)합니다. 이렇게 함으로써 rsp는 스레드 구조체의 시작 주소로 정렬됩니다.
rsp로부터 시작되는 페이지는 항상 스레드 구조체를 포함하고 있으므로, rsp를 스레드 구조체의 시작 주소로 간주하여 현재 실행 중인 스레드를 찾을 수 있습니다. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// thread_start() 함수를 위한 전역 기술자 테이블(Global Descriptor Table, GDT)에 관련된 내용을 설명하고 있습니다.
// GDT는 x86 아키텍처에서 사용되는 메모리 세그먼트를 정의하는 데이터 구조입니다.
// 메모리 세그먼트는 메모리 접근 권한, 범위 등의 속성을 가지고 있으며, 세그먼트 디스크립터는 이러한 속성을 저장하고 관리하는 역할을 합니다.
// 해당 주석에서는 "thread_start"에 대한 전역 기술자 테이블(GDT)이 설정되어야 한다는 내용을 설명하고 있습니다.
// 그러나 GDT는 thread_init() 함수 호출 이후에 설정되기 때문에, thread_init() 함수 이전에는 임시 GDT를 설정해야 합니다.
// 임시 GDT는 thread_init() 함수가 호출되기 전에 설정되는데, 이렇게 함으로써 스레드 초기화가 완료되고 GDT가 설정된 후에 올바른 GDT가 사용될 수 있도록 보장됩니다.
// 따라서 주석은 "thread_start" 함수를 위해 임시 GDT를 설정해야 한다는 내용을 나타내고 있습니다. 이는 thread_init() 함수 이전에 수행되어야 하는 작업입니다.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* 현재 실행 중인 코드를 스레드로 변환하여 스레딩 시스템을 초기화합니다.
 일반적으로 이러한 작업은 가능하지 않지만, 이 경우에만 가능한 이유는 loader.S 파일이 스택의 하단을 페이지 경계로 배치함으로써 가능하게 했기 때문입니다.
 또한 실행 대기열(run queue)과 TID(lock)를 초기화합니다.
 이 함수를 호출한 후에는 thread_create()를 사용하여 스레드를 생성하기 전에 페이지 할당기(page allocator)를 초기화해야 합니다.
 이 함수가 완료될 때까지 thread_current()를 호출하는 것은 안전하지 않습니다. 즉, thread_current() 함수를 호출하기 전에 thread_init() 함수가 완료되도록 해야 합니다.
 따라서 thread_init() 함수는 스레딩 시스템의 초기화를 수행하며, 실행 중인 코드를 스레드로 변환하고 실행 대기열과 TID(lock)를 초기화합니다.
 이 함수를 호출한 후에는 페이지 할당기를 초기화해야 하며, thread_current() 함수를 호출하기 전에 thread_init() 함수가 완료되도록 주의해야 합니다.*/

void
thread_init (void) { //인터럽트가 비활성화된 상태에서 실행되어야 함을 보장하기 위해 ASSERT(intr_get_level() == INTR_OFF)를 사용하여 인터럽트가 비활성화되어 있는지 확인합니다.
	ASSERT (intr_get_level () == INTR_OFF);
	/*  이 코드는 커널 초기화 과정에서 임시 GDT를 로드하고,
	 이 임시 GDT에는 사용자 모드와 관련된 정보가 없으며,
	 커널이 초기화되면 gdt_init() 함수를 통해 사용자 컨텍스트를 포함한 GDT가 다시 구성될 것임을 나타냅니다. */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* 전역 스레드 컨텍스트를 초기화 */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&destruction_req);
	list_init (&sleep_list);

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();

	global_tick = 0x7FFFFFFFFFFFFFFF;
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) {
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down (&idle_started);
}

/*  이 함수는 타이머 인터럽트 핸들러의 호출에 응답하여 타이머 틱마다 실행되며,
 주로 스케줄링과 타이밍과 관련된 작업을 수행합니다.
 이 함수는 인터럽트 컨텍스트에서 실행되므로 인터럽트에 관련된 처리를 수행하는 데 유용하게 사용될 수 있습니다. */
void
thread_tick (void) {
	struct thread *t = thread_current ();
	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

void
thread_compare(struct thread *t1, struct thread *t2) {
	return t1->priority > t2->priority ? t1 : t2;	

}

/* "NAME이라는 이름과 주어진 초기 PRIORITY로 새 커널 스레드를 생성하며,
 이 스레드는 AUX를 인자로하여 FUNCTION을 실행합니다.
 그리고 이 스레드를 준비 큐에 추가합니다.
 새 스레드의 스레드 식별자를 반환하거나, 생성이 실패하면 TID_ERROR를 반환합니다.
 thread_start()가 호출되었다면,
 새 스레드는 thread_create()가 반환하기 전에 스케줄링될 수 있습니다.
 심지어는 thread_create()가 반환하기 전에 종료될 수도 있습니다.
 반대로, 원래 스레드는 새 스레드가 스케줄링되기 전에 임의의 시간동안 실행될 수 있습니다. 순서를 보장해야 한다면 세마포어나 다른 형태의 동기화를 사용하세요.
 제공된 코드는 새 스레드의 priority 멤버를 PRIORITY로 설정하지만,
 실제 우선 순위 스케줄링은 구현되어 있지 않습니다.
 우선 순위 스케줄링은 Problem 1-3의 목표입니다."
 이 설명은 커널 스레드 생성에 관한 것으로, thread_create()라는 함수가 어떤 동작을 수행하는지 설명하고 있습니다.
 이 함수는 특정 이름, 초기 우선 순위, 실행할 함수, 그리고 그 함수의 인자를 가지는 새로운 커널 스레드를 생성하고,
 그 스레드를 준비 큐에 추가합니다.
 또한 이 설명에서는 thread_create() 함수가 반환하기 전에 새 스레드가 스케줄링되거나,
 심지어 종료될 수 있다는 점을 언급하고 있습니다.
 이러한 동작은 스레드 스케줄링의 비결정적 특성 때문에 발생합니다.
 이를 보완하기 위해 순서를 보장해야 하는 경우에는 세마포어 등의 동기화 메커니즘을 사용하는 것이 필요하다고 언급하고 있습니다.
 마지막으로, 제공된 코드는 새로운 스레드의 우선 순위를 설정하지만,
 실제로 우선 순위에 따른 스케줄링은 구현되어 있지 않다고 합니다.
 이러한 우선 순위 스케줄링은 문제 1-3에서 해결하도록 목표를 설정하고 있습니다. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;
	
	/*disable😒*/
	ASSERT (function != NULL);

	/* thread의 위치 */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* "만약 스케줄링이 되면 kernel_thread를 호출하십시오.
		참고) rdi는 첫 번째 인자이고, rsi는 두 번째 인자입니다." */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock (t);//현재 실행중인 스레드
	// if (t->priority > thread_get_priority()) {
	// 	thread_yield();
	// }
	enum intr_level old_level;
	old_level = intr_disable();
	test_max_priority();

	intr_set_level(old_level);
	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}



/* "차단된 스레드 T를 실행 가능한 상태로 전환한다"는 것은,
 스레드가 어떤 작업을 기다리는 동안 실행을 멈추었다가, 이제 실행을 준비하는 상태로 전환되는 것을 의미합니다.
"만약 T가 차단되어 있지 않다면 이것은 오류입니다"는 스레드가 이미 차단 상태가 아닌데 이 함수가 호출되면 문제가 생길 수 있음을 의미합니다.
 이런 경우에는 thread_yield() 함수를 사용하여 실행 중인 스레드를 실행 가능한 상태로 만들어야 합니다.
"이 함수는 실행 중인 스레드를 선점하지 않습니다"는 이 함수가 실행 중인 스레드를 중단시키지 않고 그대로 둔다는 것을 의미합니다.
 이는 중요한 특성이 될 수 있습니다. 만약 호출자가 직접 인터럽트를 비활성화했다면, 이 함수를 호출하여 스레드의 차단을 원자적으로(다른 작업이 동시에 수행되지 않도록) 해제하고, 그 외 다른 데이터를 업데이트할 수 있다는 것을 의미합니다. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	list_insert_ordered(&ready_list, & t->elem, cmp_priority, NULL);/**/
	t->status = THREAD_READY;
	intr_set_level (old_level); /*"매개변수로 전달된 상태를 인터럽트의 상태로 설정하고 이전 인터럽트 상태를 반환한다."*/
}

/* Returns the name of the running thread. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* "현재 실행 중인 스레드를 반환한다.
이는 running_thread() 함수에 몇 가지 정합성 검사를 추가한 것이다.
자세한 내용은 thread.h 상단의 큰 주석을 참조하라." */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/*  스레드 T가 실제로 스레드인지 확인하고, 스레드의 스택 오버플로우 여부를 확인하기 위한 것입니다.
	 만약 이러한 어설션(assertion)이 발생한다면, 스레드가 스택을 오버플로우한 것일 수 있습니다.
	 각 스레드는 4KB 미만의 스택을 가지고 있으므로, 큰 자동 배열이나 중간 정도의 재귀 호출 등이 스택 오버플로우를 발생시킬 수 있습니다.*/
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU라는 표현은 현재 실행 중인 스레드가 CPU를 양보한다는 의미입니다.
 이는 현재 스레드가 다른 스레드에게 CPU 실행 시간을 양보하고,
 스케줄러에 의해 즉시 다시 스케줄될 수 있다는 것을 나타냅니다. */
/*스케줄러는 실행 가능한 스레드 중에서 어떤 스레드를 다음에 실행할지 결정하는 주체입니다.
 Yields the CPU를 호출하면 현재 스레드는 다른 스레드에게 CPU 실행을 양보하므로, 스케줄러는 양보된 스레드를 다시 실행할 수 있도록 선택할 수 있습니다.
 스케줄러의 판단에 따라 즉시 다시 스케줄되기도 하고, 나중에 다시 실행되기도 합니다.*/
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();//인터럽트를 비활성화 하고 이전의 인터럽트 상태를 반환한다.
	if (curr != idle_thread)
		list_insert_ordered(&ready_list, & curr->elem, cmp_priority, NULL);/**/
	curr->status = THREAD_READY;
	schedule ();
	
	intr_set_level (old_level);
}
//현재 스레드를 나타내는 포인터 curr를 얻는다. 이후 인터럽트 레벨을 저장하기 위한 변수 old_level을 선언한다.
//intr_context()함수를 사용하여 현재 코드가 인터럽트 컨텍스트에서 실행 중인지 확인한다. 인터럽트 컨텍스트에서는 스레드를 양보하는 것이 불가능하다.
//ASSERT문을 사용하여 이를 확인한다.
//intr_disable()함수를 호출하여 현재 인터럽트를 비활성화한다. 이렇게 함으로써 스레드 스케줄링 도중 다른 인터럽트가 발생하는 것을 방지한다.
//cuur이 idle스레드(아무런 작업을 수행하지 않고 대기하는 상태일 때 실행되는 특수한 종류의 스레드)가 아닌 경우, 현재 스레드를 실행 대기 리스트인 ready_list의 뒤쪽에 추가한다.
//이렇게 하게되면 현재 스레드는 실행가능한 상태(THREAD_READY)가 되며, 다른 스레드에게 CPU실행을 양보한다.
//schedule()함수를 호출하여 스케줄러에 의해 다음에 실행될 스레드를 선택한다. Schedule()함수는 실행 가능한 스레드 중에서 가장 우선순위가 높은 스레드를 선택하여 cpu를 할당한다.
//마지막으로 intr_set_level()함수를 사용하여 이전의 인터럽트 레벨로 복원한다. 이를 통해 이전에 활성화되었던 인터럽트가 다시 활성화된다.
//thread_yield()함수는 현재 스레드를 양보하고 스케줄러에게 cpu실행을 양보한 후, 다음에 실행될 스레드를 선택하여 cpu를 할당하게 된다.



/* "현재 스레드의 우선 순위를 NEW_PRIORITY로 설정합니다." */
void
thread_set_priority (int new_priority) {
	thread_current ()->priority = new_priority;
	// if(list_entry (list_front(&ready_list), struct thread, elem)->priority > new_priority)
	// 	thread_yield();
	test_max_priority();
	
}
//readylist의 우선순위가 가장 높은 값이랑 현재 running_thread의 우선순위를 비교 // !list_empty(&ready_list) && 예외처리 무조건 해줘야함!!!!!!!!!!!!!
void
test_max_priority(void) {
	if(!list_empty(&ready_list) && list_entry (list_front(&ready_list), struct thread, elem)->priority > thread_current()->priority)
		thread_yield();
}

/* 현재 스레드의 우선순위를 반환한다. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}


/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* 이 코드는 init_thread라는 함수를 통해 스레드 초기화를 수행합니다.
결국, init_thread 함수는 이름과 우선순위가 주어진
새로운 스레드를 안전하게 초기화하는 작업을 수행합니다. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);//ASSERT (t != NULL); : 입력으로 들어온 스레드 t가 NULL이 아닌지 확인합니다. 만약 NULL이면, 프로그램은 에러를 발생시킵니다.
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);//ASSERT (PRI_MIN <= priority && priority <= PRI_MAX); : 주어진 priority가 허용된 범위 내에 있는지 확인합니다. priority 값이 PRI_MIN보다 작거나 PRI_MAX보다 크면, 프로그램은 에러를 발생시킵니다.
	ASSERT (name != NULL);//ASSERT (name != NULL); : name이 NULL이 아닌지 확인합니다. name이 NULL이면 프로그램은 에러를 발생시킵니다.

	memset (t, 0, sizeof *t);//memset (t, 0, sizeof *t); : 스레드 구조체를 0으로 초기화합니다. 이렇게 함으로써 모든 멤버가 기본값으로 설정됩니다.
	t->status = THREAD_BLOCKED;//t->status = THREAD_BLOCKED; : 스레드의 상태를 '차단됨'으로 설정합니다. 초기화시에 스레드는 차단 상태로 시작하며, 스케줄러나 다른 함수에 의해 나중에 '실행 가능' 상태로 변경될 수 있습니다.
	strlcpy (t->name, name, sizeof t->name);//strlcpy (t->name, name, sizeof t->name); : 주어진 name을 스레드의 이름으로 복사합니다.
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);//t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *); : 스레드의 스택 포인터를 설정합니다. 이 포인터는 스레드의 스택에 데이터를 푸시하거나 팝하는 데 사용됩니다.
	t->priority = priority;//t->priority = priority; : 주어진 priority를 스레드의 우선순위로 설정합니다.
	t->magic = THREAD_MAGIC;//t->magic = THREAD_MAGIC; : 스레드의 '마법 값'을 설정합니다. 이 값은 주로 디버깅에서 스레드가 올바르게 초기화되었는지 확인하는 데 사용됩니다.
	t->original_priority = priority;
	list_init(&t->donations);
	
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current ()->status = status;
	schedule ();
}
/*schedule() 스케줄러는 실행할 스레드를 결정하고, 해당 스레드로 컨텍스트를 전환합니다. */
static void
schedule (void) {
	struct thread *curr = running_thread ();/*현재 실행 중인 스레드를 가져온다.*/
	struct thread *next = next_thread_to_run ();/*다음에 실행할 스레드를 결정한다.*/

	ASSERT (intr_get_level () == INTR_OFF);/*이후 인터럽트가 꺼져 있는지 (INTR_OFF) */
	ASSERT (curr->status != THREAD_RUNNING);/* 현재 스레드가 실행 중인 상태가 아닌지*/
	ASSERT (is_thread (next));/*그리고 선택한 다음 스레드가 유효한지를 확인합니다.*/
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;
	

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif

	if (curr != next) {
		/*  스레드가 종료 상태(THREAD_DYING)인 경우,
		 그 스레드의 구조체를 파괴하도록 설계되어 있음을 설명합니다.
		 이 작업은 thread_exit() 함수가 자신이 필요로 하는 데이터를 파괴하지 않도록 하기 위해 나중에 수행됩니다.
		 이 함수는 Pintos의 멀티태스킹 기능을 관리하는 핵심 부분 중 하나입니다.
		 다른 스레드가 CPU를 사용할 수 있도록 현재 실행 중인 스레드를 중지하고, 새로운 스레드를 실행합니다. */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}

/*주어진 두 개의 list_elem 구조체를 비교하여 그들이 가리키는 thread 구조체의 wakeup_ticks 값을 비교합니다.
 wakeup_ticks 값이 작은 thread를 먼저 오도록 하여 정렬 순서를 결정합니다.*/
bool wakeup_tick_less_function(const struct list_elem *a, const struct list_elem *b,
                               void *aux UNUSED)
{
    struct thread *thread_a = list_entry(a, struct thread, elem);
    struct thread *thread_b = list_entry(b, struct thread, elem);
    return thread_a->wakeup_ticks < thread_b->wakeup_ticks;
}



/*스레드를 잠들게 하는 역할을 수행한다.*/
void thread_sleep(int64_t ticks)
{
	
    struct thread *curr = thread_current();/*함수를 사용하여 현재 실행 중인 스레드를 가져온다.*/
    enum intr_level old_level;
    ASSERT(!intr_context());/*intr_context함수를 사용하여 현재 스레드가 인터럽트 컨텍스에서 실행 중인지 확인하다. 인터럽트 컨텍스트에서 thread_sleep함수를 호출하는 것을 방지하기 위해 사용한다.*/
    old_level = intr_disable(); /* 인터럽트 비활성화*/
    if (curr != idle_thread)    /*만약 스레드가 유휴스레드가 아닌 경우 why? 유휴 스레드는 대기 상태에 들어가지 않으며 항상 실행 가능한 상태로 유지한다. */
    {
		if (ticks < global_tick)
			global_tick =ticks;/*ticks값이 global_tick보다 작을 경우, global_tick값을 ticks로 업데이트 한다. 스레드가 일시적으로 블록된 상태에서 특정 시간이 지났을 때 재게될수 있도록한다.*/
        /*localtick을 wakeup_tick에 저장해준다. why? wakeup_ticks 변수에 ticks값을 저장한다. 이는 스레드가 얼마나 오랫동안 잠들어 있어야 하는 지를 나타낸다.*/
        curr->wakeup_ticks = ticks;
        /*list_insert_ordered함수를 통해서 "tick" 값에 따라 sleep_list에 스레드를 작은 값부터 큰 값의 순서대로 삽입한다.*/
        list_insert_ordered(&sleep_list, &curr->elem, wakeup_tick_less_function, NULL);
        // do_schedule(THREAD_BLOCKED);
		thread_block(); /*현재 스레드를 블록 상태로 전환한다. 이로써 스레드는 실행 가능한 상태에서 제외되고, 일정 시간이 지나면 다시 스케줄링되어 실행될 수 있다.*/
		
	}
    intr_set_level(old_level); /*해당 함수를 사용하여 이전의 인터럽트 상태를 복원한다. 이전에 비활성화 된 인터럽트를 다시 활성화 한다.*/
}


/*잠들어 있는 스레드를 깨우는 역할을 수행한다.*/
void thread_wakeup(void)
{
	struct thread *sleep_thread;/*sleep_thread라는 구조체 포인터를 선언한다. 이변수는 잠들어 있는 스레드를 가리킬 것이다.*/

	if (!list_empty(&sleep_list)) /*sleep_list가 비어있는 지 확인한다. 만약 비어있는 상태가 아니라면*/
	{
		sleep_thread = list_entry(list_pop_front(&sleep_list), struct thread, elem); /*list_pop_front 함수를 사용하여 sleep_list에서 맨 앞의 스레드를 가져온다. 이는 잠들어 있던 스레드 중 가장 일찍 깨어나야 하는 스레드를 의미한다.*/
		
		if (!list_empty(&sleep_list)) /*다른 스레드들이 여전히 잠들어 있는 확인한다.*/
			global_tick = list_entry(list_begin(&sleep_list), struct thread, elem)->wakeup_ticks; /* global_tick값을 다음으로 일찍 깨어나야 하는 스레드의 wakeup_ticks값으로 설정한다. 이는 sleep_list의 첫 번째 원소가 가장 일찍 깨어나야 하는 스레드이기 때문이다.*/
		else
			global_tick = global_tick = 0x7FFFFFFFFFFFFFFF; /*잠들어있는 스레드가 없는 경우 global_tick값을 매우 큰 값으로 설정한다.why?다음으로 깨어나야할 스레드가 없음을 의미한다.*/
		list_insert_ordered(&ready_list, &sleep_thread->elem, cmp_priority, NULL); /*ready_list에 깨어난 스레드를 우선순위로 삽입한다. ready_list는 실행가능한 스레드들을 저장하는 리스트이다.*/
		sleep_thread->status = THREAD_READY; /*깨어난 스레드의 상태를 Thread_ready로 설정한다.*/	
			
	
	};
	
		
}

/* 비교 함수 cmp_priority는 두 개의 리스트 원소를 받아와 그들의 우선 순위를 비교합니다.

인자:
- a: 첫 번째 리스트 원소 (struct list_elem 타입)
- b: 두 번째 리스트 원소 (struct list_elem 타입)
- aux: 사용되지 않는 매개변수 (void 포인터)

반환값:
- true: 첫 번째 스레드의 우선 순위가 두 번째 스레드의 우선 순위보다 높음
- false: 첫 번째 스레드의 우선 순위가 두 번째 스레드의 우선 순위보다 낮거나 같음*/
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
	struct thread *thread_a = list_entry(a, struct thread, elem);/*list_entry 함수를 사용하여 첫 번째 리스트 원소(a)를 struct thread 타입으로 변환합니다.*/
	struct thread *thread_b = list_entry(b, struct thread, elem);/*list_entry 함수를 사용하여 두 번째 리스트 원소(b)를 struct thread 타입으로 변환합니다.*/
	
	return thread_a->priority > thread_b->priority;	/*첫 번째 스레드(thread_a)의 우선 순위가 두 번째 스레드(thread_b)의 우선 순위보다 높으면 true를 반환합니다.*/
	
};

void
donate_priority(void) {
	int depth;
	depth = 0;
	struct thread *temp;

	while (temp->waiting_lock != NULL && depth < 8)
	{
		depth++;
		if (temp->waiting_lock->holder == NULL)
			break;
		if (temp->waiting_lock->holder->priority >= temp->priority)
			break;	
		temp->waiting_lock->holder->priority = temp->priority;
		temp = temp->waiting_lock->holder;
	}
}



