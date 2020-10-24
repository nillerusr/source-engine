#!/usr/bin/env python
# Copyright 2010 Google Inc. All Rights Reserved.
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

"""Base functionality for google tests.

This module contains base classes and high-level functions for Google-style
tests.
"""

__author__ = 'dborowitz@google.com (Dave Borowitz)'

import collections
import difflib
import getpass
import itertools
import json
import os
import re
import signal
import subprocess
import sys
import tempfile
import types
import unittest
import urlparse

try:
  import faulthandler  # pylint: disable=g-import-not-at-top
except ImportError:
  # //testing/pybase:pybase can't have deps on any extension modules as it
  # is used by code that is executed in such a way it cannot import them. :(
  # We use faulthandler if it is available (either via a user declared dep
  # or from the Python 3.3+ standard library).
  faulthandler = None

from google.apputils import app  # pylint: disable=g-import-not-at-top
import gflags as flags
from google.apputils import shellutil

FLAGS = flags.FLAGS

# ----------------------------------------------------------------------
# Internal functions to extract default flag values from environment.
# ----------------------------------------------------------------------
def _GetDefaultTestRandomSeed():
  random_seed = 301
  value = os.environ.get('TEST_RANDOM_SEED', '')
  try:
    random_seed = int(value)
  except ValueError:
    pass
  return random_seed


def _GetDefaultTestTmpdir():
  """Get default test temp dir."""
  tmpdir = os.environ.get('TEST_TMPDIR', '')
  if not tmpdir:
    tmpdir = os.path.join(tempfile.gettempdir(), 'google_apputils_basetest')

  return tmpdir


flags.DEFINE_integer('test_random_seed', _GetDefaultTestRandomSeed(),
                     'Random seed for testing. Some test frameworks may '
                     'change the default value of this flag between runs, so '
                     'it is not appropriate for seeding probabilistic tests.',
                     allow_override=1)
flags.DEFINE_string('test_srcdir',
                    os.environ.get('TEST_SRCDIR', ''),
                    'Root of directory tree where source files live',
                    allow_override=1)
flags.DEFINE_string('test_tmpdir', _GetDefaultTestTmpdir(),
                    'Directory for temporary testing files',
                    allow_override=1)


# We might need to monkey-patch TestResult so that it stops considering an
# unexpected pass as a as a "successful result".  For details, see
# http://bugs.python.org/issue20165
def _MonkeyPatchTestResultForUnexpectedPasses():
  """Workaround for <http://bugs.python.org/issue20165>."""

  # pylint: disable=g-doc-return-or-yield,g-doc-args,g-wrong-blank-lines

  def wasSuccessful(self):
    """Tells whether or not this result was a success.

    Any unexpected pass is to be counted as a non-success.
    """
    return (len(self.failures) == len(self.errors) ==
            len(self.unexpectedSuccesses) == 0)

  # pylint: enable=g-doc-return-or-yield,g-doc-args,g-wrong-blank-lines

  test_result = unittest.result.TestResult()
  test_result.addUnexpectedSuccess('test')
  if test_result.wasSuccessful():  # The bug is present.
    unittest.result.TestResult.wasSuccessful = wasSuccessful
    if test_result.wasSuccessful():  # Warn the user if our hot-fix failed.
      sys.stderr.write('unittest.result.TestResult monkey patch to report'
                       ' unexpected passes as failures did not work.\n')


_MonkeyPatchTestResultForUnexpectedPasses()


