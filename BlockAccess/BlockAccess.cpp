#include "BlockAccess.h"
#include<stdio.h>
#include <cstring>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
        RecId prevRecId;
        RelCacheTable::getSearchIndex(relId, &prevRecId);
        int block = -1, slot = -1;

        if(prevRecId.block == -1 && prevRecId.slot == -1) {
            RelCatEntry relCatEntry;
            RelCacheTable::getRelCatEntry(relId, &relCatEntry);

            block = relCatEntry.firstBlk;
            slot = 0;
        } else {
            block = prevRecId.block;
            slot = prevRecId.slot + 1;
        }

        RelCatEntry relCatBuffer;
        RelCacheTable::getRelCatEntry(relId, &relCatBuffer);
        while(block != -1) {
            RecBuffer blockBuffer(block);
            HeadInfo blockHeader;
            blockBuffer.getHeader(&blockHeader);
            unsigned char slotMap[blockHeader.numSlots];
            blockBuffer.getSlotMap(slotMap);

            if(slot > blockHeader.numSlots) {
                block = blockHeader.rblock;
                slot = 0;
                continue;
            }
            if(slotMap[slot] == SLOT_UNOCCUPIED) {
                slot++;
                continue;
            }
            
            Attribute record[blockHeader.numAttrs];
            blockBuffer.getRecord(record, slot);

            AttrCatEntry attrCatBuffer;
            AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuffer);

            int attrOffset = attrCatBuffer.offset;

            int cmpVal = compareAttrs(record[attrOffset], attrVal, attrCatBuffer.attrType);

            if(
                (op == NE && cmpVal != 0) ||
                (op == LT && cmpVal < 0)  ||
                (op == LE && cmpVal <= 0) ||
                (op == EQ && cmpVal == 0) ||
                (op == GT && cmpVal > 0)  ||
                (op == GE && cmpVal >= 0)
            ) {
                RecId newRecId = {block, slot};
                RelCacheTable::setSearchIndex(relId, &newRecId);

                return RecId{block, slot};
            }
            slot++;
        }
        return RecId{-1, -1};
}

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){
    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
       RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelationName;    // set newRelationName with newName
    strcpy(newRelationName.sVal, newName);

    // search the relation catalog for an entry with "RelName" = newRelationName
    RecId searchIndex = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, newRelationName, EQ);
    printf("%d, %d\n", searchIndex.block, searchIndex.slot);

    // If relation with name newName already exists (result of linearSearch
    //                                               is not {-1, -1})
    //    return E_RELEXIST;
    if(searchIndex.block != -1 && searchIndex.slot != -1) 
        return E_RELEXIST;

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelationName;    // set oldRelationName with oldName
    strcpy(oldRelationName.sVal, oldName);

    // search the relation catalog for an entry with "RelName" = oldRelationName
    searchIndex = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, oldRelationName, EQ);
    printf("%d, %d\n", searchIndex.block, searchIndex.slot);

    // If relation with name oldName does not exist (result of linearSearch is {-1, -1})
    //    return E_RELNOTEXIST;
    if(searchIndex.slot == -1 && searchIndex.block == -1)
        return E_RELNOTEXIST;

    /* get the relation catalog record of the relation to rename using a RecBuffer
       on the relation catalog [RELCAT_BLOCK] and RecBuffer.getRecord function
    */
    RecBuffer relCatBlock (RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, searchIndex.slot);

    /* update the relation name attribute in the record with newName.
       (use RELCAT_REL_NAME_INDEX) */
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newName);

    // set back the record value using RecBuffer.setRecord
    relCatBlock.setRecord(relCatRecord, searchIndex.slot);

    /*
    update all the attribute catalog entries in the attribute catalog corresponding
    to the relation with relation name oldName to the relation name newName
    */   
    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);


    //for i = 0 to numberOfAttributes :
    int numberOfAttributes = relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    for(int i = 0; i < numberOfAttributes; i++) {
    //    linearSearch on the attribute catalog for relName = oldRelationName
        searchIndex = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);
    //    get the record using RecBuffer.getRecord
        RecBuffer attrCatBlock(ATTRCAT_BLOCK);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord, searchIndex.slot);

    //    update the relName field in the record to newName
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);

    //    set back the record using RecBuffer.setRecord
        attrCatBlock.setRecord(attrCatRecord, searchIndex.slot);
    }

    return SUCCESS;
}


