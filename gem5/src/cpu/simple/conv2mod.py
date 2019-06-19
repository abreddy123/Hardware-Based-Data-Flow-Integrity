import subprocess

subprocess.call(['cp','base.cc','base_orig.cc'])
subprocess.call(['cp','base.hh','base_orig.hh'])
subprocess.call(['cp','timing.cc','timing_orig.cc'])

subprocess.call(['cp','base_mod.cc','base.cc'])
subprocess.call(['cp','base_mod.hh','base.hh'])
subprocess.call(['cp','timing_mod.cc','timing.cc'])

