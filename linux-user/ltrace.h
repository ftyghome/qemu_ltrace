#pragma once

#include "qemu/typedefs.h"
#ifndef LTRACE_HEADERS

#define LTRACE_HEADERS

#include <stdint.h>

#include "gdbstub/user.h"
#include "qemu/osdep.h"

#include "qemu.h"
#include "qemu/qemu-print.h"
#include "qemu/typedefs.h"
#include "user-internals.h"
#include <sys/resource.h>
#include <sys/ucontext.h>
struct plt_stub {
  uint64_t addr;
  const char *name;
};

void ltrace_handle_symbol(const char *symbol, CPUArchState *cpu_env,
                          bool is_ret);

#endif