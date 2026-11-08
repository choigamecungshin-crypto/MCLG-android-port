#pragma once
/*
 * Minimal SAL annotation compatibility for non-MSVC toolchains.
 *
 * The original Windows/console sources use Microsoft's Source Annotation
 * Language for static analysis. These annotations do not change runtime
 * semantics, so erase them for Android/Clang builds.
 */
#ifndef _SAL2_ANNOTATIONS
#define _SAL2_ANNOTATIONS

#define _In_
#define _In_opt_
#define _In_z_
#define _In_opt_z_
#define _Out_
#define _Out_opt_
#define _Out_z_
#define _Out_opt_z_
#define _Inout_
#define _Inout_opt_
#define _Inout_z_
#define _In_reads_(count)
#define _In_reads_opt_(count)
#define _In_reads_bytes_(count)
#define _In_reads_bytes_opt_(count)
#define _Out_writes_(count)
#define _Out_writes_opt_(count)
#define _Out_writes_bytes_(count)
#define _Out_writes_bytes_opt_(count)
#define _Inout_updates_(count)
#define _Inout_updates_bytes_(count)
#define _Outptr_
#define _Outptr_opt_
#define _Outptr_result_maybenull_
#define _Outptr_result_nullonfailure_
#define _Ret_maybenull_
#define _Ret_notnull_
#define _Success_(condition)
#define _Check_return_
#define _Must_inspect_result_
#define _Post_z_
#define _Pre_
#define _Post_
#define _Field_size_(count)
#define _Field_size_bytes_(count)
#define _Null_terminated_
#define _When_(condition, annotation)
#define _At_(target, annotation)
#define _Use_decl_annotations_
#define _Analysis_assume_(condition)
#define _In_range_(minimum, maximum)
#define _Out_range_(minimum, maximum)
#define _Deref_out_
#define _Deref_out_opt_
#define _Deref_inout_
#define _Deref_inout_opt_
#define _COM_Outptr_
#define _COM_Outptr_opt_
#define _Printf_format_string_
#define _Scanf_format_string_
#define _Acquires_lock_(lock)
#define _Releases_lock_(lock)
#define _Requires_lock_held_(lock)
#define _Requires_lock_not_held_(lock)
#define _Post_equal_to_(value)
#define _Pre_equal_to_(value)
#define _Post_satisfies_(condition)
#define _Pre_satisfies_(condition)

#endif
