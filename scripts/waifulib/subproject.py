#!/usr/bin/env python
# encoding: utf-8
# Copyright (c) 2019 a1batross

'''
Subproject tool

Helps you have standalone environment for each subproject(subdirectory)

Usage:
		def options(opt):
			opt.load('subproject')

		def configure(conf):
			conf.add_subproject('folder1 folder2')

		def build(bld):
			bld.add_subproject('folder1 folder2')
'''

from waflib import Configure, Logs, Options, Utils
import os, sys

def opt(f):
	"""
	Decorator: attach new option functions to :py:class:`waflib.Options.OptionsContext`.

	:param f: method to bind
	:type f: function
	"""
	setattr(Options.OptionsContext, f.__name__, f)
	return f

def get_waifulib_by_path(path):
	if not os.path.isabs(path):
		path = os.path.abspath(path)
	
	waifulib = os.path.join(path, 'scripts', 'waifulib')
	if os.path.isdir(waifulib):
		return waifulib
	return None

def check_and_add_waifulib(path):
	waifulib = get_waifulib_by_path(path)
	
	if waifulib:
		sys.path.insert(0, waifulib)

def remove_waifulib(path):
	waifulib = get_waifulib_by_path(path)
	
	if waifulib:
		sys.path.remove(waifulib)

@opt
def add_subproject(ctx, names):
	names_lst = Utils.to_list(names)

	for name in names_lst:
		if not os.path.isabs(name):
			# absolute paths only
			wscript_dir = os.path.join(ctx.path.abspath(), name)
		else: wscript_dir = name
		
		wscript_path = os.path.join(wscript_dir, 'wscript')

		if not os.path.isfile(wscript_path):
			# HACKHACK: this way we get warning message right in the help
			# so this just becomes more noticeable
			ctx.add_option_group('Cannot find wscript in ' + wscript_path + '. You probably missed submodule update')
		else:
			check_and_add_waifulib(wscript_dir)
			ctx.recurse(name)
			remove_waifulib(wscript_dir)

def options(opt):
	grp = opt.add_option_group('Subproject options')

	grp.add_option('-S', '--skip-subprojects', action='store', dest = 'SKIP_SUBDIRS', default=None,
		help = 'don\'t recurse into specified subprojects. Use only directory name.')

def get_subproject_env(ctx, path, log=False):
	# remove top dir path
	path = str(path)
	if path.startswith(ctx.top_dir):
		if ctx.top_dir[-1] != os.pathsep:
			path = path[len(ctx.top_dir) + 1:]
		else: path = path[len(ctx.top_dir):]

	# iterate through possible subprojects names
	folders = os.path.normpath(path).split(os.sep)
	# print(folders)
	for i in range(1, len(folders)+1):
		name = folders[-i]
		# print(name)
		if name in ctx.all_envs:
			if log: Logs.pprint('YELLOW', 'env: changed to %s' % name)
			return ctx.all_envs[name]
	if log: Logs.pprint('YELLOW', 'env: changed to default env')
	raise IndexError('top env')

@Configure.conf
def add_subproject(ctx, dirs, prepend = None):
	'''
	Recurse into subproject directory
	
	:param dirs: Directories we recurse into
	:type dirs: array or string
	:param prepend: Prepend virtual path, useful when managing projects with different environments
	:type prepend: string
	
	'''
	if isinstance(ctx, Configure.ConfigurationContext):
		if not ctx.env.IGNORED_SUBDIRS and ctx.options.SKIP_SUBDIRS:
			ctx.env.IGNORED_SUBDIRS = ctx.options.SKIP_SUBDIRS.split(',')

		for prj in Utils.to_list(dirs):
			if ctx.env.SUBPROJECT_PATH:
				subprj_path = list(ctx.env.SUBPROJECT_PATH)
			else:
				subprj_path = []

			if prj in ctx.env.IGNORED_SUBDIRS:
				ctx.msg(msg='--X %s' % '/'.join(subprj_path), result='ignored', color='YELLOW')
				continue
			
			if prepend:
				subprj_path.append(prepend)
			
			subprj_path.append(prj)
			
			saveenv = ctx.env
			
			ctx.setenv('_'.join(subprj_path), ctx.env) # derive new env from previous
			
			ctx.env.ENVNAME = prj
			ctx.env.SUBPROJECT_PATH = list(subprj_path)
			
			ctx.msg(msg='--> %s' % '/'.join(subprj_path), result='in progress', color='BLUE')
			check_and_add_waifulib(os.path.join(ctx.path.abspath(), prj))
			ctx.recurse(prj)
			remove_waifulib(os.path.join(ctx.path.abspath(), prj))
			ctx.msg(msg='<-- %s' % '/'.join(subprj_path), result='done', color='BLUE')
			
			ctx.setenv('') # save env changes
			
			ctx.env = saveenv # but use previous
	else:
		if not ctx.all_envs:
			ctx.load_envs()

		for prj in Utils.to_list(dirs):
			if prj in ctx.env.IGNORED_SUBDIRS:
				continue

			if ctx.env.SUBPROJECT_PATH:
				subprj_path = list(ctx.env.SUBPROJECT_PATH)
			else:
				subprj_path = []
			
			if prepend:
				subprj_path.append(prepend)
			
			subprj_path.append(prj)
			saveenv = ctx.env
			try:
				ctx.env = ctx.all_envs['_'.join(subprj_path)]
			except:
				ctx.fatal('Can\'t find env cache %s' % '_'.join(subprj_path))
			
			check_and_add_waifulib(os.path.join(ctx.path.abspath(), prj))
			ctx.recurse(prj)
			remove_waifulib(os.path.join(ctx.path.abspath(), prj))
			ctx.env = saveenv
