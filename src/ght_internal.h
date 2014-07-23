/******************************************************************************
 *  LibGHT, software to manage point clouds.
 *  LibGHT is free and open source software provided by the Government of Canada
 *  Copyright (c) 2012 Natural Resources Canada
 *
 *  Nouri Sabo <nsabo@NRCan.gc.ca>, Natural Resources Canada
 *  Paul Ramsey <pramsey@opengeo.org>, OpenGeo
 *
 ******************************************************************************/

#ifndef _GHT_INTERNAL_H
#define _GHT_INTERNAL_H

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "ght_config.h"
#include "ght_core.h"
#include "ght_stringbuffer.h"
#include "ght_bytebuffer.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include "ght_stdint.h"
#endif

#include "libxml/xmlmemory.h"

#define GHT_NUM_TYPES 11
#define GHT_EPSILON 10e-8

/* Up to double/int64 */
#define GHT_ATTRIBUTE_MAX_SIZE  8

typedef enum {
	GHT_DUPES_NO = 0, GHT_DUPES_YES = 1
} GhtDuplicates;

typedef enum {
	GHT_IO_FILE, GHT_IO_MEM
} GhtIoType;

typedef enum {
	GHT_NONE, GHT_GLOBAL, GHT_SAME, GHT_CHILD, GHT_SPLIT
} GhtHashMatch;

typedef enum {
	GHT_GREATER_THAN, GHT_LESS_THAN, GHT_BETWEEN, GHT_EQUAL
} GhtFilterMode;

static char *GhtTypeStrings[] = { "unknown", "int8_t", "uint8_t", "int16_t",
		"uint16_t", "int32_t", "uint32_t", "int64_t", "uint64_t", "double",
		"float" };

static size_t GhtTypeSizes[] = { -1, /* GHT_UNKNOWN */
sizeof(int8_t), sizeof(uint8_t), /* GHT_INT8,   GHT_UINT8, */
sizeof(int16_t), sizeof(uint16_t), /* GHT_INT16,  GHT_UINT16 */
sizeof(int32_t), sizeof(uint32_t), /* GHT_INT32,  GHT_UINT32 */
sizeof(int64_t), sizeof(uint64_t), /* GHT_INT64,  GHT_UINT64 */
sizeof(double), sizeof(float) /* GHT_DOUBLE, GHT_FLOAT */
};

typedef struct {
	int position;
	char *name;
	char *description;
	GhtType type;
	double scale;
	double offset;
} GhtDimension;

typedef struct {
	int num_dims;
	int max_dims;
	GhtDimension **dims;
} GhtSchema;

typedef struct {
	GhtIoType type;
	FILE *file;
	char *filename;
	size_t filesize;
	bytebuffer_t *bytebuffer;
} GhtWriter;

typedef struct {
	GhtIoType type;
	FILE *file;
	char *filename;
	const uint8_t *bytes_start;
	const uint8_t *bytes_current;
	size_t bytes_size;
	const GhtSchema *schema;
	uint8_t endian;
	uint8_t version;
} GhtReader;

typedef struct {
	GhtRange range;
	GhtFilterMode mode;
	const GhtDimension *dim;
} GhtFilter;

typedef struct GhtAttribute_t {
	const GhtDimension *dim;
	struct GhtAttribute_t *next;
	char val[GHT_ATTRIBUTE_MAX_SIZE];
} GhtAttribute;

typedef struct {
	double min;
	double max;
	double sum;
	int count;
	GhtType type;
	const GhtDimension *dim;
} GhtAttributeStats;

struct GhtNodeList_t;

typedef struct {
	GhtHash *hash;

	uint8_t ghtFlag;  // TODO flag representé par 8 bits, c'est à dire 8 espaces pour des valuers

	struct GhtNodeList_t *children;
	GhtAttribute *attributes;
	double z_avg;   // TODO test pour la valeur Z moyenne
} GhtNode;

typedef struct GhtNodeList_t {
	int num_nodes;
	int max_nodes;
	GhtNode **nodes;
} GhtNodeList;

typedef struct {
	const GhtSchema *schema;
	GhtNode *root;
	int num_nodes;
	GhtConfig config;
} GhtTree;

