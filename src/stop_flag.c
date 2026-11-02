#include "stop_flag.h"

#include <signal.h>
#include <string.h>

static volatile sig_atomic_t stop_requested;

static void on_stop_signal(int signo)
{
    (void)signo;
    stop_requested = 1;
}

void stop_flag_install_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_stop_signal;
    sigemptyset(&sa.sa_mask);
    /* 不加 SA_RESTART：打断阻塞中的 read 等，返回 EINTR */
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

int stop_flag_is_set(void)
{
    return stop_requested != 0;
}
