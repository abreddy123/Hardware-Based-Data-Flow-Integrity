#import all gem5 objects
import m5
from m5.objects import *     #imports all SimObjects
from m5.objects import Cache
from caches import *

#instantiate a system
system = System()

#init clock and voltage domain
#clk_domain is a parameter of thr System SimObject
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'   # Gem5 converts units
system.clk_domain.voltage_domain = VoltageDomain()

#Memory system set-up
system.mem_mode = 'timing'
# Use 'timing' for simulations, the options are atomic, functional

#add memory
system.mem_ranges = [AddrRange('1GB')]

#Create a CPU
system.cpu = TimingSimpleCPU()

#Create caches
system.cpu.icache = L1ICache()
system.cpu.dcache = L1DCache()

#Memory bus
system.membus = SystemXBar()

#Connect caches to CPU ports
system.cpu.icache.connectCPU(system.cpu)
system.cpu.dcache.connectCPU(system.cpu)

# Connect CPU to memory system
#system.cpu.icache_port = system.membus.slave
#system.cpu.dcache_port = system.membus.slave

#L1 cache to L2 cache thru bus

system.l2bus = L2XBar()

system.cpu.icache.connectBus(system.l2bus)
system.cpu.dcache.connectBus(system.l2bus)

#create L2 cache
system.l2cache = L2Cache()
system.l2cache.connectCPUSideBus(system.l2bus)

system.l2cache.connectMemSideBus(system.membus)

#To make things work
system.cpu.createInterruptController()
#system.cpu.interrupts[0].pio = system.membus.master
#system.cpu.interrupts[0].int_master = system.membus.slave
#system.cpu.interrupts[0].slave = system.membus.master

system.system_port = system.membus.slave

#Mem controller
system.mem_ctrl = DDR3_1600_8x8()

#Set up physical memory ranges
system.mem_ctrl.range = system.mem_ranges[0]

#Mem to bus connection
system.mem_ctrl.port = system.membus.master

#Tell system what to do 
process = Process()
process.cmd = ['tests/test-progs/hello/bin/riscv/linux/hello']
system.cpu.workload = process
system.cpu.createThreads()

#create a root object
root = Root(full_system = False, system=system)

#Instantiate all of the C++ 
m5.instantiate()

#Run
print ("Beginning Simulation for RISCV")

exit_event = m5.simulate()

print ("Exiting @ tick %i because %s" % (m5.curTick(),exit_event.getCause()))

















