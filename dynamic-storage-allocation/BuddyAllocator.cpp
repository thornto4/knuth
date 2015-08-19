#include "BuddyAllocator.h"

#include <iostream>
#include <string>
#include <string.h>

/*
 * MemoryBlock is a doubly linked list representing available free memory regions of size 2^k. This allows for quick traversal of the
 * available free memory blocks by the buddy allocator.
 * */
struct MemoryBlock
{
    MemoryBlock *prev, *next;
    union {
        struct {
            bool available;
            uint8_t k;
        };
        struct {} list;
    };
};

/*
 * The buddy system relies on the following:
 * - blocks of size 2^k such that 0 <= k <= m
 * - address range from [0, 2^m -1]
 * - blocks are allocated by splitting larger blocks in half
 * - blocks are reclaimed by coalescing two contiguous buddies of equal size back into the larger block it was initially split from
 * */

BuddyAllocator::BuddyAllocator(uint16_t m) :
    m_order(m),
    m_blocks(nullptr)
{
    if (m > 8) {
        throw "Insufficient Memory";
    }

    // create buffer of size 2^m
    m_buff = new char[1 << m];
    memset(m_buff, 0, 1 << m);

    // create lists to store blocks of size 1, 2, 4, 8, ..., 2^m
    m_blocks = new MemoryBlock[m+1];

    // note, MemoryBlock serves both as
    // m_blocks[k].prev --> points to the front element
    // m_blocks[k].next --> points to the rear element

    // initialize lists
    for (int k = 3; k < m; ++k ) {
        m_blocks[k].next = m_blocks[k].prev = &m_blocks[k];
    }

    m_blocks[m].next = m_blocks[m].prev = (MemoryBlock*)(m_buff);
    MemoryBlock *block = (MemoryBlock*)(m_buff);
    block->next = block->prev = &m_blocks[m];
    block->available = 1;
    block->k = m;

    /// \attention I'm being lazy by creating but ignoring lists for blocks 2^0, 2^1, and 2^2.
    /// The smallest block size we support is 8 bytes. Keeping the extra lists around means I can
    /// map block size 2^8 to m_blocks[8]. i thought this was preferable to saying m_blocks[8-3].
}

BuddyAllocator::~BuddyAllocator()
{
    delete [] m_blocks;
    delete [] m_buff;
}

namespace
{
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
    uint16_t nextPowerOfTwo(uint16_t input) {

        uint32_t v = input;

        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v++;

        return v;
    }
}
char *BuddyAllocator::alloc(uint16_t bytes)
{
    if (bytes <= 0) {
        throw "Har har har";
    }

    if (bytes >= 256) {
        throw "Insufficient Memory";
    }

    bytes += 1; // must account for our boolean flag

    uint16_t blockSize = nextPowerOfTwo(bytes);
    uint16_t k = 0;
    while(blockSize >>= 1) {
        ++k;
    }

    for (uint8_t j = k; j <= m_order; ++j) {
        // find smallest available block that sufficient for the request
        if (m_blocks[j].next != &m_blocks[j]) {
            // remove free block and update list
            MemoryBlock *freeBlock = m_blocks[j].next;
            MemoryBlock *nextBlock = freeBlock->next;
            m_blocks[j].next = nextBlock;
            nextBlock->prev = &m_blocks[j];

            // do we need to split blocks ?
            while (j != k) {
                --j;

                // if so, split the block and enter the unused half in the available list
                nextBlock = (MemoryBlock*)((uint32_t)freeBlock ^ (1 << j));
                nextBlock->available = 1;
                nextBlock->k = j;
                nextBlock->next = nextBlock->prev = &m_blocks[j];
                m_blocks[j].next = m_blocks[j].prev = nextBlock;
            }

            // we've found and reserved our block
            char *address = toUserSpace(freeBlock);
            used[address] = freeBlock->k;
            return address;
        }
    }

    // there are no known available blocks of sufficient size to meet the request

    throw "Insufficient Memory!";
}

void BuddyAllocator::free(char *address)
{
    std::cout << "deallocating " << address << std::endl;
}

void BuddyAllocator::print()
{
    for (auto iter = used.begin(); iter != used.end(); ++iter) {
        std::cout << "{ 0x" << (uint32_t)iter->first << ", " << (1 << iter->second) << " }" << std::endl;
    }

    for (int i = 3; i <= m_order; ++i) {
        for( MemoryBlock *freeBlock = m_blocks[i].prev; freeBlock != &m_blocks[i]; freeBlock = freeBlock->next ) {
            std::cout << "|  " << (1 << freeBlock->k) << "  |";
        }
    }
    std::cout << std::endl;
}

char *BuddyAllocator::toUserSpace(MemoryBlock *block)
{
    return ((char*)block) + 1;
}

MemoryBlock *BuddyAllocator::fromUserSpace(char *address)
{
    return (MemoryBlock*)(address - 1);
}
