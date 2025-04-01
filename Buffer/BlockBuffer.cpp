#include "BlockBuffer.h"
#include<iostream>
#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(char blockType){
    // allocate a block on the disk and a buffer in memory to hold the new block of
    // given type using getFreeBlock function and get the return error codes if any.
    int blocktype = blockType == 'R' ? REC : UNUSED_BLK;

    int blockNum = getFreeBlock(blocktype);

    if(blockNum < 0 || blockNum >= DISK_BLOCKS) {
        printf("Error: Block is not available\n");
        this->blockNum = blockNum;
        return;
    }

    // set the blockNum field of the object to that of the allocated block
    // number if the method returned a valid block number,
    // otherwise set the error code returned as the block number.
    this->blockNum = blockNum;

    // (The caller must check if the constructor allocatted block successfully
    // by checking the value of block number field.)
}

RecBuffer::RecBuffer() : BlockBuffer::BlockBuffer('R'){}
// call parent non-default constructor with 'R' denoting record block.

BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

// call the corresponding parent constructor
IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType){}

// call the corresponding parent constructor
IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum){}

IndInternal::IndInternal() : IndBuffer('I'){}
// call the corresponding parent constructor
// 'I' used to denote IndInternal.

IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum){}
// call the corresponding parent constructor

IndLeaf::IndLeaf() : IndBuffer('L'){} // this is the way to call parent non-default constructor.
// 'L' used to denote IndLeaf.

//this is the way to call parent non-default constructor.
IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum){}




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

int BlockBuffer::getBlockNum(){

    //return corresponding block number.
    return this->blockNum;
}