int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
       RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;    // set relNameAttr to relName
    strcpy(relNameAttr.sVal, relName);

    // Search for the relation with name relName in relation catalog using linearSearch()
    RecId searchIndex = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAttr, EQ);

    // If relation with name relName does not exist (search returns {-1,-1})
    //    return E_RELNOTEXIST;
    if(searchIndex.block == -1 && searchIndex.slot == -1)
        E_RELNOTEXIST;

    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    /* declare variable attrToRenameRecId used to store the attr-cat recId
    of the attribute to rename */
    RecId attrToRenameRecId{-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    /* iterate over all Attribute Catalog Entry record corresponding to the
       relation to find the required attribute */
    while (true) {
        // linear search on the attribute catalog for RelName = relNameAttr
        searchIndex = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);
        printf("%d, %d\n", searchIndex.block, searchIndex.slot);

        // if there are no more attributes left to check (linearSearch returned {-1,-1})
        //     break;
        if(searchIndex.block == -1 && searchIndex.slot == -1)
            break;

        /* Get the record from the attribute catalog using RecBuffer.getRecord
          into attrCatEntryRecord */
          RecBuffer attrCatBlock(searchIndex.block);
          attrCatBlock.getRecord(attrCatEntryRecord, searchIndex.slot);

        // if attrCatEntryRecord.attrName = oldName
        //     attrToRenameRecId = block and slot of this record
        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0){
            attrToRenameRecId = searchIndex;
            break;
        }

        // if attrCatEntryRecord.attrName = newName
        //     return E_ATTREXIST;
        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0)
            return E_ATTREXIST;
    }

    // if attrToRenameRecId == {-1, -1}
    //     return E_ATTRNOTEXIST;
    if(attrToRenameRecId.block == -1 || attrToRenameRecId.slot == -1)
        return E_ATTRNOTEXIST;

    // Update the entry corresponding to the attribute in the Attribute Catalog Relation.
    /*   declare a RecBuffer for attrToRenameRecId.block and get the record at
         attrToRenameRecId.slot */
         RecBuffer attrCatBlock(attrToRenameRecId.block);
         Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
         attrCatBlock.getRecord(attrCatRecord, attrToRenameRecId.slot);

    //   update the AttrName of the record with newName
    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);

    //   set back the record with RecBuffer.setRecord
    attrCatBlock.setRecord(attrCatRecord, attrToRenameRecId.slot);

    return SUCCESS;
}

