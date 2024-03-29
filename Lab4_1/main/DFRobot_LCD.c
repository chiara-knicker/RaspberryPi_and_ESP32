#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/i2c.h>
#include "esp_log.h"

#include "i2c_api.h"
#include "DFRobot_LCD.h"

const uint8_t color_define[4][3] = 
{
    {255, 255, 255},            // white
    {255, 0, 0},                // red
    {0, 255, 0},                // green
    {0, 0, 255},                // blue
};

uint8_t _showfunction;
uint8_t _showcontrol;
uint8_t _showmode;
uint8_t _initialized;
uint8_t _numlines,_currline;
uint8_t _lcdAddr = LCD_ADDRESS;
uint8_t _RGBAddr = RGB_ADDRESS;
uint8_t _cols = 16;
uint8_t _rows = 2;
uint8_t _backlightval;

void init()
{
	esp_err_t err = i2c_master_init(); //Wire.begin();
	_showfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
	begin(_cols, _rows, LCD_5x8DOTS);

	if (err != ESP_OK) {
	  printf("Failed to i2c_param_config %d", err);
  	}
}

void clear()
{
    command(LCD_CLEARDISPLAY);        // clear display, set cursor position to zero
    vTaskDelay(pdMS_TO_TICKS(2));     // this command takes a long time!
}

void home()
{
    command(LCD_RETURNHOME);        // set cursor position to zero
    vTaskDelay(pdMS_TO_TICKS(2));   // this command takes a long time!
}

void noDisplay()
{
    _showcontrol &= ~LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | _showcontrol);
}

void display() {
    _showcontrol |= LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | _showcontrol);
}

void stopBlink()
{
    _showcontrol &= ~LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | _showcontrol);
}
void blink()
{
    _showcontrol |= LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | _showcontrol);
}

void noCursor()
{
    _showcontrol &= ~LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | _showcontrol);
}

void cursor() {
    _showcontrol |= LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | _showcontrol);
}

void scrollDisplayLeft(void)
{
    command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

void scrollDisplayRight(void)
{
    command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

void leftToRight(void)
{
    _showmode |= LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | _showmode);
}

void rightToLeft(void)
{
    _showmode &= ~LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | _showmode);
}

void noAutoscroll(void)
{
    _showmode &= ~LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | _showmode);
}

void autoscroll(void)
{
    _showmode |= LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | _showmode);
}

void customSymbol(uint8_t location, uint8_t charmap[])
{
    location &= 0x7; // we only have 8 locations 0-7
    command(LCD_SETCGRAMADDR | (location << 3));
    
    uint8_t data[9];
    data[0] = 0x40;
    for(int i=0; i<8; i++)
    {
        data[i+1] = charmap[i];
    }
    send(data, 9);
}

void setCursor(uint8_t col, uint8_t row)
{
    col = (row == 0 ? col|0x80 : col|0xc0);
    uint8_t data[3] = {0x80, col};

    send(data, 2);
}

void setRGB(uint8_t r, uint8_t g, uint8_t b)
{
    setReg(REG_RED, r);
    setReg(REG_GREEN, g);
    setReg(REG_BLUE, b);
}

void setPWM(uint8_t color, uint8_t pwm)
{
    setReg(color, pwm);
}

void setColor(uint8_t color)
{
    if(color > 3)return ;
    setRGB(color_define[color][0], color_define[color][1], color_define[color][2]);
}

void setColorAll()
{
    setRGB(0, 0, 0);
}

void setColorWhite()
{
    setRGB(255, 255, 255);
}

void blinkLED(void)
{
    ///< blink period in seconds = (<reg 7> + 1) / 24
    ///< on/off ratio = <reg 6> / 256
    setReg(0x07, 0x17);  // blink every second
    setReg(0x06, 0x7f);  // half on, half off
}

void noBlinkLED(void)
{
    setReg(0x07, 0x00);
    setReg(0x06, 0xff);
}

inline void write(uint8_t value)
{
    uint8_t data[3] = {0x40, value};
    send(data, 2);
}

inline void command(uint8_t value)
{
    uint8_t data[3] = {0x80, value};
    send(data, 2);
}

void blink_on()
{
	blink();
}

void blink_off()
{
	stopBlink();
}

void cursor_on()
{
	cursor();
}

void cursor_off()
{
	noCursor();
}

void setBacklight(uint8_t new_val)
{
	if(new_val){
		blinkLED();		// turn backlight on
	}else{
		noBlinkLED();	// turn backlight off
	}
}

void load_custom_character(uint8_t char_num, uint8_t *rows)
{
	customSymbol(char_num, rows);
}

void print(const char c[])
{
	while (*c) {
      	write(*c++);
  	}
}

void printstr(const char c[])
{
	print(c);
}

/*******************************private*******************************/
void begin(uint8_t cols, uint8_t lines, uint8_t dotsize) 
{
    if (lines > 1) {
        _showfunction |= LCD_2LINE;
    }
    _numlines = lines;
    _currline = 0;

    ///< for some 1 line displays you can select a 10 pixel high font
    if ((dotsize != 0) && (lines == 1)) {
        _showfunction |= LCD_5x10DOTS;
    }

    ///< SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
    ///< according to datasheet, we need at least 40ms after power rises above 2.7V
    ///< before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
    vTaskDelay(pdMS_TO_TICKS(50)); //delay(50);

    ///< this is according to the hitachi HD44780 datasheet (page 45 figure 23)

    ///< Send function set command sequence
    command(LCD_FUNCTIONSET | _showfunction);
    vTaskDelay(pdMS_TO_TICKS(5)); //delay(5);  // wait more than 4.1ms
	
    ///< second try
    command(LCD_FUNCTIONSET | _showfunction);
    vTaskDelay(pdMS_TO_TICKS(5)); //delay(5);

    ///< third go
    command(LCD_FUNCTIONSET | _showfunction);

    ///< turn the display on with no cursor or blinking default
    _showcontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    display();

    ///< clear it off
    clear();

    ///< Initialize to default text direction (for romance languages)
    _showmode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    ///< set the entry mode
    command(LCD_ENTRYMODESET | _showmode);
    
    ///< backlight init
    setReg(REG_MODE1, 0);
    ///< set LEDs controllable by both PWM and GRPPWM registers
    setReg(REG_OUTPUT, 0xFF);
    ///< set MODE2 values
    ///< 0010 0000 -> 0x20  (DMBLNK to 1, ie blinky mode)
    setReg(REG_MODE2, 0x20);
    
    setColorWhite();
}

void send(uint8_t *data, uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_lcdAddr << 1) | I2C_MASTER_WRITE, true);

    for(int i=0; i<len; i++) {
        i2c_master_write_byte(cmd, data[i], true);
	vTaskDelay(pdMS_TO_TICKS(5)); //delay(5);
    }

    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
	  printf("Failed to send data %d\n", err);
    }
}

void setReg(uint8_t addr, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_RGBAddr << 1) | I2C_MASTER_WRITE, true);

    //i2c_master_write_byte(cmd, 0b10000000, true); // Control Byte
    i2c_master_write_byte(cmd, addr, true);
    //i2c_master_write_byte(cmd, 0b00000000, true); // Control Byte
    i2c_master_write_byte(cmd, data, true);

    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
	  printf("Failed to setReg %d\n", err);
    }
}
