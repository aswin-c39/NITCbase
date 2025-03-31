#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char  StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer() {
    // copy blockAllocMap blocks from disk to buffer (using readblock() of disk)
    // blocks 0 to 3
    for(int i = 0, blockAllocMapSlot = 0; i < 4; i++) {
        unsigned char buffer[BLOCK_SIZE];
        Disk::readBlock(buffer, i);
        for(int slot = 0; slot < BLOCK_SIZE; slot++, blockAllocMapSlot++)
            StaticBuffer::blockAllocMap[blockAllocMapSlot] = buffer[slot]; 
    }
    /* initialise metainfo of all the buffer blocks with
     dirty:false, free:true, timestamp:-1 and blockNum:-1
  */
    for(int i = 0; i < BUFFER_CAPACITY; i++) {
        metainfo[i].free = true;
        metainfo[i].dirty = false;
        metainfo[i].timeStamp = -1;
        metainfo[i].blockNum = -1;
    }
}


StaticBuffer::~StaticBuffer() {
  // copy blockAllocMap blocks from buffer to disk(using writeblock() of disk)
    for(int i = 0, blockAllocMapSlot = 0; i < 4; i++) {
        unsigned char buffer[BLOCK_SIZE];

        for(int slot = 0; slot < BLOCK_SIZE; slot++, blockAllocMapSlot++)
            buffer[slot] = blockAllocMap[blockAllocMapSlot];

        Disk::writeBlock(buffer, i);
    }

    /*iterate through all the buffer blocks,
    write back blocks with metainfo as free:false,dirty:true
    (you did this already)
  */
    for(int i = 0; i < BUFFER_CAPACITY; i++) {
        if(!metainfo[i].free && metainfo[i].dirty)
            Disk::writeBlock(blocks[i], metainfo[i].blockNum);
    }
}

int StaticBuffer::getFreeBuffer(int blockNum){
    // Check if blockNum is valid (non zero and less than DISK_BLOCKS)
    // and return E_OUTOFBOUND if not valid.
    if(blockNum < 0 || blockNum >= DISK_BLOCKS)
        E_OUTOFBOUND;

    // increase the timeStamp in metaInfo of all occupied buffers.
    for(int i = 0; i < BUFFER_CAPACITY; i++)  {
        if(!metainfo[i].free)
            metainfo[i].timeStamp++;
    }

    // let bufferNum be used to store the buffer number of the free/freed buffer.
    int bufferNum = 0;

    // iterate through metainfo and check if there is any buffer free

    // if a free buffer is available, set bufferNum = index of that free buffer.
    for( ; bufferNum < BUFFER_CAPACITY; bufferNum++) {
        if(metainfo[bufferNum].free)
            break;
    }

    // if a free buffer is not available,
    if(bufferNum >= BUFFER_CAPACITY) {
    //     find the buffer with the largest timestamp
        int maxTimeStamp = -1;
        for(int i = 0; i < BUFFER_CAPACITY; i++) {
            if(metainfo[i].timeStamp > maxTimeStamp) {
                maxTimeStamp = metainfo[i].timeStamp;
                bufferNum = i;
            }
        }
        //IF IT IS DIRTY, write back to the disk using Disk::writeBlock()
        //set bufferNum = index of this buffer
        if(metainfo[bufferNum].dirty)
            Disk::writeBlock(blocks[bufferNum], metainfo[bufferNum].blockNum);
    }
    

    // update the metaInfo entry corresponding to bufferNum with
    // free:false, dirty:false, blockNum:the input block number, timeStamp:0.
    metainfo[bufferNum].free = false;
    metainfo[bufferNum].dirty = false;
    metainfo[bufferNum].blockNum = blockNum;
    metainfo[bufferNum].timeStamp = 0;

    // return the bufferNum.
    return bufferNum;
}

int StaticBuffer::getBufferNum(int blockNum) {
    if(blockNum < 0 || blockNum > DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    for(int i = 0; i < BUFFER_CAPACITY; i++) {
        if(metainfo[i].blockNum == blockNum) {
            return i;
        }
    }
    return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::setDirtyBit(int blockNum){
    // find the buffer index corresponding to the block using getBufferNum().
    int bufferNum = getBufferNum(blockNum);

    // if block is not present in the buffer (bufferNum = E_BLOCKNOTINBUFFER)
    //     return E_BLOCKNOTINBUFFER
    if(bufferNum == E_BLOCKNOTINBUFFER)
        return E_BLOCKNOTINBUFFER;

    // if blockNum is out of bound (bufferNum = E_OUTOFBOUND)
    //     return E_OUTOFBOUND
    if(bufferNum == E_OUTOFBOUND)
        return E_OUTOFBOUND;

    // else
    //     (the bufferNum is valid)
    //     set the dirty bit of that buffer to true in metainfo
    metainfo[bufferNum].dirty = true;

    // return SUCCESS
    return SUCCESS;
}

int StaticBuffer::getStaticBlockType(int blockNum){
    // Check if blockNum is valid (non zero and less than number of disk blocks)
    // and return E_OUTOFBOUND if not valid.
    if(blockNum < 0 || blockNum >= DISK_BLOCKS) return blockNum;

    // Access the entry in block allocation map corresponding to the blockNum argument
    // and return the block type after type casting to integer.
    return (int)blockAllocMap[blockNum];
}