#!/usr/bin/env python
# encoding: utf-8
# Copyright (c) 2019 mittorn
CC=../android-ndk-r10e/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc CXX=../android-ndk-r10e/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc LD=../android-ndk-r10e/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ld AR=../android-ndk-r10e/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ar LINK=../android-ndk-r10e/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ld STRIP=../android-ndk-r10e/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-strip ./waf configure -T debug

../android-ndk-r10e/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc -DDX_TO_GL_ABSTRACTION -DGL_GLEXT_PROTOTYPES -DBINK_VIDEO -DUSE_SDL=1 -DANDROID=1 -D_ANDROID=1 -DLINUX=1 -D_LINUX=1 -DPOSIX=1 -D_POSIX=1 -DGNUC -DNDEBUG -DNO_HOOK_MALLOC -D_DLL_EXT=.so /home/jusic/source-engine/main.c -c -o/home/jusic/source-engine/build/test.c.1.o
'''
Reconfigure

Store/load configuration user input

Usage:
		def options(opt):
			opt.load('reconfigure')

		def configure(conf):
			conf.load('reconfigure')

		./waf configure --reconfigure
'''

from waflib import Configure, Logs, Options, Utils, ConfigSet
import os
import optparse

def options(opt):
	opt.add_option('--rebuild-cache', dest='rebuild_cache', default=False, action='store_true', help='load previous configuration')
	opt.add_option('--reconfigure', dest='reconfigure', default=False, action='store_true', help='load and update configuration')

def configure(conf):
	store_path = os.path.join(conf.bldnode.abspath(), 'configuration.py')
	store_data = ConfigSet.ConfigSet()
	options = vars(conf.options)
	environ = conf.environ
	if conf.options.reconfigure or conf.options.rebuild_cache:
		store_data.load(store_path)
		if conf.options.reconfigure:
			for o in options:
				if options[o]: store_data['OPTIONS'][o] = options[o]
			store_data['ENVIRON'].update(environ)
			store_data.store(store_path)
		conf.environ = store_data['ENVIRON']
		conf.options = optparse.Values(store_data['OPTIONS'])
	else:
	    store_data['OPTIONS'] = vars(conf.options)
	    store_data['ENVIRON'] = conf.environ
	    store_data.store(store_path)
