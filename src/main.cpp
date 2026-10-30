#include <csignal>

#include "load_balancer.hpp"

static volatile std::sig_atomic_t g_running = 1;
static void on_sigint(int) { g_running = 0; }

int main(int argc, char * argv[]) {

    std::string load_method = "least-connections";
    if (argc > 1) {
        load_method = argv[1];
    }

    struct sigaction sa{};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // no SA_RESTART, so accept() returns EINTR
    sigaction(SIGINT, &sa, nullptr);

    std::vector<int> ports = {9001, 9002, 9003};

    load_balancer::LoadBalancer loadbalancer(ports, load_method);
    loadbalancer.start_listen(g_running);

    return 0;
}