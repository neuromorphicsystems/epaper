import epaper

with epaper.open('/dev/tty.usbserial-AL05Y1QR') as display:
    display.show('example.png')
