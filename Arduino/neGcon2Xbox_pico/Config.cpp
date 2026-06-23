#include "Config.h"
#include <EEPROM.h>
#include <FatFS.h>
#include "XboxControllerPico.h"
#include <hardware/sync.h>

// config の実体定義
ConfigParams config = {
  .negLtCurve = DEFAULT_NEG_LT_CURVE,
  .negRtCurve = DEFAULT_NEG_RT_CURVE,
  .negReduceHandlePlay = DEFAULT_NEG_REDUCE_HANDLE_PLAY,
  .lxMax = DEFAULT_NEG_TWIST_MAX,
  .jogconDialMax = DEFAULT_JOGCON_DIAL_MAX,
  .stickMode = DEFAULT_STICK_MODE,
  .analogLxMax = DEFAULT_ANALOG_LX_MAX
};

// 他のソースファイルで定義されているグローバル変数への外部参照
extern bool fs_ready;
extern volatile bool flash_busy;
extern byte beforeStickMode;
extern bool config_file_writing;

/// <summary>
/// 設定パラメータを使って CONFIG.INI 形式のデータを指定した出力先に書き出します。
/// </summary>
/// <param name="out">出力先（FileなどのPrintインターフェース）</param>
/// <param name="config">設定パラメータ構造体</param>
void writeConfigIni(Print& out, const ConfigParams& config) {
  // セクション名とヘッダコメントを出力
  out.println(F("[neGcon]"));
  out.println(F("; neGcon2Xbox_pico CONFIGURATION FILE"));
  out.println();
  
  // アナログ感度カーブ（RT/LT）の設定出力
  out.println(F("; RT/LT analog sensitivity curve (0: LINEAR, 1: A1 (p=2.0), 2: A2 (p=4.0), 3: A3 (p=6.0))"));
  out.print(F("negLtCurve=")); out.println(config.negLtCurve);
  out.print(F("negRtCurve=")); out.println(config.negRtCurve);
  out.println();
  
  // ねじり遊び削減値（デッドゾーン補正）の設定出力
  out.println(F("; Twist play reduction (deadzone compensation) value (0 to 32)"));
  out.print(F("negReduceHandlePlay=")); out.println(config.negReduceHandlePlay);
  out.println();
  
  // ひねり最大角キャリブレーション値の設定出力
  out.println(F("; Twist max angle calibration value (1 to 255)"));
  out.print(F("negTwistMax=")); out.println(config.lxMax);
  out.println();
  
  // Jogconダイヤル最大回転位置キャリブレーション値の設定出力
  out.println(F("; Jogcon Dial max position calibration value (8 to 500)"));
  out.print(F("jogconDialMax=")); out.println(config.jogconDialMax);
  out.println();
  
  // コントローラーボタン配置モードの設定出力
  out.println(F("; Controller operation mode (0: STD, 1: SWAPAB, 2: SWAPLTRT, 3: SWAPAB_SWAPLTRT)"));
  out.print(F("stickMode=")); out.println(config.stickMode);
  out.println();
  
  // アナログスティック最大キャリブレーション値の設定出力
  out.println(F("; Analog stick max calibration value (128 to 255)"));
  out.print(F("analogLxMax=")); out.println(config.analogLxMax);
}

/// <summary>
/// EEPROMの初期設定と全領域のクリアを行います。
/// </summary>
void eepromFormat() {
  Serial.printf("[%lu] EEP Write!\n", millis());
  // 全領域を一旦0でクリア
  for (int i = 0; i < EEPROMSIZE; i++) {
    ::EEPROM.write(i, 0);
  }
  // フォーマット完了を示す識別マジックキャラクタ 'cf1' を先頭3バイトに書き込む
  ::EEPROM.write(0, 'c');
  ::EEPROM.write(1, 'f');
  ::EEPROM.write(2, '1');
  
  // 各アドレスに初期デフォルト値を書き込む
  ::EEPROM.write(EEPADR_NEGMODE, DEFAULT_STICK_MODE);
  ::EEPROM.write(EEPADR_NEG_NEGMAX, DEFAULT_NEG_TWIST_MAX);
  ::EEPROM.write(EEPADR_ANALOG_STICKMAX, DEFAULT_ANALOG_LX_MAX);
  ::EEPROM.write(EEPADR_JOG_MAX_U, (byte)(DEFAULT_JOGCON_DIAL_MAX >> 8));
  ::EEPROM.write(EEPADR_JOG_MAX_L, (byte)(DEFAULT_JOGCON_DIAL_MAX & 0xff));
  ::EEPROM.write(EEPADR_NEG_REDUCE_HANDLE_PLAY, DEFAULT_NEG_REDUCE_HANDLE_PLAY + 1); // ゲタ（+1）をはかせて保存
  ::EEPROM.write(EEPADR_NEG_ANALOG_LT_CURVE, DEFAULT_NEG_LT_CURVE);
  ::EEPROM.write(EEPADR_NEG_ANALOG_RT_CURVE, DEFAULT_NEG_RT_CURVE);
  
  // 物理Flashへ反映（コミット）
  ::EEPROM.commit();
}

