/*
# 2SHOT-CHAT version 4.0 (フレーム専用) <FreeSoft>
# (c)www.rescue.ne.jp
#
# C言語版　（C)PETIT-POWER/ZeRo http://www.petit-power.com/
#
#
# [改定履歴］---------------------------------------------------------
#　2000/09/30   公開版作成
#　2000/10/15   リリース２　	制限機能と強制閉鎖機能追加
#　2000/10/21   リリース３　	環境設定の構造変更とバグ修正
#　2000/11/04   リリース４　	満室になった際のログクリアの指定を追加
#                        : 途中省略
#  2003/06/02	リリース15　　後から入ってきた人が故意に操作して閉鎖できてしまう問題の修正
#				同時入室時の排他制御の追加
#
# [使用ライブラリ］---------------------------------------------------------
#     結城浩氏作成 kfmem.c ycookie.c yutils.c
#     http://www.st.rim.or.jp/~hyuki/
#
#     MD5部(APACHEソースより)
#      Copyright (c) 1996-1999 The Apache Group.  All rights reserved.
#      Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All rights reserved.
#
# [設置構成] ---------------------------------------------------------
#
#    < >内はパーミッション値(表記の値は一般値ですのであなたのサーバでの最適値に変更してください)
#
# /2shot/ (ＣＧＩが実行できる任意のディレクトリ)
#    |
#    |-- 2shot.cgi <755>
#    |-- config.cgi <644>           ←環境設定ファイル
#    |-- acl.cgi <644>              ←アクセス拒否リスト　IPまたはドメイン名を1行１データで記述
#    |-- check.cgi <644>            ←待機メッセージの拒否文字列のファイル
#    |--/x/ (データ格納フォルダ) <755>　←フォルダの場所はconfig.cgiに記載する
#        |
#        |-- .htaccess このフォルダにアクセスできないようにする設定
#        |             もしこの設定が使えないサーバの場合は、データ格納フォルダをＷＷＷから
#        |             見えない場所に用意するか、/x/を推測しにくい名称に変えるといいでしょう。
#        |
#        |-- 01.ent <666>
#        |-- 01.mes <666>
#        |-- 01.log <666>
#        |-- 01.lock <666>
#        |   以下略... 必要な分だけ用意する
#        |
*/

/* リリースバージョン番号 */
#define	REL_VERSION		"Ver4.2a"

/* ファイルロック処理をする場合は１ */
#define USELOCK 1
#define	HAVE_FLOCK	1		/* FLOCK関数を使用する場合は１　LOCKFのときは０ */
#define READ_BUFFER_SIZE 256
/* ロックの試行間隔(マイクロ秒) */
//#define LOCK_INTERVAL_MICROSECS (500 * 1000)
#define LOCK_INTERVAL_MICROSECS (100* 1000)
#define RETRY_CNT   10			/* 部屋ロック　リトライ回数 */
      

/* atexit 関数が使えるときは 1 にする */
#define USE_ATEXIT 0

/* 保存・表示の漢字コード */
#define KANJICODE "utf-8"

#define	READ_MODE	"r"
#define	WRITE_MODE	"w"
#define	APPEND_MODE	"a"

// URLのホストの後が正しい値かどうか判別する
#define isURL(c) (isalpha(c)||isdigit(c)||c=='*'||c=='-'||c=='.'||c=='@'||c=='_'||c=='+'||c=='%'||c=='/'||c=='~'||c==':')


#if USELOCK
#include <sys/file.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include "yutils.h"
#include "ycookie.h"
#include "config.h"
#include "md5.h"

/* 文字数の制限 */
#define	HANDLE_MAX	32
#define	MSG_MAX		256
#define BODY_MAX	256
#define COMMENT_MAX	4096

/* グローバル変数 */

// 管理者名
static char ADMIN_NAME[32];
// 管理者パスワード
static char ADMIN_PASS[32];

// 部屋別ログ・・・ファイルのベースディレクトリ
static char BASE_DIR[128];


// 一覧画面でのコメント行
char *LIST_COMMENT = NULL;

// チャット画面のコメント　上・下段

static char *UPPER_COMMENT = NULL;
static char *LOWER_COMMENT = NULL;
static int   UPPER_TYPE = 0;		// 上段フレームの表示位置　０：上　１：下
static int   LOWER_TYPE = 0;		// 下段フレームの表示位置　０：上　１：下

// ○ホスト名の告示(2:ソース上にする 1:画面上にする 0:しない)
static int VIEW_HOST=1;

// ○１部屋で会話ログ(.log)で利用できる最大ファイルサイズ
#define FUSE	5000

// ○タグを許可 1:する 0:しない
static int	ALLOW_HTML=1;

// ◎A　HREFタグとIMGタグの自動変換 1:する 0:しない
static int	auto_url=0;

// ○空室一覧表の自動更新時間の設定(秒) ... 60秒以上有効
static int RELOAD=60;

// ○無発言監視タイマ(秒)
static int LIMIT=600;
static int NOTICE=0;

//　内部タイトル・ボディ情報
static char TITLE[64];

//通常状態
static char BODY[BODY_MAX+1];

//リミット近辺状態
static char BODY_NOTICE[BODY_MAX+1];

// 警告時のメッセージ（サウンド系）
static char NOTICE_MSG[256];
//"<BGSOUND src=\"./notice.wav\" loop=2>"

// 満室時のオーナーメッセージ
static char FULL_MSG[256];

// 内部入室状況
static int room_flg;

// ○男女の色

static char	COLOR1[64];	// 男
static char	COLOR2[64];	// 女

// ○状態表示の色
static char COLOR3[64];	// 空室
static char COLOR4[64];	// 待機中
static char COLOR5[64];	// 満室

// ○自分の発言の色(薄めにする)
static char	COLOR6[64];
// ○画面に挿入するリンクＵＲＬ(URL)と名称
static char MODORU[64];
static char MODORU_NAME[256];

// ○チャット表示行
static int  MAX = 30;
//　部屋数
static int  ROOM_MAX;

// 入室時のログクリアモード　0:しない　1:する
static int flgLogClear = 0;

// 女性専用部屋開始番号
static int female_room = 0;

// アクセスログフラグ
static int access_log_flg = 0;

// 初期化時の更新間隔秒数

static int	dauto = 0;
char	chat_file[256];
char	mes_file[256];
char	entry_file[256];
char	lock_file[256]={0};
int		ninzu;
unsigned char	password[BUFSIZ];
unsigned char	cryptpass[BUFSIZ];
unsigned char	wkpass[BUFSIZ];
int		Clr = 0;

char	**RoomTable = NULL;
int	icount;
char	wkbuff[1024];
char	*cmd;
char	*room;
int		n_room;
char	pwd[BUFSIZ];
char	*num;
int		n_num;
char	*script_name;
char	ROOM_PWD[2][BUFSIZ];
FILE *lockfile_fp;

//
// 部屋クローズのステータス定義
//
enum {
	OWNER_CLOSE=0,
	TIMEOUT_CLOSE,
	ADMIN_CLOSE
};

int main(void);
void First(void);
void InForm(void);
void ChatForm(void);
void RoomFlgChg(void);
char *Change_Url(char *data);
void Chat(void);
void Write(void);
void Entry(void);
void Bye(void);
void ProfUpdate(void);
int GetData(char *filename);
void List(void);
void Form(void);
void SSI_View(void);
void Clear(void);
void Close(int n);
void error(char *hd,char *msg);
void copyright(void );
void printerror(char *msg1, char *msg2);
void make_quote(char buf[], int len);
char *read_file(char *filename);
char **split_by_char(char splitchar, char *str, int expected_items);
void lockfile(FILE *fp);
void unlockfile(FILE *fp);
void sigexit(int sig);
void RoomLock(void);
void RoomUnLock(void);
size_t get_file_size(FILE *fp);
void safe_strcat(char *dst, char *src, int len);
void safe_strcpy(char *dst, char *src, int len);
int safe_stricmp(char *dst, char *src);
void init_form(char *kanjicode);
char *jcode_convert(char *value, char *charcode);
void set_form(char *key, char *value);
char *get_form(char *key);
void exit_form(void);
void myexit(int x);
void norm_form(char *key);
int read_config(void);
void encode_pass(char *pass, char encpass[], int addval);
char *mycrypt(char *str, char *salt);
void access_log(int type,int room,char *Name,char *Host,char *Addr);
int Check_Tag(char *buf,char *pat);
int ptime(char *filename);
long GetFileSize(char *filename);
void DebugPrint(char *text);

/*BASE 64 */
int base64_encode(unsigned char *src, size_t sourcelen, char *target, size_t targetlen);
size_t base64_decode(char *aex, unsigned char *target, size_t targetlen);


#define TOKENS (10+26+26)

char token[TOKENS] = {
        '0','1','2','3','4','5','6','7','8','9',
        'A','B','C','D','E','F','G','H','I','J','K','L','M',
        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
        'a','b','c','d','e','f','g','h','i','j','k','l','m',
        'n','o','p','q','r','s','t','u','v','w','x','y','z',
};

/*
 * MAIN ROUTINE
 */
char	date_now[16]; 

