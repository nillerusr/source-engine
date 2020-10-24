#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright 2008 Google Inc. All Rights Reserved.

"""Lightweight routines for producing more friendly output.

Usage examples:

  'New messages: %s' % humanize.Commas(star_count)
    -> 'New messages: 58,192'

  'Found %s.' % humanize.Plural(error_count, 'error')
    -> 'Found 2 errors.'

  'Found %s.' % humanize.Plural(error_count, 'ox', 'oxen')
    -> 'Found 2 oxen.'

  'Copied at %s.' % humanize.DecimalPrefix(rate, 'bps', precision=3)
    -> 'Copied at 42.6 Mbps.'

  'Free RAM: %s' % humanize.BinaryPrefix(bytes_free, 'B')
    -> 'Free RAM: 742 MiB'

  'Finished all tasks in %s.' % humanize.Duration(elapsed_time)
    -> 'Finished all tasks in 34m 5s.'

These libraries are not a substitute for full localization.  If you
need localization, then you will have to think about translating
strings, formatting numbers in different ways, and so on.  Use
ICU if your application is user-facing.  Use these libraries if
your application is an English-only internal tool, and you are
tired of seeing "1 results" or "3450134804 bytes used".

Compare humanize.*Prefix() to C++ utilites HumanReadableNumBytes and
HumanReadableInt in strings/human_readable.h.
"""



import datetime
import math
import re

SIBILANT_ENDINGS = frozenset(['sh', 'ss', 'tch', 'ax', 'ix', 'ex'])
DIGIT_SPLITTER = re.compile(r'\d+|\D+').findall

# These are included because they are common technical terms.
SPECIAL_PLURALS = {
    'index': 'indices',
    'matrix': 'matrices',
    'vertex': 'vertices',
}

VOWELS = frozenset('AEIOUaeiou')


def Commas(value):
  """Formats an integer with thousands-separating commas.

  Args:
    value: An integer.

  Returns:
    A string.
  """
  if value < 0:
    sign = '-'
    value = -value
  else:
    sign = ''
  result = []
  while value >= 1000:
    result.append('%03d' % (value % 1000))
    value /= 1000
  result.append('%d' % value)
  return sign + ','.join(reversed(result))


def Plural(quantity, singular, plural=None):
  """Formats an integer and a string into a single pluralized string.

  Args:
    quantity: An integer.
    singular: A string, the singular form of a noun.
    plural: A string, the plural form.  If not specified, then simple
        English rules of regular pluralization will be used.

  Returns:
    A string.
  """
  return '%d %s' % (quantity, PluralWord(quantity, singular, plural))


def PluralWord(quantity, singular, plural=None):
  """Builds the plural of an English word.

  Args:
    quantity: An integer.
    singular: A string, the singular form of a noun.
    plural: A string, the plural form.  If not specified, then simple
        English rules of regular pluralization will be used.

  Returns:
    the plural form of the word.
  """
  if quantity == 1:
    return singular
  if plural:
    return plural
  if singular in SPECIAL_PLURALS:
    return SPECIAL_PLURALS[singular]

  # We need to guess what the English plural might be.  Keep this
  # function simple!  It doesn't need to know about every possiblity;
  # only regular rules and the most common special cases.
  #
  # Reference: http://en.wikipedia.org/wiki/English_plural

  for ending in SIBILANT_ENDINGS:
    if singular.endswith(ending):
      return '%ses' % singular
  if singular.endswith('o') and singular[-2:-1] not in VOWELS:
    return '%ses' % singular
  if singular.endswith('y') and singular[-2:-1] not in VOWELS:
    return '%sies' % singular[:-1]
  return '%ss' % singular


def WordSeries(words, conjunction='and'):
  """Convert a list of words to an English-language word series.

  Args:
    words: A list of word strings.
    conjunction: A coordinating conjunction.

  Returns:
    A single string containing all the words in the list separated by commas,
    the coordinating conjunction, and a serial comma, as appropriate.
  """
  num_words = len(words)
  if num_words == 0:
    return ''
  elif num_words == 1:
    return words[0]
  elif num_words == 2:
    return (' %s ' % conjunction).join(words)
  else:
    return '%s, %s %s' % (', '.join(words[:-1]), conjunction, words[-1])


