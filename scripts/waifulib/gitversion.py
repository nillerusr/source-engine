# encoding: utf-8
# gitversion.py -- waf plugin to get git version
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

import subprocess
from waflib import Configure, Logs

@Configure.conf
def get_git_version(conf):
	# try grab the current version number from git
	node = conf.path.find_node('.git')
	
	if not node:
		return None
	
	try:
		stdout = conf.cmd_and_log([conf.env.GIT[0], 'describe', '--dirty', '--always'],
			cwd = node.parent)
		version = stdout.strip()
	except Exception as e:
		version = ''
		Logs.debug(str(e))

	if len(version) == 0:
		version = None

	return version

def configure(conf):
	if conf.find_program('git', mandatory = False):	
		conf.start_msg('Checking git hash')
		ver = conf.get_git_version()

		if ver:
			conf.env.GIT_VERSION = ver
			conf.end_msg(conf.env.GIT_VERSION)
		else:
			conf.end_msg('no', color='YELLOW')
