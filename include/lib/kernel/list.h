#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

/* "Doubly linked list": 양방향 연결 리스트 구현을 위한 헤더 파일이며, 동적 할당된 메모리를 필요로 하지 않습니다.
 대신 각 구조체는 'struct list_elem' 멤버를 포함해야 합니다. 모든 리스트 함수는 이 'struct list_elem'을 사용하여 작동합니다.
 'list_entry' 매크로를 사용하여 'struct list_elem'을 포함한 구조체 객체로의 변환을 수행할 수 있습니다.

"List traversal": 리스트의 반복에 사용되는 함수들이 설명되어 있습니다.
 리스트의 시작점, 다음 요소, 끝점 등을 반환하는 함수들이 포함됩니다.

"List insertion": 리스트에 요소를 삽입하는 함수들이 설명되어 있습니다.
 요소를 특정 위치에 삽입하거나, 앞쪽이나 뒤쪽에 삽입하는 함수들이 포함됩니다.

"List removal": 리스트에서 요소를 제거하는 함수들이 설명되어 있습니다.
 특정 요소를 제거하거나, 리스트의 맨 앞쪽이나 맨 뒤쪽에서 요소를 제거하는 함수들이 포함됩니다.

"List elements": 리스트의 처음과 끝에 위치한 요소를 반환하는 함수들이 설명되어 있습니다.

"List properties": 리스트의 크기와 비어 있는지 여부를 확인하는 함수들이 설명되어 있습니다.

"Miscellaneous": 리스트를 뒤집는 함수가 포함되어 있습니다.

"Operations on lists with ordered elements": 정렬된 요소를 가진 리스트에 대한 동작을 수행하는 함수들이 설명되어 있습니다.
 리스트를 정렬하거나, 정렬된 위치에 요소를 삽입하거나, 중복 요소를 제거하는 함수들이 포함됩니다.

"Max and min": 최댓값과 최솟값을 반환하는 함수들이 포함되어 있습니다.

해당 헤더 파일은 양방향 연결 리스트의 구현을 제공하며, 리스트 관련 동작을 수행하기 위한 다양한 함수들을 포함하고 있습니다.*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* List element. */
struct list_elem {
	struct list_elem *prev;     /* Previous list element. */
	struct list_elem *next;     /* Next list element. */
};

/* List. */
struct list {
	struct list_elem head;      /* List head. */
	struct list_elem tail;      /* List tail. */
};

/* Converts pointer to list element LIST_ELEM into a pointer to
   the structure that LIST_ELEM is embedded inside.  Supply the
   name of the outer structure STRUCT and the member name MEMBER
   of the list element.  See the big comment at the top of the
   file for an example. */
#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
	((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
		- offsetof (STRUCT, MEMBER.next)))

void list_init (struct list *);

/* List traversal. */
struct list_elem *list_begin (struct list *);
struct list_elem *list_next (struct list_elem *);
struct list_elem *list_end (struct list *);

struct list_elem *list_rbegin (struct list *);
struct list_elem *list_prev (struct list_elem *);
struct list_elem *list_rend (struct list *);

struct list_elem *list_head (struct list *);
struct list_elem *list_tail (struct list *);

/* List insertion. */
void list_insert (struct list_elem *, struct list_elem *);
void list_splice (struct list_elem *before,
		struct list_elem *first, struct list_elem *last);
void list_push_front (struct list *, struct list_elem *);
void list_push_back (struct list *, struct list_elem *);

/* List removal. */
struct list_elem *list_remove (struct list_elem *);
struct list_elem *list_pop_front (struct list *);
struct list_elem *list_pop_back (struct list *);

/* List elements. */
struct list_elem *list_front (struct list *);
struct list_elem *list_back (struct list *);

/* List properties. */
size_t list_size (struct list *);
bool list_empty (struct list *);

/* Miscellaneous. */
void list_reverse (struct list *);

/* Compares the value of two list elements A and B, given
   auxiliary data AUX.  Returns true if A is less than B, or
   false if A is greater than or equal to B. */
typedef bool list_less_func (const struct list_elem *a,
                             const struct list_elem *b,
                             void *aux);

/* Operations on lists with ordered elements. */
void list_sort (struct list *,
                list_less_func *, void *aux);
void list_insert_ordered (struct list *, struct list_elem *,
                          list_less_func *, void *aux);
void list_unique (struct list *, struct list *duplicates,
                  list_less_func *, void *aux);

/* Max and min. */
struct list_elem *list_max (struct list *, list_less_func *, void *aux);
struct list_elem *list_min (struct list *, list_less_func *, void *aux);

#endif /* lib/kernel/list.h */
