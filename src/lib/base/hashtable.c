/* hashtable - Fast, simple array-based hash table, optimized for
   'put' and 'get' ('delete' somewhat inefficient).  Stores copies of
   keys but not of data objects, which are managed as void*s (memory
   management expected to be done externally) */

/* $Id: hashtable.c,v 1.1.1.1 2004-06-03 22:43:11 acs Exp $ 
   Written by Adam Siepel, 2002.
   Copyright 2002, Adam Siepel, University of California.
*/

#define MULTIPLIER 31		/* recommended by Kernighan and Pike */
#define LOADING_FACTOR 5

#include <stdlib.h>
#include <assert.h>
#include <lists.h>
#include <hashtable.h>
#include <string.h>
#include <math.h>
#include <misc.h>

Hashtable* hsh_new(int est_capacity) {
  Hashtable* ht;
  int i;
  ht = (Hashtable*)smalloc(sizeof(Hashtable));
  ht->nbuckets = ceil(est_capacity*1.0/LOADING_FACTOR);
  assert(ht->nbuckets > 0);
  ht->keys = (List**)calloc(ht->nbuckets, sizeof(List*));
  ht->vals = (List**)calloc(ht->nbuckets, sizeof(List*));
  for (i = 0; i < ht->nbuckets; i++) 
    ht->keys[i] = ht->vals[i] = NULL;    
  return ht;
}

void hsh_put(Hashtable *ht, char* key, void* val) {
  unsigned int bucket = hsh_hash_func(ht, key);
  char *keycpy;
  if (ht->keys[bucket] == NULL) {
    ht->keys[bucket] = lst_new_ptr(LOADING_FACTOR);
    ht->vals[bucket] = lst_new_ptr(LOADING_FACTOR);
  }
  keycpy = smalloc(sizeof(char) * (strlen(key) + 1));
  strcpy(keycpy, key);

  lst_push_ptr(ht->keys[bucket], keycpy);
  lst_push_ptr(ht->vals[bucket], val);

}

/* Equality function to pass to list_find_equivalent; simply tests for
   exact string match */
int equal(void *key1ptr, void *key2) {
  return !strcmp(*((char**)key1ptr), (char*)key2);
}

void* hsh_get(Hashtable* ht, char *key) {
  unsigned int bucket;
  int idx;
  bucket = hsh_hash_func(ht, key);
  if (ht->keys[bucket] == NULL || 
      (idx = lst_find_compare(ht->keys[bucket], key, equal)) == -1) 
    return (void*)-1;
  
  return lst_get_ptr(ht->vals[bucket], idx);
}

int hsh_delete(Hashtable* ht, char *key) {
  unsigned int bucket;
  int idx;
  bucket = hsh_hash_func(ht, key);
  if (ht->keys[bucket] == NULL || 
      (idx = lst_find_compare(ht->keys[bucket], key, equal)) == -1) 
    return 0;

  lst_delete_idx(ht->keys[bucket], idx);
  lst_delete_idx(ht->vals[bucket], idx);
  return 1;
}

void hsh_free(Hashtable *ht) {
  int i, j;
  for (i = 0; i < ht->nbuckets; i++) {
    if (ht->keys[i] != NULL) {
      for (j = 0; j < lst_size(ht->keys[i]); j++)
        free(lst_get_ptr(ht->keys[i], j));
      lst_free(ht->keys[i]);
      lst_free(ht->vals[i]);      
    }
  }
  free(ht->keys);
  free(ht->vals);
  free(ht);
}

void hsh_free_with_vals(Hashtable *ht) {
  int i, j;
  for (i = 0; i < ht->nbuckets; i++) {
    if (ht->keys[i] != NULL) {
      for (j = 0; j < lst_size(ht->keys[i]); j++)
        free(lst_get_ptr(ht->keys[i], j));
      for (j = 0; j < lst_size(ht->vals[i]); j++)
        free(lst_get_ptr(ht->vals[i], j));
      lst_free(ht->keys[i]);
      lst_free(ht->vals[i]);      
    }
  }
  free(ht->keys);
  free(ht->vals);
  free(ht);
}

/* Hash function */
unsigned int hsh_hash_func(Hashtable *ht, char* key) {
  unsigned int h = 0;
  int i = 0;
  for (i = 0; key[i] != '\0'; i++)
    h = MULTIPLIER * h + key[i];
  return h % ht->nbuckets;
}
