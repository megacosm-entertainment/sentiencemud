#ifndef __ARRAY_H__
#define __ARRAY_H__

#include <stdbool.h>        // For "bool"
#include <stddef.h>
#include <stdint.h>

typedef struct array_type ARRAY;
typedef void (ARRAY_DELETER_FUNC)(void *data);
typedef void (ARRAY_COPIER_FUNC)(void *ptr, void *src);
typedef char *(ARRAY_STRINGER_FUNC)(const void *data);

/* Callback for array_map: transform one element into another.
 * dst  -- destination slot in the output array (write your result here)
 * src  -- source element in the input array (read-only)
 * The output ARRAY's copier/deleter govern dst's memory. */
typedef void (ARRAY_MAP_FUNC)(void *dst, const void *src);

/* Callback for array_filter: return true to keep the element. */
typedef bool (ARRAY_FILTER_FUNC)(const void *data);

/* Callback for array_reduce: fold src into accumulator.
 * acc  -- the running accumulator (read/write)
 * src  -- the current element (read-only) */
typedef void (ARRAY_REDUCE_FUNC)(void *acc, const void *src);

/* Callback for array_foreach: called once per element.
 * data     -- pointer to the element (read/write)
 * index    -- zero-based position in the array
 * userdata -- caller-supplied context pointer (may be NULL) */
typedef void (ARRAY_FOREACH_FUNC)(void *data, size_t index, void *userdata);

/* Callback for array_find / array_find_all: return true if the element
 * matches the search criteria.
 * data     -- pointer to the element (read-only)
 * userdata -- caller-supplied context pointer (may be NULL) */
typedef bool (ARRAY_FIND_FUNC)(const void *data, void *userdata);

/* Callback for array_sort: classic comparator (same contract as qsort).
 * Return <0 if *a sorts before *b, 0 if equal, >0 if *a sorts after *b. */
typedef int (ARRAY_COMPARE_FUNC)(const void *a, const void *b);

// Data structure to hold allocated array data
struct array_type {
    void *ptr;              // Pointer to allocated memory
    size_t size;            // Size of each element
    size_t length;          // Number of elements

    ARRAY_DELETER_FUNC *deleter;    // Used to delete the elements of the ARRAY safely, if specified
    ARRAY_COPIER_FUNC *copier;
};


/* =========================================================================
 * Core array operations
 * ======================================================================= */

bool is_array_empty(ARRAY *arr);

/* Create an array with optional deleter/copier callbacks */
ARRAY *new_arrayx(size_t length, size_t size,
                  ARRAY_DELETER_FUNC *deleter, ARRAY_COPIER_FUNC *copier);

/* Create a plain array (no callbacks) */
ARRAY *new_array(size_t length, size_t size);

/* Deep-copy an array */
ARRAY *copy_array(ARRAY *arr);

/* Free an array and all its elements */
void free_array(ARRAY *arr);

/* Return a pointer to the element at index (allows in-place modification) */
void *array_get(ARRAY *arr, size_t index);

/* Copy data into the element at index (requires copier) */
bool array_set(ARRAY *arr, size_t index, void *data);

/* Resize the array, freeing excess elements when shrinking */
bool array_set_length(ARRAY *arr, size_t new_length);

/* Append a single element to the end (requires copier) */
bool array_append(ARRAY *arr, void *data);

/* Insert a single element at index, shifting later elements up (requires copier).
 * index == arr->length is equivalent to array_append. */
bool array_insert(ARRAY *arr, size_t index, void *data);

/* Remove the element at index, shifting later elements down.
 * Calls the deleter on the removed element if one is set. */
bool array_remove(ARRAY *arr, size_t index);

/* =========================================================================
 * Typed array constructors
 * ======================================================================= */

ARRAY *new_int_array(size_t length);
ARRAY *new_float_array(size_t length);
ARRAY *new_string_array(size_t length);

