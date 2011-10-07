# -*- coding: utf-8 -*-
"""
    ponyguruma._highlevel
    ~~~~~~~~~~~~~~~~~~~~~

    Oniguruma Bindings for Python

    :copyright: Copyright 2007 by Armin Ronacher, Georg Brandl.
    :license: BSD.
"""
from warnings import warn

from ponyguruma.constants import OPTION_NONE, ENCODING_ASCII, SYNTAX_DEFAULT
from ponyguruma._lowlevel import *


class RegexpWarning(RegexpError):
    __module__ = 'ponyguruma'


class CalculatedProperty(object):

    def __init__(self, func):
        self.func = func
        self.__name__ = func.func_name
        self.__doc__ = func.__doc__

    def __get__(self, obj, type=None):
        if obj is None:
            return self
        value = self.func(obj)
        setattr(obj, self.__name__, value)
        return value

    def __repr__(self):
        return '<%s %r>' % (
            self.__class__.__name__,
            self.__name__
        )


class Regexp(BaseRegexp):
    """
    Holds one pattern.
    """
    __module__ = 'ponyguruma'

    # __new__ signature:
    # def __new__(cls, pattern, flags=OPTION_NONE,
    #             encoding=ENCODING_ASCII, syntax=SYNTAX_DEFAULT)

    def factory(cls, flags=OPTION_NONE, encoding=ENCODING_ASCII,
                syntax=SYNTAX_DEFAULT):
        """
        Return a factory function that creates Regexp objects with a defined
        set of Oniguruma flags, encoding and syntax.
        """
        def create(pattern):
            return cls(pattern, flags, encoding, syntax)
        return create
    factory = classmethod(factory)

    def match(self, string, pos=0, endpos=-1):
        """
        If zero or more characters at the beginning of `string` match
        the regular expression pattern, return a corresponding `Match`
        instance.  Return `None` if the string does not match the pattern;
        note that this is different from a zero-length match.

        If pos is given it will start matching at exactly that position,
        if endpos is a positive integer it will stop matching at that
        position.

        **Note:** If you want to locate a match anywhere in `string`
        you should use `search` instead.
        """
        state = regexp_match(self, string, pos, endpos, True)
        if state is not None:
            return Match(state)

    def search(self, string, pos=0, endpos=-1):
        """
        Scan through `string` looking for a location where the regular
        expression pattern produces a match, and return a corresponding
        `Match` instance.  Return `None` if no position in the string
        matches the pattern; note that this is different from finding a
        zero-length match at some point in the string.

        The pos and endpos parameters can be used to limit the search range.
        """
        state = regexp_match(self, string, pos, endpos, False)
        if state is not None:
            return Match(state)

    def find(self, string, pos=0, endpos=-1):
        """
        Return an iterator yielding `Match` instances over all
        non-overlapping matches for the pattern in string.  Empty matches
        are included in the result unless they touch the beginning of
        another match.
        """
        while 1:
            state = regexp_match(self, string, pos, endpos, False)
            if state is None:
                return
            m = Match(state)
            pos = m.end()
            yield m

    def findstrings(self, string, pos=0, endpos=-1):
        """
        Like find but yields the string value of the matches.
        """
        for match in self.find(string, pos, endpos):
            yield match.group()

    def subn(self, repl, string, count=0, pos=0, endpos=-1):
        """
        Perform the same operation as `sub()`, but return a tuple
        ``(new_string, number_of_subs_made)``.
        """
        new = [string[:pos]]
        if not callable(repl):
            if '\\' in repl:
                repl = lambda m, r=repl: m.expand(r)
            else:
                repl = lambda m, r=repl: r
        if endpos == -1:
            endpos = len(string)
        n = skipped = 0
        while 1:
            state = regexp_match(self, string, pos, endpos, False)
            if state is None:
                break
            n += 1
            m = Match(state)
            startpos = m.start()
            new.append(string[pos - skipped:startpos])
            pos = m.end()
            if startpos == pos:
                skipped = 1
            else:
                skipped = 0
            new.append(repl(m))
            if n == count or pos - skipped >= endpos:
                break
            pos += skipped
        new.append(string[pos:])
        return ''.join(new), n

    def sub(self, repl, string, count=0, pos=0, endpos=-1):
        r"""
        Return the string obtained by replacing the leftmost
        non-overlapping occurrences of the pattern in string by the
        replacement `repl`.

        If `repr` is a function it will be called with the `Match`
        object instance and has to return a string.  Otherwise `repr`
        must be a string that can have group references (``\1`` or
        ``\g<name>``).
        """
        return self.subn(repl, string, count, pos, endpos)[0]

    def split(self, string, maxsplit=0, pos=0, endpos=-1, flat=False):
        """
        Split string by the occurrences of the pattern.  If capturing
        parentheses are used in pattern, then the text of all groups
        in the pattern are also returned as part of the resulting list.
        If `maxsplit` is nonzero, at most maxsplit splits occur, and the
        remainder of the string is returned as the final element of the list.

        Unless there are captured groups in the pattern the delimiter is not
        part of the returned list.  Groups will appear in the result as a
        tuple of the groups, even if only one group is present.  If you set
        `flat` to `True` the tuples will be merged into the list so that all
        groups become part of the result as strings.
        """
        result = []
        startstring = string[:pos]
        n = 0
        push_match = (flag and result.append or result.extend)
        while 1:
            state = regexp_match(self, string, pos, endpos, False)
            if state is None:
                break
            n += 1
            m = Match(state)
            result.append(string[pos:m.start()])
            if len(m):
                push_match(m.groups)
            pos = m.end()
            if n == maxsplit:
                break
        result.append(string[pos:])
        result[0] = startstring + result[0]
        return result

    def __str__(self):
        return str(self.pattern)

    def __unicode__(self):
        return unicode(self.pattern)

    def __repr__(self):
        return 'Regexp(%r)' % (self.pattern,)


