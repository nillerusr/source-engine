# encoding: utf-8
# cxx11.py -- check if compiler can compile C++11 code with lambdas
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
#   CXX11_MANDATORY(optional) -- fail if C++11 not available
# Output:
#   HAVE_CXX11 -- true if C++11 available, otherwise false

modern_cpp_flags = {
	'msvc':    [],
	'default': ['-std=c++11']
}

CXX11_LAMBDA_FRAGMENT='''
class T
{
	static void M(){}
public:
	void t()
	{
		auto l = []()
		{
			T::M();
		};
	}
};

int main()
{
	T t;
	t.t();
}
'''

@Configure.conf
def check_cxx11(ctx, *k, **kw):
	if not 'msg' in kw:
		kw['msg'] = 'Checking if \'%s\' supports C++11' % ctx.env.COMPILER_CXX

	if not 'mandatory' in kw:
		kw['mandatory'] = False

	# not best way, but this check
	# was written for exactly mainui_cpp,
	# where lambdas are mandatory
	return ctx.check_cxx(fragment=CXX11_LAMBDA_FRAGMENT, *k, **kw)

def configure(conf):
	flags = get_flags_by_compiler(modern_cpp_flags, conf.env.COMPILER_CXX)

	if conf.check_cxx11():
		conf.env.HAVE_CXX11 = True
	elif len(flags) != 0 and conf.check_cxx11(msg='...trying with additional flags', cxxflags = flags):
		conf.env.HAVE_CXX11 = True
		conf.env.CXXFLAGS += flags
	else:
		conf.env.HAVE_CXX11 = False

		if conf.env.CXX11_MANDATORY:
			conf.fatal('C++11 support not available!')