/* =========================================================================
 * Split: parse a formatted string into a typed array
 *
 * Accepted formats:
 *   split_string_array  ->  ["hello","caf\u00e9","\uD83D\uDE00",NULL]
 *
 * NULL handling in split_string_array / join_string_array:
 *   - A bare (unquoted) NULL token in the input sets that element to a
 *     null char* (the slot holds a null pointer, not the string "NULL").
 *   - Matching is case-insensitive: NULL, null, Null, etc. all work.
 *   - join_string_array emits a bare NULL token for null-pointer elements.
 *   split_int_array     ->  [1,3,7,1]
 *   split_float_array   ->  [1.3,7.4,89.0]
 *
 * All functions:
 *   - Tolerate whitespace around brackets, commas, and values
 *   - Return NULL on NULL input, malformed input, or allocation failure
 *   - Return a heap-allocated ARRAY; caller must free_array() it
 *
 * split_string_array additionally:
 *   - Handles raw UTF-8 sequences in string values
 *   - Decodes \uXXXX escapes (BMP codepoints) to UTF-8
 *   - Decodes surrogate pairs (\uD800\uDCxx) for codepoints > U+FFFF
 *   - Decodes standard JSON escapes: \\ \" \/ \b \f \n \r \t
 *   - Encodes invalid UTF-8 bytes as \uFFFD on round-trip
 * ======================================================================= */
ARRAY *split_string_array(const char *input);
ARRAY *split_int_array(const char *input);
ARRAY *split_float_array(const char *input);

/* =========================================================================
 * Join: format a typed array into a string
 *
 * Generic form takes an explicit stringer callback.
 * Concrete helpers use built-in stringers for each type.
 *
 * All functions:
 *   - Return a newly allocated NUL-terminated string
 *   - Caller must free() the returned string
 *   - Return NULL on NULL/empty array or allocation failure
 *
 * join_string_array:
 *   - Wraps each element in double-quotes
 *   - Escapes control chars, '"', and '\'
 *   - Passes valid UTF-8 multi-byte sequences through unchanged
 *   - Encodes codepoints > U+FFFF as surrogate pairs
 *
 * join_float_array:
 *   - Uses %.17g format to preserve full double precision
 * ======================================================================= */

char *array_join(ARRAY *arr, ARRAY_STRINGER_FUNC *stringer);
char *join_int_array(ARRAY *arr);
char *join_float_array(ARRAY *arr);
char *join_string_array(ARRAY *arr);

/* =========================================================================
 * Stack operations  (LIFO — top is the last element)
 *
 *   array_stack_push   --  copy data onto the top of the stack
 *   array_stack_pop    --  copy the top element into out then remove it;
 *                          pass out=NULL to discard the value
 *   array_stack_peek   --  return a pointer to the top element without
 *                          removing it (invalidated by any reallocation)
 *
 * push requires a copier.  pop calls the deleter (if set) after copying.
 * All three return false / NULL on an empty array or missing copier.
 * ======================================================================= */

bool  array_stack_push(ARRAY *arr, void *data);
bool  array_stack_pop (ARRAY *arr, void *out);
void *array_stack_peek(ARRAY *arr);

/* =========================================================================
 * Queue operations  (FIFO — enqueue at tail, dequeue from head)
 *
 *   array_queue_enqueue  --  copy data onto the tail of the queue
 *   array_queue_dequeue  --  copy the head element into out then remove it;
 *                            pass out=NULL to discard the value
 *   array_queue_peek     --  return a pointer to the head element without
 *                            removing it (same invalidation caveat as peek)
 *
 * enqueue requires a copier.  dequeue calls the deleter (if set) after
 * copying.
 * ======================================================================= */

bool  array_queue_enqueue(ARRAY *arr, void *data);
bool  array_queue_dequeue(ARRAY *arr, void *out);
void *array_queue_peek   (ARRAY *arr);


/* =========================================================================
 * Functional operations
 *
 *   array_map     --  produce a new ARRAY by transforming every element
 *   array_filter  --  produce a new ARRAY containing only matching elements
 *   array_reduce  --  fold all elements into a single accumulator value
 *   array_foreach --  iterate over all elements, calling a function for each
 * ======================================================================= */

