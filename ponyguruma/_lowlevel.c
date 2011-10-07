/**
 * ponyguruma._lowlevel
 * ~~~~~~~~~~~~~~~~~~~~
 *
 * low-level binding to the ongiguruma regular expression engine.
 *
 * :copyright: 2007 by Armin Ronacher, Georg Brandl.
 * :license: BSD.
 */

#include "Python.h"
#include "pyconfig.h"
#include "oniguruma.h"
#include <stdio.h>

/* Calculate the proper encoding to use for Python Unicode strings */
#if Py_UNICODE_SIZE == 2
#  ifdef WORDS_BIGENDIAN
#    define UNICODE_ENCODING ONIG_ENCODING_UTF16_BE
#  else
#    define UNICODE_ENCODING ONIG_ENCODING_UTF16_LE
#  endif
#elif Py_UNICODE_SIZE == 4
#  ifdef WORDS_BIGENDIAN
#    define UNICODE_ENCODING ONIG_ENCODING_UTF32_BE
#  else
#    define UNICODE_ENCODING ONIG_ENCODING_UTF32_LE
#  endif
#else
#  error "unsupported Py_UNICODE_SIZE"
#endif

typedef struct {
	PyObject_HEAD
	regex_t *regex;
	PyObject *pattern;
	int unicode;
} BaseRegexp;

typedef struct {
	PyObject_HEAD
	BaseRegexp *regexp;
	PyObject *string;
	OnigRegion *region;
	Py_ssize_t pos;
	Py_ssize_t endpos;
} MatchState;

static PyObject *RegexpError;


/**
 * The oniguruma syntax for python
 */
static OnigSyntaxType OnigSyntaxPython;


/**
 * Get the oniguruma encoding for a given integer. Because we don't forward
 * oniguruma encodings to the python space, we have to use something like that.
 */
static OnigEncodingType *
get_onig_encoding(int encoding)
{
	switch (encoding) {
		case  0: return ONIG_ENCODING_ASCII;
		case  1: return ONIG_ENCODING_ISO_8859_1;
		case  2: return ONIG_ENCODING_ISO_8859_2;
		case  3: return ONIG_ENCODING_ISO_8859_3;
		case  4: return ONIG_ENCODING_ISO_8859_4;
		case  5: return ONIG_ENCODING_ISO_8859_5;
		case  6: return ONIG_ENCODING_ISO_8859_6;
		case  7: return ONIG_ENCODING_ISO_8859_7;
		case  8: return ONIG_ENCODING_ISO_8859_8;
		case  9: return ONIG_ENCODING_ISO_8859_9;
		case 10: return ONIG_ENCODING_ISO_8859_10;
		case 11: return ONIG_ENCODING_ISO_8859_11;
		case 12: return ONIG_ENCODING_ISO_8859_11;
		case 13: return ONIG_ENCODING_ISO_8859_13;
		case 14: return ONIG_ENCODING_ISO_8859_14;
		case 15: return ONIG_ENCODING_ISO_8859_15;
		case 16: return ONIG_ENCODING_ISO_8859_16;
		case 17: return ONIG_ENCODING_UTF8;
		case 18: return ONIG_ENCODING_UTF16_BE;
		case 19: return ONIG_ENCODING_UTF16_LE;
		case 20: return ONIG_ENCODING_UTF32_BE;
		case 21: return ONIG_ENCODING_UTF32_LE;
		case 22: return ONIG_ENCODING_EUC_JP;
		case 23: return ONIG_ENCODING_EUC_TW;
		case 24: return ONIG_ENCODING_EUC_KR;
		case 25: return ONIG_ENCODING_EUC_CN;
		case 26: return ONIG_ENCODING_SJIS;
		/* case 27: return ONIG_ENCODING_KOI8; */
		case 28: return ONIG_ENCODING_KOI8_R;
		case 29: return ONIG_ENCODING_CP1251;
		case 30: return ONIG_ENCODING_BIG5;
		case 31: return ONIG_ENCODING_GB18030;
		default: return ONIG_ENCODING_UNDEF;
	}
}

/**
 * Like get_onig_encoding, but for the syntax.
 */
