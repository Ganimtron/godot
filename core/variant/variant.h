/*************************************************************************/
/*  variant.h                                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef VARIANT_H
#define VARIANT_H

#include "core/io/ip_address.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/color.h"
#include "core/math/face3.h"
#include "core/math/plane.h"
#include "core/math/quat.h"
#include "core/math/transform.h"
#include "core/math/transform_2d.h"
#include "core/math/vector3.h"
#include "core/math/vector3i.h"
#include "core/object/object_id.h"
#include "core/string/node_path.h"
#include "core/string/ustring.h"
#include "core/templates/rid.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"

class Object;
class Node; // helper
class Control; // helper

struct PropertyInfo;
struct MethodInfo;

typedef Vector<uint8_t> PackedByteArray;
typedef Vector<int32_t> PackedInt32Array;
typedef Vector<int64_t> PackedInt64Array;
typedef Vector<float> PackedFloat32Array;
typedef Vector<double> PackedFloat64Array;
typedef Vector<String> PackedStringArray;
typedef Vector<Vector2> PackedVector2Array;
typedef Vector<Vector3> PackedVector3Array;
typedef Vector<Color> PackedColorArray;

class Variant {
public:
	// If this changes the table in variant_op must be updated
	enum Type {
		NIL,

		// atomic types
		BOOL,
		INT,
		FLOAT,
		STRING,

		// math types
		VECTOR2,
		VECTOR2I,
		RECT2,
		RECT2I,
		VECTOR3,
		VECTOR3I,
		TRANSFORM2D,
		PLANE,
		QUAT,
		AABB,
		BASIS,
		TRANSFORM,

		// misc types
		COLOR,
		STRING_NAME,
		NODE_PATH,
		RID,
		OBJECT,
		CALLABLE,
		SIGNAL,
		DICTIONARY,
		ARRAY,

		// typed arrays
		PACKED_BYTE_ARRAY,
		PACKED_INT32_ARRAY,
		PACKED_INT64_ARRAY,
		PACKED_FLOAT32_ARRAY,
		PACKED_FLOAT64_ARRAY,
		PACKED_STRING_ARRAY,
		PACKED_VECTOR2_ARRAY,
		PACKED_VECTOR3_ARRAY,
		PACKED_COLOR_ARRAY,

		VARIANT_MAX
	};

private:
	friend struct _VariantCall;
	friend class VariantInternal;
	// Variant takes 20 bytes when real_t is float, and 36 if double
	// it only allocates extra memory for aabb/matrix.

	Type type = NIL;

	struct ObjData {
		ObjectID id;
		Object *obj;
	};

	/* array helpers */
	struct PackedArrayRefBase {
		SafeRefCount refcount;
		_FORCE_INLINE_ PackedArrayRefBase *reference() {
			if (this->refcount.ref()) {
				return this;
			} else {
				return nullptr;
			}
		}
		static _FORCE_INLINE_ PackedArrayRefBase *reference_from(PackedArrayRefBase *p_base, PackedArrayRefBase *p_from) {
			if (p_base == p_from) {
				return p_base; //same thing, do nothing
			}

			if (p_from->reference()) {
				if (p_base->refcount.unref()) {
					memdelete(p_base);
				}
				return p_from;
			} else {
				return p_base; //keep, could not reference new
			}
		}
		static _FORCE_INLINE_ void destroy(PackedArrayRefBase *p_array) {
			if (p_array->refcount.unref()) {
				memdelete(p_array);
			}
		}
		_FORCE_INLINE_ virtual ~PackedArrayRefBase() {} //needs virtual destructor, but make inline
	};

	template <class T>
	struct PackedArrayRef : public PackedArrayRefBase {
		Vector<T> array;
		static _FORCE_INLINE_ PackedArrayRef<T> *create() {
			return memnew(PackedArrayRef<T>);
		}
		static _FORCE_INLINE_ PackedArrayRef<T> *create(const Vector<T> &p_from) {
			return memnew(PackedArrayRef<T>(p_from));
		}

		static _FORCE_INLINE_ const Vector<T> &get_array(PackedArrayRefBase *p_base) {
			return static_cast<PackedArrayRef<T> *>(p_base)->array;
		}
		static _FORCE_INLINE_ Vector<T> *get_array_ptr(const PackedArrayRefBase *p_base) {
			return &const_cast<PackedArrayRef<T> *>(static_cast<const PackedArrayRef<T> *>(p_base))->array;
		}

		_FORCE_INLINE_ PackedArrayRef(const Vector<T> &p_from) {
			array = p_from;
			refcount.init();
		}
		_FORCE_INLINE_ PackedArrayRef() {
			refcount.init();
		}
	};

	/* end of array helpers */
	_ALWAYS_INLINE_ ObjData &_get_obj();
	_ALWAYS_INLINE_ const ObjData &_get_obj() const;

	union {
		bool _bool;
		int64_t _int;
		double _float;
		Transform2D *_transform2d;
		::AABB *_aabb;
		Basis *_basis;
		Transform *_transform;
		PackedArrayRefBase *packed_array;
		void *_ptr; //generic pointer
		uint8_t _mem[sizeof(ObjData) > (sizeof(real_t) * 4) ? sizeof(ObjData) : (sizeof(real_t) * 4)];
	} _data alignas(8);

	void reference(const Variant &p_variant);

	void _clear_internal();

	_FORCE_INLINE_ void clear() {
		static const bool needs_deinit[Variant::VARIANT_MAX] = {
			false, //NIL,
			false, //BOOL,
			false, //INT,
			false, //FLOAT,
			true, //STRING,
			false, //VECTOR2,
			false, //VECTOR2I,
			false, //RECT2,
			false, //RECT2I,
			false, //VECTOR3,
			false, //VECTOR3I,
			true, //TRANSFORM2D,
			false, //PLANE,
			false, //QUAT,
			true, //AABB,
			true, //BASIS,
			true, //TRANSFORM,

			// misc types
			false, //COLOR,
			true, //STRING_NAME,
			true, //NODE_PATH,
			false, //RID,
			true, //OBJECT,
			true, //CALLABLE,
			true, //SIGNAL,
			true, //DICTIONARY,
			true, //ARRAY,

			// typed arrays
			true, //PACKED_BYTE_ARRAY,
			true, //PACKED_INT32_ARRAY,
			true, //PACKED_INT64_ARRAY,
			true, //PACKED_FLOAT32_ARRAY,
			true, //PACKED_FLOAT64_ARRAY,
			true, //PACKED_STRING_ARRAY,
			true, //PACKED_VECTOR2_ARRAY,
			true, //PACKED_VECTOR3_ARRAY,
			true, //PACKED_COLOR_ARRAY,
		};

		if (unlikely(needs_deinit[type])) { //make it fast for types that dont need deinit
			_clear_internal();
		}
		type = NIL;
	}

	static void _register_variant_operators();
	static void _unregister_variant_operators();
	static void _register_variant_methods();
	static void _unregister_variant_methods();
	static void _register_variant_setters_getters();
	static void _unregister_variant_setters_getters();
	static void _register_variant_constructors();
	static void _unregister_variant_constructors();
	static void _register_variant_utility_functions();
	static void _unregister_variant_utility_functions();

