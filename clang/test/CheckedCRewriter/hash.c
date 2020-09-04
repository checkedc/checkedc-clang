// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
/*
 * Based on hash.c in Very Secure FTPd
 */

//#include <stdlib_checked.h>
//#include <string_checked.h>
//#include <stdio_checked.h>

#include <stddef.h>
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *memcpy(void * restrict dest : itype(restrict _Array_ptr<T>) byte_count(n),
             const void * restrict src : itype(restrict _Array_ptr<const T>) byte_count(n),
             size_t n) : itype(_Array_ptr<T>) byte_count(n);
extern void *memset(void * dest : byte_count(n),
             int c,
             size_t n) : bounds(dest, (_Array_ptr<char>)dest + n);
extern int memcmp(const void *src1 : byte_count(n), const void *src2 : byte_count(n),
           size_t n);

_Itype_for_any(T) void
vsf_sysutil_memclr(void* p_dest : itype(_Array_ptr<T>) byte_count(size), unsigned int size)
{
  /* Safety */
  if (size == 0)
  {
    return;
  }
  memset(p_dest, '\0', size);
}

typedef unsigned int (*hashfunc_t)(unsigned int, void*);

struct hash* hash_alloc(unsigned int buckets, unsigned int key_size,
                        unsigned int value_size, hashfunc_t hash_func);
void* hash_lookup_entry(struct hash* p_hash, void* p_key);
void hash_add_entry(struct hash* p_hash, void* p_key, void* p_value);
void hash_free_entry(struct hash* p_hash, void* p_key);
//CHECK_ALL: _Ptr<struct hash> hash_alloc(unsigned int buckets, unsigned int key_size, unsigned int value_size, _Ptr<unsigned int (unsigned int , void *)> hash_func);
//CHECK_ALL: void * hash_lookup_entry(_Ptr<struct hash> p_hash, void *p_key);
//CHECK_ALL: void hash_add_entry(_Ptr<struct hash> p_hash, void *p_key, void *p_value);
//CHECK_ALL: void hash_free_entry(_Ptr<struct hash> p_hash, void *p_key);

#define bug(s) {  }

struct hash_node
{
  void* p_key;
  void* p_value;
  struct hash_node* p_prev;
  struct hash_node* p_next;
//CHECK_ALL:  _Ptr<struct hash_node> p_prev;
//CHECK_ALL:  _Ptr<struct hash_node> p_next;
};

struct hash
{
  unsigned int buckets;
  unsigned int key_size;
  unsigned int value_size;
  hashfunc_t hash_func;
  struct hash_node** p_nodes;
//CHECK_ALL:  _Ptr<unsigned int (unsigned int , void *)> hash_func;
//CHECK_ALL:  _Array_ptr<_Ptr<struct hash_node>> p_nodes : count(buckets);
};

/* Internal functions */
struct hash_node** hash_get_bucket(struct hash* p_hash, void* p_key);
struct hash_node* hash_get_node_by_key(struct hash* p_hash, void* p_key);
//CHECK_ALL: _Array_ptr<_Ptr<struct hash_node>> hash_get_bucket(_Ptr<struct hash> p_hash, void *p_key);
//CHECK_ALL: _Ptr<struct hash_node> hash_get_node_by_key(_Ptr<struct hash> p_hash, void *p_key);

struct hash*
hash_alloc(unsigned int buckets, unsigned int key_size,
           unsigned int value_size, hashfunc_t hash_func)
//CHECK_ALL: _Ptr<struct hash> hash_alloc(unsigned int buckets, unsigned int key_size, unsigned int value_size, _Ptr<unsigned int (unsigned int , void *)> hash_func)
{
  unsigned int size;
  struct hash* p_hash = malloc(sizeof(*p_hash));
//CHECK_ALL:  _Ptr<struct hash> p_hash =  malloc<struct hash>(sizeof(*p_hash));
  p_hash->buckets = buckets;
  p_hash->key_size = key_size;
  p_hash->value_size = value_size;
  p_hash->hash_func = hash_func;
  size = (unsigned int) sizeof(struct hash_node*) * buckets;
  //FIX eventually:
  //p_hash->p_nodes = malloc(size);
  p_hash->p_nodes = malloc(sizeof(struct hash_node*) * buckets);
//CHECK_ALL:  p_hash->p_nodes = malloc<_Ptr<struct hash_node>>(sizeof(struct hash_node*) * buckets);
  vsf_sysutil_memclr(p_hash->p_nodes, size);
  return p_hash;
}

