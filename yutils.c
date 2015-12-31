/*
 * $RCSfile: yutils.c,v $
 * $Revision: 1.1.1.1 $
 *
 * Copyright (C) 1998 by Hiroshi Yuki <hyuki@st.rim.or.jp>.
 * http://www.st.rim.or.jp/~hyuki/
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "yutils.h"

static char xdigit(char *buf);

/*
 * Append a string STR to ALLOCBUF.
 * Appending position is *POS.
 * *ALLOCSIZEP is the allocated size of STR.
 * The memory is reallocated automatically.
 */
char *append_string(char *allocbuf, int *allocsizep, int *pos, char *str)
{
    int addlen = strlen(str);
    if (*allocsizep <= *pos + addlen) {
        *allocsizep = *allocsizep * 2 + addlen;
        allocbuf = realloc(allocbuf, *allocsizep + 1);
    }
    strcpy(allocbuf + *pos, str);
    *pos += addlen;
    return allocbuf;
}

/*
 * Append a character C to ALLOCBUF.
 */
char *append_char(char *allocbuf, int *allocsizep, int *pos, char c)
{
    char add[2];
    add[0] = c;
    add[1] = '\0';
    return append_string(allocbuf, allocsizep, pos, add);
}

/*
 * Translate a char FROM to TO in BUF.
 */
void tr(char *buf, char from, char to)
{
    while (*buf) {
        if (*buf == from) {
            *buf = to;
        }
        buf++;
    }
}

/* 
 * Decode %[0-9A-Fa-f][0-9A-Fa-f] in BUF.
 */
void decode_percent_hex(char *buf)
{
    int i = 0, j = 0;
    int len = strlen(buf);

    while (i < len) {
        if (buf[i] == '%' && i < len - 2 && isxdigit(buf[i + 1]) && isxdigit(buf[i + 2])) {
            char minibuf[3];
            minibuf[0] = buf[i + 1];
            minibuf[1] = buf[i + 2];
            minibuf[2] = '\0';
            buf[j] = xdigit(minibuf);
            i += 3;
            j++;
        } else {
            buf[j] = buf[i];
            i++;
            j++;
        }
    }
    if (buf[j]) {
        buf[j] = '\0';
    }
}

/* Convert [0-9A-Fa-f][0-9A-Fa-f] to 1 byte */
static char xdigit(char *buf)
{
    int x;

    sscanf(buf, "%02X", &x);
    return (char)x;
}

/* 
 * Encode characters in BUF as %[0-9A-Fa-f][0-9A-Fa-f].
 * List of characters to be encoded are given as ENCSTR.
 * Return newly allocated memory.
 */
char *encode_percent_hex(char *buf, char *encstr)
{
    int allocsize = strlen(buf) + 1;
    char *newbuf = (char *)malloc(allocsize);
    int si = 0, di = 0;
    char minibuf[10];

    newbuf[0] = '\0';
    for (si = 0; buf[si] != 0; si++) {
        if (strchr(encstr, buf[si])) {
            sprintf(minibuf, "%c%02X", '%', buf[si]);
            newbuf = append_string(newbuf, &allocsize, &di, minibuf);
        } else {
            newbuf = append_char(newbuf, &allocsize, &di, buf[si]);
        }
    }
    return newbuf;
}
