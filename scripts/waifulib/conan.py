# encoding: utf-8
# conan.py -- Conan Package Manager integration
# Copyright (C) 2019 a1batross
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

from waflib import Logs, Utils
from waflib.Configure import conf
import os
import sys
import subprocess
import json

def options(opt):
	grp = opt.add_option_group('Conan options')
	
	grp.add_option('--disable-conan', action = 'store_true', default = False, dest = 'NO_CONAN',
		help = 'completely disable Conan')
		
	grp.add_option('--force-conan', action = 'store_true', default = False, dest = 'CONAN_MANDATORY',
		help = 'require Conan, useful for testing')
		
	grp.add_option('--conan-profile', action = 'store', default = None, dest = 'CONAN_PROFILE',
		help = 'set conan profile')
	
	# grp.add_option('')

def conan(ctx, args, quiet=False):
	# print('%s %s' % (ctx.env.CONAN[0], arg_str))
	argv = [ctx.env.CONAN[0]]
	argv += Utils.to_list(args)
	ret = b''
	retval = False
	
	ctx.logger.info('argv: {}'.format(argv))
	if quiet:
		try:
			ret = subprocess.check_output(argv, cwd=ctx.bldnode.abspath())
			ctx.logger.info('output: \n{}'.format(ret))
		except subprocess.CalledProcessError as e:
			ret = e.output
			ctx.logger.info('FAIL!!!\nretval: {}\noutput: \n{}'.format(e.returncode, ret))
		else:
			retval = True
	else:
		retval = subprocess.call(argv, cwd=ctx.bldnode.abspath())
		if retval != 0:
			ctx.logger.info('FAIL!!!\nretval: {}'.format(retval))
			retval = False
		else:
			retval = True
	
	if sys.version_info > (3, 0):
		ret = ret.decode('utf-8')
	ret = ret.strip().replace('\r\n', '\n')
	
	return (retval, ret)

@conf
def add_conan_remote(ctx, name, url):
	"""
	Adds conan remote
	
	:param name: name of remote
	:type name: string
	:param url: url path
	:type url: string
	"""
	
	if not ctx.env.CONAN:
		ctx.fatal("Conan is not installed!")
	
	ctx.start_msg('Checking if conan has %s remote' % name)
	[success,remotes] = conan(ctx, 'remote list --raw', quiet=True)
	
	if not success:
		ctx.end_msg('no')
		ctx.fatal('conan has failed to list remotes')
		
	found = False
	for v in remotes.splitlines():
		a = v.split(' ')
		if a[0] == name:
			if a[1] == url:
				found = True
			else:
				ctx.end_msg('no')
				ctx.fatal('''Conan already has %s remote, but it points to another remote!
You can remote it with:\n
$ %s remote remove %s''' % (name, ctx.env.CONAN[0], name))
			break
	
	if not found:
		ctx.end_msg('no')
		ctx.start_msg('Adding new %s remote to conan' % name)
		if conan(ctx, 'remote add %s %s' % (name, url), quiet=True) == False:
			ctx.end_msg('fail', color='RED')
			ctx.fatal('conan has failed to add %s remote' % name)
		ctx.end_msg('done')
	else:
		ctx.end_msg('yes')

@conf
def parse_conan_json(ctx, name):
	with open(os.path.join(ctx.bldnode.abspath(), 'conanbuildinfo.json')) as jsonfile:
		cfg = json.load(jsonfile)
		
		deps = cfg["dependencies"]
		
		ctx.env['HAVE_%s' % name] = True
		
		for dep in deps:
			def to_u8(arr):
				if  sys.version_info > (3, 0):
					return arr
				ret = []
				for i in arr:
					ret += [ i.encode('utf8') ]
				return ret
		
			ctx.env['INCLUDES_%s' % name] += to_u8(dep['include_paths'])
			ctx.env['LIB_%s' % name] += to_u8(dep['libs'])
			ctx.env['LIBPATH_%s' % name] += to_u8(dep['lib_paths'])
			ctx.env['DEFINES_%s' % name] += to_u8(dep['defines'])
			ctx.env['CFLAGS_%s' % name] += to_u8(dep['cflags'])
			ctx.env['CXXFLAGS_%s' % name] += to_u8(dep['cflags'])
			ctx.env['LINKFLAGS_%s' % name] += to_u8(dep['sharedlinkflags'])
	return


