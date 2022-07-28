# encoding: utf-8
# masm.py -- Microsoft Macro Assembler file support

from waflib import Errors, Logs, Task
from waflib.TaskGen import extension

class masm(Task.Task):
	"""
	Compiles assembler files with masm
	"""
	color = 'BLUE'
	run_str = '${AS} ${ASFLAGS} ${ASMPATH_ST:INCPATHS} ${ASMDEFINES_ST:DEFINES} ${AS_TGT_F}${TGT} ${AS_SRC_F}${SRC}'

@extension('.masm')
def create_masm_task(self, node):
	return self.create_compiled_task('masm', node)

def configure(conf):
	conf.env.AS = conf.env.CC[0].replace('CL.exe', 'ml64.exe')
	conf.env.ASFLAGS = ['/nologo', '/c']
	conf.env.AS_SRC_F = ['']
	conf.env.AS_TGT_F = ['/Fo']