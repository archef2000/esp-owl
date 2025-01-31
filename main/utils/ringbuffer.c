#include "ringbuffer.h"

void initRingBuffer(RingBuffer *buffer) {
    buffer->start = 0;
    buffer->end = 0;
    buffer->size = 0;
}

void putRingBuffer(RingBuffer *buffer, char *value) {
    if (buffer->size < BUFFER_SIZE) {
        buffer->size++;
    } else {
        free(buffer->data[buffer->end]);
        buffer->start = (buffer->start + 1) % BUFFER_SIZE;
    }
    buffer->data[buffer->end] = value;
    buffer->end = (buffer->end + 1) % BUFFER_SIZE;
}

char* getRingBufferIndex(RingBuffer *buffer, int index) {
    if (index >= buffer->size || index * -1 > buffer->size) { // Out of bounds
        return "";
    }
    int i = (buffer->start + (buffer->size - index -1)) % BUFFER_SIZE;
    return buffer->data[i];
}

void printBuffer(RingBuffer *buffer) {
    printf("Buffer size: %d\n", buffer->size);
    for (int i = 0; i < buffer->size; i++) {
        int index = (buffer->start + i) % BUFFER_SIZE;
        printf("\"%s\" ", buffer->data[index]);
    }
    printf("\n");
}

void setIndex(RingBuffer *buffer, int index, char *value) {
    // TODO: free old value
    while (index < 0 ) {
        index += BUFFER_SIZE;
    }
    while (index >= BUFFER_SIZE) {
        index -= BUFFER_SIZE;
    }
    if (index >= buffer->size) {
        if (buffer->start == 0) {
            buffer->start = BUFFER_SIZE - 1;
        } else {
            buffer->start--;
        }
        buffer->size++;
        buffer->data[buffer->start] = value;
        return;
    }
    int i = buffer->end - index - 1;
    if (i < 0) {
        i += BUFFER_SIZE;
    }
    buffer->data[i] = value;
}

int test_buffer() {
    RingBuffer buffer;
    initRingBuffer(&buffer);
    putRingBuffer(&buffer, "10");
    putRingBuffer(&buffer, "20");
    printBuffer(&buffer);
    setIndex(&buffer, 2, "603324");;
    printBuffer(&buffer);
    putRingBuffer(&buffer, "60");
    printBuffer(&buffer);
    printf("%s\n", getRingBufferIndex(&buffer, 2));
    putRingBuffer(&buffer, "70");
    printBuffer(&buffer);
    printf("%s\n", getRingBufferIndex(&buffer, -6));
    printf("%s\n", getRingBufferIndex(&buffer, -4));
    return 0;
}
