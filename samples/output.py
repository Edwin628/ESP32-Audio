import serial
import time

# setup
serial_port = 'COM5' 
baud_rate = 115200
output_file_path = 'output.txt' 

print("Serial Output Python App")

try:
    # open serial
    with serial.Serial(serial_port, baud_rate, timeout=1) as ser, open(output_file_path, 'w') as outfile:
        print("start recording '{}'".format(output_file_path))
        while True:
            data = ser.readline().decode('utf-8', errors='replace')
            if data:
                outfile.write(data)
                outfile.flush() 
                print(data, end='') 

except serial.serialutil.SerialException:
    print("cannot open serial")
except KeyboardInterrupt:
    print("stop recording")
except Exception as e:
    print("errors occurs", str(e))