class TestCase(unittest.TestCase):
  """Extension of unittest.TestCase providing more powerful assertions."""

  maxDiff = 80 * 20

  def __init__(self, methodName='runTest'):
    super(TestCase, self).__init__(methodName)
    self.__recorded_properties = {}

  def shortDescription(self):
    """Format both the test method name and the first line of its docstring.

    If no docstring is given, only returns the method name.

    This method overrides unittest.TestCase.shortDescription(), which
    only returns the first line of the docstring, obscuring the name
    of the test upon failure.

    Returns:
      desc: A short description of a test method.
    """
    desc = str(self)
    # NOTE: super() is used here instead of directly invoking
    # unittest.TestCase.shortDescription(self), because of the
    # following line that occurs later on:
    #       unittest.TestCase = TestCase
    # Because of this, direct invocation of what we think is the
    # superclass will actually cause infinite recursion.
    doc_first_line = super(TestCase, self).shortDescription()
    if doc_first_line is not None:
      desc = '\n'.join((desc, doc_first_line))
    return desc

  def assertStartsWith(self, actual, expected_start):
    """Assert that actual.startswith(expected_start) is True.

    Args:
      actual: str
      expected_start: str
    """
    if not actual.startswith(expected_start):
      self.fail('%r does not start with %r' % (actual, expected_start))

  def assertNotStartsWith(self, actual, unexpected_start):
    """Assert that actual.startswith(unexpected_start) is False.

    Args:
      actual: str
      unexpected_start: str
    """
    if actual.startswith(unexpected_start):
      self.fail('%r does start with %r' % (actual, unexpected_start))

  def assertEndsWith(self, actual, expected_end):
    """Assert that actual.endswith(expected_end) is True.

    Args:
      actual: str
      expected_end: str
    """
    if not actual.endswith(expected_end):
      self.fail('%r does not end with %r' % (actual, expected_end))

  def assertNotEndsWith(self, actual, unexpected_end):
    """Assert that actual.endswith(unexpected_end) is False.

    Args:
      actual: str
      unexpected_end: str
    """
    if actual.endswith(unexpected_end):
      self.fail('%r does end with %r' % (actual, unexpected_end))

  def assertSequenceStartsWith(self, prefix, whole, msg=None):
    """An equality assertion for the beginning of ordered sequences.

    If prefix is an empty sequence, it will raise an error unless whole is also
    an empty sequence.

    If prefix is not a sequence, it will raise an error if the first element of
    whole does not match.

    Args:
      prefix: A sequence expected at the beginning of the whole parameter.
      whole: The sequence in which to look for prefix.
      msg: Optional message to append on failure.
    """
    try:
      prefix_len = len(prefix)
    except (TypeError, NotImplementedError):
      prefix = [prefix]
      prefix_len = 1

    try:
      whole_len = len(whole)
    except (TypeError, NotImplementedError):
      self.fail('For whole: len(%s) is not supported, it appears to be type: '
                '%s' % (whole, type(whole)))

    assert prefix_len <= whole_len, (
        'Prefix length (%d) is longer than whole length (%d).' %
        (prefix_len, whole_len))

    if not prefix_len and whole_len:
      self.fail('Prefix length is 0 but whole length is %d: %s' %
                (len(whole), whole))

    try:
      self.assertSequenceEqual(prefix, whole[:prefix_len], msg)
    except AssertionError:
      self.fail(msg or 'prefix: %s not found at start of whole: %s.' %
                (prefix, whole))

  def assertContainsSubset(self, expected_subset, actual_set, msg=None):
    """Checks whether actual iterable is a superset of expected iterable."""
    missing = set(expected_subset) - set(actual_set)
    if not missing:
      return

    missing_msg = 'Missing elements %s\nExpected: %s\nActual: %s' % (
        missing, expected_subset, actual_set)
    if msg:
      msg += ': %s' % missing_msg
    else:
      msg = missing_msg
    self.fail(msg)

  def assertNoCommonElements(self, expected_seq, actual_seq, msg=None):
    """Checks whether actual iterable and expected iterable are disjoint."""
    common = set(expected_seq) & set(actual_seq)
    if not common:
      return

    common_msg = 'Common elements %s\nExpected: %s\nActual: %s' % (
        common, expected_seq, actual_seq)
    if msg:
      msg += ': %s' % common_msg
    else:
      msg = common_msg
    self.fail(msg)

  # TODO(user): Provide an assertItemsEqual method when our super class
  # does not provide one.  That method went away in Python 3.2 (renamed
  # to assertCountEqual, or is that different? investigate).

  def assertItemsEqual(self, *args, **kwargs):
    # pylint: disable=g-doc-args
    """An unordered sequence specific comparison.

    It asserts that actual_seq and expected_seq have the same element counts.
    Equivalent to::

        self.assertEqual(Counter(iter(actual_seq)),
                         Counter(iter(expected_seq)))

    Asserts that each element has the same count in both sequences.
    Example:
        - [0, 1, 1] and [1, 0, 1] compare equal.
        - [0, 0, 1] and [0, 1] compare unequal.

    Args:
      expected_seq: A sequence containing elements we are expecting.
      actual_seq: The sequence that we are testing.
      msg: The message to be printed if the test fails.
    """
    # pylint: enable=g-doc-args
    # In Python 3k this method is called assertCountEqual()
    if sys.version_info.major > 2:
      self.assertItemsEqual = super(TestCase, self).assertCountEqual
      self.assertItemsEqual(*args, **kwargs)
      return
    # For Python 2.x we must check for the issue below
    super_assert_items_equal = super(TestCase, self).assertItemsEqual
    try:
      super_assert_items_equal([23], [])  # Force a fail to check behavior.
    except self.failureException as error_to_introspect:
      if 'First has 0, Second has 1:  23' in str(error_to_introspect):
        # It exhibits http://bugs.python.org/issue14832
        # Always use our repaired method that swaps the arguments.
        self.assertItemsEqual = self._FixedAssertItemsEqual
      else:
        # It exhibits correct behavior. Always use the super's method.
        self.assertItemsEqual = super_assert_items_equal
      # Delegate this call to the correct method. All future calls will skip
      # this error patching code.
      self.assertItemsEqual(*args, **kwargs)
    assert 'Impossible: TestCase assertItemsEqual is broken.'

  def _FixedAssertItemsEqual(self, expected_seq, actual_seq, msg=None):
    """A version of assertItemsEqual that works around issue14832."""
    super(TestCase, self).assertItemsEqual(actual_seq, expected_seq, msg=msg)

  def assertCountEqual(self, *args, **kwargs):
    # pylint: disable=g-doc-args
    """An unordered sequence specific comparison.

    Equivalent to assertItemsEqual(). This method is a compatibility layer
    for Python 3k, since 2to3 does not convert assertItemsEqual() calls into
    assertCountEqual() calls.

    Args:
      expected_seq: A sequence containing elements we are expecting.
      actual_seq: The sequence that we are testing.
      msg: The message to be printed if the test fails.
    """
    # pylint: enable=g-doc-args
    self.assertItemsEqual(*args, **kwargs)

  def assertSameElements(self, expected_seq, actual_seq, msg=None):
    """Assert that two sequences have the same elements (in any order).

    This method, unlike assertItemsEqual, doesn't care about any
    duplicates in the expected and actual sequences.

      >> assertSameElements([1, 1, 1, 0, 0, 0], [0, 1])
      # Doesn't raise an AssertionError

    If possible, you should use assertItemsEqual instead of
    assertSameElements.

    Args:
      expected_seq: A sequence containing elements we are expecting.
      actual_seq: The sequence that we are testing.
      msg: The message to be printed if the test fails.
    """
    # `unittest2.TestCase` used to have assertSameElements, but it was
    # removed in favor of assertItemsEqual. As there's a unit test
    # that explicitly checks this behavior, I am leaving this method
    # alone.
    # Fail on strings: empirically, passing strings to this test method
    # is almost always a bug. If comparing the character sets of two strings
    # is desired, cast the inputs to sets or lists explicitly.
    if (isinstance(expected_seq, basestring) or
        isinstance(actual_seq, basestring)):
      self.fail('Passing a string to assertSameElements is usually a bug. '
                'Did you mean to use assertEqual?\n'
                'Expected: %s\nActual: %s' % (expected_seq, actual_seq))
    try:
      expected = dict([(element, None) for element in expected_seq])
      actual = dict([(element, None) for element in actual_seq])
      missing = [element for element in expected if element not in actual]
      unexpected = [element for element in actual if element not in expected]
      missing.sort()
      unexpected.sort()
    except TypeError:
      # Fall back to slower list-compare if any of the objects are
      # not hashable.
      expected = list(expected_seq)
      actual = list(actual_seq)
      expected.sort()
      actual.sort()
      missing, unexpected = _SortedListDifference(expected, actual)
    errors = []
    if missing:
      errors.append('Expected, but missing:\n  %r\n' % missing)
    if unexpected:
      errors.append('Unexpected, but present:\n  %r\n' % unexpected)
    if errors:
      self.fail(msg or ''.join(errors))

  # unittest2.TestCase.assertMulitilineEqual works very similarly, but it
  # has a different error format. However, I find this slightly more readable.
  def assertMultiLineEqual(self, first, second, msg=None):
    """Assert that two multi-line strings are equal."""
    assert isinstance(first, types.StringTypes), (
        'First argument is not a string: %r' % (first,))
    assert isinstance(second, types.StringTypes), (
        'Second argument is not a string: %r' % (second,))

    if first == second:
      return
    if msg:
      failure_message = [msg, ':\n']
    else:
      failure_message = ['\n']
    for line in difflib.ndiff(first.splitlines(True), second.splitlines(True)):
      failure_message.append(line)
      if not line.endswith('\n'):
        failure_message.append('\n')
    raise self.failureException(''.join(failure_message))

  def assertBetween(self, value, minv, maxv, msg=None):
    """Asserts that value is between minv and maxv (inclusive)."""
    if msg is None:
      msg = '"%r" unexpectedly not between "%r" and "%r"' % (value, minv, maxv)
    self.assert_(minv <= value, msg)
    self.assert_(maxv >= value, msg)

  def assertRegexMatch(self, actual_str, regexes, message=None):
    # pylint: disable=g-doc-bad-indent
    """Asserts that at least one regex in regexes matches str.

    If possible you should use assertRegexpMatches, which is a simpler
    version of this method. assertRegexpMatches takes a single regular
    expression (a string or re compiled object) instead of a list.

    Notes:
    1. This function uses substring matching, i.e. the matching
       succeeds if *any* substring of the error message matches *any*
       regex in the list.  This is more convenient for the user than
       full-string matching.

    2. If regexes is the empty list, the matching will always fail.

    3. Use regexes=[''] for a regex that will always pass.

    4. '.' matches any single character *except* the newline.  To
       match any character, use '(.|\n)'.

    5. '^' matches the beginning of each line, not just the beginning
       of the string.  Similarly, '$' matches the end of each line.

    6. An exception will be thrown if regexes contains an invalid
       regex.

    Args:
      actual_str:  The string we try to match with the items in regexes.
      regexes:  The regular expressions we want to match against str.
        See "Notes" above for detailed notes on how this is interpreted.
      message:  The message to be printed if the test fails.
    """
    # pylint: enable=g-doc-bad-indent
    if isinstance(regexes, basestring):
      self.fail('regexes is a string; use assertRegexpMatches instead.')
    if not regexes:
      self.fail('No regexes specified.')

    regex_type = type(regexes[0])
    for regex in regexes[1:]:
      if type(regex) is not regex_type:
        self.fail('regexes list must all be the same type.')

    if regex_type is bytes and isinstance(actual_str, unicode):
      regexes = [regex.decode('utf-8') for regex in regexes]
      regex_type = unicode
    elif regex_type is unicode and isinstance(actual_str, bytes):
      regexes = [regex.encode('utf-8') for regex in regexes]
      regex_type = bytes

    if regex_type is unicode:
      regex = u'(?:%s)' % u')|(?:'.join(regexes)
    elif regex_type is bytes:
      regex = b'(?:' + (b')|(?:'.join(regexes)) + b')'
    else:
      self.fail('Only know how to deal with unicode str or bytes regexes.')

    if not re.search(regex, actual_str, re.MULTILINE):
      self.fail(message or ('"%s" does not contain any of these '
                            'regexes: %s.' % (actual_str, regexes)))

  def assertCommandSucceeds(self, command, regexes=(b'',), env=None,
                            close_fds=True):
    """Asserts that a shell command succeeds (i.e. exits with code 0).

    Args:
      command: List or string representing the command to run.
      regexes: List of regular expression byte strings that match success.
      env: Dictionary of environment variable settings.
      close_fds: Whether or not to close all open fd's in the child after
        forking.
    """
    (ret_code, err) = GetCommandStderr(command, env, close_fds)

    # Accommodate code which listed their output regexes w/o the b'' prefix by
    # converting them to bytes for the user.
    if isinstance(regexes[0], unicode):
      regexes = [regex.encode('utf-8') for regex in regexes]

    command_string = GetCommandString(command)
    self.assertEqual(
        ret_code, 0,
        'Running command\n'
        '%s failed with error code %s and message\n'
        '%s' % (
            _QuoteLongString(command_string),
            ret_code,
            _QuoteLongString(err)))
    self.assertRegexMatch(
        err,
        regexes,
        message=(
            'Running command\n'
            '%s failed with error code %s and message\n'
            '%s which matches no regex in %s' % (
                _QuoteLongString(command_string),
                ret_code,
                _QuoteLongString(err),
                regexes)))

  def assertCommandFails(self, command, regexes, env=None, close_fds=True):
    """Asserts a shell command fails and the error matches a regex in a list.

    Args:
      command: List or string representing the command to run.
      regexes: the list of regular expression strings.
      env: Dictionary of environment variable settings.
      close_fds: Whether or not to close all open fd's in the child after
        forking.
    """
    (ret_code, err) = GetCommandStderr(command, env, close_fds)

    # Accommodate code which listed their output regexes w/o the b'' prefix by
    # converting them to bytes for the user.
    if isinstance(regexes[0], unicode):
      regexes = [regex.encode('utf-8') for regex in regexes]

    command_string = GetCommandString(command)
    self.assertNotEqual(
        ret_code, 0,
        'The following command succeeded while expected to fail:\n%s' %
        _QuoteLongString(command_string))
    self.assertRegexMatch(
        err,
        regexes,
        message=(
            'Running command\n'
            '%s failed with error code %s and message\n'
            '%s which matches no regex in %s' % (
                _QuoteLongString(command_string),
                ret_code,
                _QuoteLongString(err),
                regexes)))

  class _AssertRaisesContext(object):

    def __init__(self, expected_exception, test_case, test_func):
      self.expected_exception = expected_exception
      self.test_case = test_case
      self.test_func = test_func

    def __enter__(self):
      return self

    def __exit__(self, exc_type, exc_value, tb):
      if exc_type is None:
        self.test_case.fail(self.expected_exception.__name__ + ' not raised')
      if not issubclass(exc_type, self.expected_exception):
        return False
      self.test_func(exc_value)
      return True

  def assertRaisesWithPredicateMatch(self, expected_exception, predicate,
                                     callable_obj=None, *args, **kwargs):
    # pylint: disable=g-doc-args
    """Asserts that exception is thrown and predicate(exception) is true.

    Args:
      expected_exception: Exception class expected to be raised.
      predicate: Function of one argument that inspects the passed-in exception
        and returns True (success) or False (please fail the test).
      callable_obj: Function to be called.
      args: Extra args.
      kwargs: Extra keyword args.

    Returns:
      A context manager if callable_obj is None. Otherwise, None.

    Raises:
      self.failureException if callable_obj does not raise a macthing exception.
    """
    # pylint: enable=g-doc-args
    def Check(err):
      self.assert_(predicate(err),
                   '%r does not match predicate %r' % (err, predicate))

    context = self._AssertRaisesContext(expected_exception, self, Check)
    if callable_obj is None:
      return context
    with context:
      callable_obj(*args, **kwargs)

  def assertRaisesWithLiteralMatch(self, expected_exception,
                                   expected_exception_message,
                                   callable_obj=None, *args, **kwargs):
    # pylint: disable=g-doc-args
    """Asserts that the message in a raised exception equals the given string.

    Unlike assertRaisesRegexp, this method takes a literal string, not
    a regular expression.

    with self.assertRaisesWithLiteralMatch(ExType, 'message'):
      DoSomething()

    Args:
      expected_exception: Exception class expected to be raised.
      expected_exception_message: String message expected in the raised
        exception.  For a raise exception e, expected_exception_message must
        equal str(e).
      callable_obj: Function to be called, or None to return a context.
      args: Extra args.
      kwargs: Extra kwargs.

    Returns:
      A context manager if callable_obj is None. Otherwise, None.

    Raises:
      self.failureException if callable_obj does not raise a macthing exception.
    """
    # pylint: enable=g-doc-args
    def Check(err):
      actual_exception_message = str(err)
      self.assert_(expected_exception_message == actual_exception_message,
                   'Exception message does not match.\n'
                   'Expected: %r\n'
                   'Actual: %r' % (expected_exception_message,
                                   actual_exception_message))

    context = self._AssertRaisesContext(expected_exception, self, Check)
    if callable_obj is None:
      return context
    with context:
      callable_obj(*args, **kwargs)

  def assertRaisesWithRegexpMatch(self, expected_exception, expected_regexp,
                                  callable_obj=None, *args, **kwargs):
    # pylint: disable=g-doc-args
    """Asserts that the message in a raised exception matches the given regexp.

    This is just a wrapper around assertRaisesRegexp. Please use
    assertRaisesRegexp instead of assertRaisesWithRegexpMatch.

    Args:
      expected_exception: Exception class expected to be raised.
      expected_regexp: Regexp (re pattern object or string) expected to be
        found in error message.
      callable_obj: Function to be called, or None to return a context.
      args: Extra args.
      kwargs: Extra keyword args.

    Returns:
      A context manager if callable_obj is None. Otherwise, None.

    Raises:
      self.failureException if callable_obj does not raise a macthing exception.
    """
    # pylint: enable=g-doc-args
    # TODO(user): this is a good candidate for a global search-and-replace.
    return self.assertRaisesRegexp(expected_exception, expected_regexp,
                                   callable_obj, *args, **kwargs)

  def assertContainsInOrder(self, strings, target):
    """Asserts that the strings provided are found in the target in order.

    This may be useful for checking HTML output.

    Args:
      strings: A list of strings, such as [ 'fox', 'dog' ]
      target: A target string in which to look for the strings, such as
        'The quick brown fox jumped over the lazy dog'.
    """
    if not isinstance(strings, list):
      strings = [strings]

    current_index = 0
    last_string = None
    for string in strings:
      index = target.find(str(string), current_index)
      if index == -1 and current_index == 0:
        self.fail("Did not find '%s' in '%s'" %
                  (string, target))
      elif index == -1:
        self.fail("Did not find '%s' after '%s' in '%s'" %
                  (string, last_string, target))
      last_string = string
      current_index = index

  def assertContainsSubsequence(self, container, subsequence):
    """Assert that "container" contains "subsequence" as a subsequence.

    Asserts that big_list contains all the elements of small_list, in order, but
    possibly with other elements interspersed. For example, [1, 2, 3] is a
    subsequence of [0, 0, 1, 2, 0, 3, 0] but not of [0, 0, 1, 3, 0, 2, 0].

    Args:
      container: the list we're testing for subsequence inclusion.
      subsequence: the list we hope will be a subsequence of container.
    """
    first_nonmatching = None
    reversed_container = list(reversed(container))
    subsequence = list(subsequence)

    for e in subsequence:
      if e not in reversed_container:
        first_nonmatching = e
        break
      while e != reversed_container.pop():
        pass

    if first_nonmatching is not None:
      self.fail('%s not a subsequence of %s. First non-matching element: %s' %
          (subsequence, container, first_nonmatching))

  def assertTotallyOrdered(self, *groups):
    # pylint: disable=g-doc-args
    """Asserts that total ordering has been implemented correctly.

    For example, say you have a class A that compares only on its attribute x.
    Comparators other than __lt__ are omitted for brevity.

    class A(object):
      def __init__(self, x, y):
        self.x = xio
        self.y = y

      def __hash__(self):
        return hash(self.x)

      def __lt__(self, other):
        try:
          return self.x < other.x
        except AttributeError:
          return NotImplemented

    assertTotallyOrdered will check that instances can be ordered correctly.
    For example,

    self.assertTotallyOrdered(
      [None],  # None should come before everything else.
      [1],     # Integers sort earlier.
      [A(1, 'a')],
      [A(2, 'b')],  # 2 is after 1.
      [A(3, 'c'), A(3, 'd')],  # The second argument is irrelevant.
      [A(4, 'z')],
      ['foo'])  # Strings sort last.

    Args:
     groups: A list of groups of elements.  Each group of elements is a list
       of objects that are equal.  The elements in each group must be less than
       the elements in the group after it.  For example, these groups are
       totally ordered: [None], [1], [2, 2], [3].
    """
    # pylint: enable=g-doc-args

    def CheckOrder(small, big):
      """Ensures small is ordered before big."""
      self.assertFalse(small == big,
                       '%r unexpectedly equals %r' % (small, big))
      self.assertTrue(small != big,
                      '%r unexpectedly equals %r' % (small, big))
      self.assertLess(small, big)
      self.assertFalse(big < small,
                       '%r unexpectedly less than %r' % (big, small))
      self.assertLessEqual(small, big)
      self.assertFalse(big <= small,
                       '%r unexpectedly less than or equal to %r'
                       % (big, small))
      self.assertGreater(big, small)
      self.assertFalse(small > big,
                       '%r unexpectedly greater than %r' % (small, big))
      self.assertGreaterEqual(big, small)
      self.assertFalse(small >= big,
                       '%r unexpectedly greater than or equal to %r'
                       % (small, big))

    def CheckEqual(a, b):
      """Ensures that a and b are equal."""
      self.assertEqual(a, b)
      self.assertFalse(a != b, '%r unexpectedly equals %r' % (a, b))
      self.assertEqual(hash(a), hash(b),
                       'hash %d of %r unexpectedly not equal to hash %d of %r'
                       % (hash(a), a, hash(b), b))
      self.assertFalse(a < b, '%r unexpectedly less than %r' % (a, b))
      self.assertFalse(b < a, '%r unexpectedly less than %r' % (b, a))
      self.assertLessEqual(a, b)
      self.assertLessEqual(b, a)
      self.assertFalse(a > b, '%r unexpectedly greater than %r' % (a, b))
      self.assertFalse(b > a, '%r unexpectedly greater than %r' % (b, a))
      self.assertGreaterEqual(a, b)
      self.assertGreaterEqual(b, a)

    # For every combination of elements, check the order of every pair of
    # elements.
    for elements in itertools.product(*groups):
      elements = list(elements)
      for index, small in enumerate(elements[:-1]):
        for big in elements[index + 1:]:
          CheckOrder(small, big)

    # Check that every element in each group is equal.
    for group in groups:
      for a in group:
        CheckEqual(a, a)
      for a, b in itertools.product(group, group):
        CheckEqual(a, b)

  def assertDictEqual(self, a, b, msg=None):
    """Raises AssertionError if a and b are not equal dictionaries.

    Args:
      a: A dict, the expected value.
      b: A dict, the actual value.
      msg: An optional str, the associated message.

    Raises:
      AssertionError: if the dictionaries are not equal.
    """
    self.assertIsInstance(a, dict, 'First argument is not a dictionary')
    self.assertIsInstance(b, dict, 'Second argument is not a dictionary')

    def Sorted(list_of_items):
      try:
        return sorted(list_of_items)  # In 3.3, unordered are possible.
      except TypeError:
        return list_of_items

    if a == b:
      return
    a_items = Sorted(list(a.iteritems()))
    b_items = Sorted(list(b.iteritems()))

    unexpected = []
    missing = []
    different = []

    safe_repr = unittest.util.safe_repr

    def Repr(dikt):
      """Deterministic repr for dict."""
      # Sort the entries based on their repr, not based on their sort order,
      # which will be non-deterministic across executions, for many types.
      entries = sorted((safe_repr(k), safe_repr(v))
                       for k, v in dikt.iteritems())
      return '{%s}' % (', '.join('%s: %s' % pair for pair in entries))

    message = ['%s != %s%s' % (Repr(a), Repr(b), ' (%s)' % msg if msg else '')]

    # The standard library default output confounds lexical difference with
    # value difference; treat them separately.
    for a_key, a_value in a_items:
      if a_key not in b:
        missing.append((a_key, a_value))
      elif a_value != b[a_key]:
        different.append((a_key, a_value, b[a_key]))

    for b_key, b_value in b_items:
      if b_key not in a:
        unexpected.append((b_key, b_value))

    if unexpected:
      message.append(
          'Unexpected, but present entries:\n%s' % ''.join(
              '%s: %s\n' % (safe_repr(k), safe_repr(v)) for k, v in unexpected))

    if different:
      message.append(
          'repr() of differing entries:\n%s' % ''.join(
              '%s: %s != %s\n' % (safe_repr(k), safe_repr(a_value),
                                  safe_repr(b_value))
              for k, a_value, b_value in different))

    if missing:
      message.append(
          'Missing entries:\n%s' % ''.join(
              ('%s: %s\n' % (safe_repr(k), safe_repr(v)) for k, v in missing)))

    raise self.failureException('\n'.join(message))

  def assertUrlEqual(self, a, b):
    """Asserts that urls are equal, ignoring ordering of query params."""
    parsed_a = urlparse.urlparse(a)
    parsed_b = urlparse.urlparse(b)
    self.assertEqual(parsed_a.scheme, parsed_b.scheme)
    self.assertEqual(parsed_a.netloc, parsed_b.netloc)
    self.assertEqual(parsed_a.path, parsed_b.path)
    self.assertEqual(parsed_a.fragment, parsed_b.fragment)
    self.assertEqual(sorted(parsed_a.params.split(';')),
                     sorted(parsed_b.params.split(';')))
    self.assertDictEqual(
        urlparse.parse_qs(parsed_a.query, keep_blank_values=True),
        urlparse.parse_qs(parsed_b.query, keep_blank_values=True))

  def assertSameStructure(self, a, b, aname='a', bname='b', msg=None):
    """Asserts that two values contain the same structural content.

    The two arguments should be data trees consisting of trees of dicts and
    lists. They will be deeply compared by walking into the contents of dicts
    and lists; other items will be compared using the == operator.
    If the two structures differ in content, the failure message will indicate
    the location within the structures where the first difference is found.
    This may be helpful when comparing large structures.

    Args:
      a: The first structure to compare.
      b: The second structure to compare.
      aname: Variable name to use for the first structure in assertion messages.
      bname: Variable name to use for the second structure.
      msg: Additional text to include in the failure message.
    """

    # Accumulate all the problems found so we can report all of them at once
    # rather than just stopping at the first
    problems = []

    _WalkStructureForProblems(a, b, aname, bname, problems)

    # Avoid spamming the user toooo much
    max_problems_to_show = self.maxDiff // 80
    if len(problems) > max_problems_to_show:
      problems = problems[0:max_problems_to_show-1] + ['...']

    if problems:
      failure_message = '; '.join(problems)
      if msg:
        failure_message += (': ' + msg)
      self.fail(failure_message)

  def assertJsonEqual(self, first, second, msg=None):
    """Asserts that the JSON objects defined in two strings are equal.

    A summary of the differences will be included in the failure message
    using assertSameStructure.

    Args:
      first: A string contining JSON to decode and compare to second.
      second: A string contining JSON to decode and compare to first.
      msg: Additional text to include in the failure message.
    """
    try:
      first_structured = json.loads(first)
    except ValueError as e:
      raise ValueError('could not decode first JSON value %s: %s' %
                       (first, e))

    try:
      second_structured = json.loads(second)
    except ValueError as e:
      raise ValueError('could not decode second JSON value %s: %s' %
                       (second, e))

    self.assertSameStructure(first_structured, second_structured,
                             aname='first', bname='second', msg=msg)

  def getRecordedProperties(self):
    """Return any properties that the user has recorded."""
    return self.__recorded_properties

  def recordProperty(self, property_name, property_value):
    """Record an arbitrary property for later use.

    Args:
      property_name: str, name of property to record; must be a valid XML
        attribute name
      property_value: value of property; must be valid XML attribute value
    """
    self.__recorded_properties[property_name] = property_value

  def _getAssertEqualityFunc(self, first, second):
    try:
      return super(TestCase, self)._getAssertEqualityFunc(first, second)
    except AttributeError:
      # This happens if unittest2.TestCase.__init__ was never run. It
      # usually means that somebody created a subclass just for the
      # assertions and has overriden __init__. "assertTrue" is a safe
      # value that will not make __init__ raise a ValueError (this is
      # a bit hacky).
      test_method = getattr(self, '_testMethodName', 'assertTrue')
      super(TestCase, self).__init__(test_method)

    return super(TestCase, self)._getAssertEqualityFunc(first, second)


