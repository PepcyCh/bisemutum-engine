#pragma once

#ifndef PEP_CPREP_INLINE_NAMESPACE

#define PEP_CPREP_NAMESPACE_BEGIN namespace pep::cprep::inline v0_1_0 {
#define PEP_CPREP_NAMESPACE_END }

#else

#define PEP_CPREP_NAMESPACE_BEGIN namespace pep::cprep::inline PEP_CPREP_INLINE_NAMESPACE {
#define PEP_CPREP_NAMESPACE_END }

#endif
