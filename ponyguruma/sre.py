# -*- coding: utf-8 -*-
"""
    ponyguruma.sre
    ~~~~~~~~~~~~~~

    sre compatiblity module.

    TODO:

    - convert regular expressions if possible.
    - UNICODE / LOCALE / DOTALL

    :copyright: Copyright 2007 by Armin Ronacher.
    :license: BSD.
"""
from ponyguruma import Regexp, Match, RegexpError, escape, constants


I = IGNORECASE = constants.OPTION_IGNORECASE
M = MULTILINE = constants.OPTION_MULTILINE
X = VERBOSE = constants.OPTION_EXTEND


class SRE_Pattern(Regexp):

    def match(self, string, pos=0, endpos=-1):
        rv = Regexp.match(self, string, pos, endpos)
        if rv is not None:
            return SRE_Match(rv)

    def search(self, string, pos=0, endpos=-1):
        rv = Regexp.search(self, string, pos, endpos)
        if rv is not None:
            return SRE_Match(rv)

    def subn(self, repl, string, count=0, pos=0, endpos=-1):
        if callable(repl):
            def repl(m, r=repl):
                return r(SRE_Match(m))
        return Regexp.subn(self, repl, string, count, pos, endpos)

    def split(self, string, maxsplit=0, pos=0, endpos=-1):
        return Regexp.split(self, string, maxsplit, pos, endpos, True)

    def finditer(self, string, pos=0, endpos=-1):
        for match in Regexp.find(self, string, pos, endpos):
            yield SRE_Match(match)

    def findall(self, string, pos=0, endpos=-1):
        return list(Regexp.findstrings(self, string, pos, endpos))

    def _missing(self):
        raise AttributeError("'%s' object has no attribute 'find'" %
                             self.__class__.__name__)
    find = property(_missing)
    findstrings = property(_missing)
    del _missing

    def __repr__(self):
        return '<ponygurma.sre.SRE_Pattern object at 0x%x>' % \
               (id(self) & 0xffffffff)


class SRE_Match(Match):

    def __init__(self, match):
        Match.__init__(self, match.state)

    def groups(self, default=None):
        rv = Match.groups.__get__(self)
        if default is not None:
            rv = list(rv)
            for idx, item in enumerate(rv):
                if item is None:
                    rv[idx] = default
            rv = tuple(rv)
        return rv

    def groupdict(self, default=None):
        rv = Match.groupdict.__get__(self)
        if default is not None:
            for name, value in rv.iteritems():
                if value is None:
                    rv[name] = default
        return rv

    def group(self, group=0, *groups):
        if not groups:
            return Match.group(self, group)
        return tuple(map(Match.group, (group,) + groups))

    def __repr__(self):
        return '<ponygurma.sre.SRE_Match object at 0x%x>' % \
               (id(self) & 0xffffffff)


_cache = {}
_MAXCACHE = 100

def _compile(pattern, flags):
    if isinstance(pattern, SRE_Pattern):
        return pattern
    key = (type(pattern), pattern, flags)
    if key in _cache:
        return _cache[key]
    if len(_cache) >= _MAXCACHE:
        purge()
    _cache[key] = rv = SRE_Pattern(pattern, flags)
    return rv

def purge():
    _cache.clear()

def compile(pattern, flags=0):
    return SRE_Pattern(pattern, flags)

def search(pattern, string, flags=0):
    return _compile(pattern, flags).search(string)

def match(pattern, string, flags=0):
    return _compile(pattern, flags).search(string)

def split(pattern, string, maxsplit=0):
    return _compile(pattern, 0).split(string, maxsplit)

def findall(pattern, string, flags=0):
    return list(_compile(pattern, flags).findall(flags))

def finditer(pattern, string, flags=0):
    return _compile(pattern, flags).finditer(string)

def sub(pattern, repl, string, count=0):
    return _compile(pattern, 0).sub(repl, string, count)

def subn(pattern, repl, string, count=0):
    return _compile(pattern, 0).subn(repl, string, count)

error = RegexpError

__all__ = ['compile', 'purge', 'search', 'match', 'split', 'findall',
           'finditer', 'sub', 'subn', 'escape', 'I', 'IGNORECASE',
           'M', 'MULTILINE', 'X', 'VERBOSE']
