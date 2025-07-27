# 俺はneGconをアケアカのリッジレーサでつかいたいんじゃ、<BR>ついでにAirCombat22も対応しておいた for RP2040<BR>(neGcon2SwitchPICO)
![装置イメージ2](./img/img008.jpg)  

![装置イメージ3](./img/img015.jpg)  

![装置イメージ4](./img/img014.jpg)  

![組み込みイメージ](./img/img013.jpg)  

![装置イメージ](./img/img001.jpg)  

良くあるプレステのコントローラをUSBゲームコントローラに変換するアダプタです。  
先日作ったNAMCO neGcon to Nintendo Switch ConverterはAVRで作っているため、  
色々と今だとイマイチなところが多いので改めてraspberry PICO系のマイコンボードで作り直しました。  

nintendo SWITCH向けに作りましたが、Windowsなどで汎用的に使用することが可能です。  

Nitendo Switch用アケアカ版リッジレーサでネジコンが使いたい！！

と勢いで雑に作った・・・のですが、頒布するために、もう少し真面目にコードを書き直して、  
バカでっかいFlightControllerをAirCombat22に対応させてみたり、  
DualShockの辺りをちゃんと、真面目に対応させて見たら悪くは無い出来の様な気がします。  

Switch2でも動きました・・・＼(^o^)／

## ■ 本製品の頒布について

下記ショップで取り扱っていただいています。  

###家電のKENちゃん  

完成品 (ケース色 赤/赤)  
<https://www.kadenken.com/view/item/000000001894>  

完成品 (ケース色 透明/赤)  
<https://www.kadenken.com/view/item/000000001895>  