static OnigSyntaxType *
get_onig_syntax(int syntax)
{
	switch (syntax) {
		case  0: return ONIG_SYNTAX_ASIS;
		case  1: return ONIG_SYNTAX_POSIX_BASIC;
		case  2: return ONIG_SYNTAX_POSIX_EXTENDED;
		case  3: return ONIG_SYNTAX_EMACS;
		case  4: return ONIG_SYNTAX_GREP;
		case  5: return ONIG_SYNTAX_GNU_REGEX;
		case  6: return ONIG_SYNTAX_JAVA;
		case  7: return ONIG_SYNTAX_PERL;
		case  8: return ONIG_SYNTAX_PERL_NG;
		case  9: return ONIG_SYNTAX_RUBY;
		case 10:
		default: return &OnigSyntaxPython;
	}
}


/**
 * initialize the python syntax based on the ruby one
 */
static int
init_python_syntax(void)
{
	onig_copy_syntax(&OnigSyntaxPython, ONIG_SYNTAX_RUBY);
	int behavior = onig_get_syntax_behavior(&OnigSyntaxPython);

	/* use the ruby settings but disable the use of the same
	   name for multiple groups, disable warnings for stupid
	   escapes and capture named and position groups */
	onig_set_syntax_behavior(&OnigSyntaxPython, behavior & ~(
		ONIG_SYN_CAPTURE_ONLY_NAMED_GROUP |
		ONIG_SYN_ALLOW_MULTIPLEX_DEFINITION_NAME |
		ONIG_SYN_WARN_CC_OP_NOT_ESCAPED |
		ONIG_SYN_WARN_REDUNDANT_NESTED_REPEAT
	));
	/* sre like singleline */
	onig_set_syntax_options(&OnigSyntaxPython,
		ONIG_OPTION_NEGATE_SINGLELINE
	);
	return 0;
}


/**
 * Create a new Regexp object.
 */
static PyObject *
BaseRegexp_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	PyObject *pattern;
	int ienc = -1, isyn = 10, rv;
	OnigOptionType options = ONIG_OPTION_NONE;
	OnigSyntaxType *syn;
	OnigEncodingType *enc;
	UChar *pstr, *pend;
	OnigErrorInfo einfo;
	BaseRegexp *self;
	static char *kwlist[] = {"pattern", "flags", "encoding", "syntax", NULL};

	self = (BaseRegexp *)type->tp_alloc(type, 0);
	if (!self)
		return NULL;
	/* Initialize them in case __new__ fails. */
	self->regex = NULL;
	self->pattern = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|iii:BaseRegexp", kwlist,
					 &pattern, &options, &ienc, &isyn)) {
		Py_DECREF(self);
		return NULL;
	}
	if (PyUnicode_Check(pattern)) {
		if (ienc != -1) {
			PyErr_SetString(PyExc_TypeError, "an encoding can "
					"only be given for non-unicode patterns");
			Py_DECREF(self);
			return NULL;
		}
		enc = UNICODE_ENCODING;
		pstr = (UChar *) PyUnicode_AS_UNICODE(pattern);
		pend = pstr + (PyUnicode_GET_SIZE(pattern) * sizeof(PY_UNICODE_TYPE));
		self->unicode = 1;
	}
	else if (PyString_Check(pattern)) {
		if (ienc == -1) ienc = 0;
		enc = get_onig_encoding(ienc);
		pstr = (UChar *) PyString_AS_STRING(pattern);
		pend = pstr + PyString_GET_SIZE(pattern);
		self->unicode = 0;
	}
	else {
		PyErr_SetString(PyExc_TypeError, "pattern must be string or unicode");
		Py_DECREF(self);
		return NULL;
	}
	/* Got to keep a reference to the pattern string */
	Py_INCREF(pattern);
	self->pattern = pattern;

	/* XXX: check for invalid values? */
	syn = get_onig_syntax(isyn);

	rv = onig_new(&(self->regex), pstr, pend, options, enc, syn, &einfo);

	if (rv != ONIG_NORMAL) {
		UChar s[ONIG_MAX_ERROR_MESSAGE_LEN];
		onig_error_code_to_str(s, rv, &einfo);
		PyErr_SetString(RegexpError, (char *)s);
		Py_DECREF(self);
		return NULL;
	}

	return (PyObject *)self;
}

