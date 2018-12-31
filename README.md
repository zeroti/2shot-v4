# 2shot-v4

本スクリプトは、ネットサーフレスキュー[Ｗｅｂ裏技]で公開しているスクリプト (２ショットチャットV4)をC言語版に改造して再配布しているスクリプトです。

C言語によるかなり高速な低負荷（サーバに優しい）な２ショットチャットが可能になります。

Ver4.2より文字コードはUTF-8となっております。
また、スタイルシートを用いて背景色変更や文字サイズなどを変更出来るようにしましたのでV4.1よりconfig.cgiの記述内容に違いが出ております。

※Ver4.2aになりました！！！　スタイルシートでTABLEタグ周りに対応しました。（一覧表と発言ボックス部分）

そして再配布にあたっては、再配布の条件を満たしています。

ネットサーフレスキュー[Web裏技]: http://www.rescue.ne.jp/
配布規定: http://www.rescue.ne.jp/info/arrange.html
基本的な仕組みは、オリジナルスクリプトを基本としていますが、C言語への移植と機能アップ及び構造変更がなされています。

なお、設置にあたってはご利用頂いているWebサーバ以外にC言語の知識及びコンパイル＆リンク環境が必要となります。
（WEBサーバによってはコンパイルを許可しているところもありますが、詳細はご利用のWEBサーバのサイトにてお調べください）


# 利用規定
利用にあたっては、オリジナルのスクリプトの利用規定に沿う必要があります。
利用規定 http://www.rescue.ne.jp/cgi/kitei.shtml

# サポートと免責事項
この改造スクリプトによっていかなる損害を被ったとしても当方及びネットサーフレスキュー[Ｗｅｂ裏技]は一切の責任を負いません。
この改造スクリプトに関する質問は、ネットサーフレスキュー[Ｗｅｂ裏技]にはしないでください。
本C言語版２ショットチャットV4に関しては、コメント欄もしくはE-Mailにてお問い合わせください。

# 設置方法
基本となる設置は方法はオリジナルツーショットチャットV4と同様です。
オリジナルと異なる部分は、以下の通りです。
オリジナルの設置方法は、こちらです。

# 実行プログラムの生成について
ダウンロードした圧縮ファイル内にはCソースファイル、インクルードファイル、Makefileと環境設定ファイルconfig.cgiが入っています。

本スクリプトの実行プログラムを生成するには、ｇｃｃ等のＣコンパイラが動作するマシン（稼動するWEBサーバと同じOS及びCPU）でなければ動作しません。

また、Ｃ言語の知識およびＵＮＩＸ・Ｌｉｎｕｘの知識を必要とします。

なお、コンパイルエラーに関しては、基本的にはサポートしませんが、エラー箇所によってはソースを改訂致します。

設置構成例
＜＞内はパーミッション値(表記の値は一般値ですのであなたのサーバでの最適値に変更してください。

<pre>
/2shot/ (ＣＧＩが実行できる任意のディレクトリ)
|– 2shot.cgi <755>
|– style.css <644> ←　スタイルシート（ファイル名は固定です）　ここで背景色や文字サイズなどを変えることが可能です。
|– config.cgi <644> ←環境設定ファイル
|– acl.cgi <644> ← 特定IP制限用のアクセス制御ファイル
| ファイル名はconfig.hで変更可能
|– check.cgi <644> ←プロフィールメッセージでの使用禁止単語等の設定
| config.hにてファイル名変更可能
|– ソースファイル一式はココに　CGI生成後は削除した方が良いでしょう♪
|–/x/ (データ格納フォルダ) <755>
|– .htaccess このフォルダにアクセスできないようにする設定
| もしこの設定が使えないサーバの場合は、データ格納フォルダを見えない場所に用意するか、
| /x/を推測しにくい名称に変えるといいでしょう。
|– 01.ent <666>
|– 01.mes <666>
|– 01.log <666>
|– 02.ent <666>
|– 02.mes <666>
|– 02.log <666>
|– 03.ent <666>
|– 03.mes <666>
|– 03.log <666>
| 以下略… 必要な分だけ用意する
</pre>

# 環境設定について
オリジナルスクリプトは、Perlで書かれていますので色やタイトル名の変更はスクリプト自体を修正することにより可能でしたが、C言語版ではバイナリプログラムになる為、スタイルシートやconfig.cgiファイルに記述するようにしました。

これが、C言語版とオリジナル版との違いです。

config.cgiファイルはテキストファイルとなっており、%の項目は修正不可になっており次の行の設定値を定めるようになっています。

本ファイルでは、部屋ファイル設置のディレクトリ名、管理者名、パスワード、タイトル・ボディ・各種色・戻りURL・戻りメッセージ・部屋数・表示行数・制限時間等・各部屋の名称の設定が行えます。
 
なお、config.cgi以外の固定的な部分はconfig.hに記述されていますので、acl.cgi/check.cgiのファイル名やCOOKIE名称などはconfig.h内を修正してください。

# 拡張機能
本CGIはオリジナルに比べて以下の拡張機能があります。

プロフィールでの使用禁止の言葉による入室禁止機能上記のcheck.cgiにてテキストにて、１行１単語（文）を登録することにより、その言葉に該当する文字がプロフィールに入っている場合は入室を禁止することができます。
IPまたはドメインによる入室禁止機能上記のacl.cgiにてテキストにて、１行にＩＰまたはドメインを指定することにより、入室の制限をすることができます。
管理権限による強制閉鎖機能config.h内のADMIN_NAME,ADMIN_PASSWDに設定した管理者情報を元に名前とプロフィールに登録したパスワードで閉鎖したい部屋を選択して入室ボタンを押すことにより、その部屋を強制閉鎖することが可能です。
リスト表示時の中段のコメントを開放config.dat内にて、管理者が任意のメッセージをリスト画面中段に表示することが可能です。
 

# 謝辞
本スクリプトを開発するに辺り、汎用ライブラリとして結城　浩氏のクッキー制御、CGIパーツのライブラリソースを利用させて頂いています。
Ｃ　Magazine等の書籍で有名な方ですので、CGIのお勉強には良いと思います。
結城氏のHOMEPAGEは、こちらです。

某サイトの管理人様より無言アラート機能などのソースを提供して頂きました。

