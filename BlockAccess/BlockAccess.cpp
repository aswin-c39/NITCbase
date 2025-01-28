#include "BlockAccess.h"

#include <cstring>

RecId BlockAccess::linearSearch(int relId, char *attrName, Attribute attrVal, int op) {
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
                (op == LT && cmpVal < 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == EQ && cmpVal == 0) ||
                (op == GT && cmpVal > 0) ||
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