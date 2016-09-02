#include "compathack.h"

const NumArrayType NumArrayCompatTypeMap[NumArray_SentinelType] = {
${ 
	set map {NumArray_Int NumArray_Float64 NumArray_Complex128}
	set indlist [lmap type $NA_ALLTYPES {lsearch $map $type}] 
	join $indlist ,
$}
};
