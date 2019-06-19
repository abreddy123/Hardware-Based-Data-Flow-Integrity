import m5
from m5.objects import *

system = System()

system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'
system.clk_domain.voltage_domain = VoltageDomain()

#memchecker

system.memchecker = MemChecker()

system.checker = MemCheckerMonitor(memchecker = system.memchecker)

system.mem_mode = 'timing'
system.mem_ranges = [AddrRange('1GB')]

system.cpu = TimingSimpleCPU()

system.membus = SystemXBar()

#system.cpu.monitor = CommMonitor()
#system.cpu.dcache_port = system.cpu.monitor.slave
#system.cpu.monitor.master = system.membus.slave
#system.cpu.monitor.trace = MemTraceProbe(trace_file="my_trace.trc.gz")

system.cpu.dcache_port = system.checker.slave
system.checker.master = system.membus.slave
#system.cpu.monitor.master = system.membus.slave


system.cpu.icache_port = system.membus.slave
#system.cpu.dcache_port = system.membus.slave

system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.master
system.cpu.interrupts[0].int_master = system.membus.slave
system.cpu.interrupts[0].int_slave = system.membus.master

system.system_port = system.membus.slave

system.mem_ctrl = DDR3_1600_8x8()
system.mem_ctrl.range = system.mem_ranges[0]

system.mem_ctrl.port = system.membus.master

process = Process()
#process.cmd = ['configs/example/abhi/add']
#process.cmd = ['configs/example/abhi/x86/add1']
#process.cmd = ['configs/example/abhi/x86/rw_test']
#process.cmd = ['configs/example/abhi/x86/read_write2']
#process.cmd = ['configs/example/abhi/x86/read_write1']
process.cmd = ['configs/example/abhi/x86/bzip/bzip2_ins_exec']
#process.cmd = ['tests/test-progs/hello/bin/arm/linux/hello']
system.cpu.workload = process
system.cpu.createThreads()

root = Root(full_system = False,system=system)

m5.instantiate()

print("Begin simulation with add workload, ARM")

exit_event = m5.simulate()

print ("Exit @ tick %i because %s" % (m5.curTick(),exit_event.getCause()))
 