# This is not really needed here, but some unrelated code calls this
# function.
# TODO(user): sort it out.
def _SortedListDifference(expected, actual):
  """Finds elements in only one or the other of two, sorted input lists.

  Returns a two-element tuple of lists.  The first list contains those
  elements in the "expected" list but not in the "actual" list, and the
  second contains those elements in the "actual" list but not in the
  "expected" list.  Duplicate elements in either input list are ignored.

  Args:
    expected:  The list we expected.
    actual:  The list we actualy got.
  Returns:
    (missing, unexpected)
    missing: items in expected that are not in actual.
    unexpected: items in actual that are not in expected.
  """
  i = j = 0
  missing = []
  unexpected = []
  while True:
    try:
      e = expected[i]
      a = actual[j]
      if e < a:
        missing.append(e)
        i += 1
        while expected[i] == e:
          i += 1
      elif e > a:
        unexpected.append(a)
        j += 1
        while actual[j] == a:
          j += 1
      else:
        i += 1
        try:
          while expected[i] == e:
            i += 1
        finally:
          j += 1
          while actual[j] == a:
            j += 1
    except IndexError:
      missing.extend(expected[i:])
      unexpected.extend(actual[j:])
      break
  return missing, unexpected


# ----------------------------------------------------------------------
# Functions to compare the actual output of a test to the expected
# (golden) output.
#
# Note: We could just replace the sys.stdout and sys.stderr objects,
# but we actually redirect the underlying file objects so that if the
# Python script execs any subprocess, their output will also be
# redirected.
#
# Usage:
#   basetest.CaptureTestStdout()
#   ... do something ...
#   basetest.DiffTestStdout("... path to golden file ...")
# ----------------------------------------------------------------------


