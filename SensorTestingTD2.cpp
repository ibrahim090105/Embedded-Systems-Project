#include "mbed.h"
#include "C12832.h"
// import http://os.mbed.com/users/askksa12543/code/C12832/#990d5eec2ef6738058baf82c579a0ed07c8fc151
// ================= 硬件管脚映射 =================
// LCD 屏幕
C12832 lcd(D11, D13, D12, D7, D10);

// 摇杆 (使用硬件中断，完美避免屏幕刷新导致的按键丢失)
// 扩展板摇杆引脚: Left=A4(PC_1), Right=A5(PC_0), Center=D4(PB_5)
InterruptIn joy_left(A4);
InterruptIn joy_right(A5);
InterruptIn joy_center(D4);

// 5路传感器 模拟输入 
AnalogIn adc[5] = {PC_2, PC_3, PC_4, PC_5, PB_1};

// 5路传感器 数字输出 
DigitalOut dout[5] = {PC_8, PC_6, PC_9, PB_12, PB_13};

// ================= 状态变量 =================
volatile int current_view = 0;   // 0-4: CH1-CH5, 5: 全局开关, 6: 自动轮询
volatile bool ch_enable[5] = {false, false, false, false, false};
volatile bool global_enable = false;
volatile bool polling_enable = false;

// 中断消抖使用的时间戳
volatile uint32_t last_irq_time = 0;

// ================= 中断服务函数 (ISR) =================
// 注意：中断里只改状态变量，绝对不要放 printf 或 LCD 刷新！

void on_joy_left() {
    uint32_t now = us_ticker_read();
    if (now - last_irq_time > 200000) { // 200ms 软件消抖
        current_view = (current_view == 0) ? 6 : current_view - 1;
        last_irq_time = now;
    }
}

void on_joy_right() {
    uint32_t now = us_ticker_read();
    if (now - last_irq_time > 200000) { // 200ms 软件消抖
        current_view = (current_view == 6) ? 0 : current_view + 1;
        last_irq_time = now;
    }
}

void on_joy_center() {
    uint32_t now = us_ticker_read();
    if (now - last_irq_time > 200000) { // 200ms 软件消抖
        if (current_view >= 0 && current_view <= 4) {
            // 单通道开关模式
            ch_enable[current_view] = !ch_enable[current_view];
        } 
        else if (current_view == 5) {
            // 全局开关模式
            global_enable = !global_enable;
            for(int i = 0; i < 5; i++) ch_enable[i] = global_enable;
        } 
        else if (current_view == 6) {
            // 自动轮询开关模式
            polling_enable = !polling_enable;
        }
        last_irq_time = now;
    }
}

// ================= 主函数 =================
int main() {
    // 扩展板摇杆按下时接地，所以必须开启内部上拉电阻
    joy_left.mode(PullUp);
    joy_right.mode(PullUp);
    joy_center.mode(PullUp);

    // 绑定下降沿触发中断 (按下摇杆的瞬间)
    joy_left.fall(&on_joy_left);
    joy_right.fall(&on_joy_right);
    joy_center.fall(&on_joy_center);

    float adc_vals[5] = {0.0f};

    while(1) {
        // ---------------------------------------------------------
        // 1. 业务逻辑与 ADC 采样层 (即使 Disable 也会读取 ADC)
        // ---------------------------------------------------------
        if (current_view == 6 && polling_enable) {
            // 【轮询模式】：防止红外光相互干扰的高级模式
            for(int i=0; i<5; i++) dout[i] = 0; 
            
            for(int i=0; i<5; i++) {
                dout[i] = 1;         // 打开当前通道发射端
                wait_us(500);        // 等待 0.5ms 让光敏元件电压爬升稳定
                adc_vals[i] = adc[i].read(); // 读取电压
                dout[i] = 0;         // 读完立刻关闭
            }
        } 
        else {
            // 【常规模式】：根据使能状态开启发射端，然后统一读取
            for(int i=0; i<5; i++) {
                dout[i] = ch_enable[i]; // 按当前状态驱动 ULN2003
            }
            wait_us(500); // 给电路一点稳定时间
            for(int i=0; i<5; i++) {
                adc_vals[i] = adc[i].read(); // 就算 Disable 没亮灯，ADC照读不误
            }
        }

        // ---------------------------------------------------------
        // 2. UI 渲染层 (加入了 f 后缀，完美解决隐式类型转换警告)
        // ---------------------------------------------------------
        lcd.cls();
        
        if (current_view >= 0 && current_view <= 4) {
            // 视图：单通道详细信息
            lcd.locate(0, 0);
            lcd.printf("Mode:[ CH%d ] ST: %s", current_view + 1, ch_enable[current_view] ? "ON" : "OFF");
            lcd.locate(0, 12);
            lcd.printf("Analog In : %1.3f V", adc_vals[current_view] * 3.3f);
            lcd.locate(0, 22);
            lcd.printf("Raw Output: %0.4f", adc_vals[current_view]);
        } 
        else if (current_view == 5) {
            // 视图：全局总览 (压缩显示 5 个通道的数据)
            lcd.locate(0, 0);
            lcd.printf("Mode: [GLOBAL] ST:%s", global_enable ? "ON" : "OFF");
            lcd.locate(0, 12);
            lcd.printf("1:%1.1fV 2:%1.1fV 3:%1.1fV", adc_vals[0]*3.3f, adc_vals[1]*3.3f, adc_vals[2]*3.3f);
            lcd.locate(0, 22);
            lcd.printf("4:%1.1fV 5:%1.1fV", adc_vals[3]*3.3f, adc_vals[4]*3.3f);
        } 
        else if (current_view == 6) {
            // 视图：轮询总览
            lcd.locate(0, 0);
            lcd.printf("Mode: [ AUTO ] ST:%s", polling_enable ? "ON" : "OFF");
            lcd.locate(0, 12);
            lcd.printf("1:%1.1fV 2:%1.1fV 3:%1.1fV", adc_vals[0]*3.3f, adc_vals[1]*3.3f, adc_vals[2]*3.3f);
            lcd.locate(0, 22);
            lcd.printf("4:%1.1fV 5:%1.1fV (Poll)", adc_vals[3]*3.3f, adc_vals[4]*3.3f);
        }

        // ---------------------------------------------------------
        // 3. 精确延时 (修改为经典 Mbed 延时语法)
        // ---------------------------------------------------------
        wait_ms(100);  // 100毫秒延时，维持 10Hz 屏幕刷新率
    }
}