public:
	_FORCE_INLINE_ Type get_type() const {
		return type;
	}
	static String get_type_name(Variant::Type p_type);
	static bool can_convert(Type p_type_from, Type p_type_to);
	static bool can_convert_strict(Type p_type_from, Type p_type_to);

	bool is_ref() const;
	_FORCE_INLINE_ bool is_num() const {
		return type == INT || type == FLOAT;
	}
	_FORCE_INLINE_ bool is_array() const {
		return type >= ARRAY;
	}
	bool is_shared() const;
	bool is_zero() const;
	bool is_one() const;
	bool is_null() const;

	operator bool() const;
	operator signed int() const;
	operator unsigned int() const; // this is the real one
	operator signed short() const;
	operator unsigned short() const;
	operator signed char() const;
	operator unsigned char() const;
	//operator long unsigned int() const;
	operator int64_t() const;
	operator uint64_t() const;
#ifdef NEED_LONG_INT
	operator signed long() const;
	operator unsigned long() const;
#endif

	operator ObjectID() const;

	operator char32_t() const;
	operator float() const;
	operator double() const;
	operator String() const;
	operator StringName() const;
	operator Vector2() const;
	operator Vector2i() const;
	operator Rect2() const;
	operator Rect2i() const;
	operator Vector3() const;
	operator Vector3i() const;
	operator Plane() const;
	operator ::AABB() const;
	operator Quat() const;
	operator Basis() const;
	operator Transform() const;
	operator Transform2D() const;

	operator Color() const;
	operator NodePath() const;
	operator ::RID() const;

	operator Object *() const;
	operator Node *() const;
	operator Control *() const;

	operator Callable() const;
	operator Signal() const;

	operator Dictionary() const;
	operator Array() const;

	operator Vector<uint8_t>() const;
	operator Vector<int32_t>() const;
	operator Vector<int64_t>() const;
	operator Vector<float>() const;
	operator Vector<double>() const;
	operator Vector<String>() const;
	operator Vector<Vector3>() const;
	operator Vector<Color>() const;
	operator Vector<Plane>() const;
	operator Vector<Face3>() const;

	operator Vector<Variant>() const;
	operator Vector<StringName>() const;
	operator Vector<::RID>() const;
	operator Vector<Vector2>() const;

	// some core type enums to convert to
	operator Margin() const;
	operator Orientation() const;

	operator IP_Address() const;

	Object *get_validated_object() const;
	Object *get_validated_object_with_check(bool &r_previously_freed) const;

	Variant(bool p_bool);
	Variant(signed int p_int); // real one
	Variant(unsigned int p_int);