def AddIndefiniteArticle(noun):
  """Formats a noun with an appropriate indefinite article.

  Args:
    noun: A string representing a noun.

  Returns:
    A string containing noun prefixed with an indefinite article, e.g.,
    "a thing" or "an object".
  """
  if not noun:
    raise ValueError('argument must be a word: {!r}'.format(noun))
  if noun[0] in VOWELS:
    return 'an ' + noun
  else:
    return 'a ' + noun


def DecimalPrefix(quantity, unit, precision=1, min_scale=0, max_scale=None):
  """Formats an integer and a unit into a string, using decimal prefixes.

  The unit will be prefixed with an appropriate multiplier such that
  the formatted integer is less than 1,000 (as long as the raw integer
  is less than 10**27).  For example:

    DecimalPrefix(576012, 'bps') -> '576 kbps'
    DecimalPrefix(576012, '') -> '576 k'
    DecimalPrefix(576, '') -> '576'
    DecimalPrefix(1574215, 'bps', 2) -> '1.6 Mbps'

  Only the SI prefixes which are powers of 10**3 will be used, so
  DecimalPrefix(100, 'thread') is '100 thread', not '1 hthread'.

  See also:
    BinaryPrefix()

  Args:
    quantity: A number.
    unit: A string, the dimension for quantity, with no multipliers (e.g.
        "bps").  If quantity is dimensionless, the empty string.
    precision: An integer, the minimum number of digits to display.
    min_scale: minimum power of 1000 to scale to, (None = unbounded).
    max_scale: maximum power of 1000 to scale to, (None = unbounded).

  Returns:
    A string, composed by the decimal (scaled) representation of quantity at the
    required precision, possibly followed by a space, the appropriate multiplier
    and the unit.
  """
  return _Prefix(quantity, unit, precision, DecimalScale, min_scale=min_scale,
                 max_scale=max_scale)


def BinaryPrefix(quantity, unit, precision=1):
  """Formats an integer and a unit into a string, using binary prefixes.

  The unit will be prefixed with an appropriate multiplier such that
  the formatted integer is less than 1,024 (as long as the raw integer
  is less than 2**90).  For example:

    BinaryPrefix(576012, 'B') -> '562 KiB'
    BinaryPrefix(576012, '') -> '562 Ki'

  See also:
    DecimalPrefix()

  Args:
    quantity: A number.
    unit: A string, the dimension for quantity, with no multipliers (e.g.
        "B").  If quantity is dimensionless, the empty string.
    precision: An integer, the minimum number of digits to display.

  Returns:
    A string, composed by the decimal (scaled) representation of quantity at the
    required precision, possibly followed by a space, the appropriate multiplier
    and the unit.
  """
  return _Prefix(quantity, unit, precision, BinaryScale)


def _Prefix(quantity, unit, precision, scale_callable, **args):
  """Formats an integer and a unit into a string.

  Args:
    quantity: A number.
    unit: A string, the dimension for quantity, with no multipliers (e.g.
        "bps").  If quantity is dimensionless, the empty string.
    precision: An integer, the minimum number of digits to display.
    scale_callable: A callable, scales the number and units.
    **args: named arguments passed to scale_callable.

  Returns:
    A string.
  """
  separator = ' ' if unit else ''

  if not quantity:
    return '0%s%s' % (separator, unit)

  if quantity in [float('inf'), float('-inf')] or math.isnan(quantity):
    return '%f%s%s' % (quantity, separator, unit)

  scaled_quantity, scaled_unit = scale_callable(quantity, unit, **args)

  if scaled_unit:
    separator = ' '

  print_pattern = '%%.%df%%s%%s' % max(0, (precision - int(
      math.log(abs(scaled_quantity), 10)) - 1))

  return print_pattern % (scaled_quantity, separator, scaled_unit)


