#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef ASSERT_MACROS_USE_SYSLOG
#define ASSERT_MACROS_USE_SYSLOG 1
#endif

#ifndef ASSERT_MACROS_SQUELCH
#define ASSERT_MACROS_SQUELCH 0
#endif

#ifndef CONCORDD_BACKTRACE
#define CONCORDD_BACKTRACE    (HAVE_EXECINFO_H || __APPLE__)
#endif

#if HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#if HAVE_ASM_SIGCONTEXT
#include <asm/sigcontext.h>
#endif

#if HAVE_UTIL_H
#include <util.h>
#endif

#ifndef FAULT_BACKTRACE_STACK_DEPTH
#define FAULT_BACKTRACE_STACK_DEPTH        20
#endif

#if __linux__ && defined(__x86_64__) && !defined(REG_RIP)
#undef CONCORDD_BACKTRACE
#endif

static void
signal_critical(int sig, siginfo_t * info, void * ucontext)
{
    // This is the last hurah for this process.
    // We dump the stack, because that's all we can do.

    // We call some functions here which aren't async-signal-safe,
    // but this function isn't really useful without those calls.
    // Since we are making a gamble (and we deadlock if we loose),
    // we are going to set up a two-second watchdog to make sure
    // we end up terminating like we should. The choice of a two
    // second timeout is entirely arbitrary, and may be changed
    // if needs warrant.
    alarm(2);
    signal(SIGALRM, SIG_DFL);

#if CONCORDD_BACKTRACE
    void *stack_mem[FAULT_BACKTRACE_STACK_DEPTH];
    void **stack = stack_mem;
    char **stack_symbols;
    int stack_depth, i;
    ucontext_t *uc = (ucontext_t*)ucontext;

    // Shut up compiler warning.
    (void)uc;

#endif // CONCORDD_BACKTRACE

    fprintf(stderr, " *** FATAL ERROR: Caught signal %d (%s):\n", sig, strsignal(sig));

#if CONCORDD_BACKTRACE
    stack_depth = backtrace(stack, FAULT_BACKTRACE_STACK_DEPTH);

#if __APPLE__
    // OS X adds an extra call onto the stack that
    // we can leave out for clarity sake.
    stack[1] = stack[0];
    stack++;
    stack_depth--;
#endif

    // Here are are trying to update the pointer in the backtrace
    // to be the actual location of the fault.
#if __linux__
#if defined(__x86_64__)
    stack[1] = (void *) uc->uc_mcontext.gregs[REG_RIP];
#elif defined(__i386__)
    stack[1] = (void *) uc->uc_mcontext.gregs[REG_EIP];
#elif defined(__arm__)
    stack[1] = (void *) uc->uc_mcontext.arm_ip;
#else
#warning TODO: Add this arch to signal_critical
#endif

#elif __APPLE__ // #if __linux__
#if defined(__x86_64__)
    stack[1] = (void *) uc->uc_mcontext->__ss.__rip;
#endif

#else //#elif __APPLE__
#warning TODO: Add this OS to signal_critical
#endif

    // Now dump the symbols to stderr, in case syslog barfs.
    backtrace_symbols_fd(stack, stack_depth, STDERR_FILENO);

    // Load up the symbols individually, so we can output to syslog, too.
    stack_symbols = backtrace_symbols(stack, stack_depth);
#endif // CONCORDD_BACKTRACE

    syslog(LOG_CRIT, " *** FATAL ERROR: Caught signal %d (%s):", sig, strsignal(sig));

#if CONCORDD_BACKTRACE
    for(i = 0; i != stack_depth; i++) {
#if __APPLE__
        syslog(LOG_CRIT, "[BT] %s", stack_symbols[i]);
#else
        syslog(LOG_CRIT, "[BT] %2d: %s", i, stack_symbols[i]);
#endif
    }

    free(stack_symbols);
#endif // CONCORDD_BACKTRACE

    _exit(EXIT_FAILURE);
}

void
install_crash_trace_handler() {
    struct sigaction sigact = { };
    sigact.sa_sigaction = &signal_critical;
    sigact.sa_flags = SA_RESTART | SA_SIGINFO | SA_NOCLDWAIT;

    sigaction(SIGSEGV, &sigact, (struct sigaction *)NULL);
    sigaction(SIGBUS, &sigact, (struct sigaction *)NULL);
    sigaction(SIGILL, &sigact, (struct sigaction *)NULL);
    sigaction(SIGABRT, &sigact, (struct sigaction *)NULL);
}
