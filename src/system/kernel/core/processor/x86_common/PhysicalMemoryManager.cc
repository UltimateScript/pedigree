/*
 * Copyright (c) 2008 James Molloy, James Pritchett, Jörg Pfähler, Matthew Iselin
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <Log.h>
#include <panic.h>
#include <utilities/utility.h>
#include "PhysicalMemoryManager.h"

#if defined(X86)
  #include "../x86/VirtualAddressSpace.h"
  #define KERNEL_VADDRESS 0xC0000000
#elif defined(X64)
  #include "../x64/VirtualAddressSpace.h"
  #define KERNEL_VADDRESS 0xFFFFFFFF7FF00000
#endif

X86CommonPhysicalMemoryManager X86CommonPhysicalMemoryManager::m_Instance;

PhysicalMemoryManager &PhysicalMemoryManager::instance()
{
  return X86CommonPhysicalMemoryManager::instance();
}

physical_uintptr_t X86CommonPhysicalMemoryManager::allocatePage()
{
  return m_PageStack.allocate(0);
}
void X86CommonPhysicalMemoryManager::freePage(physical_uintptr_t page)
{
  m_PageStack.free(page);
}
bool X86CommonPhysicalMemoryManager::allocateRegion(MemoryRegion &Region,
                                                    size_t count,
                                                    size_t pageConstraints,
                                                    physical_uintptr_t start)
{
  // TODO
  return false;
}

void X86CommonPhysicalMemoryManager::initialise(const BootstrapStruct_t &Info)
{
  NOTICE("memory-map:");

  // Fill the page-stack (usable memory above 16MB)
  // NOTE: We must do the page-stack first, because the range-lists already need the
  //       memory-management
  MemoryMapEntry_t *MemoryMap = reinterpret_cast<MemoryMapEntry_t*>(Info.mmap_addr);
  while (reinterpret_cast<uintptr_t>(MemoryMap) < (Info.mmap_addr + Info.mmap_length))
  {
    NOTICE(" " << Hex << MemoryMap->address << " - " << (MemoryMap->address + MemoryMap->length) << ", type: " << MemoryMap->type);

    if (MemoryMap->type == 1)
      for (uint64_t i = MemoryMap->address;i < (MemoryMap->length + MemoryMap->address);i += getPageSize())
        if (i >= 0x1000000)
          m_PageStack.free(i);

    MemoryMap = adjust_pointer(MemoryMap, MemoryMap->size + 4);
  }

  // Fill the range-lists (usable memory below 1/16MB)
  MemoryMap = reinterpret_cast<MemoryMapEntry_t*>(Info.mmap_addr);
  while (reinterpret_cast<uintptr_t>(MemoryMap) < (Info.mmap_addr + Info.mmap_length))
  {
    if (MemoryMap->type == 1)
    {
      if (MemoryMap->address < 0x100000)
      {
        // NOTE: Assumes that the entry/entries starting below 1MB don't cross the
        //       1MB barrier
        if ((MemoryMap->address + MemoryMap->length) >= 0x100000)
          panic("PhysicalMemoryManager: strange memory-map");

        m_RangeBelow1MB.free(MemoryMap->address, MemoryMap->length);
      }
      else if (MemoryMap->address < 0x1000000)
      {
        uint64_t upperBound = MemoryMap->address + MemoryMap->length;
        if (upperBound >= 0x1000000)upperBound = 0x1000000;

        m_RangeBelow16MB.free(MemoryMap->address, upperBound - MemoryMap->address);
      }
    }

    MemoryMap = adjust_pointer(MemoryMap, MemoryMap->size + 4);
  }

  // Remove the pages used by the kernel from the range-list (below 16MB)
  extern void *code;
  extern void *end;
  if (m_RangeBelow16MB.allocateSpecific(reinterpret_cast<uintptr_t>(&code) - KERNEL_VADDRESS,
                                        reinterpret_cast<uintptr_t>(&end) - reinterpret_cast<uintptr_t>(&code))
      == false)
  {
    panic("PhysicalMemoryManager: could not remove the kernel image from the range-list");
  }

  // Print the ranges
  NOTICE("free memory ranges (below 1MB):");
  for (size_t i = 0;i < m_RangeBelow1MB.size();i++)
  {
    NOTICE(" " << Hex << m_RangeBelow1MB.getRange(i).address << " - " << (m_RangeBelow1MB.getRange(i).address + m_RangeBelow1MB.getRange(i).length));
  }
  NOTICE("free memory ranges (below 16MB):");
  for (size_t i = 0;i < m_RangeBelow16MB.size();i++)
  {
    NOTICE(" " << Hex << m_RangeBelow16MB.getRange(i).address << " - " << (m_RangeBelow16MB.getRange(i).address + m_RangeBelow16MB.getRange(i).length));
  }

  // Initialise the free physical ranges 
  // TODO: Ranges above 4GB
  m_PhysicalRanges.free(0, 0x100000000ULL);
  MemoryMap = reinterpret_cast<MemoryMapEntry_t*>(Info.mmap_addr);
  while (reinterpret_cast<uintptr_t>(MemoryMap) < (Info.mmap_addr + Info.mmap_length))
  {
    if (m_PhysicalRanges.allocateSpecific(MemoryMap->address, MemoryMap->length) == false)
      panic("PhysicalMemoryManager: Failed to create the list of ranges of free physical space");

    MemoryMap = adjust_pointer(MemoryMap, MemoryMap->size + 4);
  }

  // Print the ranges
  NOTICE("physical memory ranges:");
  for (size_t i = 0;i < m_PhysicalRanges.size();i++)
  {
    NOTICE(" " << Hex << m_PhysicalRanges.getRange(i).address << " - " << (m_PhysicalRanges.getRange(i).address + m_PhysicalRanges.getRange(i).length));
  }
}

X86CommonPhysicalMemoryManager::X86CommonPhysicalMemoryManager()
  : m_PageStack(), m_RangeBelow1MB(), m_RangeBelow16MB(), m_PhysicalRanges()
{
}
X86CommonPhysicalMemoryManager::~X86CommonPhysicalMemoryManager()
{
}



physical_uintptr_t X86CommonPhysicalMemoryManager::PageStack::allocate(size_t constraints)
{
  size_t index = 0;
  #if defined(X64)
    if (constraints == X86CommonPhysicalMemoryManager::below4GB)
      index = 0;
    else if (constraints == X86CommonPhysicalMemoryManager::below64GB)
      index = 1;
    else
      index = 2;

    if (index == 2 && m_StackMax[2] == m_StackSize[2])
      index = 1;
    if (index == 1 && m_StackMax[1] == m_StackSize[1])
      index = 0;
  #endif

  physical_uintptr_t result = 0;
  if (m_StackMax[index] != m_StackSize[index])
  {
    if (index == 0)
    {
      m_StackSize[0] -= 4;
      result = *(reinterpret_cast<uint32_t*>(m_Stack[0]) + m_StackSize[0] / 4);
    }
    else
    {
      m_StackSize[index] -= 8;
      result = *(reinterpret_cast<uint64_t*>(m_Stack[index]) + m_StackSize[index] / 8);
    }
  }
  return result;
}
void X86CommonPhysicalMemoryManager::PageStack::free(uint64_t physicalAddress)
{
  // Select the right stack
  size_t index = 0;
  if (physicalAddress >= 0x100000000ULL)
  {
    #if defined(X86)
      return;
    #elif defined(X64)
      index = 1;
      if (physicalAddress >= 0x1000000000ULL)
        index = 2;
    #endif
  }

  // Expand the stack if necessary
  if (m_StackMax[index] == m_StackSize[index])
  {
    // Get the kernel virtual address-space
    #if defined(X86)
      X86VirtualAddressSpace &AddressSpace = static_cast<X86VirtualAddressSpace&>(VirtualAddressSpace::getKernelAddressSpace());
    #elif defined(X64)
      X64VirtualAddressSpace &AddressSpace = static_cast<X64VirtualAddressSpace&>(VirtualAddressSpace::getKernelAddressSpace());
    #endif

    if (AddressSpace.mapPageStructures(physicalAddress,
                                       adjust_pointer(m_Stack[index], m_StackMax[index]),
                                       VirtualAddressSpace::KernelMode | VirtualAddressSpace::Write)
        == true)
      return;

    m_StackMax[index] += getPageSize();
  }

  if (index == 0)
  {
    *(reinterpret_cast<uint32_t*>(m_Stack[0]) + m_StackSize[0] / 4) = static_cast<uint32_t>(physicalAddress);
    m_StackSize[0] += 4;
  }
  else
  {
    *(reinterpret_cast<uint64_t*>(m_Stack[index]) + m_StackSize[index] / 8) = physicalAddress;
    m_StackSize[index] += 8;
  }
}
X86CommonPhysicalMemoryManager::PageStack::PageStack()
{
  for (size_t i = 0;i < StackCount;i++)
  {
    m_StackMax[i] = 0;
    m_StackSize[i] = 0;
  }

  #if defined(X86)
    m_Stack[0] = reinterpret_cast<void*>(0xF0000000);
  #elif defined(X64)
    m_Stack[0] = reinterpret_cast<void*>(0xFFFFFFFF7FC00000);
    m_Stack[1] = 0;
    m_Stack[2] = 0;
  #endif
}
