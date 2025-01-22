#include "BlockBuffer.h"

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
    
    HeadInfo head;

    BlockBuffer::getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char* buffer;

    int ret = loadBlockAndGetBufferPtr(&buffer);
    if(ret != SUCCESS) {
        return ret;
    }

    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = buffer + (32 + slotCount + (recordSize*slotNum));

    memcpy(slotPointer, rec, recordSize);

    Disk::writeBlock(buffer, this->blockNum);

    return SUCCESS;
}


int RecBuffer::getRecord(union Attribute *rec, int slotNum) {

    HeadInfo head;
    BlockBuffer::getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char* buffer;
    int ret = loadBlockAndGetBufferPtr(&buffer); 
    if(ret != SUCCESS) {
        return ret;
    }

    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = buffer + (32 + slotCount + (recordSize*slotNum));

    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    if(bufferNum == E_BLOCKNOTINBUFFER) {
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if(bufferNum == E_OUTOFBOUND) {
            return E_OUTOFBOUND;
        }
        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    }

    *buffPtr = StaticBuffer::blocks[bufferNum];

    return SUCCESS;
}