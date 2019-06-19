import m5
from m5.objects import *

system = System()
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'
system.clk_domain.voltage_domain = VoltageDomain()
system.mem_mode = 'timing'
system.mem_ranges = [AddrRange('1GB')]

system.cpu = TimingSimpleCPU()
system.memobj = SimpleMemobj()
system.cpu.icache_port = system.memobj.inst_port
system.cpu.dcache_port = system.memobj.data_port

#system.memrep = DFI_replicator()
#system.memobj.mem_side = system.memrep.sport

#system.membus_repl = SystemXBar()
#system.memobj.mem_side = system.membus_repl.slave

system.membus = SystemXBar()

system.memobj.mem_side = system.membus.slave
#system.memrep.tport = system.membus.slave

#system.membus_repl.master = system.membus.slave

#system.memrev = DFI_rev()

#system.memrep.rport = system.memrev.sport
#system.memobj.mem_side = system.membus.slave

#system.memrev.sport = system.membus.master

system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.master
system.cpu.interrupts[0].int_master = system.membus.slave
system.cpu.interrupts[0].int_slave = system.membus.master

system.mem_ctrl = DDR3_1600_8x8()
system.mem_ctrl.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.master

#system.membus.master = system.memrev.sport

system.system_port = system.membus.slave

process = Process()
#process.cmd = ['tests/test-progs/hello/bin/x86/linux/hello']
process.cmd = ['configs/example/abhi/x86/add']
system.cpu.workload = process
system.cpu.createThreads()

root = Root(full_system = False, system = system)
print('begin instantiate')
m5.instantiate()

print ("Beginning simulation!")
exit_event = m5.simulate()
print ('Exiting @ tick %i because %s' % (m5.curTick(), exit_event.getCause()))