/**
 * oniguruma requires that we free the regex object.
 */
static void
BaseRegexp_dealloc(BaseRegexp *self)
{
	if (self->regex)
		onig_free(self->regex);
	Py_XDECREF(self->pattern);
	self->ob_type->tp_free((PyObject *)self);
}

/**
 * read only property for the unicode flag.
 */
static PyObject *
BaseRegexp_getunicode(BaseRegexp *self, void *closure)
{
	return PyBool_FromLong(self->unicode);
}

static PyObject *
BaseRegexp_getpattern(BaseRegexp *self, void *closure)
{
	Py_INCREF(self->pattern);
	return self->pattern;
}

static PyObject *
BaseRegexp_getflags(BaseRegexp *self, void *closure)
{
	return PyInt_FromLong(onig_get_options(self->regex));
}

static PyGetSetDef BaseRegexp_getsetters[] = {
	{"unicode_mode", (getter)BaseRegexp_getunicode, NULL,
	 "True if the pattern is in unicode mode.", NULL},
	{"pattern", (getter)BaseRegexp_getpattern, NULL,
	 "the pattern string the Regexp was built from.", NULL},
	{"flags", (getter)BaseRegexp_getflags, NULL,
	 "the flags the Regexp was built with.", NULL},
	{NULL}
};


static PyTypeObject BaseRegexpType = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"ponyguruma._lowlevel.BaseRegexp", /* tp_name */
	sizeof(BaseRegexp),		/* tp_basicsize */
	0,				/* tp_itemsize */
	(destructor)BaseRegexp_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	0,				/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
	"",				/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	0,				/* tp_methods */
	0,				/* tp_members */
	BaseRegexp_getsetters,		/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	0,				/* tp_alloc */
	BaseRegexp_new			/* tp_new */

};


/**
 * oniguruma requires that we free the match state objects.
 */
static void
MatchState_dealloc(MatchState *self)
{
	Py_XDECREF(self->regexp);
	Py_XDECREF(self->string);
	if (self->region)
		onig_region_free(self->region, 1);
	self->ob_type->tp_free(self);
}


static PyObject *
MatchState_getregexp(MatchState *self, void *closure)
{
	Py_INCREF(self->regexp);
	return (PyObject *)self->regexp;
}

static PyObject *
MatchState_getstring(MatchState *self, void *closure)
{
	Py_INCREF(self->string);
	return self->string;
}

static PyObject *
MatchState_getpos(MatchState *self, void *closure)
{
	return PyInt_FromSsize_t(self->pos);
}

static PyObject *
MatchState_getendpos(MatchState *self, void *closure)
{
	return PyInt_FromSsize_t(self->endpos);
}

static PyGetSetDef MatchState_getsetters[] = {
	{"regexp", (getter)MatchState_getregexp, NULL, "", NULL},
	{"string", (getter)MatchState_getstring, NULL, "", NULL},
	{"pos", (getter)MatchState_getpos, NULL, "", NULL},
	{"endpos", (getter)MatchState_getendpos, NULL, "", NULL},
	{NULL}
};


static PyTypeObject MatchStateType = {
	PyObject_HEAD_INIT(NULL)
	0,				/* ob_size */
	"ponyguruma._lowlevel.MatchState", /* tp_name */
	sizeof(MatchState),		/* tp_basicsize */
	0,				/* tp_itemsize */
	(destructor)MatchState_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	0,				/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	0,				/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,		/* tp_flags */
	"internal match state object",	/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	0,				/* tp_methods */
	0,				/* tp_members */
	MatchState_getsetters,		/* tp_getset */
};


/**
 * regexp match/search function
 */
