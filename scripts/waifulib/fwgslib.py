# encoding: utf-8
# fwgslib.py -- utils for Waifu build system(Waf with extensions) by FWGS
# Copyright (C) 2018 a1batross, Michel Mooij (michel.mooij
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import os
from waflib import Utils, Errors, Configure, Build

def get_flags_by_compiler(flags, compiler, major_version=None):
	'''Returns a list of compile flags, depending on compiler

	:param flags: compiler flags
	:type flags: dict
	:param compiler: compiler string(COMPILER_CC, for example)
	:type compiler: string
	:returns: list of flags
	'''
	out = []
	if compiler in flags:
		f = flags[compiler]
		if type(f) is list:
			out += f
		elif type(f) is dict:
			if major_version in f:
				out += f[major_version]
			elif 'default' in flags:
				out += f['default']
		else:
			raise TypeError('unknown type, expected list or dict, got %s' % type(f))
	elif 'default' in flags:
		out += flags['default']
	return out

def get_flags_by_type(flags, type, compiler, major_version=None):
	'''Returns a list of compile flags, depending on compiler and build type

	:param flags: compiler flags
	:param type: build type
	:type type: string
	:param compiler: compiler string(COMPILER_CC, for example)
	:type compiler: string
	:returns: list of flags
	'''
	out = []
	if 'common' in flags:
		out += get_flags_by_compiler(flags['common'], compiler, major_version)
	if type in flags:
		out += get_flags_by_compiler(flags[type], compiler, major_version)
	return out

@Configure.conf
def filter_flags(conf, flags, required_flags, features, checkarg, compiler):
	
	check_flags = required_flags + (['-Werror'] if compiler != 'msvc' else [])
	tests = map(lambda x: {
		'features': features,
		'compiler' : compiler,
		'msg': '... ' + x,
		'define_name': Utils.quote_define_name(x),
		checkarg: [x] + check_flags }, flags )
	
	conf.env.stash()
	conf.multicheck(*tests,
		msg = 'Checking supported flags for %s in parallel' % compiler,
		mandatory = False)
	supported_flags = [ f for f in flags if conf.env[Utils.quote_define_name(f)] ]
	conf.env.revert()
	return supported_flags

@Configure.conf
def filter_cflags(conf, flags, required_flags = []):
	return conf.filter_flags(flags, required_flags, 'c', 'cflags', conf.env.COMPILER_CC)

@Configure.conf
def filter_cxxflags(conf, flags, required_flags = []):
	return conf.filter_flags(flags, required_flags, 'cxx', 'cxxflags', conf.env.COMPILER_CXX)

def conf_get_flags_by_compiler(unused, flags, compiler, major_version=None):
	return get_flags_by_compiler(flags, compiler, major_version)

setattr(Configure.ConfigurationContext, 'get_flags_by_compiler', conf_get_flags_by_compiler)
setattr(Build.BuildContext, 'get_flags_by_compiler', conf_get_flags_by_compiler)

def conf_get_flags_by_type(unused, flags, type, compiler, major_version=None):
	return get_flags_by_type(flags, type, compiler, major_version)

setattr(Configure.ConfigurationContext, 'get_flags_by_type', conf_get_flags_by_type)
setattr(Build.BuildContext, 'get_flags_by_type', conf_get_flags_by_type)

def get_deps(bld, target):
	'''Returns a list of (nested) targets on which this target depends.

	:param bld: a *waf* build instance from the top level *wscript*
	:type bld: waflib.Build.BuildContext
	:param target: task name for which the dependencies should be returned
	:type target: str
	:returns: a list of task names on which the given target depends
	'''
	try:
		tgen = bld.get_tgen_by_name(target)
	except Errors.WafError:
		return []
	else:
		uses = Utils.to_list(getattr(tgen, 'use', []))
		deps = uses[:]
		for use in uses:
			deps += get_deps(bld, use)
		return list(set(deps))


def get_tgens(bld, names):
	'''Returns a list of task generators based on the given list of task
	generator names.

	:param bld: a *waf* build instance from the top level *wscript*
	:type bld: waflib.Build.BuildContext
	:param names: list of task generator names
	:type names: list of str
	:returns: list of task generators
	'''
	tgens=[]
	for name in names:
		try:
			tgen = bld.get_tgen_by_name(name)
		except Errors.WafError:
			pass
		else:
			tgens.append(tgen)
	return list(set(tgens))


def get_targets(bld):
	'''Returns a list of user specified build targets or None if no specific
	build targets has been selected using the *--targets=* command line option.

	:param bld: a *waf* build instance from the top level *wscript*.
	:type bld: waflib.Build.BuildContext
	:returns: a list of user specified target names (using --targets=x,y,z) or None
	'''
	if bld.targets == '':
		return None
	targets = bld.targets.split(',')
	for target in targets:
		targets += get_deps(bld, target)
	return targets
