// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

class FTetherDetailUtils
{
public:
	static TArray< TSharedPtr<class IPropertyHandle>> FindPropertyLeaves(const TSharedPtr<class IPropertyHandle> Property);

	static void FlattenPropertyTree(class IDetailLayoutBuilder& DetailBuilder, const TSharedPtr<class IPropertyHandle> Property);

	static void ReplacePropertyWithChildren(class IDetailLayoutBuilder& DetailBuilder, const TSharedPtr<class IPropertyHandle> Property, TArray<TSharedPtr<class IPropertyHandle>> ExcludedProps = {});

	struct FCablePropertiesPropertyInfo
	{
		UObject* Object = nullptr;
		TSharedPtr<IPropertyHandle> Property = nullptr;
	};
	
	static TSharedRef<class FMaterialList> CreateCablePropertiesMaterialList(class IDetailLayoutBuilder& DetailBuilder, TArray<FCablePropertiesPropertyInfo> CableProperties);
};
