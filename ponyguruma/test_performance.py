"""
    ponyguruma.test_performance
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Test the performance of oniguruma against sre.

    :copyright: Copyright 2007 by Armin Ronacher.
    :license: BSD.
"""

import time
import ponyguruma
import re

COMPLEX = r"(^[-!#$%&'*+/=?^_`{}|~0-9A-Z]+(\.[-!#$%&'*+/=?^_`{}|~0-9A-Z]+)*" \
          r'|^"([\001-\010\013\014\016-\037!#-\[\]-\177]|\\[\001-011\013\014\016-\177])*"' \
          r')@(?:[A-Z0-9-]+\.)+[A-Z]{2,6}$'

def r(t, rep=100000):
    s = time.time()
    for x in xrange(rep):
        t()
    return time.time() - s

def t_compile_sre_empty():
    re.compile('')

def t_compile_sre_rep():
    re.compile('aaaaaaaaaaaaaaa')

def t_compile_sre_complex():
    return re.compile(COMPLEX, re.IGNORECASE)

def t_match_sre_complex():
    r = t_compile_sre_complex()
    r.match("foo@bar.com")

def t_compile_onig_empty():
    ponyguruma.Regexp('')

def t_compile_onig_rep():
    ponyguruma.Regexp('aaaaaaaaaaaaaaa')

def t_compile_onig_complex():
    return ponyguruma.Regexp(COMPLEX, ponyguruma.OPTION_IGNORECASE)

def t_match_onig_complex():
    r = t_compile_onig_complex()
    r.match("foo@bar.com")


if __name__ == '__main__':
    for key in sorted(locals().keys()):
        if key.startswith('t_'):
            print key[2:],
            print r(locals()[key])
