from RPLCD.i2c import CharLCD
import sys
import time

lcd = CharLCD('PCF8574', 0x27)

lcd.clear()
lcd.write_string(sys.argv[1])