# encoding: utf-8
# force_32bit.py -- force compiler to create 32-bit code
# Copyright (C) 2018 a1batross
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

try: from fwgslib import get_flags_by_compiler
except: from waflib.extras.fwgslib import get_flags_by_compiler
from waflib import Configure

# Input:
#   BIT32_MANDATORY(optional) -- fail if 32bit mode not available
# Output:
#   DEST_SIZEOF_VOID_P -- an integer, equals sizeof(void*) on target

@Configure.conf
def check_32bit(ctx, *k, **kw):
	if not 'msg' in kw:
		kw['msg'] = 'Checking if \'%s\' can target 32-bit' % ctx.env.COMPILER_CC
	
	if not 'mandatory' in kw:
		kw['mandatory'] = False
	
	return ctx.check_cc( fragment='int main(void){int check[sizeof(void*)==4?1:-1];return 0;}', *k, **kw)

def configure(conf):
	flags = ['-m32'] if not conf.env.DEST_OS == 'darwin' else ['-arch', 'i386']
	
	if conf.check_32bit():
		conf.env.DEST_SIZEOF_VOID_P = 4
	elif conf.env.BIT32_MANDATORY and conf.check_32bit(msg = '...trying with additional flags', cflags = flags, linkflags = flags):
		conf.env.LINKFLAGS += flags
		conf.env.CXXFLAGS += flags
		conf.env.CFLAGS += flags
		conf.env.DEST_SIZEOF_VOID_P = 4
	else:
		conf.env.DEST_SIZEOF_VOID_P = 8
		
		if conf.env.BIT32_MANDATORY:
			conf.fatal('Compiler can\'t create 32-bit code!')
