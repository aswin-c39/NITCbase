#include "BlockBuffer.h"
#include<iostream>
#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

int BlockBuffer::getHeader(HeadInfo *head) {
    unsigned char *buffer;

    int ret = loadBlockAndGetBufferPtr(&buffer);
    if(ret != SUCCESS) {
        return ret;
    }

    memcpy(&head->numSlots, buffer + 24, 4);
    memcpy(&head->numEntries, buffer + 16, 4);
    memcpy(&head->numAttrs, buffer + 20, 4);
    memcpy(&head->rblock, buffer + 12, 4);
    memcpy(&head->lblock, buffer + 8, 4);
    memcpy(&head->pblock, buffer + 4, 4);

    return SUCCESS;

}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block
       using loadBlockAndGetBufferPtr(&bufferPtr). */
       int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
        // return the value returned by the call.
        if(ret != SUCCESS)
            return ret;

    /* get the header of the block using the getHeader() function */
    HeadInfo head;
    BlockBuffer::getHeader(&head);

    // get number of attributes in the block.
    int attrCount = head.numAttrs;

    // get the number of slots in the block.
    int slotCount = head.numSlots;

    // if input slotNum is not in the permitted range return E_OUTOFBOUND.
    if(slotNum >= slotCount) return E_OUTOFBOUND;

    /* offset bufferPtr to point to the beginning of the record at required
       slot. the block contains the header, the slotmap, followed by all
       the records. so, for example,
       record at slot x will be at bufferPtr + HEADER_SIZE + (x*recordSize)
       copy the record from `rec` to buffer using memcpy
       (hint: a record will be of size ATTR_SIZE * numAttrs)
    */
   int recordSize = attrCount * ATTR_SIZE;
   unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize*slotNum); 
   memcpy(slotPointer, rec, recordSize);

    // update dirty bit using setDirtyBit()
    ret = StaticBuffer::setDirtyBit(this->blockNum);

    /* (the above function call should not fail since the block is already
       in buffer and the blockNum is valid. If the call does fail, there
       exists some other issue in the code) */
       if(ret != SUCCESS){
        std::cout << "There is some ERROR in the code!";
        exit(1);
       }

    return SUCCESS;
}

int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    HeadInfo head;
    BlockBuffer::getHeader(&head);
    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char* buffer;
    int ret = loadBlockAndGetBufferPtr(&buffer); 
    if(ret != SUCCESS || buffer == NULL) {
        return ret;
    }

    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = buffer + (32 + slotCount + (recordSize * slotNum));
    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}


int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char ** buffPtr) {
     
    
     /* check whether the block is already present in the buffer
       using StaticBuffer.getBufferNum() */
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
    if(bufferNum == E_OUTOFBOUND)
        return E_OUTOFBOUND;

    // if present (!=E_BLOCKNOTINBUFFER),
        // set the timestamp of the corresponding buffer to 0 and increment the
        // timestamps of all other occupied buffers in BufferMetaInfo.
        if(bufferNum != E_BLOCKNOTINBUFFER) {
            for(int i = 0; i < BUFFER_CAPACITY; i++) {
                StaticBuffer::metainfo[i].timeStamp++;
            }
            StaticBuffer::metainfo[bufferNum].timeStamp = 0;
        }
    else {
        // get a free buffer using StaticBuffer.getFreeBuffer()
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        // if the call returns E_OUTOFBOUND, return E_OUTOFBOUND here as
        // the blockNum is invalid
        if(bufferNum == E_OUTOFBOUND)
            return E_OUTOFBOUND;

        // Read the block into the free buffer using readBlock()
        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    }

    // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
    *buffPtr = StaticBuffer::blocks[bufferNum]; 

    return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;

    int ret = loadBlockAndGetBufferPtr(&bufferPtr); 
    if(ret != SUCCESS) 
        return ret;
    
    struct HeadInfo head;
    BlockBuffer::getHeader(&head);

    int slotCount = head.numSlots;

    unsigned char *slotMapinBuffer = bufferPtr + HEADER_SIZE;
    memcpy(slotMap, slotMapinBuffer, slotCount);
    return SUCCESS; 
}

int compareAttrs(Attribute attr1, Attribute attr2, int attrType) {
    if(attrType == NUMBER) 
        return attr1.nVal < attr2.nVal ? -1 : (attr1.nVal == attr2.nVal ? 0 : 1) ;
    else 
        return strcmp(attr1.sVal, attr2.sVal);
}