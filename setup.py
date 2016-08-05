#!/usr/bin/env python
###############################################################################
# Copyright (C) 2016  Billy Kozak					      #
#									      #
# This file is part of python-rflow					      #
#									      #
# This program is free software: you can redistribute it and/or modify	      #
# it under the terms of the GNU General Public License as published by	      #
# the Free Software Foundation, either version 3 of the License, or	      #
# (at your option) any later version.					      #
#									      #
# This program is distributed in the hope that it will be useful,	      #
# but WITHOUT ANY WARRANTY; without even the implied warranty of	      #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the		      #
# GNU General Public License for more details.				      #
#									      #
# You should have received a copy of the GNU General Public License	      #
# along with this program.  If not, see <http://www.gnu.org/licenses/>.	      #
###############################################################################
###############################################################################
#				    IMPORTS				      #
###############################################################################
from setuptools import setup, Extension
import distutils.command.clean
import glob
import os
import re
import shutil
###############################################################################
#				   CONSTANTS				      #
###############################################################################
ROOT_DIR=os.path.dirname(__file__) if os.path.dirname(__file__) else '.'
HIDDEN_RE=re.compile("^\..*$")

SRC_DIR = os.path.join(ROOT_DIR, 'src')

ADDITIONAL_CLEAN_FILES = []
ADDITIONAL_CLEAN_DIRS=[os.path.join(ROOT_DIR, 'build')]
###############################################################################
#				     CODE				      #
###############################################################################
def list_dir_adv(path):
        """List fiels in directories split into directories and regular files
        """
        all_paths  = set([os.path.join(path,p) for p in os.listdir(path)])

        dir_paths  = set([p for p in all_paths if os.path.isdir(p)])
        file_paths = all_paths - dir_paths

        dir_only   = [os.path.basename(d) for d in dir_paths]
        file_only  = [os.path.basename(f) for f in file_paths]

        return dir_only, file_only

def regex_list(pattern, str_list):
        """Run a regex on a list of strings returning only those that match
        """
        return [s for s in str_list if re.match(pattern, s) != None]


def file_search(path, pattern, exclude=HIDDEN_RE, depth=-1, matches=None):
	"""Perform a recursive file search using regex
	"""
	if depth == 0:
		return

	if matches == None:
		matches = []

	dir_names, file_names = list_dir_adv(path)

	new_matches = regex_list(pattern, file_names)
	matches.extend([os.path.join(path, f) for f in new_matches])

	if exclude != None:
		excluded = set(regex_list(exclude, dir_names))
	else:
		excluded = set()

	ok_dirs = set(dir_names) - excluded
	dir_names = [os.path.join(path, d) for d in ok_dirs]

	for sub_dir in dir_names:
		file_search(sub_dir, pattern, exclude, depth - 1, matches)

	return matches

def editor_files():
	"""Return a list of editor temporary files
	"""
	pattern = re.compile('(.*~)|(#.*#)|(.*\.swp)')

	return file_search(ROOT_DIR, pattern)


def pyc_files():
	"""Return a list of compiled .pyc files
	"""
	pattern = re.compile('.*.pyc')

	return file_search(SRC_DIR, pattern)

class CleanCommand(distutils.command.clean.clean):
	"""Extended clean command to delete some extra unwanted files
	"""
	def run(self):
		"""Delete some extra files along with the regular ones
		"""
		extra_files =  editor_files()
		extra_files += pyc_files()
		extra_files += ADDITIONAL_CLEAN_FILES

		for f in extra_files:
			if os.path.exists(f):
				os.remove(f)

		extra_dirs = ADDITIONAL_CLEAN_DIRS
		for d in extra_dirs:
			if os.path.exists(d):
				shutil.rmtree(d)

		distutils.command.clean.clean.run(self)
###############################################################################
#				     SETUP				      #
###############################################################################
# Extended Commands
cmdclass={
	'clean': CleanCommand
}

rflow = Extension(
	'rflow',
	sources = glob.glob(os.path.join(SRC_DIR, '*.c')),
	library_dirs = ['/usr/local/lib'],
	libraries = [':lib-rflow.so'],
	extra_compile_args = ['-std=c99']
)

setup (
	name = 'rflow',
	version = '1.0',
	description = 'These are python bindings for lib-rflow',
	ext_modules = [rflow],
	cmdclass=cmdclass
)
###############################################################################
