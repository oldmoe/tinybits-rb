#include <ruby.h>
#include <ruby/encoding.h>

#include "tinybits.h"

// Ruby module and classes
VALUE rb_mTinyBits;
VALUE rb_cPacker;
VALUE rb_cUnpacker;

// Forward declarations
static VALUE rb_packer_alloc(VALUE klass);
static VALUE rb_packer_init(VALUE self);
static VALUE rb_unpacker_alloc(VALUE klass);
static VALUE rb_unpacker_init(VALUE self);
static VALUE rb_pack(VALUE self, VALUE obj);
static VALUE rb_unpack(VALUE self, VALUE buffer);

// Structure to pass data and error status through rb_hash_foreach
typedef struct {
    tiny_bits_packer* packer;
    int error_occurred; // 0 for no error, 1 if callback encountered an error
} HashIterContext;

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
    free(packer_data);
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

static VALUE rb_packer_init(VALUE self) {
    PackerData* packer_data;
    TypedData_Get_Struct(self, PackerData, &packer_data_type, packer_data);

    packer_data->packer = tiny_bits_packer_create(256, (TB_FEATURE_STRING_DEDUPE | TB_FEATURE_COMPRESS_FLOATS) | 0); // Initial capacity and features
    if (!packer_data->packer) {
        rb_raise(rb_eRuntimeError, "Failed to initialize packer");
    }
    return self;
}

// Optimized recursive packing function
static inline int pack_ruby_object_recursive(tiny_bits_packer* packer, VALUE obj, VALUE context) {
    switch (TYPE(obj)) {
        case T_STRING: {
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
                if(!pack_ruby_object_recursive(packer, rb_ary_entry(obj, i), context)) return 0; // Propagate error
            }
            return 1; // Success
        }
        case T_FIXNUM: {
            int64_t val = NUM2LONG(obj); // Assumes fits in int64_t
            return pack_int(packer, val);
        }
        case T_FLOAT: {
            double val = NUM2DBL(obj);
            return pack_double(packer, val);
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
            //printf("Unsupported type encountered during packing: %s", rb_obj_classname(obj));
            rb_warn("Unsupported type encountered during packing: %s", rb_obj_classname(obj));
            return 0;
    }
}

// keeps the public API the same.
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

    VALUE result = rb_str_new((const char*)packer_data->packer->buffer, packer_data->packer->current_pos);
    rb_obj_freeze(result);
    return result;
}

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

    
    if(initial_pos > 0){
       if(!pack_separator(packer_data->packer)){
            rb_raise(rb_eRuntimeError, "Failed to pack object (multi-object packing error)");
       } 
    }
    
    // Call the optimized recursive function
    if (!pack_ruby_object_recursive(packer_data->packer, obj, (VALUE)&context)) {
        // Error occurred during packing (might be unsupported type or tiny_bits error)
        rb_raise(rb_eRuntimeError, "Failed to pack object (unsupported type or packing error)");
    }

    return INT2FIX(packer_data->packer->current_pos - initial_pos);
}

static VALUE rb_to_s(VALUE self){
    PackerData* packer_data;
    TypedData_Get_Struct(self, PackerData, &packer_data_type, packer_data);

    if (!packer_data->packer) {
        rb_raise(rb_eRuntimeError, "Packer not initialized");
    }

    if(packer_data->packer->current_pos == 0){
        return rb_str_new("", 0);
    }
    VALUE result = rb_str_new((const char*)packer_data->packer->buffer, packer_data->packer->current_pos);
    rb_obj_freeze(result);
    return result;
}

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

// Unpacker structure
typedef struct {
    tiny_bits_unpacker* unpacker;
    size_t strings_index;
    VALUE ruby_strings[TB_HASH_CACHE_SIZE];
} UnpackerData;

