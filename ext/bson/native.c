#include <ruby.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

/**
 * For 64 byte systems we convert to longs, for 32 byte systems we convert
 * to a long long.
 *
 * @since 2.0.0
 */
#if SIZEOF_LONG == 8
#define NUM2INT64(v) NUM2LONG(v)
#define INT642NUM(v) LONG2NUM(v)
#else
#define NUM2INT64(v) NUM2LL(v)
#define INT642NUM(v) LL2NUM(v)
#endif

/**
 * Ruby 1.8.7 does not define DBL2NUM, so we define it if it's not there.
 *
 * @since 2.0.0
 */
#ifndef DBL2NUM
#define DBL2NUM(dbl) rb_float_new(dbl)
#endif

/**
 * Define the max hostname length constant if nonexistant.
 *
 * @since 2.0.0
 */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

/**
 * Define encoding macros to be able to support 1.8.
 *
 * @since 2.0.0
 */
#ifdef HAVE_RUBY_ENCODING_H
#include <ruby/encoding.h>

/**
 * Convert a ruby string into a utf-8 compatible binary string.
 *
 * @example Convert the string to utf-8 binary.
 *    rb_bson_to_utf8_binary("test");
 *
 * @param [ String ] string The ruby string.
 *
 * @return [ String ] The encoded string.
 *
 * @since 2.0.0
 */
static VALUE rb_bson_to_utf8_binary(VALUE string)
{
  VALUE utf8 = rb_str_encode(string, rb_str_new("UTF-8", 5), 0, Qnil);
  return rb_enc_associate(utf8, rb_usascii_encoding());
}

/**
 * Convert the binary string to a ruby utf8 string.
 *
 * @example Convert the string to binary.
 *    rb_bson_from_bson_string("test");
 *
 * @param [ String ] string The ruby string.
 *
 * @return [ String ] The encoded string.
 *
 * @since 2.0.0
 */
static VALUE rb_bson_from_bson_string(VALUE string)
{
  return rb_enc_associate(string, rb_utf8_encoding());
}
#else

/**
 * convert a ruby string into a utf-8 compatible binary string.
 *
 * @example convert the string to utf-8 binary.
 *    rb_bson_to_utf8_binary("test");
 *
 * @param [ string ] string the ruby string.
 *
 * @return [ string ] the encoded string.
 *
 * @since 2.0.0
 */
static VALUE rb_bson_to_utf8_binary(VALUE string)
{
  return string;
}

/**
 * Convert the binary string to a ruby utf8 string.
 *
 * @example Convert the string to binary.
 *    rb_bson_from_bson_string("test");
 *
 * @param [ String ] string The ruby string.
 *
 * @return [ String ] The encoded string.
 *
 * @since 2.0.0
 */
static VALUE rb_bson_from_bson_string(VALUE string)
{
  return string;
}
#endif

/**
 * Constant for a null byte.
 *
 * @since 2.0.0
 */
static const char rb_bson_null_byte = 0;

/**
 * Constant for a true byte.
 *
 * @since 2.0.0
 */
static const char rb_bson_true_byte = 1;

/**
 * BSON::BINARY
 *
 * @since 2.0.0
 */
static VALUE bson_binary;

/**
 * Holds the machine id for object id generation.
 *
 * @since 2.0.0
 *
 * @todo: Need to set this value properly.
 */
static char rb_bson_machine_id[HOST_NAME_MAX];

/**
 * The counter for incrementing object ids.
 *
 * @since 2.0.0
 */
static unsigned int rb_bson_object_id_counter = 0;

/**
 * Get the current time in milliseconds, used in object id generation.
 *
 * @example Get the current time in milliseconds.
 *    rb_current_time_milliseconds();
 *
 * @return [ int ] The current time in millis.
 *
 * @since 2.0.0
 */
static unsigned long rb_current_time_milliseconds()
{
  struct timeval time;
  gettimeofday(&time, NULL);
  return (time.tv_sec) * 1000 + (time.tv_usec) / 1000;
}

/**
 * Provide default new string with binary encoding.
 *
 * @example Check encoded and provide default new binary encoded string.
 *    if (NIL_P(encoded)) encoded = rb_str_new_encoded_binary();
 *
 * @return [ String ] The new string with binary encoding.
 *
 * @since 2.0.0
 */
static VALUE rb_str_new_encoded_binary(void)
{
  VALUE encoded = rb_str_new2("");
  rb_funcall(encoded, rb_intern("force_encoding"), 1, bson_binary, 0);
  return encoded;
}