class CapturedStream(object):
  """A temporarily redirected output stream."""

  def __init__(self, stream, filename):
    self._stream = stream
    self._fd = stream.fileno()
    self._filename = filename

    # Keep original stream for later
    self._uncaptured_fd = os.dup(self._fd)

    # Open file to save stream to
    cap_fd = os.open(self._filename,
                     os.O_CREAT | os.O_TRUNC | os.O_WRONLY,
                     0600)

    # Send stream to this file
    self._stream.flush()
    os.dup2(cap_fd, self._fd)
    os.close(cap_fd)

  def RestartCapture(self):
    """Resume capturing output to a file (after calling StopCapture)."""
    # Original stream fd
    assert self._uncaptured_fd

    # Append stream to file
    cap_fd = os.open(self._filename,
                     os.O_CREAT | os.O_APPEND | os.O_WRONLY,
                     0600)

    # Send stream to this file
    self._stream.flush()
    os.dup2(cap_fd, self._fd)
    os.close(cap_fd)

  def StopCapture(self):
    """Remove output redirection."""
    self._stream.flush()
    os.dup2(self._uncaptured_fd, self._fd)

  def filename(self):
    return self._filename

  def __del__(self):
    self.StopCapture()
    os.close(self._uncaptured_fd)
    del self._uncaptured_fd


