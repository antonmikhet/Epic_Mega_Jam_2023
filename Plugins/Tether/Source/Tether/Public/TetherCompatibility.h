// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "Misc/EngineVersionComparison.h"

#if UE_VERSION_OLDER_THAN(4,25,0)
typedef class UProperty FProperty;
#endif
