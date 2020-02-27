import numpy
import serial

width = 1304
height = 984

class open:
    def __init__(self, device):
        self.device = device
        self.begins = tuple(numpy.cumsum((
            0,
            492 * 81,
            492 * 81,
            492 * 82,
            492 * 82,
            492 * 82,
            492 * 82,
            492 * 81,
            492 * 81)))
    def __enter__(self):
        self.arduino = serial.Serial(self.device, baudrate=2000000)
        if self.arduino.read()[0] != ord('r'):
            self.arduino.close()
            raise Exception('the Arduino initialization failed')
        return self
    def __exit__(self, type, value, traceback):
        self.arduino.write(b's')
        if self.arduino.read()[0] != ord('s'):
            self.arduino.close()
            raise Exception('the display has not entered sleep mode')
        self.arduino.close()
    def send(self, frame):
        """
        Sends a frame to the ePaper display, and waits until the frame is displayed.
        Grey level values between 32 and 222 are shown with red ink.
        """
        assert type(frame) is numpy.ndarray
        assert frame.dtype is numpy.dtype('uint8')
        assert frame.shape == (height, width)
        frame = numpy.copy(frame)
        frame[frame < 32] = 0
        frame[frame > 222] = 2
        frame[frame > 2] = 3
        packed_frame = numpy.zeros(width * height // 4, dtype=numpy.uint8)
        lsb_frame = frame & 0b1
        msb_frame = frame >> 1
        for bit in range(0, 8):
            packed_frame[self.begins[0]:self.begins[1]] = numpy.bitwise_or(
                packed_frame[self.begins[0]:self.begins[1]], (msb_frame[492:height,bit:648:8] << (7 - bit)).flatten())
            packed_frame[self.begins[1]:self.begins[2]] = numpy.bitwise_or(
                packed_frame[self.begins[1]:self.begins[2]], (lsb_frame[492:height,bit:648:8] << (7 - bit)).flatten())
            packed_frame[self.begins[2]:self.begins[3]] = numpy.bitwise_or(
                packed_frame[self.begins[2]:self.begins[3]], (msb_frame[492:height,(648 + bit):width:8] << (7 - bit)).flatten())
            packed_frame[self.begins[3]:self.begins[4]] = numpy.bitwise_or(
                packed_frame[self.begins[3]:self.begins[4]], (lsb_frame[492:height,(648 + bit):width:8] << (7 - bit)).flatten())
            packed_frame[self.begins[4]:self.begins[5]] = numpy.bitwise_or(
                packed_frame[self.begins[4]:self.begins[5]], (msb_frame[0:492,(648 + bit):width:8] << (7 - bit)).flatten())
            packed_frame[self.begins[5]:self.begins[6]] = numpy.bitwise_or(
                packed_frame[self.begins[5]:self.begins[6]], (lsb_frame[0:492,(648 + bit):width:8] << (7 - bit)).flatten())
            packed_frame[self.begins[6]:self.begins[7]] = numpy.bitwise_or(
                packed_frame[self.begins[6]:self.begins[7]], (msb_frame[0:492,bit:648:8] << (7 - bit)).flatten())
            packed_frame[self.begins[7]:self.begins[8]] = numpy.bitwise_or(
                packed_frame[self.begins[7]:self.begins[8]], (lsb_frame[0:492,bit:648:8] << (7 - bit)).flatten())
        self.arduino.write(b'r')
        self.arduino.write(packed_frame.tobytes())
        if self.arduino.read()[0] != ord('r'):
            self.arduino.close()
            raise Exception('the frame was not acknowledged')
    def show(self, filename):
        """
        Load and show the given image.
        Color images are converted to grey levels.
        """
        import PIL.Image
        self.send(numpy.asarray(PIL.Image.open(filename).convert('L')))

def show(device, filename):
    with open(filename) as display:
        display.show(filename)