/**
 * Take the provided params and return the encoded bytes or a default one.
 *
 * @example Get the default encoded bytes.
 *    rb_get_default_encoded(1, bytes);
 *
 * @param [ int ] argc The number of arguments.
 * @param [ Object ] argv The arguments.
 *
 * @return [ String ] The encoded string.
 *
 * @since 2.0.0
 */
static VALUE rb_get_default_encoded(int argc, VALUE *argv)
{
  VALUE encoded;
  rb_scan_args(argc, argv, "01", &encoded);
  if (NIL_P(encoded)) encoded = rb_str_new_encoded_binary();
  return encoded;
}

/**
 * Append the ruby float as 8-byte double value to buffer.
 *
 * @example Convert float to double and append.
 *    rb_float_to_bson(..., 1.2311);
 *
 * @param [ String] encoded Optional string buffer, default provided by rb_str_encoded_binary
 * @param [ Float ] self The ruby float value.
 *
 * @return [ String ] The encoded bytes with double value appended.
 *
 * @since 2.0.0
 */
static VALUE rb_float_to_bson(int argc, VALUE *argv, VALUE self)
{
  const double v = NUM2DBL(self);
  VALUE encoded = rb_get_default_encoded(argc, argv);
  rb_str_cat(encoded, (char*) &v, 8);
  return encoded;
}

/**
 * Convert the bytes for the double into a Ruby float.
 *
 * @example Convert the bytes to a float.
 *    rb_float_from_bson_double(class, bytes);
 *
 * @param [ Class ] The float class.
 * @param [ String ] The double bytes.
 *
 * @return [ Float ] The ruby float value.
 *
 * @since 2.0.0
 */
static VALUE rb_float_from_bson_double(VALUE self, VALUE value)
{
  const char * bytes;
  double v;
  StringValue(value);
  bytes = RSTRING_PTR(value);
  memcpy(&v, bytes, RSTRING_LEN(value));
  return DBL2NUM(v);
}

/**
 * Generate the data for the next object id.
 *
 * @example Generate the data for the next object id.
 *    rb_object_id_generator_next(0, NULL, object_id);
 *
 * @param [ int ] argc The argument count.
 * @param [ Time ] time The optional Ruby time.
 * @param [ BSON::ObjectId ] self The object id.
 *
 * @return [ String ] The raw bytes for the id.
 *
 * @since 2.0.0
 */
static VALUE rb_object_id_generator_next(int argc, VALUE* time, VALUE self)
{
  char bytes[12];
  unsigned long t;
  unsigned short pid = htons(getpid());

  if (argc == 0 || (argc == 1 && *time == Qnil)) {
    t = rb_current_time_milliseconds();
  }
  else {
    t = htonl(NUM2UINT(rb_funcall(*time, rb_intern("to_i"), 0)));
  }

  memcpy(&bytes, &time, 4);
  memcpy(&bytes[4], rb_bson_machine_id, 3);
  memcpy(&bytes[7], &pid, 2);
  memcpy(&bytes[9], (unsigned char*) &rb_bson_object_id_counter, 3);
  rb_bson_object_id_counter++;
  return rb_str_new(bytes, 12);
}

/**
 * Check if the integer is a 32 bit integer.
 *
 * @example Check if the integer is 32 bit.
 *    rb_integer_is_bson_int32(integer);
 *
 * @param [ Integer ] self The ruby integer.
 *
 * @return [ true, false ] If the integer is 32 bit.
 *
 * @since 2.0.0
 */
static VALUE rb_integer_is_bson_int32(VALUE self)
{
  const int64_t v = NUM2INT64(self);
  if (INT_MIN <= v && v <= INT_MAX) {
    return Qtrue;
  }
  else {
    return Qfalse;
  }
}

/**
 * Convert the Ruby integer into a BSON as per the 32 bit specification,
 * which is 4 bytes.
 *
 * @example Convert the integer to 32bit BSON.
 *    rb_integer_to_bson_int32(128, encoded);
 *
 * @param [ Integer ] self The Ruby integer.
 * @param [ String ] encoded The Ruby binary string to append to.
 *
 * @return [ String ] encoded Ruby binary string with BSON raw bytes appended.
 *
 * @since 2.0.0
 */
static VALUE rb_integer_to_bson_int32(VALUE self, VALUE encoded)
{
  const int32_t v = NUM2INT(self);
  const char bytes[4] = {
    v & 255,
    (v >> 8) & 255,
    (v >> 16) & 255,
    (v >> 24) & 255
  };
  return rb_str_cat(encoded, bytes, 4);
}

