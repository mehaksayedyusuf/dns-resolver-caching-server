#!/usr/bin/env bash
# ==============================================================================
# VM Cluster Node Deployer Script for VMware Workstation Pro
# Run this script inside each Ubuntu/Debian Linux Virtual Machine
# Usage:
#   ./scripts/deploy_vm_cluster.sh [cluster_id] [port] [threads]
# Examples:
#   ./scripts/deploy_vm_cluster.sh CLUSTER-ALPHA 8053 4
#   ./scripts/deploy_vm_cluster.sh CLUSTER-BETA 8053 4
# ==============================================================================

set -e

CLUSTER_ID=${1:-"CLUSTER-ALPHA"}
PORT=${2:-8053}
THREADS=${3:-4}
BIND_IP="0.0.0.0"

echo "======================================================================"
echo "    DNS RESOLVER VM CLUSTER DEPLOYER (VMware Workstation Pro)         "
echo "======================================================================"
echo "  Deploying Node  : ${CLUSTER_ID}"
echo "  Listening Port  : ${PORT} (UDP)"
echo "  Worker Threads  : ${THREADS}"
echo "======================================================================"

# 1. Check Compiler & Build Tools
if ! command -v g++ &> /dev/null || ! command -v make &> /dev/null; then
    echo "[DEPLOYER] g++ or make not found. Installing build-essential..."
    sudo apt-get update && sudo apt-get install -y build-essential
fi

# 2. Build the DNS Server Binary
echo "[DEPLOYER] Compiling DNS server binary using Linux make..."
mkdir -p bin
make clean
make all

if [ ! -f "bin/dns_server" ]; then
    echo "[ERROR] Compilation failed! Binary 'bin/dns_server' not found."
    exit 1
fi

# 3. Detect and Display Node IP
IP_ADDR=$(hostname -I | awk '{print $1}')
echo "======================================================================"
echo "  Compilation Successful!"
echo "  VM IP Address   : ${IP_ADDR}"
echo "  Client Query Cmd: dig @${IP_ADDR} -p ${PORT} example.com A"
echo "======================================================================"
echo "[DEPLOYER] Starting ${CLUSTER_ID} on ${BIND_IP}:${PORT}..."

# 4. Execute Server
exec ./bin/dns_server -b "${BIND_IP}" -p "${PORT}" --cluster-id "${CLUSTER_ID}" -t "${THREADS}"
