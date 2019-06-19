import subprocess

subprocess.call(['cp','base.cc','base_mod.cc'])
subprocess.call(['cp','base.hh','base_mod.hh'])
subprocess.call(['cp','timing.cc','timing_mod.cc'])

subprocess.call(['cp','base_orig.cc','base.cc'])
subprocess.call(['cp','base_orig.hh','base.hh'])
subprocess.call(['cp','timing_orig.cc','timing.cc'])

