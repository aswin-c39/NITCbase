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
    //printf("%d, %d\n", searchIndex.block, searchIndex.slot);

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
    //printf("%d, %d\n", searchIndex.block, searchIndex.slot);

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
        RecBuffer attrCatBlock(searchIndex.block);
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
        //printf("%d, %d\n", searchIndex.block, searchIndex.slot);

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
        if(relId == RELCAT_RELID)
            return E_MAXRELATIONS;

        // Otherwise,
        // get a new record block (using the appropriate RecBuffer constructor!)
        RecBuffer blockBuffer;
        // get the block number of the newly allocated block
        // (use BlockBuffer::getBlockNum() function)
        blockNum = blockBuffer.getBlockNum();

        // let ret be the return value of getBlockNum() function call
        if (blockNum == E_DISKFULL) {
            return E_DISKFULL;
        }

        // Assign rec_id.block = new block number(i.e. ret) and rec_id.slot = 0
        rec_id.block = blockNum;
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
            RecBuffer prevBlockBuffer(prevBlockNum);
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


/*
NOTE: This function will copy the result of the search to the `record` argument.
      The caller should ensure that space is allocated for `record` array
      based on the number of attributes in the relation.
*/
int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    // Declare a variable called recid to store the searched record
    RecId recId;

    /* search for the record id (recid) corresponding to the attribute with
    attribute name attrName, with value attrval and satisfying the condition op
    using linearSearch() */
    recId = linearSearch(relId, attrName, attrVal, op);
    // if there's no record satisfying the given condition (recId = {-1, -1})
    //    return E_NOTFOUND;
    if(recId.block == -1 && recId.block == -1)
        return E_NOTFOUND;

    /* Copy the record with record id (recId) to the record buffer (record)
       For this Instantiate a RecBuffer class object using recId and
       call the appropriate method to fetch the record
    */
   RecBuffer blockBuffer(recId.block);
    blockBuffer.getRecord(record, recId.slot);

    return SUCCESS;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {
    // if the relation to delete is either Relation Catalog or Attribute Catalog,
    //     return E_NOTPERMITTED
        // (check if the relation names are either "RELATIONCAT" and "ATTRIBUTECAT".
        // you may use the following constants: RELCAT_NAME and ATTRCAT_NAME)
    if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr; // (stores relName as type union Attribute)
    // assign relNameAttr.sVal = relName
    strcpy(relNameAttr.sVal, relName);

    //  linearSearch on the relation catalog for RelName = relNameAttr
    RecId relCatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAttr, EQ);

    // if the relation does not exist (linearSearch returned {-1, -1})
    //     return E_RELNOTEXIST
    if(relCatRecId.block == -1 && relCatRecId.slot == -1)
        return E_RELNOTEXIST;

    RecBuffer relCatBlockBuffer(relCatRecId.block);

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    /* store the relation catalog record corresponding to the relation in
       relCatEntryRecord using RecBuffer.getRecord */
    relCatBlockBuffer.getRecord(relCatEntryRecord, relCatRecId.slot);

    /* get the first record block of the relation (firstBlock) using the
       relation catalog entry record */
    int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    /* get the number of attributes corresponding to the relation (numAttrs)
       using the relation catalog entry record */
    int numAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    /*
     Delete all the record blocks of the relation
    */
    int currBlockNum = firstBlock;
    //     Hint: to know if we reached the end, check if nextBlock = -1
    while(currBlockNum != -1) {
        RecBuffer currBlockBuffer(currBlockNum);

        // for each record block of the relation:
        // get block header using BlockBuffer.getHeader
        HeadInfo currBlockheader;
        currBlockBuffer.getHeader(&currBlockheader);

        // get the next block from the header (rblock)
        currBlockNum = currBlockheader.rblock;

        // release the block using BlockBuffer.releaseBlock
        currBlockBuffer.releaseBlock();
    }


    /***
        Deleting attribute catalog entries corresponding the relation and index
        blocks corresponding to the relation with relName on its attributes
    ***/

    // reset the searchIndex of the attribute catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    int numberOfAttributesDeleted = 0;

    while(true) {
        // attrCatRecId = linearSearch on attribute catalog for RelName = relNameAttr
        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

        // if no more attributes to iterate over (attrCatRecId == {-1, -1})
        //     break;
        if(attrCatRecId.block == -1 & attrCatRecId.slot == -1)
            break;

        numberOfAttributesDeleted++;

        // create a RecBuffer for attrCatRecId.block
        RecBuffer attrCatBlockBuffer(attrCatRecId.block);

        // get the header of the block
        HeadInfo attrCatheader;
        attrCatBlockBuffer.getHeader(&attrCatheader);

        // get the record corresponding to attrCatRecId.slot
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlockBuffer.getRecord(attrCatRecord, attrCatRecId.slot);
        // declare variable rootBlock which will be used to store the root
        // block field from the attribute catalog record.
        int rootBlock = attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
        // (This will be used later to delete any indexes if it exists)

        // Update the Slotmap for the block by setting the slot as SLOT_UNOCCUPIED
        // Hint: use RecBuffer.getSlotMap and RecBuffer.setSlotMap
        unsigned char slotMap[attrCatheader.numSlots];
        attrCatBlockBuffer.getSlotMap(slotMap);

        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        attrCatBlockBuffer.setSlotMap(slotMap);


        /* Decrement the numEntries in the header of the block corresponding to
           the attribute catalog entry and then set back the header
           using RecBuffer.setHeader */
        attrCatheader.numEntries--;
        attrCatBlockBuffer.setHeader(&attrCatheader);

        /* If number of entries become 0, releaseBlock is called after fixing
           the linked list.
        */
        if (attrCatheader.numEntries == 0) {
            /* Standard Linked List Delete for a Block
               Get the header of the left block and set it's rblock to this
               block's rblock
            */

            // create a RecBuffer for lblock and call appropriate methods
            RecBuffer prevBlock(attrCatheader.lblock);

            HeadInfo leftHeader;
            prevBlock.getHeader(&leftHeader);

            leftHeader.rblock = attrCatheader.rblock;
            prevBlock.setHeader(&leftHeader);

            if (attrCatheader.rblock != INVALID_BLOCKNUM) {
                /* Get the header of the right block and set it's lblock to
                   this block's lblock */
                // create a RecBuffer for rblock and call appropriate methods
                RecBuffer nextBlock(attrCatheader.rblock);

                HeadInfo rightHeader;
                nextBlock.getHeader(&rightHeader);

                rightHeader.lblock = attrCatheader.lblock;
                nextBlock.setHeader(&rightHeader);

            } else {
                // (the block being released is the "Last Block" of the relation.)
                /* update the Relation Catalog entry's LastBlock field for this
                   relation with the block number of the previous block. */
                RelCatEntry relCatEntryBuffer;
                RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntryBuffer);

                relCatEntryBuffer.lastBlk = attrCatheader.lblock;
            }

            // (Since the attribute catalog will never be empty(why?), we do not
            //  need to handle the case of the linked list becoming empty - i.e
            //  every block of the attribute catalog gets released.)

            // call releaseBlock()
            attrCatBlockBuffer.releaseBlock();
        }
        /*

        // (the following part is only relevant once indexing has been implemented)
        // if index exists for the attribute (rootBlock != -1), call bplus destroy
        if (rootBlock != -1) {
            // delete the bplus tree rooted at rootBlock using BPlusTree::bPlusDestroy()
        }
        */
    }

    /*** Delete the entry corresponding to the relation from relation catalog ***/
    // Fetch the header of Relcat block
    HeadInfo relCatHeader;
    relCatBlockBuffer.getHeader(&relCatHeader);

    /* Decrement the numEntries in the header of the block corresponding to the
       relation catalog entry and set it back */
    relCatHeader.numEntries--;
    relCatBlockBuffer.setHeader(&relCatHeader);

    /* Get the slotmap in relation catalog, update it by marking the slot as
       free(SLOT_UNOCCUPIED) and set it back. */
    unsigned char slotMap[relCatHeader.numSlots];
    relCatBlockBuffer.getSlotMap(slotMap);

    slotMap[relCatRecId.slot] = SLOT_UNOCCUPIED;
    relCatBlockBuffer.setSlotMap(slotMap);

    /*** Updating the Relation Cache Table ***/
    /** Update relation catalog record entry (number of records in relation
        catalog is decreased by 1) **/
    // Get the entry corresponding to relation catalog from the relation
    RelCatEntry relCatEntryBuffer;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntryBuffer);

    // cache and update the number of records and set it back
    // (using RelCacheTable::setRelCatEntry() function)
    relCatEntryBuffer.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntryBuffer);

    /** Update attribute catalog entry (number of records in attribute catalog
        is decreased by numberOfAttributesDeleted) **/
    // i.e., #Records = #Records - numberOfAttributesDeleted

    // Get the entry corresponding to attribute catalog from the relation
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntryBuffer);
    // cache and update the number of records and set it back
    // (using RelCacheTable::setRelCatEntry() function)
    relCatEntryBuffer.numRecs -= numberOfAttributesDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntryBuffer);

    return SUCCESS;
}

