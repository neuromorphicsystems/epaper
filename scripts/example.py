import epaper
import numpy

device = '/dev/ttyACM0'

# show a single image
print('epaper.show')
epaper.show(device, 'example.png')

# show multiple images (faster than multiple calls to epaper.show)
with epaper.open(device) as display:
    print('display.show')
    display.show('example.png')
    print('display.show')
    display.show('example.png')

# use a numpy array
with epaper.open(device) as display:
    frame = numpy.zeros((epaper.height, epaper.width), dtype=numpy.uint8)
    frame[1::3,::] = 255
    frame[2::3,::] = 128
    print('display.send')
    display.send(frame)
