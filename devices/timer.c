#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */

static int64_t ticks;


/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);\
// static int64_t global_ticks = 0 ;

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. */
void
timer_init (void) {
	/* 8254 input frequency divided by TIMER_FREQ, rounded to
	   nearest. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb (0x43, 0x34);    /* CW: counter 0, LSB then MSB, mode 2, binary. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);

	intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/*OS가 부팅된 이후로 경과한 타이머 틱(tick) 수를 반환합니다. */
/*이 함수는 운영 체제가 부팅된 이후 경과한 타이머 틱의 수를 계산하여 반환합니다.
 타이머 틱은 일반적으로 컴퓨터의 하드웨어 타이머에 의해 생성되는 고정된 간격의 시간 단위를 나타냅니다.
 이 값은 시스템이 부팅된 이후의 경과 시간을 추적하는 데 사용될 수 있습니다.*/
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level);
	barrier ();
	return t;
}

/* timer_ticks() 함수는 THEN 이후로 경과한 타이머 틱(tick)의 수를 반환합니다. 여기서 THEN은 이전에 timer_ticks() 함수를 호출하여 얻은 값입니다.*/
/* timer_ticks() 함수는 프로그램이 실행된 이후 경과한 타이머 틱의 수를 반환하여 경과한 시간을 추적하는 데 사용될 수 있습니다.*/
int64_t
timer_elapsed (int64_t then) {
	return timer_ticks () - then;
}

/* Ticks 타이머 틱에 대해 실행을 일시 중단한다. "TICKS"는 실행이 중단되는 시간을 나타내는 값입니다. 의미는 주어진 시간(TICKS) 동안 프로그램 실행을 중지하고 대기하는 것을 의미*/
void
timer_sleep (int64_t ticks) { /*timer_ticks함수를 호출하여 현재 시간을 저장한다.*/
	int64_t start = timer_ticks ();
	
	ASSERT (intr_get_level () == INTR_ON);/*특정 지점에서 인터럽트가 활성화된 상태임을 검사한다. 이 조건은 보통 인터럽트 처리에 관련된 코드에서 사용되며, 인터럽트가 활성화된 상태에서 실행되어야 하는 것을 보장하기 위해 사용된다.*/
	if(timer_elapsed (start) < ticks) /*timer_elapsed(start) 함수를 호출하여 start 시간부터 현재까지 경과한 시간을 계산합니다. 이는 타이머의 경과 시간을 가져오는 함수입니다.해당 값을 global tick값과 비교한다.*/
		thread_sleep(start + ticks);/*thread_sleep(start + ticks)는 "현재 시간(start)으로부터 특정 시간(ticks)이 지난 후에 스레드를 깨우십시오"라는 명령을 나타냅니다. 즉, 스레드는 start + ticks시점에 깨어나게 됩니다.*/
}//start + ticks 시점에 도달하면, 스레드는 깨어나게 되며(thread_wakeup), 이 스레드는 다시 실행 가능한 상태가 됩니다.

/* Suspends execution for approximately MS milliseconds. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */


/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) {
	/* Wait for a timer tick. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* Run LOOPS loops. */
	start = ticks;
	busy_wait (loops);

	/* If the tick count changed, we iterated too long. */
	barrier ();
	return start != ticks;
}



/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}



/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes. */
		timer_sleep (ticks);
	} else {
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}

/*타이머 인터럽트가 발생했을 때  호출되는 함수이다. 운영체제의 스케줄링과 관련된 작업을 수행한다.*/
static void timer_interrupt (struct intr_frame *args UNUSED)
{
	ticks++;
	thread_tick ();
	while (global_tick <= ticks)
		thread_wakeup();
	// if (thread_mlfqs == true)
	// {
	// 	thread_current()->recent_cpu += (1 << 14);

	// 	if (ticks % TIMER_FREQ == 0)
	// 	{
	// 		calculate_load_avg();
	// 		calculate_recent_cpu();
	// 	}
	// 	if (ticks % 4 == 0)
	// 	{
	// 		recalculate_priority();
	// 	}
	// }	

}
   