/* array_map
 *
 * Creates a new array of the same length using out_size, out_deleter, and
 * out_copier for the output type.  For each element, calls:
 *     map_fn(out_slot, in_slot)
 * The mapper writes the transformed value directly into out_slot; the output
 * array's copier is NOT called on top of that, so the mapper is fully
 * responsible for initialising each destination slot.
 *
 * Returns the new array, or NULL on allocation failure. */
ARRAY *array_map(ARRAY *arr,
                 size_t out_size,
                 ARRAY_DELETER_FUNC *out_deleter,
                 ARRAY_COPIER_FUNC  *out_copier,
                 ARRAY_MAP_FUNC     *map_fn);

/* array_filter
 *
 * Creates a new array containing only elements for which filter_fn returns
 * true.  The output array shares the same size/deleter/copier as the input;
 * elements are deep-copied into the new array via the input's copier.
 * Requires a copier on the input array.
 *
 * Returns the new array (may have length 0), or NULL on allocation failure
 * or missing copier.  A zero-length result is a valid empty array. */
ARRAY *array_filter(ARRAY *arr, ARRAY_FILTER_FUNC *filter_fn);

/* array_reduce
 *
 * Folds every element into *acc by calling:
 *     reduce_fn(acc, element)
 * acc must point to a caller-managed buffer of appropriate size; it is
 * neither allocated nor freed by array_reduce.  The caller should
 * initialise *acc to the desired seed value before calling.
 *
 * Returns true on success, false if arr or reduce_fn is NULL. */
bool array_reduce(ARRAY *arr, void *acc, ARRAY_REDUCE_FUNC *reduce_fn);

/* array_foreach
 *
 * Calls foreach_fn(element_ptr, index, userdata) for every element in
 * order.  The element pointer is a direct reference into the array's
 * storage; modifications are visible immediately.
 * userdata is passed through unchanged and may be NULL.
 *
 * Returns true on success, false if arr or foreach_fn is NULL. */
bool array_foreach(ARRAY *arr, ARRAY_FOREACH_FUNC *foreach_fn, void *userdata);

/* =========================================================================
 * Search, slice, concat, sort
 * ======================================================================= */

/* array_concat
 *
 * Returns a new array containing all elements of 'a' followed by all
 * elements of 'b'.  Both arrays must have the same element size.
 * Elements are deep-copied using 'a's copier (both arrays must share the
 * same copier, or at minimum 'a' must have one).
 * Returns NULL on mismatched sizes, missing copier, or allocation failure. */
ARRAY *array_concat(ARRAY *a, ARRAY *b);

/* array_slice
 *
 * Returns a new array containing elements [start, end).
 * 'end' is clamped to arr->length if it exceeds it.
 * Elements are deep-copied via arr's copier (required).
 * Returns NULL on invalid range (start >= end after clamping), missing
 * copier, or allocation failure. */
ARRAY *array_slice(ARRAY *arr, size_t start, size_t end);

/* array_find
 *
 * Returns the index of the first element for which find_fn returns true,
 * or (size_t)-1 if no match is found.
 * userdata is forwarded to every find_fn call unchanged. */
size_t array_find(ARRAY *arr, ARRAY_FIND_FUNC *find_fn, void *userdata);

/* array_find_all
 *
 * Returns a new int array containing the indices of every element for
 * which find_fn returns true, in ascending order.
 * Returns an empty (length==1, value==0) int array if nothing matches.
 * Returns NULL on allocation failure.
 * The returned array has no copier/deleter; free it with free_array(). */
ARRAY *array_find_all(ARRAY *arr, ARRAY_FIND_FUNC *find_fn, void *userdata);

/* array_sort
 *
 * Sorts the array in-place using qsort with the supplied comparator.
 * The comparator receives pointers to elements (same as qsort).
 * Returns false if arr or cmp is NULL. */
bool array_sort(ARRAY *arr, ARRAY_COMPARE_FUNC *cmp);

#endif /* __ARRAY_H__ */

