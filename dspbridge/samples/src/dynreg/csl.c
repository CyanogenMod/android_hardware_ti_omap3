/*
 *  Copyright 2001-2008 Texas Instruments - http://www.ti.com/
 * 
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


/*
 *  ======== cslce.c ========
 *  Public Functions:
 *      CSL_AnsiToWchar
 *      CSL_Atoi
 *      CSL_ByteSwap
 *      CSL_Exit
 *      CSL_Init
 *      CSL_NumToAscii
 *      CSL_Strcmp
 *      CSL_Strcpyn
 *      CSL_Strlen
 *      CSL_Strncat
 *      CSL_Strncmp
 *      CSL_Strtok
 *      CSL_Strtokr
 *      CSL_WcharToAnsi
 *      CSL_Wstrlen
 *
 */

/* ----------------------------------- Host OS */
#include <host_os.h>

/*  ----------------------------------- DSP/BIOS Bridge */
#include <std.h>
#include <dbdefs.h>
#include <string.h>

/*  ----------------------------------- Trace & Debug */
#include <dbc.h>
#include <dbg_zones.h>
#include <gt.h>

/*  ----------------------------------- This */
#include <csl.h>

/* Is character c in the string pstrDelim? */
#define IsDelimiter(c, pstrDelim) ((c != '\0') && \
        (strchr(pstrDelim, c) != NULL))

    /*  ----------------------------------- Globals */
#if GT_TRACE
static struct GT_Mask CSL_DebugMask = { 0, 0 };	/* GT trace var. */
#endif

#ifdef UNICODE
/*
 *  ======== CSL_AnsiToWchar ========
 *  Purpose:
 *  Convert an ansi string to a wide CHAR string.
 */
ULONG CSL_AnsiToWchar(OUT WCHAR *pwszDest, IN PSTR pstrSource, IN ULONG uSize)
{
	DWORD dwLength;
	DWORD i;

	if (!pstrSource) {
		return (0);
	}
	dwLength = CSL_Strlen(pstrSource);
	for (i = 0;(i < dwLength) && (i < (uSize - 1));i++) {
		pwszDest[i] = pstrSource[i];
	}
	pwszDest[i] = '\0';
	return (i);
}
#endif

/*
 *  ======= CSL_Atoi =======
 *  Purpose:
 *      Convert a string to an integer
 */
#if 0
INT CSL_Atoi(IN CONST CHAR * ptstrSrc)
{
	char *end_position;

	DBC_Require(ptstrSrc);

	return simple_strtol(ptstrSrc, &end_position, 10);
}

#endif				/* 0 */

/*
 *  ======== CSL_ByteSwap ========
 *  Purpose:
 *      Swap bytes in buffer.
 */
VOID CSL_ByteSwap(IN PSTR pstrSrc, OUT PSTR pstrDest, IN ULONG ulBytes)
{
	int i, tmp;

	/*DBC_Require((ulBytes % 2) == 0);
	   DBC_Require(pstrSrc && pstrDest); */

	/* swap bytes. */
	for (i = 0;i < ulBytes / 2;i++) {
		tmp = 2 * i + 1;
		pstrDest[2 * i] = pstrSrc[tmp];
		pstrDest[tmp] = pstrSrc[2 * i];
	}
}

/*
 *  ======== CSL_Exit ========
 *  Purpose:
 *      Discontinue usage of the CSL module.
 */
void CSL_Exit()
{
	/* GT_0trace(CSL_DebugMask, GT_5CLASS, "CSL_Exit\n"); */
}

/*
 *  ======== CSL_Init ========
 *  Purpose:
 *      Initialize the CSL module's private state.
 */
bool CSL_Init()
{
	/* GT_create(&CSL_DebugMask, "CS"); */

	/* GT_0trace(CSL_DebugMask, GT_5CLASS, "CSL_Init\n"); */

	return (true);
}

/*
 *  ======== CSL_NumToAscii ========
 *  Purpose:
 *      Convert a 1 or 2 digit number to a 2 digit string.
 */
VOID CSL_NumToAscii(OUT PSTR pstrNumber, DWORD dwNum)
{
	CHAR tens;

	/* DBC_Require(dwNum < 100); */

	if (dwNum < 100) {
		tens = (CHAR) dwNum / 10;
		dwNum = dwNum % 10;

		if (tens) {
			pstrNumber[0] = tens + '0';
			pstrNumber[1] = (CHAR) dwNum + '0';
			pstrNumber[2] = '\0';
		} else {
			pstrNumber[0] = (CHAR) dwNum + '0';
			pstrNumber[1] = '\0';
		}
	} else {
		pstrNumber[0] = '\0';
	}
}

