更新時の注意

v0.5.5.2 から、基本設定と標準の記号定義はDLLに内蔵されています。
hitomoji.ini に書いていない項目には、内蔵のデフォルト設定が適用されます。
新しい設定項目も、ユーザーが指定しなければ新しいDLLの推奨値で動作します。
明示的に指定した項目は、ユーザーの設定が優先されます。

install.bat は、既存の %APPDATA%\Hitomoji\hitomoji.ini と my.ini を保持します。
install.bat /f は両方を配布時の内容で上書きするため、事前にバックアップしてください。
参照用の hitomoji.sample.ini と hikanji.ini は、通常の更新でも最新版へ更新されます。

設定項目と記述例は hitomoji.sample.ini を参照し、変更したい項目だけを
hitomoji.ini に記述してください。サンプル自体を編集しても動作は変わりません。
hikanji.ini も参照用です。独自の記号定義は hitomoji.ini の [key-table] に記述します。
my.ini を利用するには、hitomoji.ini の「# @include my.ini」の先頭の # を外します。

旧版の設定は自動変換・削除しません。以前の @include hikanji.ini は、
標準定義が内蔵されたため不要です。独自に編集していた場合は、その変更分を
hitomoji.ini または my.ini に移してから include を外してください。
旧名の layer2.sample.ini や my.layer2.ini を include している場合も、
既存ファイルはそのまま使えます。my.ini に移す場合は include の名前も変更してください。