_captured_streams = {}


def _CaptureTestOutput(stream, filename):
  """Redirect an output stream to a file.

  Args:
    stream: Should be sys.stdout or sys.stderr.
    filename: File where output should be stored.
  """
  assert not _captured_streams.has_key(stream)
  _captured_streams[stream] = CapturedStream(stream, filename)


def _StopCapturingStream(stream):
  """Stops capturing the given output stream.

  Args:
    stream: Should be sys.stdout or sys.stderr.
  """
  assert _captured_streams.has_key(stream)
  for cap_stream in _captured_streams.itervalues():
    cap_stream.StopCapture()


def _DiffTestOutput(stream, golden_filename):
  """Compare ouput of redirected stream to contents of golden file.

  Args:
    stream: Should be sys.stdout or sys.stderr.
    golden_filename: Absolute path to golden file.
  """
  _StopCapturingStream(stream)

  cap = _captured_streams[stream]
  try:
    _Diff(cap.filename(), golden_filename)
  finally:
    # remove the current stream
    del _captured_streams[stream]
    # restore other stream capture
    for cap_stream in _captured_streams.itervalues():
      cap_stream.RestartCapture()


# We want to emit exactly one notice to stderr telling the user where to look
# for their stdout or stderr that may have been consumed to aid debugging.
_notified_test_output_path = ''


