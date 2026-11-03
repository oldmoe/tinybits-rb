#include <ruby.h>
#include <ruby/encoding.h>

#include "tinybits.h"

// Ruby module and classes
/*
 * Document-module: TinyBits
 *
 * A Ruby extension for fast binary serialization and deserialization of Ruby objects.
 */
VALUE rb_mTinyBits;
/*
 * Document-class: TinyBits::Packer
 *
 * The Packer class handles serialization of Ruby objects to the TinyBits binary format.
 */
VALUE rb_cPacker;
/*
 * Document-class: TinyBits::Unpacker
 *
 * The Unpacker class handles deserialization of TinyBits binary format to Ruby objects.
 */
VALUE rb_cUnpacker;

// Forward declarations
static VALUE rb_packer_alloc(VALUE klass);
static VALUE rb_packer_init(int argc, VALUE* argv, VALUE self);
static VALUE rb_unpacker_alloc(VALUE klass);
static VALUE rb_unpacker_init(int argc, VALUE* argv, VALUE self);
static VALUE rb_pack(VALUE self, VALUE obj);
static VALUE rb_unpack(VALUE self, VALUE buffer);

// Structure to pass data and error status through rb_hash_foreach
typedef struct {
    tiny_bits_packer* packer;
    int error_occurred; // 0 for no error, 1 if callback encountered an error
} HashIterContext;

static int binary_encindex; // rb_ascii8bit_encindex(), set in Init

// Forward declaration for the recursive function
static int pack_ruby_object_recursive(tiny_bits_packer* packer, VALUE obj, VALUE context);

// Callback function for rb_hash_foreach
// It receives key, value, and the packer pointer (passed as user_data)
inline static int hash_foreach_callback(VALUE key, VALUE value, VALUE user_data) {
    // Cast user_data back to our context struct pointer
    HashIterContext* context = (HashIterContext*)user_data;

    // Pack the key using the packer from the context
    if (!pack_ruby_object_recursive(context->packer, key, user_data)) {
        context->error_occurred = 1; // Signal error occurred
        return ST_STOP;              // Stop iteration
    }
    // Pack the value using the packer from the context
    if (!pack_ruby_object_recursive(context->packer, value, user_data)) {
        context->error_occurred = 1; // Signal error occurred
        return ST_STOP;              // Stop iteration
    }

    // If both succeeded, continue
    return ST_CONTINUE;
}

// Packer structure
typedef struct {
    tiny_bits_packer* packer;
} PackerData;

static void packer_free(void* data) {
    PackerData* packer_data = (PackerData*)data;
    if (packer_data->packer) {
        tiny_bits_packer_destroy(packer_data->packer);
    }
    xfree(packer_data);
}

static size_t packer_memsize(const void* data) {
    return sizeof(PackerData);
}

static const rb_data_type_t packer_data_type = {
    "TinyBits::Packer",
    {0, packer_free, packer_memsize,},
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE rb_packer_alloc(VALUE klass) {
    PackerData* packer_data = ALLOC(PackerData);
    packer_data->packer = NULL;
    return TypedData_Wrap_Struct(klass, &packer_data_type, packer_data);
}

// Calls set_dict(target, strs, lens, count) with the #to_s of each dictionary member
#define WITH_DICT_STRINGS(dict, set_dict, target) do {                                  \
    Check_Type(dict, T_ARRAY);                                                         \
    long dict_n = RARRAY_LEN(dict);                                                    \
    VALUE dict_strs = rb_ary_new_capa(dict_n);                                         \
    for (long i = 0; i < dict_n; i++) {                                                \
        rb_ary_push(dict_strs, rb_obj_as_string(rb_ary_entry(dict, i)));               \
    }                                                                                  \
    VALUE dict_buf;                                                                    \
    const char **dict_ptrs = ALLOCV(dict_buf, dict_n * (sizeof(char *) + sizeof(size_t)) + 1); \
    size_t *dict_lens = (size_t *)(dict_ptrs + dict_n);                                \
    for (long i = 0; i < dict_n; i++) {                                                \
        VALUE s = RARRAY_AREF(dict_strs, i);                                           \
        dict_ptrs[i] = RSTRING_PTR(s);                                                 \
        dict_lens[i] = RSTRING_LEN(s);                                                 \
    }                                                                                  \
    int dict_ok = set_dict(target, dict_ptrs, dict_lens, dict_n);                      \
    ALLOCV_END(dict_buf);                                                              \
    RB_GC_GUARD(dict_strs);                                                            \
    if (!dict_ok) rb_raise(rb_eRuntimeError, "Failed to set dictionary");              \
} while (0)

