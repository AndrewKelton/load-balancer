#include <csignal>

#include "load_balancer.hpp"
#include "utils.hpp"

static volatile std::sig_atomic_t g_running = 1;
static void on_sigint(int) { g_running = 0; }

int main(int argc, char * argv[]) {

    command_line_setting_t settings = parse_command_line(argc, argv);

    struct sigaction sa{};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // no SA_RESTART, so accept() returns EINTR
    sigaction(SIGINT, &sa, nullptr);

    std::vector<int> ports = {9001, 9002, 9003};

    load_balancer::LoadBalancer loadbalancer(ports, 8080, settings.method, settings.rate_limiting_active);
    loadbalancer.start_listen();
    loadbalancer.start_load_balancer(g_running);

    return 0;
}