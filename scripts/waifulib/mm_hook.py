from waflib import TaskGen
@TaskGen.extension('.mm')
def mm_hook(self, node):
	"""Alias .mm files to be compiled the same as .cpp files, gcc will do the right thing."""
	return self.create_compiled_task('cxx', node)