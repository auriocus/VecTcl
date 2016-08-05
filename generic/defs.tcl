# enum ctype vectcltype
set typetable {
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

set NA_INTEGERS [list NumArray_Int {*}$NA_FIXEDINTEGERS]
set NA_REALFLOATS {NumArray_Float32 NumArray_Float64}

set NA_REALTYPES [list {*}$NA_INTEGERS {*}$NA_REALFLOATS]
set NA_COMPLEXTYPES {NumArray_Complex64 NumArray_Complex128}


foreach {enum ctype vectcltype} $typetable {
	dict set NA_TO_CTYPE $enum $ctype
	dict set NA_TO_VECTCLTYPE $enum $vectcltype
}

set NA_ALLTYPES [dict keys $NA_TO_CTYPE]
set NA_NUMERICTYPES [lrange $NA_ALLTYPES 0 end-1] ;# remove the Tcl_Obj type

proc cquote {x} {
	set q [string map {\" \"\" \\ \\\\} $x]
	return "\"$q\""
}

proc = {v} { upvar $v n; return $n }
