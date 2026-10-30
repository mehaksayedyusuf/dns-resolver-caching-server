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

// ============================================================================
// DNS RESOLVER & CACHING SERVER (MAIN SERVER ENTRY & REACTOR THREAD POOL)
// Academic Theme 4: Network Systems and Tools (Team 11)
// Architecture: Reactor Socket Listener + Worker Thread Pool + 6-Stage VM
// ============================================================================

static volatile bool g_running = true;

// Win32 Console Signal Handler (Ctrl+C / Termination)
BOOL WINAPI ConsoleCtrlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[SERVER] Shutdown signal received. Initiating graceful shutdown...\n";
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

// Client Transaction Task for Thread Pool
struct ClientTask {
    std::vector<uint8_t> buffer;
    sockaddr_in client_addr;
};

// Thread-Safe Worker Task Queue (Win32 CRITICAL_SECTION + CONDITION_VARIABLE)
class WorkerThreadPool {
public:
    WorkerThreadPool(size_t num_workers, VMEngine& vm, SOCKET listen_socket)
        : vm_(vm), listen_socket_(listen_socket), stopping_(false) {
        InitializeCriticalSection(&cs_);
        InitializeConditionVariable(&cv_);

        for (size_t i = 0; i < num_workers; ++i) {
            HANDLE h = CreateThread(NULL, 0, WorkerThreadProc, this, 0, NULL);
            if (h != NULL) {
                worker_threads_.push_back(h);
            }
        }
    }

    ~WorkerThreadPool() {
        stop();
        DeleteCriticalSection(&cs_);
    }

    void enqueue(ClientTask task) {
        EnterCriticalSection(&cs_);
        tasks_.push(std::move(task));
        WakeConditionVariable(&cv_);
        LeaveCriticalSection(&cs_);
    }

    void stop() {
        EnterCriticalSection(&cs_);
        stopping_ = true;
        WakeAllConditionVariable(&cv_);
        LeaveCriticalSection(&cs_);

        for (HANDLE h : worker_threads_) {
            WaitForSingleObject(h, 2000);
            CloseHandle(h);
        }
        worker_threads_.clear();
    }

private:
    static DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
        WorkerThreadPool* pool = static_cast<WorkerThreadPool*>(lpParam);
        pool->worker_loop();
        return 0;
    }

    void worker_loop() {
        while (true) {
            ClientTask task;
            {
                EnterCriticalSection(&cs_);
                while (tasks_.empty() && !stopping_) {
                    SleepConditionVariableCS(&cv_, &cs_, INFINITE);
                }
                if (stopping_ && tasks_.empty()) {
                    LeaveCriticalSection(&cs_);
                    break;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
                LeaveCriticalSection(&cs_);
            }

            // Execute 6-stage VM pipeline for client datagram
            std::string client_ip = inet_ntoa(task.client_addr.sin_addr);
            uint16_t client_port = ntohs(task.client_addr.sin_port);

            VMTransaction tx;
            tx.client_ip = client_ip;
            tx.client_port = client_port;
            tx.inbound_raw = std::move(task.buffer);

            vm_.process_transaction(tx);

            // Log detailed query outcome and cache verification
            std::string qname = !tx.query_packet.questions.empty() ? tx.query_packet.questions[0].qname : "<malformed>";
            std::string qtype_str = !tx.query_packet.questions.empty() ? ProtocolEngine::qtype_to_string(tx.query_packet.questions[0].qtype) : "";
            double lat_ms = tx.latency_microseconds / 1000.0;

            if (tx.condition == VMCondition::CACHE_HIT) {
                uint32_t rem_ttl = !tx.response_packet.answers.empty() ? tx.response_packet.answers[0].ttl : 0;
                std::cout << "[CACHE HIT] " << qname << " (" << qtype_str << ") -> "
                          << tx.response_packet.answers.size() << " answer(s) in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms (remaining TTL: "
                          << rem_ttl << "s)\n";
            } else if (tx.condition == VMCondition::CACHE_NEGATIVE_HIT) {
                std::cout << "[CACHE NEGATIVE HIT] " << qname << " -> NXDOMAIN in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms\n";
            } else if (tx.condition == VMCondition::UPSTREAM_SUCCESS) {
                std::cout << "[CACHE MISS -> RESOLVED] " << qname << " (" << qtype_str << ") -> "
                          << tx.response_packet.answers.size() << " answer(s) from upstream in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms (saved to cache)\n";
            } else if (tx.condition == VMCondition::UPSTREAM_NXDOMAIN) {
                std::cout << "[CACHE MISS -> NXDOMAIN] " << qname << " -> NXDOMAIN from upstream in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms (negative cached)\n";
            } else if (tx.condition == VMCondition::CACHE_EXPIRED) {
                std::cout << "[CACHE EXPIRED] " << qname << " (" << qtype_str << ") -> Refreshed from upstream in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms\n";
            } else if (tx.condition == VMCondition::UPSTREAM_TIMEOUT) {
                std::cout << "[UPSTREAM TIMEOUT] " << qname << " -> SERVFAIL in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms\n";
            } else if (tx.condition == VMCondition::FORMERR_MALFORMED) {
                std::cout << "[FORMERR] Malformed query from " << client_ip << ":" << client_port << "\n";
            } else {
                std::cout << "[RESOLVED] " << qname << " (" << VMEngine::condition_to_string(tx.condition) << ") in "
                          << std::fixed << std::setprecision(2) << lat_ms << "ms\n";
            }

            // Transmit serialized response datagram back to client
            if (!tx.outbound_raw.empty()) {
                int sent = sendto(listen_socket_, reinterpret_cast<const char*>(tx.outbound_raw.data()),
                                  static_cast<int>(tx.outbound_raw.size()), 0,
                                  reinterpret_cast<struct sockaddr*>(&task.client_addr),
                                  sizeof(task.client_addr));
                if (sent == SOCKET_ERROR) {
                    std::cerr << "[WORKER] sendto failed: " << WSAGetLastError() << "\n";
                }
            }
        }
    }

    VMEngine& vm_;
    SOCKET listen_socket_;
    CRITICAL_SECTION cs_;
    CONDITION_VARIABLE cv_;
    std::queue<ClientTask> tasks_;
    std::vector<HANDLE> worker_threads_;
    bool stopping_;
};

