#pragma once

// =====================================================================
// EmoDisplay - Phong cách robot EMO (LivingAI) tối giản & sinh động
// Dành riêng cho màn hình OLED 128x64 trên ESP32-S3.
//
// ĐẶC ĐIỂM:
// - KHÔNG CẦN MIỆNG (đúng chuẩn robot EMO): dành trọn không gian cho
//   2 mắt to tròn, sắc nét (40x36px bo góc 12px), nổi bật trên nền đen.
// - SIÊU NHẸ: Chỉ quản lý 2 mắt và các nắp mí che góc, chạy 25 FPS
//   êm ái không nghẽn I2C / không tốn RAM.
// - HIỆU ỨNG EMO:
//   + Nhịp thở bồng bềnh (Idle Floating Bobbing) lên xuống nhẹ nhàng.
//   + Tự động chớp mắt ngẫu nhiên (Auto-blink).
//   + Lắng nghe (Listening): Mắt mở to, hơi ngước và nghiêng tò mò.
//   + Đang nói (Speaking): Mắt nảy nhấp nhô theo nhịp nói cực cute.
//   + Hỗ trợ đầy đủ 19+ biểu cảm cảm xúc chuẩn của Xiaozhi.
// =====================================================================

#include "display/oled_display.h"
#include <lvgl.h>
#include <string>
#include <cstdint>

enum class EmoState {
    kIdle,          // Bình thường / Chờ
    kListening,     // Đang lắng nghe (nghiêng mắt tò mò)
    kSpeaking,      // Đang nói (nhấp nhô nhịp nhàng)
    kThinking,      // Đang suy nghĩ (đảo mắt sang bên)
    kHappy,         // Vui vẻ (mắt vầng trăng khuyết ^_^)
    kLaughing,      // Cười tít mắt
    kSad,           // Buồn bã (mắt cụp xuống)
    kCrying,        // Khóc
    kAngry,         // Giận dữ (mắt xếch vào trong)
    kSurprised,     // Bất ngờ (mắt mở to tròn xoe)
    kLoving,        // Yêu thương / Trái tim
    kEmbarrassed,   // Ngại ngùng
    kConfused,      // Bối rối (mắt lệch bất đối xứng)
    kCool,          // Ngầu / Tự tin
    kWinking,       // Nháy mắt
    kSleepy,        // Buồn ngủ (mắt híp mỏng)
    kWarning,       // Cảnh báo
    kDisgusted,     // Khó chịu
    kCurious        // Tò mò
};

// Quirk ngẫu nhiên khi ở Idle (liếc nhìn xung quanh)
enum class EmoQuirk {
    kNone,
    kBlink,
    kLookLeft,
    kLookRight,
    kLookUp,
    kCuriousTilt
};

class EmoDisplay : public OledDisplay {
public:
    EmoDisplay(esp_lcd_panel_io_handle_t io_handle,
               esp_lcd_panel_handle_t panel_handle,
               int width, int height,
               bool mirror_x, bool mirror_y);
    ~EmoDisplay() override;

    void SetupUI() override;
    bool SetEmotion(const char* emotion) override;
    void SetStatus(const char* status) override;

private:
    void BuildFace();
    void ApplyState(EmoState state);
    void ScheduleNextBlink();
    void ScheduleNextQuirk();
    static void OnAnimTick(lv_timer_t* timer);
    void UpdateAnimation();

    EmoState current_state_ = EmoState::kIdle;
    EmoState target_state_  = EmoState::kIdle;

    // Kích thước mắt chuẩn
    int base_w_ = 40;
    int base_h_ = 36;
    int base_radius_ = 12;
    int eye_gap_ = 14;
    int center_y_ = 14;

    // LVGL Objects
    lv_obj_t* face_root_ = nullptr;
    lv_obj_t* left_eye_  = nullptr;
    lv_obj_t* right_eye_ = nullptr;

    // Mí mắt che góc tạo biểu cảm (Happy, Sad, Angry)
    lv_obj_t* left_cut_bottom_  = nullptr;
    lv_obj_t* right_cut_bottom_ = nullptr;
    lv_obj_t* left_cut_top_     = nullptr;
    lv_obj_t* right_cut_top_    = nullptr;

    // Animation variables
    lv_timer_t* anim_timer_ = nullptr;
    uint32_t tick_count_ = 0;

    // Biến vị trí & kích thước hiện tại (interpolation)
    float cur_w_l_ = 40.0f, cur_h_l_ = 36.0f, cur_y_l_ = 14.0f, cur_x_l_ = 17.0f;
    float cur_w_r_ = 40.0f, cur_h_r_ = 36.0f, cur_y_r_ = 14.0f, cur_x_r_ = 71.0f;
    float cur_cut_bottom_ = 0.0f; // 0 (không che) -> 1.0 (che hình cười)
    float cur_cut_top_    = 0.0f;

    // Mục tiêu (target)
    float tgt_w_l_ = 40.0f, tgt_h_l_ = 36.0f, tgt_y_l_ = 14.0f, tgt_x_l_ = 17.0f;
    float tgt_w_r_ = 40.0f, tgt_h_r_ = 36.0f, tgt_y_r_ = 14.0f, tgt_x_r_ = 71.0f;
    float tgt_cut_bottom_ = 0.0f;
    float tgt_cut_top_    = 0.0f;

    // Quirk & Blink timers
    uint32_t next_blink_tick_ = 0;
    uint32_t blink_end_tick_  = 0;
    bool is_blinking_ = false;

    uint32_t next_quirk_tick_ = 0;
    uint32_t quirk_end_tick_  = 0;
    EmoQuirk current_quirk_ = EmoQuirk::kNone;
};
