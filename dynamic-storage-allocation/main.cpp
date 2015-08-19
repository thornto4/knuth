#include <iostream>

#include "BuddyAllocator.h"

int main()
{
    BuddyAllocator buddy(8u); // 2^8 byte buffer
    buddy.alloc(4);
    buddy.print();
    return 0;
}

