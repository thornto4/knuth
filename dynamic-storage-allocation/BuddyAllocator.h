#ifndef BUDDYALLOCATOR
#define BUDDYALLOCATOR

#include <stdint.h>
#include <memory>
#include <map>

#include "Allocator.h"

/*
 * Buddy Allocator manages a contiguous block of 2^m bytes. Implements Knuth's "buddy system" in order to manage block allocation and deallocation.
 * See Knuth's The Art of Programming, Vol 1 for more details.
 * */

struct MemoryBlock;

class BuddyAllocator : public Allocator
{
public:
    BuddyAllocator(uint16_t m);
    ~BuddyAllocator();

    char *alloc(uint16_t bytes) override;
    void free(char *address) override;
    void print() override;

    void showDetails(bool show) { m_details = show; }

private:
    char *toUserSpace(MemoryBlock *);
    MemoryBlock *fromUserSpace(char *);

    uint16_t m_order;
    MemoryBlock *m_blocks;
    char * m_buff;

    std::map<MemoryBlock*, uint8_t> used;
    bool m_details;
};

#endif // BUDDYALLOCATOR

