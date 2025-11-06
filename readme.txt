//1. Chương trình này làm gì? (tóm tắt nhanh)

Board “POWER CONTROL ARB PC-04” dùng Ethernet (W5100/W5500) để kết nối MQTT.

Đọc điện áp/dòng/công suất/năng lượng từ các công tơ PZEM-004T v2 (1 kênh “Tổng” + 8 kênh OUT1..OUT8 qua mạch chọn S0/S1).

Đọc nhiệt độ/độ ẩm từ SHT3x.

Hiển thị thông số trên LCD I2C 16×2 và có menu cấu hình bằng encoder A/B/OK.

Điều khiển 8 relay, nhận lệnh qua MQTT/CONTROL kèm CRC16; trả trạng thái (MODE/RELAY/V/A/TEM/HUM) cũng kèm CRC.

Lưu cấu hình (tên, ID, IP động/tĩnh, server, port, ngôn ngữ, …) vào EEPROM.

//2. Phần cứng & chân I/O chính

Relay: Relay[] = {5,6,8,11,41,37,35,33} (8 ngõ). Trạng thái đọc phản hồi ở ReadRelay[] = {3,7,10,12,40,36,34,32}.

PZEM:

pzemTong trên Serial2.

pzem1234 trên Serial3, chọn kênh bằng S1_pzem1234=27, S0_pzem1234=28 (4 ngõ).

pzem5678 trên Serial1, chọn kênh bằng S1_pzem5678=29, S0_pzem5678=39 (4 ngõ).

SHT3x: I2C.

LCD: LiquidCrystal_I2C lcd(0x27, 16, 2).

Encoder: A=47 (CLK), B=48 (DT), OK=49 (SW).

Chế độ Auto/Man: Auto=30, Man=31, đèn báo Den_Auto_Man=13.

MAC: base 00:01:02:04:00:00, 2 byte cuối gán theo ID để mỗi board có MAC khác nhau.

//3. Cấu hình & EEPROM (layout nổi bật)

ngonngu tại addr 100 (0=EN, 1=VI).

NAME 5 ký tự: addr 101..105 (chỉ số vào bảng ký tự chuChar).

ID: addr 106 (1..999).

IP động/tĩnh: addr 120 (0=DHCP, 1=Static).

IP tĩnh:

IP: 121..124, Gateway: 125..128, Subnet: 129..132.

Server Address: 141..172 (tối đa 32 ký tự, lưu dưới dạng chỉ số vào chuChar).

Port: 173 (High), 174 (Low) → ghép thành port = (H<<8)+L.

