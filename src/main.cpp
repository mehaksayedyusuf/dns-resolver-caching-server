#include "common.hpp"
#include "protocol_engine.hpp"
#include "storage_engine.hpp"
#include "upstream_engine.hpp"
#include "telemetry_engine.hpp"
#include "vm_engine.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <queue>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <csignal>

// ============================================================================
// DNS RESOLVER & CACHING SERVER (VM MULTI-CLUSTER CLIENT-SERVER REACTOR)
// Academic Theme 4: Network Systems and Tools (Team 11)
// Architecture: Multi-Interface Reactor + Worker Pool + 6-Stage VM + Cluster ID
// ============================================================================

static std::atomic<bool> g_running(true);

// Universal Signal Handler for Graceful Shutdown
void SignalHandler(int signum) {
    (void)signum;
    std::cout << "\n[SERVER] Shutdown signal received. Initiating graceful shutdown...\n";
    g_running = false;
}

#ifdef _WIN32
// Win32 Console Signal Handler (Ctrl+C / Termination)
BOOL WINAPI ConsoleCtrlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[SERVER] Shutdown signal received. Initiating graceful shutdown...\n";
        g_running = false;
        return TRUE;
    }
    return FALSE;
}
#endif

// Client Transaction Task for Thread Pool
struct ClientTask {
    std::vector<uint8_t> buffer;
    sockaddr_in client_addr;
};

// Thread-Safe Worker Task Queue (Cross-Platform C++14 Thread Pool)
class WorkerThreadPool {
public:
    WorkerThreadPool(size_t num_workers, VMEngine& vm, SOCKET listen_socket, const std::string& cluster_id)
        : vm_(vm), listen_socket_(listen_socket), cluster_id_(cluster_id), stopping_(false) {
        for (size_t i = 0; i < num_workers; ++i) {
            workers_.emplace_back(&WorkerThreadPool::worker_loop, this);
        }
    }

    ~WorkerThreadPool() {
        stop();
    }

    void enqueue(ClientTask task) {
        {
            std::unique_lock<std::mutex> lock(queue_mtx_);
            tasks_.push(std::move(task));
        }
        queue_cv_.notify_one();
    }

