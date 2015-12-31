/*
 * ユーザー定義
 */

// 管理者名

// 環境設定ファイル名


#define	CONFIG_FILE	"./config.cgi"

// プロフィールでの禁止言葉のファイル指定
#define CHECK_FILE	"./check.cgi"

// プロバイダ制限ファイル
#define ACL_FILE	"./acl.cgi"

/*
○クッキーを認識する範囲と名称
#  もしあなたのホームページが http://www.foo.bar/~user/ なら、$path = '/~user/'; と設定しましょう.
#  もしあなたのホームページが http://www.foo.bar/ なら、$path = '/'; と設定しましょう.
#  詳しいことは http://www.netscape.com/newsref/std/cookie_spec.html のpathの項目をご覧ください.
*/

/* クッキー名 */
static char cookiename[] = "2shot-chat-c-v4";

/* クッキーの保存日数 */
static int cookiedays = 30;
