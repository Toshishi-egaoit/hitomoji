更新時の注意(v0.6.0.1用)

v0.5.5.2 から、基本設定と標準の記号定義はDLLに内蔵されています。
そのため、hitomoji.dicは配付物に含まなくなりました。
hitomoji.ini に書いていない項目には、内蔵のデフォルト設定が適用されます。
新しい設定項目も、ユーザーが指定しなければ新しいDLLの推奨値で動作します。
明示的に指定した項目は、ユーザーの設定が優先されます。

install.bat は、%APPDATA%\Hitomojiのフォルダにある以下のファイルを上書きしません。
　- hitomoji.ini 
　- my.ini
全てを上書きしたい場合は、install.bat /f を実行してください。

hitomoji.sample.iniは、設定項目の記述例です。
デフォルト値を変更する場合は、更したい内容を hitomoji.ini に記述してください。

非漢字などを独自でキー定義したい場合は、my.ini を作成して hitomoji.ini から
include することをおすすめします。
この記述方法については、hitomoji.sample.ini を参照してください。

hikanji.ini には、JIS X0208の非漢字のうち、1区と2区の内容が定義されています。
※hikanji.iniは非漢字の「よみ」を調べるための参照用ですので、変更しないで
ください。変更したい場合は、my.ini に記述してください。

旧版の設定は自動変換・削除しません。以前の @include hikanji.ini は、
標準定義が内蔵されたため不要です。独自に編集していた場合は、その変更分を
hitomoji.ini または my.ini に移してから include を外してください。
旧名の layer2.sample.ini や my.layer2.ini を include している場合も、
既存ファイルはそのまま使えます。my.ini に移す場合は include の名前も変更してください。
