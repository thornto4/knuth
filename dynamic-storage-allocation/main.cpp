#include <iostream>

#include "BuddyAllocator.h"

#include <string.h>

int main()
{
    BuddyAllocator buddy(8u); // 2^8 byte buffer
    buddy.showDetails(true);

    char *foo = buddy.alloc(11);
    strcpy(foo, "HelloWorld");
    buddy.print();
    buddy.free(foo);
    buddy.print();
    return 0;
}