int BlockAccess::insert(int relId, Attribute *record) {
    // get the relation catalog entry from relation cache
    // ( use RelCacheTable::getRelCatEntry() of Cache Layer)
    RelCatEntry relCatBuffer;
    RelCacheTable::getRelCatEntry(relId, &relCatBuffer);

    int blockNum = relCatBuffer.firstBlk;

    // rec_id will be used to store where the new record will be inserted
    RecId rec_id = {-1, -1};

    int numOfSlots = relCatBuffer.numSlotsPerBlk;
    int numOfAttributes = relCatBuffer.numAttrs;

    int prevBlockNum = -1;

    /*
        Traversing the linked list of existing record blocks of the relation
        until a free slot is found OR
        until the end of the list is reached
    */
    while (blockNum != -1) {
        // create a RecBuffer object for blockNum (using appropriate constructor!)
        RecBuffer blockBuffer(blockNum);

        // get header of block(blockNum) using RecBuffer::getHeader() function
        HeadInfo blockHeader;
        blockBuffer.getHeader(&blockHeader);

        // get slot map of block(blockNum) using RecBuffer::getSlotMap() function
        int numSlots = blockHeader.numSlots;
        unsigned char slotMap[numSlots];
        blockBuffer.getSlotMap(slotMap);

        // search for free slot in the block 'blockNum' and store it's rec-id in rec_id
        // (Free slot can be found by iterating over the slot map of the block)
        /* slot map stores SLOT_UNOCCUPIED if slot is free and
           SLOT_OCCUPIED if slot is occupied) */
           int slotIndex;
           for(slotIndex = 0; slotIndex < numOfSlots; slotIndex++) {
                /* if a free slot is found, set rec_id and discontinue the traversal
                of the linked list of record blocks (break from the loop) */
                if(slotMap[slotIndex] == SLOT_UNOCCUPIED) {
                    rec_id = RecId{blockNum, slotIndex};
                    break;
                }
           }
            if(rec_id.block != -1 && rec_id.slot != -1)
                break;
        /* otherwise, continue to check the next block by updating the
           block numbers as follows:
              update prevBlockNum = blockNum
              update blockNum = header.rblock (next element in the linked
                                               list of record blocks)
        */
       prevBlockNum = blockNum;
       blockNum = blockHeader.rblock;
    }

    //  if no free slot is found in existing record blocks (rec_id = {-1, -1})
    if(rec_id.block == -1 && rec_id.slot == -1)
    {
        // if relation is RELCAT, do not allocate any more blocks
        //     return E_MAXRELATIONS;
        if(relId = RELCAT_RELID)
            return E_MAXRELATIONS;

        // Otherwise,
        // get a new record block (using the appropriate RecBuffer constructor!)
        RecBuffer blockBuffer;
        // get the block number of the newly allocated block
        // (use BlockBuffer::getBlockNum() function)
        int ret = blockBuffer.getBlockNum();

        // let ret be the return value of getBlockNum() function call
        if (ret == E_DISKFULL) {
            return E_DISKFULL;
        }

        // Assign rec_id.block = new block number(i.e. ret) and rec_id.slot = 0
        rec_id.block = ret;
        rec_id.slot = 0;

        /*
            set the header of the new record block such that it links with
            existing record blocks of the relation
            set the block's header as follows:
            blockType: REC, pblock: -1
            lblock
                  = -1 (if linked list of existing record blocks was empty
                         i.e this is the first insertion into the relation)
                  = prevBlockNum (otherwise),
            rblock: -1, numEntries: 0,
            numSlots: numOfSlots, numAttrs: numOfAttributes
            (use BlockBuffer::setHeader() function)
        */
        HeadInfo blockHeader;
        blockHeader.blockType = REC;
        blockHeader.pblock = blockHeader.rblock = -1;
        blockHeader.lblock = prevBlockNum;
        blockHeader.numAttrs = numOfAttributes;
        blockHeader.numSlots = numOfSlots;
        blockHeader.numEntries = 0;

        blockBuffer.setHeader(&blockHeader);

        /*
            set block's slot map with all slots marked as free
            (i.e. store SLOT_UNOCCUPIED for all the entries)
            (use RecBuffer::setSlotMap() function)
        */
       unsigned char slotMap[numOfSlots];
       for(int i = 0; i < numOfSlots; i++)
            slotMap[i] = SLOT_UNOCCUPIED;
        blockBuffer.setSlotMap(slotMap);

        if(prevBlockNum != -1)
        {
            // create a RecBuffer object for prevBlockNum
            RecBuffer prevBlockBuffer(prevBlockBuffer);
            // get the header of the block prevBlockNum and
            HeadInfo prevBlockheader;
            prevBlockBuffer.getHeader(&prevBlockheader);

             // update the rblock field of the header to the new block
            // number i.e. rec_id.block
            prevBlockheader.rblock = rec_id.block;

            // (use BlockBuffer::setHeader() function)
            prevBlockBuffer.setHeader(&prevBlockheader);
        }
        else
        {
            // update first block field in the relation catalog entry to the
            // new block (using RelCacheTable::setRelCatEntry() function)
            relCatBuffer.firstBlk = blockNum;
            RelCacheTable::setRelCatEntry(relId, &relCatBuffer);
        }

        // update last block field in the relation catalog entry to the
        // new block (using RelCacheTable::setRelCatEntry() function)
        relCatBuffer.lastBlk = blockNum;
        RelCacheTable::setRelCatEntry(relId, &relCatBuffer);
    }

    // create a RecBuffer object for rec_id.block
    RecBuffer blockBuffer(rec_id.block);
    // insert the record into rec_id'th slot using RecBuffer.setRecord())
    blockBuffer.setRecord(record, rec_id.slot);

    /* update the slot map of the block by marking entry of the slot to
       which record was inserted as occupied) */
    // (ie store SLOT_OCCUPIED in free_slot'th entry of slot map)
    // (use RecBuffer::getSlotMap() and RecBuffer::setSlotMap() functions)
    unsigned char slotMap[numOfSlots];
    blockBuffer.getSlotMap(slotMap);
    slotMap[rec_id.slot] = SLOT_OCCUPIED;
    blockBuffer.setSlotMap(slotMap);

    // increment the numEntries field in the header of the block to
    // which record was inserted
    // (use BlockBuffer::getHeader() and BlockBuffer::setHeader() functions)
    HeadInfo blockHeader;
    blockBuffer.getHeader(&blockHeader);
    blockHeader.numEntries++;
    blockBuffer.setHeader(&blockHeader);

    // Increment the number of records field in the relation cache entry for
    // the relation. (use RelCacheTable::setRelCatEntry function)
    relCatBuffer.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatBuffer);

    return SUCCESS;
}