# XXX: not expanding other escapes here... and do not replace \\1
_repl_re = Regexp(r"\\(?:(\d+)|g<(.+?)>)")


class Match(object):
    """
    Wrapper class for the match results.
    """
    __module__ = 'ponyguruma'

    def __init__(self, state):
        self.state = state

    def spans(self):
        """
        A tuple of tuples with the start and end position of the captured
        groups.
        """
        return match_get_groups(self.state)
    spans = CalculatedProperty(spans)

    def groupnames(self):
        """
        A dict for name -> group_number.
        """
        return match_get_group_names(self.state)
    groupnames = CalculatedProperty(groupnames)

    def groups(self):
        """
        A tuple of the group values, ignoring the first group.  Thus the
        regexp ``r'(.)(.)(.)'`` matched against ``abc`` will return
        ``('a', 'b', 'c')`` but not ``('abc', 'a', 'b', 'c')``.
        """
        return tuple([self.group(x) for x in xrange(1, len(self.spans))])
    groups = CalculatedProperty(groups)

    def groupdict(self):
        """
        A dict of all named groups with their corresponding values.
        """
        d = {}
        for key in self.groupnames:
            d[key] = self.group(key)
        return d
    groupdict = CalculatedProperty(groupdict)

    def lastindex(self):
        """
        The integer index of the last matched capturing group, or `None`
        if no group was matched at all.  For example, the expressions
        ``(a)b``, ``((a)(b))``, and ``((ab))`` will have ``lastindex == 1``
        if applied to the string ``'ab'``, while the expression ``(a)(b)``
        will have ``lastindex == 2``, if applied to the same string.
        """
        m = (None, -1)
        for idx, (start, end) in enumerate(self.spans[1:]):
            if end > m[1]:
                m = (idx + 1, end)
        return m[0]
    lastindex = CalculatedProperty(lastindex)

    def lastgroup(self):
        """
        The name of the last matched capturing group, or `None` if the
        group didn't have a name, or if no group was matched at all.
        """
        g = self.lastindex
        if g is not None:
            for name, index in self.groupnames.iteritems():
                if index == g:
                    return name
    lastgroup = CalculatedProperty(lastgroup)

    def expand(self, template):
        """
        Expand a template string.
        """
        def handle(match):
            numeric, named = match.groups
            if numeric:
                return self.group(int(numeric))
            return self.group(named)
        return _repl_re.sub(handle, template)

    def span(self, group=0):
        """
        The span of a single group.  Group can be a string if it's a
        named group, otherwise an integer.  If you omit the value the
        span of the whole match is returned.
        """
        if isinstance(group, basestring):
            group = self.groupnames[group]
        return self.spans[group]

    def start(self, group=0):
        """
        Get the start position of a group or the whole match if no
        group is provided.
        """
        return self.span(group)[0]

    def end(self, group=0):
        """
        Get the end position of a group or the whole match if no group
        is provided.
        """
        return self.span(group)[1]

    def group(self, group=0):
        """
        Return the value of a single group.
        """
        if isinstance(group, basestring):
            group = self.groupnames[group]
        return match_extract_group(self.state, group)

    def re(self):
        """
        The regular expression object that created this match.
        """
        return self.state.regexp
    re = property(re, doc=re.__doc__)

    def string(self):
        """
        The string this match object matches on.
        """
        return self.state.string
    string = property(string, doc=re.__doc__)

    def pos(self):
        """
        The search start position.  This is equivalent to the `pos` parameter of
        the `match()` / `search()` methods that created this match object.

        Don't mix this up with `start()` which gives you the position of the
        actual match begin.
        """
        return self.state.pos
    pos = property(pos, doc=pos.__doc__)

    def endpos(self):
        """
        The search end position.  This is equivalent to the `endpos` parameter of
        the `match()` / `search()` methods that created this match object.

        Don't mix this up with `end()` which gives you the position of the actual
        match end.
        """
        return self.state.endpos
    endpos = property(end, "The match end position.")

    __getitem__ = group

    def __len__(self):
        return len(self.spans) - 1

    def __iter__(self):
        return iter(self.groups)

    def __eq__(self, other):
        return self.__class__ is other.__class__ and \
               self.state is other.state

    def __ne__(self, other):
        return not self.__eq__(other)

    def __nonzero__(self):
        # If this isn't defined, Python checks if __len__() != 0!
        return True

    def __unicode__(self):
        return unicode(self.group(0))

    def __str__(self):
        return str(self.group(0))

    def __repr__(self):
        return '<%s groups: %d, span: (%d, %d)>' % (
            self.__class__.__name__,
            len(self),
            self.start(),
            self.end()
        )


