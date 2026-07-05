/// BLE 协议常量定义
///
/// 必须与 ESP32 端保持一致。
/// Flutter 端作为 GATT Client（中心设备），主动扫描并连接
/// 名为 "AutoNavDisplay" 的 ESP32 设备，向 5 个特征值写入 JSON 数据。
class BleConstants {
  BleConstants._();

  /// 主服务 UUID（与 ESP32 端约定）
  static const String serviceUuid =
      '0000ffe0-0000-1000-8000-00805f9b34fb';

  /// 引导信息特征值（KEY_TYPE=10001）
  static const String charGuideUuid =
      '0000ffe1-0000-1000-8000-00805f9b34fb';

  /// 车道信息特征值（KEY_TYPE=13012）
  static const String charDriveWayUuid =
      '0000ffe2-0000-1000-8000-00805f9b34fb';

  /// 路况光柱特征值（KEY_TYPE=13011）
  static const String charTmcUuid =
      '0000ffe3-0000-1000-8000-00805f9b34fb';

  /// 导航状态特征值（KEY_TYPE=10019）
  static const String charStateUuid =
      '0000ffe4-0000-1000-8000-00805f9b34fb';

  /// 定位信息特征值（KEY_TYPE=10065）
  static const String charLocationUuid =
      '0000ffe5-0000-1000-8000-00805f9b34fb';

  /// Client Characteristic Configuration Descriptor
  static const String cccdUuid =
      '00002902-0000-1000-8000-00805f9b34fb';

  /// ESP32 设备名（ESP32 端广播名）
  static const String deviceName = 'AutoNavDisplay';

  /// 单包最大字节数（默认 MTU 下建议上限）
  static const int maxPacketBytes = 500;
}
