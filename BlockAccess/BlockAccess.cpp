#include "BlockAccess.h"

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
        RecBuffer attrCatBlock(searchindex.block);
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