static PyObject *
regexp_match(PyObject *self, PyObject *args)
{
	PyObject *string, *from_start;
	BaseRegexp *regexp;
	Py_ssize_t pos, endpos;
	int ifrom_start;
	MatchState *state = NULL;
	UChar *str, *str_start, *str_end;
	
	if (!PyArg_ParseTuple(args, "OOiiO:match", &regexp, &string,
			      &pos, &endpos, &from_start))
		return NULL;
	if (!PyObject_IsInstance((PyObject *)regexp,
				      (PyObject *)&BaseRegexpType)) {
		PyErr_SetString(PyExc_TypeError, "regular expression "
				"object required");
		return NULL;
	}
	if (pos < 0) {
		PyErr_SetString(PyExc_ValueError, "pos must be >= 0");
		return NULL;
	}
	ifrom_start = PyObject_IsTrue(from_start);
	if (ifrom_start < 0)
		return NULL;
	if (PyString_Check(string)) {
		if (regexp->unicode) {
			/* Encode using default encoding. */
			string = PyUnicode_FromEncodedObject(string, NULL, NULL);
			if (!string)
				return NULL;
		} else {
			Py_INCREF(string);
		}
	}
	else if (PyUnicode_Check(string)) {
		if (regexp->unicode) {
			Py_INCREF(string);
		} else {
			/* Decode using default encoding. */
			string = PyUnicode_AsEncodedString(string, NULL, NULL);
			if (!string)
				return NULL;
		}
	}
	else {
		PyErr_SetString(PyExc_TypeError, "string to match must be "
				"string or unicode");
		return NULL;
	}
	if (endpos == -1) {
		endpos = (regexp->unicode ? PyUnicode_GET_SIZE(string) :
			  PyString_GET_SIZE(string));
	}
	if (endpos < 0) {
		PyErr_SetString(PyExc_ValueError, "endpos must be >= -1, where "
				"-1 means the length of the string to match");
		Py_DECREF(string);
		return NULL;
	}
	
	if (regexp->unicode) {
		str = (UChar *) PyUnicode_AS_UNICODE(string);
		str_start = str + (sizeof(Py_UNICODE) * pos);
		str_end = str + (sizeof(Py_UNICODE) * endpos);
	} else {
		str = (UChar *) PyString_AS_STRING(string);
		str_start = str + pos;
		str_end = str + endpos;
	}

	if (str_start > str_end)
		goto nomatch;

	state = PyObject_New(MatchState, &MatchStateType);
	Py_INCREF(regexp);
	state->regexp = regexp;
	state->region = onig_region_new();
	state->string = string;
	state->pos = pos;
	state->endpos = endpos;

	if (((ifrom_start)
	      ? onig_match(regexp->regex, str, str_end, str_start, state->region,
			   ONIG_OPTION_NONE)
	      : onig_search(regexp->regex, str, str_end, str_start, str_end,
			    state->region, ONIG_OPTION_NONE)
		    ) >= 0)
		return (PyObject *) state;

nomatch:
	Py_XDECREF(state);
	Py_INCREF(Py_None);
	return Py_None;
}


/**
 * get a tuple of groups
 */
static PyObject *
match_get_groups(PyObject *self, PyObject *state)
{
	OnigRegion *region;
	int count, i;
	PyObject *rv;

	if (!PyObject_IsInstance(state, (PyObject *)&MatchStateType)) {
		PyErr_SetString(PyExc_TypeError, "match state required");
		return NULL;
	}

	region = ((MatchState *)state)->region;
	count = region->num_regs;

	rv = PyTuple_New(count);
	if (!rv)
		return NULL;

	for (i = 0; i < count; i++) {
		int beg = region->beg[i];
		int end = region->end[i];
		PyObject *pair;
		if (((MatchState *)state)->regexp->unicode) {
			beg /= sizeof(Py_UNICODE);
			end /= sizeof(Py_UNICODE);
		}
		pair = Py_BuildValue("(ii)", beg, end);
		if (!pair) {
			Py_DECREF(rv);
			return NULL;
		}
		PyTuple_SET_ITEM(rv, i, pair);
	}

	return rv;
}


/**
 * get a dict for idx -> name
 */
static int
iterate_group_names(const UChar *name, const UChar *name_end,
		    int ngroup_num, int *group_nums, regex_t *reg,
		    void *arg)
{
	int i;
	for (i = 0; i < ngroup_num; i++) {
		if (PyDict_SetItemString((PyObject *)arg, (char *)name,
					 PyInt_FromLong(group_nums[i])) < 0)
			return -1;
	}
	return 0;
}


