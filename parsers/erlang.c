/*
*   Copyright (c) 2003, Brent Fulgham <bfulgham@debian.org>
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Erlang language
*   files.  Some of the parsing constructs are based on the Emacs 'etags'
*   program by Francesco Potori <pot@gnu.org>
*/
/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>

#include "entry.h"
#include "options.h"
#include "read.h"
#include "routines.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_MACRO, K_FUNCTION, K_MODULE, K_RECORD, K_TYPE
} erlangKind;

static kindOption ErlangKinds[] = {
	{TRUE, 'd', "macro",    "macro definitions"},
	{TRUE, 'f', "function", "functions"},
	{TRUE, 'm', "module",   "modules"},
	{TRUE, 'r', "record",   "record definitions"},
	{TRUE, 't', "type",     "type definitions"},
};

/*
*   FUNCTION DEFINITIONS
*/
/* tagEntryInfo and vString should be preinitialized/preallocated but not
 * necessary. If successful you will find class name in vString
 */

static boolean isIdentifierFirstCharacter (int c)
{
	return (boolean) (isalpha (c));
}

static boolean isIdentifierCharacter (int c)
{
	return (boolean) (isalnum (c) || c == '_' || c == ':');
}

static const unsigned char *skipSpace (const unsigned char *cp)
{
	while (isspace ((int) *cp))
		++cp;
	return cp;
}

static const unsigned char *parseIdentifier (
		const unsigned char *cp, vString *const identifier)
{
	vStringClear (identifier);
	while (isIdentifierCharacter ((int) *cp))
	{
		vStringPut (identifier, (int) *cp);
		++cp;
	}
	vStringTerminate (identifier);
	return cp;
}

static void makeMemberTag (
		vString *const identifier, erlangKind kind, vString *const module)
{
	if (ErlangKinds [kind].enabled  &&  vStringLength (identifier) > 0)
	{
		tagEntryInfo tag;
		initTagEntry (&tag, vStringValue (identifier), & (ErlangKinds[kind]));

		if (module != NULL  &&  vStringLength (module) > 0)
		{
			tag.extensionFields.scopeKind = &(ErlangKinds [K_MODULE]);
			tag.extensionFields.scopeName = vStringValue (module);
		}
		makeTagEntry (&tag);
	}
}

static void parseModuleTag (const unsigned char *cp, vString *const module)
{
	vString *const identifier = vStringNew ();
	parseIdentifier (cp, identifier);
	makeSimpleTag (identifier, ErlangKinds, K_MODULE);

	/* All further entries go in the new module */
	vStringCopy (module, identifier);
	vStringDelete (identifier);
}

static void parseSimpleTag (const unsigned char *cp, erlangKind kind)
{
	vString *const identifier = vStringNew ();
	parseIdentifier (cp, identifier);
	makeSimpleTag (identifier, ErlangKinds, kind);
	vStringDelete (identifier);
}

static void parseFunctionTag (const unsigned char *cp, vString *const module)
{
	vString *const identifier = vStringNew ();
	parseIdentifier (cp, identifier);
	makeMemberTag (identifier, K_FUNCTION, module);
	vStringDelete (identifier);
}

/*
 * Directives are of the form:
 * -module(foo)
 * -define(foo, bar)
 * -record(graph, {vtab = notable, cyclic = true}).
 * -type some_type() :: any().
 * -opaque some_opaque_type() :: any().
 */
static void parseDirective (const unsigned char *cp, vString *const module)
{
	/*
	 * A directive will be either a record definition or a directive.
	 * Record definitions are handled separately
	 */
	vString *const directive = vStringNew ();
	const char *const drtv = vStringValue (directive);
	cp = parseIdentifier (cp, directive);
	cp = skipSpace (cp);
	if (*cp == '(')
		++cp;

	if (strcmp (drtv, "record") == 0)
		parseSimpleTag (cp, K_RECORD);
	else if (strcmp (drtv, "define") == 0)
		parseSimpleTag (cp, K_MACRO);
	else if (strcmp (drtv, "type") == 0)
		parseSimpleTag (cp, K_TYPE);
	else if (strcmp (drtv, "opaque") == 0)
		parseSimpleTag (cp, K_TYPE);
	else if (strcmp (drtv, "module") == 0)
		parseModuleTag (cp, module);
	/* Otherwise, it was an import, export, etc. */
	
	vStringDelete (directive);
}

static void findErlangTags (void)
{
	vString *const module = vStringNew ();
	const unsigned char *line;

	while ((line = fileReadLine ()) != NULL)
	{
		const unsigned char *cp = line;

		if (*cp == '%')  /* skip initial comment */
			continue;
		if (*cp == '"')  /* strings sometimes start in column one */
			continue;

		if ( *cp == '-')
		{
			++cp;  /* Move off of the '-' */
			parseDirective(cp, module);
		}
		else if (isIdentifierFirstCharacter ((int) *cp))
			parseFunctionTag (cp, module);
	}
	vStringDelete (module);
}

extern parserDefinition *ErlangParser (void)
{
	static const char *const extensions[] = { "erl", "ERL", "hrl", "HRL", NULL };
	parserDefinition *def = parserNew ("Erlang");
	def->kinds = ErlangKinds;
	def->kindCount = KIND_COUNT (ErlangKinds);
	def->extensions = extensions;
	def->parser = findErlangTags;
	return def;
}

/* vi:set tabstop=4 shiftwidth=4: */
