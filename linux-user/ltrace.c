#include "ltrace.h"
#include <sys/ioctl.h>


void ltrace_handle_malloc(CPUArchState *cpu_env, bool is_ret);
void ltrace_handle_free(CPUArchState *cpu_env, bool is_ret);
void ltrace_handle_puts(CPUArchState *cpu_env, bool is_ret);
void ltrace_handle_printf(CPUArchState *cpu_env, bool is_ret);
void ltrace_handle_fflush(CPUArchState *cpu_env, bool is_ret);
void ltrace_handle_memcpy(CPUArchState *cpu_env, bool is_ret);
void ltrace_handle_unknown(const char *symbol, CPUArchState *cpu_env,
                           bool is_ret);
void ltrace_read_mem_str(CPUArchState *cpu_env, vaddr addr);
bool check_buf_has_end(const uint8_t *buf, size_t len);
bool print_with_no_newline(const uint8_t *buf, size_t len);

static bool ltrace_begin = false;

bool check_buf_has_end(const uint8_t *buf, size_t len) {
  for (int i = 0; i < len; i++) {
    if(buf[i]=='\0') return true;
  }
  return false;
}

bool print_with_no_newline(const uint8_t *buf, size_t len) {
  for (int i = 0; i < len; i++) {
    if (buf[i] == '\0')
      return true;
    if (buf[i] == '\n')
      qemu_log("\\n");
    else qemu_log("%c",buf[i]);
  }
  return false;
}

void ltrace_read_mem_str(CPUArchState *cpu_env, vaddr addr) {
  CPUState *cpu = env_cpu(cpu_env);
  CPUClass *cc;
  cc = CPU_GET_CLASS(cpu);
  uint8_t *buf = alloca(8);
  bool str_complete;
  do {
    if (cc->memory_rw_debug) {
      cc->memory_rw_debug(cpu, addr, buf, 8, false);
    } else
      cpu_memory_rw_debug(cpu, addr, buf, 8, false);
    addr += 8;
    str_complete = print_with_no_newline(buf, 8);
  } while(!str_complete);
}

void ltrace_handle_malloc(CPUArchState *cpu_env, bool is_ret) {
  uint64_t value = 0;
#ifdef TARGET_AARCH64
  value = cpu_env->xregs[0];
#endif
#ifdef TARGET_RISCV
  value = cpu_env->gpr[10];
#endif
  if(!is_ret)
    qemu_log("malloc(%ld)", value);
  else qemu_log(" = 0x%lx\n",value);
}

void ltrace_handle_free(CPUArchState *cpu_env, bool is_ret) {
  uint64_t value = 0;
#ifdef TARGET_AARCH64
  value = cpu_env->xregs[0];
#endif
#ifdef TARGET_RISCV
  value = cpu_env->gpr[10];
#endif
  if (!is_ret)
    qemu_log("free(0x%lx)", value);
  else
    qemu_log(" = <void>\n");
}

void ltrace_handle_puts(CPUArchState *cpu_env, bool is_ret) {
  uint64_t x0 = 0;
#ifdef TARGET_AARCH64
  x0 = cpu_env->xregs[0];
#endif
#ifdef TARGET_RISCV
  x0 = cpu_env->gpr[10];
#endif
  if(!is_ret){
    qemu_log("puts(\"");
    ltrace_read_mem_str(cpu_env, x0);
    qemu_log("\")");
  } else
  qemu_log(" = %ld\n", x0);
}

void ltrace_handle_printf(CPUArchState *cpu_env, bool is_ret) {
  uint64_t x0 = 0;
#ifdef TARGET_AARCH64
  x0 = cpu_env->xregs[0];
#endif
#ifdef TARGET_RISCV
  x0 = cpu_env->gpr[10];
#endif
  if (!is_ret) {
  qemu_log("printf(\"");
  ltrace_read_mem_str(cpu_env, x0);
  qemu_log("\", ...)");
  } else
  qemu_log(" = %ld\n", x0);
}

void ltrace_handle_fflush(CPUArchState *cpu_env, bool is_ret) {
  uint64_t x0 = 0;
#ifdef TARGET_AARCH64
  x0 = cpu_env->xregs[0];
#endif
#ifdef TARGET_RISCV
  x0 = cpu_env->gpr[10];
#endif
  if (!is_ret)
    qemu_log("fflush(0x%lx)", x0);
  else
    qemu_log(" = %ld\n", x0);
}

void ltrace_handle_memcpy(CPUArchState *cpu_env, bool is_ret) {
  uint64_t x0 = 0, x1 = 0, x2 = 0;
#ifdef TARGET_AARCH64
  x0 = cpu_env->xregs[0];
  x1 = cpu_env->xregs[1];
  x2 = cpu_env->xregs[2];
#endif
#ifdef TARGET_RISCV
  x0 = cpu_env->gpr[10];
  x1 = cpu_env->gpr[11];
  x2 = cpu_env->gpr[12];
#endif
  if (!is_ret)
    qemu_log("memcpy(0x%lx, 0x%lx, %ld)", x0,x1,x2);
  else
    qemu_log(" = 0x%lx\n", x0);
}

void ltrace_handle_unknown(const char *symbol, CPUArchState *cpu_env,
                           bool is_ret) {
  if (!strcmp(symbol, "__libc_start_main")) return;
  if (!strcmp(symbol, "__cxa_finalize")) return;
  if (!strcmp(symbol, "_dl_catch_exception")) return;
    if (!is_ret)
      qemu_log("%s(?)", symbol);
    else
      qemu_log(" = (?)\n");
}

void ltrace_handle_symbol(const char *symbol, CPUArchState *cpu_env,
                          bool is_ret) {
  if (!ltrace_begin) {
    qemu_log("++++ ltrace ++++\n");
    ltrace_begin = true;
  }
  if (!strcmp(symbol, "malloc")) {
    ltrace_handle_malloc(cpu_env,is_ret);
  } else if (!strcmp(symbol, "puts")) {
    ltrace_handle_puts(cpu_env,is_ret);
  } else if (!strcmp(symbol, "fflush")) {
    ltrace_handle_fflush(cpu_env,is_ret);
  } else if (!strcmp(symbol, "printf")) {
    ltrace_handle_printf(cpu_env, is_ret);
  } else if (!strcmp(symbol, "free")) {
    ltrace_handle_free(cpu_env, is_ret);
  } else if (!strcmp(symbol, "memcpy")) {
    ltrace_handle_memcpy(cpu_env, is_ret);
  }else ltrace_handle_unknown(symbol, cpu_env, is_ret);
}