#define BSON_INDEX_SIZE 1024
#define BSON_INDEX_CHAR_SIZE 5
static char bson_array_indexes[BSON_INDEX_SIZE][BSON_INDEX_CHAR_SIZE];

static void init_integer_bson_array_indexes(void)
{
  int i;
  for (i = 0; i < BSON_INDEX_SIZE; i++) {
    snprintf(bson_array_indexes[i], BSON_INDEX_CHAR_SIZE, "%d", i);
  }
}

#define INTEGER_CHAR_SIZE 22

/**
 * Convert the Ruby integer into a character string and append with nullchar to encoded BSON.
 *
 * @example Convert the integer to string and append with nullchar.
 *    rb_integer_to_bson_key(128, encoded);
 *
 * @param [ Integer ] self The Ruby integer.
 * @param [ String ] encoded The Ruby binary string to append to.
 *
 * @return [ String ] encoded Ruby binary string with BSON raw bytes appended.
 *
 * @since 2.0.0
 */
static VALUE rb_integer_to_bson_key(int argc, VALUE *argv, VALUE self)
{
  char bytes[INTEGER_CHAR_SIZE];
  const int64_t v = NUM2INT64(self);
  VALUE encoded = rb_get_default_encoded(argc, argv);
  int length;
  if (v < BSON_INDEX_SIZE)
    return rb_str_cat(encoded, bson_array_indexes[v], strlen(bson_array_indexes[v]) + 1);
  length = snprintf(bytes, INTEGER_CHAR_SIZE, "%ld", (long)v);
  return rb_str_cat(encoded, bytes, length + 1);
}

/**
 * Convert the provided raw bytes into a 32bit Ruby integer.
 *
 * @example Convert the bytes to an Integer.
 *    rb_integer_from_bson_int32(Int32, bytes);
 *
 * @param [ BSON::Int32 ] self The Int32 eigenclass.
 * @param [ String ] bytes The raw bytes.
 *
 * @return [ Integer ] The Ruby integer.
 *
 * @since 2.0.0
 */
static VALUE rb_integer_from_bson_int32(VALUE self, VALUE bson)
{
  const uint8_t *v = (const uint8_t*) RSTRING_PTR(bson);
  const uint32_t integer = v[0] + (v[1] << 8) + (v[2] << 16) + (v[3] << 24);
  return INT2NUM(integer);
}

/**
 * Convert the provided raw bytes into a 64bit Ruby integer.
 *
 * @example Convert the bytes to an Integer.
 *    rb_integer_from_bson_int64(Int64, bytes);
 *
 * @param [ BSON::Int64 ] self The Int64 eigenclass.
 * @param [ String ] bytes The raw bytes.
 *
 * @return [ Integer ] The Ruby integer.
 *
 * @since 2.0.0
 */
static VALUE rb_integer_from_bson_int64(VALUE self, VALUE bson)
{
  uint8_t *v;
  uint32_t byte_0, byte_1, byte_2, byte_3;
  int64_t lower, upper;
  StringValue(bson);
  v = (uint8_t*) RSTRING_PTR(bson);
  byte_0 = v[0];
  byte_1 = v[1];
  byte_2 = v[2];
  byte_3 = v[3];
  lower = byte_0 + (byte_1 << 8) + (byte_2 << 16) + (byte_3 << 24);
  byte_0 = v[4];
  byte_1 = v[5];
  byte_2 = v[6];
  byte_3 = v[7];
  upper = byte_0 + (byte_1 << 8) + (byte_2 << 16) + (byte_3 << 24);
  return INT642NUM(lower + (upper << 32));
}

/**
 * Append the 64-bit integer to encoded BSON Ruby binary string.
 *
 * @example Append the 64-bit integer to encoded BSON.
 *    int64_t_to_bson(128, encoded);
 *
 * @param [ int64_t ] self The 64-bit integer.
 * @param [ String ] encoded The BSON Ruby binary string to append to.
 *
 * @return [ String ] encoded Ruby binary string with BSON raw bytes appended.
 *
 * @since 2.0.0
 */
static VALUE int64_t_to_bson(int64_t v, VALUE encoded)
{
  const char bytes[8] = {
    v & 255,
    (v >> 8) & 255,
    (v >> 16) & 255,
    (v >> 24) & 255,
    (v >> 32) & 255,
    (v >> 40) & 255,
    (v >> 48) & 255,
    (v >> 56) & 255
  };
  return rb_str_cat(encoded, bytes, 8);
}