/*
NOTE: the caller is expected to allocate space for the argument `record` based
      on the size of the relation. This function will only copy the result of
      the projection onto the array pointed to by the argument.
*/
int BlockAccess::project(int relId, Attribute *record) {
    // get the previous search index of the relation relId from the relation
    // cache (use RelCacheTable::getSearchIndex() function)
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    // declare block and slot which will be used to store the record id of the
    // slot we need to check.
    int block, slot;

    /* if the current search index record is invalid(i.e. = {-1, -1})
       (this only happens when the caller reset the search index)
    */
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        // (new project operation. start from beginning)

        // get the first record block of the relation from the relation cache
        // (use RelCacheTable::getRelCatEntry() function of Cache Layer)
        RelCatEntry relCatEntryBuffer;
        RelCacheTable::getRelCatEntry(relId, &relCatEntryBuffer);

        // block = first record block of the relation
        // slot = 0
        block = relCatEntryBuffer.firstBlk;
        slot = 0;
    }
    else
    {
        // (a project/search operation is already in progress)

        // block = previous search index's block
        // slot = previous search index's slot + 1
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }


    // The following code finds the next record of the relation
    /* Start from the record id (block, slot) and iterate over the remaining
       records of the relation */
    while (block != -1)
    {
        // create a RecBuffer object for block (using appropriate constructor!)
        RecBuffer currentBlockBuffer (block);

        // get header of the block using RecBuffer::getHeader() function
        HeadInfo currentBlockheader;
        currentBlockBuffer.getHeader(&currentBlockheader);
        // get slot map of the block using RecBuffer::getSlotMap() function
        unsigned char slotMap[currentBlockheader.numSlots];
        currentBlockBuffer.getSlotMap(slotMap);

        if(slot >= currentBlockheader.numSlots)
        {
            // (no more slots in this block)
            // update block = right block of block
            // update slot = 0
            // (NOTE: if this is the last block, rblock would be -1. this would
            //        set block = -1 and fail the loop condition )
            block = currentBlockheader.rblock;
            slot = 0;
        }
        else if (slotMap[slot] = SLOT_UNOCCUPIED)
        { // (i.e slot-th entry in slotMap contains SLOT_UNOCCUPIED)

            // increment slot
            slot++;
        }
        else {
            // (the next occupied slot / record has been found)
            break;
        }
    }

    if (block == -1){
        // (a record was not found. all records exhausted)
        return E_NOTFOUND;
    }

    // declare nextRecId to store the RecId of the record found
    RecId nextRecId{block, slot};

    // set the search index to nextRecId using RelCacheTable::setSearchIndex
    RelCacheTable::setSearchIndex(relId, &nextRecId);

    /* Copy the record with record id (nextRecId) to the record buffer (record)
       For this Instantiate a RecBuffer class object by passing the recId and
       call the appropriate method to fetch the record
    */
   RecBuffer recordBuffer(block);
   recordBuffer.getRecord(record, slot);

    return SUCCESS;
}