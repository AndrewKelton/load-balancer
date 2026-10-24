
#include "load_balancer.hpp"

int main() {
    std::vector<int> ports(3);
    ports[0] = 9001;
    ports[1] = 9002;
    ports[2] = 9003;

    load_balancer::LoadBalancer loadbalancer(ports);
    loadbalancer.start_listen();

    return 0;
}