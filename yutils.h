/*
 * $RCSfile: yutils.h,v $
 * $Revision: 1.1.1.1 $
 *
 * Copyright (C) 1998 by Hiroshi Yuki <hyuki@st.rim.or.jp>.
 * http://www.st.rim.or.jp/~hyuki/
 * All rights reserved.
 */

#ifndef YUTILS_H
#define YUTILS_H

char *append_string(char *allocbuf, int *allocsizep, int *pos, char *str);
char *append_char(char *allocbuf, int *allocsizep, int *pos, char c);
char **split_by_char(char splitchar, char *str, int expected_items);
void tr(char *buf, char from, char to);
void decode_percent_hex(char *buf);
char *encode_percent_hex(char *buf, char *encstr);

#endif
