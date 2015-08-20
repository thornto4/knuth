#include "BuddyAllocator.h"

#include <iostream>
#include <string>
#include <string.h>
#include <stdlib.h>

/*
 * MemoryBlock is a doubly linked list representing available free memory regions of size 2^k. This allows for quick traversal of the
 * available free memory blocks by the buddy allocator.
 * */
struct MemoryBlock
{
    union {
        struct {
            bool available;
            uint8_t k;
        };
        struct {} list;
    };
    MemoryBlock *prev, *next;
};

std::ostream &operator<<(std::ostream &out, const MemoryBlock &block ) {
    if (block.available) {
        out << "MemoryBlock( " << &block << ", " << (1 << block.k) << " )";
    } else {
        out << "MemoryBlock( " << &block << ", RESERVED )";
    }
    return out;
}

/*
 * The buddy system relies on the following:
 * - blocks of size 2^k such that 0 <= k <= m
 * - address range from [0, 2^m -1]
 * - blocks are allocated by splitting larger blocks in half
 * - blocks are reclaimed by coalescing two contiguous buddies of equal size back into the larger block it was initially split from
 * */

BuddyAllocator::BuddyAllocator(uint16_t m) :
    m_order(m),
    m_blocks(nullptr),
    m_buff(nullptr),
    m_details(false)
{
    if (m > 8) {
        throw "Insufficient Memory";
    }

    // create buffer of size 2^m
    m_buff = (char*) _mm_malloc(1 << m, 1 << m);       // allign our buffer on byte alignments the width of the max block ... should make debugging easier.
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
    _mm_free( m_buff );
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
    if (m_details) {
        std::cout << "*** Allocating " << bytes << " bytes" << std::endl;
    }

    if (bytes <= 0) {
        throw "Har har har";
    }

    if (bytes >= 256) {
        throw "Insufficient Memory";
    }

    bytes += 1; // must account for our boolean flag

    // find our block size and its index
    uint16_t blockSize = nextPowerOfTwo(bytes);
    uint16_t k = 0;
    while(blockSize >>= 1) {
        ++k;
    }

    if (m_details) {
        std::cout << "   Searching for free block of size " << (1 << k) << std::endl;
    }

    for (uint8_t j = k; j <= m_order; ++j) {
        // find smallest available block that sufficient for the request
        if (m_blocks[j].next != &m_blocks[j]) {
            // remove free block and update list
            MemoryBlock *freeBlock = m_blocks[j].next;
            MemoryBlock *nextBlock = freeBlock->next;
            m_blocks[j].next = nextBlock;
            nextBlock->prev = &m_blocks[j];
            freeBlock->available  = 0;

            if (m_details) {
                std::cout << "   Found available block: " <<  *freeBlock << std::endl;
            }

            // do we need to split blocks ?
            while (j != k) {
                --j;

                // if so, split the block and enter the unused half in the available list
                nextBlock = getBuddy(freeBlock, j);
                nextBlock->available = 1;
                nextBlock->k = j;
                nextBlock->next = nextBlock->prev = &m_blocks[j];
                m_blocks[j].next = m_blocks[j].prev = nextBlock;

                if (m_details) {
                    std::cout << "      Split required - Creating smaller block: " << *nextBlock << std::endl;
                }
            }

            // we've found and reserved our block
            char *address = toUserSpace(freeBlock);
            used[freeBlock] = j;

            if (m_details) {
                std::cout << "   Allocation Success - Returning available block: " << *freeBlock << std::endl << std::endl;
            }

            return address;
        }
    }

    // there are no known available blocks of sufficient size to meet the request

    if (m_details) {
        std::cout << "No blocks of size >=" << (1<<k) << "available. Allocation failed." << std::endl;
    }

    throw "Insufficient Memory!";
}

void BuddyAllocator::free(char *address)
{
    if (m_details) {
        std::cout << "*** Freeing memory at address: 0x" << (void*)address << std::endl;
    }
    // remove block from used list
    MemoryBlock *block = fromUserSpace(address);
    uint8_t k = used[block];
    used.erase(block);

    if (m_details) {
        std::cout << "   Located at block: " << *block << std::endl;
    }

    // is buddy available?
    MemoryBlock *buddy = getBuddy(block, k);

    // combine buddy if:
    // 1. we're not at the last block
    // 2. our buddy is available
    // 3. our buddy is also the same size we are
    while (k != m_order && buddy->available == 1 && buddy->k == k) {

        if (m_details) {
            std::cout << "      Coalescing - Reclaiming additional block: " << *buddy << std::endl;
        }

        // remove buddy from our free list
        // 1. point the node behind me, to whatever is in front of me
        // 2. point the node in front of me, to whatever is behind me
        buddy->prev->next = buddy->next;
        buddy->next->prev = buddy->prev;

        // bump up block level
        ++k;

        if (buddy < block) {
            block = buddy;
        }

        buddy = getBuddy(block, k);
    }

    // add newly reclaimed block to the available list
    block->available = 1;
    block->k = k;

    MemoryBlock *nextBlock = m_blocks[k].next;
    block->next = nextBlock;
    nextBlock->prev = block;

    block->prev = &m_blocks[k];
    m_blocks[k].next = block;

    if (m_details) {
        std::cout << "   Free Success - New block available: " << *block << std::endl << std::endl;
    }

    // a list of size n looks like the following:
    // [ Head ] --> [ rear ]<-->[ n - 1]<-- ... -->[ 2 ] [ front ]
    // [ Head ] --> [ newBlock ]<-->[ n ]<-->[ n - 1]<-- ... -->[ 2 ] [ front ]
}

char *BuddyAllocator::toUserSpace(MemoryBlock *block)
{
    return ((char*)block) + 1;
}

MemoryBlock *BuddyAllocator::fromUserSpace(char *address)
{
    return (MemoryBlock*)(address - 1);
}

MemoryBlock *BuddyAllocator::getBuddy(MemoryBlock *block, uint8_t k)
{
    return (MemoryBlock*)((uintptr_t)block ^ (1<<k));
}

void BuddyAllocator::print()
{

    std::cout << "========= Used Memory =======" << std::endl << std::endl;
    for (auto && pair : used) {
        std::cout << "{ " << *(pair.first) << " , Data(" << toUserSpace(pair.first) << ") }" << std::endl;
    }
    std::cout << std::endl;

    // print available

    // +-----------------------+
    // | / / / / 16384 / / / / |
    // +-----------------------+

    // +-----------------------+
    // |         16384         |
    // +-----------------------+


    std::string leftSegment = "---------";
    std::string rightSegment= "--------+";
    std::string leftUnused  = "         ";
    std::string rightUnused = "        |";

    std::string top, middle, bottom;

    top = bottom = "+";
    middle = "|";

    for (int i = 3; i <= m_order; ++i) {
        for (MemoryBlock *block = m_blocks[i].prev; block != &m_blocks[i]; block = block->next) {
            // print available block
            std::string blockSize = std::to_string(1 << block->k);
            std::string segment;
            segment.append(blockSize.size(), '-');
            segment = leftSegment + segment + rightSegment;

            top += segment;
            middle += leftUnused + blockSize + rightUnused;
            bottom += segment;
        }
    }

    std::cout << "========= Available Memory =======" << std::endl;
    std::cout << std::endl;
    std::cout << top << std::endl;
    std::cout << middle << std::endl;
    std::cout << bottom << std::endl;
    std::cout << std::endl;
    std::cout << "============================" << std::endl << std::endl;
}
