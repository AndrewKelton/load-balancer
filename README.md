# Load-Balancer

A load balancer and HTTP server implementation in C++, using round-robin and least-connections scheduling. This program utilizes multithreading techniques such as thread pools and background workers to handle large volumes of requests at concurrently. The load balancer redirects client requests to one of the three backend servers that can be spun up, client requests are handled using a thread pool of workers who dequeue tasks (either round-robin or least-connections) and send client requests to the respective server, and send the result back to the client. 

## Getting Started

### Dependencies

* C++23
* CMake

### Installing

1. **Clone the repository:**
```bash
git clone https://github.com/AndrewKelton/load-balancer.git
cd load-balancer
```

2. **Create and enter build directory:**
```bash
cmake -B build
```

3. **Compile the program**:
```bash
cmake --build build
```

### Executing program

After building, the executables will be located in the `build/` directory

```bash
# start the load balancer
./build/load_balancer <'round-robin' or 'least-connections'>

# in three seperate terminals, start the backend servers
./build/backend <port number> 
```

## Known Issues

* Unchecked I/O return values for write(), send(), and recv()

* No framing on reads

* Hardcoded MAX_NUM_SERVERS

* DEGRADED servers are ignored until they are marked OK again

## Authors

Contributors names and contact info

Andrew Kelton
[@andrew-kelton](https://www.linkedin.com/in/andrew-kelton)

## License

This project is licensed under the MIT License - see the LICENSE file for details