キット版 (要半田付け　ケース無し）  
<https://www.kadenken.com/view/item/000000001893>  

※売り切れ時は、追加生産個数の参考にしたいので  
　「再入荷のお知らせを受け取る」おいていただけると幸いです。  


## 作り方
どこの誤家庭にもある、Seeed XIAO RP2040 または raspberry PICOボートとプレステのコントローラをつないでください。  
1KΩのプルアップ抵抗は必須です。DATAと3.3Vの間につけてください  


## PS1/PS2 joypad との配線 

|PSX Pin | PSX Signal | XIAO RP2040 | raspberry PICO | RP2040 Signal |
| :- |  :- |  :- |  :- |  :- |
| 1 | DAT | 9 | GP4 | MISO<BR>[need pullup by 1k owm registor to 3.3V] |
| 2 | CMD | 10 | GP3 | MOSI |
| 3 | 9V<BR> (for motor, If you not necessary NC)| (VCC) | (VSYS) | +5V |
| 4 | GND | GND | GND | Power GND |
| 5 | 3.3V | +3V3 | 3V3 | Power 3.3V |
| 6 | Attention | 7 | GP1 | GPIO1 (Controller Select) |
| 7 | CLK | 8 | GP2 | SCK |
| 8 | NC | - | - |
| 9 | ACK | - | - |

![XIAO配線イメージ](./img/img002.png)  

![PICO装置イメージ](./img/img003.png)  

## Firmware書き込み方法
ケースまたはマイコンボード上にあるBボタンまたはBOOTSELボタンを押しながらUSBケーブルを接続してください。  
PCに接続した後も念のため数秒程度はボタンを離さないようにしてください。  
上手く接続できれば回路が「RPI-RP2」という名前で、USBメモリのように認識されます。  
「RPI-RP2」を開き、「xxxxx.uf2」ファイルをドラッグ&ドロップしたら完了です。  

「./firmware」のフォルダにコンパイル済のファイルが置いてあります。  

## ボタン配置について
接続するコントローラによってボタン配置や挙動が自動的に変更されます。  

## neGcon接続時のボタン配置
アケアカのリッジレーサに最適化されたモードです。  
neGcon接続時は[B]のBOOTボタンを押す事で設定を切り替えられます。(V1.20以降)  
また、neGcon/JoyCon/ANALOG JOYSTICK(SCPH-1110)接続時は、BOOTボタンをそのまま10秒程度更に長押しをすると、  
LEDが点滅し、最大角設定モードになります。(V1.30以降)  

アナログの感度に関しては、本Adapterでも補正を少しいれていますが、さらに調整が必要な場合は、  
アケアカ内のゲーム内の「設定」→「ボタン」→「こだわりのボタン設定」→「感度設定」で調整可能です。  

各モードのLED色は下記の通りです。  
| LEDの色 | モード |
| :- |  :- |
| 青色 | STD筐体モード  |
| 黄色 | DX筐体モード  |
| 紫色 | STD筐体モード(I/IIボタンSWAP)  |
| 緑色 | DX筐体モード(I/IIボタンSWAP)  |
| 白色 | デジタルモード  |
| 青点滅 | 最大角設定モード[ネジコン/ANALOG JOYSTICK(SCPH-1110)のみ]  |

### ボタン配置(アケアカ：リッジレーサ STD筐体モード)
|neGconボタン | Nintendo Switchコントローラ | 備考 |
| :- |  :- |  :- |
| ねじり (アナログ) | 左アナログスティックのX | 左アナログスティックのYは常にCenterです。 |
| I (アナログ) | 左アナログスティックのY 上方向 |  |
| II (アナログ) | 左アナログスティックのY 下方向 |  |
| L (アナログ) | Lボタン | 押し込まないと反応しません |
| R (デジタル) | Rボタン | |
| A (デジタル) | Aボタン | |
| B (デジタル) | Xボタン | |
| START (デジタル) | ＋ボタン |  |

### ボタン配置(アケアカ：リッジレーサ DX筐体モード)
| neGconボタン | Nintendo Switchコントローラ | 備考 |
| :- |  :- |  :- |
| ねじり (アナログ) | 左アナログスティックのX | 左アナログスティックのYは常にCenterです。 |
| I (アナログ) | ZRボタン | 押し込み量によって連射速度が変わります |
| II (アナログ) | ZLボタン | 押し込み量によって連射速度が変わります |
| L (アナログ) | Lボタン |  押し込まないと反応しません |
| R (デジタル) | Rボタン | |
| A (デジタル) | Aボタン | |
| B (デジタル) | Xボタン | |
| START (デジタル) | ＋ボタン |  |

## JOGCON接続時のボタン配置  
(V1.40以降) 
 
### ボタン配置
| JOGCON | Nintendo Switchコントローラ | 備考 |
| :- |  :- |  :- |
| JOGダイヤル | 左アナログスティック(X) |  |
| 右スティック (アナログ) | 右アナログスティック |  |
| ○  | 右アナログスティック | 値はボタンを押した時、0x00か0xffの2値になります。 |
| □  | 右アナログスティック | 値はボタンを押した時、0x00か0xffの2値になります。 |
| 十字キー | 十字キー |  |
| SELECT | -ボタン |  |
| START  | +ボタン |  |
| △ | Xボタン | |
| ○  | Aボタン | |
| □ | Yボタン | |
| ✕  | Bボタン | |
| L1  | Lボタン |  |
| L2  | ZLボタン |  |
| R1  | Rボタン |  |
| R2  | ZRボタン |  |


### JOGCONのダイヤル初期値について  
ダイヤルのZero位置は、接続時の時の位置が初期値になります。  
位置を調整したい場合は「MODEボタン」を押し、JOGCONモードをOFFにしたのち、ダイヤル位置を希望のZero位置にしてください。  
再度「MODEボタン」を押し、JOGCONモードに変更すると当該位置がZero位置になります。  

### JOGCONのダイヤルForcebackについて  
JOGCONのダイヤルは設定された最大角度を超える場合には、Forcebackを返しています。  
このForcebackですが、ジョグコンの説明書に記載通り、JOGCON側の機能としてセーフティモードが搭載されいます。  
この機能により約60秒間ダイヤル以外の操作が無いとForcebackが自動的に無くなりますが、  
何かJOGCONのボタンを押すことでForcebackが復帰します。  

## ANALOG JOYSTICK (SHPH-1110)接続時のボタン配置
アナログの感度に関しては、  
ゲーム内の「設定」→「ボタン」→「こだわりのボタン設定」→「感度設定」で調整可能です。  

### ボタン配置(アケアカ：AirCombat22モード)
| SHPH-1110 | Nintendo Switchコントローラ | 備考 |
| :- |  :- |  :- |
| 左スティック (アナログ) | 右アナログスティック | 操縦桿 |
| 右スティック (アナログ) | 左アナログスティック | スロットル |
| 右スティック HATスイッチ | 十字キー |  |
| SELECT | -ボタン |  |
| START  | +ボタン | メニュー |
| △ | Xボタン | クレジット |
| ○  | Aボタン | ミサイル |
| □ | Bボタン | バルカン |
| ✕  | Yボタン | START(視点切り替え)|
| L1  | ZLボタン |  |
| L2  | Lボタン |  |
| R1  | ZRボタン |  |
| R2  | Rボタン |  |


## DUALSHOCK/DUALSHOCK2接続時のボタン配置
※2回目の頒布で使用したFirmware V1.30ではバグがあり、アナログレバーが使用できないことが判明しました。  
V1.40以降のFirmwareへの更新をお願いします。  

### ボタン配置
| DUALSHOCK/DUALSHOCK2 | Nintendo Switchコントローラ | 備考 |
| :- |  :- |  :- |
| 左スティック (アナログ) | 左アナログスティック |  |
| 右スティック (アナログ) | 右アナログスティック |  |
| ○ (アナログ) | 未使用 | DUALSHOCK2のみ |
| □ (アナログ) | 未使用 | DUALSHOCK2のみ |
| 十字キー | 十字キー |  |
| SELECT | -ボタン |  |
| START  | +ボタン |  |
| △ | Xボタン | |
| ○  | Aボタン | |
| □ | Yボタン | |
| ✕  | Bボタン | |
| L1  | Lボタン |  |
| L2  | ZLボタン |  |
| R1  | Rボタン |  |
| R2  | ZRボタン |  |

## その他プレステコントローラ接続時のボタン配置
### ボタン配置
| プレステコントローラ | Nintendo Switchコントローラ | 備考 |
| :- |  :- |  :- |
| 十字キー | 十字キー |  |
| SELECT | -ボタン |  |
| START  | +ボタン |  |
| △ | Xボタン | |
| ○  | Aボタン | |
| □ | Yボタン | |
| ✕  | Bボタン | |
| L1  | Lボタン |  |
| L2  | ZLボタン |  |
| R1  | Rボタン |  |
| R2  | ZRボタン |  |

## 最大角度設定モードについて
neGcon/JoyCon/ANALOG JOYSTICK(SCPH-1110)接続時は、[B]のBOOTボタンを長押しをすると、  
LEDが点滅し、最大角設定モードになります。(V1.30以降)  

このモードで最大角を設定することでユーザ自身がneGcon/JoGconの捻り最大角度、  
ANALOG STICKでは操作時の最大角度を設定することができます。  

### 最大角の設定方法  
対応コントローラを接続し、[B]のBOOTボタンを10秒程度更に長押しすると当該モードに入ります。  

| LEDの色 | モード |
| :- |  :- |
| 青点滅 | 最大角設定モード[ネジコン/JOGCON/ANALOG JOYSTICK(SCPH-1110)のみ]  |

当該モードに入ったら、設定したい最大角度を実際に操作し、  
パッドの「スタートボタン」を一回押してください。  
LEDの点滅が止まり、元のモードに戻ります。  
設定値は自動で保存されます。  

初期値に戻したい場合は、コントローラやのニュートラル状態にして、パッドの「スタートボタン」を押してください。  
また、キャンセルは、[B]のBOOTボタンを1回押してください。  

### JOGCONでの最大角設定について  
当該モードに入ると、ダイヤルが自動的にZero位置まで移動します。  
モードに入る時にダイヤルには手を触れない様にお願いします。  

以降の操作については、他同様に設定したい最大角度までダイヤルを回し、  
パッドの「スタートボタン」を一回押してください。  

## 専用基板および筐体について

下記フォルダーに、PCBデータと筐体データがおいてあるので JLCPCBさんの辺りに、  
まとめてぶん投げると一番安い仕様を選択すれば大体、基板は$5+送料、筐体は$8+送料で仕上がるはずです。  

<https://github.com/v9938/neGcon2SwitchPICO/tree/main/pcb>

![基板イメージ](./img/img004.png) ![基板イメージ2](./img/img005.png)  
![筐体イメージ](./img/img006.jpg) ![筐体イメージ](./img/img007.jpg)  

※専用基板および完成品の頒布は現在準備中です。  
![頒布筐体](./img/img016.jpg)  


## パーツリスト  
|部品番号 | 部品名 | 参考 購入先 |
| :- |  :- |  :- |
| U1 | Seeed XIAO RP2040 | 秋月電子 ￥980 |
| R1 | 未実装 | ※R1かR2のどちらかを実装 |
| R2 | 1KΩ1/6W | 秋月電子 1袋100本入 ￥120 |
| 番号なし | PSコントローラコネクタ | 秋月電子 Vstone VS-C1接続用基板側コネクター ￥300 |
| その他 | 2.6mmx12mm タッピングねじ(サラ) | 筐体で使う固定のネジになります。 |

## 組み立て方法  
それほどパーツ数が多くないので、下記写真の様に組み立てください。  
PSコネクタは実装後に、曲げると壊れるので表側のタブも少し曲げて、基板にハンダした方が良いです。  

![基板表](./img/img009.jpg)  
![基板裏](./img/img010.jpg)  

筐体に入れる場合はマイコンボードに付属のヘッダピンは使わない様にして実装してください  
また、PSコネクタを半田付けする時に筐体に一度組み込んでハンダしましょう。  
(1mm程度基板から浮いて実装します)  


![組み込み1](./img/img011.jpg) ![組み込み2](./img/img012.jpg) 

## コントローラの遅延について  
本変換機の遅延は、USB Data Frameで1パケット分になります。  
変換機でのUSB Data Frameの要求値は1msですが、USB Data Frameの送信間隔はHOSTが決めています。  
そのため、実際のキー入力から遅延時間はHOSTにより変わります。  
こちらで計測した範囲では以下の様になっていました。  

|USB HOST | USB Data Frame間隔(キー入力の間隔) | 遅延時間(キー入力から本体への時間) |
| :- |  :- |  :- |
| Switch/Switch2 | 約7.5ms | 15ms～22.5ms |
| Windows系 | 約1.5ms | 3ms～4.5ms |

### PS1 CONTROLLER(SCPH-1080)のLボタンを押した時の実測値  
※Lボタンを押した時が0ms →「USB DATA 10 00 08 80 80 80 80 00」 がLボタンを押した時のデータ  

■nintendo SWITCH2
![遅延Switch2](./img/img018.png) 
■Microsoft Windows11
![遅延Windows](./img/img017.png) 


## 動作確認済コントローラ
(V1.4にて確認済)  
SONY SCPH-1010 標準コントローラ  
SONY SCPH-1080 標準コントローラ  
SONY SCPH-1110 ANALOG JOYSTICK  
SONY SEDR-10   ANALOG CONTROLLER (DUAL SHOCK2)  
HORI アナログ振動パッド
Sammy SMY-2002KP KEYBOARD PAD mini  
Logicool LPKC-40000 NetPlay Controller  
Namco NPC-101 neGcon (ネジコン 初期型)
Namco NPC-102 ボリュームコントローラ  
Namco NPC-105 JOGCON  


## 使用したlibraryについて  
本ソフトは下記libraryを利用しています。  

### Adafruit_NeoPixel
<https://github.com/adafruit/Adafruit_NeoPixel>  

### Adafruit_TinyUSB_Library
<https://github.com/adafruit/Adafruit_TinyUSB_Arduino>

### PsxNewLib  
<https://github.com/SukkoPera/PsxNewLib>  

### SwitchControllerPico  
<https://github.com/sorasen2020/SwitchControllerPico>  

## おまけ  
JLCPCBさんがやっているOpen-Source Engineer Storiesに参加中です。  
下記ポストをリポストしてもらえると嬉しいです。  

<https://x.com/v9938/status/1938441988971405770>
