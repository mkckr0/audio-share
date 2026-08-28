"""
AudioStream UDP Discovery Beacon & Multi-Server Manager Simulation.
Simulates:
1. UDP port 59200 DiscoveryBeacon broadcasting
2. Multi-server LAN reception, parsing, deduplication, and active list updates
3. Server timeout and expiration pruning (6.0s stale server removal)
"""

from typing import Dict, List, Optional
from .protocol import DiscoveryBeacon


class ServerEntry:
    def __init__(self, beacon: DiscoveryBeacon, last_seen_ms: float):
        self.beacon = beacon
        self.last_seen_ms = last_seen_ms

    @property
    def key(self) -> str:
        return f"{self.beacon.ip_address}:{self.beacon.port}"


class DiscoveryBeaconListener:
    def __init__(self, timeout_ms: float = 6000.0):
        self.timeout_ms = timeout_ms
        self.servers: Dict[str, ServerEntry] = {}
        self.total_beacons_received = 0

    def receive_beacon_data(self, data: bytes, current_time_ms: float) -> Optional[ServerEntry]:
        """Parse raw UDP broadcast payload and update server table."""
        try:
            beacon = DiscoveryBeacon.deserialize(data)
        except Exception:
            return None
            
        self.total_beacons_received += 1
        key = f"{beacon.ip_address}:{beacon.port}"
        entry = ServerEntry(beacon, current_time_ms)
        self.servers[key] = entry
        return entry

    def prune_stale_servers(self, current_time_ms: float) -> List[str]:
        """Prune servers not seen within timeout_ms. Returns list of pruned keys."""
        pruned = []
        for key, entry in list(self.servers.items()):
            if current_time_ms - entry.last_seen_ms > self.timeout_ms:
                pruned.append(key)
                del self.servers[key]
        return pruned

    def get_active_servers(self) -> List[DiscoveryBeacon]:
        return [entry.beacon for entry in self.servers.values()]
