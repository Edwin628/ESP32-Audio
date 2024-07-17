from heapq import heappush, heappop, heapify
import json

# Class representing a node in the Huffman tree
class HuffmanNode:
    def __init__(self, char, freq):
        self.char = char
        self.freq = freq
        self.left = None
        self.right = None

    def __lt__(self, other):
        return self.freq < other.freq

# Function to build the Huffman tree
def build_huffman_tree(frequency):
    heap = [HuffmanNode(char, freq) for char, freq in frequency.items()]
    heapify(heap)

    while len(heap) > 1:
        node1 = heappop(heap)
        node2 = heappop(heap)
        merged = HuffmanNode(None, node1.freq + node2.freq)
        merged.left = node1
        merged.right = node2
        heappush(heap, merged)

    return heap[0]

# Function to generate Huffman codes from the Huffman tree
def generate_huffman_codes(node, prefix="", codebook=None):
    if codebook is None:
        codebook = {}

    if node.char is not None:
        codebook[node.char] = prefix
    else:
        generate_huffman_codes(node.left, prefix + "0", codebook)
        generate_huffman_codes(node.right, prefix + "1", codebook)

    return codebook

# Function to read frequencies from a file
def read_frequencies_from_file(filename):
    frequency = {}
    with open(filename, 'r') as file:
        for line in file:
            parts = line.split()
            if len(parts) == 2:
                char = parts[0]
                freq = float(parts[1])
                frequency[char] = freq
    return frequency

def main():
    filename = 'frequencies.txt'  # The frequency file
    frequency = read_frequencies_from_file(filename)

    huffman_tree = build_huffman_tree(frequency)
    huffman_codebook = generate_huffman_codes(huffman_tree)

    print("Huffman Codes:", huffman_codebook)

    # Save the generated Huffman codebook as a JSON file
    with open('huffman_codebook.json', 'w') as f:
        json.dump(huffman_codebook, f)

if __name__ == "__main__":
    main()