int BlockBuffer::setHeader(struct HeadInfo *head){

    unsigned char *bufferPtr;
    // get the starting address of the buffer containing the block using
    // loadBlockAndGetBufferPtr(&bufferPtr).
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
        // return the value returned by the call.
        if(ret != SUCCESS)
            return ret;

    // cast bufferPtr to type HeadInfo*
    struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

    // copy the fields of the HeadInfo pointed to by head (except reserved) to
    // the header of the block (pointed to by bufferHeader)
    //(hint: bufferHeader->numSlots = head->numSlots )
    bufferHeader->blockType = head->blockType;
    bufferHeader->lblock = head->lblock;
    bufferHeader->numAttrs = head->numAttrs;
    bufferHeader->numEntries = head->numEntries;
    bufferHeader->numSlots = head->numSlots;
    bufferHeader->pblock = head->pblock;
    bufferHeader->rblock = head->rblock;

    // update dirty bit by calling StaticBuffer::setDirtyBit()
    // if setDirtyBit() failed, return the error code
    ret = StaticBuffer::setDirtyBit(this->blockNum);

    if(ret != SUCCESS)
        return ret;

    return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *record, int slotNum)
{
	// read the block at this.blockNum into a buffer
	unsigned char *buffer;
	//// Disk::readBlock(buffer, this->blockNum);
	int ret = loadBlockAndGetBufferPtr(&buffer);
	if (ret != SUCCESS)
		return ret;
	
	// get the header using this.getHeader() function
	HeadInfo head;
	BlockBuffer::getHeader(&head);

	// get number of attributes in the block.
	int attrCount = head.numAttrs;

    // get the number of slots in the block.
	int slotCount = head.numSlots;

	//! if input slotNum is not in the permitted range 
	if (slotNum >= slotCount) return E_OUTOFBOUND;

	int recordSize = attrCount * ATTR_SIZE;
	unsigned char *slotPointer = buffer + (HEADER_SIZE + slotCount + (recordSize * slotNum)); // calculate buffer + offset

	// load the record into the rec data structure
	memcpy(slotPointer, record, recordSize);

	ret = StaticBuffer::setDirtyBit(this->blockNum);

	//! The above function call should not fail since the block is already
    //! in buffer and the blockNum is valid. If the call does fail, there
    //! exists some other issue in the code) 
	if (ret != SUCCESS) {
		std::cout << "There is some error in the code!\n";
		exit(1);
	}

	// // Disk::writeBlock(buffer, this->blockNum);

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

int RecBuffer::setSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block using
       loadBlockAndGetBufferPtr(&bufferPtr). */
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
        // return the value returned by the call.
    if(ret != SUCCESS) return ret;

    // get the header of the block using the getHeader() function
    HeadInfo blockHeader;
    getHeader(&blockHeader);

    int numSlots = blockHeader.numSlots;

    // the slotmap starts at bufferPtr + HEADER_SIZE. Copy the contents of the
    // argument `slotMap` to the buffer replacing the existing slotmap.
    // Note that size of slotmap is `numSlots`
    unsigned char *slotPointer = bufferPtr + HEADER_SIZE;
    memcpy(slotPointer, slotMap, numSlots);

    // update dirty bit using StaticBuffer::setDirtyBit
    ret = StaticBuffer::setDirtyBit(this->blockNum);

    // if setDirtyBit failed, return the value returned by the call
    if(ret != SUCCESS) return ret;

    return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType){

    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block
       using loadBlockAndGetBufferPtr(&bufferPtr). */
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
        // return the value returned by the call.
    if(ret != SUCCESS)
        return ret;

    // store the input block type in the first 4 bytes of the buffer.
    // (hint: cast bufferPtr to int32_t* and then assign it)
    // *((int32_t *)bufferPtr) = blockType;
    int32_t *blockTypePtr = (int32_t*)bufferPtr;
    *blockTypePtr = blockType;

    // update the StaticBuffer::blockAllocMap entry corresponding to the
    // object's block number to `blockType`.
    StaticBuffer::blockAllocMap[this->blockNum] = blockType;

    // update dirty bit by calling StaticBuffer::setDirtyBit()
    ret = StaticBuffer::setDirtyBit(this->blockNum);

    // if setDirtyBit() failed
        // return the returned value from the call
    if(ret != SUCCESS)
        return ret;

    return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType){

    // iterate through the StaticBuffer::blockAllocMap and find the block number
    // of a free block in the disk.
    int blockNum;

    for(blockNum = 0; blockNum < DISK_BLOCKS; blockNum++) {
        if(StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK)
            break;
    }

    // if no block is free, return E_DISKFULL.
    if(blockNum >= DISK_BLOCKS)
        return E_DISKFULL;

    // set the object's blockNum to the block number of the free block.
    this->blockNum = blockNum;

    // find a free buffer using StaticBuffer::getFreeBuffer() .
    int bufferindex = StaticBuffer::getFreeBuffer(blockNum);

    if(bufferindex < 0 || bufferindex >= BUFFER_CAPACITY) {
        printf("Error: Buffer is full\n");
        return bufferindex;
    }

    // initialize the header of the block passing a struct HeadInfo with values
    // pblock: -1, lblock: -1, rblock: -1, numEntries: 0, numAttrs: 0, numSlots: 0
    // to the setHeader() function.
    HeadInfo blockHeader;
    blockHeader.pblock = blockHeader.lblock = blockHeader.rblock = -1;
    blockHeader.numAttrs = blockHeader.numEntries = blockHeader.numSlots = 0;
    setHeader(&blockHeader);

    // update the block type of the block to the input block type using setBlockType().
    setBlockType(blockType);

    // return block number of the free block.
    return blockNum;
}

void BlockBuffer::releaseBlock(){

    // if blockNum is INVALID_BLOCKNUM (-1), or it is invalidated already, do nothing
    if(blockNum == INVALID_BLOCKNUM || StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK)
        return;
    //else
        /* get the buffer number of the buffer assigned to the block
           using StaticBuffer::getBufferNum().
           (this function return E_BLOCKNOTINBUFFER if the block is not
           currently loaded in the buffer)
            */
    int bufferIndex = StaticBuffer::getBufferNum(blockNum);

        // if the block is present in the buffer, free the buffer
        // by setting the free flag of its StaticBuffer::tableMetaInfo entry
        // to true.
        if(bufferIndex >= 0 && bufferIndex < BUFFER_CAPACITY)
            StaticBuffer::metainfo[bufferIndex].free = true;

        // free the block in disk by setting the data type of the entry
        // corresponding to the block number in StaticBuffer::blockAllocMap
        // to UNUSED_BLK.
        StaticBuffer::blockAllocMap[blockNum] = UNUSED_BLK;

        // set the object's blockNum to INVALID_BLOCK (-1)
        this->blockNum = INVALID_BLOCKNUM;
}

int compareAttrs(Attribute attr1, Attribute attr2, int attrType) {
    if(attrType == NUMBER) 
        return attr1.nVal < attr2.nVal ? -1 : (attr1.nVal == attr2.nVal ? 0 : 1) ;
    else 
        return strcmp(attr1.sVal, attr2.sVal);
}