/// <summary>
/// neGconのアナログLT感度カーブの設定値を復元します。
/// </summary>
/// <returns>復元されたカーブのインデックス (0-3)</returns>
byte restoreNegLtCurve() {
  byte tmp;
  // EEPROMバッファから値を取得
  tmp = ::EEPROM.read(EEPADR_NEG_ANALOG_LT_CURVE);
  // 値が範囲外の場合はデフォルト値に戻し、物理Flashへ緊急コミット
  if (tmp >= NEG_ANALOG_CURVE_MAX) {
    tmp = DEFAULT_NEG_LT_CURVE;
    Serial.printf("[%lu] EEP Write!\n", millis());
    ::EEPROM.write(EEPADR_NEG_ANALOG_LT_CURVE, tmp);
    ::EEPROM.commit();
  }
  return tmp;
}

/// <summary>
/// neGconのアナログRT感度カーブの設定値を復元します。
/// </summary>
/// <returns>復元されたカーブのインデックス (0-3)</returns>
byte restoreNegRtCurve() {
  byte tmp;
  // EEPROMバッファから値を取得
  tmp = ::EEPROM.read(EEPADR_NEG_ANALOG_RT_CURVE);
  // 値が範囲外の場合はデフォルト値に戻し、物理Flashへ緊急コミット
  if (tmp >= NEG_ANALOG_CURVE_MAX) {
    tmp = DEFAULT_NEG_RT_CURVE;
    Serial.printf("[%lu] EEP Write!\n", millis());
    ::EEPROM.write(EEPADR_NEG_ANALOG_RT_CURVE, tmp);
    ::EEPROM.commit();
  }
  return tmp;
}

/// <summary>
/// EEPROMが有効にフォーマットされているか確認します。
/// </summary>
/// <returns>有効な設定データが存在すれば true、存在しなければ false</returns>
bool eepromCheck() {
  // 先頭3バイトがマジックナンバー 'cf1' になっているかを確認
  if (::EEPROM.read(0) != 'c') return false;
  if (::EEPROM.read(1) != 'f') return false;
  if (::EEPROM.read(2) != '1') return false;
  return true;
}

/// <summary>
/// neGconひねり量の最大キャリブレーション値を復元します。
/// </summary>
/// <returns>復元された最大キャリブレーション値 (0x80-255)</returns>
byte restoreNegDegMax() {
  byte tmp;
  // EEPROMバッファから値を取得
  tmp = ::EEPROM.read(EEPADR_NEG_NEGMAX);
  // 最小閾値 (0x80) を下回る異常値の場合はデフォルトに強制復元
  if (tmp < 0x80) {
    tmp = DEFAULT_NEG_TWIST_MAX;
    Serial.printf("[%lu] EEP Write!\n", millis());
    ::EEPROM.write(EEPADR_NEG_NEGMAX, tmp);
    ::EEPROM.commit();
  }
  return tmp;
}

/// <summary>
/// Jogconダイヤルの最大回転キャリブレーション値を復元します。
/// </summary>
/// <returns>復元された最大キャリブレーション値 (8-500)</returns>
short restorejogMax() {
  short tmp;
  // 上位・下位バイトを結合してshort型の数値に復元
  tmp = (short)::EEPROM.read(EEPADR_JOG_MAX_U);
  tmp = (tmp << 8) | (short)::EEPROM.read(EEPADR_JOG_MAX_L);
  // 最小しきい値 (0x10) を下回る場合は初期デフォルトに戻す
  if (tmp < 0x10) {
    tmp = DEFAULT_JOGCON_DIAL_MAX;
    ::EEPROM.write(EEPADR_JOG_MAX_U, (byte)(tmp >> 8));
    ::EEPROM.write(EEPADR_JOG_MAX_L, (byte)(tmp & 0xff));
    Serial.printf("[%lu] EEP Write!\n", millis());
    ::EEPROM.commit();
  }
  return tmp;
}

