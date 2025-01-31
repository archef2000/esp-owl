#include <stdio.h>

#define BUFFER_SIZE 20

typedef struct {
    char *data[BUFFER_SIZE];
    int start;  // Index of the oldest element
    int end;    // Index where the next element will be written
    int size;   // Current number of elements in the buffer
} RingBuffer;

void initRingBuffer(RingBuffer *buffer);
void putRingBuffer(RingBuffer *buffer, char *value);
char* getRingBufferIndex(RingBuffer *buffer, int index);
void printBuffer(RingBuffer *buffer);
void setIndex(RingBuffer *buffer, int index, char *value);
int test_buffer();