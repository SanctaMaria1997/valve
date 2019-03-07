/*

BSD 2-Clause License

Copyright (c) 2019, SanctaMaria1997
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef LIBVALVE_H
#define LIBVALVE_H

#include <sys/tree.h>

typedef RB_HEAD(AllocationPointTree,AllocationPoint) AllocationPointTree_t;
typedef RB_HEAD(MemoryBlockTree,MemoryBlock) MemoryBlockTree_t;

typedef struct AllocationPoint AllocationPoint;

struct AllocationPoint
{
  long int address;
  unsigned long int current_num_allocations;
  unsigned long int current_bytes_allocated;
  unsigned long int total_num_allocations;
  unsigned long int total_bytes_allocated;
  MemoryBlockTree_t memory_blocks;
  RB_ENTRY(AllocationPoint) AllocationPointLinks;
};

int compare_allocation_points(AllocationPoint *a1,AllocationPoint *a2);

RB_PROTOTYPE(AllocationPointTree,AllocationPoint,AllocationPointLinks,compare_allocation_points);

typedef struct MemoryBlock MemoryBlock;

struct MemoryBlock
{
  unsigned long int address;
  size_t size;
  AllocationPoint *allocation_point;
  RB_ENTRY(MemoryBlock) MemoryBlockLinks;
};

int compare_memory_blocks(MemoryBlock *mb1,MemoryBlock *mb2);

RB_PROTOTYPE(MemoryBlockTree,MemoryBlock,MemoryBlockLinks,compare_memory_blocks);

#endif
