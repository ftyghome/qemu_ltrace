#include "ltrace.h"
#include "qemu/typedefs.h"
#include "signal-common.h"
#include <sys/ioctl.h>

enum ParamType { INT, HEX, STRING, VOID, ETC, NONE };

#ifdef TARGET_AARCH64
  #define REG(X) (cpu_env->xregs[X])
#endif
#ifdef TARGET_RISCV
  #define REG(X) (cpu_env->gpr[X])
#endif

void ltrace_handle_generic(CPUArchState *cpu_env, const char* symbol, int paramCount, enum ParamType inType[5], enum ParamType outType, bool is_ret);
void ltrace_handle_unknown(const char *symbol, CPUArchState *cpu_env,
                           bool is_ret);
void ltrace_read_mem_str(CPUArchState *cpu_env, vaddr addr);
bool check_buf_has_end(const uint8_t *buf, size_t len);
bool print_with_no_newline(const uint8_t *buf, size_t len);

static bool ltrace_begin = false;

// when entering the ltrace_handle_symbol, current inst may be thought to be a "is_ret" by mistake.
// this happens when a inst is both breakpointed by "symbol breakpoint" and "return breakpoint". It may be not returning at that time.
static const char* pending_return = NULL;

void ltrace_handle_generic(CPUArchState *cpu_env, const char *symbol,
                           int paramCount, enum ParamType inType[5],
                           enum ParamType outType, bool is_ret) {
  int reg_index, reg_init;
#ifdef TARGET_AARCH64
  reg_index = reg_init = 0;
#endif
#ifdef TARGET_RISCV
  reg_index = reg_init = 10;
#endif
  if (!is_ret) {
    qemu_log("%s(", symbol);
    for (int i = 0; i < paramCount; i++) {
      switch (inType[i]) {
      case INT:
        qemu_log("%ld", REG(reg_index));
        break;
      case HEX:
        qemu_log("0x%lx", REG(reg_index));
        break;
      case STRING:
        ltrace_read_mem_str(cpu_env, REG(reg_index));
        break;
      case ETC:
        qemu_log("...");
        break;
      default:
        break;
      }
      reg_index++;
      if(i!=paramCount-1) qemu_log(",");
    }
    qemu_log(")");
  } else {
    qemu_log(" = ");
      switch (outType) {
      case INT:
        qemu_log("%ld", REG(reg_init));
        break;
      case HEX:
        qemu_log("0x%lx", REG(reg_init));
        break;
      case STRING:
        ltrace_read_mem_str(cpu_env, REG(reg_init));
        break;
      case VOID:
        qemu_log("<void>");
        break;
      default:
        break;
      }
      qemu_log("\n");
  }
}

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
  if(addr == 0){
    qemu_log("(nil)");
    return;
  } 
  qemu_log("\"");
  do {
    if (cc->memory_rw_debug) {
      cc->memory_rw_debug(cpu, addr, buf, 8, false);
    } else
      cpu_memory_rw_debug(cpu, addr, buf, 8, false);
    addr += 8;
    str_complete = print_with_no_newline(buf, 8);
  }
  while (!str_complete)
    ;
  qemu_log("\"");
}

void ltrace_handle_unknown(const char *symbol, CPUArchState *cpu_env,
                           bool is_ret) {
  if (!strcmp(symbol, "__libc_start_main")) return;
  if (!strcmp(symbol, "__cxa_finalize")) return;
  if (!strcmp(symbol, "_dl_catch_exception")) return;
  if (!strcmp(symbol, "__errno_location")) return;
    if (!is_ret)
      qemu_log("%s(?)", symbol);
    else
      qemu_log(" = (?)\n");
}