/// <summary>
/// 通常アナログスティックの最大キャリブレーション値を復元します。
/// </summary>
/// <returns>復元された最大キャリブレーション値 (128-255)</returns>
byte restoreAnaDegMax() {
  byte tmp;
  // EEPROMバッファから値を取得
  tmp = ::EEPROM.read(EEPADR_ANALOG_STICKMAX);
  // アナログしきい値の下限 (128) を下回る場合はデフォルトに戻す
  if (tmp < 0x80) {
    tmp = DEFAULT_ANALOG_LX_MAX;
    Serial.printf("[%lu] EEP Write!\n", millis());
    ::EEPROM.write(EEPADR_ANALOG_STICKMAX, tmp);
    ::EEPROM.commit();
  }
  return tmp;
}

/// <summary>
/// コントローラーのボタン配置モード設定を復元します。
/// </summary>
/// <returns>復元されたモードインデックス (MODE_STD ~ AIRCON22)</returns>
byte restoreNegStickMode() {
  byte tmp;
  // EEPROMバッファから値を取得
  tmp = ::EEPROM.read(EEPADR_NEGMODE);
  // 有効な動作モードであるかバリデーションを実施
  switch (tmp) {
    case MODE_STD:
    case MODE_SWAPAB:
    case MODE_SWAPLTRT:
    case MODE_SWAPAB_SWAPLTRT:
    case MODE_AIRCON22:
      break;
    default:
      // 不明なモードは標準モードに戻し、物理Flashへ即座に書き込み
      tmp = DEFAULT_STICK_MODE;
      Serial.printf("[%lu] EEP Write!\n", millis());
      ::EEPROM.write(EEPADR_NEGMODE, tmp);
      ::EEPROM.commit();
      break;
  }
  return tmp;
}

/// <summary>
/// neGconハンドル（ひねり）の遊び削減値を復元します。
/// </summary>
/// <returns>復元された遊び削減値 (0-32)</returns>
byte restoreNegReduceHandlePlay() {
  byte tmp;
  // EEPROMバッファから値を取得
  tmp = ::EEPROM.read(EEPADR_NEG_REDUCE_HANDLE_PLAY);
  // ゲタ（+1）を考慮し、未設定（0）または範囲外の場合はデフォルト値にする
  if (tmp == 0 || tmp > MAX_NEG_REDUCE_HANDLE_PLAY+1) {
    tmp = DEFAULT_NEG_REDUCE_HANDLE_PLAY + 1;
    Serial.printf("[%lu] EEP Write!\n", millis());
    ::EEPROM.write(EEPADR_NEG_REDUCE_HANDLE_PLAY, tmp);
    ::EEPROM.commit();
  }
  return tmp - 1; // ゲタ（-1）を外して実際の遊び値を返却
}

/// <summary>
/// 現在の設定パラメータを /CONFIG.INI ファイルへ書き出し保存します。
/// </summary>
void saveConfig(byte currentStickMode) {
  if (!fs_ready) return;
  
  flash_busy = true; // Flashへのファイル書き出し中のバス競合を防ぐため、Core 1 (サブコア) の動作を一時停止
  delay(10);
  config_file_writing = true; // ホストPC（MSCモード）からの同時ファイル書き込み要求の衝突を防ぐフラグをセット
  
  File file = FatFS.open("/CONFIG.INI", "w");
  if (!file) {
    config_file_writing = false;
    flash_busy = false;
    return;
  }

  // 設定モード・切断中・MSCモードなどの一時的な内部状態は保存せず、通常動作モードのみを書き出す。
  // それ以外の状態のときは、設定モード突入前の動作モード（beforeStickMode）を代わりに保存する。
  auto isNormalStickMode = [](byte m) {
    return m == MODE_STD || m == MODE_SWAPAB || m == MODE_SWAPLTRT ||
           m == MODE_SWAPAB_SWAPLTRT || m == MODE_AIRCON22;
  };
  byte saveStickMode = isNormalStickMode(currentStickMode) ? currentStickMode : beforeStickMode;
  
  ConfigParams saveConfig = config;
  saveConfig.stickMode = saveStickMode;
  writeConfigIni(file, saveConfig); // INI形式で書き込み

  file.close();
  config_file_writing = false;
  flash_busy = false; // Core 1 の動作を再開
}

