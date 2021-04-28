#! /usr/bin/env python
# Modified: Alibek Omarov <a1ba.omarov@gmail.com>
# Originally taken from Thomas Nagy's blogpost

"""
Strip executables upon installation
"""

import shutil, os
from waflib import Build, Utils, Context, Errors, Logs

def options(opt):
	grp = opt.option_groups['install/uninstall options']
	grp.add_option('--strip', dest='strip', action='store_true', default=False,
		help='strip binaries. You must pass this flag to install command [default: %default]')
	
	grp.add_option('--strip-to-file', dest='strip_to_file', action='store_true', default=False,
		help='strip debug information to file *.debug. Implies --strip. You must pass this flag to install command [default: %default]')

def configure(conf):
	if conf.env.DEST_BINFMT in ['elf', 'mac-o']:
		conf.find_program('strip', var='STRIP')
		if not conf.env.STRIPFLAGS:
			conf.env.STRIPFLAGS = os.environ['STRIPFLAGS'] if 'STRIPFLAGS' in os.environ else []
		
		# a1ba: I am lazy to add `export OBJCOPY=` everywhere in my scripts
		# so just try to deduce which objcopy we have
		try:
			k = conf.env.STRIP[0].rfind('-')
			if k >= 0:
				objcopy_name = conf.env.STRIP[0][:k] + '-objcopy'
				try: conf.find_program(objcopy_name, var='OBJCOPY')
				except: conf.find_program('objcopy', var='OBJCOPY')
			else:
				conf.find_program('objcopy', var='OBJCOPY')
		except:
			conf.find_program('objcopy', var='OBJCOPY')

def copy_fun(self, src, tgt):
	inst_copy_fun(self, src, tgt)

	if not self.generator.bld.options.strip and not self.generator.bld.options.strip_to_file:
		return

	if self.env.DEST_BINFMT not in ['elf', 'mac-o']: # don't strip unknown formats or PE
		return

	if getattr(self.generator, 'link_task', None) and self.generator.link_task.outputs[0] in self.inputs:
		tgt_debug = tgt + '.debug'
		strip_cmd = self.env.STRIP + self.env.STRIPFLAGS + [tgt]
		ocopy_cmd = self.env.OBJCOPY + ['--only-keep-debug', tgt, tgt_debug]
		ocopy_debuglink_cmd = self.env.OBJCOPY + ['--add-gnu-debuglink=%s' % tgt_debug, tgt]
		c1 = Logs.colors.NORMAL
		c2 = Logs.colors.CYAN
		c3 = Logs.colors.YELLOW
		c4 = Logs.colors.BLUE
		try:
			if self.generator.bld.options.strip_to_file:
				self.generator.bld.cmd_and_log(ocopy_cmd, output=Context.BOTH, quiet=Context.BOTH)
				if not self.generator.bld.progress_bar:
					Logs.info('%s+ objcopy --only-keep-debug %s%s%s %s%s%s', c1, c4, tgt, c1, c3, tgt_debug, c1)
			
			self.generator.bld.cmd_and_log(strip_cmd, output=Context.BOTH, quiet=Context.BOTH)
			if not self.generator.bld.progress_bar:
				f1 = os.path.getsize(src)
				f2 = os.path.getsize(tgt)
				Logs.info('%s+ strip %s%s%s (%d bytes change)', c1, c2, tgt, c1, f2 - f1)
				
			if self.generator.bld.options.strip_to_file:
				self.generator.bld.cmd_and_log(ocopy_debuglink_cmd, output=Context.BOTH, quiet=Context.BOTH)
				if not self.generator.bld.progress_bar:
					Logs.info('%s+ objcopy --add-gnu-debuglink=%s%s%s %s%s%s', c1, c3, tgt_debug, c1, c2, tgt, c1)
		except Errors.WafError as e:
			print(e.stdout, e.stderr)

inst_copy_fun = Build.inst.copy_fun
Build.inst.copy_fun = copy_fun