def _MaybeNotifyAboutTestOutput(outdir):
  global _notified_test_output_path
  if _notified_test_output_path != outdir:
    _notified_test_output_path = outdir
    sys.stderr.write('\nNOTE: Some tests capturing output into: %s\n' % outdir)


class _DiffingTestOutputContext(object):

  def __init__(self, diff_fn):
    self._diff_fn = diff_fn

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_value, tb):
    self._diff_fn()
    return True


# Public interface
def CaptureTestStdout(outfile='', expected_output_filepath=None):
  """Capture the stdout stream to a file.

  If expected_output_filepath, then this function returns a context manager
  that stops capturing and performs a diff when the context is exited.

    with basetest.CaptureTestStdout(expected_output_filepath=some_filepath):
      sys.stdout.print(....)

  Otherwise, StopCapturing() must be called to stop capturing stdout, and then
  DiffTestStdout() must be called to do the comparison.

  Args:
    outfile: The path to the local filesystem file to which to capture output;
        if omitted, a standard filepath in --test_tmpdir will be used.
    expected_output_filepath: The path to the local filesystem file containing
        the expected output to be diffed against when the context is exited.
  Returns:
    A context manager if expected_output_filepath is specified, otherwise
        None.
  """
  if not outfile:
    outfile = os.path.join(FLAGS.test_tmpdir, 'captured.out')
    outdir = FLAGS.test_tmpdir
  else:
    outdir = os.path.dirname(outfile)
  _MaybeNotifyAboutTestOutput(outdir)
  _CaptureTestOutput(sys.stdout, outfile)
  if expected_output_filepath is not None:
    return _DiffingTestOutputContext(
        lambda: DiffTestStdout(expected_output_filepath))


def CaptureTestStderr(outfile='', expected_output_filepath=None):
  """Capture the stderr stream to a file.

  If expected_output_filepath, then this function returns a context manager
  that stops capturing and performs a diff when the context is exited.

    with basetest.CaptureTestStderr(expected_output_filepath=some_filepath):
      sys.stderr.print(....)

  Otherwise, StopCapturing() must be called to stop capturing stderr, and then
  DiffTestStderr() must be called to do the comparison.

  Args:
    outfile: The path to the local filesystem file to which to capture output;
        if omitted, a standard filepath in --test_tmpdir will be used.
    expected_output_filepath: The path to the local filesystem file containing
        the expected output, to be diffed against when the context is exited.
  Returns:
    A context manager if expected_output_filepath is specified, otherwise
        None.
  """
  if not outfile:
    outfile = os.path.join(FLAGS.test_tmpdir, 'captured.err')
    outdir = FLAGS.test_tmpdir
  else:
    outdir = os.path.dirname(outfile)
  _MaybeNotifyAboutTestOutput(outdir)
  _CaptureTestOutput(sys.stderr, outfile)
  if expected_output_filepath is not None:
    return _DiffingTestOutputContext(
        lambda: DiffTestStderr(expected_output_filepath))


def DiffTestStdout(golden):
  _DiffTestOutput(sys.stdout, golden)


def DiffTestStderr(golden):
  _DiffTestOutput(sys.stderr, golden)


def StopCapturing():
  """Stop capturing redirected output.  Debugging sucks if you forget!"""
  while _captured_streams:
    _, cap_stream = _captured_streams.popitem()
    cap_stream.StopCapture()
    del cap_stream


def _WriteTestData(data, filename):
  """Write data into file named filename."""
  fd = os.open(filename, os.O_CREAT | os.O_TRUNC | os.O_WRONLY, 0o600)
  if not isinstance(data, (bytes, bytearray)):
    data = data.encode('utf-8')
  os.write(fd, data)
  os.close(fd)


