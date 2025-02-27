#include "OpenRelTable.h"

#include <cstring>
#include <stdlib.h>
#include <stdio.h>

AttrCacheEntry* createAttrCacheEntryList (int size) {
    AttrCacheEntry *head = nullptr, *curr = nullptr;
    head = curr = (AttrCacheEntry*) malloc (sizeof(AttrCacheEntry));
    size--;
    while (size--) {
        curr->next = (AttrCacheEntry*) malloc (sizeof(AttrCacheEntry));
        curr = curr->next;
    }
    curr->next = nullptr;

    return head;
}

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable()
{

    // initialize relCache and attrCache with nullptr
    for (int i = 0; i < MAX_OPEN; ++i)
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
        tableMetaInfo[i].free = true;
    }

    /************ Setting up Relation Cache entries ************/
    // (we need to populate relation cache with entries for the relation catalog
    //  and attribute catalog.)

    // setting up the variables
    RecBuffer relCatBlock (RELCAT_BLOCK);
    Attribute relCatRecord [RELCAT_NO_ATTRS];
    RelCacheEntry *relCacheEntry = nullptr;

    for (int relId = RELCAT_RELID; relId <= ATTRCAT_RELID + 1; relId++) {
        relCatBlock.getRecord(relCatRecord, relId);

        relCacheEntry = (RelCacheEntry *) malloc (sizeof(RelCacheEntry));
        RelCacheTable::recordToRelCatEntry(relCatRecord, &(relCacheEntry->relCatEntry));
        relCacheEntry->recId.block = RELCAT_BLOCK;
        relCacheEntry->recId.slot = relId;

        RelCacheTable::relCache[relId] = relCacheEntry;
    }

    
    /************ Setting up Attribute cache entries ************/
    // (we need to populate attribute cache with entries for the relation catalog
    //  and attribute catalog.)

    // setting up the variables
    RecBuffer attrCatBlock (ATTRCAT_BLOCK);
    Attribute attrCatRecord [ATTRCAT_NO_ATTRS];
    AttrCacheEntry *attrCacheEntry = nullptr, *head = nullptr;

    for (int relId = RELCAT_RELID, recordId = 0; relId <= ATTRCAT_RELID+1; relId++) {
        int numberOfAttributes = RelCacheTable::relCache[relId]->relCatEntry.numAttrs;
        head = createAttrCacheEntryList (numberOfAttributes);
        attrCacheEntry = head;
        
        while (numberOfAttributes--) {
            attrCatBlock.getRecord(attrCatRecord, recordId);

            AttrCacheTable::recordToAttrCatEntry(
                attrCatRecord, 
                &(attrCacheEntry->attrCatEntry)
            );
            attrCacheEntry->recId.slot = recordId++;
            attrCacheEntry->recId.block = ATTRCAT_BLOCK;

            attrCacheEntry = attrCacheEntry->next;
        }

        AttrCacheTable::attrCache[relId] = head;
    }

    /************ Setting up tableMetaInfo entries ************/

  // in the tableMetaInfo array
  //   set free = false for RELCAT_RELID and ATTRCAT_RELID
  //   set relname for RELCAT_RELID and ATTRCAT_RELID
  tableMetaInfo[RELCAT_RELID].free = tableMetaInfo[ATTRCAT_RELID].free = false;
  strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);
  strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
}

OpenRelTable::~OpenRelTable()
{
    // free all the memory that you allocated in the constructor
    for(int i = 2; i < MAX_OPEN; i++) {
            if(!tableMetaInfo[i].free)
                OpenRelTable::closeRel(i);
        }
}
    
/* This function will open a relation having name `relName`.
Since we are currently only working with the relation and attribute catalog, we
will just hardcode it. In subsequent stages, we will loop through all the relations
and open the appropriate one.
*/
int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {

  /* traverse through the tableMetaInfo array,
    find the entry in the Open Relation Table corresponding to relName.*/
  
  for(int i = 0 ; i < MAX_OPEN; i++) {
    if(!tableMetaInfo[i].free && strcmp(tableMetaInfo[i].relName, relName) == 0)
      return i;
  }
  // if found return the relation id, else indicate that the relation do not
  // have an entry in the Open Relation Table.
  return E_RELNOTOPEN;
}

int OpenRelTable::getFreeOpenRelTableEntry() {

  /* traverse through the tableMetaInfo array,
    find a free entry in the Open Relation Table.*/
    // if found return the relation id, else return E_CACHEFULL.

    for(int i = 2; i < MAX_OPEN; i++) {
        if(tableMetaInfo[i].free)
            return i;
    }

    return E_CACHEFULL;

  
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) {


  int relId = getRelId(relName);
  if(relId != E_RELNOTOPEN){
    // (checked using OpenRelTable::getRelId())
    return relId;
    // return that relation id;
  }

  /* find a free slot in the Open Relation Table
     using OpenRelTable::getFreeOpenRelTableEntry(). */

     // let relId be used to store the free slot.

  relId = OpenRelTable::getFreeOpenRelTableEntry();
//printf("relid=%d",relId);
  if (relId == E_CACHEFULL)
    return E_CACHEFULL;

  /****** Setting up Relation Cache entry for the relation ******/

  /* search for the entry with relation name, relName, in the Relation Catalog using
      BlockAccess::linearSearch().
      Care should be taken to reset the searchIndex of the relation RELCAT_RELID
      before calling linearSearch().*/  
      Attribute attrVal;
      strcpy(attrVal.sVal, relName);
      //printf("Pass1\n");
      RelCacheTable::resetSearchIndex(RELCAT_RELID);
  // relcatRecId stores the rec-id of the relation `relName` in the Relation Catalog.
    //printf("Pass2\n");
      
  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, attrVal, EQ);