static PyObject *
match_get_group_names(PyObject *self, PyObject *state)
{
	PyObject *rv;
	regex_t *regex;

	if (!PyObject_IsInstance(state, (PyObject *)&MatchStateType)) {
		PyErr_SetString(PyExc_TypeError, "match state required");
		return NULL;
	}

	rv = PyDict_New();
	if (!rv)
		return NULL;

	regex = ((MatchState *)state)->regexp->regex;
	if (onig_number_of_names(regex)) {
		if (onig_foreach_name(regex, iterate_group_names, (void *)rv) < 0) {
			Py_DECREF(rv);
			return NULL;
		}
	}
	return rv;
}


/**
 * extract the substring of a group.
 */
static PyObject *
match_extract_group(PyObject *self, PyObject *args)
{
	MatchState *state;
	long group;
	Py_ssize_t len, start;

	if (!PyArg_ParseTuple(args, "Oi:match_extract_group", &state, &group))
		return NULL;

	if (!PyObject_IsInstance((PyObject *)state, (PyObject *)&MatchStateType)) {
		PyErr_SetString(PyExc_TypeError, "match state required");
		return NULL;
	}

	if (state->region->num_regs <= group) {
		PyErr_SetString(PyExc_IndexError, "no such group");
		return NULL;
	}

	start = state->region->beg[group];
	if (start < 0 && state->region->end[group] < 0) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	len = state->region->end[group] - start;

	if (state->regexp->unicode)
		return PyUnicode_FromUnicode(
			PyUnicode_AS_UNICODE(state->string) + start / sizeof(Py_UNICODE),
			len / sizeof(Py_UNICODE));
	else
		return PyString_FromStringAndSize(
			PyString_AS_STRING(state->string) + start, len);
}


/**
 * Forward a warning call to the _highlevel module
 */
static void
on_regexp_warning(const char *message)
{
	PyObject *module = NULL, *warn_func = NULL;
	PyObject *args = Py_BuildValue("(s)", message);
	if (!args)
		goto ret;
	module = PyImport_ImportModule("ponyguruma");
	if (!module)
		goto ret;
	warn_func = PyObject_GetAttrString(module, "warn_func");
	if (!warn_func)
		goto ret;
	PyObject_CallObject(warn_func, args);
  ret:
	/* there's no way for the error to be detected */
	PyErr_Clear();
	Py_XDECREF(args);
	Py_XDECREF(module);
	Py_XDECREF(warn_func);
}


static PyMethodDef module_methods[] = {
	{"regexp_match", (PyCFunction)regexp_match, METH_VARARGS,
	 "internal matching helper function"},
	{"match_get_groups", (PyCFunction)match_get_groups, METH_O,
	 "internal matching helper function"},
	{"match_get_group_names", (PyCFunction)match_get_group_names, METH_O,
	 "internal matching helper function"},
	{"match_extract_group", (PyCFunction)match_extract_group, METH_VARARGS,
	 "internal matching helper function"},
	{NULL, NULL, 0, NULL}
};


#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
init_lowlevel(void)
{
	PyObject *module;

	if (init_python_syntax() < 0)
		return;

	if (PyType_Ready(&BaseRegexpType) < 0 ||
	    PyType_Ready(&MatchStateType) < 0 )
		return;

	module = Py_InitModule3("ponyguruma._lowlevel", module_methods, "");
	if (!module)
		return;

	RegexpError = PyErr_NewException("ponyguruma.RegexpError", NULL, NULL);
	Py_INCREF(RegexpError);
	PyModule_AddObject(module, "RegexpError", RegexpError);

	Py_INCREF(&BaseRegexpType);
	PyModule_AddObject(module, "BaseRegexp", (PyObject *)&BaseRegexpType);

	Py_INCREF(&MatchStateType);
	PyModule_AddObject(module, "MatchState", (PyObject *)&MatchStateType);

	PyObject *version = Py_BuildValue("(iii)", ONIGURUMA_VERSION_MAJOR,
					  ONIGURUMA_VERSION_MINOR,
					  ONIGURUMA_VERSION_TEENY);
	PyModule_AddObject(module, "VERSION", (PyObject*)version);

	onig_set_warn_func(on_regexp_warning);
	onig_set_verb_warn_func(on_regexp_warning);
}