/** Initialize memory/message handling with defaults (malloc/free/printf) */
void ght_init(void);

/** Allocate memory using runtime memory management */
void* ght_malloc(size_t size);
/** Free memory using runtime memory management */
void ght_free(void *ptr);
/** String duplication using runtime memory management */
char* ght_strdup(const char *str);
/** Memory resizing using runtime memory management */
void* ght_realloc(void *ptr, size_t size);
/** Send error message and stop */
void ght_error(const char *fmt, ...);
/** Send info message */
void ght_info(const char *fmt, ...);
/** Send warning message */
void ght_warn(const char *fmt, ...);

/**
 * Calculate the amount of charaters two GhtHashes have in common.
 * Returns -1 for two full hashes with nothing in common.
 * Returns 0 if one of the hashes is "", the "master hash".
 *
 * Examples:
 *   ("abcdef", "abc", 3) => 3
 *   ("abc", "abcdef", 3) => 3
 *   ("abc", "", 3) => 0
 *   ("abcdef", "abcdef", 2) => 2
 *   ("abc", "1abc", 3) => -1
 */
int ght_hash_common_length(const GhtHash *a, const GhtHash *b, int max_len);

/** Generate hash, up to resolution characters in length */
GhtErr ght_hash_from_coordinate(const GhtCoordinate *coord,
		unsigned int resolution, GhtHash **hash);

/** Generate area, since hash of finite resolution bounds an area */
GhtErr ght_area_from_hash(const GhtHash *hash, GhtArea *area);

/** Generate coordinate, as the mid-point of the GhtArea defined by a hash */
GhtErr ght_coordinate_from_hash(const GhtHash *hash, GhtCoordinate *coord);

/** Release hash memory */
GhtErr ght_hash_free(GhtHash *hash);

/** Write hash to byte buffer */
GhtErr ght_hash_write(const GhtHash *hash, GhtWriter *writer);

/** Read hash from byte buffer */
GhtErr ght_hash_read(GhtReader *reader, GhtHash **hash);

/** Make a copy of the input hash */
GhtErr ght_hash_clone(const GhtHash *hash, GhtHash **hash_new);

/**
 * Find the common parts of two hash strings and return pointers
 * to the unique bits. Also returns a code indicating the kind
 * of match between the hashes.
 *  GHT_NONE, no match ("acbde" "12345")
 *  GHT_GLOBAL, match with the global hash key ("" "abcde")
 *  GHT_SAME, identical hashes  ("abcde" "abcde")
 *  GHT_CHILD, hash b is child of a ("abc" "abcde")
 *  GHT_SPLIT, hash a and b share a common prefix ("abcde" "abcpq")
 *             so they will both need to be split
 */
GhtErr ght_hash_leaf_parts(const GhtHash *a, const GhtHash *b, int maxlen,
		GhtHashMatch *matchtype, GhtHash **a_leaf, GhtHash **b_leaf);

/** Free a node and all its children and attributes */
GhtErr ght_node_free(GhtNode *node);

/** Add node_to_insert to a tree of nodes headed by node */
GhtErr ght_node_insert_node(GhtNode *node, GhtNode *node_to_insert,
		GhtDuplicates duplicates);

/** Set the hash string on a node, takes ownership of hash */
GhtErr ght_node_set_hash(GhtNode *node, GhtHash *hash);

/** Get the coordinates represented by the node */
GhtErr ght_node_get_coordinate(const GhtNode *node, GhtCoordinate *coord);

/** Get the attribute list handing off the node */
GhtErr ght_node_get_attributes(const GhtNode *node, GhtAttribute **attr);



/** TODO Verification de la valeur de FLAG ght */
GhtErr ght_node_get_ghtFlag(const GhtNode *node);


/** Create a new node from a hash */
GhtErr ght_node_new_from_hash(GhtHash *hash, GhtNode **node);

/** Create a new code from a coordinate */
GhtErr ght_node_new_from_coordinate(const GhtCoordinate *coord, unsigned int resolution, GhtNode **node);

/** Fill a stringbuffer with a printout of the node tree */
GhtErr ght_node_to_string(GhtNode *node, stringbuffer_t *sb, int level);

/** How many leaf nodes in this tree? */
GhtErr ght_node_count_leaves(const GhtNode *node, int *count);