/*
 * Document-method: initialize
 *
 * Initializes a new Packer object
 *
 * @param dict [Array] strings (or objects responding to #to_s) to deduplicate against without
 *   storing them in the output (order is significant), the unpacker must use the same dict
 * @return [Packer] The initialized packer object.
 */
static VALUE rb_packer_init(int argc, VALUE* argv, VALUE self) {
    PackerData* packer_data;
    TypedData_Get_Struct(self, PackerData, &packer_data_type, packer_data);

    VALUE opts = Qnil;
    rb_scan_args(argc, argv, ":", &opts);
    VALUE dict = NIL_P(opts) ? Qnil : rb_hash_lookup(opts, ID2SYM(rb_intern("dict")));

    packer_data->packer = tiny_bits_packer_create(256, (TB_FEATURE_STRING_DEDUPE | TB_FEATURE_COMPRESS_FLOATS) | 0); // Initial capacity and features
    if (!packer_data->packer) {
        rb_raise(rb_eRuntimeError, "Failed to initialize packer");
    }
    if (!NIL_P(dict)) {
        WITH_DICT_STRINGS(dict, tiny_bits_packer_set_dict, packer_data->packer);
    }
    return self;
}

// Optimized recursive packing function
static inline int pack_ruby_object_recursive(tiny_bits_packer* packer, VALUE obj, VALUE context) {
    switch (TYPE(obj)) {
        case T_STRING: {
            if (ENCODING_GET_INLINED(obj) == binary_encindex)
                return pack_blob(packer, RSTRING_PTR(obj), RSTRING_LEN(obj));
            return pack_str(packer, RSTRING_PTR(obj), RSTRING_LEN(obj));
        }
        case T_HASH: {
            long len = RHASH_SIZE(obj);
            int written = pack_map(packer, len); 
            if (written <= 0) return 0; // Error check based on tiny_bits API
            rb_hash_foreach(obj, hash_foreach_callback, context);    
            return (((HashIterContext *)context)->error_occurred == 0);
        }
        case T_ARRAY: {
            long len = RARRAY_LEN(obj);
            int written = pack_arr(packer, len); 
            if (written <= 0) return 0; // Error check based on tiny_bits API
            for (long i = 0; i < len; i++) {
                if(!pack_ruby_object_recursive(packer, RARRAY_AREF(obj, i), context)) return 0; // Propagate error
            }
            return 1; // Success
        }
        case T_FIXNUM: {
            return pack_int(packer, (int64_t)FIX2LONG(obj));
        }
        case T_BIGNUM: {
            // within int64: a Bignum only past the Fixnum range (2**62)
            if (rb_absint_numwords(obj, 63, NULL) > 1) {
                rb_warn("Integer out of the int64 range encountered during packing");
                return 0;
            }
            return pack_int(packer, (int64_t)NUM2LL(obj));
        }
        case T_FLOAT: {
            return pack_double(packer, RFLOAT_VALUE(obj));
        }
        case T_NIL:
            return pack_null(packer);
        case T_TRUE:
            return pack_true(packer);
        case T_FALSE:
            return pack_false(packer);
        case T_SYMBOL: {
            VALUE str = rb_sym2str(obj);
            return pack_str(packer, RSTRING_PTR(str), RSTRING_LEN(str));
        }
        default:
            if(rb_obj_is_kind_of(obj, rb_cTime)){
                double unixtime = NUM2DBL(rb_funcall(obj, rb_intern("to_f"), 0));
                return pack_datetime(packer, unixtime, FIX2INT(rb_time_utc_offset(obj)));    
            }
            if(rb_respond_to(obj, rb_intern("to_tinybits"))){
                VALUE custom_obj = rb_funcall(obj, rb_intern("to_tinybits"), 0);
                return pack_ruby_object_recursive(packer, custom_obj, context);
            }
            //printf("Unsupported type encountered during packing: %s", rb_obj_classname(obj));
            rb_warn("Unsupported type encountered during packing: %s", rb_obj_classname(obj));
            return 0;
    }
}

