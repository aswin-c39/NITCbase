#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <cstring>
#include <iostream>

void updateAttributeName(const char* relName, const char* oldAttrName, const char* newAttrName) {

  RecBuffer attrCatBuffer(ATTRCAT_BLOCK);

  HeadInfo attrCatHeader;
  attrCatBuffer.getHeader(&attrCatHeader);

  for(int recIndex = 0; recIndex < attrCatHeader.numEntries; recIndex++) {
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBuffer.getRecord(attrCatRecord, recIndex);

    if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName) && strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldAttrName) == 0) {
      strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttrName);
      attrCatBuffer.setRecord(attrCatRecord, recIndex);
      std::cout << "Attribute Name Updated Successfully \n\n";
      break;
    }

    if(recIndex == attrCatHeader.numSlots - 1) {
      recIndex = -1;
      attrCatBuffer = RecBuffer(attrCatHeader.rblock);
      attrCatBuffer.getHeader(&attrCatHeader);
    }
  }
}

void printAttributeCatalog() {
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  RecBuffer attrCatBuffer(ATTRCAT_BLOCK);

  HeadInfo relCatHeader;
  HeadInfo attrCatHeader;

  relCatBuffer.getHeader(&relCatHeader);
  attrCatBuffer.getHeader(&attrCatHeader);

  int attrCatSlotIndex = 0;

  for(int i = 0; i < relCatHeader.numEntries; i++) {

    Attribute relCatRecord[RELCAT_NO_ATTRS];

    relCatBuffer.getRecord(relCatRecord, i);

    printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    for(int j = 0;  j < relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal; j++, attrCatSlotIndex++) {

      attrCatBuffer.getRecord(attrCatRecord, attrCatSlotIndex);
      
      if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) {
        const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
      
        printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
      }

      if(attrCatSlotIndex == attrCatHeader.numSlots - 1) {
        attrCatSlotIndex = -1;
        attrCatBuffer = RecBuffer (attrCatHeader.rblock);
        attrCatBuffer.getHeader(&attrCatHeader);
      }
    }
    printf("\n");
  }
}

int main(int argc, char *argv[]) {
    Disk disk_run;
    StaticBuffer bufferCache;
    OpenRelTable cache;

    return FrontendInterface::handleFrontend(argc, argv);
    
}