_INT_TYPES = (int, long)  # Sadly there is no types.IntTypes defined for us.


def _WalkStructureForProblems(a, b, aname, bname, problem_list):
  """The recursive comparison behind assertSameStructure."""
  if type(a) != type(b) and not (
      isinstance(a, _INT_TYPES) and isinstance(b, _INT_TYPES)):
    # We do not distinguish between int and long types as 99.99% of Python 2
    # code should never care.  They collapse into a single type in Python 3.
    problem_list.append('%s is a %r but %s is a %r' %
                        (aname, type(a), bname, type(b)))
    # If they have different types there's no point continuing
    return

  if isinstance(a, collections.Mapping):
    for k in a:
      if k in b:
        _WalkStructureForProblems(a[k], b[k],
                                  '%s[%r]' % (aname, k), '%s[%r]' % (bname, k),
                                  problem_list)
      else:
        problem_list.append('%s has [%r] but %s does not' % (aname, k, bname))
    for k in b:
      if k not in a:
        problem_list.append('%s lacks [%r] but %s has it' % (aname, k, bname))

  # Strings are Sequences but we'll just do those with regular !=
  elif isinstance(a, collections.Sequence) and not isinstance(a, basestring):
    minlen = min(len(a), len(b))
    for i in xrange(minlen):
      _WalkStructureForProblems(a[i], b[i],
                                '%s[%d]' % (aname, i), '%s[%d]' % (bname, i),
                                problem_list)
    for i in xrange(minlen, len(a)):
      problem_list.append('%s has [%i] but %s does not' % (aname, i, bname))
    for i in xrange(minlen, len(b)):
      problem_list.append('%s lacks [%i] but %s has it' % (aname, i, bname))

  else:
    if a != b:
      problem_list.append('%s is %r but %s is %r' % (aname, a, bname, b))


class OutputDifferedError(AssertionError):
  pass


class DiffFailureError(Exception):
  pass


def _DiffViaExternalProgram(lhs, rhs, external_diff):
  """Compare two files using an external program; raise if it reports error."""
  # The behavior of this function matches the old _Diff() method behavior
  # when a TEST_DIFF environment variable was set.  A few old things at
  # Google depended on that functionality.
  command = [external_diff, lhs, rhs]
  try:
    subprocess.check_output(command, close_fds=True, stderr=subprocess.STDOUT)
    return True  # No diffs.
  except subprocess.CalledProcessError as error:
    failure_output = error.output
    if error.returncode == 1:
      raise OutputDifferedError('\nRunning %s\n%s\nTest output differed from'
                                ' golden file\n' % (command, failure_output))
  except EnvironmentError as error:
    failure_output = str(error)

  # Running the program failed in some way that wasn't a diff.
  raise DiffFailureError('\nRunning %s\n%s\nFailure diffing test output'
                         ' with golden file\n' % (command, failure_output))


def _Diff(lhs, rhs):
  """Given two pathnames, compare two files.  Raise if they differ."""
  # Some people rely on being able to specify TEST_DIFF in the environment to
  # have tests use their own diff wrapper for use when updating golden data.
  external_diff = os.environ.get('TEST_DIFF')
  if external_diff:
    return _DiffViaExternalProgram(lhs, rhs, external_diff)
  try:
    with open(lhs, 'r') as lhs_f:
      with open(rhs, 'r') as rhs_f:
        diff_text = ''.join(
            difflib.unified_diff(lhs_f.readlines(), rhs_f.readlines()))
    if not diff_text:
      return True
    raise OutputDifferedError('\nComparing %s and %s\nTest output differed '
                              'from golden file:\n%s' % (lhs, rhs, diff_text))
  except EnvironmentError as error:
    # Unable to read the files.
    raise DiffFailureError('\nComparing %s and %s\nFailure diffing test output '
                           'with golden file: %s\n' % (lhs, rhs, error))


def DiffTestStringFile(data, golden):
  """Diff data agains a golden file."""
  data_file = os.path.join(FLAGS.test_tmpdir, 'provided.dat')
  _WriteTestData(data, data_file)
  _Diff(data_file, golden)


def DiffTestStrings(data1, data2):
  """Diff two strings."""
  diff_text = ''.join(
      difflib.unified_diff(data1.splitlines(True), data2.splitlines(True)))
  if not diff_text:
    return
  raise OutputDifferedError('\nTest strings differed:\n%s' % diff_text)


def DiffTestFiles(testgen, golden):
  _Diff(testgen, golden)


def GetCommandString(command):
  """Returns an escaped string that can be used as a shell command.

  Args:
    command: List or string representing the command to run.
  Returns:
    A string suitable for use as a shell command.
  """
  if isinstance(command, types.StringTypes):
    return command
  else:
    return shellutil.ShellEscapeList(command)


def GetCommandStderr(command, env=None, close_fds=True):
  """Runs the given shell command and returns a tuple.

  Args:
    command: List or string representing the command to run.
    env: Dictionary of environment variable settings.
    close_fds: Whether or not to close all open fd's in the child after forking.

  Returns:
    Tuple of (exit status, text printed to stdout and stderr by the command).
  """
  if env is None: env = {}
  # Forge needs PYTHON_RUNFILES in order to find the runfiles directory when a
  # Python executable is run by a Python test.  Pass this through from the
  # parent environment if not explicitly defined.
  if os.environ.get('PYTHON_RUNFILES') and not env.get('PYTHON_RUNFILES'):
    env['PYTHON_RUNFILES'] = os.environ['PYTHON_RUNFILES']

  use_shell = isinstance(command, types.StringTypes)
  process = subprocess.Popen(
      command,
      close_fds=close_fds,
      env=env,
      shell=use_shell,
      stderr=subprocess.STDOUT,
      stdout=subprocess.PIPE)
  output = process.communicate()[0]
  exit_status = process.wait()
  return (exit_status, output)


def _QuoteLongString(s):
  """Quotes a potentially multi-line string to make the start and end obvious.

  Args:
    s: A string.

  Returns:
    The quoted string.
  """
  if isinstance(s, (bytes, bytearray)):
    try:
      s = s.decode('utf-8')
    except UnicodeDecodeError:
      s = str(s)
  return ('8<-----------\n' +
          s + '\n' +
          '----------->8\n')


class TestProgramManualRun(unittest.TestProgram):
  """A TestProgram which runs the tests manually."""

  def runTests(self, do_run=False):
    """Run the tests."""
    if do_run:
      unittest.TestProgram.runTests(self)


def main(*args, **kwargs):
  # pylint: disable=g-doc-args
  """Executes a set of Python unit tests.

  Usually this function is called without arguments, so the
  unittest.TestProgram instance will get created with the default settings,
  so it will run all test methods of all TestCase classes in the __main__
  module.


  Args:
    args: Positional arguments passed through to unittest.TestProgram.__init__.
    kwargs: Keyword arguments passed through to unittest.TestProgram.__init__.
  """
  # pylint: enable=g-doc-args
  _RunInApp(RunTests, args, kwargs)