void*
hash_lookup_entry(struct hash* p_hash, void* p_key)
//CHECK_ALL: void * hash_lookup_entry(_Ptr<struct hash> p_hash, void *p_key)
{
  struct hash_node* p_node = hash_get_node_by_key(p_hash, p_key);
//CHECK_ALL:  _Ptr<struct hash_node> p_node =  hash_get_node_by_key(p_hash, p_key);
  if (!p_node)
  {
    //FIX maybe one day:
    //return p_node;
    return NULL;
  }
  return p_node->p_value;
}

void
hash_add_entry(struct hash* p_hash, void* p_key, void* p_value)
//CHECK_ALL: void hash_add_entry(_Ptr<struct hash> p_hash, void *p_key, void *p_value)
{
  struct hash_node** p_bucket;
  struct hash_node* p_new_node;
//CHECK_ALL:  _Ptr<_Ptr<struct hash_node>> p_bucket = ((void *)0);
//CHECK_ALL:  _Ptr<struct hash_node> p_new_node = ((void *)0);
  if (hash_lookup_entry(p_hash, p_key))
  {
    bug("duplicate hash key");
  }
  p_bucket = hash_get_bucket(p_hash, p_key);
  p_new_node = malloc(sizeof(*p_new_node));
//CHECK_ALL:  p_new_node = malloc<struct hash_node>(sizeof(*p_new_node));
  p_new_node->p_prev = 0;
  p_new_node->p_next = 0;

  p_new_node->p_key = malloc(p_hash->key_size);
  memcpy(p_new_node->p_key, p_key, p_hash->key_size);
  p_new_node->p_value = malloc(p_hash->value_size);
  memcpy(p_new_node->p_value, p_value, p_hash->value_size);
  if (!*p_bucket)
  {
    *p_bucket = p_new_node;    
  }
  else
  {
    p_new_node->p_next = *p_bucket;
    (*p_bucket)->p_prev = p_new_node;
    *p_bucket = p_new_node;
  }
}

void
hash_free_entry(struct hash* p_hash, void* p_key)
//CHECK_ALL: void hash_free_entry(_Ptr<struct hash> p_hash, void *p_key)
{
  struct hash_node* p_node = hash_get_node_by_key(p_hash, p_key);
//CHECK_ALL:  _Ptr<struct hash_node> p_node =  hash_get_node_by_key(p_hash, p_key);
  if (!p_node)
  {
    bug("hash node not found");
  }
  free(p_node->p_key);
  free(p_node->p_value);

  if (p_node->p_prev)
  {
    p_node->p_prev->p_next = p_node->p_next;
  }
  else
  {
    struct hash_node** p_bucket = hash_get_bucket(p_hash, p_key);
    *p_bucket = p_node->p_next;
  }
  if (p_node->p_next)
  {
    p_node->p_next->p_prev = p_node->p_prev;
  }
  free(p_node);
}

struct hash_node**
hash_get_bucket(struct hash* p_hash, void* p_key)
//CHECK_ALL: _Array_ptr<_Ptr<struct hash_node>> hash_get_bucket(_Ptr<struct hash> p_hash, void *p_key)
{
  unsigned int bucket = (*p_hash->hash_func)(p_hash->buckets, p_key);
  if (bucket >= p_hash->buckets)
  {
    bug("bad bucket lookup");
  }
  return &(p_hash->p_nodes[bucket]);
}

struct hash_node*
hash_get_node_by_key(struct hash* p_hash, void* p_key)
//CHECK_ALL: _Ptr<struct hash_node> hash_get_node_by_key(_Ptr<struct hash> p_hash, void *p_key)
{
  struct hash_node** p_bucket = hash_get_bucket(p_hash, p_key);
  struct hash_node* p_node = *p_bucket;
//CHECK_ALL:  _Ptr<_Ptr<struct hash_node>> p_bucket =  hash_get_bucket(p_hash, p_key);
//CHECK_ALL:  _Ptr<struct hash_node> p_node =  *p_bucket;
  if (!p_node)
  {
    return p_node;
  }
  while (p_node != 0 &&
         memcmp(p_key, p_node->p_key, p_hash->key_size) != 0)
  {
    p_node = p_node->p_next;
  }
  return p_node;
}