/** How many attributes on this node? */
GhtErr ght_node_count_attributes(const GhtNode *node, uint8_t *count);

/** Delete an attribute from the node (frees the attribute) */
GhtErr ght_node_delete_attribute(GhtNode *node, const GhtDimension *dim);

/** Add a new attribute to the node */
GhtErr ght_node_add_attribute(GhtNode *node, GhtAttribute *attribute);

/** Move attributes to the highest level in the tree at which they apply to all children */
GhtErr ght_node_compact_attribute(GhtNode *node, const GhtDimension *dim,
		GhtAttribute *attr);

/** Recursively build a GhtNodeList from a tree of GhtNode */
GhtErr ght_node_to_nodelist(const GhtNode *node, GhtNodeList *nodelist,
		GhtAttribute *attr, GhtHash *hash);

/** Recursively calculate the extent GhtArea of a tree of GhtNode */
GhtErr ght_node_get_extent(const GhtNode *node, const GhtHash *hash,
		GhtArea *area);

/** Recursively filter out sub-elements of the tree that don't pass the filter, returns a freshly allocated tree that corresponds to the filter */
GhtErr ght_node_filter_by_attribute(const GhtNode *node,
		const GhtFilter *filter, GhtNode **filtered_node);

/** Write a byte representation of a node tree */
GhtErr ght_node_write(const GhtNode *node, GhtWriter *writer);


// Patrick : get hash from node
GhtErr ght_node_get_hash(const GhtNode *node, GhtHash **hash);
//GhtErr ght_node_get_ghtFlag(const GhtNode *node);


/** Write a byte representation of a node tree */
GhtErr ght_node_read(GhtReader *reader, GhtNode **node);

/** Create an empty nodelist */
GhtErr ght_nodelist_new(int capacity, GhtNodeList **nodelist);

/** How many nodes in this GhtNodeList? */
GhtErr ght_nodelist_get_num_nodes(const GhtNodeList *nodelist, int *num_nodes);

/** Get a GhtNode by index number */
GhtErr ght_nodelist_get_node(const GhtNodeList *nodelist, int index,
		GhtNode **node);

/** Add a new node to a nodelist */
GhtErr ght_nodelist_add_node(GhtNodeList *nl, GhtNode *node);

/** Free a nodelist, and optionally all the nodes referenced by the list */
GhtErr ght_nodelist_free_deep(GhtNodeList *nl);

/** Free a nodelist, but not the nodes it holds */
GhtErr ght_nodelist_free_shallow(GhtNodeList *nl);

/** Allocate a new tree and initialize config parameters */
GhtErr ght_tree_new(const GhtSchema *schema, GhtTree **tree);

/** Build a tree from a linear nodelist */
GhtErr ght_tree_from_nodelist(const GhtSchema *schema, GhtNodeList *nlist,
		GhtConfig *config, GhtTree **tree);

/** Free a GhtTree from memory, including nodes and schema */
GhtErr ght_tree_free(GhtTree *tree);

/** Add a GhtNode to a GhtTree */
GhtErr ght_tree_insert_node(GhtTree *tree, GhtNode *node);

/** Write a GhtTree to memory or file */
GhtErr ght_tree_write(const GhtTree *tree, GhtWriter *writer);

/** Read the top level hash key off the GhtTree */
GhtErr ght_tree_get_hash(const GhtTree *tree, GhtHash **hash);

/** Read the schema from the GhtTree */
GhtErr ght_tree_get_schema(const GhtTree *tree, const GhtSchema **schema);

/** Read the point count from the GhtTree */
GhtErr ght_tree_get_numpoints(const GhtTree *tree, int *numpoints);

/** Compact all the attributes from 'Z' onwards */
GhtErr ght_tree_compact_attributes(GhtTree *tree);

/** Write a GhtTree to memory or file */
GhtErr ght_tree_write(const GhtTree *tree, GhtWriter *writer);

/** Write a GhtTree to memory of file */
GhtErr ght_tree_read(GhtReader *reader, GhtTree **tree);

/** Take in a tree and output a populated GhtNodeList, creates complete copy of data */
GhtErr ght_tree_to_nodelist(const GhtTree *tree, GhtNodeList *nodelist);

