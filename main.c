// ================================================
//   Эльбрус — Радар из STALCRAFT:X
//   Волна повторяет окружность и идёт плавно вверх
// ================================================

#include "gd32f30x.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_gpio.h"
#include "gd32f30x_i2c.h"
#include "system_gd32f30x.h"
#include <stdlib.h>

#define OLED_ADDR 0x3C

volatile uint32_t msTicks = 0;
void SysTick_Handler(void) { msTicks++; }

void delay_ms(uint32_t ms)
{
    uint32_t start = msTicks;
    while ((msTicks - start) < ms);
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
    i2c_data_transmit(I2C0, cmd); while(!i2c_flag_get(I2C0, I2C_FLAG_TBE));
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

static void OLED_ClearBuffer(void)
{
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

static void OLED_Update(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        OLED_DataBlock(page, oled_buffer + page*128, 128);
    }
}

static void OLED_Pixel(uint8_t x, uint8_t y)
{
    if (x >= 128 || y >= 64) return;
    oled_buffer[(y/8)*128 + x] |= (1 << (y%8));
}

/* OLED Init */
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

/* ===================== Эльбрус Радар ===================== */
void draw_elbrus(uint32_t frame)
{
    OLED_ClearBuffer();

    // Надпись "ЭЛЬБРУС"
    const char* title = "ELBRUS";
    uint8_t tx = 35;
    for (int i = 0; title[i]; i++) {
        for (int j = 0; j < 5; j++) OLED_Pixel(tx + j, 3);
        tx += 7;
    }

    // Концентрические полукруглые линии сетки
    for (int r = 15; r <= 47; r += 7) {
        for (int a = -88; a <= 88; a += 3) {
            int x = 64 + (int)(r * a / 95);
            int y = 55 - (int)(r * (95 - (a < 0 ? -a : a)) / 95);
            OLED_Pixel(x, y);
        }
    }

    // Яркая волна, которая повторяет форму окружности и идёт от центра вверх
    uint32_t wave_phase = frame * 2;                    // скорость движения волны

    for (int a = -82; a <= 82; a += 2) {
        int radius = 42;
        int x = 64 + (int)(radius * a / 92);

        // Волна начинается от центра и поднимается вверх по дуге
        int center_y = 52;
        int wave_offset = (wave_phase % 140);
        int y = center_y - (int)(wave_offset * 0.75) - (a * a / 220);

        if (y >= 8 && y <= 52) {
            OLED_Pixel(x, y);
            OLED_Pixel(x - 1, y);      // толщина волны
            OLED_Pixel(x + 1, y);
        }
    }

    // Центральная вертикальная линия
    for (int y = 54; y >= 12; y -= 2) {
        OLED_Pixel(64, y);
    }

    OLED_Update();
}

int main(void)
{
    SystemInit();
    SysTick_Config(SystemCoreClock / 1000);
    i2c_config();
    OLED_Init();

    uint32_t frame = 0;

    while (1)
    {
        draw_elbrus(frame);
        frame++;

        delay_ms(28);        // плавность ~35 fps
    }
}