//printf("Pass3\n");
      
  if (relcatRecId.block == -1 && relcatRecId.slot == -1) {
    // (the relation is not found in the Relation Catalog.)
    return E_RELNOTEXIST;
  }

  /* read the record entry corresponding to relcatRecId and create a relCacheEntry
      on it using RecBuffer::getRecord() and RelCacheTable::recordToRelCatEntry().
      update the recId field of this Relation Cache entry to relcatRecId.
      use the Relation Cache entry to set the relId-th entry of the RelCacheTable.
    NOTE: make sure to allocate memory for the RelCacheEntry using malloc()
  */

  RecBuffer relationBuffer(relcatRecId.block);
  Attribute relationRecord [RELCAT_NO_ATTRS];
//printf("Pass5\n");
      
  relationBuffer.getRecord(relationRecord, relcatRecId.slot);
  RelCacheEntry* relCacheEntry = (RelCacheEntry*) malloc(sizeof(RelCacheEntry));
  RelCacheTable::recordToRelCatEntry(relationRecord, &(relCacheEntry->relCatEntry));
  relCacheEntry->recId.block = relcatRecId.block;
  relCacheEntry->recId.slot = relcatRecId.slot;
  RelCacheTable::relCache[relId] = relCacheEntry;
  //printf("Pass7\n");
      
  /****** Setting up Attribute Cache entry for the relation ******/

  // let listHead be used to hold the head of the linked list of attrCache entries.
  AttrCacheEntry* listHead;

  /*iterate over all the entries in the Attribute Catalog corresponding to each
  attribute of the relation relName by multiple calls of BlockAccess::linearSearch()
  care should be taken to reset the searchIndex of the relation, ATTRCAT_RELID,
  corresponding to Attribute Catalog before the first call to linearSearch().*/
  //{
      /* let attrcatRecId store a valid record id an entry of the relation, relName,
      in the Attribute Catalog.*/
      /* read the record entry corresponding to attrcatRecId and create an
      Attribute Cache entry on it using RecBuffer::getRecord() and
      AttrCacheTable::recordToAttrCatEntry().
      update the recId field of this Attribute Cache entry to attrcatRecId.
      add the Attribute Cache entry to the linked list of listHead .*/
      // NOTE: make sure to allocate memory for the AttrCacheEntry using malloc()
  //}

  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  AttrCacheEntry* attrCacheEntry = nullptr, *head = nullptr;

  int numberOfArributes = RelCacheTable::relCache[relId]->relCatEntry.numAttrs;
  head = createAttrCacheEntryList(numberOfArributes);
  attrCacheEntry = head;
//printf("Pass atttr\n");
      
  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
  
  while(numberOfArributes--)
  {
    RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, RELCAT_ATTR_RELNAME, attrVal,EQ);
  //printf("Pass while\n");
      
    RecBuffer attrCatBlock(attrcatRecId.block);
    attrCatBlock.getRecord(attrCatRecord, attrcatRecId.slot);

    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
    
    attrCacheEntry->recId.block = attrcatRecId.block;
    attrCacheEntry->recId.slot = attrcatRecId.slot;

    attrCacheEntry = attrCacheEntry->next;
  }
//printf("Pass after\n");
  //printf("relid=%d\n",relId);
      
  // set the relIdth entry of the AttrCacheTable to listHead.
  AttrCacheTable::attrCache[relId] = head;

  /****** Setting up metadata in the Open Relation Table for the relation******/

  // update the relIdth entry of the tableMetaInfo with free as false and
  // relName as the input.
  tableMetaInfo[relId].free = false;
  strcpy(tableMetaInfo[relId].relName, relName);

  return relId;
}


int OpenRelTable::closeRel(int relId) {
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
    return E_NOTPERMITTED;
  }

  if (relId <= 0 || relId > MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if (tableMetaInfo[relId].free == true) {
    return E_RELNOTOPEN;
  }

  // free the memory allocated in the relation and attribute caches which was
  // allocated in the OpenRelTable::openRel() function
  free(RelCacheTable::relCache[relId]);

  AttrCacheEntry *head = AttrCacheTable::attrCache[relId];
  AttrCacheEntry *next = head->next; 

  while(next != nullptr) {
    free(head);
    head = next;
    next = next->next;
  }
  free(head);
  // update `tableMetaInfo` to set `relId` as a free slot
  // update `relCache` and `attrCache` to set the entry at `relId` to nullptr
  tableMetaInfo[relId].free = true;
  RelCacheTable::relCache[relId] = nullptr;
  AttrCacheTable::attrCache[relId] = nullptr;

  return SUCCESS;
}
