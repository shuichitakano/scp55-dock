"""Single removal latch; firmware owns voltage settling and RESET sequencing."""
from dataclasses import dataclass
@dataclass
class Inputs:
    master_clear_n: bool = False
    cd1_low: bool = False
    cd2_low: bool = False
    fault_n: bool = True
    power_clock: bool = False
    bus_request: bool = False
    data_request: bool = False
@dataclass
class Outputs:
    power_enable: bool
    bus_enable: bool
    data_enable: bool
class Interlock:
    def __init__(self):
        self.power_armed = False
        self.previous_power_clock = False
    def step(self, p: Inputs) -> Outputs:
        clear_n = p.master_clear_n and p.cd1_low and p.cd2_low and p.fault_n
        if not clear_n: self.power_armed = False
        elif p.power_clock and not self.previous_power_clock: self.power_armed = True
        self.previous_power_clock = p.power_clock
        bus = self.power_armed and p.bus_request
        return Outputs(self.power_armed, bus, bus and p.data_request)