/// <summary>
/// CONFIG.INI ファイルまたは EEPROM から設定パラメータをロードします。
/// </summary>
void loadConfig() {
  // ファイルシステムが起動していない場合は、フォールバックとして物理EEPROMから設定をロードします
  if (!fs_ready) {
    if (eepromCheck() != true) {
      eepromFormat();
    }
    config.negLtCurve = restoreNegLtCurve();
    config.negRtCurve = restoreNegRtCurve();
    config.negReduceHandlePlay = restoreNegReduceHandlePlay();
    config.lxMax = restoreNegDegMax();
    config.analogLxMax = restoreAnaDegMax();
    config.jogconDialMax = restorejogMax();
    config.stickMode = restoreNegStickMode();
    return;
  }

  // CONFIG.INI が存在しない場合は、新規作成およびEEPROMからの設定移行を行います
  if (!FatFS.exists("/CONFIG.INI")) {
    migrateOrInitConfig();
  }

  File file = FatFS.open("/CONFIG.INI", "r");
  if (!file) {
    // ファイルオープンに失敗した場合は、フォールバックとして物理EEPROMからロードします
    if (eepromCheck() != true) {
      eepromFormat();
    }
    config.negLtCurve = restoreNegLtCurve();
    config.negRtCurve = restoreNegRtCurve();
    config.negReduceHandlePlay = restoreNegReduceHandlePlay();
    config.lxMax = restoreNegDegMax();
    config.analogLxMax = restoreAnaDegMax();
    config.jogconDialMax = restorejogMax();
    config.stickMode = restoreNegStickMode();
    return;
  }

  // ファイルサイズが異常に小さい、または大きすぎる場合は破損とみなし、削除してEEPROMからロードします
  if (file.size() < 50 || file.size() > 4096) {
    file.close();
    FatFS.remove("/CONFIG.INI");
    if (eepromCheck() != true) {
      eepromFormat();
    }
    config.negLtCurve = restoreNegLtCurve();
    config.negRtCurve = restoreNegRtCurve();
    config.negReduceHandlePlay = restoreNegReduceHandlePlay();
    config.lxMax = restoreNegDegMax();
    config.analogLxMax = restoreAnaDegMax();
    config.jogconDialMax = restorejogMax();
    config.stickMode = restoreNegStickMode();
    return;
  }

  bool updated = false; // パラメータ値に更新があったかどうかの検出フラグ
  int loopCount = 0;    // 無限ループ防止用カウンタ
  
  // ファイルから1行ずつ読み込んで解析します
  while (file.available()) {
    loopCount++;
    String line = file.readStringUntil('\n');
    line.trim();
    // コメント行、セクション宣言、空行は無視します
    if (line.startsWith(";") || line.startsWith("[") || line.length() == 0) {
      continue;
    }
    int eqIdx = line.indexOf('=');
    if (eqIdx > 0) {
      String key = line.substring(0, eqIdx);
      String val = line.substring(eqIdx + 1);
      key.trim();
      val.trim();
      int intVal = val.toInt();
      // 各設定キーに対応する構造体メンバへ値を適用し、値が変化していれば更新フラグを立てます
      if (key == "negLtCurve" && config.negLtCurve != (byte)intVal) {
        config.negLtCurve = (byte)intVal;
        updated = true;
      } else if (key == "negRtCurve" && config.negRtCurve != (byte)intVal) {
        config.negRtCurve = (byte)intVal;
        updated = true;
      } else if (key == "negReduceHandlePlay" && config.negReduceHandlePlay != (byte)intVal) {
        config.negReduceHandlePlay = (byte)intVal;
        updated = true;
      } else if (key == "negTwistMax" && config.lxMax != (byte)intVal) {
        config.lxMax = (byte)intVal;
        updated = true;
      } else if (key == "jogconDialMax" && config.jogconDialMax != (short)intVal) {
        config.jogconDialMax = (short)intVal;
        updated = true;
      } else if (key == "stickMode" && config.stickMode != (byte)intVal) {
        config.stickMode = (byte)intVal;
        updated = true;
      } else if (key == "analogLxMax" && config.analogLxMax != (byte)intVal) {
        config.analogLxMax = (byte)intVal;
        updated = true;
      }
    }
    if (loopCount > 100) {
      break;
    }
  }
  file.close();

  // ファイルシステムからロードした設定値を、既存コード内のEEPROM.read()との互換性のためRAMバッファにミラー保存します
  if (fs_ready) {
    ::EEPROM.write(EEPADR_NEGMODE, config.stickMode);
    ::EEPROM.write(EEPADR_NEG_NEGMAX, config.lxMax);
    ::EEPROM.write(EEPADR_ANALOG_STICKMAX, config.analogLxMax);
    ::EEPROM.write(EEPADR_JOG_MAX_U, (byte)(config.jogconDialMax >> 8));
    ::EEPROM.write(EEPADR_JOG_MAX_L, (byte)(config.jogconDialMax & 0x00ff));
    ::EEPROM.write(EEPADR_NEG_REDUCE_HANDLE_PLAY, config.negReduceHandlePlay + 1);
    ::EEPROM.write(EEPADR_NEG_ANALOG_LT_CURVE, config.negLtCurve);
    ::EEPROM.write(EEPADR_NEG_ANALOG_RT_CURVE, config.negRtCurve);
  }

  // ファイルから読み込まれた値に更新があった場合、フェールセーフ（フォールバック）用に物理EEPROM（Flash）へ安全に同期保存します
  if (updated && fs_ready) {
    flash_busy = true; // Core 1 の動作を一時停止
    delay(10);
    uint32_t ints = save_and_disable_interrupts(); // 割り込みを禁止
    ::EEPROM.write(0, 'c');
    ::EEPROM.write(1, 'f');
    ::EEPROM.write(2, '1');
    ::EEPROM.write(EEPADR_NEGMODE, config.stickMode);
    ::EEPROM.write(EEPADR_NEG_NEGMAX, config.lxMax);
    ::EEPROM.write(EEPADR_ANALOG_STICKMAX, config.analogLxMax);
    ::EEPROM.write(EEPADR_JOG_MAX_U, (byte)(config.jogconDialMax >> 8));
    ::EEPROM.write(EEPADR_JOG_MAX_L, (byte)(config.jogconDialMax & 0x00ff));
    ::EEPROM.write(EEPADR_NEG_REDUCE_HANDLE_PLAY, config.negReduceHandlePlay + 1);
    ::EEPROM.write(EEPADR_NEG_ANALOG_LT_CURVE, config.negLtCurve);
    ::EEPROM.write(EEPADR_NEG_ANALOG_RT_CURVE, config.negRtCurve);
    ::EEPROM.commit(); // 物理Flashへコミット実行
    restore_interrupts(ints); // 割り込み復帰
    flash_busy = false; // Core 1 の動作を再開
  }
}