int IndInternal::getEntry(void *ptr, int indexNum) {
    // if the indexNum is not in the valid range of [0, MAX_KEYS_INTERNAL-1]
    //     return E_OUTOFBOUND.
    if(indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL) return indexNum;

    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block
       using loadBlockAndGetBufferPtr(&bufferPtr). */
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
    //     return the value returned by the call.
    if(ret != SUCCESS) return ret;

    // typecast the void pointer to an internal entry pointer
    struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;

    /*
    - copy the entries from the indexNum`th entry to *internalEntry
    - make sure that each field is copied individually as in the following code
    - the lChild and rChild fields of InternalEntry are of type int32_t
    - int32_t is a type of int that is guaranteed to be 4 bytes across every
      C++ implementation. sizeof(int32_t) = 4
    */

    /* the indexNum'th entry will begin at an offset of
       HEADER_SIZE + (indexNum * (sizeof(int) + ATTR_SIZE) )         [why?]
       from bufferPtr */
    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

    memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
    memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
    memcpy(&(internalEntry->rChild), entryPtr + 20, 4);

    return SUCCESS;
}

int IndLeaf::getEntry(void *ptr, int indexNum) {

    // if the indexNum is not in the valid range of [0, MAX_KEYS_LEAF-1]
    //     return E_OUTOFBOUND.
    if(indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL) return indexNum;

    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block
       using loadBlockAndGetBufferPtr(&bufferPtr). */
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
    //     return the value returned by the call.
    if(ret != SUCCESS) return ret;

    // copy the indexNum'th Index entry in buffer to memory ptr using memcpy

    /* the indexNum'th entry will begin at an offset of
       HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE)  from bufferPtr */
    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
    memcpy((struct Index *)ptr, entryPtr, LEAF_ENTRY_SIZE);

    return SUCCESS;
}

int IndLeaf::setEntry(void *ptr, int indexNum) {

    if(indexNum < 0 || indexNum >= MAX_KEYS_LEAF) {
        return E_OUTOFBOUND;
    }

    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block
       using loadBlockAndGetBufferPtr(&bufferPtr). */
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
    //     return the value returned by the call.
    if(ret != SUCCESS) return ret;

    // copy the Index at ptr to indexNum'th entry in the buffer using memcpy

    /* the indexNum'th entry will begin at an offset of
       HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE)  from bufferPtr */
    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
    memcpy(entryPtr, (struct Index *)ptr, LEAF_ENTRY_SIZE);

    // update dirty bit using setDirtyBit()
    ret = StaticBuffer::setDirtyBit(this->blockNum);
    // if setDirtyBit failed, return the value returned by the call
    if(ret != SUCCESS) return ret;

    return SUCCESS;
}

int IndInternal::setEntry(void *ptr, int indexNum) {
    // if the indexNum is not in the valid range of [0, MAX_KEYS_INTERNAL-1]
    //     return E_OUTOFBOUND.
    if(indexNum < 0 || indexNum >= MAX_KEYS_LEAF) {
        return E_OUTOFBOUND;
    }

    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block
       using loadBlockAndGetBufferPtr(&bufferPtr). */
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
    //     return the value returned by the call.
    if(ret != SUCCESS) return ret;

    // typecast the void pointer to an internal entry pointer
    struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;

    /*
    - copy the entries from *internalEntry to the indexNum`th entry
    - make sure that each field is copied individually as in the following code
    - the lChild and rChild fields of InternalEntry are of type int32_t
    - int32_t is a type of int that is guaranteed to be 4 bytes across every
      C++ implementation. sizeof(int32_t) = 4
    */

    /* the indexNum'th entry will begin at an offset of
       HEADER_SIZE + (indexNum * (sizeof(int) + ATTR_SIZE) )         [why?]
       from bufferPtr */

    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

    memcpy(entryPtr, &(internalEntry->lChild), 4);
    memcpy(entryPtr + 4, &(internalEntry->attrVal), ATTR_SIZE);
    memcpy(entryPtr + 20, &(internalEntry->rChild), 4);


    // update dirty bit using setDirtyBit()
    // if setDirtyBit failed, return the value returned by the call
    ret = StaticBuffer::setDirtyBit(this->blockNum);

    return SUCCESS;
}