/*
 * Document-method: pack
 * 
 * Packs a Ruby object into a binary string.
 * Supports Ruby types: String, Array, Hash, Integer, Float, nil, true, false, Symbol, and Time.
 * Objects can implement a `to_tinybits` method to provide custom serialization.
 *
 * @param obj [Object] The Ruby object to pack.
 * @return [String] The packed binary string (frozen).
 * @raise [RuntimeError] If packing fails due to unsupported types or other errors.
 */
static VALUE rb_pack(VALUE self, VALUE obj) {
    PackerData* packer_data;
    TypedData_Get_Struct(self, PackerData, &packer_data_type, packer_data);


    if (!packer_data->packer) {
        rb_raise(rb_eRuntimeError, "Packer not initialized");
    }

    // Reset before packing (assuming this is efficient)
    tiny_bits_packer_reset(packer_data->packer);

    HashIterContext context;
    context.packer = packer_data->packer;        // Pass the current packer
    context.error_occurred = 0;   // Initialize error flag

    // Call the optimized recursive function
    if (!pack_ruby_object_recursive(packer_data->packer, obj, (VALUE)&context)) {
        // Error occurred during packing (might be unsupported type or tiny_bits error)
        rb_raise(rb_eRuntimeError, "Failed to pack object (unsupported type or packing error)");
    }

    VALUE result = rb_str_new((const char*)tiny_bits_packer_data(packer_data->packer), tiny_bits_packer_size(packer_data->packer));
    rb_obj_freeze(result);
    return result;
}

/*
 * Document-method: push
 *
 * Appends a packed object to the current buffer.
 * Inserts a separator when appending to non-empty buffer.
 *
 * @param obj [Object] The Ruby object to append.
 * @return [Integer] The number of bytes added to the buffer.
 * @raise [RuntimeError] If packing fails.
 */
static VALUE rb_push(VALUE self, VALUE obj) {
    PackerData* packer_data;
    TypedData_Get_Struct(self, PackerData, &packer_data_type, packer_data);

    if (!packer_data->packer) {
        rb_raise(rb_eRuntimeError, "Packer not initialized");
    }

    size_t initial_pos = packer_data->packer->current_pos;

    HashIterContext context;
    context.packer = packer_data->packer;        // Pass the current packer
    context.error_occurred = 0;   // Initialize error flag

    /*
    if(initial_pos > 0){
       if(!pack_separator(packer_data->packer)){
            rb_raise(rb_eRuntimeError, "Failed to pack object (multi-object packing error)");
       } 
    }
    */
    // Call the optimized recursive function
    if (!pack_ruby_object_recursive(packer_data->packer, obj, (VALUE)&context)) {
        // Error occurred during packing (might be unsupported type or tiny_bits error)
        rb_raise(rb_eRuntimeError, "Failed to pack object (unsupported type or packing error)");
    }

    return INT2FIX(packer_data->packer->current_pos - initial_pos);
}

/*
 * Document-method: to_s
 *
 * Returns the current packed buffer as a string.
 *
 * @return [String] The current packed buffer contents (frozen).
 */
static VALUE rb_to_s(VALUE self){
    PackerData* packer_data;
    TypedData_Get_Struct(self, PackerData, &packer_data_type, packer_data);

    if (!packer_data->packer) {
        rb_raise(rb_eRuntimeError, "Packer not initialized");
    }

    if(tiny_bits_packer_size(packer_data->packer) == 0){
        return rb_str_new("", 0);
    }
    VALUE result = rb_str_new((const char*)tiny_bits_packer_data(packer_data->packer), tiny_bits_packer_size(packer_data->packer));
    rb_obj_freeze(result);
    return result;
}

/*
 * Document-method: reset
 *
 * Resets the packer's buffer to empty.
 *
 * @return [Packer] self
 */
