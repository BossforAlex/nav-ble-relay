/// BLE 协议常量定义
///
/// 必须与 ESP32 端保持一致。
/// Flutter 端作为 GATT Client（中心设备），主动扫描并连接
/// 名为 "AutoNavDisplay" 的 ESP32 设备。
///
/// v0.5.6 重构：参考 alexanderlavrushko/BLE-HUD-navigation-ESP32 极简模式
/// 1 Service + 2 Characteristics：
///   - charDataUuid：手机写（WRITE | WRITE_NR）—— 写 JSON 导航数据
///   - charPollUuid：手机订阅（NOTIFY）—— 收到 ESP32 的 poll 后立刻写数据
///
/// 所有 5 类导航数据（GUIDE/DRIVE/TMC/STATE/LOCATION）通过 JSON 中的
/// "type" 字段路由到同一个 char。
class BleConstants {
  BleConstants._();

  /// 主服务 UUID（与 ESP32 端约定）
  static const String serviceUuid =
      '0000ffe0-0000-1000-8000-00805f9b34fb';

  /// 数据特征值（手机写）：所有导航数据走这一个 char
  /// JSON 格式：{"type": "guide"|"drive"|"tmc"|"state"|"location", "data": {...}}
  static const String charDataUuid =
      '0000ffe1-0000-1000-8000-00805f9b34fb';

  /// Poll 特征值（手机订阅 NOTIFY）：ESP32 每 2 秒发一次空通知
  /// 收到后立刻把最新的导航数据写一次 charDataUuid
  static const String charPollUuid =
      '0000ffe2-0000-1000-8000-00805f9b34fb';

  /// Client Characteristic Configuration Descriptor
  static const String cccdUuid =
      '00002902-0000-1000-8000-00805f9b34fb';

  /// ESP32 设备名（ESP32 端广播名）
  static const String deviceName = 'AutoNavDisplay';

  /// 单包最大字节数（默认 MTU 下建议上限）
  static const int maxPacketBytes = 500;
}
