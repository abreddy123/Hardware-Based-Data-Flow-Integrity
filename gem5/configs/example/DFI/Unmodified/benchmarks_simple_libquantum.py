from __future__ import print_function

import optparse
import sys
import os

import m5
from m5.defines import buildEnv
from m5.objects import *
from m5.util import *

addToPath('../../../')
from common import Options
from common import Simulation

parser = optparse.OptionParser()
Options.addCommonOptions(parser)
Options.addSEOptions(parser)
(options, args) = parser.parse_args()

data_dir = '/home/grads/a/abreddy/Thesis/SPEC_Data/'
spec_dir = '/home/grads/a/abreddy/Thesis/DFI_scripts_configs/Unmodified/Binaries/'

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

if options.maxinsts:
     system.cpu.max_insts_any_thread = options.maxinsts

libquantum = Process()
libquantum.executable =  spec_dir + 'libquantum'
print("Starting input.source")
libquantum.cmd = [libquantum.executable] + ['1397','8']
#libquantum.input = data_dir + '462.libquantum/data/ref/input/13x13.tst'
#libquantum.cmd = [libquantum.executable] + ['-d', input_program0]
print("All Done")
system.cpu.workload = libquantum
system.cpu.createThreads()

root = Root(full_system = False,system=system)

m5.instantiate()

print("Begin simulation with libquantum workload, X86")

exit_event = m5.simulate()

print ("Exit @ tick %i because %s" % (m5.curTick(),exit_event.getCause()))
 