static VALUE rb_reset(VALUE self){
    PackerData* packer_data;
    TypedData_Get_Struct(self, PackerData, &packer_data_type, packer_data);


    if (!packer_data->packer) {
        rb_raise(rb_eRuntimeError, "Packer not initialized");
    }

    // Reset before packing (assuming this is efficient)
    tiny_bits_packer_reset(packer_data->packer);
    return self;
}

#define KEY_CACHE_CAPA 63
#define KEY_CACHE_MAX_MISSES 1024

// Unpacker structure
typedef struct {
    tiny_bits_unpacker* unpacker;
    int symbolize_keys; // map keys that are strings come back as symbols
    VALUE strings;      // Ruby strings by dedupe id, dictionary strings first
    VALUE buffer;       // the buffer set by #buffer=, kept alive while popping
    int keys_count;
    int keys_misses;    // misses while the cache is full
    VALUE keys[KEY_CACHE_CAPA]; // interned map keys sorted by (length, bytes), kept across unpacks
} UnpackerData;

static void unpacker_mark(void* data) {
    UnpackerData* unpacker_data = (UnpackerData*)data;
    rb_gc_mark_movable(unpacker_data->strings);
    rb_gc_mark(unpacker_data->buffer); // pinned, the C unpacker points into it (embedded strings move)
    for (int i = 0; i < unpacker_data->keys_count; i++) rb_gc_mark_movable(unpacker_data->keys[i]);
}

static void unpacker_compact(void* data) {
    UnpackerData* unpacker_data = (UnpackerData*)data;
    unpacker_data->strings = rb_gc_location(unpacker_data->strings);
    for (int i = 0; i < unpacker_data->keys_count; i++) unpacker_data->keys[i] = rb_gc_location(unpacker_data->keys[i]);
}

// Map keys repeat across unpacks, a small sorted cache saves the global fstring table lookup
// (same approach as the json gem's name cache, but persistent; when full, misses are not
// inserted, and after KEY_CACHE_MAX_MISSES of them it restarts empty to follow new keys)
static inline VALUE key_cache_fetch(UnpackerData* unpacker_data, const char* str, long length) {
    if (length < 2 || length > 128) // same range as string dedupe
        return rb_enc_interned_str(str, length, rb_utf8_encoding());
    VALUE* keys = unpacker_data->keys;
    int low = 0, high = unpacker_data->keys_count - 1;
    while (low <= high) {
        int mid = (low + high) >> 1;
        long cmp = length - RSTRING_LEN(keys[mid]);
        if (cmp == 0) cmp = memcmp(str, RSTRING_PTR(keys[mid]), length);
        if (cmp == 0) return keys[mid];
        if (cmp > 0) low = mid + 1; else high = mid - 1;
    }
    VALUE key = rb_enc_interned_str(str, length, rb_utf8_encoding());
    if (unpacker_data->keys_count == KEY_CACHE_CAPA) {
        if (++unpacker_data->keys_misses < KEY_CACHE_MAX_MISSES) return key;
        unpacker_data->keys_misses = 0;
        unpacker_data->keys_count = 0;
        low = 0;
    }
    MEMMOVE(&keys[low + 1], &keys[low], VALUE, unpacker_data->keys_count - low);
    keys[low] = key;
    unpacker_data->keys_count++;
    return key;
}

static void unpacker_free(void* data) {
    UnpackerData* unpacker_data = (UnpackerData*)data;
    if (unpacker_data->unpacker) {
        tiny_bits_unpacker_destroy(unpacker_data->unpacker);
    }
    xfree(unpacker_data);
}

static size_t unpacker_memsize(const void* data) {
    return sizeof(UnpackerData);
}