/** Calculate the spatial extent of a GhtTree */
GhtErr ght_tree_get_extent(const GhtTree *tree, GhtArea *area);

/** Allocate new tree with only nodes that meet the filter condition */
GhtErr ght_tree_filter_greater_than(const GhtTree *tree, const char *dimname,
		double value, GhtTree **tree_filtered);

/** Allocate new tree with only nodes that meet the filter condition */
GhtErr ght_tree_filter_less_than(const GhtTree *tree, const char *dimname,
		double value, GhtTree **tree_filtered);

/** Allocate new tree with only nodes that meet the filter condition */
GhtErr ght_tree_filter_between(const GhtTree *tree, const char *dimname,
		double value1, double value2, GhtTree **tree_filtered);

/** Allocate new tree with only nodes that meet the filter condition */
GhtErr ght_tree_filter_equal(const GhtTree *tree, const char *dimname,
		double value, GhtTree **tree_filtered);

/** Allocate a new attribute and fill in the value from a double */
GhtErr ght_attribute_new_from_double(const GhtDimension *dim, double val,
		GhtAttribute **attr);

/** Allocate a new attribute and copy in the value from a byte buffer */
GhtErr ght_attribute_new_from_bytes(const GhtDimension *dim, uint8_t *bytes,
		GhtAttribute **attr);

/** Read the next attribute from an attribute list */
GhtErr ght_attribute_get_next(const GhtAttribute *attr,
		GhtAttribute **nextattr);

/** Free an attribtue */
GhtErr ght_attribute_free(GhtAttribute *attr);

/** Return the scaled and offset version of the packed attribute value */
GhtErr ght_attribute_get_value(const GhtAttribute *attr, double *val);

/** Get the width of the packed attributes in bytes */
GhtErr ght_attribute_get_size(const GhtAttribute *attr, size_t *sz);

/** Get the dimension associated with a GhtAttribute */
GhtErr ght_attribute_get_dimension(const GhtAttribute *attr,
		const GhtDimension **dim);

/** Traverse GhtAttribute chain and copy out GhtAttribute that corresponds to the GhtDimension */
GhtErr ght_attribute_get_by_dimension(const GhtAttribute *attr,
		const GhtDimension *dim, GhtAttribute *found);

/** Set the packed attribute value */
GhtErr ght_attribute_set_value(GhtAttribute *attr, double val);

/** Write an appropriately formatted value into the stringbuffer_t */
GhtErr ght_attribute_to_string(const GhtAttribute *attr, stringbuffer_t *sb);

/** Copy an attribute and all linked siblings */
GhtErr ght_attribute_clone(const GhtAttribute *attr_in,
		GhtAttribute **attr_out);

/** Merge attr2 and attr1, removing those dimensions of attr2 that already exist in attr2 */
GhtErr ght_attribute_union(GhtAttribute *attr1, GhtAttribute *attr2,
		GhtAttribute **attr);

/** Write byte representation of attribute into writer */
GhtErr ght_attribute_write(const GhtAttribute *attr, GhtWriter *writer);

/** Read attribute from byte representation */
GhtErr ght_attribute_read(GhtReader *reader, GhtAttribute **attr);

/** Give a type string (eg "uint16_t"), return the GhtType number */
GhtErr ght_type_from_str(const char *str, GhtType *type);

/** Create an empty dimension */
GhtErr ght_dimension_new(GhtDimension **dim);

/** Create a populated dimension */
GhtErr ght_dimension_new_from_parameters(const char *name, const char *desc,
		GhtType type, double scale, double offset, GhtDimension **dim);

/** Set the name on a GhtDimension (names must be unique) */
GhtErr ght_dimension_set_name(GhtDimension *dim, const char *name);

/** Set the description on a GhtDimension */
GhtErr ght_dimension_set_description(GhtDimension *dim, const char *desc);

/** Set the offset on a GhtDimension */
GhtErr ght_dimension_set_offset(GhtDimension *dim, double offset);

/** Set the scale on a GhtDimension */
GhtErr ght_dimension_set_scale(GhtDimension *dim, double scale);

/** Set the scale on a GhtDimension */
GhtErr ght_dimension_set_type(GhtDimension *dim, GhtType type);