⚠️ Vấn đề: trong docEEPROM() có 2 dòng luôn ghi EEPROM.write(173,0x07); EEPROM.write(174,0x5C); ⇒ mỗi lần chạy ép port về 1884 (0x075C), làm mất cấu hình port bạn chỉnh ở menu sau khi reset. (Xem mục #10 để sửa.)

//4. Chuỗi topic MQTT (tạo động từ NAME & ID)

Trong doccauhinh():

CLIENT_ID = "<NAME>/<ID>" (VD: POWER/01).

Từ đó tạo các topic:

Lệnh vào:

CONTROL = "<NAME>/<ID>/CONTROL" (subscribe).

Trạng thái/lần phát ra:

STATUS = "<NAME>/<ID>/STATUS" (LWT & publish “online”).

MODE = "<NAME>/<ID>/MODE"

RELAY = "<NAME>/<ID>/RELAY"

V = "<NAME>/<ID>/V"

A = "<NAME>/<ID>/A"

TEM = "<NAME>/<ID>/TEM"

HUM = "<NAME>/<ID>/HUM"

⚠️ Thiếu: REPLY không được khởi tạo! Biến mqtt_topic_pub_REPLY không được gán trong doccauhinh(), nhưng có lệnh publish tới mqtt_topic_pub_REPLY trong docenthernet(). Điều này khiến publish không có topic (sai/không gửi được). (Xem mục #10 để sửa.)

//5. Kết nối mạng & MQTT

ConnectEthernet() (20 giây/lần thử):

DHCP hoặc IP tĩnh → nếu nhận IP ≠ 0.0.0.0 coi như đã lên mạng.

Cấu hình PubSubClient: setServer(mqtt_server, mqtt_port), setCallback(docenthernet).

Gọi subscribe(CONTROL, 1) (thực tế chỉ có hiệu lực sau khi connected).

reconnectserver():

Gọi mqttClient.connect(CLIENT_ID, mqtt_user, mqtt_pwd, STATUS, 1, 1, "0").
→ LWT (Last Will) đặt tại STATUS với message "0" (retained).

Khi connect OK:

publish(STATUS, 0x31, retained) → 0x31 = ký tự '1' ⇒ thông báo “online”.

subscribe(CONTROL, 1).

Bật lcd.backlight(), set cờ để gửi trạng thái khởi động.

//6. Lệnh MQTT vào & định dạng payload (kèm CRC16)

Hàm docenthernet(char* topic, byte* payload, unsigned int length)

Nhận payload 4 byte theo thứ tự:

vitrirelay (1..20, hoặc 0 để áp lệnh cho tất cả)

tatmo (0 hoặc 255)

CRCLo (thực chất là byte cao của CRC – đặt tên hơi ngược)

CRCHi (byte thấp)

Tính CRC16: calcCRC16(2, crcdata) trên 2 byte đầu [vitrirelay, tatmo].
So khớp:

vitrirelay <= 20

tatmo ∈ {0, 255}

CRCLo == (crc >> 8) và CRCHi == (crc & 0xFF)

Nếu hợp lệ:

Phản hồi về REPLY: 4 byte y hệt request
(⚠️ nhưng do thiếu gán mqtt_topic_pub_REPLY, hiện tại không gửi được).

Chuyển tatmo 255→1, 0→0, set lenh=1 để thực thi ở loop().

Điều khiển relay (dieukhienrelay()):

Nếu 1 <= vitrirelay <= 8 → digitalWrite(Relay[vitrirelay-1], not(tatmo)).

Nếu vitrirelay==0 → áp lệnh cho toàn bộ 8 relay.

Lưu ý: Số relay thực tế là 8, nhưng mảng trạng thái n[] và đóng gói RELAY chuẩn bị cho 20 kênh (3 byte bitmask). Các bit >8 hiện luôn 0 (không hại, nhưng dư thừa).

//7. Dữ liệu cảm biến & bản tin trạng thái ra

Đọc PZEM (docdienapdongdien() mỗi 10 giây):

Kênh 0: pzemTong (IN).

Kênh 1..4: chọn S0/S1 cho pzem1234.

Kênh 5..8: chọn S0/S1 cho pzem5678.

Lưu vào: dienap[i] (int), dongdien[i] (float), congsuat[i] (float), diennangtieuthu[i] (float Wh).

Đọc SHT3x (docnhietdodoam() ~5s/lần): temp_c1, humidity1.

Gửi trạng thái (guitrangthai()):

MODE: 1 byte: 0x00 (Man?) / 0xFF (Auto?) + CRC(2B).
(Code lấy digitalRead(Auto): nếu nhấn Auto=0 → 0x00, ngược lại 0xFF.)

RELAY: 3 byte bitmask (tối đa 24 kênh) + CRC(2B). Với 8 relay thực, chỉ byte1 có ý nghĩa.

V (điện áp kênh IN): 2 byte big-endian + CRC(2B).

A (dòng IN): 1 byte số nguyên (cắt phần thập phân) + CRC(2B).

Giới hạn 0..255 A — nếu >255 sẽ tràn.

TEM/HUM: 1 byte (0..255) + CRC(2B).

CRC16: luôn tính trên phần dữ liệu (không gồm CRC), sau đó gửi [High, Low] (code đặt tên CRCLo/CRCHi hơi lệch).

//8. LCD hiển thị & Menu cấu hình bằng encoder

Màn hình chính (hienLCD()):

Dòng 1: điện áp & dòng IN (định dạng: VVV V AAA.A).

Dòng 2: temp°C, humidity%, và nhấp nháy AUTO/MAN.

Menu Setup (SetupLCD() và loạt hàm SetupLCD_*): 4 tầng Setup_1..4.

OUTPUT: xem IN/OUTx, RESET năng lượng PZEM (từng kênh / tất cả).

PARAMETER: (placeholder, chưa hiện menu cụ thể).

SETUP:

NAME: 5 ký tự, nhập theo bảng chuChar.

ID: 1..999 (lưu EEPROM addr 106).
→ Đổi ID sẽ reset board (để cập nhật MAC & topics).

IP: Auto (DHCP) hoặc Manual (IP/GW/Subnet) → lưu EEPROM → reset.

SERVER:

Address (tối đa 32 ký tự, lưu dạng chỉ số chuChar)

PORT (có màn nhập, nhưng hiện không lưu EEPROM – xem mục #10)

User/Pass: chưa triển khai.

LANGUAGE: EN / VI (addr 100).

Encoder đọc bằng pulseIn(A,1) + đọc kênh B để tăng/giảm; OK để chọn/lưu; tự thoát menu nếu 20s không thao tác.

//9. Luồng chạy chính

setup():

Khởi tạo Serial, đặt toàn bộ relay HIGH (tắt).

LCD → splash “POWER CONTROL / ARB PC-04”.

doccauhinh() → đọc EEPROM, dựng CLIENT_ID, topics, server, port, MAC.

Gọi ConnectEthernet() (thử lấy IP, cấu hình MQTT client).

Khởi động PZEM, SHT3x, gán pin chế độ Auto/Man, encoder…

loop():

Nếu chưa có IP → ConnectEthernet() theo chu kỳ.

Nếu có mạng nhưng chưa vào MQTT → reconnectserver().

Nếu lenh==1 (vừa nhận lệnh) → dieukhienrelay() rồi xóa cờ.

doctrangthai() để đọc SHT3x, PZEM khi có biến đổi, nháy LED Auto/Man, cập nhật n[i].

Nếu đã vào MQTT → guitrangthai() khi có thay đổi (và lúc khởi động).

Nếu đang trong menu → SetupLCD(); nếu không → hienLCD().

//10. Điểm cần sửa/cải thiện (rất nên làm)

Thiếu gán topic REPLY
Thêm vào doccauhinh():

REPLYString = CLIENT_IDString;
REPLYString += "/REPLY";
REPLYString.toCharArray(REPLYChar, REPLYString.length()+1);
mqtt_topic_pub_REPLY = REPLYChar;


→ để mqttClient.publish(mqtt_topic_pub_REPLY, ...) hoạt động.

Port bị ép về 1884 mỗi lần khởi động
Trong docEEPROM() bỏ hoặc chỉ khởi tạo một lần:

// CHỈ NÊN làm khi phát hiện EEPROM trống/chưa init:
// EEPROM.write(173, 0x07);
// EEPROM.write(174, 0x5C);


Và khi người dùng đổi port ở menu, lưu lại EEPROM:

Trong khối xử lý “PORT SETUP” (khi nhấn OK để lưu), thêm:

EEPROM.write(173, (port >> 8) & 0xFF);
EEPROM.write(174, port & 0xFF);
resetBoard();  // để áp dụng ngay


Đóng gói RELAY cho 8 kênh
Hiện tạo 3 byte (24 bit) trong khi chỉ có 8 relay. Có thể giữ như cũ (tương thích “tối đa 20/24”), hoặc rút gọn còn 1 byte cho gọn băng thông.

Kiểu dữ liệu publish

A (dòng) đang gửi số nguyên 1 byte → dễ tràn nếu >255 A, và mất phần thập phân.
Gợi ý: gửi 2 byte (x10) hoặc float 4 byte, có thang đo rõ ràng, kèm CRC như hiện tại.

TEM/HUM cũng là 1 byte — ok nếu bạn đảm bảo phạm vi.

String trên AVR
Bạn dùng String nhiều (dễ phân mảnh heap). Gợi ý đổi sang char buffer + snprintf.

Subscribe trước khi connect
ConnectEthernet() có subscribe khi chưa connect — PubSubClient sẽ bỏ qua. Bạn đã subscribe lại sau connect, nên không gây lỗi, chỉ hơi thừa.

Username/Password MQTT
Hiện để trống (#define mqtt_user "", #define mqtt_pwd ""). Nếu broker yêu cầu auth, cần:

Lưu user/pass vào EEPROM (giống Server Address).

Khi connect, truyền cặp user/pass thực.

CRC đặt tên biến
Bạn đang dùng CRCLo = (crc >> 8) và CRCHi = (crc & 0xFF) — tên “Lo/Hi” hơi ngược nghĩa. Không sai, nhưng dễ gây nhầm lẫn khi tích hợp hệ khác; document rõ thứ tự [High, Low].

Khởi tạo giá trị mặc định EEPROM
Đoạn “gợi ý” ghi default (NAME, SERVER “192.168.11.48”, …) đang comment. Nếu muốn tự-setup lần đầu:

Nên có cờ “đã init” (ví dụ addr 0 = 0xA5).

Chỉ ghi default một lần khi chưa init.

//11. Giao thức lệnh mẫu

Điều khiển: "<NAME>/<ID>/CONTROL" – payload 4 byte

[vitrirelay, tatmo, CRC_H, CRC_L]
// vitri: 1..8 (0 = tất cả)
// tatmo: 0 (OFF) hoặc 255 (ON)
// CRC16 trên 2 byte đầu (vitrirelay, tatmo), gửi theo thứ tự High, Low


Phản hồi: "<NAME>/<ID>/REPLY" – echo lại 4 byte trên khi hợp lệ.

Sau đó relay được set trong dieukhienrelay().

LWT/Trạng thái:

STATUS: "0" (LWT khi mất kết nối), "1" (0x31) khi online.

MODE: [mode, CRC_H, CRC_L] — mode=0x00 (Man?) / 0xFF (Auto?).

RELAY: [byte1, byte2, byte3, CRC_H, CRC_L] — bit = trạng thái từng kênh.

V: [V_H, V_L, CRC_H, CRC_L].

A: [A, CRC_H, CRC_L].

TEM: [t, CRC_H, CRC_L].

HUM: [h, CRC_H, CRC_L].

//12. Mẹo test nhanh

Đặt EEPROM phù hợp (NAME/ID/server/port).

Kiểm tra LCD có IP ≠ 0.0.0.0 → MQTT “connected”.

Sub ở PC (MQTT Explorer/Mosquitto_sub) vào:

+/+/STATUS, +/+/MODE, +/+/RELAY, +/+/V, +/+/A, +/+/TEM, +/+/HUM.

Gửi lệnh CONTROL hợp lệ (đúng CRC). Nếu chưa thấy REPLY → sửa mục #10.1.