def conan_update_profile(ctx, settings, profile):
	args = ['profile', 'update']
	
	for (key, value) in settings.items():
		args2 = args + [ 'settings.%s=%s' % (key, value), profile ]
		if conan(ctx, args2, quiet=True) == False:
			ctx.fatal('Can\'t update profile')

@conf
def add_dependency(ctx, pkg, *k, **kw):
	"""
	Retrieves and adds depedency during configuration stage
	
	:param pkg: package name in conan format
	:type pkg: string
	:param remote: remote name, optional
	:type remote: string
	:param options: package options, optional
	:type options: dict
	:param uselib_store: set uselib name, optional
	:type uselib_store: string
	"""
	
	if not ctx.env.CONAN:
		ctx.fatal("Conan is not installed!")
	
	name = pkg.split('/')[0]
	ctx.msg(msg='Downloading dependency %s' % name, result='in process', color='BLUE')
	
	args = ['install', pkg, '-g', 'json', '--build=missing', '-pr', ctx.env.CONAN_PROFILE]
	if 'remote' in kw:
		args += ['-r', kw['remote']]
	
	if 'options' in kw:
		for (key, value) in kw['options'].items():
			args += ['-o', '%s=%s' % (key, value)]
	
	if conan(ctx, args):
		if 'uselib_store' in kw:
			uselib = kw['uselib_store']
		else: uselib = name.upper() # we just use upper names everywhere
		ctx.parse_conan_json(uselib)
		ctx.msg(msg='Downloading dependency %s' % name, result='ok', color='GREEN')
		return
	
	ctx.msg(msg='Downloading dependency %s' % name, result='fail', color='RED')
	ctx.fatal('Conan has failed installing dependency %s' % pkg)

def configure(conf):
	# already configured
	if conf.env.CONAN:
		return

	# respect project settings
	if not conf.env.CONAN_MANDATORY:
		conf.env.CONAN_MANDATORY = conf.options.CONAN_MANDATORY

	# disabled by user request
	if conf.options.NO_CONAN and not conf.env.CONAN_MANDATORY:
		conf.env.CONAN = None
		return
	
	conf.find_program('conan', mandatory=conf.env.MANDATORY)
	
	if not conf.env.CONAN:
		return
	
	conf.start_msg('Checking conan version')
	ver = conan(conf, '--version', quiet=True)
	if not ver:
		conf.end_msg('fail')
		if conf.env.CONAN_MANDATORY:
			conf.fatal('Conan has failed! Check your conan installation')
		else:
			Logs.warn('Conan has failed! Check your conan installation. Continuing...')
	
	if conf.options.CONAN_PROFILE:
		conf.env.CONAN_PROFILE = conf.options.CONAN_PROFILE
	else:
		profile = conf.env.CONAN_PROFILE = os.path.join(conf.bldnode.abspath(), 'temp_profile')
		settings = dict()
		
		conan(conf, ['profile', 'new', profile, '--detect', '--force'], quiet=True)
		# NOTE: Conan installs even 32-bit runtime packages on x86_64 for now :(
		# it may potentially break system on Linux
		if conf.env.DEST_SIZEOF_VOID_P == 4 and conf.env.DEST_CPU in ['x86', 'x86_64'] and conf.env.DEST_OS != 'linux':
			settings['arch'] = 'x86'
		
		if conf.env.DEST_OS2 == 'android':
			settings['os'] = 'Android'
		
		if conf.env.COMPILER_CC == 'msvc':
			settings['compiler.runtime'] = 'MT'
		
		conan_update_profile(conf, settings, profile)
		
		# I think Conan is respecting environment CC/CXX values, so it's not need
		# to specify compiler here
		#compiler = conf.env.COMPILER_CC
		#if conf.env.COMPILER_CC == 'msvc':
		#	compiler = 'Visual Studio'
		#elif conf.env.DEST_OS == 'darwin' and conf.env.COMPILER_CC == 'clang':
		#	compiler = 'apple-clang'
		#elif conf.env.COMPILER_CC == 'suncc':
		#	compiler = 'sun-cc'
		#settings['compiler'] = compiler
		
	conf.end_msg(ver)

