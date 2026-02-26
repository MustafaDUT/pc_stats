# 🖥️ PC Stats & Ambient Monitor - ESP32 🌡️

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Python](https://img.shields.io/badge/Python-3.10%2B-blue.svg)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg)

Bu proje, bilgisayarınızın performans verilerini (CPU, GPU, RAM) gerçek zamanlı olarak bir ESP32 ve 4'lü LED Matris panelinde görüntülemenizi sağlar. Aynı zamanda ortam sıcaklığı, nemi ve saat gibi bilgileri de içeren farklı modlar arasında geçiş yapabilir!

---

## ✨ Özellikler
- **Gerçek Zamanlı İstatistikler:** PC'den gelen CPU, GPU ve RAM kullanımını dinamik bar grafikleriyle takip edin.
- **Ortam İzleyici:** SHT21 sensörü ile oda sıcaklığı ve nemini anlık görüntüleyin.
- **Saat Modu:** PC backend üzerinden senkronize edilen dijital saat.
- **mDNS Otomatik Keşif:** IP adresi girmeye son! Backend ve ESP32 birbirini ağda otomatik bulur.
- **Dokunmatik Kontrol:** Dokunmatik sensör ile ekranlar (Saat -> PC Stats -> Ortam) arası geçiş.
- **Düşük Enerji & Güvenlik:** Sadece okuma (Read-only) yapan hafif bir backend servisi.

---

## 🛠️ Donanım Gereksinimleri
- **ESP32-S3** (veya benzeri ESP32 modelleri)
- **MAX7219 4-in-1 LED Matris** (8x32 toplam)
- **SHT21 / HTU21DF** Sıcaklık ve Nem Sensörü (Opsiyonel ama önerilir)
- **TTP223 Dokunmatik Sensör** (Ekran geçişleri için)
- **Bağlantı Şeması (Özet):**
  - **Matrix:** CS: 10, DIN: 11, CLK: 12
  - **Sensör (I2C):** SDA: 8, SCL: 9
  - **Touch:** Pin 2

---

## 🚀 Hızlı Kurulum Rehberi

Projeyi ayağa kaldırmak çok basit! Sadece şu sırayı takip edin:

### 1️⃣ ESP32 Tarafı (Yazılım Yükleme)
1. `esp32_led_matris` klasörünü **VS Code + PlatformIO** ile açın.
2. `src/main.cpp` dosyasını açın ve şu satırları bulun:
   ```cpp
   // 13. ve 14. satırlar
   const char *ssid = "WiFi_Adınız";
   const char *password = "WiFi_Şifreniz";
   ```
3. Kendi WiFi bilgilerinizi girin ve kaydedin.
4. ESP32 cihazınızı bilgisayara bağlayıp **Upload** (Yükle) butonuna basın.

### 2️⃣ PC Tarafı (Backend Servisi)
Bu servis verileri toplayıp ESP32'ye gönderir.
1. Python 3.10 veya üzerinin yüklü olduğundan emin olun.
2. Terminali (veya PowerShell) proje ana dizininde açın.
3. Bağımlılıkları tek komutla kurun:
   ```bash
   # Windows için (PowerShell):
   ./install.ps1

   # Mac/Linux için:
   chmod +x install.sh
   ./install.sh
   ```
   *(Not: `uv` kullanıyorsanız direkt `uv sync` yapabilirsiniz.)*

4. Servisi başlatın:
   ```bash
   python main.py
   ```

---

## 🎮 Kullanım
- Her şey hazır! ESP32 ve PC aynı ağda olduğu sürece veriler saniyeler içinde akmaya başlar.
- ESP32 üzerindeki dokunmatik sensöre dokunarak şu modlar arasında gezinebilirsiniz:
  1. **Ambient:** Ortam sıcaklığı, Nem ve ESP iç sıcaklığı.
  2. **PC Stats:** CPU, GPU ve RAM kullanım grafikler.
  3. **Clock:** Saat ve saniye gösterimi.

---

## 📜 Lisans & Uygunluk
Bu proje **MIT Lisansı** ile korunmaktadır.
- Projeyi ticari/kişisel her türlü amaçla kullanabilir, değiştirebilir ve dağıtabilirsiniz.
- MIT lisansı son derece esnek ve geliştirici dostudur. GitHub üzerinde paylaşmak için en uygun lisanslardan biridir.

---

*Keyifli kullanımlar!* 🚀