def _IsInAppMain():
  """Returns True iff app.main or app.really_start is active."""
  f = sys._getframe().f_back  # pylint: disable=protected-access
  app_dict = app.__dict__
  while f:
    if f.f_globals is app_dict and f.f_code.co_name in ('run', 'really_start'):
      return True
    f = f.f_back
  return False


class SavedFlag(object):
  """Helper class for saving and restoring a flag value."""

  def __init__(self, flag):
    self.flag = flag
    self.value = flag.value
    self.present = flag.present

  def RestoreFlag(self):
    self.flag.value = self.value
    self.flag.present = self.present




def _RunInApp(function, args, kwargs):
  """Executes a set of Python unit tests, ensuring app.really_start.

  Most users should call basetest.main() instead of _RunInApp.

  _RunInApp calculates argv to be the command-line arguments of this program
  (without the flags), sets the default of FLAGS.alsologtostderr to True,
  then it calls function(argv, args, kwargs), making sure that `function'
  will get called within app.run() or app.really_start(). _RunInApp does this
  by checking whether it is called by either app.run() or
  app.really_start(), or by calling app.really_start() explicitly.

  The reason why app.really_start has to be ensured is to make sure that
  flags are parsed and stripped properly, and other initializations done by
  the app module are also carried out, no matter if basetest.run() is called
  from within or outside app.run().

  If _RunInApp is called from within app.run(), then it will reparse
  sys.argv and pass the result without command-line flags into the argv
  argument of `function'. The reason why this parsing is needed is that
  __main__.main() calls basetest.main() without passing its argv. So the
  only way _RunInApp could get to know the argv without the flags is that
  it reparses sys.argv.

  _RunInApp changes the default of FLAGS.alsologtostderr to True so that the
  test program's stderr will contain all the log messages unless otherwise
  specified on the command-line. This overrides any explicit assignment to
  FLAGS.alsologtostderr by the test program prior to the call to _RunInApp()
  (e.g. in __main__.main).

  Please note that _RunInApp (and the function it calls) is allowed to make
  changes to kwargs.

  Args:
    function: basetest.RunTests or a similar function. It will be called as
        function(argv, args, kwargs) where argv is a list containing the
        elements of sys.argv without the command-line flags.
    args: Positional arguments passed through to unittest.TestProgram.__init__.
    kwargs: Keyword arguments passed through to unittest.TestProgram.__init__.
  """
  if faulthandler:
    try:
      faulthandler.enable()
    except Exception as e:  # pylint: disable=broad-except
      sys.stderr.write('faulthandler.enable() failed %r; ignoring.\n' % e)
    else:
      faulthandler.register(signal.SIGTERM)
  if _IsInAppMain():
    # Save command-line flags so the side effects of FLAGS(sys.argv) can be
    # undone.
    saved_flags = dict((f.name, SavedFlag(f))
                       for f in FLAGS.FlagDict().itervalues())

    # Here we'd like to change the default of alsologtostderr from False to
    # True, so the test programs's stderr will contain all the log messages.
    # The desired effect is that if --alsologtostderr is not specified in
    # the command-line, and __main__.main doesn't set FLAGS.logtostderr
    # before calling us (basetest.main), then our changed default takes
    # effect and alsologtostderr becomes True.
    #
    # However, we cannot achive this exact effect, because here we cannot
    # distinguish these situations:
    #
    # A. main.__main__ has changed it to False, it hasn't been specified on
    #    the command-line, and the default was kept as False. We should keep
    #    it as False.
    #
    # B. main.__main__ hasn't changed it, it hasn't been specified on the
    #    command-line, and the default was kept as False. We should change
    #    it to True here.
    #
    # As a workaround, we assume that main.__main__ never changes
    # FLAGS.alsologstderr to False, thus the value of the flag is determined
    # by its default unless the command-line overrides it. We want to change
    # the default to True, and we do it by setting the flag value to True, and
    # letting the command-line override it in FLAGS(sys.argv) below by not
    # restoring it in saved_flag.RestoreFlag().
    if 'alsologtostderr' in saved_flags:
      FLAGS.alsologtostderr = True
      del saved_flags['alsologtostderr']

    # The call FLAGS(sys.argv) parses sys.argv, returns the arguments
    # without the flags, and -- as a side effect -- modifies flag values in
    # FLAGS. We don't want the side effect, because we don't want to
    # override flag changes the program did (e.g. in __main__.main)
    # after the command-line has been parsed. So we have the for loop below
    # to change back flags to their old values.
    argv = FLAGS(sys.argv)
    for saved_flag in saved_flags.itervalues():
      saved_flag.RestoreFlag()


    function(argv, args, kwargs)
  else:
    # Send logging to stderr. Use --alsologtostderr instead of --logtostderr
    # in case tests are reading their own logs.
    if 'alsologtostderr' in FLAGS:
      FLAGS.SetDefault('alsologtostderr', True)

    def Main(argv):
      function(argv, args, kwargs)

    app.really_start(main=Main)


def RunTests(argv, args, kwargs):
  """Executes a set of Python unit tests within app.really_start.

  Most users should call basetest.main() instead of RunTests.

  Please note that RunTests should be called from app.really_start (which is
  called from app.run()). Calling basetest.main() would ensure that.

  Please note that RunTests is allowed to make changes to kwargs.

  Args:
    argv: sys.argv with the command-line flags removed from the front, i.e. the
      argv with which app.run() has called __main__.main.
    args: Positional arguments passed through to unittest.TestProgram.__init__.
    kwargs: Keyword arguments passed through to unittest.TestProgram.__init__.
  """
  test_runner = kwargs.get('testRunner')

  # Make sure tmpdir exists
  if not os.path.isdir(FLAGS.test_tmpdir):
    os.makedirs(FLAGS.test_tmpdir)

  # Run main module setup, if it exists
  main_mod = sys.modules['__main__']
  if hasattr(main_mod, 'setUp') and callable(main_mod.setUp):
    main_mod.setUp()

  # Let unittest.TestProgram.__init__ called by
  # TestProgramManualRun.__init__ do its own argv parsing, e.g. for '-v',
  # on argv, which is sys.argv without the command-line flags.
  kwargs.setdefault('argv', argv)

  try:
    result = None
    test_program = TestProgramManualRun(*args, **kwargs)
    if test_runner:
      test_program.testRunner = test_runner
    else:
      test_program.testRunner = unittest.TextTestRunner(
          verbosity=test_program.verbosity)
    result = test_program.testRunner.run(test_program.test)
  finally:
    # Run main module teardown, if it exists
    if hasattr(main_mod, 'tearDown') and callable(main_mod.tearDown):
      main_mod.tearDown()

  sys.exit(not result.wasSuccessful())