int main()
{
int	i,j,mugon;
char	*wkp;
char	text[256];
struct tm *gmt;
time_t ti;

	ti = time(NULL);
	ti += 9*60*60;
	gmt = gmtime(&ti);
	sprintf(date_now,"%02d:%02d",gmt->tm_hour,gmt->tm_min);
	

	script_name = getenv("SCRIPT_NAME");

    /* 配列 form[] の初期化 */
    init_form(KANJICODE);

    /* クッキーの取得 */
    init_cookie(cookiename);
	/* 設定ファイルの読み込み */
	if (read_config())
	{	error("エラー","設定ファイルがありません");
	}
	norm_form("chat_name");


	cmd = get_form("img");
	if (strncmp(cmd,"copyright",9) == 0)
	{ 	copyright();
	}

	cmd = get_form("action");

	
	if (strcmp(cmd,"") == 0)
	{	First();
		myexit(0);
	}
	if (strcmp(cmd,"view") == 0)
	{	SSI_View();
		myexit(0);
	}
	if ( strcmp(cmd,"entry") != 0)
	{	printf("Content-type: text/html;charset=utf-8\n\n");
	}
	
	if (strcmp(cmd,"Form") == 0)
	{
		Form();
		myexit(0);
	}
	if (strcmp(cmd,"List") == 0)
	{
		List();
		myexit(0);
	}
	room = get_form("room");
	if (room[0] == 0)
	{	error("エラー","部屋を選択してください.");
	}
	n_room = atoi(room);

	sprintf(chat_file,"%s%s.log",BASE_DIR,room);
	sprintf(entry_file,"%s%s.ent",BASE_DIR,room);
	sprintf(mes_file,"%s%s.mes",BASE_DIR,room);
	// 1.シグナルハンドラ設定
    signal(SIGTERM, sigexit);

	if (GetFileSize(chat_file) != 0)
	{
		mugon = ptime(chat_file);
		if (mugon >= LIMIT)
		{	Close(TIMEOUT_CLOSE);
		}
	}
	
	// エントリファイル内の読み込み
	ninzu = GetData(entry_file);

	// 入室チェック
	if (strcmp(cmd,"entry") == 0)
	{	if (strcmp(get_form("chat_name"),ADMIN_NAME) == 0 &&
			strcmp(get_form("mes"),ADMIN_PASS) == 0)
		{
			char	buff[256];
			
			sprintf(buff,"%sを強制的に閉鎖しました",RoomTable[n_room-1]);
			Close(ADMIN_CLOSE); error("強制閉鎖",buff);
		} else
		{	Entry();
			InForm();
		}
		myexit(0);
	}

	num = get_form("num");
	n_num = atoi(num);

	if (n_num > 1 || *num == 0)
	{	error("警告","不正なデータがあります");
		myexit(0);
	}


	wkp = get_form("pwd");
	if (wkp[0] == 0)
	{	
		error("認証エラー","あなたは現在の利用者であるという認証ができません"); 
		myexit(0);
		
	}
		
	strcpy(pwd,wkp);
	char *cp = mycrypt(pwd, &ROOM_PWD[n_num][1]);
	if (strcmp(&ROOM_PWD[n_num][1], cp) != 0 || ROOM_PWD[n_num][0] == 0 )
	{	
		switch (n_num)
		{
		case 0:			/* 自分がオーナーの場合 */
			if (ROOM_PWD[0][0])
			{	sprintf(text,"あなたは部屋オーナーではありません");
			} else
			{	sprintf(text,"%d秒の無発言閉鎖になりました",LIMIT);
			}
			break;
		case 1:			/* オーナーじゃない場合 */
			if (ROOM_PWD[1][0])
			{	if (ROOM_PWD[0][0])
				{		sprintf(text,"あなたは現在この部屋の利用者ではありません");
				} else
				{	strcpy(text,"異常閉鎖されました");
				}
			} else
			{	if (ROOM_PWD[0][0])
				{		sprintf(text,"あなたは利用者ではありません");
				} else
				{	sprintf(text,"部屋のオーナーによる閉鎖また%d秒の無発言閉鎖になりました",LIMIT);
				}
			}
			break;
		}
		error("退室/閉鎖",text);
		myexit(0);
	}
	
	if (strcmp(cmd,"ChatForm") == 0) 
	{
		ChatForm();
		myexit(0);
	}
	

	if (strcmp(get_form("Clear"),"画面クリア")==0 && n_num==0)
	{	Clear();
	}

	wkp = get_form("announce");
	if (*wkp != 0 && n_num==0)
	{	
		room_flg |= 2;
		RoomFlgChg();
	}

	if (strcmp(cmd,"close") == 0)
	{
		if (!n_num)
		{	Close(OWNER_CLOSE);
		} else
		{	error("警告","あなたはオーナーではありません");
		}
		myexit(0);
	}
	if (strcmp(cmd,"bye") == 0 || strcmp(cmd,"bye2") == 0)
	{	Bye();
	}

	if (strcmp(cmd,"Prof") == 0 && n_num == 0)
	{	ProfUpdate();
		Chat();
	}
	if (strcmp(cmd,"Chat") == 0)
	{	Chat();
	}
	myexit(0);
}

/*
 * 最初のリスト表示画面のメイン表示部
 */
void First()
{
	puts ("Content-type: text/html;charset=utf-8\n\n");
	puts("<html><head>");
	printf("<title>%s</title>",TITLE);
	puts("<frameset rows=\"30%,*\" border=0>");
	printf("<frame src=\"%s?action=Form\" name=\"Up\">",script_name);
	printf("<frame src=\"%s?action=List\" name=\"Down\">",script_name);

	puts("</framset></head>");
	puts("<noframes>");
	puts(BODY);
	puts("<center>"
		"<h2>フレーム専用型チャットのため<br>"
		"フレーム対応ブラウザをご利用ください</h2>");
	printf("〔<a href=\"%s\" TARGET=\"_top\">%s</a>〕"
		   "</center>"
		   "</body>"
			"</noframes>"
			"</html>",MODORU,MODORU_NAME );
}


