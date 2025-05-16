# Description of Work (10 points)

This is new for Project 1. We ask you to include a file called `README.md` that contains a quick description of:

1. the design choices you made,
2. any problems you encountered while creating your solution, and
3. your solutions to those problems.

This is not meant to be comprehensive; less than 300 words will suffice.

I opted to use a linked list for the buffer in the recv_data() function as it's better for sparse, OOO instructions. I actually opted to use a deque first but I was only getting half the score I should be getting because my logic broke when it came to storing OOO packets which is an integral part of reliable data transmission (to be able to sort the packets in order). Additionally, for the longest time I actually was getting 60-80 / 90 points in the autograder and I had no clue why. Turns out I forgot to convert from Big Endian while receiving the data that led to some data being misinterpreted at different times in the autograder which led to a lower score than what I should've gotten. Most of the get_data() and receive_data() was setting up the entire thing for the initial 3-way handshake. To do this I also put a lot of helper functions at the start of the code and highlighted it so it was easy to see and reuse. I have a couple that I don't think I use in my final solution such as printing the deque(). 