static void print_banner(uint16_t port, const std::string& upstream_ip, size_t capacity, size_t threads) {
    std::cout << "\n";
    std::cout << "===========================================================================\n";
    std::cout << "        RFC 1035 DNS RESOLVER & IN-MEMORY CACHING SERVER (C++)             \n";
    std::cout << "                  Theme 4: Network Systems & Tools                         \n";
    std::cout << "===========================================================================\n";
    std::cout << "  Core Architecture Engines:\n";
    std::cout << "    • E1 (Protocol Wire Engine) : Vageesh Mathad (Computer Networks)\n";
    std::cout << "    • E2 (Dual-Index Cache)     : Mehak Sayed Yusuf (DAA & DBMS)\n";
    std::cout << "    • E3 (Upstream Forwarder)   : Ishani G (CN & OS)\n";
    std::cout << "    • E4 (Query Execution VM)   : Joel Biju (OSPP)\n";
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << "  Server Configuration:\n";
    std::cout << "    • Listening On  : 127.0.0.1:" << port << " (UDP)\n";
    std::cout << "    • Upstream DNS  : " << upstream_ip << ":53 (Google Public DNS)\n";
    std::cout << "    • Fallback DNS  : 1.1.1.1:53 (Cloudflare Public DNS)\n";
    std::cout << "    • In-Memory LRU : " << capacity << " records capacity\n";
    std::cout << "    • Worker Pool   : " << threads << " OS worker threads\n";
    std::cout << "    • Durability DB : dns_cache.db\n";
    std::cout << "===========================================================================\n";
    std::cout << "  Ready for client queries! (Press Ctrl+C to terminate gracefully)\n";
    std::cout << "  Example test command:\n";
    std::cout << "    dig @127.0.0.1 -p " << port << " example.com A\n";
    std::cout << "===========================================================================\n\n";
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    uint16_t port = DEFAULT_SERVER_PORT;
    std::string upstream_ip = "8.8.8.8";
    size_t cache_capacity = 1000;
    size_t worker_threads = 4;
    std::string db_file = "dns_cache.db";
    bool warmup = true;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "-u" && i + 1 < argc) {
            upstream_ip = argv[++i];
        } else if (arg == "-c" && i + 1 < argc) {
            cache_capacity = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "-t" && i + 1 < argc) {
            worker_threads = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--no-warmup") {
            warmup = false;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: dns_server.exe [-p port] [-u upstream] [-c capacity] [-t threads] [--no-warmup]\n";
            return 0;
        }
    }

    // Register Win32 Console Ctrl Handler
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Initialize Winsock2
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[ERROR] WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

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
            std::cout << "[CACHE] Cache warm-up successful! Restored " << cache_engine.size()
                      << " active records from '" << db_file << "'.\n";
        }
    }

    // Create UDP Listening Socket
    SOCKET server_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_sock == INVALID_SOCKET) {
        std::cerr << "[ERROR] Failed to create UDP listening socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Bind Socket to 127.0.0.1:port
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server_sock, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Socket bind to 127.0.0.1:" << port << " failed: " << WSAGetLastError() << std::endl;
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    // Set Non-blocking mode on listen socket for responsive shutdown polling
    u_long non_blocking_mode = 1;
    ioctlsocket(server_sock, FIONBIO, &non_blocking_mode);

    // Launch Multi-Threaded Worker Pool
    WorkerThreadPool pool(worker_threads, vm_engine, server_sock);

    print_banner(port, upstream_ip, cache_capacity, worker_threads);

    auto last_telemetry_time = std::chrono::steady_clock::now();
    auto last_sweep_time = std::chrono::steady_clock::now();

    // Main Network Reactor Event Loop
    while (g_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_sock, &read_fds);

        // 100ms select timeout ensures smooth exit on Ctrl+C signal
        struct timeval tv = { 0, 100000 };
        int sel = select(0, &read_fds, NULL, NULL, &tv);

        if (sel > 0) {
            uint8_t buffer[DNS_MAX_PACKET_SIZE * 2];
            sockaddr_in client_addr;
            int client_len = sizeof(client_addr);

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
                std::cout << "[CACHE SWEEP] Proactively evicted " << purged
                          << " expired record(s) via Min-Heap. Active cache size: "
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
    std::cout << "[SERVER] Stopping worker threads...\n";
    pool.stop();

    std::cout << "[SERVER] Closing network socket...\n";
    closesocket(server_sock);

    // Save active cache snapshot to disk (durability persistence)
    std::cout << "[CACHE] Dumping valid cache entries to '" << db_file << "'...\n";
    if (cache_engine.save_snapshot(db_file)) {
        std::cout << "[CACHE] Snapshot saved successfully (" << cache_engine.size() << " records active).\n";
    }

    // Print Final Telemetry Report
    std::cout << telemetry_engine.generate_dashboard();

    WSACleanup();
    std::cout << "[SERVER] Graceful shutdown complete. Goodbye!\n";
    return 0;
}