#ifdef NEED_LONG_INT
	Variant(signed long p_long); // real one
	Variant(unsigned long p_long);
//Variant(long unsigned int p_long);
#endif
	Variant(signed short p_short); // real one
	Variant(unsigned short p_short);
	Variant(signed char p_char); // real one
	Variant(unsigned char p_char);
	Variant(int64_t p_int); // real one
	Variant(uint64_t p_int);
	Variant(float p_float);
	Variant(double p_double);
	Variant(const ObjectID &p_id);
	Variant(const String &p_string);
	Variant(const StringName &p_string);
	Variant(const char *const p_cstring);
	Variant(const char32_t *p_wstring);
	Variant(const Vector2 &p_vector2);
	Variant(const Vector2i &p_vector2i);
	Variant(const Rect2 &p_rect2);
	Variant(const Rect2i &p_rect2i);
	Variant(const Vector3 &p_vector3);
	Variant(const Vector3i &p_vector3i);
	Variant(const Plane &p_plane);
	Variant(const ::AABB &p_aabb);
	Variant(const Quat &p_quat);
	Variant(const Basis &p_matrix);
	Variant(const Transform2D &p_transform);
	Variant(const Transform &p_transform);
	Variant(const Color &p_color);
	Variant(const NodePath &p_node_path);
	Variant(const ::RID &p_rid);
	Variant(const Object *p_object);
	Variant(const Callable &p_callable);
	Variant(const Signal &p_signal);
	Variant(const Dictionary &p_dictionary);

	Variant(const Array &p_array);
	Variant(const Vector<Plane> &p_array); // helper
	Variant(const Vector<uint8_t> &p_byte_array);
	Variant(const Vector<int32_t> &p_int32_array);
	Variant(const Vector<int64_t> &p_int64_array);
	Variant(const Vector<float> &p_float32_array);
	Variant(const Vector<double> &p_float64_array);
	Variant(const Vector<String> &p_string_array);
	Variant(const Vector<Vector3> &p_vector3_array);
	Variant(const Vector<Color> &p_color_array);
	Variant(const Vector<Face3> &p_face_array);

	Variant(const Vector<Variant> &p_array);
	Variant(const Vector<StringName> &p_array);
	Variant(const Vector<::RID> &p_array); // helper
	Variant(const Vector<Vector2> &p_array); // helper

	Variant(const IP_Address &p_address);

	// If this changes the table in variant_op must be updated
	enum Operator {

		//comparison
		OP_EQUAL,
		OP_NOT_EQUAL,
		OP_LESS,
		OP_LESS_EQUAL,
		OP_GREATER,
		OP_GREATER_EQUAL,
		//mathematic
		OP_ADD,
		OP_SUBTRACT,
		OP_MULTIPLY,
		OP_DIVIDE,
		OP_NEGATE,
		OP_POSITIVE,
		OP_MODULE,
		//bitwise
		OP_SHIFT_LEFT,
		OP_SHIFT_RIGHT,
		OP_BIT_AND,
		OP_BIT_OR,
		OP_BIT_XOR,
		OP_BIT_NEGATE,
		//logic
		OP_AND,
		OP_OR,
		OP_XOR,
		OP_NOT,
		//containment
		OP_IN,
		OP_MAX

	};

	static String get_operator_name(Operator p_op);
	static void evaluate(const Operator &p_op, const Variant &p_a, const Variant &p_b, Variant &r_ret, bool &r_valid);
	static _FORCE_INLINE_ Variant evaluate(const Operator &p_op, const Variant &p_a, const Variant &p_b) {
		bool valid = true;
		Variant res;
		evaluate(p_op, p_a, p_b, res, valid);
		return res;
	}

	static Variant::Type get_operator_return_type(Operator p_operator, Type p_type_a, Type p_type_b);
	typedef void (*ValidatedOperatorEvaluator)(const Variant *left, const Variant *right, Variant *r_ret);
	static ValidatedOperatorEvaluator get_validated_operator_evaluator(Operator p_operator, Type p_type_a, Type p_type_b);
