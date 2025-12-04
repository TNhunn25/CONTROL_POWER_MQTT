# Tóm tắt chức năng
- Điều khiển và giám sát 4 kênh relay qua Ethernet (W5100/W5500) bằng MQTT, kèm đo điện áp, dòng và điện năng từ các cảm biến PZEM-004T v2.
- Hiển thị số liệu và menu cấu hình trên LCD I2C 16×2, dùng encoder (A/B/SW) để duyệt và chỉnh cấu hình mạng.
- Hỗ trợ cấu hình IP/MQTT bằng lệnh Serial, lưu toàn bộ cấu hình mạng, MAC, trạng thái relay và điện năng vào EEPROM.

## Mạng & MQTT
- Địa chỉ mặc định: IP 192.168.80.5, gateway 192.168.80.254, subnet 255.255.255.0, DNS 8.8.8.8, server `mqtt.dev.altasoftware.vn`, port 1883, user `altamedia`/`Altamedia@%`. MAC được đọc từ EEPROM, nếu chưa có sẽ sinh ngẫu nhiên rồi lưu lại.
- MQTT client ID dạng `CONTROL_POWER-<MAC>` và kết nối với LWT trên `CRL_POWER/INFO` (payload `{id,status}` offline/online). Các topic sử dụng:
  - `CRL_POWER/command`: nhận JSON điều khiển.
  - `CRL_POWER/ACK`: trả kết quả xử lý lệnh.
  - `CRL_POWER/STATUS`: phát trạng thái các kênh và kênh tổng.
  - `CRL_POWER/INFO`: thông báo online/offline.
- Gói lệnh MQTT chấp nhận `{"OUTPUT":"1, ON"}` hoặc `{"OUTPUT":"1","STATE":"ON"}` (OUTPUT 1..4 hoặc "ALL"). Khi đang MAN, lệnh bị từ chối và ACK trả `Result=FAIL`.
- ACK publish dạng JSON `{Channel,Result,Status,Mode,Seq,Time}`. Telemetry `CRL_POWER/STATUS` chứa `{Device_ID,Channel,Mode,Status,V,A,Kwh,Seq,Time,KEY}`, trong đó KEY là MD5 ký dữ liệu để đảm bảo toàn vẹn.

## Chu trình hoạt động
- Khởi động: nạp/generare MAC, đọc cấu hình mạng & MQTT từ EEPROM, áp dụng Ethernet, khởi tạo MQTT callback, LCD và PZEM. Trạng thái relay và điện năng được nạp từ EEPROM; nếu đang AUTO sẽ bật lại relay theo thứ tự để tránh đóng đồng thời.
- Vòng lặp: tự động reconnect MQTT, đọc encoder/LCD sau 5s, quét PZEM chu kỳ ~3s (qua HC4052) và cập nhật kênh tổng. Chỉ publish kênh thay đổi; 4 kênh riêng được gửi trước, sau đó kênh tổng. Điện năng được ghi EEPROM khi thay đổi vượt 0.01 kWh.
- Relay có thể đồng bộ trạng thái thực tế (MAN) từ các chân phản hồi, lưu lại EEPROM và đánh dấu gửi telemetry.

## Cấu hình & hiển thị
- LCD/encoder: menu xem số liệu 5 kênh (0 tổng + 1..4), trạng thái relay, xem cấu hình mạng, và chỉnh IP/Gateway/Subnet/DNS/MQTT (có timeout 60s nếu không thao tác).
- Serial (9600 bps): hỗ trợ `SET IP|GATEWAY|SUBNET|DNS <addr>`, `SET SERVER|PORT|USER|PASS <value>`, `SHOW`, `HELP`. Khi thay đổi, hệ thống lưu EEPROM, áp dụng lại Ethernet/MQTT và đánh dấu gửi lại telemetry.

## Phần cứng chính
- Relay điều khiển: các chân {5,6,8,11,...} với phản hồi {3,7,10,12,...}; firmware dùng 4 kênh đầu (RELAY_COUNT=4) để publish và điều khiển.
- Cảm biến PZEM-004T v2 trên Serial3 qua HC4052 (S0=27, S1=28); kênh tổng dùng Serial2. LED báo MAN tại chân 13; công tắc Auto=30, Man=31. Encoder: CLK=47, DT=48, SW=49. LCD I2C địa chỉ 0x27.