class Scanner(object):
    """
    Simple regular expression based scanner.  This scanner keeps track
    of the current position and continues lexing from here.  Here a
    small example::

        >>> s = Scanner("Hello World")
        >>> s.scan(r'World')
        >>> s.scan(r'Hello')
        <Match groups: 0, span: (0, 5)>
        >>> s.pos
        5
        >>> s.scan(r'World')
        >>> s.pos
        5
        >>> s.scan(r'\s+')
        <Match groups: 0, span: (5, 6)>
        >>> s.eos
        False
        >>> s.scan('.+')
        <Match groups: 0, span: (6, 11)>
        >>> s.eos
        True
    """

    def __init__(self, string):
        self.string = string
        self._cache = {}
        self.reset()

    def eos(self):
        """True if the end of the string is reached."""
        return self.pos >= self.end
    eos = property(eos, doc=eos.__doc__)

    def rest(self):
        """The unscanned string."""
        return self.string[self.pos:]
    rest = property(rest, doc=rest.__doc__)

    def scanned(self):
        """The string that was already scanned."""
        return self.string[:self.pos]
    scanned = property(scanned, doc=scanned.__doc__)

    def reset(self):
        """Reset the scanner."""
        self.pos = 0
        self.old_pos = None
        self.end = len(self.string)
        self.match = None

    def feed(self, string):
        """Add a new string to the string that is scanned."""
        self.string += string
        self.end = len(self.string)

    def check(self, regexp):
        """
        This returns the value that `match` would return, without advancing the scan
        pointer.  Also the match register is not updated.
        """
        if regexp in self._cache:
            re = self._cache[regexp]
        else:
            re = self._cache[regexp] = Regexp(regexp)
        return re.match(self.string, self.pos)

    def scan(self, regexp):
        """
        Tries to match with pattern at the current position. If there's a match, the
        scanner advances the scan pointer and returns the match object.  Otherwise,
        the return value is `None` and the position is unchanged.
        """
        match = self.check(regexp)
        if match is not None:
            self.old_pos = self.pos
            self.pos = match.end()
            self.match = match
            return match

    def skip(self, regexp):
        """
        Works like `scan` but the match is not returned.  The return values is `True`
        if everything went well, otherwise `False`.
        """
        return self.scan(regexp) is not None

    def search(self, regexp):
        """
        Works like `scan` but scans not only at the beginning but it skips until the
        pattern matches.  If the pattern does not match the return value is `None`
        and neither the pointer no the match register is updated.  Otherwise the
        return value is the string skipped and the match register points to the
        used match object.
        """
        if regexp in self._cache:
            re = self._cache[regexp]
        else:
            re = self._cache[regexp] = Regexp(regexp)
        match = re.search(regexp, self.pos)
        if match is not None:
            self.old_pos = start = self.pos
            self.pos = end = match.end()
            self.match = match
            return self.string[start:end]

    def getch(self):
        """
        Get the next character as string or `None` if end is reached.
        """
        rv = self.match(r'(?:.|\n)')
        if rv is not None:
            return rv.group()

    def rewind(self):
        """
        Go one position back. Only one is allowed.
        """
        if self.old_pos is None:
            if self.pos == 0:
                raise RuntimeError('Cannot rewind beyond start position')
            raise RuntimeError('Cannot rewind more than one position back')
        self.pos = self.old_pos
        self.old_pos = None

    def __repr__(self):
        return '<%s %d/%d>' % (
            self.__class__.__name__,
            self.pos,
            self.end
        )


def warn_func(message):
    """
    Called from the C extension on warnings.  If you want to control
    the way warnings are handled you can override `ponyguruma.warn_func`.
    """
    warn(RegexpWarning(message), stacklevel=2)


_special_escapes = {
    '\r':   '\\r',
    '\n':   '\\n',
    '\t':   '\\t',
    '\b':   '[\\b]',
    '\v':   '\\v',
    '\f':   '\\f',
    '\0':   '\\0'
}


def escape(pattern):
    """Escape all non-alphanumeric characters in pattern."""
    s = list(pattern)
    for i, c in enumerate(s):
        if not ('a' <= c <= 'z' or 'A' <= c <= 'Z' or '0' <= c <= '9'):
            if c in _special_escapes:
                s[i] = _special_escapes[c]
            else:
                s[i] = '\\' + c
    return type(pattern)().join(s)


ALL_OBJECTS = ['Regexp', 'Scanner', 'Match', 'RegexpError',
               'RegexpWarning', 'warn_func', 'escape']
__all__ = ALL_OBJECTS + ['ALL_OBJECTS']
