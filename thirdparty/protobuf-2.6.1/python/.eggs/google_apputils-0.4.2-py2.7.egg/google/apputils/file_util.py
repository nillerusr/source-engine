#!/usr/bin/env python
# Copyright 2007 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Simple file system utilities."""

__author__ = ('elaforge@google.com (Evan LaForge)',
              'matthewb@google.com (Matthew Blecker)')

import contextlib
import errno
import os
import pwd
import shutil
import stat
import tempfile


class PasswdError(Exception):
  """Exception class for errors loading a password from a file."""


def ListDirPath(dir_name):
  """Like os.listdir with prepended dir_name, which is often more convenient."""
  return [os.path.join(dir_name, fn) for fn in os.listdir(dir_name)]


def Read(filename):
  """Read entire contents of file with name 'filename'."""
  with open(filename) as fp:
    return fp.read()


def Write(filename, contents, overwrite_existing=True, mode=0666, gid=None):
  """Create a file 'filename' with 'contents', with the mode given in 'mode'.

  The 'mode' is modified by the umask, as in open(2).  If
  'overwrite_existing' is False, the file will be opened in O_EXCL mode.

  An optional gid can be specified.

  Args:
    filename: str; the name of the file
    contents: str; the data to write to the file
    overwrite_existing: bool; whether or not to allow the write if the file
                        already exists
    mode: int; permissions with which to create the file (default is 0666 octal)
    gid: int; group id with which to create the file
  """
  flags = os.O_WRONLY | os.O_TRUNC | os.O_CREAT
  if not overwrite_existing:
    flags |= os.O_EXCL
  fd = os.open(filename, flags, mode)
  try:
    os.write(fd, contents)
  finally:
    os.close(fd)
  if gid is not None:
    os.chown(filename, -1, gid)


def AtomicWrite(filename, contents, mode=0666, gid=None):
  """Create a file 'filename' with 'contents' atomically.

  As in Write, 'mode' is modified by the umask.  This creates and moves
  a temporary file, and errors doing the above will be propagated normally,
  though it will try to clean up the temporary file in that case.

  This is very similar to the prodlib function with the same name.

  An optional gid can be specified.

  Args:
    filename: str; the name of the file
    contents: str; the data to write to the file
    mode: int; permissions with which to create the file (default is 0666 octal)
    gid: int; group id with which to create the file
  """
  fd, tmp_filename = tempfile.mkstemp(dir=os.path.dirname(filename))
  try:
    os.write(fd, contents)
  finally:
    os.close(fd)
  try:
    os.chmod(tmp_filename, mode)
    if gid is not None:
      os.chown(tmp_filename, -1, gid)
    os.rename(tmp_filename, filename)
  except OSError, exc:
    try:
      os.remove(tmp_filename)
    except OSError, e:
      exc = OSError('%s. Additional errors cleaning up: %s' % (exc, e))
    raise exc


@contextlib.contextmanager
def TemporaryFileWithContents(contents, **kw):
  """A contextmanager that writes out a string to a file on disk.

  This is useful whenever you need to call a function or command that expects a
  file on disk with some contents that you have in memory. The context manager
  abstracts the writing, flushing, and deletion of the temporary file. This is a
  common idiom that boils down to a single with statement.

  Note:  if you need a temporary file-like object for calling an internal
  function, you should use a StringIO as a file-like object and not this.
  Temporary files should be avoided unless you need a file name or contents in a
  file on disk to be read by some other function or program.

  Args:
    contents: a string with the contents to write to the file.
    **kw: Optional arguments passed on to tempfile.NamedTemporaryFile.
  Yields:
    The temporary file object, opened in 'w' mode.

  """
  temporary_file = tempfile.NamedTemporaryFile(**kw)
  temporary_file.write(contents)
  temporary_file.flush()
  yield temporary_file
  temporary_file.close()


# TODO(user): remove after migration to Python 3.2.
# This context manager can be replaced with tempfile.TemporaryDirectory in
# Python 3.2 (http://bugs.python.org/issue5178,
# http://docs.python.org/dev/library/tempfile.html#tempfile.TemporaryDirectory).
@contextlib.contextmanager
def TemporaryDirectory(suffix='', prefix='tmp', base_path=None):
  """A context manager to create a temporary directory and clean up on exit.

  The parameters are the same ones expected by tempfile.mkdtemp.
  The directory will be securely and atomically created.
  Everything under it will be removed when exiting the context.

  Args:
    suffix: optional suffix.
    prefix: options prefix.
    base_path: the base path under which to create the temporary directory.
  Yields:
    The absolute path of the new temporary directory.
  """
  temp_dir_path = tempfile.mkdtemp(suffix, prefix, base_path)
  try:
    yield temp_dir_path
  finally:
    try:
      shutil.rmtree(temp_dir_path)
    except OSError, e:
      if e.message == 'Cannot call rmtree on a symbolic link':
        # Interesting synthetic exception made up by shutil.rmtree.
        # Means we received a symlink from mkdtemp.
        # Also means must clean up the symlink instead.
        os.unlink(temp_dir_path)
      else:
        raise


def MkDirs(directory, force_mode=None):
  """Makes a directory including its parent directories.

  This function is equivalent to os.makedirs() but it avoids a race
  condition that os.makedirs() has.  The race is between os.mkdir() and
  os.path.exists() which fail with errors when run in parallel.

  Args:
    directory: str; the directory to make
    force_mode: optional octal, chmod dir to get rid of umask interaction
  Raises:
    Whatever os.mkdir() raises when it fails for any reason EXCLUDING
    "dir already exists".  If a directory already exists, it does not
    raise anything.  This behaviour is different than os.makedirs()
  """
  name = os.path.normpath(directory)
  dirs = name.split(os.path.sep)
  for i in range(0, len(dirs)):
    path = os.path.sep.join(dirs[:i+1])
    try:
      if path:
        os.mkdir(path)
        # only chmod if we created
        if force_mode is not None:
          os.chmod(path, force_mode)
    except OSError, exc:
      if not (exc.errno == errno.EEXIST and os.path.isdir(path)):
        raise


def RmDirs(dir_name):
  """Removes dir_name and every subsequently empty directory above it.

  Unlike os.removedirs and shutil.rmtree, this function doesn't raise an error
  if the directory does not exist.

  Args:
    dir_name: Directory to be removed.
  """
  try:
    shutil.rmtree(dir_name)
  except OSError, err:
    if err.errno != errno.ENOENT:
      raise

  try:
    parent_directory = os.path.dirname(dir_name)
    while parent_directory:
      try:
        os.rmdir(parent_directory)
      except OSError, err:
        if err.errno != errno.ENOENT:
          raise

      parent_directory = os.path.dirname(parent_directory)
  except OSError, err:
    if err.errno not in (errno.EACCES, errno.ENOTEMPTY, errno.EPERM):
      raise


def HomeDir(user=None):
  """Find the home directory of a user.

  Args:
    user: int, str, or None - the uid or login of the user to query for,
          or None (the default) to query for the current process' effective user

  Returns:
    str - the user's home directory

  Raises:
    TypeError: if user is not int, str, or None.
  """
  if user is None:
    pw_struct = pwd.getpwuid(os.geteuid())
  elif isinstance(user, int):
    pw_struct = pwd.getpwuid(user)
  elif isinstance(user, str):
    pw_struct = pwd.getpwnam(user)
  else:
    raise TypeError('user must be None or an instance of int or str')
  return pw_struct.pw_dir
