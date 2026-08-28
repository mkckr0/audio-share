"""
AudioStream Virtual Mic Monitor & On-Demand Lifecycle Simulation.
Simulates:
1. Windows IAudioSessionManager2 session activation / deactivation on VB-CABLE Output
2. Debounce timer logic (e.g., 3.0s delay after last app releases mic before sending CMD_STOP_MIC)
3. Android Privacy Indicator (Green Dot) lifecycle synchronization
"""

from enum import Enum
from typing import List, Optional, Callable


class MicState(Enum):
    IDLE = "IDLE"
    ACTIVE = "ACTIVE"
    DEBOUNCING = "DEBOUNCING"


class VirtualMicMonitorSimulator:
    def __init__(self, debounce_ms: float = 3000.0):
        self.debounce_ms = debounce_ms
        self.state = MicState.IDLE
        self.active_apps: List[str] = []
        self.debounce_start_ms: Optional[float] = None
        self.cmd_start_sent_count = 0
        self.cmd_stop_sent_count = 0
        self.android_green_dot_active = False

    def app_open_mic(self, app_name: str, current_time_ms: float) -> bool:
        """
        An application (e.g. Discord, Zoom, Game) opened the virtual mic device.
        Returns True if state transitioned to ACTIVE (triggering CMD_START_MIC).
        """
        if app_name not in self.active_apps:
            self.active_apps.append(app_name)

        if self.state == MicState.IDLE:
            self.state = MicState.ACTIVE
            self.cmd_start_sent_count += 1
            self.android_green_dot_active = True
            self.debounce_start_ms = None
            return True
        elif self.state == MicState.DEBOUNCING:
            # Re-activated during debounce period -> cancel debounce
            self.state = MicState.ACTIVE
            self.debounce_start_ms = None
            return False
        return False

    def app_close_mic(self, app_name: str, current_time_ms: float):
        """An application closed its recording session on the virtual mic."""
        if app_name in self.active_apps:
            self.active_apps.remove(app_name)

        if len(self.active_apps) == 0 and self.state == MicState.ACTIVE:
            # Enter debounce wait
            self.state = MicState.DEBOUNCING
            self.debounce_start_ms = current_time_ms

    def advance_time(self, current_time_ms: float) -> bool:
        """
        Advance monitor clock.
        Returns True if debounce timer expired and CMD_STOP_MIC was triggered.
        """
        if self.state == MicState.DEBOUNCING and self.debounce_start_ms is not None:
            if current_time_ms - self.debounce_start_ms >= self.debounce_ms:
                self.state = MicState.IDLE
                self.debounce_start_ms = None
                self.cmd_stop_sent_count += 1
                self.android_green_dot_active = False
                return True
        return False
