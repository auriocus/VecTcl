# note: the order in this list matters
# it defines upconversion between different types
# pure integer upconverts to all fixed-types, because we want e.g. 3i8 + 2 = 5i8
# fixed-width integers upconvert analogously to C
# enum ctype vectcltype
set typetable {
	NumArray_Bool int bool
	NumArray_Int NaWideInt int
	NumArray_Int8 int8_t int8
	NumArray_Uint8 uint8_t uint8
	NumArray_Int16 int16_t int16
	NumArray_Uint16 uint16_t uint16
	NumArray_Int32 int32_t int32
	NumArray_Uint32 uint32_t uint32
	NumArray_Int64 int64_t int64
	NumArray_Uint64 uint64_t uint64
	NumArray_Float32 float float
	NumArray_Float64 double double
	NumArray_Complex64 NumArray_ComplexFloat complex64
	NumArray_Complex128 NumArray_Complex complex128
	NumArray_Tcl_Obj Tcl_Obj* value
}


set NA_FIXEDINTEGERS {
	NumArray_Int8 
	NumArray_Uint8
	NumArray_Int16
	NumArray_Uint16
	NumArray_Int32
	NumArray_Uint32
	NumArray_Int64
	NumArray_Uint64
}

set NA_SIGNEDINTEGERS {
	NumArray_Int
	NumArray_Int8 
	NumArray_Int16
	NumArray_Int32
	NumArray_Int64
}

set NA_UNSIGNEDINTEGERS {
	NumArray_Uint8
	NumArray_Uint16
	NumArray_Uint32
	NumArray_Uint64
}

set NA_TYPESUFFIXES {
	NumArray_Int ""
	NumArray_Int8 i8
	NumArray_Int16 i16
	NumArray_Int32 i32
	NumArray_Int64 i64
	NumArray_Uint8 u8
	NumArray_Uint16 u16
	NumArray_Uint32 u32
	NumArray_Uint64 u64
}

set NA_INTEGERS [list NumArray_Int NumArray_Bool {*}$NA_FIXEDINTEGERS]
set NA_REALFLOATS {NumArray_Float32 NumArray_Float64}

set NA_REALTYPES [list {*}$NA_INTEGERS {*}$NA_REALFLOATS]
set NA_COMPLEXTYPES {NumArray_Complex64 NumArray_Complex128}


foreach {enum ctype vectcltype} $typetable {
	dict set NA_TO_CTYPE $enum $ctype
	dict set NA_TO_VECTCLTYPE $enum $vectcltype
}

set NA_ALLTYPES [dict keys $NA_TO_CTYPE]
set NA_NUMERICTYPES [lrange $NA_ALLTYPES 0 end-1] ;# remove the Tcl_Obj type

# converting types in binary operators
# given type1 and type2, this computes the result type
proc upcast_common_type {type1 type2} {
	variable NA_NUMERICTYPES
	set ind1 [lindex $type1 $NA_NUMERICTYPES]
	set ind2 [lindex $type2 $NA_NUMERICTYPES]
	if { $ind1<0 || $ind2 < 0} { 
		return -code error "Unknown data types $type1, $type2"
	}
	return [lindex $NA_NUMERICTYPES [expr {max($ind1, $ind2)}]
}

proc upcast_type {from to} {
	C {
		/* all conversions which can be done by the C compiler */
		#define BUILTINCONV(X, Y) \
			if (type == X && info -> type == Y) { \
				C_FROM_NATYPE(X) *bufptr = NumArrayIteratorDeRefPtr(&convit); \
				for (; ! NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) { \
					* bufptr++ = (C_FROM_NATYPE(X)) (*((C_FROM_NATYPE(Y)*)NumArrayIteratorDeRefPtr(&it))); \
				} \
				goto ready; \
			}
			
	}
}
proc cquote {x} {
	set q [string map {\" \"\" \\ \\\\} $x]
	return "\"$q\""
}

proc = {v} { upvar $v n; return $n }
