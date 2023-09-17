// Copyright Sam Bonifacio 2021. All Rights Reserved.


#include "TetherEditor/Public/CablePropertiesStructCustomization.h"

#include "DetailCategoryBuilder.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "TetherCableActor.h"
#include "TetherCableProperties.h"
#include "TetherDetailUtils.h"
#include "MaterialList.h"

void FCablePropertiesStructCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<SWidget> ValueWidget = StructPropertyHandle->CreatePropertyValueWidget();
	HeaderRow.NameContent()
    [
        StructPropertyHandle->CreatePropertyNameWidget()
    ]
	.ValueContent()
	[
		ValueWidget
	];
}

void FCablePropertiesStructCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildrenProps = 0;
	StructPropertyHandle->GetNumChildren(NumChildrenProps);

	TSharedPtr<IPropertyHandle> MaterialsProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTetherCableProperties, Materials));
	
	for (uint32 ChildIdx = 0; ChildIdx < NumChildrenProps; ++ChildIdx)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIdx);
		if (ChildHandle.IsValid() && ChildHandle->IsValidHandle())
		{
			if(ChildHandle->GetProperty() == MaterialsProperty->GetProperty())
			{
				// Special handling of the materials property
				const bool bEditable = MaterialsProperty->IsEditable();
				if(bEditable)
				{
					TArray<FAssetData> MaterialListOwner;
					TArray<FTetherDetailUtils::FCablePropertiesPropertyInfo> PropertyInfos;

					for(const TWeakObjectPtr<UObject> Obj : StructCustomizationUtils.GetPropertyUtilities()->GetSelectedObjects())
					{
						PropertyInfos.Add(FTetherDetailUtils::FCablePropertiesPropertyInfo{Obj.Get(), StructPropertyHandle});
					}

					TSharedRef<FMaterialList> MaterialList = FTetherDetailUtils::CreateCablePropertiesMaterialList(ChildBuilder.GetParentCategory().GetParentLayout(), PropertyInfos);
					ChildBuilder.AddCustomBuilder(MaterialList);
				}
			}
			else
			{
				IDetailPropertyRow& NewPropRow = ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
			}
		}
	}
}

TSharedRef<IPropertyTypeCustomization> FCablePropertiesStructCustomization::MakeInstance()
{
	return MakeShareable(new FCablePropertiesStructCustomization);
}
