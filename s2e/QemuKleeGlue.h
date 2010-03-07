#ifndef QEMU_KLEE_GLUE_H

#include <string>
#include <inttypes.h>

#define QEMU_KLEE_GLUE_H

class QEMU
{
public:
  static bool GetAsciiz(uint64_t Addr, std::string &Ret);
  static std::string GetUnicode(uint64_t Addr, unsigned Length);
  static bool ReadVirtualMemory(uint64_t Addr, void *Buffer, unsigned Length);
  static uint64_t GetPhysAddr(uint64_t va);
  static void DumpVirtualMemory(uint64_t Addr, unsigned Length);
};


#endif