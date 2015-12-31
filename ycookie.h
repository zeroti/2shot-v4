/*
 * $RCSfile: ycookie.h,v $
 * $Revision: 1.1.1.1 $
 *
 * Copyright (C) 1998 by Hiroshi Yuki <hyuki@st.rim.or.jp>.
 * http://www.st.rim.or.jp/~hyuki/
 * All rights reserved.
 */

#ifndef YCOOKIE_H
#define YCOOKIE_H

void init_cookie(char *cookiename);
void exit_cookie(void);
void clear_cookie(void);
void print_cookie(char *cookiename, int days, char *domain);
char *get_cookie(char *key);
void set_cookie(char *key, char *value);

#endif
