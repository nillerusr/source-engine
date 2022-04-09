# It looks like shit, but I'm not particularly worried about such scripts.

import os
import re

token_list = [
	re.compile(r'&&'),
	re.compile(r'\|\|'),
	re.compile(r'\!'),
	re.compile(r'[a-zA-Z0-9_.]*')
]

match_statement = re.compile(r'\[.*\]')

def compute_statement( defines, statement ):
	vars = {}
	for define in defines:
		d=define.split('=')[0]
		vars.update({d:True})

	def t( op ):
		if op == '1': return True
		elif op == '0': return False
		elif op not in vars: return False

		return vars[op]

	pos = 0

	statement = re.sub(r'\[|\]| |\$', '', statement)

	l = []

	final = True
	final_init = False

	while pos < len(statement):
		for token in token_list:
			r = token.search(statement, pos)
			if r and r.start() == pos:
				l += [r.group(0)]
				pos = r.end()

	k = 0
	for i in range(len(l)):
		j = i-k
		if l[j] == '!' and j+1 < len(l):
			df = l[j+1]
			if df in vars:
				vars[df] = not vars[df]
			else: vars.update({df:True})
			del l[j]
			k += 1

	k = 0
	for i in range(len(l)):
		j = i-k
		if l[j] == '&&' and j+1 < len(l) and j-1 >= 0:
			val = 0
			if t(l[j-1]) and t(l[j+1]):
				val = 1
			del l[j+1], l[j], l[j-1]
			l.insert(j, str(val))
			k += 2

	k = 0
	for i in range(len(l)):
		j = i-k
		if l[j] == '||' and j+1 < len(l) and j-1 >= 0:
			val = 0
			if t(l[j-1]) or t(l[j+1]):
				val = 1
			del l[j+1], l[j], l[j-1]
			l.insert(j, str(val))
			k += 2

	return t(l[0])

def project_key(l):
	for k in l.keys():
		if '$Project' in k:
			return k

def fix_dos_path( path ):
	path = path.replace('\\', '/')
	p = path.split('/')

	filename = p[-1]
	find_path = '/'.join(p[0:len(p)-1])
	if find_path == '': find_path = './'
	else: find_path += '/'

	dirlist = os.listdir(find_path)
	for file in dirlist:
		if file == filename:
			return find_path+file
		elif file.lower() == filename.lower():
			return find_path+file
	return find_path+filename

def parse_vpcs( env ,vpcs, basedir ):
	back_path = os.path.abspath('.')
	os.chdir(env.SUBPROJECT_PATH[0])

	sources = []
	defines = []
	includes = []

	for vpc in vpcs:
		f=open(vpc, 'r').read().replace('\\\n', ';')

		re.sub(r'//.*', '', f)
		l = f.split('\n')

		iBrackets = 0

		next_br = False
		ret = {}
		cur_key = ''

		for i in l:
			if i == '': continue

			s = match_statement.search(i)
			if s and not compute_statement(env.DEFINES+defines, s.group(0)):
				continue

			if i.startswith('$') and iBrackets == 0:
				ret.update({i:[]})
				cur_key = i
				next_br = True
			elif i == '{':
				iBrackets += 1
				next_br = False
			elif i == '}':
				iBrackets -= 1
			elif iBrackets > 0:
				ret[cur_key].append(i)

			if next_br:
				next_br = False

		key = project_key(ret)
		l=ret[key]

		for i in l:
			if '-$File' in i and '.h"' not in i:
				for k in i.split(';'):
					k = k.replace('$SRCDIR', basedir)
					s = fix_dos_path(k.split('"')[1])

					for j in range(len(sources)):
						if sources[j] == s:
							del sources[j]
							break

			elif '$File' in i and '.h"' not in i:
				for j in i.split(';'):
					j = j.replace('$SRCDIR', basedir)
					s = fix_dos_path(j.split('"')[1])
					sources.append(s)

		for i in ret['$Configuration']:
			if '$PreprocessorDefinitions' in i:
				i = i.replace('$BASE', '')
				s = i.split('"')[1]
				s = re.split(';|,', s)
				for j in s:
					if j != '' and j not in defines:
						defines.append(j)
			if '$AdditionalIncludeDirectories' in i:
				i = i.replace('$BASE', '').replace('$SRCDIR', basedir)
				s = i.split('"')[1]
				s = re.split(';|,', s)
				for j in s:
					j = j.replace('\\','/')
					if j != '' and j not in includes:
						includes.append(j)
	os.chdir(back_path)

	return {'defines':defines, 'includes':includes, 'sources': sources}