static void unpacker_free(void* data) {
    UnpackerData* unpacker_data = (UnpackerData*)data;
    if (unpacker_data->unpacker) {
        tiny_bits_unpacker_destroy(unpacker_data->unpacker);
    }
    for (size_t i = 0; i < TB_HASH_CACHE_SIZE; i++) {
        unpacker_data->ruby_strings[i] = Qnil;
    }
    free(unpacker_data);
}

static size_t unpacker_memsize(const void* data) {
    return sizeof(UnpackerData);
}

static const rb_data_type_t unpacker_data_type = {
    "TinyBits::Unpacker",
    {0, unpacker_free, unpacker_memsize,},
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE rb_unpacker_alloc(VALUE klass) {
    UnpackerData* unpacker_data = ALLOC(UnpackerData);
    unpacker_data->unpacker = NULL;
    return TypedData_Wrap_Struct(klass, &unpacker_data_type, unpacker_data);
}

static VALUE rb_unpacker_init(VALUE self) {
    UnpackerData* unpacker_data;
    TypedData_Get_Struct(self, UnpackerData, &unpacker_data_type, unpacker_data);

    unpacker_data->unpacker = tiny_bits_unpacker_create();
    if (!unpacker_data->unpacker) {
        rb_raise(rb_eRuntimeError, "Failed to initialize unpacker");
    }
    unpacker_data->strings_index = 0;
    return self;
}

static inline VALUE rb_unpack_str(UnpackerData* unpacker_data, tiny_bits_value value, size_t interned){
    int32_t id  = value.str_blob_val.id;
    if(id > 0)
        return unpacker_data->ruby_strings[id-1];
    else if(id <= 0){
        VALUE str;
        if(interned > 0){
            str = rb_enc_interned_str(value.str_blob_val.data, value.str_blob_val.length, rb_utf8_encoding());
        } else {
            str = rb_utf8_str_new(value.str_blob_val.data, value.str_blob_val.length);
            rb_obj_freeze(str);
        }
        if(id < 0){
            unpacker_data->ruby_strings[abs(id)-1] = str;
        }
        return str;
    }            
    return Qundef;
}

static VALUE unpack_ruby_object(UnpackerData* unpacker_data, size_t interned) {
    tiny_bits_unpacker* unpacker = unpacker_data->unpacker;
    tiny_bits_value value;
    enum tiny_bits_type type = unpack_value(unpacker, &value);

    if (type == TINY_BITS_ERROR) {
        return Qundef; // Use Qundef as a sentinel for error (not nil)
    }

    switch (type) {
        case TINY_BITS_STR: {        
            return rb_unpack_str(unpacker_data, value, interned);    
        }
        case TINY_BITS_DOUBLE:
            return DBL2NUM(value.double_val);
        case TINY_BITS_INT:
            return LONG2NUM(value.int_val);
        case TINY_BITS_NULL:
            return Qnil;
        case TINY_BITS_TRUE:
            return Qtrue;
        case TINY_BITS_FALSE:
            return Qfalse;
        case TINY_BITS_ARRAY: {
            VALUE arr = rb_ary_new_capa(value.length);
            for (size_t i = 0; i < value.length; i++) {
                VALUE element = unpack_ruby_object(unpacker_data, 0);
                if (element == Qundef) return Qundef; // Error
                rb_ary_push(arr, element);
            }
            return arr;
        }
        case TINY_BITS_MAP: {
            VALUE hash = rb_hash_new_capa(value.length);
            for (size_t i = 0; i < value.length; i++) {  
                VALUE key = unpack_ruby_object(unpacker_data, 1);
                if (key == Qundef) return Qundef; // Error
                VALUE val = unpack_ruby_object(unpacker_data, 0);
                if (val == Qundef) return Qundef; // Error
                rb_hash_aset(hash, key, val);
            }
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

static VALUE rb_unpack(VALUE self, VALUE buffer) {
    UnpackerData* unpacker_data;
    TypedData_Get_Struct(self, UnpackerData, &unpacker_data_type, unpacker_data);

    if (!unpacker_data->unpacker) {
        rb_raise(rb_eRuntimeError, "Unpacker not initialized");
    }

    StringValue(buffer); // Ensure it's a string

    tiny_bits_unpacker_set_buffer(unpacker_data->unpacker, (const unsigned char*)RSTRING_PTR(buffer), RSTRING_LEN(buffer));

    VALUE result = unpack_ruby_object(unpacker_data, 0);
    if (result == Qundef) {
        rb_raise(rb_eRuntimeError, "Failed to unpack data");
    }

    return result;
}

static VALUE rb_set_buffer(VALUE self, VALUE buffer){
    UnpackerData* unpacker_data;
    TypedData_Get_Struct(self, UnpackerData, &unpacker_data_type, unpacker_data);

    if (!unpacker_data->unpacker) {
        rb_raise(rb_eRuntimeError, "Unpacker not initialized");
    }

    StringValue(buffer); // Ensure it's a string

    tiny_bits_unpacker_set_buffer(unpacker_data->unpacker, (const unsigned char*)RSTRING_PTR(buffer), RSTRING_LEN(buffer));

    // set the buffer as an instance variable to mainatin a reference to it
    rb_iv_set(self, "@buffer", buffer);

    return self;
}

static VALUE rb_pop(VALUE self) {

    VALUE buffer = rb_iv_get(self, "@buffer");
    if(buffer == Qnil){
        rb_raise(rb_eRuntimeError, "No buffer is set");
    }

    UnpackerData* unpacker_data;
    TypedData_Get_Struct(self, UnpackerData, &unpacker_data_type, unpacker_data);

    if (!unpacker_data->unpacker) {
        rb_raise(rb_eRuntimeError, "Unpacker not initialized");
    }

    tiny_bits_unpacker* unpacker = unpacker_data->unpacker;
    tiny_bits_value value;

    if(unpacker->current_pos >= unpacker->size - 1){
        return Qnil;
    }
    
    if(unpacker->current_pos > 0){
        enum tiny_bits_type type = unpack_value(unpacker, &value);
        if(type != TINY_BITS_SEP){
            rb_raise(rb_eRuntimeError, "Malformed multi-object buffer");
        }
    }
    
    VALUE result = unpack_ruby_object(unpacker_data, 0);
    if (result == Qundef) {
        rb_raise(rb_eRuntimeError, "Failed to unpack data");
    }

    return result;
}


void Init_tinybits_ext(void) {
    rb_mTinyBits = rb_define_module("TinyBits");
    rb_cPacker = rb_define_class_under(rb_mTinyBits, "Packer", rb_cObject);
    rb_cUnpacker = rb_define_class_under(rb_mTinyBits, "Unpacker", rb_cObject);

    rb_define_alloc_func(rb_cPacker, rb_packer_alloc);
    rb_define_method(rb_cPacker, "initialize", rb_packer_init, 0);
    rb_define_method(rb_cPacker, "pack", rb_pack, 1);
    rb_define_alias(rb_cPacker, "encode", "pack");
    rb_define_alias(rb_cPacker, "dump", "pack");
    rb_define_method(rb_cPacker, "push", rb_push, 1);
    rb_define_alias(rb_cPacker, "<<", "push");
    rb_define_method(rb_cPacker, "to_s", rb_to_s, 0);
    rb_define_method(rb_cPacker, "reset", rb_reset, 0);

    rb_define_alloc_func(rb_cUnpacker, rb_unpacker_alloc);
    rb_define_method(rb_cUnpacker, "initialize", rb_unpacker_init, 0);
    rb_define_method(rb_cUnpacker, "unpack", rb_unpack, 1);
    rb_define_alias(rb_cUnpacker, "load", "unpack");
    rb_define_alias(rb_cUnpacker, "decode", "unpack");
    rb_define_method(rb_cUnpacker, "buffer=", rb_set_buffer, 1);
    rb_define_method(rb_cUnpacker, "pop", rb_pop, 0);
}
