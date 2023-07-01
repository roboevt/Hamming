# Hamming

Implementation of a basic channel coding schecme, Hamming codes. Many thanks to [this awesome video](https://www.youtube.com/watch?v=X8jsijhllIA&t=0s) from 3 Blue 1 Brown on Youtube!

Here is an example/demo of the full process:
```
Transmission side:
Message:        00000000111001011010110110000110
Expand:         00111001011010101011000001100000
Correction:     00000000000000000000000000001110
Encoded:        00111001111010101011000101110100

Reception side (with random bit flip):
Input requires correction, bit 23 was flipped.
Check:          23
Decoded:        00111001011010101011000101110100
Received:       00000000111001011010110110000110
Message received successfully!
```
