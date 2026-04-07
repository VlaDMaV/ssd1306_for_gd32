// ================================================
//   RADAR / METAL DETECTOR display on SSD1306
//   ADC: PA0 (channel 0) — DC voltage input
// ================================================

#include "gd32f30x.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_i2c.h"
#include "gd32f30x_adc.h"
#include "system_gd32f30x.h"
#include <stdlib.h>
#include <string.h>

#define OLED_ADDR 0x3C


volatile uint32_t msTicks = 0;
void SysTick_Handler(void) { msTicks++; }

void delay_ms(uint32_t ms)
{
    uint32_t start = msTicks;
    while ((msTicks - start) < ms);
}

/* ===================== ADC ===================== */
void adc_config(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_ADC0);
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV8);

    /* PA0 as analog input */
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, GPIO_PIN_0);

    adc_deinit(ADC0);
    adc_mode_config(ADC_MODE_FREE);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, DISABLE);
    adc_special_function_config(ADC0, ADC_SCAN_MODE,       DISABLE);
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);
    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 1);
    adc_regular_channel_config(ADC0, 0, ADC_CHANNEL_0, ADC_SAMPLETIME_55POINT5);
    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL,
                                       ADC0_1_2_EXTTRIG_REGULAR_NONE);
    adc_external_trigger_config(ADC0, ADC_REGULAR_CHANNEL, ENABLE);

    adc_enable(ADC0);
    delay_ms(1);
    adc_calibration_enable(ADC0);
}

uint16_t adc_read(void)
{
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
    while (!adc_flag_get(ADC0, ADC_FLAG_EOC));
    return adc_regular_data_read(ADC0);
}

/* ===================== I2C ===================== */
void i2c_config(void)
{
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_I2C0);
    gpio_init(GPIOB, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_6 | GPIO_PIN_7);
    i2c_clock_config(I2C0, 400000, I2C_DTCY_2);
    i2c_enable(I2C0);
}

static void OLED_Cmd(uint8_t cmd)
{
    while(i2c_flag_get(I2C0, I2C_FLAG_I2CBSY));
    i2c_start_on_bus(I2C0);
    while(!i2c_flag_get(I2C0, I2C_FLAG_SBSEND));
    i2c_master_addressing(I2C0, OLED_ADDR << 1, I2C_TRANSMITTER);
    while(!i2c_flag_get(I2C0, I2C_FLAG_ADDSEND));
    i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);
    i2c_data_transmit(I2C0, 0x00); while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));
    i2c_data_transmit(I2C0, cmd);  while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));
    i2c_stop_on_bus(I2C0);
}

static void OLED_DataBlock(uint8_t page, const uint8_t* data, uint8_t len)
{
    OLED_Cmd(0xB0 + page);
    OLED_Cmd(0x00);
    OLED_Cmd(0x10);
    while(i2c_flag_get(I2C0, I2C_FLAG_I2CBSY));
    i2c_start_on_bus(I2C0);
    while(!i2c_flag_get(I2C0, I2C_FLAG_SBSEND));
    i2c_master_addressing(I2C0, OLED_ADDR << 1, I2C_TRANSMITTER);
    while(!i2c_flag_get(I2C0, I2C_FLAG_ADDSEND));
    i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);
    i2c_data_transmit(I2C0, 0x40); while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));
    for(uint8_t i = 0; i < len; i++) {
        i2c_data_transmit(I2C0, data[i]);
        while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));
    }
    i2c_stop_on_bus(I2C0);
}

static uint8_t oled_buffer[1024];

static void OLED_ClearBuffer(void) { memset(oled_buffer, 0, sizeof(oled_buffer)); }

static void OLED_Update(void)
{
    for (uint8_t page = 0; page < 8; page++)
        OLED_DataBlock(page, oled_buffer + page*128, 128);
}

static void OLED_Pixel(uint8_t x, uint8_t y)
{
    if (x >= 128 || y >= 64) return;
    oled_buffer[(y/8)*128 + x] |= (1 << (y%8));
}

void OLED_Init(void)
{
    delay_ms(100);
    OLED_Cmd(0xAE);
    OLED_Cmd(0x20); OLED_Cmd(0x10);
    OLED_Cmd(0xB0);
    OLED_Cmd(0xC8);
    OLED_Cmd(0x00);
    OLED_Cmd(0x10);
    OLED_Cmd(0x40);
    OLED_Cmd(0x81); OLED_Cmd(0xFF);
    OLED_Cmd(0xA1);
    OLED_Cmd(0xA6);
    OLED_Cmd(0xA8); OLED_Cmd(0x3F);
    OLED_Cmd(0xA4);
    OLED_Cmd(0xD3); OLED_Cmd(0x00);
    OLED_Cmd(0xD5); OLED_Cmd(0xF0);
    OLED_Cmd(0xD9); OLED_Cmd(0x22);
    OLED_Cmd(0xDA); OLED_Cmd(0x12);
    OLED_Cmd(0xDB); OLED_Cmd(0x20);
    OLED_Cmd(0x8D); OLED_Cmd(0x14);
    OLED_Cmd(0xAF);
}

/* ===================== RADAR DRAWING ===================== */

#define RADAR_CX    64
#define RADAR_CY    63
#define RADAR_MAX_R 55

static void draw_semicircle(int cx, int cy, int r)
{
    int x = 0, y = r;
    int d = 3 - 2 * r;
    while (x <= y) {
        if (cy - y >= 0) {
            OLED_Pixel(cx + x, cy - y);
            OLED_Pixel(cx - x, cy - y);
        }
        if (cy - x >= 0) {
            OLED_Pixel(cx + y, cy - x);
            OLED_Pixel(cx - y, cy - x);
        }
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

void draw_radar(uint32_t frame, uint16_t adc_val)
{
    OLED_ClearBuffer();

    /* active_r — максимальный радиус, растёт линейно с сигналом (1..RADAR_MAX_R) */
    int active_r = 1 + (int)((uint32_t)adc_val * (RADAR_MAX_R - 1) / 4095);

    /* Статичные кольца — только те, что вписываются в active_r */
    const int radii[] = {15, 25, 35, 45, 55};
    for (int i = 0; i < 5; i++)
        if (radii[i] <= active_r)
            draw_semicircle(RADAR_CX, RADAR_CY, radii[i]);

    /* Горизонтальная линия основания */
    for (int x = RADAR_CX - active_r; x <= RADAR_CX + active_r; x++)
        OLED_Pixel(x, RADAR_CY);

    /* Вертикальная пунктирная линия */
    for (int y = RADAR_CY - active_r; y <= RADAR_CY; y += 2)
        OLED_Pixel(RADAR_CX, y);

    /* Расширяющееся кольцо — до active_r */
    int ring_r = frame % (active_r + 1);
    for (int d = -2; d <= 2; d++) {
        int r = ring_r + d;
        if (r >= 1 && r <= active_r)
            draw_semicircle(RADAR_CX, RADAR_CY, r);
    }

    OLED_Update();
}

int main(void)
{
    SystemInit();
    SysTick_Config(SystemCoreClock / 1000);
    i2c_config();
    adc_config();
    OLED_Init();

    uint32_t frame = 0;

    while (1)
    {
        uint16_t adc_val = adc_read();
        draw_radar(frame, adc_val);
        frame++;
        delay_ms(16);
    }
}