#ifdef PTRCALL_ENABLED
	typedef void (*PTROperatorEvaluator)(const void *left, const void *right, void *r_ret);
	static PTROperatorEvaluator get_ptr_operator_evaluator(Operator p_operator, Type p_type_a, Type p_type_b);
#endif

	void zero();
	Variant duplicate(bool deep = false) const;
	static void blend(const Variant &a, const Variant &b, float c, Variant &r_dst);
	static void interpolate(const Variant &a, const Variant &b, float c, Variant &r_dst);

	/* Built-In Methods */

	typedef void (*ValidatedBuiltInMethod)(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret);
	typedef void (*PTRBuiltInMethod)(void *p_base, const void **p_args, void *r_ret, int p_argcount);

	static bool has_builtin_method(Variant::Type p_type, const StringName &p_method);

	static ValidatedBuiltInMethod get_validated_builtin_method(Variant::Type p_type, const StringName &p_method);
	static PTRBuiltInMethod get_ptr_builtin_method(Variant::Type p_type, const StringName &p_method);

	static int get_builtin_method_argument_count(Variant::Type p_type, const StringName &p_method);
	static Variant::Type get_builtin_method_argument_type(Variant::Type p_type, const StringName &p_method, int p_argument);
	static String get_builtin_method_argument_name(Variant::Type p_type, const StringName &p_method, int p_argument);
	static Vector<Variant> get_builtin_method_default_arguments(Variant::Type p_type, const StringName &p_method);
	static bool has_builtin_method_return_value(Variant::Type p_type, const StringName &p_method);
	static Variant::Type get_builtin_method_return_type(Variant::Type p_type, const StringName &p_method);
	static bool is_builtin_method_const(Variant::Type p_type, const StringName &p_method);
	static bool is_builtin_method_vararg(Variant::Type p_type, const StringName &p_method);
	static void get_builtin_method_list(Variant::Type p_type, List<StringName> *p_list);

	void call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant &r_ret, Callable::CallError &r_error);
	Variant call(const StringName &p_method, const Variant &p_arg1 = Variant(), const Variant &p_arg2 = Variant(), const Variant &p_arg3 = Variant(), const Variant &p_arg4 = Variant(), const Variant &p_arg5 = Variant());

	static String get_call_error_text(const StringName &p_method, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce);
	static String get_call_error_text(Object *p_base, const StringName &p_method, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce);
	static String get_callable_error_text(const Callable &p_callable, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce);

	//dynamic (includes Object)
	void get_method_list(List<MethodInfo> *p_list) const;
	bool has_method(const StringName &p_method) const;

	/* Constructors */

	typedef void (*ValidatedConstructor)(Variant &r_base, const Variant **p_args);
	typedef void (*PTRConstructor)(void *base, const void **p_args);

	static int get_constructor_count(Variant::Type p_type);
	static ValidatedConstructor get_validated_constructor(Variant::Type p_type, int p_constructor);
	static PTRConstructor get_ptr_constructor(Variant::Type p_type, int p_constructor);
	static int get_constructor_argument_count(Variant::Type p_type, int p_constructor);
	static Variant::Type get_constructor_argument_type(Variant::Type p_type, int p_constructor, int p_argument);
	static String get_constructor_argument_name(Variant::Type p_type, int p_constructor, int p_argument);
	static void construct(Variant::Type, Variant &base, const Variant **p_args, int p_argcount, Callable::CallError &r_error);

	static void get_constructor_list(Type p_type, List<MethodInfo> *r_list); //convenience

	/* Properties */

	void set_named(const StringName &p_member, const Variant &p_value, bool &r_valid);
	Variant get_named(const StringName &p_member, bool &r_valid) const;

	typedef void (*ValidatedSetter)(Variant *base, const Variant *value);
	typedef void (*ValidatedGetter)(const Variant *base, Variant *value);

	static bool has_member(Variant::Type p_type, const StringName &p_member);
	static Variant::Type get_member_type(Variant::Type p_type, const StringName &p_member);
	static void get_member_list(Type p_type, List<StringName> *r_members);

	static ValidatedSetter get_member_validated_setter(Variant::Type p_type, const StringName &p_member);
	static ValidatedGetter get_member_validated_getter(Variant::Type p_type, const StringName &p_member);

	typedef void (*PTRSetter)(void *base, const void *value);
	typedef void (*PTRGetter)(const void *base, void *value);

	static PTRSetter get_member_ptr_setter(Variant::Type p_type, const StringName &p_member);
	static PTRGetter get_member_ptr_getter(Variant::Type p_type, const StringName &p_member);

	/* Indexing */

	static bool has_indexing(Variant::Type p_type);
	static Variant::Type get_indexed_element_type(Variant::Type p_type);

	typedef void (*ValidatedIndexedSetter)(Variant *base, int64_t index, const Variant *value, bool &oob);
	typedef void (*ValidatedIndexedGetter)(const Variant *base, int64_t index, Variant *value, bool &oob);

	static ValidatedIndexedSetter get_member_validated_indexed_setter(Variant::Type p_type);
	static ValidatedIndexedGetter get_member_validated_indexed_getter(Variant::Type p_type);

	typedef void (*PTRIndexedSetter)(void *base, int64_t index, const void *value);
	typedef void (*PTRIndexedGetter)(const void *base, int64_t index, void *value);

	static PTRIndexedSetter get_member_ptr_indexed_setter(Variant::Type p_type);
	static PTRIndexedGetter get_member_ptr_indexed_getter(Variant::Type p_type);

	void set_indexed(int64_t p_index, const Variant &p_value, bool &r_valid, bool &r_oob);
	Variant get_indexed(int64_t p_index, bool &r_valid, bool &r_oob) const;

	uint64_t get_indexed_size() const;

	/* Keying */

	static bool is_keyed(Variant::Type p_type);

	typedef void (*ValidatedKeyedSetter)(Variant *base, const Variant *key, const Variant *value, bool &valid);
	typedef void (*ValidatedKeyedGetter)(const Variant *base, const Variant *key, Variant *value, bool &valid);
	typedef bool (*ValidatedKeyedChecker)(const Variant *base, const Variant *key, bool &valid);

	static ValidatedKeyedSetter get_member_validated_keyed_setter(Variant::Type p_type);
	static ValidatedKeyedGetter get_member_validated_keyed_getter(Variant::Type p_type);
	static ValidatedKeyedChecker get_member_validated_keyed_checker(Variant::Type p_type);

	typedef void (*PTRKeyedSetter)(void *base, const void *key, const void *value);
	typedef void (*PTRKeyedGetter)(const void *base, const void *key, void *value);
	typedef bool (*PTRKeyedChecker)(const void *base, const void *key);

	static PTRKeyedSetter get_member_ptr_keyed_setter(Variant::Type p_type);
	static PTRKeyedGetter get_member_ptr_keyed_getter(Variant::Type p_type);
	static PTRKeyedChecker get_member_ptr_keyed_checker(Variant::Type p_type);

	void set_keyed(const Variant &p_key, const Variant &p_value, bool &r_valid);
	Variant get_keyed(const Variant &p_key, bool &r_valid) const;
	bool has_key(const Variant &p_key, bool &r_valid) const;

	/* Generic */

	void set(const Variant &p_index, const Variant &p_value, bool *r_valid = nullptr);
	Variant get(const Variant &p_index, bool *r_valid = nullptr) const;
	bool in(const Variant &p_index, bool *r_valid = nullptr) const;

	bool iter_init(Variant &r_iter, bool &r_valid) const;
	bool iter_next(Variant &r_iter, bool &r_valid) const;
	Variant iter_get(const Variant &r_iter, bool &r_valid) const;

	void get_property_list(List<PropertyInfo> *p_list) const;

	static void call_utility_function(const StringName &p_name, Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	static bool has_utility_function(const StringName &p_name);

	typedef void (*ValidatedUtilityFunction)(Variant *r_ret, const Variant **p_args, int p_argcount);
	typedef void (*PTRUtilityFunction)(void *r_ret, const void **p_args, int p_argcount);

	static ValidatedUtilityFunction get_validated_utility_function(const StringName &p_name);
	static PTRUtilityFunction get_ptr_utility_function(const StringName &p_name);

	enum UtilityFunctionType {
		UTILITY_FUNC_TYPE_MATH,
		UTILITY_FUNC_TYPE_RANDOM,
		UTILITY_FUNC_TYPE_GENERAL,
	};

	static UtilityFunctionType get_utility_function_type(const StringName &p_name);

	static int get_utility_function_argument_count(const StringName &p_name);
	static Variant::Type get_utility_function_argument_type(const StringName &p_name, int p_arg);
	static String get_utility_function_argument_name(const StringName &p_name, int p_arg);
	static bool has_utility_function_return_value(const StringName &p_name);
	static Variant::Type get_utility_function_return_type(const StringName &p_name);
	static bool is_utility_function_vararg(const StringName &p_name);

	static void get_utility_function_list(List<StringName> *r_functions);

	//argsVariant call()

	bool operator==(const Variant &p_variant) const;
	bool operator!=(const Variant &p_variant) const;
	bool operator<(const Variant &p_variant) const;
	uint32_t hash() const;

	bool hash_compare(const Variant &p_variant) const;
	bool booleanize() const;
	String stringify(List<const void *> &stack) const;

	void static_assign(const Variant &p_variant);
	static void get_constants_for_type(Variant::Type p_type, List<StringName> *p_constants);
	static bool has_constant(Variant::Type p_type, const StringName &p_value);
	static Variant get_constant_value(Variant::Type p_type, const StringName &p_value, bool *r_valid = nullptr);

	typedef String (*ObjectDeConstruct)(const Variant &p_object, void *ud);
	typedef void (*ObjectConstruct)(const String &p_text, void *ud, Variant &r_value);

	String get_construct_string() const;
	static void construct_from_string(const String &p_string, Variant &r_value, ObjectConstruct p_obj_construct = nullptr, void *p_construct_ud = nullptr);

	void operator=(const Variant &p_variant); // only this is enough for all the other types

	static void register_types();
	static void unregister_types();

	Variant(const Variant &p_variant);
	_FORCE_INLINE_ Variant() {}
	_FORCE_INLINE_ ~Variant() {
		clear();
	}
};

//typedef Dictionary Dictionary; no
//typedef Array Array;

Vector<Variant> varray();
Vector<Variant> varray(const Variant &p_arg1);
Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2);
Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3);
Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3, const Variant &p_arg4);
Vector<Variant> varray(const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3, const Variant &p_arg4, const Variant &p_arg5);

struct VariantHasher {
	static _FORCE_INLINE_ uint32_t hash(const Variant &p_variant) { return p_variant.hash(); }
};

struct VariantComparator {
	static _FORCE_INLINE_ bool compare(const Variant &p_lhs, const Variant &p_rhs) { return p_lhs.hash_compare(p_rhs); }
};

Variant::ObjData &Variant::_get_obj() {
	return *reinterpret_cast<ObjData *>(&_data._mem[0]);
}

const Variant::ObjData &Variant::_get_obj() const {
	return *reinterpret_cast<const ObjData *>(&_data._mem[0]);
}

String vformat(const String &p_text, const Variant &p1 = Variant(), const Variant &p2 = Variant(), const Variant &p3 = Variant(), const Variant &p4 = Variant(), const Variant &p5 = Variant());

#endif // VARIANT_H
