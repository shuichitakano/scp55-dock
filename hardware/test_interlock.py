"""Removal / recontact and all input combinations, including stuck requests."""
from dataclasses import fields
from itertools import product
from interlock_model import Inputs, Interlock
for cause in ('cd1_low','cd2_low','fault_n','master_clear_n'):
    g=Interlock();p=Inputs(True,True,True,True,False,True,True)
    assert not g.step(p).power_enable
    p.power_clock=True;assert g.step(p).data_enable
    setattr(p,cause,False);o=g.step(p)
    assert not (o.power_enable or o.bus_enable or o.data_enable)
    setattr(p,cause,True)
    assert not g.step(p).power_enable # Contact restoration cannot rearm.
    p.power_clock=False;g.step(p)
    p.power_clock=True;assert g.step(p).data_enable
for bits in product((False,True),repeat=len(fields(Inputs))):
    p=Inputs(*bits)
    for old,clock in product((False,True),repeat=2):
        g=Interlock();g.power_armed=old;g.previous_power_clock=clock;o=g.step(p)
        if not(p.master_clear_n and p.cd1_low and p.cd2_low and p.fault_n):
            assert not(o.power_enable or o.bus_enable or o.data_enable)
        assert not o.bus_enable or (o.power_enable and p.bus_request)
        assert not o.data_enable or (o.bus_enable and p.data_request)
print('Single-latch removal and recontact checks passed.')