static const char *weekday[]={ "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
static const char *month[] ={ "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

/*
 * チャット画面表示のメイン部（入室後の画面レイアウト）
 */
void InForm()
{
	char buf[MSG_MAX*2];
	
	clear_cookie();
	base64_encode((unsigned char*)get_form("chat_name"),HANDLE_MAX,buf,MSG_MAX*2);
	
    set_cookie("name",buf);
    set_cookie("sex", get_form("sex"));
    base64_encode((unsigned char *)get_form("mes"),MSG_MAX,buf,MSG_MAX*2);
    set_cookie("mes",buf);
    print_cookie(cookiename, cookiedays, NULL);
	
	printf("Content-Type: text/html;charset=utf-8\n\n");
	printf("<html><head><title>%s - %s</title>",TITLE,RoomTable[atoi(room)-1]);
	puts ("<frameset rows=\"40%,*\"  border=\"0\">\n");
	printf ("<frame src=\"%s?action=ChatForm&amp;room=%s&amp;pwd=%s&amp;num=%d&amp;auto=%d\" name=\"Up\">",
					script_name,room,password,ninzu,dauto);
	printf ("<frame src=\"%s?action=Chat&amp;room=%s&amp;pwd=%s&amp;num=%d&amp;auto=%d\" name=\"Down\">",
					script_name,room,password,ninzu,dauto);
	puts("</frameset></head>");
	puts("<noframes>");
	puts("<center>");
	puts("<h2>フレーム専用型チャットのため<br>");
	puts("フレーム対応ブラウザをご利用ください</h2>");
	printf("〔<a href=\"%s\" TARGET=\"_top\">%s</a>〕",MODORU,MODORU_NAME);
	puts("</center></noframes></html>");
}

/*
 * チャット発言画面
 */
void ChatForm()
{
char	*rbuff;
char	**lines;
char	**items;
char	uname[HANDLE_MAX+1];
char	usex[64];
char	umessage[MSG_MAX+1];
int	i;
char	buff[256];


	uname[0] = usex[0] = 0;
	rbuff = read_file(mes_file);
	lines = split_by_char('\n', rbuff, 2);
	items = split_by_char('\t', lines[n_num], 4);
	safe_strcpy(uname,items[0] ? items[0]:"",HANDLE_MAX);
	usex[0] = items[1] ? items[1][0]:'?';
	switch (usex[0])
	{
	case 'M':
		sprintf(usex,"(<span style=\"color:%s\">男</span>)",COLOR1);
		break;
	case 'F':
		sprintf(usex, "(<span style=\"color:%s\">女</span>)",COLOR2);
		break;
	default:
		sprintf(usex, "(<span style=\"color:%s\">謎</span>)",COLOR1);
		break;
	}
	safe_strcpy(umessage,items[2] ? items[2]:"",MSG_MAX);
	free(items);
	free(lines);
	free(rbuff);

	puts("<!DOCTYPE html>");
	puts("<html><head><title>");
	puts(TITLE);
	printf(" - %s</title><meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">",
		RoomTable[n_room-1]);
	puts("<script  type=\"text/javascript\">");
	puts("<!--");
	puts("var SayFlg = 0;\n");
	puts("function SayBtn() { SayFlg = 1; }\n");
	puts("function PageBack(){ history.back(); }");
	puts("function send()");
	puts("{");
	puts("  if (SayFlg == 1)");
	puts("  {  SayFlg = 0;");
	puts("     document.InputForm.Value.value = \"\";");
	puts("     document.InputForm.Value.select();");
	puts("     document.InputForm.Value.focus();");
	puts("  }");
	puts("}");
	puts("function sendTimer() { setTimeout(\"send()\",500); }");
	puts("function message1()");
	puts("{	f = confirm(\"閉鎖します. よろしいですか？\");return f}");
	puts("function message2(){ f = confirm(\"相手を退室させます. よろしいですか？\");");
	puts("document.InputForm.auto[1].click();return f}");
	printf("function message3(){f = confirm(\"あなただけ退室します. よろしいですか？\");return f} //--></script>");
	puts("<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
	puts("</head>");
	strcpy(buff,BODY);
	buff[strlen(buff)-1] = 0;
	puts(buff);
	sprintf(buff," onLoad=\"document.InputForm.Value.focus();\">");
	puts( buff);
	if (UPPER_TYPE==0)		// コメント上段表示
	{	puts(UPPER_COMMENT);
	}
	puts("<table class=\"say\">");
	printf("<tr><th><span class=\"room_name\">《%s》</span>　<span class=\"user_name\">%s</span>%s</th>"
		,RoomTable[n_room-1]
		,uname,usex);
	printf("<form method=POST action=\"%s\" target=\"Down\" onSubmit=\"sendTimer()\">"
		,script_name);
	puts("<input type=hidden name=\"action\" value=\"Chat\">");
	printf("<input type=hidden name=\"room\" value=\"%s\">"	,room);
	printf("<input type=hidden name=\"pwd\" value=\"%s\">",pwd);
	printf("<input type=hidden name=\"num\" value=\"%s\">",num);
	puts("<th><input type=submit value=\"手動更新\" onClick=\"document.InputForm.auto[0].click();\"></th></form>");
	if (n_num == 0)
	{
		printf("<form method=POST action=\"%s\" target=\"Down\">",script_name);
		puts("<input type=hidden name=\"action\" value=\"close\">");
		printf("<input type=hidden name=\"room\" value=\"%s\">"	,room);
		printf("<input type=hidden name=\"pwd\" value=\"%s\">",pwd);
		printf("<input type=hidden name=\"num\" value=\"%s\">",num);
		printf("<input type=hidden name=\"auto\" value=\"%d\">",dauto);

		puts("<th><input type=submit value=\"閉鎖\" onClick=\"message1(); return f\"></th></form>");
		printf("<form method=POST action=\"%s\" target=\"Down\">",script_name);
		puts("<input type=hidden name=\"action\" value=\"bye2\">");
		printf("<input type=hidden name=\"room\" value=\"%s\">"	,room);
		printf("<input type=hidden name=\"pwd\" value=\"%s\">",pwd);
		printf("<input type=hidden name=\"num\" value=\"%s\">",num);
		printf("<input type=hidden name=\"auto\" value=\"%d\">",dauto);
	
		puts("<th><input type=submit value=\"相手を退室\" onClick=\"message2(); return f\"></th></form>");
	} else {
		printf("<form method=POST action=\"%s\" target=\"Down\">",script_name);
		puts("<input type=hidden name=\"action\" value=\"bye\">");
		
		printf("<input type=hidden name=\"room\" value=\"%s\">"	,room);
		printf("<input type=hidden name=\"pwd\" value=\"%s\">",pwd);
		printf("<input type=hidden name=\"num\" value=\"%s\">",num);
		
		puts("<th colspan=2><input type=submit value=\"退室\" onClick=\"message3(); return f\"></th></form>");
	}

	printf("</tr><tr><form method=POST action=\"%s\" target=\"Down\" name=\"InputForm\" onSubmit=\"SayBtn();sendTimer()\">",script_name);
	
	puts("<input type=hidden name=\"action\" value=\"Chat\">");
	
	printf("<input type=hidden name=\"room\" value=\"%s\">"	,room);
	printf("<input type=hidden name=\"pwd\" value=\"%s\">",pwd);
	printf("<input type=hidden name=\"num\" value=\"%s\">",num);
	puts("<td><input type=text name=\"Value\" size=60>");
	puts("<input type=submit value=\"発言\"　onClick=\"SayBtn();return true\"></td>");
	puts("<td colspan=3><input type=radio name=\"auto\" value=0 >手動更新");
	printf("<input type=radio name=\"auto\" value=%d checked>%d秒</td></tr></form>",dauto,dauto);
	puts("</table>");
	if (n_num == 0)		// オーナーの場合
	{
		puts("<hr><br><table class=\"profile_box\">");
		
		printf("<tr><td><form method=POST action=\"%s\" target=\"Down\">\n",script_name);
		printf("<input type=hidden name=\"action\" value=\"Prof\">\n");
		printf("<input type=hidden name=\"room\" value=\"%s\">\n",room);
		printf("<input type=hidden name=\"pwd\" value=\"%s\">\n",pwd);
		printf("<input type=hidden name=\"num\" value=\"%s\">\n",num);
		printf("<input type=hidden name=\"auto\" value=\"%s\">\n",get_form("auto"));
		printf("待機メッセージ&nbsp;&nbsp;：&nbsp;&nbsp;<input type=text name=\"mes\" size=60 maxlength=%d value=\"%s\">",MSG_MAX,umessage);
		puts("<input type=submit name=\"update\" value=\"更新\"></form>\n");
		puts("</td></tr>\n");
		puts("<tr><td><div class=\"waiting_msg\">※満室になるまでは待機メッセージの変更が可能です</div>\n");
		printf("</td></tr></table>");
	}

	if (UPPER_TYPE==1)		// コメント上段表示
	{	puts(UPPER_COMMENT);
	}

	puts("<script  type=\"text/javascript\">");
	puts("<!-- \n");
	puts(" document.InputForm.Value.select();");
	puts("     document.InputForm.Value.focus();");
	puts(" //--></script>\n");

	puts("</body>");
	puts("</html>");
}

/*
 * 満室チェック用のフラグ制御ルーチン
 */
void RoomFlgChg()
{
int	i,lock_flg;
FILE	*fp;

	if (!lock_file[0])
	{	sprintf(lock_file,"%s%s.lock",BASE_DIR,room);
		RoomLock();		// 部屋ファイルのロック
		lock_flg = 1;
	}

	ROOM_PWD[0][0] = room_flg+'0';
	fp = fopen(entry_file,WRITE_MODE);
	
	for (i=0;i < 2 && ROOM_PWD[i][0] != 0 ;++i)
	{	fprintf(fp,"%s\n",ROOM_PWD[i]);
	}
	fclose(fp);
	if (lock_flg)
	{	RoomUnLock();
	}
}
//
//  自動URL書き換えルーチン
//
char *Change_Url(char *data)
{
int tag_type;
char *work,*p,*p1;
char	url[64];

	work= malloc(2048);
	strcpy(work,data);
	if (auto_url)
	{	p = strstr(data,"http");
		if (p != NULL)
		{	tag_type =0;
#if 1		
			if (strstr(data,".jpg") != NULL)
			{  	tag_type = 1;
			}
			if (strstr(data,".gif") != NULL)
			{	tag_type = 1;
			}
			url[0] = *p;
			*p++ = 0;
			strcpy(work,data);
			if (tag_type)	// 画像だ
			{	strcat(work,"<img src=\x22");
			} else	
			{	strcat(work,"<a href=\x22");
			}

			p1 = &url[1];

			while (*p != 0)
			{
				if ( !isURL(*p) )
				{	break;
				}
				*p1++ = *p++;
			}
			*p1 = 0;
			strcat(work,url);
			if (tag_type)
			{	strcat(work,"\x22 border=0>");
			} else
			{	strcat(work,"\x22 target=_blank>");
				strcat(work,url);
				strcat(work,"</a>");
			}
			strcat(work,p);

#endif
		}
	}
	return work;
}

/*
 * チャット発言表示ルーチン
 */
void Chat()
{
char	**items;
char	**lines;
char	**disps;
char	uname[HANDLE_MAX+1];
char	*buff,*msg,*say;
char	RELinfo[BUFSIZ];
int	i,maxline,keika;
size_t	nn;
FILE	*fp;
char *tmp;
	
	
	buff = read_file(mes_file);
	lines = split_by_char('\n', buff, 2);
	items = split_by_char('\t', lines[n_num], 4);
	safe_strcpy(uname,items[0],HANDLE_MAX);
	free(items);
	free(lines);
	free(buff);

	keika = ptime(chat_file);

	msg = get_form("Value"); 
	if (msg != NULL && msg[0]!=0) 
	{	Write();
	}

	if (atoi(get_form("auto"))  > 0)
	{	sprintf(RELinfo,"〔%s秒自動更新〕",get_form("auto"));
	} else { 
		strcpy(RELinfo , "〔手動更新〕"); 
	}
	puts("<!DOCTYPE html>");
	puts("<html><head>");
	printf("<title>%s - %s</title>\n",TITLE,RoomTable[atoi(get_form("room")-1)]);
	puts("<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">");
	puts("<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
	if (atoi(get_form("auto")) > 0)
	{	printf("<meta http-equiv=\"refresh\" content=\"%s;url=%s?action=Chat&amp;room=%s&amp;pwd=%s&amp;num=%s&amp;Value=&amp;auto=%s\">\n",
			get_form("auto"),script_name,room,pwd,num,get_form("auto"));
	}

	puts("<script  type=\"text/javascript\">");
	puts("<!--");
	puts("function PageBack(){ history.back(); }");
	// 入室お知らせ機能のチェック
	if (n_num == 0) {
		if (room_flg == 3)
		{	room_flg |= 4;
			puts(FULL_MSG);
			RoomFlgChg();
		}
	}
	puts("//-->");
	puts("</script>");
	
	printf("</head>");
        if(NOTICE != 0 && LIMIT-keika<NOTICE){
            printf("%s\n",BODY_NOTICE);
            if(room_flg == 2) puts(NOTICE_MSG);
        }else{
            printf("%s\n",BODY);
	}

	if (LOWER_TYPE==0)
	{	printf("%s\n",LOWER_COMMENT);
	}
	
	// 部屋の状態表示
	switch (ninzu)
	{
	case 0:
		tmp = "空き";
		break;
	case 1:
		tmp = "待ち状態";
		break;
	case 2:
		tmp ="<span class=\"room_lock\">満室</span>";
		break;
	}
	printf("<div class=\"room_status\">部屋の状態:%s</div>",tmp);
	puts("<hr>");
	say = malloc(2048);

	buff = read_file(chat_file);
	if (buff)
	{	lines = split_by_char('\n', buff, 0);
		if (lines)
		{	for (maxline = 0; lines[maxline]; maxline++) {
				;
			}
			//maxline--;
			for (i = 0;i < maxline;++i)
			{
				if (lines[maxline-i-1][0])
				{	msg = malloc(strlen(lines[maxline-i-1])+1);
					if (msg)
					{	strcpy(msg,lines[maxline-i-1]);
						disps = split_by_char('\t', msg, 3);
						
						if (disps != NULL)
						{
							say = Change_Url(disps[2]);
							if (strcmp(disps[1],uname) == 0)
							{	printf( "<span style=\"color:%s\">%s &gt; %s</span> <span style=\"font-size:xsmall; color=%s\">(%s)</span><hr>",
								COLOR6,disps[1],say,COLOR6,disps[0]);
							}
							else {
								printf("<b>%s</b> &gt; %s <span style=\"font-size:xsmall\">(%s)</span><hr>",
									disps[1],say,disps[0]);
							}
							free(say);
							free(disps);

						}
						free(msg);
					}
				}
			}
			free(lines);
		}
		free(buff);
	}
	printf("<form method=\"post\" action=\"%s\" target=\"_self\">\n",script_name);
	puts("<input type=hidden name=\"action\" value=\"Chat\">\n");
	printf("<input type=hidden name=\"room\" value=\"%s\">\n",room);
	printf("<input type=hidden name=\"pwd\" value=\"%s\">\n",pwd);
	printf("<input type=hidden name=\"num\" value=\"%s\">\n",num);
	printf("<input type=hidden name=\"auto\" value=\"%s\">\n",get_form("auto"));

	if (n_num == 0) {
		 puts("<input type=submit name=\"Clear\" value=\"画面クリア\">\n");
		 if (room_flg == 0)
		 {	puts("&nbsp;&nbsp;<input type=submit name=\"announce\" value=\"入室お知らせ\">");
		 } else
		 {	if (room_flg == 2)
		 	{	puts("&nbsp;&nbsp;<b>入室お知らせ機能ＯＮ</b>\n");
		 	}
		 }
	}
	if (LIMIT > 0)
	{	printf("<br>%s〔無発言監視タイマ%d秒経過→%d秒後閉鎖〕〔表示%d行〕</form>",
			RELinfo,keika,LIMIT,MAX );
	}
	if (LOWER_TYPE==1)
	{	printf("%s\n",LOWER_COMMENT);
	}
	
	puts("<div align=right><a href=\"http://www.rescue.ne.jp/\" target=\"_blank\">");
	printf("<img src=\"%s?img=copyright\" border=0 alt=\"2SHOT-CHAT v4.0\"></a><br />",script_name);

	printf("Arranged&nbsp; by&nbsp; <a href=\"http://www.petit-power.com/\" target=_blank>PETIT-POWER %s</a></div>\n",REL_VERSION);

	puts("</body>");
	puts("</html>");
}

/*
 * 発言書き込みルーチン
 */
void Write()
{
FILE	*fp;
char	**items;
char	**lines;
char	uname[HANDLE_MAX+1];
char	*buff;
int		i,maxline;

	// 部屋ロックフィアル名生成
	sprintf(lock_file,"%s%s.lock",BASE_DIR,room);
	RoomLock();		// 部屋ファイルのロック

	norm_form("Value");
	buff = read_file(mes_file);
	lines = split_by_char('\n', buff, 2);
	items = split_by_char('\t', lines[n_num], 4);
	safe_strcpy(uname,items[0],HANDLE_MAX);
	free(items);
	free(lines);
	free(buff);

	buff = read_file(chat_file);
	lines = split_by_char('\n', buff,0);
	for (maxline = 0;lines[maxline] != NULL && maxline < MAX;++maxline) ;
	
	fp = fopen(chat_file,WRITE_MODE);
	
	for (i = (maxline >= MAX ? 1:0);i < maxline;++i)
	{	if (!lines[i][0])
		{	break;
		}
		fprintf(fp,"%s\n",lines[i]);

	}

	fprintf(fp,"%s\t%s\t%s\n",date_now,uname,get_form("Value"));
	fclose(fp);
	free(lines);
	free(buff);
	RoomUnLock();
}

/*
 * 入室処理ルーチン
 */
void Entry()
{
unsigned char	buff[HANDLE_MAX+MSG_MAX+4];
char *name,*wkp;
char	**lines;
int		err=0;
unsigned char	mes_sex[64],msg1[64],msg2[64],msg3[64];
unsigned char	host[256],addr[256];
char salt[9];
FILE	*fp,*fp2;
int		i,maxline;
union _PACK_ADDR {
	unsigned char	c[4];
	unsigned long 	ad;
} paddr;
struct hostent *peer_host;


	// 部屋ロックフィアル名生成
	sprintf(lock_file,"%s%s.lock",BASE_DIR,room);
	RoomLock();		// 部屋ファイルのロック

	i = ALLOW_HTML; ALLOW_HTML=0;
	norm_form("chatname");
	norm_form("mes");
	norm_form("sex");
	ALLOW_HTML=i;

	for (i = 0;i < 2;++i)
	{	
		ROOM_PWD[i][0] = 0;
	}

	fp = fopen(entry_file,"r");
	if (fp == NULL)
	{
		error("警告","データファイルがありません、管理者に連絡してください");
		myexit(0);
	}
	ninzu = 0;
	for (i = 0;i < 2;++i)
	{	if (fgets((char *)buff,256,fp) == NULL)
		{	break;
		}
		if (buff[0] == 0)
		{	break;
		}
		strtok((char *)buff,"\r\n");
		strcpy((char *)&ROOM_PWD[i][0],buff);
		ninzu++;
	}
	if (ROOM_PWD[0][0]) 
	{	room_flg = ROOM_PWD[0][0]-'0';
	} else
	{	room_flg = 0;
	}
		fclose(fp);

	if (ninzu >= 2)
	{	error("満室","この部屋は満室です。"); 
		myexit(0);
	}

	// 女性専用部屋のチェック
	
	if (ninzu == 0 && female_room != 0 &&
		n_room >= female_room)
	{
		// 女性かどうかをチェックする
		if (strcmp(get_form("sex"),"F")!=0) 
		{	
			error("警告","ココは女性オーナー専用部屋です。"); 
			myexit(0);
		}
		
	}
	
	name = get_form("chat_name");
	if (name[0] == 0) {
		err = 1; set_form("chat_name", "匿名");
		name = get_form("chat_name");
	}
	
	if (name[0] == ' ' || (unsigned char)name[0] < ' ')
	{
		error("警告","名前に空白または文字コード以外は使えません");
	}

	if (strlen(name) > HANDLE_MAX)
	{
		error("警告","名前が長すぎます");
	}
	if (ninzu == 1) {
		fp2 = fopen(mes_file,"r");
		fgets((char *)buff,HANDLE_MAX+MSG_MAX+4,fp2);
		lines = (char **)split_by_char('\t', (char *)buff, 0);
		if (strcmp(name,"匿名") == 0) {
			err = 1; 
			set_form("chat_name", "匿名2");
			name = get_form("chat_name");
		} 
		if (strcmp(name,lines[0]) == 0) {
			err = 1; 
			set_form("chat_name", "匿名2");
			name = get_form("chat_name");
		}
		strcpy((char *)mes_sex,lines[1]);
		free(lines);
		fclose(fp2);
	} else
	{	Close(0);
	}
	// メッセージチェックは専用のファイルに文字列を書くこと
	fp2 = fopen(CHECK_FILE,"r");
	
	if (fp2 != NULL)
	{
		wkp = get_form("mes");
#if 0
		if ((unsigned char )*wkp < ' ')
		{	err = -2;
		}
#endif
		if (strlen(wkp) > MSG_MAX)
		{	err = -3;
		}
		while (1) {
			if (fgets((char *)cryptpass,256,fp2) == NULL)
			{	break;
			}
			strtok((char *)cryptpass,"\r\n\t");
			if (!cryptpass[0])
			{	break;
			}
			if (strstr(wkp,cryptpass) != NULL)
			{	err = -1;
				break;
			}
		}
		fclose(fp2);
	}
	if (err < 0)
	{	
		switch (err)
		{
		case -1:
			error("警告","待機メッセージに使用禁止の言葉が入っています"); 
			break;
		case -2:
			error("警告","待機メッセージが入っていません"); 
			break;
		case -3:
			error("警告","待機メッセージが長すぎます"); 
			break;
		}
			myexit(0);
	}
	wkp = get_form("sex");
	switch (wkp[0])
	{
	case 'M':
		sprintf(mes_sex,"(<span style=\"color:%s\">男</span>)",COLOR1);
		break;
	case 'F':
		 sprintf(mes_sex,"(<span style=\"color:%s\">女</span>)",COLOR2);
		break;
	default:
		fclose(fp);
		error("不正エラー","性別が正しくありません");
		myexit(0);
		break;
	}
	/* #ホスト名の取得(リムネットなら次の４行を指定のプログラムと入れ替える) */
	
	host[0] = addr[0] = 0;
	if ((wkp = getenv("REMOTE_HOST")) != NULL)
	{	strcpy(host,wkp);
	}
	if ((wkp = getenv("REMOTE_ADDR")) != NULL)
	{	strcpy(addr,wkp);
	}
	if (!host[0]) { strcpy(host,addr); }
#if 0
	if (strcmp(host,addr) == 0) { 
		sscanf(host,"%d.%d.%d.%d",&paddr.c[0],&paddr.c[1],&paddr.c[2],&paddr.c[3]);
		peer_host = gethostbyaddr(&paddr.c[0],sizeof(paddr.ad),AF_INET);
		if (peer_host != NULL && peer_host->h_name != NULL)
				strcpy(host,peer_host->h_name);

	}
#endif
	// ＩＰ＆ＨＯＳＴ名でのＡＣＬ制限
	fp2 = fopen(ACL_FILE,"rt");
	if (fp2 != NULL)
	{
		while (1) {
			if (fgets(cryptpass,256,fp2) == NULL)
			{	break;
			}
			strtok(cryptpass,"\r\n\t");
			if (!cryptpass[0])
			{	break;
			}
			if (strstr(addr,cryptpass) != NULL ||
				strstr(host,cryptpass) !=NULL)
			{	err = -1;
				break;
			}
		}
		fclose(fp2);
	}
	if (err == -1)
	{
	error("警告","入室禁止プロバイダです。"); 
		myexit(0);
	}
	
	fp2 = fopen(mes_file,APPEND_MODE);
	fprintf(fp2,"%s\t%s\t%s\t%s\n",
			name,get_form("sex"),get_form("mes"),host);
	fclose(fp2);

//    DebugPrint("MES WRITE DONE");

	srand((unsigned)time(NULL));
	for (i = 0;i < 7;++i)
	{	password[i] = token[rand()%TOKENS];
	}
	password[7]=0;
	encode_pass(password,cryptpass,7);
	fp = fopen(entry_file,"w");

	if (ninzu)
	{	room_flg |= 1;
		ROOM_PWD[0][0] =  room_flg+'0';
		fprintf(fp,"%s\n",ROOM_PWD[0]);
	}
	fprintf(fp,"0%s\n",cryptpass);
	fclose(fp);
	fflush(fp);

	if (flgLogClear)
	{	fp2 = fopen(chat_file,"w");
	} else
	{	fp2 = fopen(chat_file,"a");
	}
	msg1[0] = msg2[0] = 0;
	if (ninzu == 0) { 
		strcpy(msg1,"オーナーとして"); 
	} else if (ninzu == 1) { 
		strcpy(msg2,"ので、このチャットルームをロックしました"); 
	}
	msg3[0] = 0;
	switch (VIEW_HOST)
	{
	case 1:
		 sprintf(msg3,"<span style=\"font-size:xxsmall\">&lt;%s&gt;</span>",host);
		 break;
	case 2:
		sprintf(msg3,"<!-- %s -->",host);
		break;
	default:
		break;
	}
	
	fprintf(fp2,"%s\tおしらせ\t%s%sさん%sが%s入室しました%s\n",date_now,name,mes_sex,msg3,msg1,msg2);

	wkp = get_form("mes");
	if (*wkp != 0) { 
		fprintf(fp2,"%s\t%sさんのプロフィール\t%s\n",
			date_now,name,get_form("mes"));
	}
	if (err) {
		fprintf(fp2,"%s\t<span style=\"color:red\">おしらせ</span>\tチャット名が入力されていないか、使えない文字<>,;:が検知された等の理由で匿名扱いとなります.\n",
			date_now);
	}
	fclose(fp2);
	fflush(fp2);
	// アクセスログの生成
	access_log(0,n_room,name,host,addr);

	RoomUnLock();
}

/*
 * チャット終了処理
 */
void Bye()
{
FILE	*fp;
char	**items;
char	**lines;
char	uname[HANDLE_MAX+1],others[256];
char	*buff;
char	**Lines;
int		i,maxline;

	buff = read_file(mes_file);
	lines = split_by_char('\n', buff, 2);
	items = split_by_char('\t', lines[n_num], 4);
	safe_strcpy(uname,items[0],HANDLE_MAX);
	free(items);
	free(lines);
	free(buff);

	fp = fopen(chat_file,"a");
	if (strcmp(cmd,"bye") == 0)
	{	fprintf(fp,"%s\tおしらせ\t%sさんが退室しましたので待機中になります.\n",
				date_now,uname); 
		ROOM_PWD[1][0] = 0;
	} else
	{
		if (strcmp(cmd,"bye2") == 0 && ninzu < 2)
		{	fprintf(fp,"%s\tおしらせ\t現在、退室させる相手はいません.\n",
			date_now);
		}else {
			fprintf(fp,"%s\tおしらせ\t相手を退室させましたので待機中になります.\n",
			date_now);
		}
	}
	fclose(fp);

	buff = malloc(1024);
	if (buff==NULL)
	{	error("警告","部屋の閉鎖処理に異常が発生しました、管理者宛に通知してください");
	}
	fp = fopen(mes_file,"r");
	fgets(buff,1024,fp);
	fclose(fp);

	fp = fopen(mes_file,"w");
	fputs(buff,fp);
	fclose(fp);

	fp = fopen(entry_file,"r");
	fgets(buff,1024,fp);
	fclose(fp);

	fp = fopen(entry_file,"w");
	buff[0] = '0';
	fputs(buff,fp);
	fclose(fp);
	free(buff);
	
	if (strcmp(cmd,"bye")==0)
	{
		// アクセスログの生成
		access_log(1,n_room,uname, getenv("REMOTE_HOST"), getenv("REMOTE_ADDR"));
		error("退室","ご利用ありがとうございました."); 
	} else {
		if (strcmp(cmd,"bye2")==0)
		{
			// アクセスログの生成
			access_log(1,n_room,"相手", NULL, NULL);
		} else {
			error("異常終了","入り口から入り直してください."); 
		}
	}
	Chat();
}

//
// PROFILE更新処理
//
void ProfUpdate()
{
FILE	*fp,*fp2;
int		i,err=0;
char	*buff;
char	msg[MSG_MAX+1];
char	**items;
char	**lines;
char	uname[MSG_MAX+HANDLE_MAX+1];
char	usex[16];
char	uhost[256];

	
	i = ALLOW_HTML; ALLOW_HTML=0;
	norm_form("mes");
	ALLOW_HTML=i;

	fp = fopen(CHECK_FILE,"r");
	
	if (fp != NULL)
	{
		buff = get_form("mes");
		while (1) {
			if (fgets(uname,MSG_MAX+HANDLE_MAX+1,fp) == NULL)
			{	break;
			}
			strtok(uname,"\r\n\t");
			if (!uname[0])
			{	break;
			}
			if (strstr(buff,uname) != NULL)
			{	err = -1;
				break;
			}
		}
		fclose(fp);
	}
	buff = get_form("mes");
	safe_strcpy(msg,buff,MSG_MAX);
	buff = read_file(mes_file);
	if (buff != NULL && err == 0) 
	{
		lines = split_by_char('\n', buff, 2);
		items = split_by_char('\t', lines[0], 4);
		safe_strcpy(uname,items[0],HANDLE_MAX);
		usex[0] = *items[1];	usex[1] = 0;
		safe_strcpy(uhost,items[3],61);

		sprintf(lock_file,"%s%s.lock",BASE_DIR,room);
		RoomLock();		// 部屋ファイルのロック
	
	
		fp = fopen(mes_file,WRITE_MODE);
		fprintf(fp,"%s\t%c\t%s\t%s\n",
			uname,usex[0],msg,uhost);
		if (lines[1][0] != 0)
		{
			fprintf(fp,"%s\n",lines[1]);
		}
		fclose(fp);
		
		fp = fopen(chat_file,"a");
		fprintf(fp,"%s\t%sさんのプロフィール\t%s\n",date_now,uname,msg); 
		fclose(fp);
		free(items);
		free(lines);
		free(buff);
		RoomUnLock();
	}
}

/*
 * 部屋の状態取得（経過時間及び部屋の状態（人数）
 */
int GetData(char *filename)
{
char	buff[64];
FILE	*fp;
int	i,nn;

	for (i = 0;i < 2;++i)
	{	
		ROOM_PWD[i][0] = 0;
	}

	fp = fopen(filename,"r");
	if (fp == NULL)
	{	return -1;
	}
	
	nn = 0;
	for (i = 0;i < 2;++i)
	{	if (fgets(buff,256,fp) == NULL)
		{	break;
		}
		if (buff[0] == 0)
		{	break;
		}
		strtok(buff,"\r\n");
		strcpy(&ROOM_PWD[i][0],buff);
		nn++;
	}
	fclose(fp);
	if (ROOM_PWD[0][0]!=0) 
	{	room_flg = ROOM_PWD[0][0]-'0';
	} else
	{	room_flg = 0;
	}
	return nn;
}

/*
 * 部屋の一覧表示
 */
void List()
{
int	i;
FILE	*fp;
char	disp[256];
char	umessage[MSG_MAX+1];
char	uname[HANDLE_MAX+1];
char	usex[64];
char	uhost[128];
int		n_auto,nn,et;
char	*p;

	n_auto = atoi(get_form("auto"));
	
	puts("<!DOCTYPE html>");
	puts("<html><head>");
	puts("<title>");
	puts(TITLE);
	puts("</title>\n");
	puts("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">");
	puts("<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
	if (n_auto != 0) { 
		printf( "<meta http-equiv=\"refresh\" content=\"%d;URL=%s?action=List&auto=%d\">",
			n_auto,script_name,n_auto);
	}
	printf("<title></title></head>%s",BODY);
	if (n_auto > 0) { 
		printf( "〔%d秒自動更新中〕〔<a href=\"%s?action=List&auto=0\">手動更新にする</a>〕<BR>\n",
			n_auto,script_name);
	} else { 
		printf("〔<a href=\"%s?action=List&auto=0\">手動更新</a>〕〔<a href=\"%s?action=List&auto=%d\">自動更新(%d秒)にする</a>〕<BR>\n",
			script_name,script_name,RELOAD,RELOAD);
	}
	
	if (LIST_COMMENT)
		puts(LIST_COMMENT);
	puts("<p>");
	puts("<table class=\"list_table\">");
	puts("<tr>");
	puts("<th class=\"list_room_name\">部屋名</th>");
	puts("<th class=\"list_room_status\">状態</th>");
	puts("<th class=\"list_user_sex\">性別</th>");
	puts("<th class=\"list_user_name\">ユーザー名</th>");
	puts("<th class=\"list_user_profile\">プロフィール</th>");
	puts("</tr>");
	for (i = 0;i < ROOM_MAX;++i)
	{	
		ROOM_PWD[0][0] = 0;
		ROOM_PWD[1][0] = 0;
		
		sprintf(chat_file,"%s%02d.log",BASE_DIR,i+1);
		sprintf(entry_file,"%s%02d.ent",BASE_DIR,i+1);
		sprintf(mes_file,"%s%02d.mes",BASE_DIR,i+1);
		sprintf(lock_file,"%s%02d.lock",BASE_DIR,i+1);

		et = ptime(chat_file);
		if (et > LIMIT)
		{	Close(TIMEOUT_CLOSE);
		}
		nn = GetData(entry_file);

		switch (nn)
		{
		case 0:											/* 空室 */
			sprintf(disp,"<span style=\"color:%s\">空室</span>",COLOR3);
			strcpy(uname,"&nbsp;");
			strcpy(usex,"&nbsp;");
			strcpy(umessage,"&nbsp;");
			strcpy(uhost,"&nbsp;");
			break;
		case 1:
			{
				char *m,**items;
				char	buff[1024];
				
				fp = fopen(mes_file,"rb");
				fgets(buff,1024,fp);
				fclose(fp);
				items = split_by_char('\t', buff, 4);
				safe_strcpy(uname,items[0],HANDLE_MAX);
				usex[0] = *items[1];
				safe_strcpy(umessage,items[2],MSG_MAX);
				safe_strcpy(uhost,items[3],64);
				free(items);
				switch (usex[0])
				{
				case 'M':
					sprintf(usex,"<span style=\"color:%s\">男</span>",COLOR1); 
					break;
				case 'F':
					sprintf(usex,"<span style=\"color:%s\">女</span>",COLOR2); 
					break;
				default:
					sprintf(usex,"<span style=\"color:%s\">？</span>",COLOR1); 
					break;
				}
				m = malloc(strlen(uhost)+1);
				strcpy(m,uhost);
				switch (VIEW_HOST)
				{
				case 1:
					sprintf(uhost,"<div style=\"font-size=xsmall\">&lt;%s&gt;</div>",m);
					break;
				case 2:
					sprintf(uhost,"<!--- %s ------>",m);
					break;
				default:
					sprintf(uhost,"&nbsp;");
					break;
				}
				sprintf(disp,"<span style=\"color:%s\">待機中</span>",COLOR4);
				free(m);
			}
			break;
		case 2:
			sprintf(disp,"<span style=\"color:%s\">満室</span>",COLOR5);
			strcpy(uname,"&nbsp;");
			strcpy(usex,"&nbsp;");
			strcpy(umessage,"&nbsp;");
			strcpy(uhost,"&nbsp;");
			break;
		default:
			sprintf(disp,"異常");
			strcpy(uname,"&nbsp;");
			strcpy(usex,"&nbsp;");
			strcpy(umessage,"&nbsp;");
			strcpy(uhost,"&nbsp;");
			break;
		}
		printf("<tr><td>%s</td><td>",RoomTable[i]);
		printf("<center>%s</center></td><td align=center>%s</td>",disp,usex);
		printf("<td>%s</td><td>%s %s</td></tr>",uname,umessage,uhost);
	}
	// # 著作表示の削除禁止
	puts("</tr></table>");
	printf("<p>〔<a href=\"%s\" target=\"_top\">%s</a>〕<p>",
		MODORU,MODORU_NAME);
	puts("<div align=right><a href=\"http://www.rescue.ne.jp/\" target=\"_blank\">");
	printf("<img src=\"%s?img=copyright\" border=0 alt=\"2SHOT-CHAT v4.0-C\"></a><BR>",script_name);
	printf("Arranged&nbsp; by&nbsp; <a href=\"http://www.petit-power.com/\" target=_blank>PETIT-POWER %s</A></div></body></html>",REL_VERSION);
    myexit(0);
}


/*
 * 入室フレーム画面の表示
 */
void Form()
{
char	uname[HANDLE_MAX+1];
char	usex[16];
char	umes[MSG_MAX+1];
char	body[BODY_MAX+1];
char	focus[256];
char	check1[256];
char	check2[256];
int	i;

	
	i = base64_decode(get_cookie("name"),uname,HANDLE_MAX-1);
	uname[i]=0;
	safe_strcpy(usex,get_cookie("sex"),2);
	i = base64_decode(get_cookie("mes"),umes,HANDLE_MAX-1);
	umes[i]=0;
	if (!uname[0]) { 
		strcpy(focus," onLoad=\"document.InputForm.chat_name.focus();\">");
	} else
	{	strcpy(focus," onLoad=\"document.InputForm.mes.focus();\">");
	}
	
	strcpy(body,BODY);
	body[strlen(body)-1] = 0;
	strcat(body,focus);

	check1[0]=check2[0]=0;
	if (usex[0] == 'F') { 
		strcpy(check2,"checked"); 
	} else { 
		strcpy(check1, "checked");
		usex[0] = 'M';
	}
	usex[1] = 0;
	puts("<!DOCTYPE html>");
	puts("<html><head>");
	puts("<title>");
	puts(TITLE);
	puts("</title>\n");
	puts("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">");
	puts("<script language=\"javascript\">");
	puts("<!--");
	puts("function PageBack(){ history.back(); }");
	puts("//-->");
	puts("</script>");
	puts("<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
	puts("</head>");
	puts(body);
	puts("<h1>");
	puts(TITLE);
	puts("</h1>");
	printf("<form method=POST action=\"%s\" target=\"_top\" name=\"InputForm\">\n"
	"<input type=hidden name=\"action\" value=\"entry\">\n"
	"チャット名 <input type=text name=\"chat_name\" value=\"%s\" size=10 maxlength=%d>\n"
	"<input type=radio name=\"sex\" value=\"M\" %s><span style=\"color:%s\">男</span>\n"
	"<input type=radio name=\"sex\" value=\"F\" %s><span style=\"color:%s\">女</span>\n"
	"<select name=\"room\" size=1>\n"
	"<option value=\"\" selected>選択してください</option>",
	script_name,uname,HANDLE_MAX,check1,COLOR1,check2,COLOR2);
	for (i = 0;i < ROOM_MAX;++i)
	{	printf( "<option value=\"%02d\">%s</option>\n",i+1,RoomTable[i]);
	}
	printf("</select>"
		"<input type=submit value=\"入室\">"
		"<input type=checkbox name=\"cookie\" value=\"1\" checked>保存<br />"
		"待機用プロフィール <input type=text name=\"mes\" size=50 maxlength=%d value=\"%s\">"
		"</form>"
		"</body></html>",MSG_MAX,umes);
}

/*
 * SSI用部屋の状態表示
 */
void SSI_View()
{
int	i;
FILE	*fp,*lfp;
int	err_room_cnt,full_room_cnt,single_room_cnt,free_room_cnt;


	err_room_cnt=full_room_cnt=single_room_cnt=free_room_cnt=0;
	for (i = 0;i < ROOM_MAX;++i)
	{	
		ROOM_PWD[0][0] = 0;
		ROOM_PWD[1][0] = 0;
		
		sprintf(chat_file,"%s%02d.log",BASE_DIR,i+1);
		sprintf(entry_file,"%s%02d.ent",BASE_DIR,i+1);
		sprintf(mes_file,"%s%02d.mes",BASE_DIR,i+1);
		sprintf(lock_file,"%s%02d.lock",BASE_DIR,i+1);
		ninzu = GetData(entry_file);
		switch (ninzu)
		{
		case 0:
			free_room_cnt++;
			break;
		case 1:
			single_room_cnt++;
			break;
		case 2:
			full_room_cnt++;
			break;
		}
	}
	puts("Content-type: text/html;charset=utf-8\n\n");
	printf("<span style=\"color:%s\">空き:%d</span>&nbsp;span style=\"color=%s\">待機中:%d</span>&nbsp;span style=\"color=%s\">満室:%d</span>\n"
			,COLOR3,free_room_cnt,COLOR4,single_room_cnt,COLOR5,full_room_cnt);
	myexit(0);
}


//
// アクセスログの記録
//
void access_log(int type,int room,char *Name,char *Host,char *Addr)
{
char	fname[256];
FILE	*fp;
struct tm *gmt;time_t ti;

	if (!access_log_flg)
	{	return;
	}
	ti = time(NULL);
	ti += 9*60*60;
	gmt = gmtime(&ti);
	sprintf(fname,"%s%04d%02d.log",BASE_DIR,gmt->tm_year+1900,gmt->tm_mon+1);
	fp = fopen(fname,"a+");
	
	fprintf(fp,"%04d/%02d/%02d %02d:%02d:%02d,",gmt->tm_year+1900,gmt->tm_mon+1,
			gmt->tm_mday,gmt->tm_hour,gmt->tm_min,gmt->tm_sec);
	if (type)
	{	fprintf(fp,"%02d,退室,",room);
	} else
	{	fprintf(fp,"%02d,入室,",room);
	}
	fprintf(fp,"%s, ",Name);
	if (Host != NULL)
	{	fprintf(fp,"%s,",Host);
	} else
	{	fprintf(fp,"---,");
	}
	if (Addr != NULL)
	{	fprintf(fp,"%s\n",Addr);
	} else
	{	fprintf(fp,"---\n");
	}
	fclose(fp);
}

//
// 発言クリア
//
void Clear()
{
	FILE	*fp ;

	sprintf(lock_file,"%s%s.lock",BASE_DIR,room);
	RoomLock();		// 部屋ファイルのロック
	
	if ((fp = fopen(chat_file,WRITE_MODE))== NULL)
	{ error("エラー","チャットファイルへ記録できません.");
	} else
	{ fprintf(fp,"%s\tおしらせ\tオーナーによって画面がクリアされました.\n",date_now);
	}
	fclose(fp);
	RoomUnLock();		// 部屋ファイルのロック
}

//
// クローズ処理
//
void Close(int stat)
{
FILE	*fp;
char	buff[256],*pp;
int	lock_flg=0;	
	
	if (!lock_file[0])
	{	sprintf(lock_file,"%s%s.lock",BASE_DIR,room);
		RoomLock();		// 部屋ファイルのロック
		lock_flg = 1;
	}
	// アクセスログの生成
	// access_log(0,n_room,"空室",NULL,NULL);
	fp = fopen(chat_file,WRITE_MODE);
	fclose(fp);
	fp = fopen(mes_file,WRITE_MODE);
	fclose(fp);
	fp = fopen(entry_file,WRITE_MODE);
	fclose(fp);

	pp =get_form("num");
	if (*pp != 0)
	{
		switch (stat)
		{
		case OWNER_CLOSE:
			error("閉鎖しました","あなたによってチャットルームが閉鎖されました.");
			break;
		case TIMEOUT_CLOSE:
			sprintf(buff,"無会話時間が%d秒を超えましたので、チャットルームは自動的に閉鎖されました.",LIMIT); 
			error("閉鎖しました",buff);
			break;
		case ADMIN_CLOSE:
			error("閉鎖しました","管理者より強制閉鎖致しました");
			break;
		default:
			sprintf(buff,"何らかの理由によりチャットルームが閉鎖されました（コード:%02d)",stat); 
			error("閉鎖しました",buff);
			break;
		}
	}
	if (lock_flg)
	{	RoomUnLock();
	}
}

//
// エラー表示
//
void error(char *hd,char *msg)
{

	if (strcmp(cmd,"entry") == 0)
	{	puts("Content-type: text/html;charset=utf-8\n\n"); 
	}
	
	puts("<!DOCTYPE html>");
	printf("<html><head><title>%s</title>",TITLE);
	puts("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">");
	puts("<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">");
	puts("<script language=\"javascript\">");
	puts("<!--");
	puts("function PageBack(){ history.back(); }");
	puts("//-->");
	puts("</script>");
	printf("</head>\n%s<h1 class=\"error\">%s</h1>",BODY,hd );
	printf("<ul>\n"
		"<li>%s</li>\n",msg);
	printf("<p><li>〔<a href=\"%s\" target=\"_top\">空室状況</a>〕<li>〔<a href=\"%s\" target=\"_top\">%s</a>〕<li>〔<A HREF=\"JavaScript:history.back()\">直前の画面</A>〕",
		script_name,MODORU,MODORU_NAME);
	puts("</ul></body></html>");
	myexit(0);
}

/* エラーページを出力する */
void printerror(char *msg1, char *msg2)
{
    printf("Content-type: text/html;charset=utf-8\\n");
    printf("\n");
    printf("<html><head><title>%s : %s</title></head>\n", msg1, msg2);
    printf("%s\n",BODY_NOTICE);
    printf("<h1>%s : %s</h1>\n", msg1, msg2);
    printf("</body>\n");
    printf("</html>\n");
    myexit(0);
}


/*
 * $RCSfile: 2shot.c,v $
 * $Revision: 1.11 $
 *
 * Copyright (C) 1999 by Hiroshi Yuki <hyuki@st.rim.or.jp>.
 * http://www.st.rim.or.jp/~hyuki/
 * All rights reserved.
 */
/*
# ■利用許諾条件
# 個人・会社を問わず、また商用・非商用を問わず、
# このプログラムを改造して使用してかまいません。無料です。
# ただし、
# ・著作権表示およびこの利用許諾条件を削除・改変しないでください。
# ・表示ページの最下部に表示されるリンクを削除しないでください。
# ・プログラム自体を商品として販売しないでください。
*/

/* HTMLとして正しく表示できるようにする */
void make_quote(char buf[], int len)
{
    char *tmpbuf = (char *)malloc(len);
    int srci = 0;
    int dsti = 0;

    for (srci = 0; buf[srci]; srci++) {
        switch (buf[srci]) {
        case '&':
            strcpy(tmpbuf + dsti, "&amp;");
            dsti += strlen("&amp;");
            break;
        case '"':
            strcpy(tmpbuf + dsti, "&quot;");
            dsti += strlen("&quot;");
            break;
        case '<':
            strcpy(tmpbuf + dsti, "&lt;");
            dsti += strlen("&lt;");
            break;
        case '>':
            strcpy(tmpbuf + dsti, "&gt;");
            dsti += strlen("&gt;");
            break;
        case ',':
            strcpy(tmpbuf + dsti, "&#44;");
            dsti += strlen("&#44;");
            break;
        default:
            tmpbuf[dsti] = buf[srci];
            dsti++;
        }
    }
    tmpbuf[dsti] = '\0';
    safe_strcpy(buf, tmpbuf, len);
    if (tmpbuf) free(tmpbuf);
}

/* ファイル全体を読む */
char *read_file(char *filename)
{
    char *buffer = 0;
    size_t size = 0;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
#if 0
        char msg[1024];
        sprintf(msg,  "%sがオープンできません。", filename);
        printerror("エラー", msg);
#endif
	return NULL;
    }
    /* ロック */
    
    /* 読み込み */
    size = get_file_size(fp);
    buffer = malloc(size + 1);
    if (buffer == NULL) {
        printerror("エラー", "メモリ不足です。");
        myexit(0);
    }
    if (size)
	fread(buffer, 1, size, fp);
    buffer[size] = '\0';
    fclose(fp);
    return buffer;
}


/* 文字の部分に '\0' を詰め、文字列の配列化をする */
/* 要素がexpected_itemsより小さいとき、残りの要素は "" を埋め草とする */
char **split_by_char(char splitchar, char *str, int expected_items)
{
    /* 一度目のスキャンで要素数を得る */
    int count = 0;
    int allocsize = 0;
    int i, index;
    char **array = 0;

    for (i = 0; str[i]; i++) {
        if (str[i] == splitchar) {
            count++;
        }
    }
    count++;
    if (count < expected_items) {
        allocsize = expected_items + 1;
    } else {
        allocsize = count + 1;
    }
    /* 要素確保 */
    array = malloc(sizeof(char *) * allocsize);
    /* 設定 */
    index = 0;
    array[index++] = &str[0];
    for (i = 0; str[i]; i++) {
        if (str[i] == splitchar) {
            str[i] = '\0';
            if (splitchar == 0x0d && str[i+1] == 0x0a)
            {	i++;
            }
			if (str[i+1])
				array[index++] = &str[i+1];
        }
    }
    /* 埋め草 */
    while (index < expected_items) {
        array[index++] = "";
    }
    array[index] = 0;
    return array;
}

/* ROOMロックファイル */
void RoomLock()
{
int	lp,locfd;

	lp = 0;
	
	while (1)
	{
		if (mkdir(lock_file,0755)==0) 
		{	break;
		}
			
        if (++lp == RETRY_CNT-1)
        {
            error("重大","部屋の状態がおかしいです、管理人宛に連絡してください");
			myexit(1);
        }
         sleep(1);
    }

}
/* ROOMロックファイル */
void RoomUnLock()
{
	rmdir(lock_file);
}

void sigexit(int sig)
{
    // ロックファイル削除
	if (lock_file[0])
	{    remove(lock_file);      
	}
    exit(1);
}


/* ファイルのロック */
void lockfile(FILE *fp)
{
#if USELOCK
int holds_lock;
int file_descriptor = fileno(fp);

	for(holds_lock=0;!holds_lock;usleep(LOCK_INTERVAL_MICROSECS))
	{
#if HAVE_FLOCK
	  if (flock(file_descriptor,LOCK_EX | LOCK_NB) == 0) { 
#else
	  if (lockf(file_descriptor,F_TLOCK,0) == 0) {
#endif
		  holds_lock = 1;
		  break;
	  }
	}
#endif

}

/* ファイルのアンロック */
void unlockfile(FILE *fp)
{
#if USELOCK
#if HAVE_FLOCK
	flock(fileno(fp), LOCK_UN);
#else
    lockf( fileno(fp), F_ULOCK, 0 ); 
#endif
#endif
}


/* ファイルサイズを得る */
size_t get_file_size(FILE *fp)
{
    size_t size;
    fseek(fp, 0, 2);
    size = ftell(fp);
    fseek(fp, 0, 0);
    return size;
}

/* 安全なstrcat */
void safe_strcat(char *dst, char *src, int len)
{
char*  dst_over = dst + len+1;

   dst = dst+strlen(dst);
	while ( dst < dst_over )
	{
		if ( *src == '\0' )
		{
			*dst = '\0'; 
			return;
		}
		*dst++ = *src++;
	}
	*dst = '\0';

}

/* 安全なstrcpy */
void safe_strcpy(char *dst, char *src, int len)
{
    int  src_len = strlen( src );
 
 	if ( src_len >= len)
	{
		if ( len > 0 )
		{
			memcpy( dst, src, len  );
			dst[len] = '\0';
 		}
	} else 
	{
 		strcpy( dst, src );
	}
}

/* 安全なstricmp */
int safe_stricmp(char *dst, char *src)
{
	char	cc;
	
	do
	{
		cc = *dst;
		if (toupper(cc) != *src)
		{	return -1;
		}
		dst++; src++;
	} while (*dst != 0 && *src != 0);
	return 0;
}

/* フォームからの情報を配列 form に入れる */
void init_form(char *kanjicode)
{
    int length = 0;
    char *method = getenv("REQUEST_METHOD");
    char *query = 0;

    /* queryにメモリを確保し、フォームからの情報を設定する */
    if (method && strcmp(method, "POST") == 0) {
        char *content_length_string = getenv("CONTENT_LENGTH");
        if (content_length_string == NULL) return;
        length = atoi(content_length_string);
        query = malloc(length + 1);
        if (!query) return;
        fread(query, 1, length, stdin);
        query[length] = '\0';
    } else {
        char *query_string = getenv("QUERY_STRING");
        if (query_string == NULL) return;
        length = strlen(query_string);
        query = malloc(length + 1);
        if (!query) return;
        strcpy(query, query_string);
    }
    {
        int i;
        char **assocarray = split_by_char('&', query, 0);
        for (i = 0; assocarray[i]; i++) {
            char **property_value = split_by_char('=', assocarray[i], 2);
            char *property = property_value[0];
            char *value = property_value[1];
            char *converted_value;
            tr(value, '+', ' ');
            decode_percent_hex(value);
			set_form(property,value);
			}
        free(assocarray);
    }
    free(query);
}



/* 連想配列もどき */
typedef struct _assoc_t {
    char *key;
    char *value;
} assoc_t;

#define MAX_FORM 100

static assoc_t form[MAX_FORM];
static int form_index = 0;

/* key, value の文字列の組みを form にセットする */
void set_form(char *key, char *value)
{
    int i;

    for (i = 0; i < form_index; i++) {
        if (strcmp(form[i].key, key) == 0) {
            /* 上書き */
            free(form[i].value);
            form[i].value = malloc(strlen(value) + 1);
            strcpy(form[i].value, value);
            return;
        }
    }
    /* 追加 */
    if (form_index < MAX_FORM) {
        form[form_index].key = malloc(strlen(key) + 1);
        strcpy(form[form_index].key, key);
        form[form_index].value = malloc(strlen(value) + 1);
        strcpy(form[form_index].value, value);
        form_index++;
    }
}

/* key に対応した文字列を得る。なければ""を返す */
char *get_form(char *key)
{
    int i;
    
    for (i = 0; i < form_index; i++) {
        if (strcmp(form[i].key, key) == 0) {
            return(form[i].value);
        }
    }
    return "";
}

/* formの解放 */
void exit_form(void)
{
    int i;
    
    for (i = 0; i < form_index; i++) {
        free(form[i].key);
        free(form[i].value);
    }
    form_index = 0;
}

/* メモリ解放して終了 */
void myexit(int x)
{
    exit_form();
    exit_cookie();
    if (RoomTable !=NULL)
    {	free(RoomTable[0]);
	free(RoomTable);
    }
    if (LIST_COMMENT)
    	free(LIST_COMMENT);
	if (lock_file[0])
	{	remove(lock_file);
		lock_file[0] = 0;
	}
    exit(x);
}

/* 使用できない文字の置換 */
void norm_form(char *key)
{
    int i,tag_flg = 0;
    int di = 0;
    char *value = get_form(key);
    int allocsize = strlen(value);
    char *newvalue = (char *)malloc(allocsize + 1);
	char	*p;
	
    strcpy(newvalue, value);
    for (i = 0; value[i]; i++) {
        switch (value[i]) {
        case '\"':
            if (tag_flg)
            {	newvalue = append_char(newvalue, &allocsize, &di, value[i]);
            } else
            {	newvalue = append_string(newvalue, &allocsize, &di, "&quot;");
            }
            break;
        case '&':
            if (tag_flg)
            {	newvalue = append_char(newvalue, &allocsize, &di, value[i]);
            } else
            {	newvalue = append_string(newvalue, &allocsize, &di, "&amp;");
            }
            break;
        case '<':
            if (ALLOW_HTML)
            {
	   			if (Check_Tag(&value[i],"<FONT") == 0 ||
            		Check_Tag(&value[i],"<SMALL>") == 0 ||
            		Check_Tag(&value[i],"<BIG>") == 0 ||
            		Check_Tag(&value[i],"<BLINK>") == 0 ||
            		Check_Tag(&value[i],"<SPAN") == 0 ||
            		Check_Tag(&value[i],"<DIV") == 0 ||
            		Check_Tag(&value[i],"<I>") == 0 ||
            		Check_Tag(&value[i],"<B>") == 0 ||
            		Check_Tag(&value[i],"<U>") == 0 ||
            		Check_Tag(&value[i],"</FONT>") == 0 ||
            		Check_Tag(&value[i],"</SMALL>") == 0 ||
            		Check_Tag(&value[i],"</BIG>") == 0 ||
            		Check_Tag(&value[i],"</BLINK>") == 0 ||
            		Check_Tag(&value[i],"</SPAN>") == 0 ||
            		Check_Tag(&value[i],"</DIV>") == 0 ||
            		Check_Tag(&value[i],"</U>") == 0 ||
            		Check_Tag(&value[i],"</I>") == 0 ||
            		Check_Tag(&value[i],"</B>") == 0)
            	{	
            		if (value[i+1] != '/')		/* 閉じミスがないかを簡易チェックする */
            		{	p = &value[i+1];
            			p = strchr(p,'<');
            			if (p == NULL || strchr(p,'>') == NULL)
            			{	
			            newvalue = append_string(newvalue, &allocsize, &di, "&lt;");
			            break;
				     }
				}
            	tag_flg = 1;
		        newvalue = append_char(newvalue, &allocsize, &di, value[i]);
	        	break;
            	}
            }
            newvalue = append_string(newvalue, &allocsize, &di, "&lt;");
            break;
        case '>':
            if (tag_flg)
            {	tag_flg = 0;
		        newvalue = append_char(newvalue, &allocsize, &di, value[i]);
	   		} else
	    	{     newvalue = append_string(newvalue, &allocsize, &di, "&gt;");
            }
            break;
        case ',':
            if (tag_flg)
            {	newvalue = append_char(newvalue, &allocsize, &di, value[i]);
            } else
            {	newvalue = append_string(newvalue, &allocsize, &di, "&#44;");
	    	}
            break;
        case '\r':
        case '\n':
            newvalue = append_string(newvalue, &allocsize, &di, "<br> ");
            if (strncmp(&value[i], "\r\n", 2) == 0) {
                i++;    /* 他より1文字余計に使うので */
            }
            break;
        default:
            newvalue = append_char(newvalue, &allocsize, &di, value[i]);
            break;
        }
    }
//    if (tag_flg)
//    {
		
//    }
    
    newvalue[di] = '\0';
    set_form(key, newvalue);
    free(newvalue);
}

/*
 * 設定ファイルの読み込み
 */
int read_config()
{
FILE	*fp;
int	item = -1,i;
int	idx = 0;
char	buff[COMMENT_MAX+1];
char	*lines;

	NOTICE_MSG[0] = FULL_MSG[0] = 0;
	fp = fopen(CONFIG_FILE,"r");
	if (fp == NULL)
	{	return -1;
	}
	do {
		if (fgets(buff,COMMENT_MAX,fp) == NULL)
		{	break;
		}
		if (buff[0] == ';')
		{	continue;
		}
		if (buff[0] == '%')
		{	if (buff[1] >= 'A')
			{	item = buff[1]-'A'+10;
			} else
			{	item = buff[1] - '0';
			}
			idx =0;
			continue;
		}
		strtok(buff,"\r\n");
		switch (item)
		{
		case 0:		/* ベースディレクトリ */
			strcpy(BASE_DIR,buff);
			break;
		case 1:		/* 管理者名 */
			strcpy(ADMIN_NAME,buff);
			break;
		case 2:		/* 管理者パスワード */
			strcpy(ADMIN_PASS,buff);
			break;
		case 3:		/* アクセスログ記録の有無　０：なし　１：あり */
			access_log_flg = atoi(buff);
			break;
		case 4:		/*　タイトル */
			safe_strcpy(TITLE,buff,64);
			break;
		case 5:		/* BODY */
			safe_strcpy(BODY,buff,BODY_MAX);
			break;
		case 6:		/* 警告開始時間　*/
			NOTICE=atoi(buff);
			break;
		case 7:		/* 警告時のBODY設定 */
			strcpy(BODY_NOTICE,buff);
			break;
		case 8:		/* 警告時のアラームメッセージ */
			strcpy(NOTICE_MSG,buff);
			break;
		case 9:		/* 満室時のオーナーメッセージ */
			strcpy(FULL_MSG,buff);
			break;
		case 10:		/* リスト表示時のコメント */
			if (!idx)
			{	LIST_COMMENT = malloc(COMMENT_MAX+1);
				LIST_COMMENT[0] = 0;
			} 
			safe_strcat(LIST_COMMENT,buff,COMMENT_MAX);
			safe_strcat(LIST_COMMENT,"\n",COMMENT_MAX);
			idx++;
			break;
		case 11:		/*　チャット画面の上段コメント */
			if (!idx)
			{	UPPER_COMMENT = malloc(1024);
				UPPER_COMMENT[0] = 0;
				UPPER_TYPE = buff[0] == 'U' ? 0:1;
			} else {
					safe_strcat(UPPER_COMMENT,buff,COMMENT_MAX);
			}
			idx++;
			break;
		case 12:		/*　チャット画面の下段コメント */
			if (!idx)
			{	LOWER_COMMENT = malloc(1024);
				LOWER_COMMENT[0] = 0;
				LOWER_TYPE = buff[0] == 'U' ? 0:1;
			} else {
					safe_strcat(LOWER_COMMENT,buff,COMMENT_MAX);
			}
			idx++;
			break;
		case 13:		/* 男の色　*/
			strcpy(COLOR1,buff);
			break;
		case 14:		/* 女性の色　*/
			strcpy(COLOR2,buff);
			break;
		case 15:		/* 空室の色　*/
			strcpy(COLOR3,buff);
			break;
		case 16:		/* 待機中の色　*/
			strcpy(COLOR4,buff);
			break;
		case 17:		/* 満室の色　*/
			strcpy(COLOR5,buff);
			break;
		case 18:		/* 自分の色　*/
			strcpy(COLOR6,buff);
			break;
		case 19:		/* 部屋数 */
			ROOM_MAX = atoi(buff);
			RoomTable = malloc(sizeof(char *)*ROOM_MAX);
			RoomTable[0] = malloc((ROOM_MAX+1)*64);
			for (i = 1;i < ROOM_MAX;++i)
			{	RoomTable[i] = RoomTable[0]+i*64;
			}
			break;
		case 20:		/* 表示行数 */
			MAX = atoi(buff);
			break;
		case 21:		/* リスト表示の自動更新 */
			RELOAD = atoi(buff);
			break;
		case 22:		/* 無言制限 */
			LIMIT = atoi(buff);
			break;
		case 23:		/* 自動更新 */
			dauto = atoi(buff);
			break;
		case 24:		/* HOSTの表示 */
			VIEW_HOST = atoi(buff);
			break;
		case 25:		/* タグの有効・無効 */
			ALLOW_HTML = atoi(buff);
			break;
		case 26:		/* 満室直後のデータクリア */
			flgLogClear = atoi(buff);
			break;
		case 27:		/* 女性専用の部屋開始番号　*/
			female_room=atoi(buff);
			break;
		case 28:		/* URLの自動リンク機能　*/
			auto_url=atoi(buff);
			break;
		case 29:		/* 戻るURL */
			strcpy(MODORU,buff);
			break;
		case 30:		/* 戻るメッセージ */
			strcpy(MODORU_NAME,buff);
			break;
		case 31:		/* 部屋名 */
			strcpy(RoomTable[idx++],buff);
			if (idx >= ROOM_MAX)
			{	item = 31;
			}
			break;
		}
	} while(feof(fp) == 0);
	fclose(fp);
	return 0;
}
void copyright()
{

static unsigned char icon[] = {
	0x47,0x49,0x46,0x38,0x39,0x61,0x2b,0x00,0x23,0x00,0xb3,0x02,0x00,0x00,0x00,0x00,0x84,0x84,0x84,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0x21,0xf9,0x04,0x01,0x00,0x00,0x02,0x00,0x2c,0x00,0x00,0x00,0x00,0x2b,0x00,0x23,0x00,0x40,0x04,
	0xc3,0x50,0xc8,0x49,0xab,0xbd,0x38,0x6b,0x00,0x42,0x98,0x9f,0x06,0x7a,0xa4,0x67,0x01,0x15,0xd7,0x7d,
	0x6a,0xcb,0x76,0x13,0xf7,0x9a,0xe2,0x26,0x4b,0x65,0x59,0xef,0xb5,0x1c,0xf2,0x98,0x9b,0x04,0x05,0x14,
	0xa9,0x68,0x2a,0x41,0xa7,0xa5,0x4c,0x0e,0x8f,0x21,0x61,0xac,0x45,0x44,0x39,0x99,0xcc,0x27,0xa7,0xd8,
	0x8b,0xae,0x9e,0x3f,0xee,0x25,0x09,0x13,0x17,0xa5,0xe6,0x74,0x76,0x57,0x85,0x11,0x95,0xd3,0x19,0x0b,
	0x4c,0x97,0xde,0xd0,0x31,0xdd,0xf7,0x5e,0xe6,0x23,0xdf,0x69,0x27,0x61,0x81,0x3c,0x2b,0x32,0x6d,0x73,
	0x84,0x14,0x57,0x8c,0x83,0x8a,0x8f,0x66,0x24,0x02,0x8e,0x90,0x29,0x88,0x95,0x19,0x78,0x98,0x41,0x65,
	0x9b,0x46,0x2b,0x01,0x80,0x36,0x54,0x9d,0x38,0x34,0xa6,0x1f,0xa9,0x92,0x3a,0xa8,0x3f,0x9a,0x54,0x33,
	0x4d,0xb1,0xb2,0x4f,0x60,0xb3,0x63,0x6d,0xa0,0xb6,0xa1,0x6e,0x9d,0x7e,0x41,0x15,0x2f,0xbd,0x5e,0xb7,
	0x87,0x4e,0x8b,0x2e,0x4d,0x93,0x5b,0xb2,0x72,0xcd,0xcc,0x9e,0x94,0x9e,0xc8,0xa7,0xd3,0x29,0xbc,0x1e,
	0xa4,0x9a,0x84,0xb0,0x71,0xd3,0x5f,0xcd,0xb4,0xd6,0x9c,0xd2,0xd6,0x6b,0x1a,0x11,0x00,0x3b};

	puts("Content-Type: image/gif\n");

	fwrite( icon, sizeof(icon), 1, stdout );
	myexit(0);

}

/* パスワードの暗号化 */
 #define TOKENS (10+26+26)
void encode_pass(char *pass, char encpass[], int addval)
{
    time_t now;
    struct tm *tmnow;
    static char token[TOKENS] = {
        '0','1','2','3','4','5','6','7','8','9',
        'A','B','C','D','E','F','G','H','I','J','K','L','M',
        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
        'a','b','c','d','e','f','g','h','i','j','k','l','m',
        'n','o','p','q','r','s','t','u','v','w','x','y','z',
    };
    char salt1, salt2;
    char salt[3];

    time(&now);
    tmnow = localtime(&now);
    salt1 = token[((unsigned int)(now / TOKENS + addval)) % TOKENS];
    salt2 = token[(now + addval) % TOKENS];

    salt[0] = salt1;
    salt[1] = salt2;
    salt[2] = '\0';
    safe_strcpy(encpass, mycrypt(pass, salt),BUFSIZ);
}

/* crypting using MD5 */
char *mycrypt(char *str, char *salt)
{
    int i;
   // struct MD5Context context;
    MD5_CTX context;
    unsigned char digest[16];
    static char encoded[256];
    
    MD5Init(&context);
    MD5Update(&context, str, strlen(str));
    MD5Update(&context, salt, 2);
    MD5Final(digest, &context);

    encoded[0] = salt[0];
    encoded[1] = salt[1];
    for (i = 0; i < 16; i++) {
        sprintf(encoded + 2 + i * 2, "%02X", digest[i]);
    }
    return encoded;
}

/* 
 * Matching Check For Tag
 */
int	Check_Tag(char *buf, char *pat)
{
	char	*sbuf;
	int	i,len;
	
	len = strlen(pat);
	sbuf = malloc(len+1);
	for (i = 0;i < len && buf[i] != 0;++i)
	{
		sbuf[i] = toupper(buf[i]);
	}
	sbuf[i] = 0;
	
	i = strncmp(sbuf,pat,len);
	
	free(sbuf);
	return i;
	
}

//
// ファイル更新秒の取り出し
//

int ptime(char *filename)
{
struct stat statbuf;
time_t	ntm;

	stat(filename,&statbuf);
	ntm = time(NULL);
	return (int)(ntm - statbuf.st_ctime);
   
}

//
// FILE SIZE の読み込み
//
long GetFileSize(char *filename)
{
FILE	*fp;
long	fsz;

	fp = fopen(filename,"rb");
	if (fp == NULL)
	{	return -1;
	}
	fseek(fp,0,SEEK_END);
	fsz = ftell(fp);
	fclose(fp);
	return fsz;
}

void DebugPrint(char *text)
{
  printf("Content-type: text/html\n\n");
  printf("%s",text);
}
