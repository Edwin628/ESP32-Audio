import matplotlib.pyplot as plt

file_path = '../samples/samples.txt'

data = {}

with open(file_path, 'r') as file:
    for line in file:
        # space and /n
        line = line.strip()
        # divide 'Sample' and numeric
        sample_info = line.split(': ')
        # sample number and value
        sample_number = int(sample_info[0].split(' ')[1])
        sample_value = int(sample_info[1])
        # add them to the dic
        data[sample_number] = sample_value

# Transfer the format
samples = list(data.keys())
values = list(data.values())

# Create Figure
plt.figure(figsize=(12, 6))
plt.plot(samples, values, label='Sample Data')
plt.title('Sample Data Visualization')
plt.xlabel('Sample Number')
plt.ylabel('Value')
plt.grid(True)
plt.legend()
plt.show()