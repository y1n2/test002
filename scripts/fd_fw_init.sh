#!/usr/bin/env bash
# fd_fw_init.sh
# Initialize ipset collections and basic iptables rules used by MAGIC dataplane

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# Default values (can be overridden by env)
MAGIC_CONTROL=${MAGIC_CONTROL:-magic_control}
MAGIC_DATA=${MAGIC_DATA:-magic_data}
SERVER_IP=${SERVER_IP:-192.168.126.1}
SEC_PORT=${SEC_PORT:-5870}

echo "[fd_fw_init] Ensuring ipset collections and iptables rules (server=${SERVER_IP} port=${SEC_PORT})"

ensure_ipset() {
  local name=$1
  if ! ipset list "$name" >/dev/null 2>&1; then
    echo "[fd_fw_init] creating ipset $name"
    sudo ipset create "$name" hash:ip || true
  else
    echo "[fd_fw_init] ipset $name exists"
  fi
}

ensure_iptables_rule() {
  local chain=$1; shift
  local rule_spec=("$@")
  # test (-C) may not be supported on very old iptables, tolerate failures
  if sudo iptables -C "$chain" "${rule_spec[@]}" >/dev/null 2>&1; then
    echo "[fd_fw_init] rule exists in $chain: ${rule_spec[*]}"
  else
    echo "[fd_fw_init] inserting rule into $chain: ${rule_spec[*]}"
    sudo iptables -I "$chain" 1 "${rule_spec[@]}"
  fi
}

main() {
  ensure_ipset "$MAGIC_CONTROL"
  ensure_ipset "$MAGIC_DATA"

  # Cleanup any leftover dataplane state from previous runs:
  # - remove conntrack entries for members of ipsets
  # - flush ipset members
  # - remove ip rules pointing to blackhole table (99)
  # - ensure blackhole default exists in table 99
  echo "[fd_fw_init] cleaning leftover dataplane state..."
  for set in "$MAGIC_CONTROL" "$MAGIC_DATA"; do
    if ipset list "$set" >/dev/null 2>&1; then
      # enumerate members
      members=$(sudo ipset save "$set" 2>/dev/null | awk '/^add/ {print $3}') || members=""
      for ip in $members; do
        echo "[fd_fw_init] removing conntrack for $ip"
        sudo conntrack -D -s "$ip" 2>/dev/null || true
        sudo conntrack -D -d "$ip" 2>/dev/null || true
      done
      echo "[fd_fw_init] flushing ipset $set"
      sudo ipset flush "$set" 2>/dev/null || true
    fi
  done

  # remove ip rules that lookup table 99 (blackhole) for any leftover clients
  for ip in $(ip rule show | awk '/lookup 99/ {for(i=1;i<=NF;i++) if($i=="from") print $(i+1)}'); do
    if [ -n "$ip" ]; then
      echo "[fd_fw_init] deleting ip rule from $ip lookup 99"
      sudo ip rule del from "$ip" lookup 99 2>/dev/null || true
    fi
  done

  # ensure blackhole default exists in table 99
  if ! ip route show table 99 | grep -q 'blackhole' >/dev/null 2>&1; then
    echo "[fd_fw_init] adding blackhole default to table 99"
    sudo ip route add blackhole default table 99 2>/dev/null || true
  fi
  echo "[fd_fw_init] dataplane cleanup complete"

  # Ensure ipset-based FORWARD accept exists for data (business traffic) only.
  # Do NOT create a FORWARD accept for control set: control-plane is handled locally (INPUT/OUTPUT).
  ensure_iptables_rule FORWARD -m set --match-set "$MAGIC_DATA" src -j ACCEPT
  # Allow established/related forwarding to flow back
  ensure_iptables_rule FORWARD -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
  # Allow OUTPUT for clients present in magic_data (business responses) and allow
  # server control responses via OUTPUT (see control rules below)
  ensure_iptables_rule OUTPUT -m set --match-set "$MAGIC_DATA" src -j ACCEPT

  # Add precise server-side ACCEPT for TLS control port (INPUT)
  ensure_iptables_rule INPUT -d "$SERVER_IP" -p tcp --dport "$SEC_PORT" -j ACCEPT

  # Prevent clients in the client subnet from sending business traffic to local link gateways
  # (packets destined to gateway IPs are delivered to local INPUT chain, so we must DROP them)
  CLIENT_NET=${CLIENT_NET:-192.168.126.0/24}
  # Drop all ICMP from client subnet to link gateways
  # Gateway 10.1.1.1 (CELLULAR) additionally blocked
  ensure_iptables_rule INPUT -s "$CLIENT_NET" -d 10.1.1.1 -p icmp -j DROP
  ensure_iptables_rule INPUT -s "$CLIENT_NET" -d 10.2.2.2 -p icmp -j DROP
  ensure_iptables_rule INPUT -s "$CLIENT_NET" -d 10.3.3.3 -p icmp -j DROP
  # Drop non-control TCP/UDP to link gateways from client subnet
  ensure_iptables_rule INPUT -s "$CLIENT_NET" -d 10.1.1.1 -p tcp -j DROP
  ensure_iptables_rule INPUT -s "$CLIENT_NET" -d 10.1.1.1 -p udp -j DROP
  ensure_iptables_rule INPUT -s "$CLIENT_NET" -d 10.2.2.2 -p tcp -j DROP
  ensure_iptables_rule INPUT -s "$CLIENT_NET" -d 10.3.3.3 -p tcp -j DROP
  ensure_iptables_rule INPUT -s "$CLIENT_NET" -d 10.2.2.2 -p udp -j DROP
  ensure_iptables_rule INPUT -s "$CLIENT_NET" -d 10.3.3.3 -p udp -j DROP

  # Ensure control-plane is only delivered locally to the server process (INPUT/OUTPUT)
  # and not forwarded through this host. To guarantee that, add a specific DROP for
  # forwarded control-plane packets (destination server:SEC_PORT). We then append a
  # default FORWARD DROP to deny forwarding by default; ACCEPT rules above match
  # ipset/established cases and are inserted at the front.
  # Ensure forwarded control-plane packets are explicitly dropped (do not forward control)
  ensure_iptables_rule FORWARD -d "$SERVER_IP" -p tcp --dport "$SEC_PORT" -j DROP
  # Allow server-originated control responses (OUTPUT) so replies are delivered locally
  ensure_iptables_rule OUTPUT -s "$SERVER_IP" -p tcp --sport "$SEC_PORT" -j ACCEPT

  # Ensure control-plane is allowed on local OUTPUT to reach clients (responses)
  ensure_iptables_rule OUTPUT -p tcp --dport "$SEC_PORT" -j ACCEPT || true

  # Set FORWARD chain default policy to DROP so forwarding is denied by default.
  echo "[fd_fw_init] setting FORWARD policy to DROP"
  sudo iptables -P FORWARD DROP || true

  # Flush ipset entries to ensure no client has data whitelist at startup
  echo "[fd_fw_init] flushing ipset entries for $MAGIC_CONTROL and $MAGIC_DATA"
  sudo ipset flush "$MAGIC_CONTROL" || true
  sudo ipset flush "$MAGIC_DATA" || true

  echo "[fd_fw_init] done"
}

main "$@"