/// <summary>
/// EEPROMの設定値を CONFIG.INI へ移行するか、またはテンプレートから初期生成します。
/// </summary>
void migrateOrInitConfig() {
  if (!fs_ready) return;
  // EEPROMに設定データが存在する場合は、読み出して CONFIG.INI を新規保存（マイグレーション）します
  if (eepromCheck() == true) {
    config.negLtCurve = restoreNegLtCurve();
    config.negRtCurve = restoreNegRtCurve();
    config.negReduceHandlePlay = restoreNegReduceHandlePlay();
    config.lxMax = restoreNegDegMax();
    config.analogLxMax = restoreAnaDegMax();
    config.jogconDialMax = restorejogMax();
    config.stickMode = restoreNegStickMode();
    saveConfig(config.stickMode);
  } else {
    // 物理EEPROMにデータが無い場合はデフォルトの設定値で CONFIG.INI を新規生成します
    File file = FatFS.open("/CONFIG.INI", "w");
    if (file) {
      ConfigParams defaultConfig = {
        .negLtCurve = DEFAULT_NEG_LT_CURVE,
        .negRtCurve = DEFAULT_NEG_RT_CURVE,
        .negReduceHandlePlay = DEFAULT_NEG_REDUCE_HANDLE_PLAY,
        .lxMax = DEFAULT_NEG_TWIST_MAX,
        .jogconDialMax = DEFAULT_JOGCON_DIAL_MAX,
        .stickMode = DEFAULT_STICK_MODE,
        .analogLxMax = DEFAULT_ANALOG_LX_MAX
      };
      writeConfigIni(file, defaultConfig);
      file.close();
    }
  }
}
