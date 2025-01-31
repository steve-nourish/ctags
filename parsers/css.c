/***************************************************************************
 * css.c
 * Token-based parser for CSS definitions
 * Author - Colomban Wendling <colomban@geany.org>
 **************************************************************************/
#include "general.h"

#include <string.h> 
#include <ctype.h> 

#include "entry.h"
#include "parse.h" 
#include "read.h" 


typedef enum eCssKinds {
	K_CLASS, K_SELECTOR, K_ID
} cssKind;

static kindOption CssKinds [] = {
	{ TRUE, 'c', "class",		"classes" },
	{ TRUE, 's', "selector",	"selectors" },
	{ TRUE, 'i', "id",			"identities" }
};

typedef enum {
	/* any ASCII */
	TOKEN_EOF = 257,
	TOKEN_SELECTOR,
	TOKEN_STRING
} tokenType;

typedef struct {
	tokenType type;
	vString *string;
} tokenInfo;


static boolean isSelectorChar (const int c)
{
	/* attribute selectors are handled separately */
	return (isalnum (c) ||
			c == '_' || // allowed char
			c == '-' || // allowed char
			c == '+' || // allow all sibling in a single tag
			c == '>' || // allow all child in a single tag
			c == '|' || // allow namespace separator
			c == '(' || // allow pseudo-class arguments
			c == ')' ||
			c == '.' || // allow classes and selectors
			c == ':' || // allow pseudo classes
			c == '*' || // allow globs as P + *
			c == '#');  // allow ids
}

static void parseSelector (vString *const string, const int firstChar)
{
	int c = firstChar;
	do
	{
		vStringPut (string, (char) c);
		c = fileGetc ();
	} while (isSelectorChar (c));
	fileUngetc (c);
	vStringTerminate (string);
}

static void readToken (tokenInfo *const token)
{
	int c;

	vStringClear (token->string);

getNextChar:

	c = fileGetc ();
	while (isspace (c))
		c = fileGetc ();

	token->type = c;
	switch (c)
	{
		case EOF: token->type = TOKEN_EOF; break;

		case '\'':
		case '"':
		{
			const int delimiter = c;
			do
			{
				vStringPut (token->string, c);
				c = fileGetc ();
				if (c == '\\')
					c = fileGetc ();
			}
			while (c != EOF && c != delimiter);
			if (c != EOF)
				vStringPut (token->string, c);
			token->type = TOKEN_STRING;
			break;
		}

		case '/': /* maybe comment start */
		{
			int d = fileGetc ();
			if (d != '*')
			{
				fileUngetc (d);
				vStringPut (token->string, c);
				token->type = c;
			}
			else
			{
				d = fileGetc ();
				do
				{
					c = d;
					d = fileGetc ();
				}
				while (d != EOF && ! (c == '*' && d == '/'));
				goto getNextChar;
			}
			break;
		}

		default:
			if (! isSelectorChar (c))
			{
				vStringPut (token->string, c);
				token->type = c;
			}
			else
			{
				parseSelector (token->string, c);
				token->type = TOKEN_SELECTOR;
			}
			break;
	}
}

/* sets selector kind in @p kind if found, otherwise don't touches @p kind */
static cssKind classifySelector (const vString *const selector)
{
	size_t i;

	for (i = vStringLength (selector); i > 0; --i)
	{
		char c = vStringItem (selector, i - 1);
		if (c == '.')
			return K_CLASS;
		else if (c == '#')
			return K_ID;
	}
	return K_SELECTOR;
}

static void findCssTags (void)
{
	boolean readNextToken = TRUE;
	tokenInfo token;

	token.string = vStringNew ();

	do
	{
		if (readNextToken)
			readToken (&token);

		readNextToken = TRUE;

		if (token.type == '@')
		{ /* At-rules, from the "@" to the next block or semicolon */
			boolean useContents;
			readToken (&token);
			useContents = (strcmp (vStringValue (token.string), "media") == 0 ||
						   strcmp (vStringValue (token.string), "supports") == 0);
			while (token.type != TOKEN_EOF &&
				   token.type != ';' && token.type != '{')
			{
				readToken (&token);
			}
			/* HACK: we *eat* the opening '{' for medias and the like so that
			 *       the content is parsed as if it was at the root */
			readNextToken = useContents && token.type == '{';
		}
		else if (token.type == TOKEN_SELECTOR)
		{ /* collect selectors and make a tag */
			cssKind kind = K_SELECTOR;
			fpos_t filePosition;
			unsigned long lineNumber;
			vString *selector = vStringNew ();
			do
			{
				if (vStringLength (selector) > 0)
					vStringPut (selector, ' ');
				vStringCat (selector, token.string);

				kind = classifySelector (token.string);
				lineNumber = getSourceLineNumber ();
				filePosition = getInputFilePosition ();

				readToken (&token);

				/* handle attribute selectors */
				if (token.type == '[')
				{
					int depth = 1;
					while (depth > 0 && token.type != TOKEN_EOF)
					{
						vStringCat (selector, token.string);
						readToken (&token);
						if (token.type == '[')
							depth++;
						else if (token.type == ']')
							depth--;
					}
					if (token.type != TOKEN_EOF)
						vStringCat (selector, token.string);
					readToken (&token);
				}
			}
			while (token.type == TOKEN_SELECTOR);
			/* we already consumed the next token, don't read it twice */
			readNextToken = FALSE;

			vStringTerminate (selector);
			if (CssKinds[kind].enabled)
			{
				tagEntryInfo e;
				initTagEntry (&e, vStringValue (selector), &(CssKinds[kind]));

				e.lineNumber	= lineNumber;
				e.filePosition	= filePosition;

				makeTagEntry (&e);
			}
			vStringDelete (selector);
		}
		else if (token.type == '{')
		{ /* skip over { ... } */
			int depth = 1;
			while (depth > 0 && token.type != TOKEN_EOF)
			{
				readToken (&token);
				if (token.type == '{')
					depth++;
				else if (token.type == '}')
					depth--;
			}
		}
	}
	while (token.type != TOKEN_EOF);

	vStringDelete (token.string);
}

/* parser definition */
extern parserDefinition* CssParser (void)
{
    static const char *const extensions [] = { "css", NULL };
    parserDefinition* def = parserNew ("CSS");
    def->kinds      = CssKinds;
    def->kindCount  = KIND_COUNT (CssKinds);
    def->extensions = extensions;
    def->parser     = findCssTags;
    return def;
}