/**
 * Convert the Ruby integer into a BSON as per the 64 bit specification,
 * which is 8 bytes.
 *
 * @example Convert the integer to 64bit BSON.
 *    rb_integer_to_bson_int64(128, encoded);
 *
 * @param [ Integer ] self The Ruby integer.
 * @param [ String ] encoded The Ruby binary string to append to.
 *
 * @return [ String ] encoded Ruby binary string with BSON raw bytes appended.
 *
 * @since 2.0.0
 */
static VALUE rb_integer_to_bson_int64(VALUE self, VALUE encoded)
{
  return int64_t_to_bson(NUM2INT64(self), encoded);
}

/**
 * Converts the milliseconds time to the raw BSON bytes. We need to
 * explicitly convert using 64 bit here.
 *
 * @example Convert the milliseconds value to BSON bytes.
 *    rb_time_to_bson(time, 2124132340000, encoded);
 *
 * @param [ Time ] self The Ruby Time object.
 * @param [ Integer ] milliseconds The milliseconds pre/post epoch.
 * @param [ String ] encoded The Ruby binary string to append to.
 *
 * @return [ String ] encoded Ruby binary string with time BSON raw bytes appended.
 *
 * @since 2.0.0
 */
static VALUE rb_time_to_bson(int argc, VALUE *argv, VALUE self)
{
  double t = NUM2DBL(rb_funcall(self, rb_intern("to_f"), 0));
  int64_t milliseconds = (int64_t)(t * 1000);
  VALUE encoded = rb_get_default_encoded(argc, argv);
  return int64_t_to_bson(milliseconds, encoded);
}

/**
 * Set four bytes for int32 in a binary string and return it.
 *
 * @example Set int32 in a BSON string.
 *   rb_string_set_int32(self, pos, int32)
 *
 * @param [ String ] self The Ruby binary string.
 * @param [ Fixnum ] The position to set.
 * @param [ Fixnum ] The int32 value.
 *
 * @return [ String ] The binary string.
 *
 * @since 2.0.0
 */
static VALUE rb_string_set_int32(VALUE str, VALUE pos, VALUE an_int32)
{
  const int32_t offset = NUM2INT(pos);
  const int32_t v = NUM2INT(an_int32);
  const char bytes[4] = {
    v & 255,
    (v >> 8) & 255,
    (v >> 16) & 255,
    (v >> 24) & 255
  };
  if (offset < 0 || offset + 4 > RSTRING_LEN(str)) {
    rb_raise(rb_eArgError, "invalid position");
  }
  memcpy(RSTRING_PTR(str) + offset, bytes, 4);
  return str;
}

/**
 * Convert the ruby string to a BSON string.
 *
 * @example Convert the Ruby string to a BSON string.
 *    rb_string_to_bson_string(0, Qnil, "test");
 *
 * @param [ String ] self The string value.
 * @param [ String ] encoded The encoded string to append to.
 *
 * @return [ String ] The encoded string.
 *
 * @since 2.0.0
 */
static VALUE rb_string_to_bson_string(VALUE self, VALUE encoded)
{
  VALUE binary = rb_bson_to_utf8_binary(self);
  StringValue(binary);
  rb_str_cat(encoded, RSTRING_PTR(binary), RSTRING_LEN(binary));
  return encoded;
}

/**
 * Check for illegal characters in string.
 *
 * @example Check for illegal characters.
 *    rb_string_check_for_illegal_characters("test");
 *
 * @param [ String ] self The string value.
 *
 * @since 2.0.0
 */
static VALUE rb_string_check_for_illegal_characters(VALUE self)
{
  if (strlen(RSTRING_PTR(self)) != (size_t)RSTRING_LEN(self))
    rb_raise(rb_eRuntimeError, "Illegal C-String contains a null byte.");
  return self;
}

/**
 * Encode a false value to bson.
 *
 * @example Encode the false value.
 *    rb_false_class_to_bson(0, false);
 *
 * @param [ int ] argc The number or arguments.
 * @param [ Array<Object> ] argv The arguments.
 * @param [ TrueClass ] self The true value.
 *
 * @return [ String ] The encoded string.
 *
 * @since 2.0.0
 */
static VALUE rb_false_class_to_bson(int argc, VALUE *argv, VALUE self)
{
  VALUE encoded = rb_get_default_encoded(argc, argv);
  rb_str_cat(encoded, &rb_bson_null_byte, 1);
  return encoded;
}

/**
 * Encode a true value to bson.
 *
 * @example Encode the true value.
 *    rb_true_class_to_bson(0, true);
 *
 * @param [ int ] argc The number or arguments.
 * @param [ Array<Object> ] argv The arguments.
 * @param [ TrueClass ] self The true value.
 *
 * @return [ String ] The encoded string.
 *
 * @since 2.0.0
 */