    void stop() {
        {
            std::unique_lock<std::mutex> lock(queue_mtx_);
            stopping_ = true;
        }
        queue_cv_.notify_all();

        for (std::thread& t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }
        workers_.clear();
    }

private:
    void worker_loop() {
        while (true) {
            ClientTask task;
            {
                std::unique_lock<std::mutex> lock(queue_mtx_);
                while (tasks_.empty() && !stopping_) {
                    queue_cv_.wait(lock);
                }
                if (stopping_ && tasks_.empty()) {
                    break;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }

            // Client endpoint identification (Client-Server mechanism)
            std::string client_ip = inet_ntoa(task.client_addr.sin_addr);
            uint16_t client_port = ntohs(task.client_addr.sin_port);

            VMTransaction tx;
            tx.client_ip = client_ip;
            tx.client_port = client_port;
            tx.inbound_raw = std::move(task.buffer);

            // Execute 6-stage VM pipeline
            vm_.process_transaction(tx);

            // Log detailed outcome with Cluster Node prefix
            std::string qname = !tx.query_packet.questions.empty() ? tx.query_packet.questions[0].qname : "<malformed>";
            std::string qtype_str = !tx.query_packet.questions.empty() ? ProtocolEngine::qtype_to_string(tx.query_packet.questions[0].qtype) : "";
            double lat_ms = tx.latency_microseconds / 1000.0;

            std::string prefix = "[" + cluster_id_ + "] ";

            if (tx.condition == VMCondition::CACHE_HIT) {
                uint32_t rem_ttl = !tx.response_packet.answers.empty() ? tx.response_packet.answers[0].ttl : 0;
                std::cout << prefix << "[CACHE HIT] " << qname << " (" << qtype_str << ") -> "
                          << tx.response_packet.answers.size() << " answer(s) to " << client_ip << ":" << client_port
                          << " in " << std::fixed << std::setprecision(2) << lat_ms << "ms (TTL: " << rem_ttl << "s)\n";
            } else if (tx.condition == VMCondition::CACHE_NEGATIVE_HIT) {
                std::cout << prefix << "[NEGATIVE HIT] " << qname << " -> NXDOMAIN to " << client_ip << " in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms\n";
            } else if (tx.condition == VMCondition::UPSTREAM_SUCCESS) {
                std::cout << prefix << "[CACHE MISS -> RESOLVED] " << qname << " (" << qtype_str << ") -> "
                          << tx.response_packet.answers.size() << " answer(s) from upstream to " << client_ip
                          << " in " << std::fixed << std::setprecision(2) << lat_ms << "ms (cached)\n";
            } else if (tx.condition == VMCondition::UPSTREAM_NXDOMAIN) {
                std::cout << prefix << "[MISS -> NXDOMAIN] " << qname << " -> NXDOMAIN to " << client_ip
                          << " in " << std::fixed << std::setprecision(2) << lat_ms << "ms\n";
            } else if (tx.condition == VMCondition::CACHE_EXPIRED) {
                std::cout << prefix << "[CACHE EXPIRED] " << qname << " (" << qtype_str << ") -> Refreshed in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms\n";
            } else if (tx.condition == VMCondition::UPSTREAM_TIMEOUT) {
                std::cout << prefix << "[UPSTREAM TIMEOUT] " << qname << " -> SERVFAIL in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms\n";
            } else if (tx.condition == VMCondition::FORMERR_MALFORMED) {
                std::cout << prefix << "[FORMERR] Malformed query from " << client_ip << ":" << client_port << "\n";
            } else {
                std::cout << prefix << "[RESOLVED] " << qname << " (" << VMEngine::condition_to_string(tx.condition) << ") in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms\n";
            }

            // Transmit serialized RFC 1035 response datagram back to client
            if (!tx.outbound_raw.empty()) {
                int sent = sendto(listen_socket_, reinterpret_cast<const char*>(tx.outbound_raw.data()),
                                  static_cast<int>(tx.outbound_raw.size()), 0,
                                  reinterpret_cast<struct sockaddr*>(&task.client_addr),
                                  sizeof(task.client_addr));
                if (sent == SOCKET_ERROR) {
                    std::cerr << prefix << "[WORKER] sendto failed: " << WSAGetLastError() << "\n";
                }
            }
        }
    }

    VMEngine& vm_;
    SOCKET listen_socket_;
    std::string cluster_id_;
    std::mutex queue_mtx_;
    std::condition_variable queue_cv_;
    std::queue<ClientTask> tasks_;
    std::vector<std::thread> workers_;
    bool stopping_;
};

static void print_banner(const std::string& cluster_id, const std::string& bind_ip, uint16_t port,
                         const std::string& upstream_ip, size_t capacity, size_t threads,
                         const std::string& peer_cluster) {
    std::cout << "\n";
    std::cout << "===========================================================================\n";
    std::cout << "        RFC 1035 DNS RESOLVER & IN-MEMORY CACHING SERVER (C++)             \n";
    std::cout << "        Theme 4: Network Systems & Tools | Clustered VM Deployment         \n";
    std::cout << "===========================================================================\n";
    std::cout << "  Cluster Identity & Networking:\n";
    std::cout << "    • Cluster Node ID : " << cluster_id << "\n";
    std::cout << "    • Listening Bind  : " << bind_ip << ":" << port << " (UDP)\n";
    std::cout << "    • Primary Upstream: " << upstream_ip << ":53 (Google Public DNS)\n";
    std::cout << "    • Fallback DNS    : 1.1.1.1:53 (Cloudflare Public DNS)\n";
    if (!peer_cluster.empty()) {
        std::cout << "    • Peer Cluster    : " << peer_cluster << " (Inter-Cluster Peering)\n";
    }
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << "  Engine Performance & Storage:\n";
    std::cout << "    • In-Memory Cache : " << capacity << " records capacity (Dual-Index LRU+Heap)\n";
    std::cout << "    • Worker Pool     : " << threads << " OS worker threads\n";
    std::cout << "    • Persistence DB  : dns_cache_" << cluster_id << ".db\n";
    std::cout << "===========================================================================\n";
    std::cout << "  Ready for client queries! (Press Ctrl+C to terminate gracefully)\n";
    std::cout << "  Example Client Query from Host or Client VM:\n";
    std::cout << "    dig @" << (bind_ip == "0.0.0.0" ? "<VM_IP>" : bind_ip) << " -p " << port << " example.com A\n";
    std::cout << "===========================================================================\n\n";
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    std::string cluster_id = "CLUSTER-ALPHA";
    std::string bind_ip = "0.0.0.0";
    uint16_t port = DEFAULT_SERVER_PORT;
    std::string upstream_ip = "8.8.8.8";
    size_t cache_capacity = 1000;
    size_t worker_threads = 4;
    std::string peer_cluster = "";
    std::string db_file = "";
    bool warmup = true;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-b" || arg == "--bind") && i + 1 < argc) {
            bind_ip = argv[++i];
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-u" || arg == "--upstream") && i + 1 < argc) {
            upstream_ip = argv[++i];
        } else if ((arg == "-c" || arg == "--capacity") && i + 1 < argc) {
            cache_capacity = static_cast<size_t>(std::stoul(argv[++i]));
        } else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
            worker_threads = static_cast<size_t>(std::stoul(argv[++i]));
        } else if ((arg == "--cluster-id" || arg == "--cluster") && i + 1 < argc) {
            cluster_id = argv[++i];
        } else if (arg == "--peer" && i + 1 < argc) {
            peer_cluster = argv[++i];
        } else if (arg == "--db" && i + 1 < argc) {
            db_file = argv[++i];
        } else if (arg == "--no-warmup") {
            warmup = false;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: dns_server [-b bind_ip] [-p port] [-u upstream] [-c capacity] [-t threads] [--cluster-id id] [--peer ip:port]\n";
            return 0;
        }
    }

    if (db_file.empty()) {
        db_file = "dns_cache_" + cluster_id + ".db";
    }

    // Register Signal Handlers
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[ERROR] WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }
#endif

    // Instantiate Satellite Engines
    StorageEngine cache_engine(cache_capacity);
    UpstreamConfig upstream_cfg;
    upstream_cfg.primary_server = upstream_ip;
    upstream_cfg.fallback_server = "1.1.1.1";
    UpstreamEngine upstream_engine(upstream_cfg);
    TelemetryEngine telemetry_engine;
    VMEngine vm_engine(cache_engine, upstream_engine, telemetry_engine);

    // Boot Warm-up: Load persistent cache snapshot from disk if available
    if (warmup) {
        if (cache_engine.load_snapshot(db_file)) {
            std::cout << "[" << cluster_id << "][CACHE] Warm-up successful! Restored " << cache_engine.size()
                      << " active records from '" << db_file << "'.\n";
        }
    }

    // Create UDP Listening Socket
    SOCKET server_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_sock == INVALID_SOCKET) {
        std::cerr << "[ERROR] Failed to create UDP listening socket: " << WSAGetLastError() << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // Bind Socket to specified interface (0.0.0.0 allows all VM interfaces)
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (bind_ip == "0.0.0.0") {
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        server_addr.sin_addr.s_addr = inet_addr(bind_ip.c_str());
    }

    if (bind(server_sock, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Socket bind to " << bind_ip << ":" << port << " failed: " << WSAGetLastError() << std::endl;
        closesocket(server_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // Set Non-blocking mode on listen socket for responsive shutdown polling
#ifdef _WIN32
    u_long non_blocking_mode = 1;
    ioctlsocket(server_sock, FIONBIO, &non_blocking_mode);
#else
    int flags = fcntl(server_sock, F_GETFL, 0);
    fcntl(server_sock, F_SETFL, flags | O_NONBLOCK);
#endif

    // Launch Multi-Threaded Worker Pool
    WorkerThreadPool pool(worker_threads, vm_engine, server_sock, cluster_id);

    print_banner(cluster_id, bind_ip, port, upstream_ip, cache_capacity, worker_threads, peer_cluster);

    auto last_telemetry_time = std::chrono::steady_clock::now();
    auto last_sweep_time = std::chrono::steady_clock::now();

    // Main Network Reactor Event Loop
    while (g_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_sock, &read_fds);

        // 100ms select timeout ensures smooth exit on Ctrl+C signal
        struct timeval tv = { 0, 100000 };
        int sel = select(server_sock + 1, &read_fds, NULL, NULL, &tv);

        if (sel > 0) {
            uint8_t buffer[DNS_MAX_PACKET_SIZE * 2];
            sockaddr_in client_addr;
#ifdef _WIN32
            int client_len = sizeof(client_addr);
#else
            socklen_t client_len = sizeof(client_addr);
#endif

            int bytes = recvfrom(server_sock, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                 reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

            if (bytes > 0) {
                ClientTask task;
                task.buffer.assign(buffer, buffer + bytes);
                task.client_addr = client_addr;
                pool.enqueue(std::move(task));
            }
        }

        auto now = std::chrono::steady_clock::now();

        // Proactive TTL sweep every 5 seconds (Engine E2 Min-Heap)
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_sweep_time).count() >= 5) {
            size_t purged = cache_engine.purge_expired();
            if (purged > 0) {
                std::cout << "[" << cluster_id << "][CACHE SWEEP] Evicted " << purged
                          << " expired record(s) via Min-Heap. Active: "
                          << cache_engine.size() << "/" << cache_capacity << "\n";
            }
            last_sweep_time = now;
        }

        // Print telemetry every 30 seconds if active queries exist
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_telemetry_time).count() >= 30) {
            TelemetrySnapshot snap = telemetry_engine.get_snapshot();
            if (snap.total_queries > 0) {
                std::cout << telemetry_engine.generate_dashboard();
            }
            last_telemetry_time = now;
        }
    }

    // Shutdown Sequence
    std::cout << "[" << cluster_id << "] Stopping worker threads...\n";
    pool.stop();

    std::cout << "[" << cluster_id << "] Closing network socket...\n";
    closesocket(server_sock);

    // Save active cache snapshot to disk (durability persistence)
    std::cout << "[" << cluster_id << "][CACHE] Saving valid cache entries to '" << db_file << "'...\n";
    if (cache_engine.save_snapshot(db_file)) {
        std::cout << "[" << cluster_id << "][CACHE] Snapshot saved successfully (" << cache_engine.size() << " records active).\n";
    }

    // Print Final Telemetry Report
    std::cout << telemetry_engine.generate_dashboard();

#ifdef _WIN32
    WSACleanup();
#endif
    std::cout << "[" << cluster_id << "] Graceful shutdown complete. Goodbye!\n";
    return 0;
}
