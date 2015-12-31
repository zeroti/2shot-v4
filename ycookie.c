/*
 * $RCSfile: ycookie.c,v $
 * $Revision: 1.1.1.1 $
 *
 * Copyright (C) 1998 by Hiroshi Yuki <hyuki@st.rim.or.jp>.
 * http://www.st.rim.or.jp/~hyuki/
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "yutils.h"
#include "ycookie.h"

/* 連想配列もどき */
typedef struct _assoc_t {
    char *key;
    char *value;
} assoc_t;

#define MAX_COOKIE 100

static assoc_t cookie[MAX_COOKIE];
static int cookie_index = 0;

/*
 * 圧縮されたクッキー文字列を展開し、cookie[] に得る。
 */
void init_cookie(char *cookiename)
{
    char **sqpairs = 0;
    char *env = getenv("HTTP_COOKIE");
    int i;

    if (!env) {
        return;
    }
    sqpairs = split_by_char(';', env, 0);
    for (i = 0; sqpairs[i]; i++) {
        char **sqpair = split_by_char('=', sqpairs[i], 2);
        char *sqkey = sqpair[0];
        char *sqvalue = sqpair[1];
        while (*sqkey == ' ') sqkey++;
        if (strcmp(sqkey, cookiename) == 0) {
            int j;
            char **pairs = 0;
            tr(sqvalue, ':', ';');
            tr(sqvalue, '_', '=');
            pairs = split_by_char(';', sqvalue, 0);
            for (j = 0; pairs[j]; j++) {
                char **pair = split_by_char('=', pairs[j], 2);
                char *name = pair[0];
                char *value = pair[1];
                tr(value, '+', ' ');
                decode_percent_hex(name);
                decode_percent_hex(value);
                set_cookie(name, value);
                free(pair);
            }
            free(pairs);
            free(sqpair);
            break;
        } else {
            free(sqpair);
        }
    }
    free(sqpairs);
}

/*
 * クッキー cookie[] を圧縮し、cookiename=圧縮されたクッキー文字列を返す
 */
static char *make_cookie(char *cookiename)
{
    char *encode = "%+;,=&_:";
    int allocsize = strlen(cookiename);
    char *sqstr = (char *)malloc(allocsize + 1);
    int di = 0;
    int i;

    sqstr[0] = '\0';
    sqstr = append_string(sqstr, &allocsize, &di, cookiename);
    sqstr = append_string(sqstr, &allocsize, &di, "=");
    for (i = 0; i < cookie_index; i++) {
        char *name = cookie[i].key;
        char *value = cookie[i].value;
        char *newname = encode_percent_hex(name, encode);
        char *newvalue = encode_percent_hex(value, encode);
        tr(newname, ' ', '+');
        tr(newvalue, ' ', '+');
        sqstr = append_string(sqstr, &allocsize, &di, newname);
        sqstr = append_string(sqstr, &allocsize, &di, "_");
        sqstr = append_string(sqstr, &allocsize, &di, newvalue);
        sqstr = append_string(sqstr, &allocsize, &di, ":");
        free(newname);
        free(newvalue);
    }
    sqstr = append_string(sqstr, &allocsize, &di, "; ");
    return sqstr;
}

/*
 * 現在から指定日数以後の日付を得る。
 * Fri, 31-Dec-2010 00:00:00 GMT
 * （クッキーで使用するものなので、形式は変更してはいけない）
 */
 
static char *get_expire_date_string(int days)
{
    static char expiredate[100];
    static char *month[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static char *week[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    time_t now = time(0);
    struct tm *t = 0;

    now += days * 24 * 60 * 60;
    t = gmtime(&now);

    t->tm_year += 1900;
    sprintf(expiredate, "%s, %02d-%s-%04d %02d:%02d:%02d GMT",
        week[t->tm_wday],
        t->tm_mday,
        month[t->tm_mon],
        t->tm_year,
        t->tm_hour,
        t->tm_min,
        t->tm_sec);
    return expiredate;
}

/*
 * クッキー %cookie を出力する
 */
void print_cookie(char *cookiename, int days, char *domain)
{
    char *cookiestr = make_cookie(cookiename);
    char *expdate = get_expire_date_string(days);

    printf("Set-Cookie: %s;", cookiestr);
    printf(" expires=%s;", expdate);
    if (domain) {
        printf(" domain=%s;", domain);
	}
	printf("\n");
}

/* key, value の文字列の組みを cookie にセットする */
void set_cookie(char *key, char *value)
{
    int i;
    
    for (i = 0; i < cookie_index; i++) {
        if (strcmp(cookie[i].key, key) == 0) {
            /* 上書き */
            free(cookie[i].value);
            cookie[i].value = malloc(strlen(value) + 1);
            strcpy(cookie[i].value, value);
            return;
        }
    }
    /* 追加 */
    if (cookie_index < MAX_COOKIE) {
        cookie[cookie_index].key = malloc(strlen(key) + 1);
        strcpy(cookie[cookie_index].key, key);
        cookie[cookie_index].value = malloc(strlen(value) + 1);
        strcpy(cookie[cookie_index].value, value);
        cookie_index++;
    }
}

/* key に対応した文字列を得る。なければ""を返す */
char *get_cookie(char *key)
{
    int i;
    
    for (i = 0; i < cookie_index; i++) {
        if (strcmp(cookie[i].key, key) == 0) {
            return(cookie[i].value);
        }
    }
    return "";
}

/* cookieの解放 */
void exit_cookie(void)
{
    int i;
    
    for (i = 0; i < cookie_index; i++) {
        free(cookie[i].key);
        free(cookie[i].value);
        cookie[i].key = 0;
        cookie[i].value = 0;
    }
    cookie_index = 0;
}

void clear_cookie(void)
{
    exit_cookie();
}