# Prefixes and corresponding min_scale and max_scale for decimal formating.
DECIMAL_PREFIXES = ('y', 'z', 'a', 'f', 'p', 'n', u'µ', 'm',
                    '', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y')
DECIMAL_MIN_SCALE = -8
DECIMAL_MAX_SCALE = 8


def DecimalScale(quantity, unit, min_scale=0, max_scale=None):
  """Get the scaled value and decimal prefixed unit in a tupple.

    DecimalScale(576012, 'bps') -> (576.012, 'kbps')
    DecimalScale(1574215, 'bps') -> (1.574215, 'Mbps')

  Args:
    quantity: A number.
    unit: A string.
    min_scale: minimum power of 1000 to normalize to (None = unbounded)
    max_scale: maximum power of 1000 to normalize to (None = unbounded)

  Returns:
    A tuple of a scaled quantity (float) and BinaryPrefix for the
    units (string).
  """
  if min_scale is None or min_scale < DECIMAL_MIN_SCALE:
    min_scale = DECIMAL_MIN_SCALE
  if max_scale is None or max_scale > DECIMAL_MAX_SCALE:
    max_scale = DECIMAL_MAX_SCALE
  powers = DECIMAL_PREFIXES[
      min_scale - DECIMAL_MIN_SCALE:max_scale - DECIMAL_MIN_SCALE + 1]
  return _Scale(quantity, unit, 1000, powers, min_scale)


def BinaryScale(quantity, unit):
  """Get the scaled value and binary prefixed unit in a tupple.

    BinaryPrefix(576012, 'B') -> (562.51171875, 'KiB')

  Args:
    quantity: A number.
    unit: A string.

  Returns:
    A tuple of a scaled quantity (float) and BinaryPrefix for the
    units (string).
  """
  return _Scale(quantity, unit, 1024,
                ('Ki', 'Mi', 'Gi', 'Ti', 'Pi', 'Ei', 'Zi', 'Yi'))


def _Scale(quantity, unit, multiplier, prefixes=None, min_scale=None):
  """Returns the formatted quantity and unit into a tuple.

  Args:
    quantity: A number.
    unit: A string
    multiplier: An integer, the ratio between prefixes.
    prefixes: A sequence of strings.
        If empty or None, no scaling is done.
    min_scale: minimum power of multiplier corresponding to the first prefix.
        If None assumes prefixes are for positive powers only.

  Returns:
    A tuple containing the raw scaled quantity (float) and the prefixed unit.
  """
  if (not prefixes or not quantity or math.isnan(quantity) or
      quantity in [float('inf'), float('-inf')]):
    return float(quantity), unit

  if min_scale is None:
    min_scale = 0
    prefixes = ('',) + tuple(prefixes)
  value, prefix = quantity, ''
  for power, prefix in enumerate(prefixes, min_scale):
    # This is more numerically accurate than '/ multiplier ** power'.
    value = float(quantity) * multiplier ** -power
    if abs(value) < multiplier:
      break
  return value, prefix + unit

# Contains the fractions where the full range [1/n ... (n - 1) / n]
# is defined in Unicode.
FRACTIONS = {
    3: (None, u'⅓', u'⅔', None),
    5: (None, u'⅕', u'⅖', u'⅗', u'⅘', None),
    8: (None, u'⅛', u'¼', u'⅜', u'½', u'⅝', u'¾', u'⅞', None),
}

FRACTION_ROUND_DOWN = 1.0 / (max(FRACTIONS.keys()) * 2.0)
FRACTION_ROUND_UP = 1.0 - FRACTION_ROUND_DOWN


def PrettyFraction(number, spacer=''):
  """Convert a number into a string that might include a unicode fraction.

  This method returns the integer representation followed by the closest
  fraction of a denominator 2, 3, 4, 5 or 8.
  For instance, 0.33 will be converted to 1/3.
  The resulting representation should be less than 1/16 off.

  Args:
    number: a python number
    spacer: an optional string to insert between the integer and the fraction
        default is an empty string.

  Returns:
    a unicode string representing the number.
  """
  # We do not want small negative numbers to display as -0.
  if number < -FRACTION_ROUND_DOWN:
    return u'-%s' % PrettyFraction(-number)
  number = abs(number)
  rounded = int(number)
  fract = number - rounded
  if fract >= FRACTION_ROUND_UP:
    return str(rounded + 1)
  errors_fractions = []
  for denominator, fraction_elements in FRACTIONS.items():
    numerator = int(round(denominator * fract))
    error = abs(fract - (float(numerator) / float(denominator)))
    errors_fractions.append((error, fraction_elements[numerator]))
  unused_error, fraction_text = min(errors_fractions)
  if rounded and fraction_text:
    return u'%d%s%s' % (rounded, spacer, fraction_text)
  if rounded:
    return str(rounded)
  if fraction_text:
    return fraction_text
  return u'0'


def Duration(duration, separator=' '):
  """Formats a nonnegative number of seconds into a human-readable string.

  Args:
    duration: A float duration in seconds.
    separator: A string separator between days, hours, minutes and seconds.

  Returns:
    Formatted string like '5d 12h 30m 45s'.
  """
  try:
    delta = datetime.timedelta(seconds=duration)
  except OverflowError:
    return '>=' + TimeDelta(datetime.timedelta.max)
  return TimeDelta(delta, separator=separator)


def TimeDelta(delta, separator=' '):
  """Format a datetime.timedelta into a human-readable string.

  Args:
    delta: The datetime.timedelta to format.
    separator: A string separator between days, hours, minutes and seconds.

  Returns:
    Formatted string like '5d 12h 30m 45s'.
  """
  parts = []
  seconds = delta.seconds
  if delta.days:
    parts.append('%dd' % delta.days)
  if seconds >= 3600:
    parts.append('%dh' % (seconds // 3600))
    seconds %= 3600
  if seconds >= 60:
    parts.append('%dm' % (seconds // 60))
    seconds %= 60
  seconds += delta.microseconds / 1e6
  if seconds or not parts:
    parts.append('%gs' % seconds)
  return separator.join(parts)


def NaturalSortKey(data):
  """Key function for "natural sort" ordering.

  This key function results in a lexigraph sort. For example:
  - ['1, '3', '20'] (not ['1', '20', '3']).
  - ['Model 9', 'Model 70 SE', 'Model 70 SE2']
    (not ['Model 70 SE', 'Model 70 SE2', 'Model 9']).

  Usage:
    new_list = sorted(old_list, key=humanize.NaturalSortKey)
    or
    list_sort_in_place.sort(key=humanize.NaturalSortKey)

  Based on code by Steven Bazyl <sbazyl@google.com>.

  Args:
    data: str, The key being compared in a sort.

  Returns:
    A list which is comparable to other lists for the purpose of sorting.
  """
  segments = DIGIT_SPLITTER(data)
  for i, value in enumerate(segments):
    if value.isdigit():
      segments[i] = int(value)
  return segments


def UnixTimestamp(unix_ts, tz):
  """Format a UNIX timestamp into a human-readable string.

  Args:
    unix_ts: UNIX timestamp (number of seconds since epoch). May be a floating
        point number.
    tz: datetime.tzinfo object, timezone to use when formatting. Typical uses
        might want to rely on datelib or pytz to provide the tzinfo object, e.g.
        use datelib.UTC, datelib.US_PACIFIC, or pytz.timezone('Europe/Dublin').

  Returns:
    Formatted string like '2013-11-17 11:08:27.720000 PST'.
  """
  date_time = datetime.datetime.fromtimestamp(unix_ts, tz)
  return date_time.strftime('%Y-%m-%d %H:%M:%S.%f %Z')


def AddOrdinalSuffix(value):
  """Adds an ordinal suffix to a non-negative integer (e.g. 1 -> '1st').

  Args:
    value: A non-negative integer.

  Returns:
    A string containing the integer with a two-letter ordinal suffix.
  """
  if value < 0 or value != int(value):
    raise ValueError('argument must be a non-negative integer: %s' % value)

  if value % 100 in (11, 12, 13):
    suffix = 'th'
  else:
    rem = value % 10
    if rem == 1:
      suffix = 'st'
    elif rem == 2:
      suffix = 'nd'
    elif rem == 3:
      suffix = 'rd'
    else:
      suffix = 'th'

  return str(value) + suffix
