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
spec_dir = '/home/grads/a/abreddy/Thesis/Benchmarks/bzip2/'

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

bzip2 = Process()
bzip2.executable =  spec_dir + 'bzip2_ins_exec'
#input_program0 = data_dir + '401.bzip2/data/ref/input/input.source'
#input_program1 = data_dir + '401.bzip2/data/ref/input/chicken.jpg'
input_program2 = data_dir + '401.bzip2/data/ref/input/liberty.jpg'
#input_program3 = data_dir + '401.bzip2/data/ref/input/text.html'
#input_program4 = data_dir + '401.bzip2/data/all/input/input.program'
#input_program5 = data_dir + '401.bzip2/data/all/input/input.combined'
#print("Starting input.source")
#bzip2.cmd = [bzip2.executable] + [input_program0, '280']
#print("Starting chicken.jpg")
#bzip2.cmd = [bzip2.executable] + [input_program1, '30']
print("Starting liberty.jpg")
bzip2.cmd = [bzip2.executable] + [input_program2, '30']
#print("Starting text.html")
#bzip2.cmd = [bzip2.executable] + [input_program3, '280']
#print("Starting input.program")
#bzip2.cmd = [bzip2.executable] + [input_program4, '280']
#print("Starting input.combined")
#bzip2.cmd = [bzip2.executable] + [input_program5, '200']
print("All Done")
#bzip2.cmd = ['configs/example/abhi/x86/bzip/bzip2_ins_exec']
system.cpu.workload = bzip2
system.cpu.createThreads()

root = Root(full_system = False,system=system)

m5.instantiate()

print("Begin simulation with add workload, X86")

exit_event = m5.simulate()

print ("Exit @ tick %i because %s" % (m5.curTick(),exit_event.getCause()))
 
