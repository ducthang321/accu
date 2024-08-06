#include <Wire.h>
#include <Adafruit_INA219.h>

// Khai báo cảm biến INA219
Adafruit_INA219 ina219;

// Dung lượng đầy đủ của pin (mAh) khi còn mới
const float Q_full_new = 3000.0;
// Dung lượng đầy đủ hiện tại của pin (mAh)
float Q_full_current = Q_full_new;

// Biến cho bộ lọc Kalman
float Q = 0.1;  // Quá trình nhiễu
float R = 0.1;  // Đo lường nhiễu
float P = 1;    // Sai số ước tính
float K = 0;    // Kalman Gain
float SoC_estimate = 100.0; // Ước tính ban đầu của SoC

unsigned long previousMillis = 0;
unsigned long lastUpdateTime = 0;
float elapsedTime_hours = 0;
float remainingCapacity_mAh = Q_full_new;

// Biến giả lập sạc pin
bool charging = false;
unsigned long chargeStartMillis = 0;
float energyCharged_mAh = 0; // Năng lượng đã nạp vào pin (mAh)

void setup() {
  Serial.begin(115200);
  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }
  Serial.println("INA219 is ready.");
  lastUpdateTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Kiểm tra xem có đủ thời gian để cập nhật trạng thái pin không
  if (currentMillis - lastUpdateTime >= 5000) { // 5000 ms = 5 giây
    lastUpdateTime = currentMillis; // Cập nhật thời gian cập nhật cuối cùng

    // Kiểm tra trạng thái sạc
    if (charging) {
      simulateCharging(currentMillis);
    } else {
      float busVoltage = ina219.getBusVoltage_V();
      float current_mA = ina219.getCurrent_mA();
      elapsedTime_hours = (currentMillis - previousMillis) / (1000.0 * 3600.0); // Chuyển đổi mili giây thành giờ
      previousMillis = currentMillis;
      
      remainingCapacity_mAh -= current_mA * elapsedTime_hours;

      float SoC_measurement = (remainingCapacity_mAh / Q_full_current) * 100.0;

      // Cập nhật bộ lọc Kalman
      P = P + Q;
      K = P / (P + R);
      SoC_estimate = SoC_estimate + K * (SoC_measurement - SoC_estimate);
      P = (1 - K) * P;

      // Tính toán SoH
      float SoH = (Q_full_current / Q_full_new) * 100.0;
      Serial.print("Voltage: "); Serial.print(busVoltage); Serial.println(" V");
      Serial.print("Current: "); Serial.print(current_mA); Serial.println(" mA");
      Serial.print("Remaining Capacity: "); Serial.print(remainingCapacity_mAh); Serial.println(" mAh");
      Serial.print("Measured SoC: "); Serial.print(SoC_measurement); Serial.println(" %");
      Serial.print("Estimated SoC: "); Serial.print(SoC_estimate); Serial.println(" %");
      Serial.print("SoH: "); Serial.print(SoH); Serial.println(" %");
      Serial.println("");
    }

    // Giả lập bắt đầu sạc pin khi dung lượng còn lại dưới 20%
    if (remainingCapacity_mAh < 0.2 * Q_full_current && !charging) {
      startCharging();
    }
  }

  delay(100); // Thời gian trễ giữa các lần kiểm tra trạng thái sạc
}

void startCharging() {
  charging = true;
  chargeStartMillis = millis(); //Hàm lấy thời gian bằng mili giây
  energyCharged_mAh = 0; // Đặt lại năng lượng đã nạp vào pin
  Serial.println("Charging started.");
}

void simulateCharging(unsigned long currentMillis) {
  // Giả lập sạc pin với dòng điện cố định
  float chargeCurrent_mA = 500.0; // Giả lập dòng sạc là 500 mA
  unsigned long chargeInterval = currentMillis - chargeStartMillis;
  float chargeTime_hours = chargeInterval / (1000.0 * 3600.0); // Chuyển đổi mili giây thành giờ
  
  float charged_mAh = chargeCurrent_mA * chargeTime_hours;
  energyCharged_mAh += charged_mAh; // Cộng thêm năng lượng đã nạp vào pin
  remainingCapacity_mAh += charged_mAh;

  // Giới hạn dung lượng còn lại không vượt quá dung lượng đầy đủ hiện tại
  if (remainingCapacity_mAh >= Q_full_current) {
    remainingCapacity_mAh = Q_full_current;
    charging = false;
    Q_full_current = energyCharged_mAh; // Cập nhật dung lượng đầy đủ hiện tại
    Serial.println("Charging completed.");
  }

  chargeStartMillis = currentMillis; // Cập nhật thời gian bắt đầu sạc cho lần tiếp theo
}