void ltrace_handle_symbol(const char *symbol, CPUArchState *cpu_env,
                          bool is_ret) {
                          
  
  static enum ParamType INT_type[5] = {INT};
  static enum ParamType STR_type[5] = {STRING};
  static enum ParamType HEX_type[5] = {HEX};
  static enum ParamType INT_INT_type[5] = {INT, INT};
  static enum ParamType STR_INT_type[5] = {STRING, INT};
  static enum ParamType INT_HEX_type[5] = {INT, HEX};
  static enum ParamType STR_HEX_type[5] = {STRING, HEX};
  static enum ParamType STR_ETC_type[5] = {STRING, ETC};
  static enum ParamType STR_STR_type[5] = {STRING, STRING};
  static enum ParamType HEX_STR_HEX_type[5] = {HEX, STRING,HEX};
  static enum ParamType HEX_INT_INT_type[5] = {HEX, INT, INT};
  static enum ParamType HEX_HEX_INT_type[5] = {HEX, HEX, INT};
  static enum ParamType INT_HEX_STRING_HEX_HEX_type[5] = {INT,HEX,STRING,HEX,HEX};

  if (!ltrace_begin) {
    qemu_log("++++ ltrace ++++\n"); ltrace_begin = true;
  }
  
  if(is_ret && (!pending_return || strcmp(symbol,pending_return)!=0)) return;
  if(!is_ret) pending_return = symbol;

  if (!strcmp(symbol, "malloc")) {
    ltrace_handle_generic(cpu_env, symbol,1,INT_type,HEX,is_ret);
  } else if (!strcmp(symbol, "puts")) {
    ltrace_handle_generic(cpu_env, symbol,1,STR_type,INT,is_ret);
  } else if (!strcmp(symbol, "fflush")) {
    ltrace_handle_generic(cpu_env, symbol,1,HEX_type,INT,is_ret);
  } else if (!strcmp(symbol, "printf")) {
    ltrace_handle_generic(cpu_env, symbol,2,STR_ETC_type,INT,is_ret);
  } else if (!strcmp(symbol, "free")) {
    ltrace_handle_generic(cpu_env, symbol,1,HEX_type,VOID,is_ret);
  } else if (!strcmp(symbol, "memcpy")) {
    ltrace_handle_generic(cpu_env, symbol,3,HEX_HEX_INT_type,HEX,is_ret);
  } else if (!strcmp(symbol, "mallopt")) {
    ltrace_handle_generic(cpu_env, symbol,2,INT_INT_type,INT,is_ret);
  } else if (!strcmp(symbol, "strrchr")) {
    ltrace_handle_generic(cpu_env, symbol, 2, STR_INT_type, STRING, is_ret);
  } else if (!strcmp(symbol, "stat64")) {
    ltrace_handle_generic(cpu_env, symbol, 2, STR_HEX_type, INT, is_ret);
  } else if (!strcmp(symbol, "strcmp")) {
    ltrace_handle_generic(cpu_env, symbol, 2, STR_STR_type, INT, is_ret);
  } else if (!strcmp(symbol, "setgid")) {
    ltrace_handle_generic(cpu_env, symbol, 1, INT_type, INT, is_ret);
  } else if (!strcmp(symbol, "setuid")) {
    ltrace_handle_generic(cpu_env, symbol, 1, INT_type, INT, is_ret);
  } else if (!strcmp(symbol, "time")) {
    ltrace_handle_generic(cpu_env, symbol, 1, INT_type, INT, is_ret);
  } else if (!strcmp(symbol, "ioctl")) {
    ltrace_handle_generic(cpu_env, symbol, 2, INT_HEX_type, INT, is_ret);
  } else if (!strcmp(symbol, "getenv")) {
    ltrace_handle_generic(cpu_env, symbol, 1, STR_type, HEX, is_ret);
  } else if (!strcmp(symbol, "getuid")) {
    ltrace_handle_generic(cpu_env, symbol, 0, NULL, INT, is_ret);
  } else if (!strcmp(symbol, "getgid")) {
    ltrace_handle_generic(cpu_env, symbol, 0, NULL, INT, is_ret);
  } else if (!strcmp(symbol, "memset")) {
    ltrace_handle_generic(cpu_env, symbol, 3, HEX_INT_INT_type, HEX, is_ret);
  } else if (!strcmp(symbol, "strlen")) {
    ltrace_handle_generic(cpu_env, symbol, 1, STR_type, INT,is_ret);
  } else if (!strcmp(symbol, "lstat64")) {
    ltrace_handle_generic(cpu_env, symbol, 2, STR_HEX_type, INT,is_ret);
  } else if (!strcmp(symbol, "strcpy")) {
    ltrace_handle_generic(cpu_env, symbol, 2, STR_STR_type, STRING,is_ret);
  } else if (!strcmp(symbol, "getopt_long")) {
    ltrace_handle_generic(cpu_env, symbol, 5, INT_HEX_STRING_HEX_HEX_type, HEX,is_ret);
  } else if (!strcmp(symbol, "isatty")) {
    ltrace_handle_generic(cpu_env, symbol, 1, INT_type, INT,is_ret);
  } else if (!strcmp(symbol, "gnu_dev_major" )|| !strcmp(symbol, "gnu_dev_minor" )) {
    ltrace_handle_generic(cpu_env, symbol, 1, HEX_type, INT,is_ret);
  } else if (!strcmp(symbol, "readdir64")) {
    ltrace_handle_generic(cpu_env, symbol, 1, HEX_type, HEX,is_ret);
  } else if (!strcmp(symbol, "vasprintf")) {
    ltrace_handle_generic(cpu_env, symbol, 3, HEX_STR_HEX_type, INT,is_ret);
  } else if (!strcmp(symbol, "opendir")) {
    ltrace_handle_generic(cpu_env, symbol, 1, STR_type, HEX,is_ret);
  } else if (!strcmp(symbol, "putchar") || !strcmp(symbol, "putchar_unlocked")) {
    ltrace_handle_generic(cpu_env, symbol, 1, INT_type, INT,is_ret);
  } else if (!strcmp(symbol, "exit")) {
    ltrace_handle_generic(cpu_env, symbol, 1, INT_type, VOID,is_ret);
  } else ltrace_handle_unknown(symbol, cpu_env, is_ret);
}