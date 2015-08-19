#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdint.h>

class Allocator
{
public:
    virtual char *alloc(uint16_t bytes) = 0;
    virtual void free(char *address) = 0;
    virtual void print() = 0;
};

#endif // ALLOCATOR_H