/*
 *  ======== CSL_Strcmp ========
 *  Purpose:
 *      Compare 2 ASCII strings.  Works the same was as stdio's strcmp.
 */
LONG CSL_Strcmp(IN CONST PSTR pstrStr1, IN CONST PSTR pstrStr2)
{
	return strcmp(pstrStr1, pstrStr2);
}

/*
 *  ======== CSL_Strcpyn ========
 *  Purpose:
 *      Safe strcpy function.
 */
PSTR CSL_Strcpyn(OUT PSTR pstrDest, IN CONST PSTR pstrSrc, DWORD cMax)
{
	return strncpy(pstrDest, pstrSrc, cMax);
}

/*
 *  ======== CSL_Strlen ========
 *  Purpose:
 *      Determine the length of a null terminated ASCII string.
 */
DWORD CSL_Strlen(IN CONST PSTR pstrSrc)
{
	CONST CHAR *pStr = pstrSrc;

	/* DBC_Require(pstrSrc); */

	while (*pStr++) ;

	return ((DWORD)(pStr - pstrSrc - 1));
}

/*
 *  ======== CSL_Strncat ========
 *  Purpose:
 *      Concatenate two strings together
 */
PSTR CSL_Strncat(IN PSTR pszDest, IN PSTR pszSrc, IN DWORD dwSize)
{

	/* DBC_Require(pszDest && pszSrc); */

	return (strncat(pszDest, pszSrc, dwSize));
}

/*
 *  ======== CSL_Strncmp ========
 *  Purpose:
 *      Compare at most n characters of two ASCII strings.  Works the same
 *      way as stdio's strncmp.
 */
LONG CSL_Strncmp(IN CONST PSTR pstrStr1, IN CONST PSTR pstrStr2, DWORD n)
{
	return strncmp(pstrStr1, pstrStr2, n);
}

/*
 *  ======= CSL_Strtokr =======
 *  Purpose:
 *      Re-entrant version of strtok.
 */
CHAR *CSL_Strtokr(IN CHAR *pstrSrc, IN CONST CHAR *szSeparators,
														OUT CHAR **ppstrLast)
{
	CHAR *pstrTemp;
	CHAR *pstrToken;

	/*DBC_Require(szSeparators != NULL);
	   DBC_Require(ppstrLast != NULL);
	   DBC_Require(pstrSrc != NULL || *ppstrLast != NULL);
	 */
	/*
	 *  Set string location to beginning (pstrSrc != NULL) or to the
	 *  beginning of the next token.
	 */
	pstrTemp = (pstrSrc != NULL) ? pstrSrc : *ppstrLast;
	if (*pstrTemp == '\0') {
		pstrToken = NULL;
	} else {
		pstrToken = pstrTemp;
		while (*pstrTemp != '\0'
		       && !IsDelimiter(*pstrTemp, szSeparators)) {
			pstrTemp++;
		}
		if (*pstrTemp != '\0') {
			while (IsDelimiter(*pstrTemp, szSeparators)) {
				*pstrTemp = '\0';	
				/* TODO: Shouldn't we do this for only 1 char?? */
				pstrTemp++;
			}
		}
		/* Location in string for next call */
		*ppstrLast = pstrTemp;
	}
	return (pstrToken);
}

#ifdef UNICODE
/*
 *  ======== CSL_WcharToAnsi ========
 *  Purpose:
 *      UniCode to Ansi conversion.
 *  Note:
 *      uSize is # of chars in destination buffer.
 */
ULONG CSL_WcharToAnsi(OUT PSTR pstrDest, IN WCHAR *pwszSource, ULONG uSize)
{
	PSTR pstrTemp = pstrDest;
	ULONG uNumOfChars = 0;

	if (!pwszSource) {
		return (0);
	}
	while ((*pwszSource != TEXT('\0')) && (uSize-- > 0)) {
		*pstrTemp++ = (CHAR) * pwszSource++;
		uNumOfChars++;
	}
	*pstrTemp = '\0';

	return (uNumOfChars);
}

/*
 *  ======== CSL_Wstrlen ========
 *  Purpose:
 *      Determine the length of a null terminated UNICODE string.
 */
DWORD CSL_Wstrlen(IN CONST TCHAR *ptstrSrc)
{
	CONST TCHAR *ptstr = ptstrSrc;

	/*DBC_Require(ptstrSrc); */

	while (*ptstr++) ;

	return ((DWORD) (ptstr - ptstrSrc - 1));
}
#endif