/** Where is the dimension in the schema? */
GhtErr ght_dimension_get_position(const GhtDimension *dim, uint8_t *position);

/** What's the name of this dimension? */
GhtErr ght_dimension_get_name(const GhtDimension *dim, const char **name);

/** What's the type of this dimension? */
GhtErr ght_dimension_get_type(const GhtDimension *dim, GhtType *type);

/** What's the index of this dimension? */
GhtErr ght_dimension_get_index(const GhtDimension *dim, int *index);

/** Are these dimensions functionally the same (name, scale, offset, type) ? */
GhtErr ght_dimension_same(const GhtDimension *dim1, const GhtDimension *dim2,
		int *same);

/** Allocate a blank GhtSchema */
GhtErr ght_schema_new(GhtSchema **schema);

/** Create a copy of a GhtSchema */
GhtErr ght_schema_clone(const GhtSchema *schema, GhtSchema **newschema);

/** Free an existing schema */
GhtErr ght_schema_free(GhtSchema *schema);

/** Are these schemas functionally the same (name, scale, offset, type) in all dimensions? */
GhtErr ght_schema_same(const GhtSchema *s1, const GhtSchema *s2, int *same);

/** Append a GhtDimension to the GhtSchema */
GhtErr ght_schema_add_dimension(GhtSchema *schema, GhtDimension *dim);

/** Find the GhtDimension corresponding to a name */
GhtErr ght_schema_get_dimension_by_name(const GhtSchema *schema,
		const char *name, GhtDimension **dim);

/** Find the GhtDimension corresponding to an index */
GhtErr ght_schema_get_dimension_by_index(const GhtSchema *schema, int i,
		GhtDimension **dim);

/** How many dimensions in this GhtSchema? */
GhtErr ght_schema_get_num_dimensions(const GhtSchema *schema,
		unsigned int *num_dims);

/** Create a schema from an XML document */
GhtErr ght_schema_from_xml_str(const char *xmlstr, GhtSchema **schema);

/** Turn a schema into an XML document */
GhtErr ght_schema_to_xml_str(const GhtSchema *schema, char **xml_str,
		size_t *xml_str_size);

/** Write out an XML representation of a GhtSchema */
GhtErr ght_schema_to_xml_file(const GhtSchema *schema, const char *filename);

/** Write out an XML representation of a GhtSchema */
GhtErr ght_schema_from_xml_file(const char *filename, GhtSchema **schema);

/** Close filehandle if necessary and free all memory along with writer */
GhtErr ght_writer_free(GhtWriter *writer);

/** Create a new file-based writer */
GhtErr ght_writer_new_file(const char *filename, GhtWriter **writer);

/** Create a new memory-backed writer */
GhtErr ght_writer_new_mem(GhtWriter **writer);

/** Read current size of writen bytes */
GhtErr ght_writer_get_size(GhtWriter *writer, size_t *size);

/** Copy bytes from memory writer into external buffer */
GhtErr ght_writer_get_bytes(GhtWriter *writer, uint8_t *bytes);

/** Write bytes out to the target */
GhtErr ght_write(GhtWriter *writer, const void *bytes, size_t bytesize);

/** Create a new file-based reader */
GhtErr ght_reader_new_file(const char *filename, const GhtSchema *schema,
		GhtReader **reader);

/** Create a new memory-based reader */
GhtErr ght_reader_new_mem(const uint8_t *bytes_start, size_t bytes_size,
		const GhtSchema *schema, GhtReader **reader);

/** Close filehandle if necessary and free all memory along with reader */
GhtErr ght_reader_free(GhtReader *reader);

/** Read bytes in from a reader */
GhtErr ght_read(GhtReader *reader, void *bytes, size_t read_size);

/** Set up a tree configuration with defaults */
GhtErr ght_config_init(GhtConfig *config);

/** Convert a hex string into a byte buffer */
GhtErr bytes_from_hexbytes(const char *hex, size_t hexsize, uint8_t **bytes);

/** Convert a byte buffer into a hex string */
GhtErr hexbytes_from_bytes(const uint8_t *bytes, size_t bytesize, char **hex);

/** Test that a file exists */
int fexists(const char *filename);

#endif /* _GHT_INTERNAL_H */
