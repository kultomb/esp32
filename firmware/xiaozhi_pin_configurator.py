import os
import sys
import time
import threading
import subprocess
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import serial.tools.list_ports

# Đường dẫn project
PROJECT_DIR = r"C:\xiaozhi"
CONFIG_H_PATH = os.path.join(PROJECT_DIR, r"main\boards\bread-compact-wifi\config.h")
BUILD_DIR = os.path.join(PROJECT_DIR, "build")
IDF_PATH = r"C:\Espressif\frameworks\esp-idf-v5.5.1"
PYTHON_DIR = r"C:\Espressif\tools\idf-python\3.11.2"
ESP_PYTHON = r"C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"

# Danh sách chân GPIO trên ESP32-S3 Super Mini
AVAILABLE_GPIOS = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "21", "47", "48", "NC"]

THEME_EMO = "🤖 EMO Robot (Mắt to bồng bềnh, tối giản EMO, không miệng)"
THEME_MOCHI = "🐱 Mochi Robot (Bộ 19 biểu cảm chi tiết + Miệng uốn lượn lò xo)"

class XiaozhiToolApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Xiaozhi S3 Super Mini - Pin & Theme Configurator & Flasher")
        self.root.geometry("860x780")
        self.root.configure(bg="#181825")
        self.root.minsize(800, 700)

        # Theme Colors
        self.c_bg = "#181825"
        self.c_card = "#1e1e2e"
        self.c_border = "#313244"
        self.c_text = "#cdd6f4"
        self.c_accent = "#89b4fa"
        self.c_green = "#a6e3a1"
        self.c_orange = "#fab387"
        self.c_red = "#f38ba8"

        self.setup_styles()
        self.create_widgets()
        self.load_current_config()
        self.refresh_ports()

    def setup_styles(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TLabel", background=self.c_card, foreground=self.c_text, font=("Segoe UI", 10))
        style.configure("Header.TLabel", background=self.c_bg, foreground=self.c_accent, font=("Segoe UI", 16, "bold"))
        style.configure("SubHeader.TLabel", background=self.c_card, foreground=self.c_orange, font=("Segoe UI", 11, "bold"))
        style.configure("TCombobox", fieldbackground="#313244", background="#45475a", foreground="#ffffff", font=("Segoe UI", 10))
        self.root.option_add("*TCombobox*Listbox.background", "#313244")
        self.root.option_add("*TCombobox*Listbox.foreground", "#ffffff")
        self.root.option_add("*TCombobox*Listbox.font", ("Segoe UI", 10))

    def create_widgets(self):
        # Header
        top_frame = tk.Frame(self.root, bg=self.c_bg)
        top_frame.pack(fill="x", padx=20, pady=(15, 8))

        title = tk.Label(top_frame, text="⚡ Xiaozhi ESP32-S3 Super Mini Tool", font=("Segoe UI", 16, "bold"), bg=self.c_bg, fg=self.c_accent)
        title.pack(anchor="w")
        desc = tk.Label(top_frame, text="Tùy chỉnh chân cắm linh kiện, đổi giao diện biểu cảm (EMO / Mochi 19 cảm xúc), Rebuild siêu tốc và Nạp trực tiếp", font=("Segoe UI", 9), bg=self.c_bg, fg="#a6adc8")
        desc.pack(anchor="w")

        # Container chia 3 cột chân
        pins_container = tk.Frame(self.root, bg=self.c_card, bd=1, relief="solid", highlightbackground=self.c_border)
        pins_container.pack(fill="x", padx=20, pady=4)

        # --- CỘT 1: OLED & BUTTON ---
        col1 = tk.Frame(pins_container, bg=self.c_card)
        col1.pack(side="left", fill="both", expand=True, padx=12, pady=10)

        tk.Label(col1, text="🖥️ MÀN HÌNH OLED (I2C)", font=("Segoe UI", 11, "bold"), bg=self.c_card, fg=self.c_orange).pack(anchor="w", pady=(0, 4))
        self.combo_sda = self.create_pin_row(col1, "SDA (Dữ liệu):", "3")
        self.combo_scl = self.create_pin_row(col1, "SCL (Xung nhịp):", "4")

        tk.Label(col1, text="\n🔘 NÚT BẤM (CHAT / WAKE)", font=("Segoe UI", 11, "bold"), bg=self.c_card, fg=self.c_orange).pack(anchor="w", pady=(0, 4))
        self.combo_boot = self.create_pin_row(col1, "BOOT Button:", "0")

        # --- CỘT 2: LOA MAX98357A ---
        col2 = tk.Frame(pins_container, bg=self.c_card)
        col2.pack(side="left", fill="both", expand=True, padx=12, pady=10)

        tk.Label(col2, text="🔊 LOA (I2S MAX98357A)", font=("Segoe UI", 11, "bold"), bg=self.c_card, fg=self.c_orange).pack(anchor="w", pady=(0, 4))
        self.combo_spk_din = self.create_pin_row(col2, "DIN (Data):", "5")
        self.combo_spk_bclk = self.create_pin_row(col2, "BCLK (Bit Clock):", "6")
        self.combo_spk_lrc = self.create_pin_row(col2, "LRC (L/R Clock):", "7")

        # --- CỘT 3: MIC INMP441 ---
        col3 = tk.Frame(pins_container, bg=self.c_card)
        col3.pack(side="left", fill="both", expand=True, padx=12, pady=10)

        tk.Label(col3, text="🎙️ MIC (I2S INMP441)", font=("Segoe UI", 11, "bold"), bg=self.c_card, fg=self.c_orange).pack(anchor="w", pady=(0, 4))
        self.combo_mic_ws = self.create_pin_row(col3, "WS (Word Select):", "8")
        self.combo_mic_sck = self.create_pin_row(col3, "SCK (Serial Clock):", "9")
        self.combo_mic_sd = self.create_pin_row(col3, "SD (Data Out):", "10")

        # --- THEME SELECTOR CARD ---
        theme_container = tk.Frame(self.root, bg=self.c_card, bd=1, relief="solid", highlightbackground=self.c_border)
        theme_container.pack(fill="x", padx=20, pady=4)

        t_inner = tk.Frame(theme_container, bg=self.c_card)
        t_inner.pack(fill="x", padx=15, pady=8)

        tk.Label(t_inner, text="🎭 KIỂU MẮT BIỂU CẢM:", font=("Segoe UI", 10, "bold"), bg=self.c_card, fg=self.c_accent).pack(side="left", padx=(0, 10))
        self.theme_combo = ttk.Combobox(t_inner, values=[THEME_EMO, THEME_MOCHI], width=58, state="readonly")
        self.theme_combo.set(THEME_EMO)
        self.theme_combo.pack(side="left", padx=5)

        # Port Selection & Action Bar
        action_bar = tk.Frame(self.root, bg=self.c_bg)
        action_bar.pack(fill="x", padx=20, pady=8)

        # COM Port Picker
        port_frame = tk.Frame(action_bar, bg=self.c_bg)
        port_frame.pack(side="left")
        tk.Label(port_frame, text="Cổng COM:", font=("Segoe UI", 10, "bold"), bg=self.c_bg, fg=self.c_text).pack(side="left", padx=(0, 5))
        self.port_combo = ttk.Combobox(port_frame, width=12, state="readonly")
        self.port_combo.pack(side="left", padx=2)
        btn_refresh = tk.Button(port_frame, text="🔄", font=("Segoe UI", 9), bg="#313244", fg=self.c_text, activebackground="#45475a", bd=0, padx=6, pady=2, command=self.refresh_ports)
        btn_refresh.pack(side="left", padx=4)

        # Buttons
        btn_frame = tk.Frame(action_bar, bg=self.c_bg)
        btn_frame.pack(side="right")

        self.btn_build = tk.Button(btn_frame, text="🔨 1. Build Firmware", font=("Segoe UI", 10, "bold"), bg="#45475a", fg="#ffffff", activebackground="#585b70", bd=0, padx=12, pady=6, command=lambda: self.start_action("build"))
        self.btn_build.pack(side="left", padx=5)

        self.btn_flash = tk.Button(btn_frame, text="⚡ 2. Nạp (Flash)", font=("Segoe UI", 10, "bold"), bg="#313244", fg="#ffffff", activebackground="#45475a", bd=0, padx=12, pady=6, command=lambda: self.start_action("flash"))
        self.btn_flash.pack(side="left", padx=5)

        self.btn_both = tk.Button(btn_frame, text="🚀 Build & Nạp Luôn", font=("Segoe UI", 10, "bold"), bg="#a6e3a1", fg="#11111b", activebackground="#94e2d5", bd=0, padx=14, pady=6, command=lambda: self.start_action("both"))
        self.btn_both.pack(side="left", padx=5)

        # Console Log Terminal
        log_frame = tk.Frame(self.root, bg=self.c_card, bd=1, relief="solid", highlightbackground=self.c_border)
        log_frame.pack(fill="both", expand=True, padx=20, pady=(4, 12))

        log_header = tk.Frame(log_frame, bg="#11111b")
        log_header.pack(fill="x")
        tk.Label(log_header, text="  Terminal Log Output", font=("Consolas", 9, "bold"), bg="#11111b", fg="#6c7086").pack(side="left", pady=3)
        btn_clear = tk.Button(log_header, text="Xóa Log", font=("Segoe UI", 8), bg="#181825", fg="#a6adc8", bd=0, padx=6, command=self.clear_log)
        btn_clear.pack(side="right", padx=5, pady=2)

        self.log_text = scrolledtext.ScrolledText(log_frame, bg="#11111b", fg="#a6e3a1", insertbackground="white", font=("Consolas", 9), bd=0)
        self.log_text.pack(fill="both", expand=True, padx=4, pady=4)

    def create_pin_row(self, parent, label_text, default_val):
        row = tk.Frame(parent, bg=self.c_card)
        row.pack(fill="x", pady=2)
        tk.Label(row, text=label_text, width=15, anchor="w", bg=self.c_card, fg=self.c_text).pack(side="left")
        cb = ttk.Combobox(row, values=AVAILABLE_GPIOS, width=5, state="readonly")
        cb.set(default_val)
        cb.pack(side="right")
        return cb

    def log(self, text, end="\n"):
        self.log_text.insert(tk.END, text + end)
        self.log_text.see(tk.END)

    def clear_log(self):
        self.log_text.delete("1.0", tk.END)

    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            if "COM61" in ports:
                self.port_combo.set("COM61")
            else:
                self.port_combo.set(ports[0])
            self.log(f"🔎 Đã tìm thấy {len(ports)} cổng COM: {', '.join(ports)}")
        else:
            self.port_combo.set("")
            self.log("⚠️ Không tìm thấy cổng COM nào. Vui lòng cắm cáp USB ESP32!")

    def load_current_config(self):
        if not os.path.exists(CONFIG_H_PATH):
            return
        try:
            with open(CONFIG_H_PATH, 'r', encoding='utf-8') as f:
                content = f.read()

            import re
            def get_pin(macro):
                m = re.search(rf'#define\s+{macro}\s+GPIO_NUM_([0-9A-Z_]+)', content)
                return m.group(1) if m else None

            sda = get_pin("DISPLAY_SDA_PIN")
            scl = get_pin("DISPLAY_SCL_PIN")
            spk_din = get_pin("AUDIO_I2S_SPK_GPIO_DOUT")
            spk_bclk = get_pin("AUDIO_I2S_SPK_GPIO_BCLK")
            spk_lrc = get_pin("AUDIO_I2S_SPK_GPIO_LRCK")
            mic_ws = get_pin("AUDIO_I2S_MIC_GPIO_WS")
            mic_sck = get_pin("AUDIO_I2S_MIC_GPIO_SCK")
            mic_sd = get_pin("AUDIO_I2S_MIC_GPIO_DIN")
            boot = get_pin("BOOT_BUTTON_GPIO")

            if sda: self.combo_sda.set(sda)
            if scl: self.combo_scl.set(scl)
            if spk_din: self.combo_spk_din.set(spk_din)
            if spk_bclk: self.combo_spk_bclk.set(spk_bclk)
            if spk_lrc: self.combo_spk_lrc.set(spk_lrc)
            if mic_ws: self.combo_mic_ws.set(mic_ws)
            if mic_sck: self.combo_mic_sck.set(mic_sck)
            if mic_sd: self.combo_mic_sd.set(mic_sd)
            if boot: self.combo_boot.set(boot)

            # Đọc kiểu biểu cảm
            if "CURRENT_DISPLAY_STYLE DISPLAY_STYLE_MOCHI" in content:
                self.theme_combo.set(THEME_MOCHI)
                self.log("📋 Đã tải cấu hình: Kiểu Mochi Robot (19 biểu cảm + miệng)")
            else:
                self.theme_combo.set(THEME_EMO)
                self.log("📋 Đã tải cấu hình: Kiểu EMO Robot (mắt to bồng bềnh)")

        except Exception as e:
            self.log(f"⚠️ Lỗi đọc config.h: {e}")

    def save_config(self):
        sda = self.combo_sda.get()
        scl = self.combo_scl.get()
        spk_din = self.combo_spk_din.get()
        spk_bclk = self.combo_spk_bclk.get()
        spk_lrc = self.combo_spk_lrc.get()
        mic_ws = self.combo_mic_ws.get()
        mic_sck = self.combo_mic_sck.get()
        mic_sd = self.combo_mic_sd.get()
        boot = self.combo_boot.get()

        theme_val = self.theme_combo.get()
        style_macro = "DISPLAY_STYLE_MOCHI" if "Mochi" in theme_val else "DISPLAY_STYLE_EMO"

        config_content = f"""#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_{mic_ws}
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_{mic_sck}
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_{mic_sd}
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_{spk_din}
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_{spk_bclk}
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_{spk_lrc}

#else

#define AUDIO_I2S_GPIO_WS GPIO_NUM_{mic_ws}
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_{mic_sck}
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_{mic_sd}
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_{spk_din}

#endif

#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_{boot}
#define TOUCH_BUTTON_GPIO       GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define DISPLAY_SDA_PIN GPIO_NUM_{sda}
#define DISPLAY_SCL_PIN GPIO_NUM_{scl}
#define DISPLAY_WIDTH   128

#if CONFIG_OLED_SSD1306_128X32
#define DISPLAY_HEIGHT  32
#elif CONFIG_OLED_SSD1306_128X64
#define DISPLAY_HEIGHT  64
#elif CONFIG_OLED_SH1106_128X64
#define DISPLAY_HEIGHT  64
#define SH1106
#else
#error "OLED display type is not selected"
#endif

#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true

// =====================================================================
// CẤU HÌNH GIAO DIỆN BIỂU CẢM (DISPLAY / EMOTION THEME)
// 1: EMO Robot (2 mắt to, bồng bềnh, nghiêng mắt khi nghe, không miệng)
// 2: Mochi Robot (Bộ 19 biểu cảm chi tiết + miệng uốn lượn lò xo)
// =====================================================================
#define DISPLAY_STYLE_EMO    1
#define DISPLAY_STYLE_MOCHI  2
#define CURRENT_DISPLAY_STYLE {style_macro}

#define LAMP_GPIO GPIO_NUM_18

#endif // _BOARD_CONFIG_H_
"""
        with open(CONFIG_H_PATH, 'w', encoding='utf-8') as f:
            f.write(config_content)
        self.log(f"💾 Đã lưu cấu hình: Kiểu mắt={style_macro}, OLED({sda},{scl}), Loa({spk_din},{spk_bclk},{spk_lrc}), Mic({mic_ws},{mic_sck},{mic_sd})")

    def set_buttons_state(self, state):
        self.btn_build.config(state=state)
        self.btn_flash.config(state=state)
        self.btn_both.config(state=state)

    def start_action(self, mode):
        self.set_buttons_state("disabled")
        threading.Thread(target=self.run_process, args=(mode,), daemon=True).start()

    def run_process(self, mode):
        try:
            if mode in ("build", "both"):
                self.save_config()
                self.log("\n==========================================")
                self.log("🚀 Bắt đầu Build Firmware...")
                self.log("==========================================")
                
                # Chạy ninja trong C:\xiaozhi\build
                cmd = f'powershell -Command "$env:PATH = \\"{PYTHON_DIR};$env:PATH\\"; $env:IDF_PATH = \\"{IDF_PATH}\\"; & \\"{IDF_PATH}\\export.ps1\\"; ninja -C \\"{BUILD_DIR}\\" all"'
                p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, shell=True, cwd=PROJECT_DIR)
                for line in iter(p.stdout.readline, ''):
                    if line:
                        self.log(line.strip())
                p.wait()

                if p.returncode != 0:
                    self.log("❌ Build thất bại! Vui lòng kiểm tra log bên trên.")
                    messagebox.showerror("Lỗi", "Quá trình biên dịch gặp lỗi!")
                    self.set_buttons_state("normal")
                    return

                # Gộp file All-in-one
                self.log("📦 Đang đóng gói file xiaozhi_custom_s3mini_all_in_one.bin...")
                dest_all = r"c:\Users\CMD\Desktop\esp32 super mini xiaozhi\xiaozhi_custom_s3mini_all_in_one.bin"
                dest_folder = r"c:\Users\CMD\Desktop\esp32 super mini xiaozhi\my_custom_firmware"
                
                merge_cmd = f'"{ESP_PYTHON}" -m esptool --chip esp32s3 merge_bin -o "{dest_all}" --flash_mode dio --flash_size 4MB --flash_freq 80m 0x0 "{BUILD_DIR}\\bootloader\\bootloader.bin" 0x8000 "{BUILD_DIR}\\partition_table\\partition-table.bin" 0xd000 "{BUILD_DIR}\\ota_data_initial.bin" 0x10000 "{BUILD_DIR}\\xiaozhi.bin" 0x300000 "{BUILD_DIR}\\generated_assets.bin"'
                subprocess.run(merge_cmd, shell=True, cwd=PROJECT_DIR)
                
                # Lưu riêng file bin theo từng theme
                theme_val = self.theme_combo.get()
                style_name = "mochi" if "Mochi" in theme_val else "emo"
                theme_bin_path = os.path.join(dest_folder, f"xiaozhi_custom_s3mini_{style_name}.bin")

                # Copy sang folder my_custom_firmware và firmware
                try:
                    import shutil
                    shutil.copy(f"{BUILD_DIR}\\xiaozhi.bin", f"{dest_folder}\\xiaozhi.bin")
                    shutil.copy(dest_all, f"{dest_folder}\\xiaozhi_custom_s3mini_all_in_one.bin")
                    shutil.copy(dest_all, theme_bin_path)
                    shutil.copy(f"{BUILD_DIR}\\xiaozhi.bin", rf"{PROJECT_DIR}\firmware\xiaozhi.bin")
                    shutil.copy(dest_all, rf"{PROJECT_DIR}\firmware\xiaozhi_custom_s3mini_all_in_one.bin")
                    shutil.copy(dest_all, rf"{PROJECT_DIR}\firmware\xiaozhi_custom_s3mini_{style_name}.bin")
                except:
                    pass

                self.log(f"✅ BUILD THÀNH CÔNG! Đã cập nhật bản {style_name.upper()}.")

            if mode in ("flash", "both"):
                port = self.port_combo.get()
                if not port:
                    self.log("❌ Chưa chọn cổng COM để nạp!")
                    messagebox.showwarning("Cảnh báo", "Vui lòng chọn cổng COM trước khi nạp!")
                    self.set_buttons_state("normal")
                    return

                self.log("\n==========================================")
                self.log(f"⚡ Đang nạp firmware vào {port}...")
                self.log("==========================================")

                # Tự động nạp đúng file bin của theme đã chọn (nếu có sẵn)
                dest_folder = r"c:\Users\CMD\Desktop\esp32 super mini xiaozhi\my_custom_firmware"
                theme_val = self.theme_combo.get()
                style_name = "mochi" if "Mochi" in theme_val else "emo"
                theme_bin = os.path.join(dest_folder, f"xiaozhi_custom_s3mini_{style_name}.bin")

                if os.path.exists(theme_bin):
                    flash_bin = theme_bin
                    self.log(f"🎯 Sử dụng firmware tương ứng: xiaozhi_custom_s3mini_{style_name}.bin")
                else:
                    flash_bin = r"c:\Users\CMD\Desktop\esp32 super mini xiaozhi\xiaozhi_custom_s3mini_all_in_one.bin"
                    self.log("⚠️ Chưa có bản build riêng cho theme này, dùng bản all-in-one hiện tại. Khuyên dùng nút 'Build & Nạp Luôn'!")

                flash_cmd = f'"{ESP_PYTHON}" -m esptool --port {port} --chip esp32s3 -b 921600 write_flash 0x0 "{flash_bin}"'
                p = subprocess.Popen(flash_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, shell=True, cwd=PROJECT_DIR)
                for line in iter(p.stdout.readline, ''):
                    if line:
                        self.log(line.strip())
                p.wait()

                if p.returncode == 0:
                    self.log(f"\n🎉 NẠP THÀNH CÔNG VÀO {port}!")
                    messagebox.showinfo("Thành công", f"Đã nạp thành công firmware vào {port}!")
                else:
                    self.log("❌ Nạp thất bại! Hãy kiểm tra kết nối cáp USB hoặc bấm giữ BOOT khi nạp.")
                    messagebox.showerror("Lỗi", "Nạp firmware không thành công!")

        except Exception as ex:
            self.log(f"❌ Lỗi ngoại lệ: {ex}")
            messagebox.showerror("Lỗi", str(ex))
        finally:
            self.set_buttons_state("normal")

if __name__ == "__main__":
    root = tk.Tk()
    app = XiaozhiToolApp(root)
    root.mainloop()