static const rb_data_type_t unpacker_data_type = {
    "TinyBits::Unpacker",
    {unpacker_mark, unpacker_free, unpacker_memsize, unpacker_compact,},
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE rb_unpacker_alloc(VALUE klass) {
    UnpackerData* unpacker_data = ALLOC(UnpackerData);
    unpacker_data->unpacker = NULL;
    unpacker_data->symbolize_keys = 0;
    unpacker_data->strings = Qnil;
    unpacker_data->buffer = Qnil;
    unpacker_data->keys_count = 0;
    unpacker_data->keys_misses = 0;
    return TypedData_Wrap_Struct(klass, &unpacker_data_type, unpacker_data);
}

/*
 * Document-method: initialize
 *
 * Initializes a new Unpacker.
 *
 * @param symbolize_keys [Boolean] unpack hash keys that are strings as symbols (default false)
 * @param dict [Array] the dictionary the packer used (order is significant)
 * @return [Unpacker] The initialized unpacker object.
 */
static VALUE rb_unpacker_init(int argc, VALUE* argv, VALUE self) {
    UnpackerData* unpacker_data;
    TypedData_Get_Struct(self, UnpackerData, &unpacker_data_type, unpacker_data);

    VALUE opts = Qnil;
    VALUE dict = Qnil;
    rb_scan_args(argc, argv, ":", &opts);
    if (!NIL_P(opts)) {
        unpacker_data->symbolize_keys = RTEST(rb_hash_lookup(opts, ID2SYM(rb_intern("symbolize_keys"))));
        dict = rb_hash_lookup(opts, ID2SYM(rb_intern("dict")));
    }

    unpacker_data->unpacker = tiny_bits_unpacker_create();
    if (!unpacker_data->unpacker) {
        rb_raise(rb_eRuntimeError, "Failed to initialize unpacker");
    }
    VALUE strings = rb_ary_new_capa(TB_HASH_CACHE_SIZE);
    if (!NIL_P(dict)) {
        tiny_bits_unpacker* unpacker = unpacker_data->unpacker;
        WITH_DICT_STRINGS(dict, tiny_bits_unpacker_set_dict, unpacker);
        // dictionary strings stay in the first dict_count slots, only later ones are cleared
        for (size_t i = 0; i < unpacker->dict_count; i++) {
            rb_ary_push(strings, rb_enc_interned_str(unpacker->strings[i].str, unpacker->strings[i].length, rb_utf8_encoding()));
        }
    }
    RB_OBJ_WRITE(self, &unpacker_data->strings, strings);
    return self;
}

static inline void clear_strings(VALUE strings, UnpackerData* unpacker_data) {
    rb_ary_resize(strings, unpacker_data->unpacker->dict_count);
}

static inline VALUE rb_unpack_str(VALUE strings, UnpackerData* unpacker_data, tiny_bits_value value, size_t interned){
    int32_t id  = value.str_blob_val.id;
    if(id > 0)
        return rb_ary_entry(strings, id-1);
    else if(id <= 0){
        VALUE str;
        if(interned > 0){
            str = key_cache_fetch(unpacker_data, value.str_blob_val.data, value.str_blob_val.length);
        } else {
            str = rb_utf8_str_new(value.str_blob_val.data, value.str_blob_val.length);
            rb_obj_freeze(str);
        }
        if(id < 0){
            rb_ary_push(strings, str);
        }
        return str;
    }            
    return Qundef;
}

static VALUE unpack_ruby_object(VALUE strings, UnpackerData* unpacker_data, size_t interned) {
    tiny_bits_unpacker* unpacker = unpacker_data->unpacker;
    tiny_bits_value value;
    enum tiny_bits_type type = unpack_value(unpacker, &value);

    if (type == TINY_BITS_ERROR) {
        return Qundef; // Use Qundef as a sentinel for error (not nil)
    }

    switch (type) {
        case TINY_BITS_STR: {        
            return rb_unpack_str(strings, unpacker_data, value, interned);    
        }
        case TINY_BITS_DOUBLE:
            return DBL2NUM(value.double_val);
        case TINY_BITS_INT:
            return LONG2NUM(value.int_val);
        case TINY_BITS_NAN:
            return DBL2NUM(NAN);
        case TINY_BITS_INF:
            return DBL2NUM(INFINITY);
        case TINY_BITS_N_INF:
            return DBL2NUM(-INFINITY);
        case TINY_BITS_NULL:
            return Qnil;
        case TINY_BITS_TRUE:
            return Qtrue;
        case TINY_BITS_FALSE:
            return Qfalse;
        case TINY_BITS_ARRAY: {
            if (value.length <= 16) { // small arrays are created in one call
                VALUE items[16];
                for (size_t i = 0; i < value.length; i++) {
                    items[i] = unpack_ruby_object(strings, unpacker_data, 0);
                    if (items[i] == Qundef) return Qundef; // Error
                }
                return rb_ary_new_from_values(value.length, items);
            }
            VALUE arr = rb_ary_new_capa(value.length);
            for (size_t i = 0; i < value.length; i++) {
                VALUE element = unpack_ruby_object(strings, unpacker_data, 0);
                if (element == Qundef) return Qundef; // Error
                rb_ary_push(arr, element);
            }
            return arr;
        }
        case TINY_BITS_MAP: {
            VALUE hash = rb_hash_new_capa(value.length);
            VALUE pairs[2 * 16]; // small maps are inserted in one bulk call
            int bulk = value.length <= 16;
            for (size_t i = 0; i < value.length; i++) {
                VALUE key = unpack_ruby_object(strings, unpacker_data, 1);
                if (key == Qundef) return Qundef; // Error
                if (unpacker_data->symbolize_keys && RB_TYPE_P(key, T_STRING)) key = rb_str_intern(key);
                VALUE val = unpack_ruby_object(strings, unpacker_data, 0);
                if (val == Qundef) return Qundef; // Error
                if (bulk) {
                    pairs[2 * i] = key;
                    pairs[2 * i + 1] = val;
                } else {
                    rb_hash_aset(hash, key, val);
                }
            }
            if (bulk) rb_hash_bulk_insert(2 * value.length, pairs, hash);
            return hash;
        }
        case TINY_BITS_BLOB:
            // For smplicity, treat blobs as strings (similar to strings)
            VALUE blob = rb_str_new(value.str_blob_val.data, value.str_blob_val.length);
            rb_obj_freeze(blob);
            return blob;
        case TINY_BITS_DATETIME:
            VALUE time = rb_time_num_new(DBL2NUM(value.datetime_val.unixtime), INT2FIX(((int16_t)value.datetime_val.offset)));
            return time;
        default:
            return Qundef; // Error
    }
}

/*
 * Document-method: unpack
 *
 * Unpacks a binary string into a Ruby object.
 *
 * @param buffer [String] The binary string to unpack.
 * @return [Object] The unpacked Ruby object.
 * @raise [RuntimeError] If unpacking fails.
 */
static VALUE rb_unpack(VALUE self, VALUE buffer) {
    UnpackerData* unpacker_data;
    TypedData_Get_Struct(self, UnpackerData, &unpacker_data_type, unpacker_data);

    if (!unpacker_data->unpacker) {
        rb_raise(rb_eRuntimeError, "Unpacker not initialized");
    }

    StringValue(buffer); // Ensure it's a string

    tiny_bits_unpacker_set_buffer(unpacker_data->unpacker, (const unsigned char*)RSTRING_PTR(buffer), RSTRING_LEN(buffer));

    VALUE array = unpacker_data->strings;
    clear_strings(array, unpacker_data); // a failed unpack or a pop session may have left entries
    VALUE result = unpack_ruby_object(array, unpacker_data, 0);
    RB_GC_GUARD(buffer);
    // unpack is for one object only, a later #pop finds an empty buffer (and never reads this one)
    unpacker_data->unpacker->current_pos = unpacker_data->unpacker->size;

    if (result == Qundef) {
        rb_raise(rb_eRuntimeError, "Failed to unpack data");
    }

    clear_strings(array, unpacker_data);

    return result;
}

/*
 * Document-method: buffer=
 *
 * Sets the buffer for incremental unpacking.
 *
 * @param buffer [String] The binary buffer to unpack incrementally.
 * @return [Unpacker] self
 * @note Stores a reference to the buffer internally.
 */
static VALUE rb_set_buffer(VALUE self, VALUE buffer){
    UnpackerData* unpacker_data;
    TypedData_Get_Struct(self, UnpackerData, &unpacker_data_type, unpacker_data);

    if (!unpacker_data->unpacker) {
        rb_raise(rb_eRuntimeError, "Unpacker not initialized");
    }

    StringValue(buffer); // Ensure it's a string
    buffer = rb_str_new_frozen(buffer); // the caller mutating their string must not move the bytes we point into

    tiny_bits_unpacker_set_buffer(unpacker_data->unpacker, (const unsigned char*)RSTRING_PTR(buffer), RSTRING_LEN(buffer));

    // keep a reference to the buffer while popping
    RB_OBJ_WRITE(self, &unpacker_data->buffer, buffer);
    clear_strings(unpacker_data->strings, unpacker_data);
    return self;
}

/*
 * Document-method: pop
 *
 * Extracts the next object from the buffer.
 *
 * @return [Object, nil] The next unpacked object or nil if buffer is exhausted.
 * @raise [RuntimeError] If no buffer is set or if unpacking fails.
 * @note Requires #buffer= to be called first.
 */
static VALUE rb_pop(VALUE self) {
    UnpackerData* unpacker_data;
    TypedData_Get_Struct(self, UnpackerData, &unpacker_data_type, unpacker_data);

    VALUE array = unpacker_data->strings;
    if(unpacker_data->buffer == Qnil){
        rb_raise(rb_eRuntimeError, "No buffer is set");
    }

    if (!unpacker_data->unpacker) {
        rb_raise(rb_eRuntimeError, "Unpacker not initialized");
    }

    tiny_bits_unpacker* unpacker = unpacker_data->unpacker;

    if(unpacker->current_pos >= unpacker->size){
        clear_strings(array, unpacker_data);
        return Qnil;
    }

    VALUE result = unpack_ruby_object(array, unpacker_data, 0);
    if (result == Qundef) {
        rb_raise(rb_eRuntimeError, "Failed to unpack data");
    }

    if(unpacker->current_pos >= unpacker->size){
        clear_strings(array, unpacker_data);
    }

    return result;
}

/*
 * Document-method: finished?
 *
 * Checks if the buffer has been fully consumed.
 *
 * @return [Boolean] true if there are no more objects to unpack.
 * @raise [RuntimeError] If no buffer is set.
 * @note Requires #buffer= to be called first.
 */
static VALUE rb_finished(VALUE self){
    UnpackerData* unpacker_data;
    TypedData_Get_Struct(self, UnpackerData, &unpacker_data_type, unpacker_data);
    if(unpacker_data->buffer == Qnil){
        rb_raise(rb_eRuntimeError, "No buffer is set");
    }

    if (!unpacker_data->unpacker) {
        rb_raise(rb_eRuntimeError, "Unpacker not initialized");
    }

    tiny_bits_unpacker* unpacker = unpacker_data->unpacker;

    if(unpacker->current_pos >= unpacker->size){
        return Qtrue;
    }

    return Qfalse;
}

void Init_tinybits_ext(void) {
    binary_encindex = rb_ascii8bit_encindex();
    rb_mTinyBits = rb_define_module("TinyBits");
    rb_cPacker = rb_define_class_under(rb_mTinyBits, "Packer", rb_cObject);
    rb_cUnpacker = rb_define_class_under(rb_mTinyBits, "Unpacker", rb_cObject);

    rb_define_alloc_func(rb_cPacker, rb_packer_alloc);
    rb_define_method(rb_cPacker, "initialize", rb_packer_init, -1);
    rb_define_method(rb_cPacker, "pack", rb_pack, 1);
    rb_define_alias(rb_cPacker, "encode", "pack");
    rb_define_alias(rb_cPacker, "dump", "pack");
    rb_define_method(rb_cPacker, "push", rb_push, 1);
    rb_define_alias(rb_cPacker, "<<", "push");
    rb_define_method(rb_cPacker, "to_s", rb_to_s, 0);
    rb_define_method(rb_cPacker, "reset", rb_reset, 0);

    rb_define_alloc_func(rb_cUnpacker, rb_unpacker_alloc);
    rb_define_method(rb_cUnpacker, "initialize", rb_unpacker_init, -1);
    rb_define_method(rb_cUnpacker, "unpack", rb_unpack, 1);
    rb_define_alias(rb_cUnpacker, "load", "unpack");
    rb_define_alias(rb_cUnpacker, "decode", "unpack");
    rb_define_method(rb_cUnpacker, "buffer=", rb_set_buffer, 1);
    rb_define_method(rb_cUnpacker, "pop", rb_pop, 0);
    rb_define_method(rb_cUnpacker, "finished?", rb_finished, 0);
}
