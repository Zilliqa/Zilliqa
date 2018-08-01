#pragma once

#include "evpp/platform_config.h"

#ifdef __cplusplus
#define GOOGLE_GLOG_DLL_DECL
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include "libUtils/Logger.h"

#define LOG_TRACE if (false) LOG(INFO)
#define LOG_DEBUG if (false) LOG(INFO)
#define LOG_INFO if (false) LOG(INFO)
#define LOG_WARN if (false) LOG(INFO)
#define LOG_ERROR if (false) LOG(INFO)
#define LOG_FATAL if (false) LOG(INFO)
#define DLOG_TRACE if (false) LOG(INFO)
#define DLOG_WARN if (false) LOG(INFO)
#define CHECK_NOTnullptr(val) LOG_ERROR << "'" #val "' Must be non nullptr";

#endif // end of define __cplusplus

//#ifdef _DEBUG
//#ifdef assert
//#undef assert
//#endif
//#define assert(expr)  { if (!(expr)) { LOG_FATAL << #expr ;} }
//#endif
