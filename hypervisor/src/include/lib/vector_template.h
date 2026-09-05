#ifndef _HYVEMIND_LIB_VECTOR_TEMPLATE_H
#define _HYVEMIND_LIB_VECTOR_TEMPLATE_H

#include "fatal.h"
#include "halloc.h"
#include "hyvstdlib.h"

#define DEFAULT_VEC_INIT_CAPACITY 512

#define VECTOR_STRUCT_NAME CONCAT(PREFIX, vector)
#define VECTOR_STRUCT struct VECTOR_STRUCT_NAME
#define VECTOR_FUNC(name) CONCAT(name, CONCAT(PREFIX, vector))

#endif /* _HYVEMIND_LIB_VECTOR_TEMPLATE_H */

#ifndef T
#error Type parameter for vector is not specified
#endif

#ifndef PREFIX
#error Vector prefix is not specified
#endif

#ifdef VECTOR_DEFINITIONS

VECTOR_STRUCT {
    T *elements;
    unsigned int capacity;
    unsigned int insertion_idx;
};

VECTOR_STRUCT * VECTOR_FUNC(create)(const unsigned int init_capacity);
void VECTOR_FUNC(destroy)(const VECTOR_STRUCT *vector);

int VECTOR_FUNC(size)(const VECTOR_STRUCT *vector);
bool VECTOR_FUNC(empty)(const VECTOR_STRUCT *vector);

bool VECTOR_FUNC(at)(const VECTOR_STRUCT *vector, T *element, const unsigned int pos);
T VECTOR_FUNC(front)(const VECTOR_STRUCT *vector);
T VECTOR_FUNC(back)(const VECTOR_STRUCT *vector);

void VECTOR_FUNC(push_back)(VECTOR_STRUCT *vector, const T element);
void VECTOR_FUNC(pop_back)(VECTOR_STRUCT *vector);
bool VECTOR_FUNC(insert)(
        VECTOR_STRUCT *vector,
        const T element,
        const unsigned int pos
);
bool VECTOR_FUNC(erase)(VECTOR_STRUCT *vector, const unsigned int pos);
bool VECTOR_FUNC(swap)(
        VECTOR_STRUCT *vector,
        const unsigned int pos1,
        const unsigned int pos2
);

#undef VECTOR_DEFINITIONS
#endif /* VECTOR_DEFINITIONS */

#ifdef VECTOR_IMPLEMENTATION

VECTOR_STRUCT *
VECTOR_FUNC(create)(const unsigned int init_capacity)
{
    VECTOR_STRUCT *vector = (VECTOR_STRUCT *) hmalloc(sizeof(VECTOR_STRUCT));
    if (!vector) {
        pr_error("Allocating vector struct '%s' failed", TO_STR(VECTOR_STRUCT_NAME));
        return NULL;
    }

    vector->elements = (T*) hmalloc(sizeof(T) * init_capacity);
    if (!vector->elements) {
        pr_error("Allocating %s's backing array failed", TO_STR(VECTOR_STRUCT_NAME));
        hfree(vector);
        return NULL;
    }

    vector->capacity = init_capacity;
    vector->insertion_idx = 0;

    return vector;
}

void
VECTOR_FUNC(destroy)(const VECTOR_STRUCT *vector)
{
    if (!vector) {
        return;
    }

    if (vector->elements) {
        hfree(vector->elements);
    }

    hfree(vector);
}

int
VECTOR_FUNC(size)(const VECTOR_STRUCT *vector)
{
    return vector->insertion_idx;
}

bool
VECTOR_FUNC(empty)(const VECTOR_STRUCT *vector)
{
    return VECTOR_FUNC(size)(vector) == 0;
}

bool
VECTOR_FUNC(at)(const VECTOR_STRUCT *vector, T *element, const unsigned int pos)
{
    if (pos >= vector->insertion_idx) {
        return false;
    }

    *element = vector->elements[pos];
    return true;
}

/* As in C++, the precondition is that !vector.empty() */
T
VECTOR_FUNC(front)(const VECTOR_STRUCT *vector)
{
    return vector->elements[0];
}

/* As in C++, the precondition is that !vector.empty() */
T
VECTOR_FUNC(back)(const VECTOR_STRUCT *vector)
{
    return  vector->elements[vector->insertion_idx-1];
}

void
VECTOR_FUNC(push_back)(VECTOR_STRUCT *vector, const T element)
{
    if (vector->insertion_idx >= vector->capacity) {
        die_reason("todo: realloc");
    }

    vector->elements[vector->insertion_idx] = element;
    vector->insertion_idx++;
}

void
VECTOR_FUNC(pop_back)(VECTOR_STRUCT *vector)
{
    if (VECTOR_FUNC(empty)(vector)) {
        return;
    }

    vector->insertion_idx--;
}

bool
VECTOR_FUNC(insert)(VECTOR_STRUCT *vector, const T element, const unsigned int pos)
{
    NOT_YET_IMPLEMENTED;
}

bool
VECTOR_FUNC(erase)(VECTOR_STRUCT *vector, const unsigned int pos)
{
    NOT_YET_IMPLEMENTED;
}

bool
VECTOR_FUNC(swap)(
        VECTOR_STRUCT *vector,
        const unsigned int pos1,
        const unsigned int pos2
) {
    NOT_YET_IMPLEMENTED;
}

#undef VECTOR_IMPLEMENTATION
#endif /* VECTOR_IMPLEMENTATION */

#undef T
#undef PREFIX
