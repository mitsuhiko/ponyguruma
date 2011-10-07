# -*- coding: utf-8 -*-
"""
    ponyguruma
    ~~~~~~~~~~~

    Ponyguruma is the code name for a Python wrapper of the `Oniguruma`_
    regular expression engine, which has a richer set of operations than
    Python's sre and better Unicode support.

    .. _Oniguruma: http://www.geocities.jp/kosako3/oniguruma

    :copyright: Copyright 2007 by Armin Ronacher, Georg Brandl.
    :license: BSD.
"""
from ponyguruma._lowlevel import VERSION
from ponyguruma._highlevel import *
from ponyguruma.constants import *

__all__ = ALL_OBJECTS + USEFUL_CONSTANTS + ['VERSION']

del _highlevel, _lowlevel, ALL_OBJECTS, USEFUL_CONSTANTS
