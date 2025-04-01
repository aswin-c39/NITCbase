#include "AttrCacheTable.h"

#include <cstring>

AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];

int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuf) {
    if(relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if(attrCache[relId] == nullptr)
        return E_RELNOTOPEN;

    for(AttrCacheEntry* entry = attrCache[relId]; entry  != nullptr; entry = entry->next) {
        if(entry->attrCatEntry.offset == attrOffset) {
            strcpy(attrCatBuf->relName, entry->attrCatEntry.relName);
            strcpy(attrCatBuf->attrName, entry->attrCatEntry.attrName);

            attrCatBuf->attrType = entry->attrCatEntry.attrType;
            attrCatBuf->primaryFlag = entry->attrCatEntry.primaryFlag;
            attrCatBuf->rootBlock = entry->attrCatEntry.rootBlock;
            attrCatBuf->offset = entry->attrCatEntry.offset;

            return SUCCESS;
        }
    }
    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatBuf) {
    if(relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;
    
    if(attrCache[relId] == nullptr)
        return E_RELNOTOPEN;
    
    for(AttrCacheEntry *entry = attrCache[relId]; entry != nullptr;  entry = entry->next) {
        if(strcmp(entry->attrCatEntry.attrName, attrName) == 0){
            strcpy(attrCatBuf->relName, entry->attrCatEntry.relName);
            strcpy(attrCatBuf->attrName, entry->attrCatEntry.attrName);

            attrCatBuf->attrType = entry->attrCatEntry.attrType;
            attrCatBuf->primaryFlag = entry->attrCatEntry.primaryFlag;
            attrCatBuf->rootBlock = entry->attrCatEntry.rootBlock;
            attrCatBuf->offset = entry->attrCatEntry.offset;

            return SUCCESS;
        }
    }
    return E_ATTRNOTEXIST;
}

void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry* attrCatEntry) {
    strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
    strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);

    attrCatEntry->attrType = record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
    attrCatEntry->primaryFlag = record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
    attrCatEntry->rootBlock = record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
    attrCatEntry->offset = record[ATTRCAT_OFFSET_INDEX].nVal;    
}

void AttrCacheTable::attrCatEntryToRecord(AttrCatEntry *attrCatEntry, Attribute record[ATTRCAT_NO_ATTRS])
{
    strcpy(record[ATTRCAT_REL_NAME_INDEX].sVal, attrCatEntry->relName);
    strcpy(record[ATTRCAT_ATTR_NAME_INDEX].sVal, attrCatEntry->attrName);

    record[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrCatEntry->attrType;
    record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = attrCatEntry->primaryFlag;
    record[ATTRCAT_ROOT_BLOCK_INDEX].nVal = attrCatEntry->rootBlock;
    record[ATTRCAT_OFFSET_INDEX].nVal = attrCatEntry->offset;

    // copy the rest of the fields in the record to the attrCacheEntry struct
}

int AttrCacheTable::getSearchIndex(int relId, int attrOffset, IndexId *searchIndex) {

  if(relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if(AttrCacheTable::attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  } 
 AttrCacheEntry *curr = AttrCacheTable::attrCache[relId];
 int i = 0;

  while(curr)
  {
    if (i == attrOffset)
    {
      //copy the searchIndex field of the corresponding Attribute Cache entry
      //in the Attribute Cache Table to input searchIndex variable.
      *searchIndex = curr->searchIndex;
      return SUCCESS;
    }
    curr = curr->next;
    i++;
  }

  return E_ATTRNOTEXIST;
}

int AttrCacheTable::getSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {

  if(relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  AttrCacheEntry *curr = AttrCacheTable::attrCache[relId];

  if(curr == nullptr) {
    return E_RELNOTOPEN;
  } 

  while(curr)
  {
    if (strcmp(curr->attrCatEntry.attrName, attrName) == 0)
    {
      //copy the searchIndex field of the corresponding Attribute Cache entry
      //in the Attribute Cache Table to input searchIndex variable.
      *searchIndex = curr->searchIndex;
      return SUCCESS;
    }
    curr = curr->next;
  }

  return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {

  if(relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  AttrCacheEntry *curr = AttrCacheTable::attrCache[relId];

  if(curr == nullptr) {
    return E_RELNOTOPEN;
  } 

  while(curr)
  {
    if (strcmp(curr->attrCatEntry.attrName , attrName) == 0)
    {
      // copy the input searchIndex variable to the searchIndex field of the
      //corresponding Attribute Cache entry in the Attribute Cache Table.
      curr->searchIndex = *searchIndex;
      return SUCCESS;
    }
    curr = curr->next;
  }

  return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, int attrOffset, IndexId *searchIndex) {

  if(relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  AttrCacheEntry *curr = AttrCacheTable::attrCache[relId];

  if(curr == nullptr) {
    return E_RELNOTOPEN;
  } 
  int i = 0;

  while(curr)
  {
    if (i == attrOffset)
    {
      // copy the input searchIndex variable to the searchIndex field of the
      //corresponding Attribute Cache entry in the Attribute Cache Table.
      curr->searchIndex = *searchIndex;
      return SUCCESS;
    }
    curr = curr->next;
    i++;
  }

  return E_ATTRNOTEXIST;
}

int AttrCacheTable::resetSearchIndex(int relId, char attrName[ATTR_SIZE]) {

  // declare an IndexId having value {-1, -1}
  IndexId searchIndex;
  searchIndex.block = -1, searchIndex.index = -1;
  // set the search index to {-1, -1} using AttrCacheTable::setSearchIndex
  // return the value returned by setSearchIndex
  return AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
}

int AttrCacheTable::resetSearchIndex(int relId, int attrOffset) {

  // declare an IndexId having value {-1, -1}
  IndexId searchIndex;
  searchIndex.block = -1, searchIndex.index = -1;
  // set the search index to {-1, -1} using AttrCacheTable::setSearchIndex
  // return the value returned by setSearchIndex
  return AttrCacheTable::setSearchIndex(relId, attrOffset, &searchIndex);
}

int AttrCacheTable::setAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatBuf) {

  if(relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if(attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  for(AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next)
  {
    if(strcmp(entry->attrCatEntry.attrName, attrName) == 0)
    {
      // copy the attrCatBuf to the corresponding Attribute Catalog entry in
      // the Attribute Cache Table.
      entry->attrCatEntry = *attrCatBuf;

      // set the dirty flag of the corresponding Attribute Cache entry in the
      // Attribute Cache Table.
      entry->dirty = true;
      return SUCCESS;
    }
  }

  return E_ATTRNOTEXIST;
}

int AttrCacheTable::setAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatBuf) {

  if(relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if(attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  for(AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next)
  {
    if(entry->attrCatEntry.offset == attrOffset)
    {
      // copy the attrCatBuf to the corresponding Attribute Catalog entry in
      // the Attribute Cache Table.
      entry->attrCatEntry = *attrCatBuf;

      // set the dirty flag of the corresponding Attribute Cache entry in the
      // Attribute Cache Table.
      entry->dirty = true;

      return SUCCESS;
    }
  }

  return E_ATTRNOTEXIST;
}