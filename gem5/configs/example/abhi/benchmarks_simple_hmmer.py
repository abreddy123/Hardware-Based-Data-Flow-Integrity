import m5
from m5.objects import *

data_dir = '/home/grads/a/abreddy/Thesis/SPEC_Data/'
spec_dir = '/home/grads/a/abreddy/Thesis/New_Gem5/gem5/configs/example/abhi/x86/bzip/'

system = System()

system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '2GHz'
system.clk_domain.voltage_domain = VoltageDomain()

#memchecker

system.memchecker = MemChecker()

system.checker = MemCheckerMonitor(memchecker = system.memchecker)

system.mem_mode = 'timing'
system.mem_ranges = [AddrRange('2GB')]

system.cpu = TimingSimpleCPU()

system.membus = SystemXBar()


system.cpu.dcache_port = system.checker.slave
system.checker.master = system.membus.slave


system.cpu.icache_port = system.membus.slave

system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.master
system.cpu.interrupts[0].int_master = system.membus.slave
system.cpu.interrupts[0].int_slave = system.membus.master

system.system_port = system.membus.slave

system.mem_ctrl = DDR3_1600_8x8()
system.mem_ctrl.range = system.mem_ranges[0]

system.mem_ctrl.port = system.membus.master

hmmer = Process()
hmmer.executable =  spec_dir + 'hmmer_ins_exec'
input_program0 = data_dir + '456.hmmer/data/ref/input/nph3.hmm'
input_program1 = data_dir + '456.hmmer/data/ref/input/swiss41'
print("Starting input.source")
hmmer.cmd = [hmmer.executable] + [input_program0, input_program1]
print("All Done")
system.cpu.workload = hmmer
system.cpu.createThreads()

root = Root(full_system = False,system=system)

m5.instantiate()

print("Begin simulation with hmmer workload, X86")

exit_event = m5.simulate()

print ("Exit @ tick %i because %s" % (m5.curTick(),exit_event.getCause()))
 