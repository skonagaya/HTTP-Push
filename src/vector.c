#include <stdio.h>
#include <stdlib.h>

#include "vector.h"

static int strCmp(const char* s1, const char* s2)
{
    while(*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void vector_init(vector *v)
{
    v->capacity = VECTOR_INIT_CAPACITY;
    v->total = 0;
    v->items = malloc(sizeof(void *) * v->capacity);
}

int vector_total(vector *v)
{
    return v->total;
}

void vector_resize(vector *v, int capacity)
{
    #ifdef DEBUG_ON
    printf("vector_resize: %d to %d\n", v->capacity, capacity);
    #endif

    void **items = realloc(v->items, sizeof(void *) * capacity);
    if (items) {
        v->items = items;
        v->capacity = capacity;
    }
}

void vector_add(vector *v, void *item)
{
    if (vector_contains(v,item))
        return;
    if (v->capacity == v->total)
        vector_resize(v, v->capacity * 2);
    v->items[v->total++] = item;
}

int vector_contains(vector *v, void *item)
{
    for (int i = 0; i < v->total ; i++ ) {
        if (strCmp(v->items[i],item)==0)
             {
                return 1;
             }
    }
    return 0;
}

void vector_remove(vector *v, void *item)
{
    for (int i = 0; i < v->total ; i++ ) {
        if (strCmp(v->items[i],item)==0)
             {
                vector_delete(v, i);
             }
    }

}
void vector_clear(vector *v)
{
    for (int i = 0; i < v->total ; i++ ) {
        vector_delete(v, i);
    }
}

void vector_set(vector *v, int index, void *item)
{
    if (index >= 0 && index < v->total)
        v->items[index] = item;
}

void *vector_get(vector *v, int index)
{
    if (index >= 0 && index < v->total)
        return v->items[index];
    return NULL;
}

void vector_delete(vector *v, int index)
{
    if (index < 0 || index >= v->total)
        return;

    free( v->items[index]);
    v->items[index] = NULL;

    for (int i = index; i < v->total - 1; i++) {
        v->items[i] = v->items[i + 1];
        v->items[i + 1] = NULL;
    }

    v->total--;

    if (v->total > 0 && v->total == v->capacity / 4)
        vector_resize(v, v->capacity / 2);
}


void vector_free(vector *v)
{
    vector_clear(v);
    free(v->items);
}