static VALUE rb_true_class_to_bson(int argc, VALUE *argv, VALUE self)
{
  VALUE encoded = rb_get_default_encoded(argc, argv);
  rb_str_cat(encoded, &rb_bson_true_byte, 1);
  return encoded;
}

/**
 * Initialize the bson c extension.
 *
 * @since 2.0.0
 */
void Init_native()
{
  // Get all the constants to be used in the extensions.
  VALUE bson = rb_const_get(rb_cObject, rb_intern("BSON"));
  VALUE integer = rb_const_get(bson, rb_intern("Integer"));
  VALUE floats = rb_const_get(bson, rb_intern("Float"));
  VALUE float_class = rb_const_get(floats, rb_intern("ClassMethods"));
  VALUE time = rb_const_get(bson, rb_intern("Time"));
  VALUE int32 = rb_const_get(bson, rb_intern("Int32"));
  VALUE int32_class = rb_singleton_class(int32);
  VALUE int64 = rb_const_get(bson, rb_intern("Int64"));
  VALUE int64_class = rb_singleton_class(int64);
  VALUE object_id = rb_const_get(bson, rb_intern("ObjectId"));
  VALUE generator = rb_const_get(object_id, rb_intern("Generator"));
  VALUE string = rb_const_get(bson, rb_intern("String"));
  VALUE true_class = rb_const_get(bson, rb_intern("TrueClass"));
  VALUE false_class = rb_const_get(bson, rb_intern("FalseClass"));
  bson_binary = rb_const_get(bson, rb_intern("BINARY"));

  // Get the object id machine id.
  gethostname(rb_bson_machine_id, sizeof rb_bson_machine_id);
  rb_bson_machine_id[HOST_NAME_MAX - 1] = '\0';

  // Redefine the serialization methods on the Integer class.
  rb_undef_method(integer, "to_bson_int32");
  rb_define_method(integer, "to_bson_int32", rb_integer_to_bson_int32, 1);
  rb_undef_method(integer, "to_bson_int64");
  rb_define_method(integer, "to_bson_int64", rb_integer_to_bson_int64, 1);
  rb_undef_method(integer, "bson_int32?");
  rb_define_method(integer, "bson_int32?", rb_integer_is_bson_int32, 0);
  init_integer_bson_array_indexes();
  rb_undef_method(integer, "to_bson_key");
  rb_define_method(integer, "to_bson_key", rb_integer_to_bson_key, -1);

  // Redefine float's to_bson, from_bson.
  rb_undef_method(floats, "to_bson");
  rb_define_method(floats, "to_bson", rb_float_to_bson, -1);
  rb_undef_method(float_class, "from_bson_double");
  rb_define_private_method(float_class, "from_bson_double", rb_float_from_bson_double, 1);

  // Redefine deserialization methods on Int32 class.
  rb_undef_method(int32_class, "from_bson_int32");
  rb_define_private_method(int32_class, "from_bson_int32", rb_integer_from_bson_int32, 1);

  // Redefine deserialization methods on Int64 class.
  rb_undef_method(int64_class, "from_bson_int64");
  rb_define_private_method(int64_class, "from_bson_int64", rb_integer_from_bson_int64, 1);

  // Redefine the serialization methods on the time class.
  rb_undef_method(time, "to_bson");
  rb_define_method(time, "to_bson", rb_time_to_bson, -1);

  // Redefine methods on the String class.
  rb_undef_method(string, "set_int32");
  rb_define_method(string, "set_int32", rb_string_set_int32, 2);
  rb_undef_method(string, "to_utf8_binary");
  rb_define_private_method(string, "to_utf8_binary", rb_string_to_bson_string, 1);
  rb_undef_method(string, "from_bson_string");
  rb_define_method(string, "from_bson_string", rb_bson_from_bson_string, 0);
  rb_undef_method(string, "check_for_illegal_characters!");
  rb_define_private_method(string, "check_for_illegal_characters!", rb_string_check_for_illegal_characters, 0);

  // Redefine the next method on the object id generator.
  rb_undef_method(generator, "next");
  rb_define_method(generator, "next", rb_object_id_generator_next, -1);

  // Redefine the to_bson method on TrueClass.
  rb_undef_method(true_class, "to_bson");
  rb_define_method(true_class, "to_bson", rb_true_class_to_bson, -1);

  // Redefine the to_bson method on FalseClass.
  rb_undef_method(false_class, "to_bson");
  rb_define_method(false_class, "to_bson", rb_false_class_to_bson, -1);
}
